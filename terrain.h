#ifndef KL_TERRAIN_H
#define KL_TERRAIN_H

typedef struct kl_terrain {
  unsigned int buf_verts;
  unsigned int buf_norms;
  unsigned int buf_tris;
  unsigned int tris_n;
  unsigned int attribs;
} kl_terrain_t;

kl_terrain_t* kl_terrain_testsphere();

#endif /* KL_TERRAIN_H */
/* vim: set ts=2 sw=2 et */
