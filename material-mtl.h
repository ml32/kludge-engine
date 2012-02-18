#ifndef KL_MTL_H
#define KL_MTL_H

#include "material.h"
#include "array.h"

#include <stdbool.h>

kl_array_t *kl_material_mtl_incref(char *path);
void kl_material_mtl_decref(char *path);
bool kl_material_loadfrommtl(kl_array_t *entries, char *vpath, char *entvpath, kl_material_t *material);

#endif /* KL_MTL_H */
/* vim: set ts=2 sw=2 et */
