#include "quat.h"

void kl_quat_mul(kl_quat_t *dst, kl_quat_t *s1, kl_quat_t *s2) {
  *dst = (kl_quat_t){
    .r = s1->r * s2->r - s1->i * s2->i - s1->j * s2->j - s1->k * s2->k,
    .i = s1->i * s2->r + s1->r * s2->i - s1->k * s2->j + s1->j * s2->k,
    .j = s1->j * s2->r + s1->k * s2->i + s1->r * s2->j - s1->i * s2->k,
    .k = s1->k * s2->r - s1->j * s2->i + s1->i * s2->j + s1->r * s2->k
  };
}

float kl_quat_magnitude(kl_quat_t *src) {
  return sqrtf(src->r * src->r + src->i * src->i + src->j * src->j + src->k * src->k);
}

void kl_quat_norm(kl_quat_t *dst, kl_quat_t *src) {
  float m = kl_quat_magnitude(src);
  *dst = (kl_quat_t){
    .r = src->r/m,
    .i = src->i/m,
    .j = src->j/m,
    .k = src->k/m
  };
}

void kl_quat_fromvec(kl_quat_t *dst, kl_vec3f_t *src) {
  float theta;
  kl_vec3f_t u; 

  theta = kl_vec3f_magnitude(src);
  kl_vec3f_norm(&u, src);

  float c = cosf(theta/2.0f);
  float s = sinf(theta/2.0f);

  *dst = (kl_quat_t){
    .r = c,
    .i = u.x * s,
    .j = u.y * s,
    .k = u.z * s
  };
}

void kl_quat_conj(kl_quat_t *dst, kl_quat_t *src) {
  *dst = (kl_quat_t){
    .r =  src->r,
    .i = -src->i,
    .j = -src->j,
    .k = -src->k
  };
}

void kl_quat_rotate(kl_vec3f_t *dst, kl_quat_t *q, kl_vec3f_t *v) {
  kl_quat_t q_, t1, t2;
  kl_quat_t v_ = { .r = 0.0f, .i = v->x, .j = v->y, .k = v->z };
  kl_quat_conj(&q_, q);
  kl_quat_mul(&t1, &v_, &q_);
  kl_quat_mul(&t2, q, &t1);
  *dst = (kl_vec3f_t){
    .x = t2.i,
    .y = t2.j,
    .z = t2.k
  };
}
/* vim: set ts=2 sw=2 et */
