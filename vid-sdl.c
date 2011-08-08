#include "vid.h"

#include <SDL/SDL.h>
#include "platform-sdl.h"

static struct SDL_Window    *window;
static struct SDL_GLContext *context;

int kl_vid_init() {
  SDL_DisplayMode mode;

  kl_sdl_init();

  SDL_InitSubSystem(SDL_INIT_VIDEO);
  
  if (SDL_GetDisplayMode(0, 0, &mode) < 0) {
    fprintf(stderr, "vid-sdl -> SDL_GetDisplayMode error: \"%s\"\n", SDL_GetError());
    return -1;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,   24);

  window = SDL_CreateWindow("Kludge Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, mode.w, mode.h, SDL_WINDOW_FULLSCREEN|SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
  if (window == NULL) {
    fprintf(stderr, "vid-sdl -> SDL_CreateWindow error: \"%s\"\n", SDL_GetError());
    return -1;
  }

  context = SDL_GL_CreateContext(window);

  SDL_GL_SetSwapInterval(0); /* VSync disabled */

  return 0;
}

void kl_vid_size(int *w, int *h) {
  SDL_GetWindowSize(window, w, h);
}

void kl_vid_swap() {
  SDL_GL_SwapWindow(window);
}
