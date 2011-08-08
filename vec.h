#ifndef KL_VEC_H
#define KL_VEC_H

typedef struct kl_vec3f {
  float x, y, z;
} kl_vec3f_t;

typedef struct kl_vec4f {
  float x, y, z, w;
} kl_vec4f_t;

static inline void kl_vec3f_dot(float *dst, kl_vec3f_t *s1, kl_vec3f_t *s2) {
  *dst = s1->x * s2->x + s1->y * s2->y + s1->z * s2->z;
}

static inline void kl_vec3f_cross(kl_vec3f_t *dst, kl_vec3f_t *s1, kl_vec3f_t *s2) {
  dst->x = s1->y * s2->z - s1->z * s2->y;
  dst->y = s1->z * s2->x - s1->x * s2->z;
  dst->z = s1->x * s2->y - s1->y * s2->x;
}

static inline void kl_vec3f_scale(kl_vec3f_t *dst, kl_vec3f_t *s1, float *s2) {
  float scale = *s2;
  dst->x = s1->x * scale;
  dst->y = s1->y * scale;
  dst->z = s1->z * scale;
}

#endif /* KL_VEC_H */
/* vim: set ts=2 sw=2 et */
