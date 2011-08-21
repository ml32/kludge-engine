#include "renderer.h"

#include "renderer-gl3.h"

#include "bvhtree.h"

#include <stdlib.h>

static int checkfrustum(kl_bvh_bounds_t *bounds);

static kl_bvh_node_t *bvh_models = NULL;

/* ------------------------- */
int kl_render_init() { /* these all seem very unecessary */
  return kl_gl3_init();
}

void kl_render_draw(kl_camera_t *cam) {
  kl_gl3_begin_pass_gbuffer(cam);
  kl_bvh_search(bvh_models, &checkfrustum, (kl_bvh_result_cb)&kl_gl3_draw_pass_gbuffer);
  kl_gl3_end_pass_gbuffer();
  kl_gl3_composite();
}

void kl_render_add_model(kl_model_t* model, kl_vec3f_t *center, float radius) {
  kl_bvh_bounds_t bounds = {
    .center = *center,
    .radius = radius
  };
  kl_bvh_insert(&bvh_models, &bounds, model);
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
static int checkfrustum(kl_bvh_bounds_t *bounds) {
  return 1;
}
/* vim: set ts=2 sw=2 et */
