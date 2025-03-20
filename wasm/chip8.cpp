#include <cstdlib>
#include <ctime>
#include <cstdint>

const int SCREEN_WIDTH = 64;
const int SCREEN_HEIGHT = 32;
uint8_t screen[SCREEN_WIDTH * SCREEN_HEIGHT];

extern "C"
{

  // Initializes the CHIP-8 module (seed for randomness)
  void init_chip8()
  {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
  }

  // Updates the screen buffer with random 0s and 1s
  void updateScreen()
  {
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i)
    {
      // Generate a random intensity for blue (0 to 255)
      screen[i] = std::rand() % 256;
    }
  }

  // Returns a pointer to the screen buffer
  uint8_t *getScreen()
  {
    return screen;
  }

  // Returns the screen width (64)
  int getScreenWidth()
  {
    return SCREEN_WIDTH;
  }

  // Returns the screen height (32)
  int getScreenHeight()
  {
    return SCREEN_HEIGHT;
  }

} // extern "C"