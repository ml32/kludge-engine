#ifndef KL_MATERIAL_H
#define KL_MATERIAL_H

#include "texture.h"

#define KL_MATERIAL_PATHLEN 0x100
typedef struct kl_material {
  char path[KL_MATERIAL_PATHLEN];
  kl_texture_t *diffuse;
  kl_texture_t *normal;
  kl_texture_t *specular;
  kl_texture_t *emissive;
} kl_material_t;

kl_material_t *kl_material_incref(char *path);
void kl_material_decref(kl_material_t *texture);

#endif /* KL_MATERIAL_H */
/* vim: set ts=2 sw=2 et */
