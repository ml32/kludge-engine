#ifndef KL_PLANE_H
#define KL_PLANE_H

#include "vec.h"

typedef struct kl_plane {
  kl_vec3f_t norm;
  float      dist;
} kl_plane_t;

static inline float kl_plane_dist(kl_plane_t *plane, kl_vec3f_t *point) {
  return kl_vec3f_dot(&plane->norm, point) - plane->dist;
}

#endif /* KL_PLANE_H */
/* vim: set ts=2 sw=2 et */
