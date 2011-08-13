#include "matrix.h"

#include "quat.h"

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

void kl_mat4f_rotation(kl_mat4f_t *dst, kl_quat_t *src) {
  float i  = src->i;
  float j  = src->j;
  float k  = src->k;
  float r  = src->r;
  float rr = r*r;
  float ii = i*i;
  float jj = j*j;
  float kk = k*k;
  float ri = 2.0f*r*i;
  float rj = 2.0f*r*j;
  float rk = 2.0f*r*k;
  float ij = 2.0f*i*j;
  float ik = 2.0f*i*k;
  float jk = 2.0f*j*k;
  *dst = (kl_mat4f_t){
    .cell = {
      rr + ii - jj - kk, ij - rk,           ik + rj,           0.0f,
      ij + rk,           rr - ii + jj - kk, jk - ri,           0.0f,
      ik - rj,           jk + ri,           rr - ii - jj + kk, 0.0f,
      0.0f,              0.0f,              0.0f,              1.0f
    }
  };
}

void kl_mat4f_trans(kl_mat4f_t *dst, kl_vec3f_t *src) {
  *dst = (kl_mat4f_t){
    .cell = {
      1.0f,   0.0f,   0.0f,   0.0f,
      0.0f,   1.0f,   0.0f,   0.0f,
      0.0f,   0.0f,   1.0f,   0.0f,
      src->x, src->y, src->z, 1.0f
    }
  };
}

void kl_mat4f_frustum(kl_mat4f_t *dst, float l, float r, float b, float t, float n, float f) {
  *dst = (kl_mat4f_t){
    .cell = {
      (2.0f*n)/(r-l), 0.0f,           0.0f,             0.0f,
      0.0f,           (2.0f*n)/(t-b), 0.0f,             0.0f,
      (r+l)/(r-l),    (t+b)/(t-b),   -(f+n)/(f-n),     -1.0f,
      0.0f,           0.0f,          -(2.0f*f*n)/(f-n), 0.0f
    }
  };
}

/* vim: set ts=2 sw=2 et */
