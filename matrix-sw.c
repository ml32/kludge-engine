#include "matrix.h"

void kl_mat4f_mul(kl_mat4f_t *dst, kl_mat4f_t *s1, kl_mat4f_t *s2) {
  for (int c=0; c < 4; c++) {
    for (int r=0; r < 4; r++) {
      float x = 0;
      for (int i=0; i < 4; i++) {
        x += s1->cell[i*4 + r] * s2->cell[c*4 + i];
      }
      dst->cell[c*4 + r] = x;
    }
  }
}

void kl_mat4f_ortho(kl_mat4f_t *dst, float l, float r, float b, float t, float n, float f) {
  *dst = (kl_mat4f_t){
    .cell = {
      2.0f/(r-l),  0.0f,        0.0f,        0.0f,
      0.0f,        2.0f/(t-b),  0.0f,        0.0f,
      0.0f,        0.0f,        2.0f/(n-f),  0.0f,
      (l+r)/(l-r), (b+t)/(b-t), (n+f)/(n-f), 1.0f
    }
  };
}

/* vim: set ts=2 sw=2 et */
