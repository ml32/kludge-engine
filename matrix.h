#ifndef KL_MATRIX_H
#define KL_MATRIX_H

#include "vec.h"
#include "quat.h"

#include <stdint.h>

typedef union kl_mat4f {
  float      cell[16];
  kl_vec4f_t column[4];
} kl_mat4f_t;

#define KL_MAT4F_IDENTITY \
{\
  .cell = {\
    1.0f, 0.0f, 0.0f, 0.0f,\
    0.0f, 1.0f, 0.0f, 0.0f,\
    0.0f, 0.0f, 1.0f, 0.0f,\
    0.0f, 0.0f, 0.0f, 1.0f\
  }\
}

void kl_mat4f_mul(kl_mat4f_t *dst, kl_mat4f_t *s1, kl_mat4f_t *s2);
void kl_mat4f_ortho(kl_mat4f_t *dst, float l, float r, float b, float t, float n, float f);
void kl_mat4f_frustum(kl_mat4f_t *dst, float l, float r, float b, float t, float n, float f);
void kl_mat4f_perspective(kl_mat4f_t *dst, float ratio, float fov, float n, float f);
void kl_mat4f_invperspective(kl_mat4f_t *dst, float ratio, float fov, float n, float f);
void kl_mat4f_rotation(kl_mat4f_t *dst, kl_quat_t *src);
void kl_mat4f_transpose(kl_mat4f_t *dst, kl_mat4f_t *src);
void kl_mat4f_translation(kl_mat4f_t *dst, kl_vec3f_t *src);
void kl_mat4f_print(kl_mat4f_t *src);

#endif /* KL_MATRIX_H */
/* vim: set ts=2 sw=2 et */
