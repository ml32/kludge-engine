#ifndef KL_CAMERA_H
#define KL_CAMERA_H

#include "matrix.h"
#include "plane.h"

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
  kl_mat4f_t   mat_view;
  kl_mat4f_t   mat_proj;
  kl_mat4f_t   mat_iview;
  kl_mat4f_t   mat_iproj;
  kl_frustum_t frustum;
} kl_camera_t;

void kl_camera_update(kl_camera_t *cam); /* updates the view and projection matrices */
void kl_camera_local_move(kl_camera_t *cam, kl_vec3f_t *offset);
void kl_camera_local_rotate(kl_camera_t *cam, kl_vec3f_t *ang);

#endif /* KL_CAMERA_H */
/* vim: set ts=2 sw=2 et */
