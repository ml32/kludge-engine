#include "camera.h"

static void update_view(kl_camera_t *cam);
static void update_proj(kl_camera_t *cam);
static void update_frustum(kl_camera_t *cam);
static void update_rays(kl_camera_t *cam);

/* ------------------- */
void kl_camera_update(kl_camera_t *cam) {
  update_view(cam);
  update_proj(cam);
  update_frustum(cam);
  update_rays(cam);
}

void kl_camera_local_move(kl_camera_t *cam, kl_vec3f_t *offset) {
  kl_vec3f_t offset_world;
  kl_quat_rotate(&offset_world, &cam->orientation, offset);
  kl_vec3f_add(&cam->position, &cam->position, &offset_world);
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
  kl_vec3f_t ipos;
  kl_vec3f_negate(&ipos, &cam->position);

  kl_mat4f_t rot, trans;
  kl_mat4f_rotation(&rot, &cam->orientation);
  kl_mat4f_translation(&trans, &ipos);

  kl_mat4f_mul(&cam->mat_view, &rot, &trans);
}

static void update_proj(kl_camera_t *cam) {
  kl_mat4f_perspective(&cam->mat_proj, cam->aspect, cam->fov, cam->near, cam->far);
  kl_mat4f_invperspective(&cam->mat_iproj, cam->aspect, cam->fov, cam->near, cam->far);
}

static void update_frustum(kl_camera_t *cam) {
  kl_vec3f_t forward_local  = { .x = 0.0f, .y = 0.0f, .z =  1.0f };
  kl_vec3f_t forward_world, backward_world;
  kl_quat_rotate(&forward_world, &cam->orientation, &forward_local);
  kl_vec3f_negate(&backward_world, &forward_world);

  kl_plane_t far = {
    .norm = forward_world,
    .dist = kl_vec3f_dot(&forward_world, &cam->position) + cam->far
  };

  kl_plane_t near = {
    .norm = backward_world,
    .dist = kl_vec3f_dot(&backward_world, &cam->position) - cam->near
  };

  float y  = sinf(cam->fov / 2.0f);
  float yz = sqrtf(1.0f - y*y);
  float x  = cam->aspect * y;
  float xz = sqrtf(1.0f - x*x);
  kl_vec3f_t top_local = {
    .x = 0.0f,
    .y = y,
    .z = -yz
  };
  kl_vec3f_t bottom_local = {
    .x = 0.0f,
    .y = -y,
    .z = -yz
  };
  kl_vec3f_t left_local = {
    .x = -x,
    .y = 0.0f,
    .z = -xz
  };
  kl_vec3f_t right_local = {
    .x = x,
    .y = 0.0f,
    .z = -xz
  };
  kl_vec3f_t top_world, bottom_world, left_world, right_world;
  kl_quat_rotate(&top_world, &cam->orientation, &top_local);
  kl_quat_rotate(&bottom_world, &cam->orientation, &bottom_local);
  kl_quat_rotate(&left_world, &cam->orientation, &left_local);
  kl_quat_rotate(&right_world, &cam->orientation, &right_local);
  
  kl_plane_t top = {
    .norm = top_world,
    .dist = kl_vec3f_dot(&top_world, &cam->position)
  };
  kl_plane_t bottom = {
    .norm = bottom_world,
    .dist = kl_vec3f_dot(&bottom_world, &cam->position)
  };
  kl_plane_t left = {
    .norm = left_world,
    .dist = kl_vec3f_dot(&left_world, &cam->position)
  };
  kl_plane_t right = {
    .norm = right_world,
    .dist = kl_vec3f_dot(&right_world, &cam->position)
  };

  cam->frustum = (kl_frustum_t){
    .near   = near,
    .far    = far,
    .top    = top,
    .bottom = bottom,
    .left   = left,
    .right  = right
  };
}

static void update_rays(kl_camera_t *cam) {
  float dy  = tanf(cam->fov / 2.0f);
  float dx  = cam->aspect * dy;
  kl_vec3f_t local[4] = {
    {
      .x = -dx,
      .y = -dy,
      .z = -1.0f
    },
    {
      .x =  dx,
      .y = -dy,
      .z = -1.0f
    },
    {
      .x =  dx,
      .y =  dy,
      .z = -1.0f
    },
    {
      .x = -dx,
      .y =  dy,
      .z = -1.0f
    }
  };
  for (int i=0; i < 4; i++) {
    kl_quat_rotate(&cam->quad_rays[i], &cam->orientation, &local[i]);
    //cam->quad_rays[i] = local[i];
  }
}
/* vim: set ts=2 sw=2 et */
