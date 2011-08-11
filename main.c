#include "vid.h"
#include "input.h"
#include "renderer.h"

#include "model.h"

#include <stdio.h>

int main(int argc, char **argv) {
  kl_evt_generic_t evt;

  if (kl_input_init() < 0) return -1;
  if (kl_vid_init() < 0) return -1;
  if (kl_render_init() < 0) return -1;

  kl_model_t *model = kl_model_load("test_assets/test.iqm");
  printf("Model pointer: %p\n", model);

  for (;;) {
    kl_render_draw(model);
    kl_render_composite();
    kl_vid_swap();
    if (kl_input_poll(&evt)) {
      switch (evt.event.type) {
        case KL_EVT_BUTTON:
	  if (evt.button.code == KL_BTN_ESC && evt.button.state) return 0;
	  break;
      }
    }
    
  }
}
/* vim: set ts=2 sw=2 et */
