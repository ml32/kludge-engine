#ifndef KL_CAMERA_H
#define KL_CAMERA_H

#include "matrix.h"

typedef struct kl_frustum {
  float w, h, n, f;
} kl_frustum_t;

typedef struct kl_camera {
  kl_vec3f_t   position;
  kl_quat_t    orientation;
  kl_frustum_t frustum;
} kl_camera_t;

void kl_camera_view(kl_camera_t *cam, kl_mat4f_t *view);
void kl_camera_proj(kl_camera_t *cam, kl_mat4f_t *proj);
void kl_camera_local_move(kl_camera_t *cam, kl_vec3f_t *offset);
void kl_camera_local_rotate(kl_camera_t *cam, kl_vec3f_t *ang);

#endif /* KL_CAMERA_H */
/* vim: set ts=2 sw=2 et */
