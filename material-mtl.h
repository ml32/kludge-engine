#ifndef KL_MTL_H
#define KL_MTL_H

#include "material.h"

void kl_material_setmtl(char *path);
int kl_material_loadfrommtl(char *path, kl_material_t *material);

#endif /* KL_MTL_H */
/* vim: set ts=2 sw=2 et */
