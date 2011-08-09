#ifndef KL_RENDERER_H
#define KL_RENDERER_H

#include "matrix.h"

extern kl_mat4f_t kl_render_mat_view;
extern kl_mat4f_t kl_render_mat_proj;

#define KL_RENDER_FLOAT  0x00
#define KL_RENDER_UINT8  0x01
#define KL_RENDER_UINT16 0x02

#define KL_RENDER_RGB    0x10
#define KL_RENDER_RGBA   0x11
#define KL_RENDER_GRAY   0x12
#define KL_RENDER_GRAYA  0x13

typedef struct kl_render_attrib {
  unsigned int index;
  unsigned int size;
  unsigned int type;
  unsigned int buffer;
} kl_render_attrib_t;

int kl_render_init();
void kl_render_draw();
unsigned int kl_render_upload_vertdata(void *data, int n);
unsigned int kl_render_upload_tris(int *data, int n);
unsigned int kl_render_upload_texture(void *data, int w, int h, int format, int type);
void kl_render_free_texture(unsigned int texture);
unsigned int kl_render_define_attribs(int tris, kl_render_attrib_t *cfg, int n);

#endif /* KL_RENDERER_H */
/* vim: set ts=2 sw=2 et */
