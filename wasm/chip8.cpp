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
  void init_chip8()
  {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    cls();
    memset(V, 0, sizeof(V));
    I = 0;
    pc = 0x200;
  }

  /**
   * @brief Executes one cycle (one opcode) of the Chip-8 interpreter.
   *
   * Chip-8 stores instructions (opcodes) in memory, starting at addresses 0x200 and up.
   * Each instruction is 2 bytes (16 bits). The program counter (pc) points to the current
   * instruction. This function:
   *   1. Fetches the 16-bit opcode from memory[pc].
   *   2. Decodes the opcode by looking at its high nibble (and sometimes more).
   *   3. Executes the corresponding Chip-8 instruction (e.g., clear screen, jump, draw sprite).
   *   4. Advances the program counter (pc) unless the instruction itself modifies pc (e.g. jump).
   *
   * The instructions implemented here are:
   *   - 00E0  (CLS)
   *   - 00EE  (RET) [Stubbed]
   *   - 1NNN  (JP addr)
   *   - 2NNN  (CALL addr) [Stubbed]
   *   - 3XNN  (SE Vx, byte)
   *   - 6XNN  (LD Vx, byte)
   *   - 7XNN  (ADD Vx, byte)
   *   - ANNN  (LD I, addr)
   *   - DXYN  (DRW Vx, Vy, nibble)
   *   - Fx..  (Stubbed for future instructions)
   *
   * Any unrecognized or unsupported opcode is logged and then skipped.
   */
  void emulateCycle()
  {
    // Fetch 2 bytes from memory, combine them into a 16-bit opcode:
    //   memory[pc] is the high byte, memory[pc + 1] is the low byte.
    //   E.g., if memory[pc] = 0xA2 and memory[pc+1] = 0xF0, opcode = 0xA2F0.
    uint16_t opcode = (memory[pc] << 8) | memory[pc + 1];

    // Use a switch on the highest nibble (opcode & 0xF000) to determine the instruction category.
    switch (opcode & 0xF000)
    {
    case 0x0000:
    {
      // Instructions in the 0x0NNN range typically deal with clearing the screen (00E0)
      // or returning from subroutines (00EE).
      if (opcode == 0x00E0)
      {
        /**
         * 00E0: CLS
         * Clear the screen. All pixels on the display are turned off (set to 0).
         * In this implementation, cls() simply zeros out the 'screen' array.
         */
        cls();
        pc += 2; // Move to the next instruction (2 bytes).
      }
      else if (opcode == 0x00EE)
      {
        /**
         * 00EE: RET
         * Return from a subroutine. Normally, this would pop the last address from
         * the stack and set pc to that address. Here, it's just logged and pc is advanced.
         * If your ROM uses subroutines, you'll need to implement a stack.
         */
        printf("Return opcode 0x00EE encountered, but subroutine support not implemented.\n");
        pc += 2;
      }
      else
      {
        // Any other 0x0NNN opcode is typically unimplemented or system-specific.
        printf("Unsupported 0x0000 opcode: 0x%04X\n", opcode);
        pc += 2;
      }
      break;
    }
    case 0x1000:
    {
      /**
       * 1NNN: JP addr
       * Jump to address NNN. The high nibble is 1, the low 3 nibbles specify the address.
       * E.g., 0x1ABC means pc = 0xABC on execution.
       */
      uint16_t address = opcode & 0x0FFF; // Extract the lower 12 bits as the address.
      pc = address;                       // Set program counter to that address.
      break;
    }
    case 0x2000:
    {
      /**
       * 2NNN: CALL addr
       * Call a subroutine at address NNN. This means:
       *   1) Push the current pc on the stack.
       *   2) pc = NNN.
       * Here, we just log it. A real implementation would maintain a stack pointer
       * and an array for stack addresses.
       */
      printf("Call opcode 0x%04X encountered, but subroutine support not implemented.\n", opcode);
      pc += 2; // Skip for now.
      break;
    }
    case 0x3000:
    {
      /**
       * 3XNN: SE Vx, byte
       * Skip next instruction if Vx == NN. If the value in register Vx equals NN,
       * pc is advanced by an additional 2 bytes (i.e., skip the next instruction).
       */
      uint8_t x = (opcode & 0x0F00) >> 8; // Extract register index X.
      uint8_t nn = opcode & 0x00FF;       // Extract the immediate byte NN.
      if (V[x] == nn)
      {
        pc += 4; // Skip next instruction (2 bytes per instruction).
      }
      else
      {
        pc += 2; // Just move to next instruction.
      }
      break;
    }
    case 0x6000:
    {
      /**
       * 6XNN: LD Vx, NN
       * Set register Vx to the immediate value NN.
       * E.g., 0x6A05 => V[A] = 0x05
       */
      uint8_t x = (opcode & 0x0F00) >> 8; // Register index X.
      uint8_t nn = opcode & 0x00FF;       // Immediate value NN.
      V[x] = nn;
      pc += 2;
      break;
    }
    case 0x7000:
    {
      /**
       * 7XNN: ADD Vx, byte
       * Add the immediate value NN to register Vx (no carry flag in standard Chip-8).
       * E.g., 0x7A01 => V[A] += 1
       */
      uint8_t x = (opcode & 0x0F00) >> 8; // Register index X.
      uint8_t nn = opcode & 0x00FF;       // Immediate value NN.
      V[x] += nn;
      pc += 2;
      break;
    }
    case 0xA000:
    {
      /**
       * ANNN: LD I, addr
       * Set the index register I to the address NNN.
       * E.g., 0xA2F0 => I = 0x2F0
       */
      I = opcode & 0x0FFF; // Extract the lower 12 bits as the address.
      pc += 2;
      break;
    }
    case 0xD000:
    {
      /**
       * DXYN: DRW Vx, Vy, nibble
       * Draw a sprite at coordinates (Vx, Vy) with a height of N rows (where N is the
       * lowest 4 bits of the opcode). Each row of the sprite is 8 bits wide, and is
       * read from memory starting at address I. The drawing is done by XORing each
       * sprite bit with the existing screen pixel.
       *
       * Format:
       *   - x = V[(opcode & 0x0F00) >> 8]  => the X coordinate
       *   - y = V[(opcode & 0x00F0) >> 4]  => the Y coordinate
       *   - height = opcode & 0x000F       => the number of sprite rows
       *
       * For each row:
       *   - spriteByte = memory[I + row]   => 8 bits of sprite data
       *   - bit 7 is the leftmost pixel, bit 0 is the rightmost
       *   - we XOR each pixel with the screen
       *
       * Note: In a full emulator, you'd also set VF if there's a collision. This minimal
       * version omits collision detection.
       */
      uint8_t x = V[(opcode & 0x0F00) >> 8];
      uint8_t y = V[(opcode & 0x00F0) >> 4];
      uint8_t height = opcode & 0x000F;

      for (int row = 0; row < height; row++)
      {
        uint8_t spriteByte = memory[I + row]; // Each row is 1 byte (8 pixels).
        for (int col = 0; col < 8; col++)
        {
          // Extract the bit (pixel) from spriteByte.
          // (spriteByte >> (7 - col)) & 1 => leftmost bit is col=0, rightmost is col=7.
          uint8_t spritePixel = (spriteByte >> (7 - col)) & 0x1;

          // Compute the screen coordinates.
          int sx = (x + col) % SCREEN_WIDTH;
          int sy = (y + row) % SCREEN_HEIGHT;

          // XOR the pixel: if the pixel was 0, it becomes spritePixel;
          // if it was 1, it flips (1 -> 0 or 0 -> 1).
          screen[sy * SCREEN_WIDTH + sx] ^= spritePixel;
        }
      }

      pc += 2; // Move to the next instruction.
      break;
    }
    case 0xF000:
    {
      /**
       * Fx?? instructions can handle timers, keyboard input, storing/loading registers,
       * and other advanced features. We have not implemented them here, so we log them.
       */
      printf("Unsupported Fx opcode: 0x%04X\n", opcode);
      pc += 2;
      break;
    }
    default:
    {
      /**
       * If the high nibble is something we haven't handled (e.g., 4, 5, 8, 9, B, C, E),
       * or an unrecognized pattern, we log and skip.
       */
      printf("Unsupported opcode: 0x%04X\n", opcode);
      pc += 2;
      break;
    }
    }
  }

  // Run a specified number of cycles.
  void runCycles(int numCycles)
  {
    for (int i = 0; i < numCycles; i++)
    {
      emulateCycle();
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

} // extern "C"