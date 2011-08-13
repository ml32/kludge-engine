#include "input.h"

#include "platform-sdl.h"
#include "vid.h"

#include <SDL/SDL.h>

int kl_input_init() {
  kl_sdl_init();
  SDL_ShowCursor(SDL_DISABLE);
  return 0;
}

static int tocode(int sdlcode) {
  switch (sdlcode) {
    case SDLK_ESCAPE:
      return KL_BTN_ESC;
    case SDLK_RETURN:
      return KL_BTN_ENTER;
    case SDLK_LSHIFT:
      return KL_BTN_SHIFT;
    case SDLK_LCTRL:
      return KL_BTN_CTRL;
    case SDLK_LALT:
      return KL_BTN_ALT;
    case SDLK_F1:
      return KL_BTN_F1;
    case SDLK_F2:
      return KL_BTN_F2;
    case SDLK_F3:
      return KL_BTN_F3;
    case SDLK_F4:
      return KL_BTN_F4;
    case SDLK_F5:
      return KL_BTN_F5;
    case SDLK_F6:
      return KL_BTN_F6;
    case SDLK_F7:
      return KL_BTN_F7;
    case SDLK_F8:
      return KL_BTN_F8;
    case SDLK_F9:
      return KL_BTN_F9;
    case SDLK_F10:
      return KL_BTN_F10;
    case SDLK_F11:
      return KL_BTN_F11;
    case SDLK_F12:
      return KL_BTN_F12;
    case SDLK_0:
      return KL_BTN_0;
    case SDLK_1:
      return KL_BTN_1;
    case SDLK_2:
      return KL_BTN_2;
    case SDLK_3:
      return KL_BTN_3;
    case SDLK_4:
      return KL_BTN_4;
    case SDLK_5:
      return KL_BTN_5;
    case SDLK_6:
      return KL_BTN_6;
    case SDLK_7:
      return KL_BTN_7;
    case SDLK_8:
      return KL_BTN_8;
    case SDLK_9:
      return KL_BTN_9;
    case SDLK_a:
      return KL_BTN_A;
    case SDLK_b:
      return KL_BTN_B;
    case SDLK_c:
      return KL_BTN_C;
    case SDLK_d:
      return KL_BTN_D;
    case SDLK_e:
      return KL_BTN_E;
    case SDLK_f:
      return KL_BTN_F;
    case SDLK_g:
      return KL_BTN_G;
    case SDLK_h:
      return KL_BTN_H;
    case SDLK_i:
      return KL_BTN_I;
    case SDLK_j:
      return KL_BTN_J;
    case SDLK_k:
      return KL_BTN_K;
    case SDLK_l:
      return KL_BTN_L;
    case SDLK_m:
      return KL_BTN_M;
    case SDLK_n:
      return KL_BTN_N;
    case SDLK_o:
      return KL_BTN_O;
    case SDLK_p:
      return KL_BTN_P;
    case SDLK_q:
      return KL_BTN_Q;
    case SDLK_r:
      return KL_BTN_R;
    case SDLK_s:
      return KL_BTN_S;
    case SDLK_t:
      return KL_BTN_T;
    case SDLK_u:
      return KL_BTN_U;
    case SDLK_v:
      return KL_BTN_V;
    case SDLK_w:
      return KL_BTN_W;
    case SDLK_x:
      return KL_BTN_X;
    case SDLK_y:
      return KL_BTN_Y;
    case SDLK_z:
      return KL_BTN_Z;
  }
  return KL_BTN_NONE;
}

void kl_input_tick() {
}

int kl_input_poll(kl_evt_generic_t *evt) {
  SDL_Event sdlevt;
  if (SDL_PollEvent(&sdlevt)) {
    switch (sdlevt.type) {
      case SDL_KEYDOWN:
      case SDL_KEYUP:
        evt->event.type   = KL_EVT_BUTTON;
        evt->button.code  = tocode(sdlevt.key.keysym.sym);
        evt->button.state = (sdlevt.key.state == SDL_PRESSED);
        return 1;
      case SDL_MOUSEMOTION:
        evt->event.type   = KL_EVT_MOUSE;
        evt->mouse.x      = sdlevt.motion.x;
        evt->mouse.y      = sdlevt.motion.y;
        evt->mouse.dx     = sdlevt.motion.xrel;
        evt->mouse.dy     = sdlevt.motion.yrel;
        return 1;
    }
  }
  evt->event.type   = KL_EVT_NONE;
  return 0;
}
/* vim: set ts=2 sw=2 et */
