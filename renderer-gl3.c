#include "renderer-gl3.h"

#include "renderer.h"

#include "model.h"
#include "vid.h"

#include <stdio.h>
#include <stdlib.h>
 #include <string.h>
#include <assert.h>

#define GL3_PROTOTYPES
#include <GL3/gl3.h>
#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

#include "renderer-gl3-meshdata.c"
#include "renderer-gl3-shaders.c"

static float anisotropy = 4.0f;
static int mipbias = 2;

static int convertenum(int value);
static int typesize(int value);
static int channels(int value);
static int create_shader(char *name, int type, const char *src, unsigned int *shader);
static int create_program(char *name, unsigned int vshader, unsigned int fshader, unsigned int *program);
static int init_gbuffer(int width, int height);
static int init_ssao(int width, int height);
static int init_blit();
static int init_minimal();
static int init_lighting(int width, int height);
static int init_tonemap();
static void blit_to_screen(unsigned int texture, float w, float h, float x, float y, float scale, float offset);
static void bind_ubo(unsigned int program, char *name, unsigned int binding);

static unsigned int rbo_depth;
static unsigned int tex_lighting;
static unsigned int fbo_lighting;
static unsigned int ubo_envlight;
static unsigned int ubo_scene;

static unsigned int gbuffer_fshader;
static unsigned int gbuffer_vshader;
static unsigned int gbuffer_program;
static int gbuffer_uniform_tdiffuse;
static int gbuffer_uniform_tnormal;
static int gbuffer_uniform_tspecular;
static int gbuffer_uniform_temissive;
static unsigned int gbuffer_tex_depth[6];
static unsigned int gbuffer_tex_normal[6];
static unsigned int gbuffer_tex_diffuse;
static unsigned int gbuffer_tex_specular;
static unsigned int gbuffer_tex_emissive;
static unsigned int gbuffer_fbo;

/* used to downsample the g-buffer */
static unsigned int downsample_vshader;
static unsigned int downsample_fshader;
static unsigned int downsample_program;
static int downsample_uniform_tdepth;
static int downsample_uniform_tnormal;
static unsigned int downsample_fbo[5];

static unsigned int ssao_vshader;
static unsigned int ssao_fshader;
static unsigned int ssao_program;
static int ssao_uniform_tdepth;
static int ssao_uniform_tnormal;
static unsigned int ssao_tex_occlusion[6];
static unsigned int ssao_fbo[6];

/* bilateral upsampling for MSSAO */
static unsigned int upsample_vshader;
static unsigned int upsample_fshader;
static unsigned int upsample_program;
static int upsample_uniform_tdepth;
static int upsample_uniform_tnormal;
static int upsample_uniform_tsdepth;
static int upsample_uniform_tsnormal;
static int upsample_uniform_tocclusion;

/* "minimal" shader program with no fragment output (used for stencil masks) */
static unsigned int minimal_vshader;
static unsigned int minimal_program;
static int minimal_uniform_mvpmatrix;

/* flat color shader */
static unsigned int flatcolor_fshader;
static unsigned int flatcolor_program;
static int flatcolor_uniform_mvpmatrix;
static int flatcolor_uniform_color;

static unsigned int pointlight_fshader;
static unsigned int pointlight_vshader;
static unsigned int pointlight_program;
static int pointlight_uniform_tdepth;
static int pointlight_uniform_tdiffuse;
static int pointlight_uniform_tnormal;
static int pointlight_uniform_tspecular;

static unsigned int envlight_fshader;
static unsigned int envlight_vshader;
static unsigned int envlight_program;
static int envlight_uniform_tdepth;
static int envlight_uniform_tdiffuse;
static int envlight_uniform_tnormal;
static int envlight_uniform_tspecular;
static int envlight_uniform_temissive;
static int envlight_uniform_tocclusion;

static unsigned int tonemap_fshader;
static unsigned int tonemap_vshader;
static unsigned int tonemap_program;
static int tonemap_uniform_tcomposite;

static unsigned int blit_fshader;
static unsigned int blit_vshader;
static unsigned int blit_program;
static int blit_uniform_image;
static int blit_uniform_size;
static int blit_uniform_offset;
static int blit_uniform_colorscale;
static int blit_uniform_coloroffset;

/* full-screen quad (NDC coordinates) */
static int vbo_quad_coords;
static int vbo_quad_tris;
static int vao_quad;
/* "perspective" quad (quad with perspective rays) */
static int vbo_pquad_rays_eye;   /* eye coords */
static int vbo_pquad_rays_world; /* world coords */
static int vao_pquad; 
/* low-resolution sphere for light stenciling */
static int vbo_sphere_coords;
static int vbo_sphere_tris;
static int vao_sphere;

typedef struct uniform_scene {
  kl_mat4f_t   viewmatrix;
  kl_mat4f_t   projmatrix;
  kl_mat4f_t   iprojmatrix;
  kl_mat4f_t   vpmatrix;
  kl_vec4f_t   viewpos;
  kl_mat3x4f_t viewrot;
} uniform_scene_t;

typedef struct uniform_light {
  kl_vec4f_t position;
  float r, g, b;
  float intensity;
} uniform_light_t;

typedef struct uniform_envlight {
  float amb_r, amb_g, amb_b;
  float amb_intensity;
  float diff_r, diff_g, diff_b;
  float diff_intensity;
  kl_vec4f_t direction;
} uniform_envlight_t;

#define LOGBUFFER_SIZE 0x4000
static char logbuffer[LOGBUFFER_SIZE];


/* ----------------------------------------------- */

int kl_gl3_init() {
  int w, h;
  kl_vid_size(&w, &h);

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

  /* create shared depth/stencil renderbuffer */
  glGenRenderbuffers(1, &rbo_depth);
  glBindRenderbuffer(GL_RENDERBUFFER, rbo_depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);

  /* create uniform buffer objects */
  glGenBuffers(1, &ubo_envlight);
  glGenBuffers(1, &ubo_scene);

  /* create shader programs */
  if (init_gbuffer(w, h) < 0) return -1;
  if (init_ssao(w, h) < 0) return -1;
  if (init_minimal() < 0) return -1;
  if (init_lighting(w, h) < 0) return -1;
  if (init_tonemap() < 0) return -1;
  if (init_blit() < 0) return -1;

  /* create fullscreen quad VBO/VAO */
  float verts_rect[8] = {
    -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f
  };
  unsigned int tris_rect[6] = { 0, 1, 2, 0, 2, 3 };
  vbo_quad_coords = kl_gl3_upload_vertdata(verts_rect, 8*sizeof(float));
  vbo_quad_tris   = kl_gl3_upload_tris(tris_rect, 6*sizeof(unsigned int));

  kl_render_attrib_t attrib_quad = {
    .index  = 0,
    .size   = 2,
    .type   = KL_RENDER_FLOAT,
    .buffer = vbo_quad_coords
  };
  vao_quad = kl_gl3_define_attribs(vbo_quad_tris, &attrib_quad, 1);

  vbo_pquad_rays_eye   = kl_gl3_upload_vertdata(NULL, 0);
  vbo_pquad_rays_world = kl_gl3_upload_vertdata(NULL, 0);
  kl_render_attrib_t attrib_pquad[3] = {
    {
      .index  = 0,
      .size   = 2,
      .type   = KL_RENDER_FLOAT,
      .buffer = vbo_quad_coords
    },
    {
      .index  = 1,
      .size   = 3,
      .type   = KL_RENDER_FLOAT,
      .buffer = vbo_pquad_rays_eye
    },
    {
      .index  = 2,
      .size   = 3,
      .type   = KL_RENDER_FLOAT,
      .buffer = vbo_pquad_rays_world
    }
  };
  vao_pquad = kl_gl3_define_attribs(vbo_quad_tris, attrib_pquad, 3);

  /* create sphere for point lighting */
  vbo_sphere_coords = kl_gl3_upload_vertdata(verts_sphere, SPHERE_NUMVERTS * 3 * sizeof(float));
  vbo_sphere_tris   = kl_gl3_upload_tris(tris_sphere, SPHERE_NUMTRIS * 3 * sizeof(unsigned int));

  kl_render_attrib_t attrib_sphere = {
    .index  = 0,
    .size   = 3,
    .type   = KL_RENDER_FLOAT,
    .buffer = vbo_sphere_coords
  };
  vao_sphere = kl_gl3_define_attribs(vbo_sphere_tris, &attrib_sphere, 1);

  return 0;
}

void kl_gl3_update_scene(kl_scene_t *scene) {
  kl_mat3f_t *viewrot = &scene->viewrot;
  uniform_scene_t data = {
    .viewmatrix  = scene->viewmatrix,
    .projmatrix  = scene->projmatrix,
    .iprojmatrix = scene->iprojmatrix,
    .vpmatrix    = scene->vpmatrix,
    .viewpos     = { .x = scene->viewpos.x, .y = scene->viewpos.y, .z = scene->viewpos.z, .w = 1.0f },
    .viewrot     = {
      .column[0] = { .x = viewrot->column[0].x, .y = viewrot->column[0].y, .z = viewrot->column[0].z, .w = 0.0f },
      .column[1] = { .x = viewrot->column[1].x, .y = viewrot->column[1].y, .z = viewrot->column[1].z, .w = 0.0f },
      .column[2] = { .x = viewrot->column[2].x, .y = viewrot->column[2].y, .z = viewrot->column[2].z, .w = 0.0f }
    }
  };

  glBindBuffer(GL_UNIFORM_BUFFER, ubo_scene);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(uniform_scene_t), &data, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);

  kl_gl3_update_vertdata(vbo_pquad_rays_eye,   scene->ray_eye,   4*sizeof(kl_vec3f_t));
  kl_gl3_update_vertdata(vbo_pquad_rays_world, scene->ray_world, 4*sizeof(kl_vec3f_t));
}

void kl_gl3_begin_pass_gbuffer() {
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gbuffer_fbo);
  unsigned int attachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4};
  glDrawBuffers(5, attachments);

  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  glUseProgram(gbuffer_program);
  
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_scene);

  glUniform1i(gbuffer_uniform_tdiffuse, 0);
  glUniform1i(gbuffer_uniform_tnormal, 1);
  glUniform1i(gbuffer_uniform_tspecular, 2);
  glUniform1i(gbuffer_uniform_temissive, 3);
}

void kl_gl3_end_pass_gbuffer() {
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);

  /* downsample the normal and depth buffers */
  glUseProgram(downsample_program);
  glBindVertexArray(vao_quad);

  for (int i=0; i < 5; i++) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, downsample_fbo[i]);
    unsigned int attachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, attachments);

    glUniform1i(downsample_uniform_tdepth, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_depth[i]);

    glUniform1i(downsample_uniform_tnormal, 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_normal[i]);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0); 
  }

  /* render MSSAO */
  glUseProgram(ssao_program);
  glBindVertexArray(vao_pquad);

  for (int i=0; i < 6; i++) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ssao_fbo[i]);
    unsigned int attachments[] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, attachments);

    glUniform1i(ssao_uniform_tdepth, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_depth[i]);

    glUniform1i(ssao_uniform_tnormal, 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_normal[i]);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0); 
  }

  glUseProgram(upsample_program);
  glBindVertexArray(vao_quad);

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);

  for (int i=4; i >= 0; i--) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ssao_fbo[i]);
    unsigned int attachments[] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, attachments);

    glUniform1i(upsample_uniform_tdepth, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_depth[i]);

    glUniform1i(upsample_uniform_tnormal, 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_normal[i]);

    glUniform1i(upsample_uniform_tsdepth, 2);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_depth[i+1]);

    glUniform1i(upsample_uniform_tsnormal, 3);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_normal[i+1]);

    glUniform1i(upsample_uniform_tocclusion, 4);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_RECTANGLE, ssao_tex_occlusion[i+1]);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0); 
  }

  glDisable(GL_BLEND);

  glBindVertexArray(0);
  glUseProgram(0);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void kl_gl3_draw_pass_gbuffer(kl_model_t *model) {
  glFrontFace(convertenum(model->winding));
  glBindVertexArray(model->attribs);
  for (int i=0; i < model->mesh_n; i++) {
    kl_mesh_t *mesh = model->mesh + i;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mesh->material->diffuse->id);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, mesh->material->normal->id);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, mesh->material->specular->id);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, mesh->material->emissive->id);

    glDrawElements(GL_TRIANGLES, 3*mesh->tris_n, GL_UNSIGNED_INT, (void*)(3*mesh->tris_i*sizeof(int)));
  }
  glFrontFace(GL_CCW);
}

void kl_gl3_begin_pass_lighting() {
  glBlendFunc(GL_ONE, GL_ONE); /* additive blending */
  glDepthMask(GL_FALSE); /* disable depth writes */
  glCullFace(GL_FRONT);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_lighting);
  unsigned int attachments[] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffers(1, attachments);

  glClear(GL_COLOR_BUFFER_BIT);

  /* do environmental lighting pass */
  glUseProgram(envlight_program);

  glEnable(GL_BLEND);

  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_scene);
  glBindBufferBase(GL_UNIFORM_BUFFER, 1, ubo_envlight);

  glUniform1i(envlight_uniform_tdepth, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_depth[0]);

  glUniform1i(envlight_uniform_tnormal, 1);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_normal[0]);

  glUniform1i(envlight_uniform_tdiffuse, 2);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_diffuse);

  glUniform1i(envlight_uniform_tspecular, 3);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_specular);

  glUniform1i(envlight_uniform_temissive, 4);
  glActiveTexture(GL_TEXTURE4);
  glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_emissive);

  glUniform1i(envlight_uniform_tocclusion, 5);
  glActiveTexture(GL_TEXTURE5);
  glBindTexture(GL_TEXTURE_RECTANGLE, ssao_tex_occlusion[0]);

  glBindVertexArray(vao_pquad);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

  glDisable(GL_BLEND);

  /* initialize draw pass uniforms */
  glUseProgram(pointlight_program);

  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_scene);

  glUniform1i(pointlight_uniform_tdepth, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_depth[0]);

  glUniform1i(pointlight_uniform_tnormal, 1);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_normal[0]);

  glUniform1i(pointlight_uniform_tdiffuse, 2);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_diffuse);

  glUniform1i(pointlight_uniform_tspecular, 3);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_specular);

  glEnable(GL_STENCIL_TEST);
}

void kl_gl3_end_pass_lighting() {
  glBindVertexArray(0);

  glUseProgram(0);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

  glCullFace(GL_BACK);
  glDepthMask(GL_TRUE);
  glDisable(GL_STENCIL_TEST);
}

void kl_gl3_draw_pass_lighting(kl_mat4f_t *mvpmatrix, unsigned int light) {
  /* stencil pass */
  glUseProgram(minimal_program);

  glClear(GL_STENCIL_BUFFER_BIT);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_GREATER);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glStencilFunc(GL_ALWAYS, 0, 0xFF);
  glStencilOp(GL_KEEP, GL_KEEP, GL_INVERT);

  glUniformMatrix4fv(minimal_uniform_mvpmatrix, 1, GL_FALSE, (float*)mvpmatrix);

  glBindVertexArray(vao_sphere);
  glDrawElements(GL_TRIANGLES, SPHERE_NUMTRIS * 3, GL_UNSIGNED_INT, 0);

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDisable(GL_DEPTH_TEST);

  /* draw pass */
  glUseProgram(pointlight_program);

  glEnable(GL_BLEND);
  glStencilMask(0x00);
  glStencilFunc(GL_NOTEQUAL, 0, 0xFF);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

  glBindBufferBase(GL_UNIFORM_BUFFER, 1, light);

  glBindVertexArray(vao_pquad);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

  glStencilMask(0xFF);
  glDisable(GL_BLEND);
}

void kl_gl3_begin_pass_debug() {
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_lighting);
  unsigned int attachments[] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffers(1, attachments);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

  glUseProgram(flatcolor_program);
  glBindVertexArray(vao_sphere);
}

void kl_gl3_end_pass_debug() {
  glBindVertexArray(0);

  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  glDisable(GL_DEPTH_TEST);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void kl_gl3_draw_pass_debug(kl_mat4f_t *mvpmatrix, float r, float g, float b) {
  glUniformMatrix4fv(flatcolor_uniform_mvpmatrix, 1, GL_FALSE, (float*)mvpmatrix);
  glUniform3f(flatcolor_uniform_color, r, g, b);

  glDrawElements(GL_TRIANGLES, SPHERE_NUMTRIS * 3, GL_UNSIGNED_INT, 0);
}

void kl_gl3_composite() {
  glEnable(GL_FRAMEBUFFER_SRGB);

  glUseProgram(tonemap_program);

  glUniform1i(tonemap_uniform_tcomposite, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_RECTANGLE, tex_lighting);

  glBindVertexArray(vao_quad);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0); 

  glBindVertexArray(0);

  glUseProgram(0);

  glDisable(GL_FRAMEBUFFER_SRGB);
}

void kl_gl3_debugtex(int mode) {
  switch (mode) {
    case 0:
      blit_to_screen(gbuffer_tex_depth[0],  0.18, 0.18, -0.78, -0.785, 0.001, 0.0);
      blit_to_screen(gbuffer_tex_normal[0], 0.18, 0.18, -0.39, -0.785, 1.0, 0.0);
      blit_to_screen(gbuffer_tex_diffuse,   0.18, 0.18,  0.0,  -0.785, 1.0, 0.0);
      blit_to_screen(gbuffer_tex_specular,  0.18, 0.18,  0.39, -0.785, 1.0, 0.0);
      blit_to_screen(gbuffer_tex_emissive,  0.18, 0.18,  0.78, -0.785, 1.0, 0.0);
      break;
    case 1:
      blit_to_screen(gbuffer_tex_depth[0], 1.0, 1.0, 0.0, 0.0, 0.001, 0.0);
      blit_to_screen(gbuffer_tex_depth[1], 0.18, 0.18, -0.78, -0.785, 0.001, 0.0);
      blit_to_screen(gbuffer_tex_depth[2], 0.18, 0.18, -0.39, -0.785, 0.001, 0.0);
      blit_to_screen(gbuffer_tex_depth[3], 0.18, 0.18,  0.0,  -0.785, 0.001, 0.0);
      blit_to_screen(gbuffer_tex_depth[4], 0.18, 0.18,  0.39, -0.785, 0.001, 0.0);
      blit_to_screen(gbuffer_tex_depth[5], 0.18, 0.18,  0.78, -0.785, 0.001, 0.0);
      break;
    case 2:
      blit_to_screen(gbuffer_tex_normal[0], 1.0, 1.0, 0.0, 0.0, 1.0, 0.0);
      blit_to_screen(gbuffer_tex_normal[1], 0.18, 0.18, -0.78, -0.785, 1.0, 0.0);
      blit_to_screen(gbuffer_tex_normal[2], 0.18, 0.18, -0.39, -0.785, 1.0, 0.0);
      blit_to_screen(gbuffer_tex_normal[3], 0.18, 0.18,  0.0,  -0.785, 1.0, 0.0);
      blit_to_screen(gbuffer_tex_normal[4], 0.18, 0.18,  0.39, -0.785, 1.0, 0.0);
      blit_to_screen(gbuffer_tex_normal[5], 0.18, 0.18,  0.78, -0.785, 1.0, 0.0);
      break;
    case 3:
      blit_to_screen(ssao_tex_occlusion[0], 1.0, 1.0, 0.0, 0.0, -1.0, 1.0);
      blit_to_screen(ssao_tex_occlusion[1], 0.18, 0.18, -0.78, -0.785, 1.0, 0.0);
      blit_to_screen(ssao_tex_occlusion[2], 0.18, 0.18, -0.39, -0.785, 1.0, 0.0);
      blit_to_screen(ssao_tex_occlusion[3], 0.18, 0.18,  0.0,  -0.785, 1.0, 0.0);
      blit_to_screen(ssao_tex_occlusion[4], 0.18, 0.18,  0.39, -0.785, 1.0, 0.0);
      blit_to_screen(ssao_tex_occlusion[5], 0.18, 0.18,  0.78, -0.785, 1.0, 0.0);
      break;
  }
}

unsigned int kl_gl3_upload_vertdata(void *data, int n) {
  unsigned int vbo;
  glGenBuffers(1, &vbo);
  if (data != NULL) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, n, data, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }
  return vbo;
}

void kl_gl3_update_vertdata(unsigned int vbo, void *data, int n) {
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, n, data, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

unsigned int kl_gl3_upload_tris(unsigned int *data, int n) {
  unsigned int ebo;
  glGenBuffers(1, &ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, n, data, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  return ebo;
}

unsigned int kl_gl3_upload_texture(void *data, int w, int h, int format, int type) {
  unsigned int texture;
  
  int gltype = convertenum(type);
  if (gltype == GL_FALSE) {
    fprintf(stderr, "Render: Bad pixel type! (%x)\n", type);
    return 0;
  }
  int glfmt = convertenum(format);
  if (glfmt == GL_FALSE) {
    fprintf(stderr, "Render: Bad texture format! (%x)\n", format);
    return 0;
  }
  int glifmt;
  switch (format) {
    case KL_RENDER_RGB:
      glifmt = GL_SRGB;
      break;
    case KL_RENDER_RGBA:
      glifmt = GL_SRGB_ALPHA;
      break;
    default:
      glifmt = glfmt;
      break;
  }

  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  assert(type == KL_RENDER_UINT8);
  int c = channels(format);
  int bytes = w * h * c * typesize(type);
  uint8_t *buf = malloc(bytes);
  memcpy(buf, data, bytes);
  for (int i=0; i < mipbias; i++) {
    w >>= 1;
    h >>= 1;
    if (w <= 0 || h <= 0) break;
    for (int y=0; y < h; y++) {
      for (int x=0; x < w; x++) {
        int x0 = (2 * x) * c;
        int x1 = (2 * x + 1) * c;
        int y0 = (2 * y) * (2 * w) * c;
        int y1 = (2 * y + 1) * (2 * w) * c;
        
        switch (c) {
          case 4:
            buf[y*w*c + x*c + 3] = ((int)buf[x0 + y0 + 3] + (int)buf[x0 + y1 + 3] + (int)buf[x1 + y0 + 3] + (int)buf[x1 + y1 + 3]) / 4;
          case 3:
            buf[y*w*c + x*c + 2] = ((int)buf[x0 + y0 + 2] + (int)buf[x0 + y1 + 2] + (int)buf[x1 + y0 + 2] + (int)buf[x1 + y1 + 2]) / 4;
          case 2:
            buf[y*w*c + x*c + 1] = ((int)buf[x0 + y0 + 1] + (int)buf[x0 + y1 + 1] + (int)buf[x1 + y0 + 1] + (int)buf[x1 + y1 + 1]) / 4;
          case 1:
            buf[y*w*c + x*c + 0] = ((int)buf[x0 + y0 + 0] + (int)buf[x0 + y1 + 0] + (int)buf[x1 + y0 + 0] + (int)buf[x1 + y1 + 0]) / 4;
        }
      }
    }
  }
  glTexImage2D(GL_TEXTURE_2D, 0, glifmt, w, h, 0, glfmt, gltype, buf);
  glGenerateMipmap(GL_TEXTURE_2D);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  /* greyscale swizzles */
  switch (format) {
    case KL_RENDER_GRAYA:
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_GREEN);
    case KL_RENDER_GRAY:
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
      break;
  }

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);

  glBindTexture(GL_TEXTURE_2D, 0);

  return texture;
}

unsigned int kl_gl3_upload_light(kl_vec3f_t *position, float r, float g, float b, float intensity) {
  unsigned int ubo;

  uniform_light_t light = {
    .position = { position->x, position->y, position->z, 1.0f },
    .r = r, .g = g, .b = b, .intensity = intensity
  };
  
  glGenBuffers(1, &ubo);

  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(uniform_light_t), &light, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);

  return ubo;
}

void kl_gl3_update_envlight(kl_vec3f_t *direction, float amb_r, float amb_g, float amb_b, float amb_intensity, float diff_r, float diff_g, float diff_b, float diff_intensity) {
  uniform_envlight_t light = {
    .direction = { direction->x, direction->y, direction->z, 1.0f },
    .amb_r = amb_r, .amb_g = amb_g, .amb_b = amb_b,
    .amb_intensity = amb_intensity,
    .diff_r = diff_r, .diff_g = diff_g, .diff_b = diff_b,
    .diff_intensity = diff_intensity
  };
  

  glBindBuffer(GL_UNIFORM_BUFFER, ubo_envlight);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(uniform_envlight_t), &light, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void kl_gl3_free_texture(unsigned int texture) {
  glDeleteTextures(1, &texture);
}

unsigned int kl_gl3_define_attribs(int tris, kl_render_attrib_t *cfg, int n) {
  unsigned int vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  for (int i=0; i < n; i++) {
    kl_render_attrib_t *c = cfg + i;
    if (c->buffer == 0) continue;
    glEnableVertexAttribArray(c->index);
    glBindBuffer(GL_ARRAY_BUFFER, c->buffer);
    glVertexAttribPointer(c->index, c->size, convertenum(c->type), GL_FALSE, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tris);
  glBindVertexArray(0);
  return vao;
}

/* --------------------------------------*/

static int convertenum(int value) {
  switch (value) {
    case KL_RENDER_FLOAT:
      return GL_FLOAT;
    case KL_RENDER_UINT8:
      return GL_UNSIGNED_BYTE;
    case KL_RENDER_UINT16:
      return GL_UNSIGNED_SHORT;
    case KL_RENDER_RGB:
      return GL_RGB;
    case KL_RENDER_RGBA:
      return GL_RGBA;
    case KL_RENDER_GRAY:
      return GL_RED;
    case KL_RENDER_GRAYA:
      return GL_RG;
    case KL_RENDER_CW:
      return GL_CW;
    case KL_RENDER_CCW:
      return GL_CCW;
  }
  return GL_FALSE;
}

static int typesize(int value) {
  switch (value) {
    case KL_RENDER_FLOAT:
      return 4;
    case KL_RENDER_UINT8:
      return 1;
    case KL_RENDER_UINT16:
      return 2;
  }
  return 0;
}

static int channels(int value){
  switch (value) {
    case KL_RENDER_RGB:
      return 3;
    case KL_RENDER_RGBA:
      return 4;
    case KL_RENDER_GRAY:
      return 1;
    case KL_RENDER_GRAYA:
      return 2;
  }
  return 0;
}

static int create_shader(char *name, int type, const char *src, unsigned int *shader) {
  int status;
  int n = strlen(src);

  unsigned int id = glCreateShader(type);
  *shader = id;
  glShaderSource(id, 1, &src, &n);
  glCompileShader(id);
  glGetShaderiv(id, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    glGetShaderInfoLog(id, LOGBUFFER_SIZE, NULL, logbuffer);
    fprintf(stderr, "Render: Failed to compile %s\n\tDetails: %s\n", name, logbuffer);
    return -1;
  }
  return 0;
}

static int create_program(char *name, unsigned int vshader, unsigned int fshader, unsigned int *program) {
  int status;

  int id = glCreateProgram();
  *program = id;
  glAttachShader(id, vshader);
  glAttachShader(id, fshader);
  glLinkProgram(id);
  glGetProgramiv(id, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    glGetProgramInfoLog(id, LOGBUFFER_SIZE, NULL, logbuffer);
    fprintf(stderr, "Render: Failed to link %s\n\tDetails: %s\n", name, logbuffer);
    return -1;
  }
  return 0;
}

static int init_gbuffer(int width, int height) {
  /* g-buffer format: */
  /* depth: 32f */
  /* normal: 16f x, 16f y (z reconstructed) */
  /* diffuse: 8 red, 8 blue, 8 green, 8 unused */
  /* specular: 8 red, 8 blue, 8 green, 8 specular exponent */
  /* emissive: 8 red, 8 blue, 8 green, 8 intensity exponent */
  
  glGenTextures(6, gbuffer_tex_depth);
  glGenTextures(6, gbuffer_tex_normal);
  glGenTextures(1, &gbuffer_tex_diffuse);
  glGenTextures(1, &gbuffer_tex_specular);
  glGenTextures(1, &gbuffer_tex_emissive);

  int w = width;
  int h = height;
  for (int i=0; i < 6; i++) {
    glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_depth[i]);
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_R32F, w, h, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_SWIZZLE_R, GL_RED);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_SWIZZLE_G, GL_RED);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_SWIZZLE_B, GL_RED);

    glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_normal[i]);
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RG16, w, h, 0, GL_RG, GL_UNSIGNED_SHORT, NULL);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    w >>= 1;
    h >>= 1;
  }

  glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_diffuse);
  glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_specular);
  glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindTexture(GL_TEXTURE_RECTANGLE, gbuffer_tex_emissive);
  glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindTexture(GL_TEXTURE_RECTANGLE, 0);

 
  glGenFramebuffers(1, &gbuffer_fbo);
  glGenFramebuffers(5, downsample_fbo);

  glBindFramebuffer(GL_FRAMEBUFFER, gbuffer_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, gbuffer_tex_depth[0], 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_RECTANGLE, gbuffer_tex_normal[0], 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_RECTANGLE, gbuffer_tex_diffuse, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_RECTANGLE, gbuffer_tex_specular, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_RECTANGLE, gbuffer_tex_emissive, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_depth);
  int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "Render: G-buffer is incomplete.\n\tDetails: %x\n", status);
    return -1;
  }

  for (int i=0; i<5; i++) {
    glBindFramebuffer(GL_FRAMEBUFFER, downsample_fbo[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, gbuffer_tex_depth[i+1], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_RECTANGLE, gbuffer_tex_normal[i+1], 0);
    int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      fprintf(stderr, "Render: Downsampling g-buffer is incomplete.\n\tDetails: %x\n", status);
      return -1;
    }
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  if (create_shader("g-buffer vertex shader", GL_VERTEX_SHADER, vshader_gbuffer_src, &gbuffer_vshader) < 0) return -1;
  if (create_shader("g-buffer fragment shader", GL_FRAGMENT_SHADER, fshader_gbuffer_src, &gbuffer_fshader) < 0) return -1;
  if (create_program("g-buffer shader program", gbuffer_vshader, gbuffer_fshader, &gbuffer_program) < 0) return -1;

  bind_ubo(envlight_program, "scene", 0);
  gbuffer_uniform_tdiffuse  = glGetUniformLocation(gbuffer_program, "tdiffuse");
  gbuffer_uniform_tnormal   = glGetUniformLocation(gbuffer_program, "tnormal");
  gbuffer_uniform_tspecular = glGetUniformLocation(gbuffer_program, "tspecular");
  gbuffer_uniform_temissive = glGetUniformLocation(gbuffer_program, "temissive");

  if (create_shader("downsampling vertex shader", GL_VERTEX_SHADER, vshader_downsample_src, &downsample_vshader) < 0) return -1;
  if (create_shader("downsampling fragment shader", GL_FRAGMENT_SHADER, fshader_downsample_src, &downsample_fshader) < 0) return -1;
  if (create_program("downsampling shader program", downsample_vshader, downsample_fshader, &downsample_program) < 0) return -1;

  downsample_uniform_tdepth  = glGetUniformLocation(downsample_program, "tdepth");
  downsample_uniform_tnormal = glGetUniformLocation(downsample_program, "tnormal");

  return 0;
}

static int init_ssao(int width, int height) {
  glGenTextures(6, ssao_tex_occlusion);

  int w = width;
  int h = height;
  for (int i=0; i < 6; i++) {
    glBindTexture(GL_TEXTURE_RECTANGLE, ssao_tex_occlusion[i]);
    //glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_R16F, w, h, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_SWIZZLE_R, GL_RED);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_SWIZZLE_G, GL_RED);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_SWIZZLE_B, GL_RED);

    w >>= 1;
    h >>= 1;
  }
  glBindTexture(GL_TEXTURE_RECTANGLE, 0);

  glGenFramebuffers(6, ssao_fbo);
  for (int i=0; i<6; i++) {
    glBindFramebuffer(GL_FRAMEBUFFER, ssao_fbo[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, ssao_tex_occlusion[i], 0);
    int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      fprintf(stderr, "Render: Occlusion buffer is incomplete.\n\tDetails: %x\n", status);
      return -1;
    }
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  if (create_shader("ssao vertex shader", GL_VERTEX_SHADER, vshader_ssao_src, &ssao_vshader) < 0) return -1;
  if (create_shader("ssao fragment shader", GL_FRAGMENT_SHADER, fshader_ssao_src, &ssao_fshader) < 0) return -1;
  if (create_program("ssao shader program", ssao_vshader, ssao_fshader, &ssao_program) < 0) return -1;

  ssao_uniform_tdepth  = glGetUniformLocation(ssao_program, "tdepth");
  ssao_uniform_tnormal = glGetUniformLocation(ssao_program, "tnormal");

  if (create_shader("upsampling vertex shader", GL_VERTEX_SHADER, vshader_upsample_src, &upsample_vshader) < 0) return -1;
  if (create_shader("upsampling fragment shader", GL_FRAGMENT_SHADER, fshader_upsample_src, &upsample_fshader) < 0) return -1;
  if (create_program("upsampling shader program", upsample_vshader, upsample_fshader, &upsample_program) < 0) return -1;

  upsample_uniform_tdepth     = glGetUniformLocation(upsample_program, "tdepth");
  upsample_uniform_tnormal    = glGetUniformLocation(upsample_program, "tnormal");
  upsample_uniform_tsdepth    = glGetUniformLocation(upsample_program, "tsdepth");
  upsample_uniform_tsnormal   = glGetUniformLocation(upsample_program, "tsnormal");
  upsample_uniform_tocclusion = glGetUniformLocation(upsample_program, "tocclusion");

  return 0;
}

static int init_blit() {
  /* screen blitting shader program */
  if (create_shader("screen vertex shader", GL_VERTEX_SHADER, vshader_blit_src, &blit_vshader) < 0) return -1;
  if (create_shader("screen fragment shader", GL_FRAGMENT_SHADER, fshader_blit_src, &blit_fshader) < 0) return -1;
  if (create_program("screen shader program", blit_vshader, blit_fshader, &blit_program) < 0) return -1;

  blit_uniform_image  = glGetUniformLocation(blit_program, "image");
  blit_uniform_size   = glGetUniformLocation(blit_program, "size");
  blit_uniform_offset = glGetUniformLocation(blit_program, "offset");
  blit_uniform_colorscale  = glGetUniformLocation(blit_program, "colorscale");
  blit_uniform_coloroffset = glGetUniformLocation(blit_program, "coloroffset");

  return 0;
}

static int init_minimal() {
  if (create_shader("minimal vertex shader", GL_VERTEX_SHADER, vshader_minimal_src, &minimal_vshader) < 0) return -1;
 
  minimal_program = glCreateProgram();
  glAttachShader(minimal_program, minimal_vshader);
  glLinkProgram(minimal_program);
  int status;
  glGetProgramiv(minimal_program, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    glGetProgramInfoLog(minimal_program, LOGBUFFER_SIZE, NULL, logbuffer);
    fprintf(stderr, "Render: Failed to link minimal shader program.\n\tDetails: %s\n", logbuffer);
    return -1;
  }

  minimal_uniform_mvpmatrix = glGetUniformLocation(minimal_program, "mvpmatrix");

  if (create_shader("flat color fragment shader", GL_FRAGMENT_SHADER, fshader_flatcolor_src, &flatcolor_fshader) < 0) return -1;
  if (create_program("flat color shader program", minimal_vshader, flatcolor_fshader, &flatcolor_program) < 0) return -1;

  flatcolor_uniform_mvpmatrix = glGetUniformLocation(flatcolor_program, "mvpmatrix");
  flatcolor_uniform_color     = glGetUniformLocation(flatcolor_program, "color");

  return 0;
}

static int init_lighting(int width, int height) {
  glGenTextures(1, &tex_lighting);
  glBindTexture(GL_TEXTURE_RECTANGLE, tex_lighting);
  glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_RECTANGLE, 0);

  glGenFramebuffers(1, &fbo_lighting);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_lighting);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, tex_lighting, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo_depth);
  int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "Render: HDR lighting framebuffer is incomplete.\n\tDetails: %x\n", status);
    return -1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  /* point lighting */
  if (create_shader("point lighting vertex shader", GL_VERTEX_SHADER, vshader_pointlight_src, &pointlight_vshader) < 0) return -1;
  if (create_shader("point lighting fragment shader", GL_FRAGMENT_SHADER, fshader_pointlight_src, &pointlight_fshader) < 0) return -1;
  if (create_program("point lighting shader program", pointlight_vshader, pointlight_fshader, &pointlight_program) < 0) return -1;

  bind_ubo(pointlight_program, "scene", 0);
  bind_ubo(pointlight_program, "pointlight", 1);
  pointlight_uniform_tdepth     = glGetUniformLocation(pointlight_program, "tdepth");
  pointlight_uniform_tdiffuse   = glGetUniformLocation(pointlight_program, "tdiffuse");
  pointlight_uniform_tnormal    = glGetUniformLocation(pointlight_program, "tnormal");
  pointlight_uniform_tspecular  = glGetUniformLocation(pointlight_program, "tspecular");

  /* environmental lighting */
  if (create_shader("environmental lighting vertex shader", GL_VERTEX_SHADER, vshader_envlight_src, &envlight_vshader) < 0) return -1;
  if (create_shader("environmental lighting fragment shader", GL_FRAGMENT_SHADER, fshader_envlight_src, &envlight_fshader) < 0) return -1;
  if (create_program("environmental lighting shader program", envlight_vshader, envlight_fshader, &envlight_program) < 0) return -1;

  bind_ubo(envlight_program, "scene", 0);
  bind_ubo(envlight_program, "envlight", 1);
  envlight_uniform_tdepth     = glGetUniformLocation(envlight_program, "tdepth");
  envlight_uniform_tdiffuse   = glGetUniformLocation(envlight_program, "tdiffuse");
  envlight_uniform_tnormal    = glGetUniformLocation(envlight_program, "tnormal");
  envlight_uniform_tspecular  = glGetUniformLocation(envlight_program, "tspecular");
  envlight_uniform_temissive  = glGetUniformLocation(envlight_program, "temissive");
  envlight_uniform_tocclusion = glGetUniformLocation(envlight_program, "tocclusion");

  return 0;
}

static int init_tonemap() {
  if (create_shader("tonemapping vertex shader", GL_VERTEX_SHADER, vshader_tonemap_src, &tonemap_vshader) < 0) return -1;
  if (create_shader("tonemapping fragment shader", GL_FRAGMENT_SHADER, fshader_tonemap_src, &tonemap_fshader) < 0) return -1;
  if (create_program("tonemapping shader program", tonemap_vshader, tonemap_fshader, &tonemap_program) < 0) return -1;

  tonemap_uniform_tcomposite = glGetUniformLocation(tonemap_program, "tcomposite");

  return 0;
}

static void blit_to_screen(unsigned int texture, float w, float h, float x, float y, float scale, float offset) {
  glUseProgram(blit_program);

  glUniform1i(blit_uniform_image, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_RECTANGLE, texture);

  glUniform2f(blit_uniform_size, w, h);
  glUniform2f(blit_uniform_offset, x, y);
  glUniform1f(blit_uniform_colorscale, scale);
  glUniform1f(blit_uniform_coloroffset, offset);

  glBindVertexArray(vao_quad);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0); 

  glBindVertexArray(0);

  glUseProgram(0);
}

static void bind_ubo(unsigned int program, char *name, unsigned int binding) {
  unsigned int index = glGetUniformBlockIndex(program, name);
  glUniformBlockBinding(program, index, binding);
}

/* vim: set ts=2 sw=2 et */
