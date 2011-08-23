#ifndef KL_RENDERER_H
#define KL_RENDERER_H

#include "matrix.h"
#include "model.h"
#include "camera.h"

#define KL_RENDER_FLOAT  0x00
#define KL_RENDER_UINT8  0x01
#define KL_RENDER_UINT16 0x02

#define KL_RENDER_RGB    0x10
#define KL_RENDER_RGBA   0x11
#define KL_RENDER_GRAY   0x12
#define KL_RENDER_GRAYA  0x13

#define KL_RENDER_CW     0x20
#define KL_RENDER_CCW    0x21

typedef struct kl_render_attrib {
  unsigned int index;
  unsigned int size;
  unsigned int type;
  unsigned int buffer;
} kl_render_attrib_t;

typedef struct kl_render_light {
  kl_vec4f_t position;
  float r, g, b;
  float intensity;
} kl_render_light_t;

int kl_render_init();
void kl_render_draw(kl_camera_t *cam);
void kl_render_add_model(kl_model_t *model, kl_vec3f_t *center, float radius);
void kl_render_add_light(kl_render_light_t *light);
unsigned int kl_render_upload_vertdata(void *data, int n);
unsigned int kl_render_upload_tris(unsigned int *data, int n);
unsigned int kl_render_upload_texture(void *data, int w, int h, int format, int type);
void kl_render_free_texture(unsigned int texture);
unsigned int kl_render_define_attribs(int tris, kl_render_attrib_t *cfg, int n);

#endif /* KL_RENDERER_H */
/* vim: set ts=2 sw=2 et */
