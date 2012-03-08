#ifndef KL_RENDERER_H
#define KL_RENDERER_H

#include <stdbool.h>

#include "matrix.h"
#include "model.h"
#include "camera.h"

#define KL_RENDER_CW     0x20
#define KL_RENDER_CCW    0x21

#define KL_RENDER_UINT8  0x01
#define KL_RENDER_UINT16 0x02
#define KL_RENDER_FLOAT  0x03

typedef struct kl_render_attrib {
  unsigned int index;
  unsigned int size;
  unsigned int type;
  unsigned int buffer;
} kl_render_attrib_t;


int kl_render_init();
void kl_render_draw(kl_camera_t *cam);
void kl_render_pointshadow_draw(kl_vec3f_t *center);
void kl_render_set_debug(int mode);
void kl_render_add_model(kl_model_t *model);
void kl_render_add_light(kl_vec3f_t *position, float r, float g, float b, float intensity);
void kl_render_set_envlight(kl_vec3f_t *direction, float amb_r, float amb_g, float amb_b, float amb_intensity, float diff_r, float diff_g, float diff_b, float diff_intensity);
unsigned int kl_render_upload_vertdata(void *data, int n);
unsigned int kl_render_upload_tris(unsigned int *data, int n);
unsigned int kl_render_upload_texture(void *data, int w, int h, int format, bool clamp, bool filter);
void kl_render_free_texture(unsigned int texture);
unsigned int kl_render_define_attribs(int tris, kl_render_attrib_t *cfg, int n);

#endif /* KL_RENDERER_H */
/* vim: set ts=2 sw=2 et */
