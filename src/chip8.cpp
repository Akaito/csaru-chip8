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

    // Fetch opcode.
    m_opcode = m_memory[m_pc] << 8 | m_memory[m_pc + 1];

    // Prepare common portions of opcode.
    const uint8_t  x   = (m_opcode & 0x0F00) >> 8;
    auto &         vx  = m_v[x];
    const uint8_t  y   = (m_opcode & 0x00F0) >> 4;
    auto &         vy  = m_v[y];
    const uint8_t  n   = m_opcode & 0x000F;
    const uint8_t  nn  = m_opcode & 0x00FF;
    const uint16_t nnn = m_opcode & 0x0FFF;

    // Decode opcode
	// https://en.wikipedia.org/wiki/CHIP-8#Opcode_table
    if (m_opcode == 0x00E0) { // 0x00E0: clear the screen
		CSaruCore::SecureZero(m_renderOut, s_renderWidth * s_renderHeight);
        m_pc += 2;
    }
    else if (m_opcode == 0x00EE) { // 0x00EE: return from call
		if (m_sp <= 0) {
			std::fprintf(
				stderr,
				"Chip8: stack underflow; pc {0x%04X}.",
				m_pc
			);
		}
		else
			m_pc = m_stack[--m_sp] + 2;
    }
	else if ((m_opcode & 0xF000) == 0x0000) { // 0x0NNN
		// call RCA 1802 program at NNN
		// TODO : jump, or call?
		m_pc = nnn;
	}
    else if ((m_opcode & 0xF000) == 0x1000) { // 0x1NNN: jump to NNN
        m_pc = nnn;
    }
    else if ((m_opcode & 0xF000) == 0x2000) { // 0x2NNN: call NNN
		if (m_sp >= s_stackSize) {
			std::fprintf(
				stderr,
				"Chip8: stack overflow; pc {0x%04X}.",
				m_pc
			);
		}
		else {
			m_stack[m_sp++] = m_pc;
			m_pc = m_opcode & 0x0FFF;
		}
    }
    else if ((m_opcode & 0xF000) == 0x3000) { // 0x3XNN
        // skip next instruction if VX == NN
        m_pc += (vx == nn) ? 4 : 2;
    }
    else if ((m_opcode & 0xF000) == 0x4000) { // 0x4XNN
        // skip next instruction if VX != NN
		m_pc += (vx != nn) ? 4 : 2;
    }
    else if ((m_opcode & 0xF000) == 0x6000) { // 0x6XNN: set VX to NN
		vx = nn;
        m_pc += 2;
    }
    else if ((m_opcode & 0xF000) == 0x7000) { // 0x7XNN: add NN to VX
		vx += nn;
        m_pc += 2;
    }
    else if ((m_opcode & 0xF00F) == 0x8000) { // 0x8XY0: set VX to VY
        vx = vy;
        m_pc += 2;
    }
	else if ((m_opcode & 0xF00F) == 0x8004) { // 0x8XY4
		// add VY to VX; VF is set to 1 on carry, 0 otherwise.
		m_v[0xF] = ((vx + vy) < vx) ? 1 : 0;
		vx += vy;
		m_pc += 2;
	}
    else if ((m_opcode & 0xF00F) == 0x8005) { // 0x8XY5: VX -= VY
        // VF is set to 0 on borrow; 1 otherwise.
        m_v[0xF] = (vy > vx) ? 0 : 1;
        vx -= vy;
        m_pc += 2;
    }
	else if ((m_opcode & 0xF00F) == 0x8006) { // 0x8XY6
		// VX >>= 1; VF = least-significant bit before shift
		m_v[0xF] = vx & 0x01;
		vx >>= 1;
		m_pc += 2;
	}
	else if ((m_opcode & 0xF00F) == 0x800E) { // 0x8XYE
		// VX <<= 1; VF = most-significant bit before shift
		m_v[0xF] = vx & 0x80;
		vx <<= 1;
		m_pc += 2;
	}
	else if ((m_opcode & 0xF00F) == 0x9000) { // 0x9XY0
		// skip next instruction if VX != VY
		m_pc += (vx != vy) ? 4 : 2;
	}
    else if ((m_opcode & 0xF000) == 0xA000) { // 0xANNN: set I to NNN
        m_i   = m_opcode & 0x0FFF;
        m_pc += 2;
    }
    else if ((m_opcode & 0xF000) == 0xC000) { // 0xCXNN: VX = (rand & NN)
        // TODO : Replace with a per-Chip8 random number generator.
        vx = std::rand() & nn;
        m_pc += 2;
    }
    else if ((m_opcode & 0xF000) == 0xD000) { // 0xDXYN
        // XOR-draw N rows of 8-bit-wide sprites from I
        // at (VX, VY), (VX, VY+1), etc.
        // VF set to 1 if a pixel is toggled off, otherwise 0.

		m_v[0xF] = 0x0; // clear collision flag

		for (unsigned spriteTexY = 0; spriteTexY < n; ++spriteTexY) {
			const uint8_t spriteByte = m_memory[ m_i + spriteTexY ];
			for (unsigned spriteTexX = 0; spriteTexX < 8; ++spriteTexX) {
				// shift b1000'0000 right to current column
				if (spriteByte & (0x80 >> spriteTexX)) {
					auto & renderPixel = m_renderOut[
						(vy + spriteTexY) * s_renderWidth +
						vx + spriteTexX
					];
					if (renderPixel) {
						renderPixel = 0;
						m_v[0xF]    = 0x1; // collision!  set flag
					}
					else {
						renderPixel = 1;
					}
				}
			}
		}

        m_pc       += 2;
		m_drawFlag  = true;
    }
    else if ((m_opcode & 0xF0FF) == 0xE09E) { // 0xEX9E
        // skip next instruction if key at VX is pressed
		m_pc += (m_keyStates[vx]) ? 4 : 2;
    }
    else if ((m_opcode & 0xF0FF) == 0xF007) { // 0xFX07: VX = delay
        vx    = m_delayTimer;
        m_pc += 2;
    }
	else if ((m_opcode & 0xF0FF) == 0xF00A) { // 0xFX0A
		// wait for key press, then store in VX
		for (uint8_t key = 0x0; key < s_keyCount; ++key) {
			if (m_keyStates[key]) {
				m_v[ (m_opcode & 0x0F00) >> 8 ] = key;
				m_pc += 2;
			}
		}
	}
    else if ((m_opcode & 0xF0FF) == 0xF015) { // 0xFX15: delay = VX
        m_delayTimer  = vx;
        m_pc         += 2;
    }
	else if ((m_opcode & 0xF0FF) == 0xF01E) { // 0xFX1E: I += VX
		// undocumented: VF = 1 on overflow; 0 otherwise
		// (used by "Spaceflight 2091!")
		m_v[0xF]  = (m_i + vx < m_i) ? 1 : 0;
		m_i      += vx;
	}
	else if ((m_opcode & 0xF0FF) == 0xF029) { // 0xFX29
		// set I to location of character VX
		m_i = s_fontBegin + vx;
		m_pc += 2;
	}
	else if ((m_opcode & 0xF0FF) == 0xF065) { // 0xFX65
		// fill V0 to VX from memory starting at I
		// TODO : inclusive or exclusive?
		//for (uint8_t i = 0; i < vx; ++i)
		for (uint8_t i = 0; i <= vx; ++i)
			m_v[i] = m_memory[m_i + i];
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

