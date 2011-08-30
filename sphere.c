#include "sphere.h"

#include "stdio.h"

void kl_sphere_merge(kl_sphere_t *dst, kl_sphere_t *s1, kl_sphere_t *s2) {
  kl_vec3f_t temp, dir, max1, max2, center;
  float radius;

  kl_vec3f_sub(&dir, &s2->center, &s1->center);
  kl_vec3f_norm(&dir, &dir);
  
  kl_vec3f_scale(&temp, &dir, s1->radius);
  kl_vec3f_sub(&max1, &s1->center, &temp);

  kl_vec3f_scale(&temp, &dir, s2->radius);
  kl_vec3f_add(&max2, &s2->center, &temp);

  kl_vec3f_add(&center, &max1, &max2);
  kl_vec3f_scale(&center, &center, 0.5f);

  kl_vec3f_sub(&temp, &max2, &max1);
  radius = kl_vec3f_magnitude(&temp) / 2.0f;

  *dst = (kl_sphere_t){
    .center = center,
    .radius = radius
  };
} 

void kl_sphere_extend(kl_sphere_t *dst, kl_sphere_t *s1, kl_vec3f_t *s2) {
  if (kl_sphere_test(s1, s2)) {
    *dst = *s1;
    return;
  }

  kl_vec3f_t dir;
  kl_vec3f_sub(&dir, &s1->center, s2);
  kl_vec3f_norm(&dir, &dir);

  kl_vec3f_t far;
  kl_vec3f_scale(&far, &dir, s1->radius);
  kl_vec3f_add(&far, &far, &s1->center);

  kl_vec3f_t center;
  kl_vec3f_add(&center, &far, s2);
  kl_vec3f_scale(&center, &center, 0.5f);

  kl_vec3f_t temp;
  kl_vec3f_sub(&temp, &far, s2);
  float radius = kl_vec3f_magnitude(&temp) / 2.0f;

  *dst = (kl_sphere_t){
    .center = center,
    .radius = radius
  };
}

void kl_sphere_bounds(kl_sphere_t *dst, kl_vec3f_t *verts, int n) {
  if (n < 1) return;

  kl_sphere_t bounds = {
    .center = verts[0],
    .radius = 0.0f
  };

  for (int i=0; i < n; i++) {
    kl_vec3f_t *v = verts + i;
    if (kl_sphere_test(&bounds, v)) continue;
    kl_sphere_extend(&bounds, &bounds, v);
  }

  *dst = bounds;
}
/* vim: set ts=2 sw=2 et */
