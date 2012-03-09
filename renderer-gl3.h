#ifndef KL_RENDERER_GL3_H
#define KL_RENDERER_GL3_H

#include "renderer.h"

#include <stdbool.h>

#include "camera.h"
#include "model.h"
#include "array.h"

int kl_gl3_init();

void kl_gl3_pass_gbuffer(kl_array_t *models);

void kl_gl3_pass_envlight();
void kl_gl3_pass_pointlight(kl_array_t *models);

void kl_gl3_pass_pointshadow(kl_vec3f_t *center);
void kl_gl3_pass_pointshadow_face(int face, kl_array_t *models);

/* displays bounding volumes */
void kl_gl3_begin_pass_debug();
void kl_gl3_end_pass_debug();
void kl_gl3_draw_pass_debug(kl_mat4f_t *modelmatrix, float r, float g, float b);

void kl_gl3_composite(float dt); /* apply tonemapping, postprocessing, and output to screen */
void kl_gl3_debugtex(int mode); /* show output of various buffers */

unsigned int kl_gl3_upload_vertdata(void *data, int n);
void kl_gl3_update_vertdata(unsigned int vbo, void *data, int n);
unsigned int kl_gl3_upload_tris(unsigned int *data, int n);
unsigned int kl_gl3_upload_texture(void *data, int w, int h, int format, bool clamp, bool filter);
unsigned int kl_gl3_upload_light(kl_vec3f_t *position, float r, float g, float b, float intensity);
void kl_gl3_update_scene(kl_scene_t *scene);
void kl_gl3_update_envlight(kl_vec3f_t *direction, float amb_r, float amb_g, float amb_b, float amb_intensity, float diff_r, float diff_g, float diff_b, float diff_intensity);
void kl_gl3_free_texture(unsigned int texture);
unsigned int kl_gl3_define_attribs(int tris, kl_render_attrib_t *cfg, int n);

#endif /* KL_RENDERER_GL3_H */
