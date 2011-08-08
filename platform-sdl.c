#include "platform-sdl.h"

#include <SDL/SDL.h>

void kl_sdl_init() {
  static int initialized = 0;
  if (!initialized) {
    SDL_Init(0);
    initialized = 1;
  }
}
