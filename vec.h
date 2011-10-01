#ifndef KL_VEC_H
#define KL_VEC_H

#include <math.h>

/* all operations _should_ be able to accept the same source and destination */

typedef struct kl_vec3f {
  float x, y, z;
} kl_vec3f_t;

typedef struct kl_vec2f {
  float x, y;
} kl_vec2f_t;

typedef struct kl_vec4f {
  float x, y, z, w;
} kl_vec4f_t;

static inline void kl_vec3f_add(kl_vec3f_t *dst, kl_vec3f_t *s1, kl_vec3f_t *s2) {
  *dst = (kl_vec3f_t){
    .x = s1->x + s2->x,
    .y = s1->y + s2->y,
    .z = s1->z + s2->z
  };
}

static inline void kl_vec3f_sub(kl_vec3f_t *dst, kl_vec3f_t *s1, kl_vec3f_t *s2) {
  *dst = (kl_vec3f_t){
    .x = s1->x - s2->x,
    .y = s1->y - s2->y,
    .z = s1->z - s2->z
  };
}

static inline float kl_vec3f_dot(kl_vec3f_t *s1, kl_vec3f_t *s2) {
  return s1->x * s2->x + s1->y * s2->y + s1->z * s2->z;
}

static inline void kl_vec3f_cross(kl_vec3f_t *dst, kl_vec3f_t *s1, kl_vec3f_t *s2) {
  *dst = (kl_vec3f_t){
    .x = s1->y * s2->z - s1->z * s2->y,
    .y = s1->z * s2->x - s1->x * s2->z,
    .z = s1->x * s2->y - s1->y * s2->x
  };
}

static inline void kl_vec3f_scale(kl_vec3f_t *dst, kl_vec3f_t *s1, float scale) {
  *dst = (kl_vec3f_t){
    .x = s1->x * scale,
    .y = s1->y * scale,
    .z = s1->z * scale
  };
}

static inline float kl_vec3f_magnitude(kl_vec3f_t *src) {
  return sqrtf(src->x * src->x + src->y * src->y + src->z * src->z);
}

static inline float kl_vec3f_dist(kl_vec3f_t *s1, kl_vec3f_t *s2) {
  kl_vec3f_t diff;
  kl_vec3f_sub(&diff, s1, s2);
  return kl_vec3f_magnitude(&diff);
}

static inline void kl_vec3f_midpoint(kl_vec3f_t *dst, kl_vec3f_t *s1, kl_vec3f_t *s2) {
  *dst = (kl_vec3f_t){
    .x = (s1->x + s2->x) / 2.0f,
    .y = (s1->y + s2->y) / 2.0f,
    .z = (s1->z + s2->z) / 2.0f
  };
}

static inline void kl_vec3f_norm(kl_vec3f_t *dst, kl_vec3f_t *src) {
  float m = kl_vec3f_magnitude(src);

  if (m == 0.0f) {
    *dst = (kl_vec3f_t){ .x = 0.0f, .y = 0.0f, .z = 0.0f };
    return;
  }

  float s = 1.0f/m;
  kl_vec3f_scale(dst, src, s);
}

static inline void kl_vec3f_negate(kl_vec3f_t *dst, kl_vec3f_t *src) {
  *dst = (kl_vec3f_t){
    .x = -src->x,
    .y = -src->y,
    .z = -src->z
  };
}

static inline void kl_vec2f_mul(kl_vec2f_t *dst, kl_vec2f_t *s1, kl_vec2f_t *s2) {
  *dst = (kl_vec2f_t){
    .x = s1->x * s2->x,
    .y = s1->y * s2->y
  };
}

static inline void kl_vec2f_add(kl_vec2f_t *dst, kl_vec2f_t *s1, kl_vec2f_t *s2) {
  *dst = (kl_vec2f_t){
    .x = s1->x + s2->x,
    .y = s1->y + s2->y
  };
}

static inline void kl_vec2f_sub(kl_vec2f_t *dst, kl_vec2f_t *s1, kl_vec2f_t *s2) {
  *dst = (kl_vec2f_t){
    .x = s1->x - s2->x,
    .y = s1->y - s2->y
  };
}

#endif /* KL_VEC_H */
/* vim: set ts=2 sw=2 et */
