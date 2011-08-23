#include "vid.h"
#include "input.h"
#include "renderer.h"

#include "model.h"
#include "camera.h"

#include <stdio.h>
#include <math.h>

int main(int argc, char **argv) {
  kl_evt_generic_t evt;

  if (kl_vid_init() < 0) return -1;
  if (kl_input_init() < 0) return -1;
  if (kl_render_init() < 0) return -1;
  
  int w, h;
  kl_vid_size(&w, &h);

  kl_camera_t cam = {
    .position    = { .x = 0.0f, .y = 0.0f, .z = 0.0f },
    .orientation = { .r = 1.0f, .i = 0.0f, .j = 0.0f, .k = 0.0f },
    .aspect = (float)w/(float)h,
    .fov = 1.5708f, /* pi/2 rad or 90 deg */
    .near = 1.0f,
    .far  = 1000.0f
  };
  kl_model_t *model = kl_model_load("test_assets/test.iqm");
  kl_vec3f_t center = { .x = 0.0f, .y = 0.0f, .z = 0.0f };
  float      radius = 100.0f;
  kl_render_add_model(model, &center, radius);
  kl_render_light_t *light = malloc(sizeof(kl_render_light_t));
  *light = (kl_render_light_t) {
    .position = { .x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f }, 
    .r = 1.0f, .g = 1.0f, .b = 1.0f,
    .intensity = 100.0f
  };
  fprintf(stderr, "adding light: < %f, %f, %f > ( %f, %f, %f ) %f\n", light->position.x, light->position.y, light->position.z, light->r, light->g, light->b, light->intensity);
  kl_render_add_light(light);
  
  int move_f = 0;
  int move_b = 0;
  int move_l = 0;
  int move_r = 0;
  kl_vec3f_t mouseangle = { .x = 0.0f, .y = 0.0f, .z = 0.0f };
  for (;;) {
    kl_input_tick();
    int dx = 0; int dy = 0;
    while (kl_input_poll(&evt)) {
      switch (evt.event.type) {
        case KL_EVT_BUTTON:
          switch (evt.button.code) {
            case KL_BTN_ESC:
              if (evt.button.state) return 0;
              break;
            case KL_BTN_W:
              move_f = evt.button.state;
              break;
            case KL_BTN_A:
              move_l = evt.button.state;
              break;
            case KL_BTN_S:
              move_b = evt.button.state;
              break;
            case KL_BTN_D:
              move_r = evt.button.state;
              break;
          }
          break;
        case KL_EVT_MOUSE:
          dx += evt.mouse.dx;
          dy += evt.mouse.dy;
      }
    }
    mouseangle.y = -(float)dx/200.0f;
    mouseangle.x = -(float)dy/200.0f;
    kl_camera_local_rotate(&cam, &mouseangle);
    
    kl_vec3f_t offset = {
      .x = (move_l ? 0.2f : 0.0f) - (move_r ? 0.2f : 0.0f),
      .y = 0.0f,
      .z = (move_f ? 0.2f : 0.0f) - (move_b ? 0.2f : 0.0f)
    };
    kl_camera_local_move(&cam, &offset);

    kl_render_draw(&cam);
    kl_vid_swap();
  }
}
/* vim: set ts=2 sw=2 et */
