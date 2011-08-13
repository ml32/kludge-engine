#include "camera.h"

void kl_camera_view(kl_camera_t *cam, kl_mat4f_t *view) {
  kl_mat4f_t rot, trans;
  kl_mat4f_rotation(&rot, &cam->orientation);
  kl_mat4f_trans(&trans, &cam->position);
  kl_mat4f_mul(view, &rot, &trans);
}

void kl_camera_proj(kl_camera_t *cam, kl_mat4f_t *proj) {
  float l = -cam->frustum.w/2.0f;
  float r =  cam->frustum.w/2.0f;
  float b = -cam->frustum.h/2.0f;
  float t =  cam->frustum.w/2.0f;
  float n =  cam->frustum.n;
  float f =  cam->frustum.f;
  kl_mat4f_frustum(proj, l, r, b, t, n, f);
}

void kl_camera_local_move(kl_camera_t *cam, kl_vec3f_t *offset) {
  kl_vec3f_t temp;
  kl_quat_rotate(&temp, &cam->orientation, offset);
  cam->position.x += temp.x;
  cam->position.y += temp.y;
  cam->position.z += temp.z;
}

void kl_camera_local_rotate(kl_camera_t *cam, kl_vec3f_t *ang) {
  kl_quat_t q;
  kl_quat_fromvec(&q, ang);

  kl_quat_t temp;
  kl_quat_mul(&temp, &cam->orientation, &q);
  kl_quat_norm(&cam->orientation, &temp);
}

/* vim: set ts=2 sw=2 et */
