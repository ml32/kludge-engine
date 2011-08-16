#ifndef KL_MODEL_H
#define KL_MODEL_H

#include "texture.h"

#define KL_BUFFER_POSITION 0x00
#define KL_BUFFER_TEXCOORD 0x01
#define KL_BUFFER_NORMAL   0x02
#define KL_BUFFER_TANGENT  0x03
#define KL_BUFFER_BLENDIDX 0x04
#define KL_BUFFER_BLENDWT  0x05

typedef struct kl_material {
  kl_texture_t *diffuse;
  kl_texture_t *normal;
  kl_texture_t *specular;
  kl_texture_t *emissive;
} kl_material_t;

typedef struct kl_mesh {
  kl_material_t material;
  unsigned int tris_i, tris_n; /* starting index and count */
} kl_mesh_t;

typedef struct kl_model {
  int          winding;
  unsigned int bufs[6]; /* several vertex buffer objects */
  unsigned int tris;    /* element array buffer */
  unsigned int attribs; /* a vertex array object (or equivalent) */
  unsigned int mesh_n;
  kl_mesh_t    mesh[];
} kl_model_t;

kl_model_t *kl_model_load(char *path);

#endif /* KL_MODEL_H */
/* vim: set ts=2 sw=2 et */
