// This is *heavily*  based on Laurence Muller's tutorial at
// http://www.multigesture.net/articles/how-to-write-an-emulator-chip-8-interpreter/

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
void Chip8::Initialize () {

    m_pc     = s_progRomRamBegin;
    m_opcode = 0x0000;
    m_i      = 0x0000;
    m_sp     = 0x00;

    std::memcpy(m_memory + s_fontBegin, s_fontSet, sizeof(s_fontSet));

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
    else if ((m_opcode & 0xF000) == 0x6000) { // 0x6XNN: set VX to NN
        m_v[ m_opcode & 0x0F00 ] = m_opcode & 0x00FF;
        m_pc += 2;
    }
    else if ((m_opcode & 0xF000) == 0xA000) { // 0xANNN: set I to NNN
        m_i   = m_opcode & 0x0FFF;
        m_pc += 2;
    }
    else if ((m_opcode & 0xF000) == 0xD000) { // 0xDXYN
        // XOR-draw N rows of 8-bit-wide sprites from I
        // at (VX, VY), (VX, VY+1), etc.
        // VF set to 1 if a pixel is toggled off, otherwise 0.
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

