#include "camera.h"

static void update_view(kl_camera_t *cam);
static void update_proj(kl_camera_t *cam);

/* ------------------- */
void kl_camera_update(kl_camera_t *cam) {
  update_view(cam);
  update_proj(cam);
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

/* ---------------------- */
static void update_view(kl_camera_t *cam) {
  kl_mat4f_t rot, trans;
  kl_mat4f_rotation(&rot, &cam->orientation);
  kl_mat4f_translation(&trans, &cam->position);

  kl_mat4f_mul(&cam->mat_view, &rot, &trans);

  kl_mat4f_t irot, itrans;
  kl_mat4f_transpose(&irot, &rot);
  kl_vec3f_t ipos;
  kl_vec3f_negate(&ipos, &cam->position);
  kl_mat4f_translation(&itrans, &ipos);

  kl_mat4f_mul(&cam->mat_iview, &itrans, &irot);
}

static void update_proj(kl_camera_t *cam) {
  kl_mat4f_perspective(&cam->mat_proj, cam->aspect, cam->fov, cam->near, cam->far);
  kl_mat4f_invperspective(&cam->mat_iproj, cam->aspect, cam->fov, cam->near, cam->far);
}
/* vim: set ts=2 sw=2 et */
