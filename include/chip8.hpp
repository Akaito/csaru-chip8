// This is *heavily*  based on Laurence Muller's tutorial at
// http://www.multigesture.net/articles/how-to-write-an-emulator-chip-8-interpreter/

#include <csaru-core-cpp/csaru-core-cpp.h>

struct Chip8 {
    static const unsigned s_memoryBytes   = 4096;
    static const unsigned s_registerCount =   16;
    static const unsigned s_renderWidth   =   64;
    static const unsigned s_renderHeight  =   32;
    static const unsigned s_stackSize     =   16;
    static const unsigned s_keyCount      =   16;

    static const uint16_t s_interpreterBegin = 0x000;
    static const uint16_t s_interpreterEnd   = 0x1FF;
    static const uint16_t s_fontBegin        = 0x050;
    static const uint16_t s_fontEnd          = 0x0A0;
    static const uint16_t s_progRomRamBegin  = 0x200;
    static const uint16_t s_progRomRamEnd    = 0xFFF;

    uint8_t  m_memory[s_memoryBytes];
    uint8_t  m_v[s_registerCount]; // registers V1-VF
    uint16_t m_i;  // index register
    uint16_t m_pc; // program counter
    uint8_t  m_delayTimer; // decrement if not 0
    uint8_t  m_soundTimer; // decrement if not 0; beep at 0
    uint8_t  m_keyStates[s_keyCount]; // hex keypad buttonstates

	uint16_t m_opcode;
    uint16_t m_stack[s_stackSize];
    uint8_t  m_sp; // stack pointer
    uint8_t  m_renderOut[s_renderWidth * s_renderHeight]; // 64x32 B&W display

    bool     m_drawFlag; // whether or not a GUI application should render

    void Initialize (unsigned randSeed);
    bool LoadProgram (const char * path);
    void EmulateCycle ();

    //void UpdateKeyStates ();
	//void KeyDown (uint8_t key);
	//void KeyUp (uint8_t key);
};

