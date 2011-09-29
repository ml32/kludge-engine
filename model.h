#ifndef KL_MODEL_H
#define KL_MODEL_H

#include "sphere.h"
#include "material.h"

#define KL_MODEL_PROP    0x01
#define KL_MODEL_ACTOR   0x02
#define KL_MODEL_TERRAIN 0x03

typedef struct kl_mesh {
  kl_material_t *material;
  unsigned int tris_i, tris_n; /* starting index and count */
} kl_mesh_t;

typedef struct kl_model_bufs_prop {
  unsigned int position;
  unsigned int texcoord;
  unsigned int normal;
  unsigned int tangent;
} kl_model_bufs_prop_t;

typedef struct kl_model_bufs_actor {
  unsigned int position;
  unsigned int texcoord;
  unsigned int normal;
  unsigned int tangent;
  unsigned int blendidx;
  unsigned int blendwt;
} kl_model_bufs_actor_t;

typedef struct kl_model_bufs_terrain {
  unsigned int position;
  unsigned int normal;
  unsigned int texweight;
} kl_model_bufs_terrain_t;

typedef union kl_model_bufs {
  kl_model_bufs_prop_t    prop;
  kl_model_bufs_actor_t   actor;
  kl_model_bufs_terrain_t terrain;
}  kl_model_bufs_t;

typedef struct kl_model {
  int type;
  kl_sphere_t bounds;
  int winding;
  kl_model_bufs_t bufs; /* several vertex buffer objects */
  unsigned int tris;    /* element array buffer */
  unsigned int attribs; /* a vertex array object (or equivalent) */
  unsigned int mesh_n;
  kl_mesh_t    mesh[];
} kl_model_t;

kl_model_t *kl_model_load(char *path);

#endif /* KL_MODEL_H */
/* vim: set ts=2 sw=2 et */
