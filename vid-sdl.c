#include "vid.h"

#include <SDL/SDL.h>
#include "platform-sdl.h"

struct SDL_Window    *kl_vid_sdl_window;
struct SDL_GLContext *kl_vid_sdl_context;

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

  //kl_vid_sdl_window = SDL_CreateWindow("Kludge Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, mode.w, mode.h, SDL_WINDOW_FULLSCREEN|SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
  kl_vid_sdl_window = SDL_CreateWindow("Kludge Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1440, 900, SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
  if (kl_vid_sdl_window == NULL) {
    fprintf(stderr, "vid-sdl -> SDL_CreateWindow error: \"%s\"\n", SDL_GetError());
    return -1;
  }

  kl_vid_sdl_context = SDL_GL_CreateContext(kl_vid_sdl_window);

  SDL_GL_SetSwapInterval(0); /* VSync disabled */

  //SDL_SetWindowGrab(kl_vid_sdl_window, SDL_TRUE);

  return 0;
}

void kl_vid_size(int *w, int *h) {
  SDL_GetWindowSize(kl_vid_sdl_window, w, h);
}

void kl_vid_swap() {
  SDL_GL_SwapWindow(kl_vid_sdl_window);
}
