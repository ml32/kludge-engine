#ifndef KL_INPUT_H
#define KL_INPUT_H

#include <stdbool.h>

#define KL_BTN_NONE  0x00
#define KL_BTN_ESC   0x01
#define KL_BTN_ENTER 0x02
#define KL_BTN_LSHIFT 0x04
#define KL_BTN_RSHIFT 0x05
#define KL_BTN_LCTRL 0x06
#define KL_BTN_RCTRL 0x07
#define KL_BTN_LALT  0x08
#define KL_BTN_RALT  0x09
#define KL_BTN_F1    0x10
#define KL_BTN_F2    0x11
#define KL_BTN_F3    0x12
#define KL_BTN_F4    0x13
#define KL_BTN_F5    0x14
#define KL_BTN_F6    0x15
#define KL_BTN_F7    0x16
#define KL_BTN_F8    0x17
#define KL_BTN_F9    0x18
#define KL_BTN_F10   0x19
#define KL_BTN_F11   0x1A
#define KL_BTN_F12   0x1B
#define KL_BTN_0     0x20
#define KL_BTN_1     0x21
#define KL_BTN_2     0x22
#define KL_BTN_3     0x23
#define KL_BTN_4     0x24
#define KL_BTN_5     0x25
#define KL_BTN_6     0x26
#define KL_BTN_7     0x27
#define KL_BTN_8     0x28
#define KL_BTN_9     0x29
#define KL_BTN_A     0x30
#define KL_BTN_B     0x31
#define KL_BTN_C     0x32
#define KL_BTN_D     0x33
#define KL_BTN_E     0x34
#define KL_BTN_F     0x35
#define KL_BTN_G     0x36
#define KL_BTN_H     0x37
#define KL_BTN_I     0x38
#define KL_BTN_J     0x39
#define KL_BTN_K     0x3A
#define KL_BTN_L     0x3B
#define KL_BTN_M     0x3C
#define KL_BTN_N     0x3D
#define KL_BTN_O     0x3E
#define KL_BTN_P     0x3F
#define KL_BTN_Q     0x40
#define KL_BTN_R     0x41
#define KL_BTN_S     0x42
#define KL_BTN_T     0x43
#define KL_BTN_U     0x44
#define KL_BTN_V     0x45
#define KL_BTN_W     0x46
#define KL_BTN_X     0x47
#define KL_BTN_Y     0x48
#define KL_BTN_Z     0x49
#define KL_BTN_COUNT 0x50

#define KL_EVT_NONE   0x00
#define KL_EVT_BUTTON 0x01
#define KL_EVT_MOUSE  0x02

typedef struct kl_evt {
  int type;
} kl_evt_t;

typedef struct kl_evt_button {
  kl_evt_t evt;
  int  code;
  bool isdown;
} kl_evt_button_t;

typedef struct kl_evt_mouse {
  kl_evt_t evt;
  int x;
  int y;
  int dx;
  int dy;
} kl_evt_mouse_t;

typedef union kl_evt_generic {
  kl_evt_t        event;
  kl_evt_button_t button;
  kl_evt_mouse_t  mouse;
} kl_evt_generic_t;

int kl_input_init();
void kl_input_tick();
bool kl_input_poll(kl_evt_generic_t *evt);

#endif /* KL_INPUT_H */
/* vim: set ts=2 sw=2 et */
