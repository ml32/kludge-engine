#ifndef KL_SPHERE_H
#define KL_SPHERE_H

#include "vec.h"

typedef struct kl_sphere {
  kl_vec3f_t center;
  float      radius;
} kl_sphere_t;

void kl_sphere_merge(kl_sphere_t *dst, kl_sphere_t *s1, kl_sphere_t *s2);
void kl_sphere_extend(kl_sphere_t *dst, kl_sphere_t *s1, kl_vec3f_t *s2);
void kl_sphere_bounds(kl_sphere_t *dst, kl_vec3f_t *verts, int n);
static inline int  kl_sphere_test(kl_sphere_t *bounds, kl_vec3f_t *v) {
  return kl_vec3f_dist(&bounds->center, v) <= bounds->radius;
}

#endif /* KL_SPHERE_H */

/* vim: set ts=2 sw=2 et */
