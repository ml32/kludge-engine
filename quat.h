#ifndef KL_QUAT_H
#define KL_QUAT_H

#include "vec.h"

typedef struct kl_quat {
  float r, i, j, k;
} kl_quat_t;

void kl_quat_mul(kl_quat_t *dst, kl_quat_t *s1, kl_quat_t *s2);
float kl_quat_magnitude(kl_quat_t *src);
void kl_quat_norm(kl_quat_t *dst, kl_quat_t *src);
void kl_quat_fromvec(kl_quat_t *dst, kl_vec3f_t *src);
void kl_quat_conj(kl_quat_t *dst, kl_quat_t *src);
void kl_quat_rotate(kl_vec3f_t *dst, kl_quat_t *q, kl_vec3f_t *v);

#endif /* KL_QUAT_H */
/* vim: set ts=2 sw=2 et */
