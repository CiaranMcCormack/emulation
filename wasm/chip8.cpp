#include <cstdlib>
#include <ctime>
#include <cstdint>
#include <cstring>
#include <stdio.h>

const int SCREEN_WIDTH = 64;
const int SCREEN_HEIGHT = 32;

// The screen buffer holds 1-bit values for each pixel.
uint8_t screen[SCREEN_WIDTH * SCREEN_HEIGHT];

// Chip‑8 has 4K of memory, 16 registers (V0–VF), an index register, and a program counter.
uint8_t memory[4096];
uint8_t V[16];
uint16_t I;
uint16_t pc;

uint16_t stack[16]; // Stack to hold return addresses (Chip-8 typically supports 16 levels)
uint8_t sp = 0;     // Stack pointer, initially 0 (points to the next free slot)

uint8_t delayTimer = 0; // Delay timer: decrements at 60 Hz (in a full emulator)
uint8_t soundTimer = 0; // Sound timer: decrements at 60 Hz and triggers sound when > 0

uint8_t keys[16]; // Keypad state: 16 keys, each with a value of 0 (up) or 1 (down)

float timerAccumulator = 0.0f;                   // Accumulated time for timers
const float TIMER_INTERVAL_MS = 1000.0f / 60.0f; // ~16.67 ms at 60Hz

// Load the built‑in Chip‑8 fontset (16 characters × 5 bytes each) at memory address 0x50
static constexpr uint8_t FONTSET[80] = {
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
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

// Clear the screen by zeroing the screen buffer.
void cls()
{
  memset(screen, 0, SCREEN_WIDTH * SCREEN_HEIGHT);
}

extern "C"
{
  // Load a Chip‑8 program into memory starting at 0x200.
  void loadProgram(uint8_t *program, int size)
  {
    memcpy(memory + 0x200, program, size);
    pc = 0x200;
  }

  // Initialize the Chip‑8 state.
  void init()
  {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    cls();
    memset(V, 0, sizeof(V));
    I = 0;
    pc = 0x200;

    memcpy(memory + 0x50, FONTSET, sizeof(FONTSET));
  }

  /**
   * Executes one cycle (one opcode) of the Chip-8 interpreter.
   *
   * This function fetches the next 2-byte opcode from memory at the address indicated by
   * the program counter (pc), decodes it by inspecting its most significant nibble (and sometimes more),
   * executes the corresponding operation, and then updates pc appropriately.
   *
   * Supported instructions include:
   *   - 00E0: CLS            - Clear the display.
   *   - 00EE: RET            - Return from a subroutine (requires stack support).
   *   - 1NNN: JP addr        - Jump to address NNN.
   *   - 2NNN: CALL addr      - Call subroutine at address NNN (stack support required).
   *   - 3XNN: SE Vx, byte    - Skip next instruction if Vx equals NN.
   *   - 4XNN: SNE Vx, byte   - Skip next instruction if Vx does NOT equal NN.
   *   - 6XNN: LD Vx, byte    - Load immediate value NN into register Vx.
   *   - 7XNN: ADD Vx, byte   - Add immediate value NN to register Vx (no carry).
   *   - 8XY0: LD Vx, Vy      - Set Vx = Vy.
   *   - 8XY1: OR Vx, Vy      - Set Vx = Vx OR Vy.
   *   - 8XY2: AND Vx, Vy     - Set Vx = Vx AND Vy.
   *   - 8XY3: XOR Vx, Vy     - Set Vx = Vx XOR Vy.
   *   - 8XY4: ADD Vx, Vy     - Add Vy to Vx; set VF = carry.
   *   - 8XY5: SUB Vx, Vy     - Subtract Vy from Vx; set VF = NOT borrow.
   *   - 8XY6: SHR Vx         - Shift Vx right by 1; VF set to least significant bit.
   *   - 8XY7: SUBN Vx, Vy    - Set Vx = Vy - Vx; VF = NOT borrow.
   *   - 8XYE: SHL Vx         - Shift Vx left by 1; VF set to most significant bit.
   *   - 9XY0: SNE Vx, Vy     - Skip next instruction if Vx != Vy.
   *   - ANNN: LD I, addr     - Set index register I = NNN.
   *   - BNNN: JP V0, addr    - Jump to address NNN plus V0.
   *   - CXNN: RND Vx, byte   - Set Vx = (random byte) AND NN.
   *   - DXYN: DRW Vx, Vy, nibble - Draw sprite at (Vx, Vy) with height N.
   *   - EX9E: SKP Vx         - Skip next instruction if key with value Vx is pressed.
   *   - EXA1: SKNP Vx        - Skip next instruction if key with value Vx is NOT pressed.
   *   - Fx07: LD Vx, DT      - Load delay timer value into Vx.
   *   - Fx0A: LD Vx, K       - Wait for a key press, then store that key’s value in Vx.
   *   - Fx15: LD DT, Vx      - Set delay timer to value in Vx.
   *   - Fx18: LD ST, Vx      - Set sound timer to value in Vx.
   *   - Fx1E: ADD I, Vx      - Add Vx to index register I.
   *   - Fx29: LD F, Vx       - Set I to the location of the sprite for the hex digit in Vx.
   *   - Fx33: LD B, Vx       - Store BCD representation of Vx in memory at I, I+1, and I+2.
   *   - Fx55: LD [I], V0..Vx - Store registers V0 through Vx in memory starting at I.
   *   - Fx65: LD V0..Vx, [I] - Read registers V0 through Vx from memory starting at I.
   *
   * Any unsupported opcode is logged and skipped.
   */
  void emulateCycle()
  {
    // Fetch the opcode: combine two consecutive bytes into one 16-bit value.
    uint16_t opcode = (memory[pc] << 8) | memory[pc + 1];

    // Decode the opcode by examining its most significant nibble.
    switch (opcode & 0xF000)
    {
    case 0x0000:
    {
      // Opcodes in the 0x0000 range are system instructions.
      if (opcode == 0x00E0)
      {
        /**
         * 00E0 - CLS: Clear the display.
         * This instruction clears the entire screen by zeroing out the 'screen' array.
         */
        cls();
        pc += 2;
      }
      else if (opcode == 0x00EE)
      {
        /**
         * 00EE - RET: Return from a subroutine.
         * Normally, this instruction pops the last address off a stack and sets pc to that address.
         * Here, if stack support is implemented, we pop from the stack; otherwise, log and advance.
         */
        if (sp > 0)
        {
          sp--;
          pc = stack[sp];
        }
        else
        {
          printf("Stack underflow on RET opcode: 0x%04X\n", opcode);
          pc += 2;
        }
      }
      else
      {
        // Unsupported or system-specific 0x0NNN opcode.
        printf("Unsupported 0x0000 opcode: 0x%04X\n", opcode);
        pc += 2;
      }
      break;
    }
    case 0x1000:
    {
      /**
       * 1NNN - JP addr: Jump to address NNN.
       * Sets the program counter to the address specified by the lower 12 bits of the opcode.
       */
      uint16_t address = opcode & 0x0FFF;
      pc = address;
      break;
    }
    case 0x2000:
    {
      /**
       * 2NNN - CALL addr: Call subroutine at address NNN.
       * Pushes the current pc+2 onto the stack, increments the stack pointer,
       * and sets pc to the address NNN.
       */
      if (sp < 16)
      {
        stack[sp] = pc + 2;
        sp++;
        pc = opcode & 0x0FFF;
      }
      else
      {
        printf("Stack overflow on CALL opcode: 0x%04X\n", opcode);
        pc += 2;
      }
      break;
    }
    case 0x3000:
    {
      /**
       * 3XNN - SE Vx, byte: Skip next instruction if Vx equals NN.
       * If register Vx equals NN, pc is increased by 4; otherwise, by 2.
       */
      uint8_t x = (opcode & 0x0F00) >> 8;
      uint8_t nn = opcode & 0x00FF;
      if (V[x] == nn)
        pc += 4;
      else
        pc += 2;
      break;
    }
    case 0x4000:
    {
      /**
       * 4XNN - SNE Vx, byte: Skip next instruction if Vx does NOT equal NN.
       * If register Vx does not equal NN, pc is increased by 4; otherwise, by 2.
       */
      uint8_t x = (opcode & 0x0F00) >> 8;
      uint8_t nn = opcode & 0x00FF;
      if (V[x] != nn)
        pc += 4;
      else
        pc += 2;
      break;
    }
    case 0x5000:
    {
      /**
       * 5XY0 — SNE Vx, Vy: Skip next instruction if Vx ≠ Vy.
       * If the value in register Vx does NOT equal the value in Vy,
       * advance pc by 4 (skipping one 2‑byte opcode). Otherwise advance by 2.
       */
      uint8_t x = (opcode & 0x0F00) >> 8;
      uint8_t y = (opcode & 0x00F0) >> 4;
      if (V[x] != V[y])
      {
        pc += 4;
      }
      else
      {
        pc += 2;
      }
      break;
    }
    case 0x6000:
    {
      /**
       * 6XNN - LD Vx, byte: Load immediate value NN into register Vx.
       * E.g., 0x6A05 loads the value 0x05 into register VA.
       */
      uint8_t x = (opcode & 0x0F00) >> 8;
      uint8_t nn = opcode & 0x00FF;
      V[x] = nn;
      pc += 2;
      break;
    }
    case 0x7000:
    {
      /**
       * 7XNN - ADD Vx, byte: Add immediate value NN to register Vx.
       * This operation does not affect any carry flag.
       */
      uint8_t x = (opcode & 0x0F00) >> 8;
      uint8_t nn = opcode & 0x00FF;
      V[x] += nn;
      pc += 2;
      break;
    }
    case 0x8000:
    {
      /**
       * 8XY_ instructions: Arithmetic and logical operations between registers Vx and Vy.
       * The lowest nibble of the opcode determines the operation.
       */
      uint8_t x = (opcode & 0x0F00) >> 8;
      uint8_t y = (opcode & 0x00F0) >> 4;
      switch (opcode & 0x000F)
      {
      case 0x0:
        /**
         * 8XY0 - LD Vx, Vy: Set Vx = Vy.
         */
        V[x] = V[y];
        pc += 2;
        break;
      case 0x1:
        /**
         * 8XY1 - OR Vx, Vy: Set Vx = Vx OR Vy.
         */
        V[x] |= V[y];
        pc += 2;
        break;
      case 0x2:
        /**
         * 8XY2 - AND Vx, Vy: Set Vx = Vx AND Vy.
         */
        V[x] &= V[y];
        pc += 2;
        break;
      case 0x3:
        /**
         * 8XY3 - XOR Vx, Vy: Set Vx = Vx XOR Vy.
         */
        V[x] ^= V[y];
        pc += 2;
        break;
      case 0x4:
      {
        /**
         * 8XY4 - ADD Vx, Vy: Add Vy to Vx.
         * Set VF to 1 if there is a carry, else 0.
         */
        uint16_t sum = V[x] + V[y];
        V[0xF] = (sum > 0xFF) ? 1 : 0;
        V[x] = sum & 0xFF;
        pc += 2;
        break;
      }
      case 0x5:
      {
        /**
         * 8XY5 - SUB Vx, Vy: Subtract Vy from Vx.
         * Set VF to 1 if Vx > Vy (no borrow), else 0.
         */
        V[0xF] = (V[x] > V[y]) ? 1 : 0;
        V[x] = V[x] - V[y];
        pc += 2;
        break;
      }
      case 0x6:
      {
        /**
         * 8XY6 - SHR Vx: Shift Vx right by 1.
         * The least significant bit of Vx is stored in VF.
         */
        V[0xF] = V[x] & 0x1;
        V[x] >>= 1;
        pc += 2;
        break;
      }
      case 0x7:
      {
        /**
         * 8XY7 - SUBN Vx, Vy: Set Vx = Vy - Vx.
         * Set VF to 1 if Vy > Vx (no borrow), else 0.
         */
        V[0xF] = (V[y] > V[x]) ? 1 : 0;
        V[x] = V[y] - V[x];
        pc += 2;
        break;
      }
      case 0xE:
      {
        /**
         * 8XYE - SHL Vx: Shift Vx left by 1.
         * The most significant bit of Vx is stored in VF.
         */
        V[0xF] = (V[x] & 0x80) >> 7;
        V[x] <<= 1;
        pc += 2;
        break;
      }
      default:
        printf("Unsupported 8XY_ opcode: 0x%04X\n", opcode);
        pc += 2;
        break;
      }
      break;
    }
    case 0x9000:
    {
      /**
       * 9XY0 - SNE Vx, Vy: Skip next instruction if Vx != Vy.
       */
      uint8_t x = (opcode & 0x0F00) >> 8;
      uint8_t y = (opcode & 0x00F0) >> 4;
      pc += (V[x] != V[y]) ? 4 : 2;
      break;
    }
    case 0xA000:
    {
      /**
       * ANNN - LD I, addr: Load the 12-bit address NNN into the index register I.
       */
      I = opcode & 0x0FFF;
      pc += 2;
      break;
    }
    case 0xB000:
    {
      /**
       * BNNN - JP V0, addr: Jump to address NNN plus the value of V0.
       */
      uint16_t address = opcode & 0x0FFF;
      pc = address + V[0];
      break;
    }
    case 0xC000:
    {
      /**
       * CXNN - RND Vx, byte: Set Vx = (random byte) AND NN.
       * Generates a random number between 0 and 255, ANDs it with NN, and stores the result in Vx.
       */
      uint8_t x = (opcode & 0x0F00) >> 8;
      uint8_t nn = opcode & 0x00FF;
      V[x] = (std::rand() % 256) & nn;
      pc += 2;
      break;
    }
    case 0xD000:
    {
      /**
       * DXYN - DRW Vx, Vy, nibble: Draw a sprite at (Vx, Vy) with height N.
       * The sprite is read from memory starting at address I, where each row is 8 bits wide.
       * Drawing is performed using XOR, toggling the pixels on the screen.
       * VF is set to 1 if any pixel is erased (collision), otherwise 0.
       */
      uint8_t x = V[(opcode & 0x0F00) >> 8];
      uint8_t y = V[(opcode & 0x00F0) >> 4];
      uint8_t height = opcode & 0x000F;
      uint8_t collision = 0;

      for (int row = 0; row < height; row++)
      {
        uint8_t spriteByte = memory[I + row];
        for (int col = 0; col < 8; col++)
        {
          uint8_t spritePixel = (spriteByte >> (7 - col)) & 0x1;
          int sx = (x + col) % SCREEN_WIDTH;
          int sy = (y + row) % SCREEN_HEIGHT;
          // Check existing pixel before XOR
          if (screen[sy * SCREEN_WIDTH + sx] && spritePixel)
          {
            collision = 1;
          }
          screen[sy * SCREEN_WIDTH + sx] ^= spritePixel;
        }
      }
      V[0xF] = collision; // Set VF = collision flag
      pc += 2;
      break;
    }
    case 0xE000:
    {
      /**
       * EX9E / EXA1 - Key input instructions.
       * EX9E: Skip next instruction if the key corresponding to the value in Vx is pressed.
       * EXA1: Skip next instruction if the key corresponding to the value in Vx is NOT pressed.
       * The key state is determined by a global keys array (keys[0] through keys[15]).
       */
      uint8_t x = (opcode & 0x0F00) >> 8;
      uint8_t key = V[x] & 0x0F; // Chip-8 keys are in the range 0-F.
      switch (opcode & 0x00FF)
      {
      case 0x9E:
        pc += (keys[key] ? 4 : 2);
        break;
      case 0xA1:
        pc += (!keys[key] ? 4 : 2);
        break;
      default:
        printf("Unsupported E- prefix opcode: 0x%04X\n", opcode);
        pc += 2;
        break;
      }
      break;
    }
    case 0xF000:
    {
      /**
       * Fx-- instructions cover a range of operations.
       * In this implementation, we support:
       *
       * Fx07 - LD Vx, DT   : Load the current delay timer value into Vx.
       * Fx0A - LD Vx, K    : Wait for a key press, then store that key’s value in Vx.
       * Fx15 - LD DT, Vx   : Set the delay timer to the value in Vx.
       * Fx18 - LD ST, Vx   : Set the sound timer to the value in Vx.
       * Fx1E - ADD I, Vx   : Add Vx to the index register I.
       * Fx29 - LD F, Vx    : Set I to the location of the sprite for the hexadecimal digit in Vx.
       * Fx33 - LD B, Vx    : Store the BCD representation of Vx in memory at I, I+1, and I+2.
       * Fx55 - LD [I], V0..Vx  : Store registers V0 through Vx in memory starting at I.
       * Fx65 - LD V0..Vx, [I]  : Read registers V0 through Vx from memory starting at I.
       */
      uint8_t x = (opcode & 0x0F00) >> 8;
      uint8_t kk = opcode & 0x00FF;
      switch (kk)
      {
      case 0x07:
        // Fx07: LD Vx, DT – Load delay timer into Vx.
        V[x] = delayTimer;
        pc += 2;
        break;
      case 0x0A:
      {
        /**
         * Fx0A - LD Vx, K: Wait for a key press, then store that key’s value in Vx.
         * Execution should pause here (pc does NOT advance) until any Chip‑8 key (0x0–0xF)
         * is pressed. Once pressed, store the key index in Vx and increment pc.
         */
        bool pressed = false;
        for (int k = 0; k < 16; k++)
        {
          if (keys[k])
          {
            V[x] = k;
            pressed = true;
            break;
          }
        }
        if (pressed)
        {
          pc += 2;
        }
        // If no key is down, do NOT advance pc — effectively “blocking” until input
        break;
      }
      case 0x15:
        // Fx15: LD DT, Vx – Set delay timer to the value in Vx.
        delayTimer = V[x];
        pc += 2;
        break;
      case 0x18:
        // Fx18: LD ST, Vx – Set sound timer to the value in Vx.
        soundTimer = V[x];
        pc += 2;
        break;
      case 0x1E:
        // Fx1E: ADD I, Vx – Add Vx to the index register I.
        I += V[x];
        pc += 2;
        break;
      case 0x29:
        // Fx29: LD F, Vx – Set I to the location of the sprite for the hexadecimal digit in Vx.
        // Conventionally, the font sprites are stored in memory starting at address 0x50, with each sprite 5 bytes long.
        I = 0x50 + (V[x] * 5);
        pc += 2;
        break;
      case 0x33:
      {
        // Fx33: LD B, Vx – Store the BCD representation of Vx in memory at I, I+1, and I+2.
        uint8_t value = V[x];
        memory[I] = value / 100;
        memory[I + 1] = (value / 10) % 10;
        memory[I + 2] = value % 10;
        pc += 2;
        break;
      }
      case 0x55:
      {
        // Fx55: LD [I], V0..Vx – Store registers V0 through Vx in memory starting at I.
        for (int i = 0; i <= x; i++)
        {
          memory[I + i] = V[i];
        }
        pc += 2;
        break;
      }
      case 0x65:
      {
        // Fx65: LD V0..Vx, [I] – Read registers V0 through Vx from memory starting at I.
        for (int i = 0; i <= x; i++)
        {
          V[i] = memory[I + i];
        }
        pc += 2;
        break;
      }
      default:
        printf("Unsupported Fx opcode: 0x%04X\n", opcode);
        pc += 2;
        break;
      }
      break;
    }
    default:
    {
      /**
       * For any opcode that doesn't match the above cases,
       * log the unsupported opcode and move to the next instruction.
       */
      printf("Unsupported opcode: 0x%04X\n", opcode);
      pc += 2;
      break;
    }
    }
  }

  void updateTimers()
  {
    if (delayTimer > 0)
      delayTimer--;
    if (soundTimer > 0)
      soundTimer--;
  }

  // Run a specified number of cycles.
  void run(int numCycles, double deltaMs)
  {
    // 1) Run CPU cycles
    for (int i = 0; i < numCycles; i++)
    {
      emulateCycle();
    }

    // 2) Accumulate time, decrement timers at 60 Hz
    timerAccumulator += (float)deltaMs;
    while (timerAccumulator >= TIMER_INTERVAL_MS)
    {
      updateTimers();
      timerAccumulator -= TIMER_INTERVAL_MS;
    }
  }

  // Return a pointer to the screen buffer.
  uint8_t *getScreen()
  {
    return screen;
  }

  // Return the screen width.
  int getScreenWidth()
  {
    return SCREEN_WIDTH;
  }

  // Return the screen height.
  int getScreenHeight()
  {
    return SCREEN_HEIGHT;
  }

  uint8_t getSoundTimer()
  {
    return soundTimer;
  }

  // Mark a key as pressed.
  void setKeyDown(int key)
  {
    if (key >= 0 && key < 16)
    {
      keys[key] = 1;
    }
  }

  // Mark a key as released.
  void setKeyUp(int key)
  {
    if (key >= 0 && key < 16)
    {
      keys[key] = 0;
    }
  }

} // extern "C"