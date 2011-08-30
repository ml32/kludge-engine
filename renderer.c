#include "renderer.h"

#include "renderer-gl3.h"

#include "bvhtree.h"
#include "plane.h"
#include "sphere.h"

#include <stdlib.h>

static int checkfrustum(kl_sphere_t *bounds, kl_frustum_t *frustum);

static kl_bvh_node_t *bvh_models = NULL;
static kl_bvh_node_t *bvh_lights = NULL;


/* ------------------------- */
int kl_render_init() { /* these all seem very unecessary */
  return kl_gl3_init();
}

void kl_render_draw(kl_camera_t *cam) {
  kl_camera_update(cam);

  kl_mat4f_t mat_vp;
  kl_mat4f_mul(&mat_vp, &cam->mat_proj, &cam->mat_view);
  kl_mat4f_t mat_ivp;
  kl_mat4f_mul(&mat_ivp, &cam->mat_iview, &cam->mat_iproj);

  kl_gl3_begin_pass_gbuffer(&cam->mat_view, &mat_vp);
  kl_bvh_search(bvh_models, (kl_bvh_filter_cb)&checkfrustum, &cam->frustum, (kl_bvh_result_cb)&kl_gl3_draw_pass_gbuffer);
  kl_gl3_end_pass_gbuffer();
  kl_gl3_begin_pass_lighting(&cam->mat_view, &mat_vp, &mat_ivp);
  kl_bvh_search(bvh_lights, (kl_bvh_filter_cb)&checkfrustum, &cam->frustum, (kl_bvh_result_cb)&kl_gl3_draw_pass_lighting);
  kl_gl3_end_pass_lighting();
  kl_gl3_composite();
  kl_gl3_debugtex();
}

void kl_render_add_model(kl_model_t* model) {
  kl_bvh_insert(&bvh_models, &model->bounds, model);
}

void kl_render_add_light(kl_vec3f_t *position, float r, float g, float b, float intensity) {
  unsigned int *light = malloc(sizeof(unsigned int));
  *light = kl_gl3_upload_light(position, r, g, b, intensity);
  kl_sphere_t bounds = {
    .center = { .x = position->x, .y = position->y, .z = position->z },
    .radius = 16.0f * sqrtf(intensity) /* 16 * sqrt(intensity) is the distance at which light contribution is less than 1/256 */
  };
  kl_bvh_insert(&bvh_lights, &bounds, light);
}

unsigned int kl_render_upload_vertdata(void *data, int n) {
  return kl_gl3_upload_vertdata(data, n);
}

unsigned int kl_render_upload_tris(unsigned int *data, int n) {
  return kl_gl3_upload_tris(data, n);
}

unsigned int kl_render_upload_texture(void *data, int w, int h, int format, int type) {
  return kl_gl3_upload_texture(data, w, h, format, type);
}

void kl_render_free_texture(unsigned int texture) {
  kl_gl3_free_texture(texture);
}

unsigned int kl_render_define_attribs(int tris, kl_render_attrib_t *cfg, int n) {
  return kl_gl3_define_attribs(tris, cfg, n);
}

/* ------------------------- */
static int checkfrustum(kl_sphere_t *bounds, kl_frustum_t *frustum) {
  if (kl_plane_dist(&frustum->near, &bounds->center) > bounds->radius)
    return 0;
  if (kl_plane_dist(&frustum->far, &bounds->center) > bounds->radius)
    return 0;
  if (kl_plane_dist(&frustum->top, &bounds->center) > bounds->radius)
    return 0;
  if (kl_plane_dist(&frustum->bottom, &bounds->center) > bounds->radius)
    return 0;
  if (kl_plane_dist(&frustum->left, &bounds->center) > bounds->radius)
    return 0;
  if (kl_plane_dist(&frustum->right, &bounds->center) > bounds->radius)
    return 0;
  return 1;
}
/* vim: set ts=2 sw=2 et */
