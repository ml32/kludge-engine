#ifndef KL_CAMERA_H
#define KL_CAMERA_H

#include "matrix.h"
#include "plane.h"

typedef struct kl_scene {
  kl_mat4f_t viewmatrix;
  kl_mat4f_t projmatrix;
  kl_mat4f_t iprojmatrix;
  kl_mat4f_t vpmatrix;
  kl_mat3f_t viewrot;
  kl_vec3f_t viewpos;
  kl_vec3f_t ray_eye[4];
  kl_vec3f_t ray_world[4];
} kl_scene_t;

typedef struct kl_frustum {
  kl_plane_t near, far;
  kl_plane_t top, bottom, left, right;
} kl_frustum_t;

typedef struct kl_camera {
  kl_vec3f_t   position;
  kl_quat_t    orientation;
  float        aspect;
  float        fov; /* in radians! */
  float        near, far;
} kl_camera_t;

void kl_camera_update_scene(kl_camera_t *cam, kl_scene_t *scene);
void kl_camera_update_frustum(kl_camera_t *cam, kl_frustum_t *frustum);
void kl_camera_local_move(kl_camera_t *cam, kl_vec3f_t *offset);
void kl_camera_local_rotate(kl_camera_t *cam, kl_vec3f_t *ang);

#endif /* KL_CAMERA_H */
/* vim: set ts=2 sw=2 et */
