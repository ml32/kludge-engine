#ifndef KL_RENDERER_H
#define KL_RENDERER_H

#include "matrix.h"

extern kl_mat4f_t kl_render_mat_view;
extern kl_mat4f_t kl_render_mat_proj;

#define KL_RENDER_FLOAT 0x00
#define KL_RENDER_UINT8 0x01

typedef struct kl_render_attrib {
  int index;
  int size;
  int type;
  int buffer;
} kl_render_attrib_t;

int kl_render_init();
void kl_render_draw();
int kl_render_upload_vertdata(void *data, int n);
int kl_render_upload_tris(int *data, int n);
int kl_render_define_attribs(int tris, kl_render_attrib_t *cfg, int n);

#endif /* KL_RENDERER_H */
/* vim: set ts=2 sw=2 et */
