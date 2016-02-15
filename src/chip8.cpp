// This is *heavily*  based on Laurence Muller's tutorial at
// http://www.multigesture.net/articles/how-to-write-an-emulator-chip-8-interpreter/

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "../include/chip8.hpp"

//=====================================================================
//
// Static locals
//
//=====================================================================

//=====================================================================
static const uint16_t s_fontSet[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80, // F
};


//=====================================================================
//
// Chip8 definitions
//
//=====================================================================

//=====================================================================
void Chip8::Initialize (unsigned randSeed) {

    CSaruCore::SecureZero(m_memory, s_memoryBytes);
    CSaruCore::SecureZero(m_v, s_registerCount);
    m_i          = 0x0000;
    m_pc         = s_progRomRamBegin;
    m_delayTimer = 0;
    m_soundTimer = 0;
    CSaruCore::SecureZero(m_keyStates, s_keyCount);

    m_opcode = 0x0000;
    CSaruCore::SecureZero(m_stack, s_stackSize);
    m_sp     = 0x00;
    CSaruCore::SecureZero(m_renderOut, s_renderWidth * s_renderHeight);

    std::memcpy(m_memory + s_fontBegin, s_fontSet, sizeof(s_fontSet));

    // TODO : Replace with a per-Chip8 random number generator.
    std::srand(randSeed);

}

//=====================================================================
void Chip8::EmulateCycle () {

    // Fetch opcode
    m_opcode = m_memory[m_pc] << 8 | m_memory[m_pc + 1];
    //std::printf("  test 0x%04X\n", m_opcode & 0xF000);

    // Decode opcode
    if (m_opcode == 0x00E0) { // 0x00E0: clear the screen
        std::memset(m_renderOut, 0, sizeof(m_renderOut));
        m_pc += 2;
    }
    else if (m_opcode == 0x00EE) { // 0x00EE: return from call
        m_pc = m_stack[--m_sp] + 2;
    }
    else if ((m_opcode & 0xF000) == 0x1000) { // 0x1NNN: jump to NNN
        m_pc = m_opcode & 0x0FFF;
    }
    else if ((m_opcode & 0xF000) == 0x2000) { // 0x2NNN: call NNN
        m_stack[m_sp++] = m_pc;
        m_pc = m_opcode & 0x0FFF;
    }
    else if ((m_opcode & 0xF000) == 0x4000) { // 0x4XNN:
        // skip next instruction if VX != NN
        auto & vx = m_v[ m_opcode & 0x0F00 ];
        if (vx != (m_opcode & 0x00FF))
            m_pc += 2;
        m_pc += 2;
    }
    else if ((m_opcode & 0xF000) == 0x6000) { // 0x6XNN: set VX to NN
        m_v[ m_opcode & 0x0F00 ] = m_opcode & 0x00FF;
        m_pc += 2;
    }
    else if ((m_opcode & 0xF000) == 0x7000) { // 0x7XNN: add NN to VX
        m_v[ m_opcode & 0x0F00 ] += m_opcode & 0x00FF;
        m_pc += 2;
    }
    else if ((m_opcode & 0xF00F) == 0x8000) { // 0x8XY0: set VX to VY
        auto & vx = m_v[ (m_opcode & 0x0F00) >> 16 ];
        auto & vy = m_v[ (m_opcode & 0x00F0) >>  8 ];
        vx = vy;
        m_pc += 2;
    }
    else if ((m_opcode & 0xF00F) == 0x8005) { // 0x8XY5: VX -= VY
        // VF is set to 0 on borrow; 1 otherwise.
        auto & vx = m_v[ (m_opcode & 0x0F00) >> 16 ];
        auto & vy = m_v[ (m_opcode & 0x00F0) >>  8 ];
        m_v[0xF] = (vy > vx) ? 0 : 1;
        vx -= vy;
        m_pc += 2;
    }
    else if ((m_opcode & 0xF000) == 0xA000) { // 0xANNN: set I to NNN
        m_i   = m_opcode & 0x0FFF;
        m_pc += 2;
    }
    else if ((m_opcode & 0xF000) == 0xC000) { // 0xCXNN: VX = (rand & NN)
        // TODO : Replace with a per-Chip8 random number generator.
        m_v[ m_opcode & 0x0F00 ] = std::rand() & (m_opcode & 0x00FF);
        m_pc += 2;
    }
    else if ((m_opcode & 0xF000) == 0xD000) { // 0xDXYN
        // XOR-draw N rows of 8-bit-wide sprites from I
        // at (VX, VY), (VX, VY+1), etc.
        // VF set to 1 if a pixel is toggled off, otherwise 0.
        m_pc += 2;
    }
    else if ((m_opcode & 0xF0FF) == 0xE09E) { // 0xEX9E
        // skip next instruction if key at VX is pressed
        if (m_keyStates[ m_v[ (m_opcode & 0x0F00) >> 16 ] ])
            m_pc += 2;
        m_pc += 2;
    }
    else {
        std::fprintf(
            stderr,
            "Chip8: Bad opcode {0x%04X} at {0x%04X} ({0x%04X}).\n",
            m_opcode,
            m_pc,
            m_pc - s_progRomRamBegin
        );
    }

    // Update timers
    if (m_delayTimer)
        --m_delayTimer;
    if (m_soundTimer) {
        if (!--m_soundTimer) // sound if we just hit 0
            CSaruCore::Beep(); // beep the PC speaker
    }

}

//=====================================================================
bool Chip8::LoadProgram (const char * path) {

    std::FILE * progFile = std::fopen(path, "rb");
    if (!progFile) {
        std::fprintf(stderr, "Chip8: Failed to open program file at {%s}.\n", path);
        return false;
    }

    std::size_t readCount = std::fread(
        m_memory + s_progRomRamBegin,
        1, /* size of element to read (in bytes) */
        s_progRomRamEnd - s_progRomRamBegin, /* number of element to read */
        progFile
    );
    fclose(progFile);
    if (!readCount) {
        std::fprintf(stderr, "Chip8: Failed to read from program file {%s}.\n", path);
        return false;
    }

    return true;

}

//=====================================================================
void UpdateKeyStates () {
}

