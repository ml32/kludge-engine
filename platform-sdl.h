#ifndef KL_SDL_H
#define KL_SDL_H

#include <SDL/SDL.h>

extern struct SDL_Window    *kl_vid_sdl_window;
extern struct SDL_GLContext *kl_vid_sdl_context;

void kl_sdl_init();

#endif /* KL_SDL_H */
