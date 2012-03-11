#include "vid.h"
#include "input.h"
#include "renderer.h"
#include "resource.h"

#include "terrain.h"
#include "model.h"
#include "camera.h"
#include "time.h"

#include <stdio.h>
#include <math.h>
#include <stdbool.h>

int main(int argc, char **argv) {
  kl_evt_generic_t evt;

  kl_resource_add_dir("./test_assets/", "");

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
	.viewport = { .x = 0, .y = 0, .z = w, .w = h },
    .near = 1.0f,
    .far  = 10000.0f
  };

  kl_terrain_testsphere();

  kl_model_t *model1 = kl_model_load("test_assets/test.iqm");
  kl_render_add_model(model1);
  kl_model_t *model2 = kl_model_load("test_assets/sponza.obj");
  kl_render_add_model(model2);

  kl_vec3f_t light1_pos = { .x = 500.0f, .y = 300.0f, .z = 0.0f };
  kl_render_add_light(&light1_pos, 1.0f, 0.8f, 0.5f, 25000.0f);
  kl_vec3f_t light2_pos = { .x = -500.0f, .y = 300.0f, .z = 0.0f };
  kl_render_add_light(&light2_pos, 1.0f, 0.8f, 0.5f, 25000.0f);
  kl_vec3f_t light3_pos = { .x = 0.0f, .y = 300.0f, .z = 0.0f };
  kl_render_add_light(&light3_pos, 1.0f, 0.8f, 0.5f, 25000.0f);
  kl_vec3f_t envlight_dir = { -2.0f, -2.0f, 1.0f };
  kl_render_set_envlight(&envlight_dir, 0.8f, 0.8f, 1.0f, 0.05f, 1.0f, 0.9f, 0.6f, 0.0f);
  
  kl_timer_t timer = KL_TIMER_INIT;
  
  bool move_f = 0;
  bool move_b = 0;
  bool move_l = 0;
  bool move_r = 0;
  kl_vec3f_t mouseangle = { .x = 0.0f, .y = 0.0f, .z = 0.0f };
  for (;;) {
    float dt = kl_timer_tick(&timer);
  
    kl_input_tick();
    int dx = 0; int dy = 0;
    while (kl_input_poll(&evt)) {
      switch (evt.event.type) {
        case KL_EVT_BUTTON:
          switch (evt.button.code) {
            case KL_BTN_ESC:
              if (evt.button.isdown) return 0;
              break;
            case KL_BTN_W:
              move_f = evt.button.isdown;
              break;
            case KL_BTN_A:
              move_l = evt.button.isdown;
              break;
            case KL_BTN_S:
              move_b = evt.button.isdown;
              break;
            case KL_BTN_D:
              move_r = evt.button.isdown;
              break;
            case KL_BTN_1:
              kl_render_set_debug(0);
              break;
            case KL_BTN_2:
              kl_render_set_debug(1);
              break;
            case KL_BTN_3:
              kl_render_set_debug(2);
              break;
            case KL_BTN_4:
              kl_render_set_debug(3);
              break;
            case KL_BTN_5:
              kl_render_set_debug(4);
              break;
            case KL_BTN_6:
              kl_render_set_debug(5);
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
      .x = (move_r ? 1.0f : 0.0f) - (move_l ? 1.0f : 0.0f),
      .y = 0.0f,
      .z = (move_b ? 1.0f : 0.0f) - (move_f ? 1.0f : 0.0f)
    };
    kl_vec3f_scale(&offset, &offset, 320.0f * dt);
    kl_camera_local_move(&cam, &offset);

    kl_render_draw(&cam);
    kl_vid_swap();
  }
}
/* vim: set ts=2 sw=2 et */
