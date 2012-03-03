#include "input.h"

#include "platform-glfw.h"

#include <GL/glfw.h>

int kl_input_init() {
  kl_glfw_init();
  glfwDisable(GLFW_MOUSE_CURSOR);
  return 0;
}

static int glfwcode(int code) {
  switch (code) {
    case KL_BTN_ESC:
      return GLFW_KEY_ESC;
    case KL_BTN_ENTER:
      return GLFW_KEY_ENTER;
    case KL_BTN_LSHIFT:
      return GLFW_KEY_LSHIFT;
    case KL_BTN_RSHIFT:
      return GLFW_KEY_RSHIFT;
    case KL_BTN_LCTRL:
      return GLFW_KEY_LCTRL;
    case KL_BTN_RCTRL:
      return GLFW_KEY_RCTRL;
    case KL_BTN_LALT:
      return GLFW_KEY_LALT;
    case KL_BTN_RALT:
      return GLFW_KEY_RALT;
    case KL_BTN_F1:
      return GLFW_KEY_F1;
    case KL_BTN_F2:
      return GLFW_KEY_F2;
    case KL_BTN_F3:
      return GLFW_KEY_F3;
    case KL_BTN_F4:
      return GLFW_KEY_F4;
    case KL_BTN_F5:
      return GLFW_KEY_F5;
    case KL_BTN_F6:
      return GLFW_KEY_F6;
    case KL_BTN_F7:
      return GLFW_KEY_F7;
    case KL_BTN_F8:
      return GLFW_KEY_F8;
    case KL_BTN_F9:
      return GLFW_KEY_F9;
    case KL_BTN_F10:
      return GLFW_KEY_F10;
    case KL_BTN_0:
      return '0';
    case KL_BTN_1:
      return '1';
    case KL_BTN_2:
      return '2';
    case KL_BTN_3:
      return '3';
    case KL_BTN_4:
      return '4';
    case KL_BTN_5:
      return '5';
    case KL_BTN_6:
      return '6';
    case KL_BTN_7:
      return '7';
    case KL_BTN_8:
      return '8';
    case KL_BTN_9:
      return '9';
    case KL_BTN_A:
      return 'A';
    case KL_BTN_B:
      return 'B';
    case KL_BTN_C:
      return 'C';
    case KL_BTN_D:
      return 'D';
    case KL_BTN_E:
      return 'E';
    case KL_BTN_F:
      return 'F';
    case KL_BTN_G:
      return 'G';
    case KL_BTN_H:
      return 'H';
    case KL_BTN_I:
      return 'I';
    case KL_BTN_J:
      return 'J';
    case KL_BTN_K:
      return 'K';
    case KL_BTN_L:
      return 'L';
    case KL_BTN_M:
      return 'M';
    case KL_BTN_N:
      return 'N';
    case KL_BTN_O:
      return 'O';
    case KL_BTN_P:
      return 'P';
    case KL_BTN_Q:
      return 'Q';
    case KL_BTN_R:
      return 'R';
    case KL_BTN_S:
      return 'S';
    case KL_BTN_T:
      return 'T';
    case KL_BTN_U:
      return 'U';
    case KL_BTN_V:
      return 'V';
    case KL_BTN_W:
      return 'W';
    case KL_BTN_X:
      return 'X';
    case KL_BTN_Y:
      return 'Y';
    case KL_BTN_Z:
      return 'Z';
  }
  return -1;
}

static int  current_button = 0;
static bool mouse_polled   = false;

void kl_input_tick() {
  glfwPollEvents();
  current_button = 0;
  mouse_polled   = false;
}

bool kl_input_poll(kl_evt_generic_t *evt) {
  static bool keystates[KL_BTN_COUNT];
  while (current_button < KL_BTN_COUNT) {
    int i = current_button++;
    int c = glfwcode(i);
    if (c < 0) continue;
	int state = glfwGetKey(c);
    if (state == GLFW_PRESS && !keystates[i]) {
      evt->event.type    = KL_EVT_BUTTON;
      evt->button.code   = i;
      evt->button.isdown = true;
      keystates[i] = true;
      return true;
    }
	if (state == GLFW_RELEASE && keystates[i]) {
      evt->event.type    = KL_EVT_BUTTON;
      evt->button.code   = i;
      evt->button.isdown = false;
      keystates[i] = false;
      return true;
    }
  }
  
  if (!mouse_polled) {
    int w, h;
    glfwGetWindowSize(&w, &h);
    
    int x, y;
    glfwGetMousePos(&x, &y);
    glfwSetMousePos(w/2, h/2);
    
    evt->event.type   = KL_EVT_MOUSE;
    evt->mouse.x      = x;
    evt->mouse.y      = y;
    evt->mouse.dx     = x - w/2;
    evt->mouse.dy     = y - h/2;
    
    mouse_polled = true;
    return true;
  }
        
  evt->event.type   = KL_EVT_NONE;
  return false;
}
/* vim: set ts=2 sw=2 et */
