#include "renderer-gl3.h"

#include "renderer.h"

#include "model.h"
#include "vid.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
#define GL3_PROTOTYPES
#include <GL3/gl3.h>
#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
*/
#include <GL/glew.h>

#include "renderer-gl3-meshdata.c"
#include "renderer-gl3-shaders.c"

static const float anisotropy = 4.0f;
static const int   mipbias = 0;
static const int   shadowsize = 512;
static const int   bouncemapsize = 12;
static const int   ssaolevels = 5;
#define MAXSSAOLEVELS 6

static int convertenum(int value);
static int create_shader(char *name, int type, const char *src, unsigned int *shader);
static int create_program(char *name, unsigned int vshader, unsigned int gshader, unsigned int fshader, unsigned int *program);
static unsigned int create_rendertexture(unsigned int target, bool filter, bool swizzle);
static void initialize_rendertexture(unsigned int texture, unsigned int target, unsigned int format, int width, int height);
static void initialize_rendertexturecube(unsigned int texture, unsigned int format, int size);
static unsigned int create_renderbuffer(unsigned int format, int width, int height);
static void set_texture(int index, unsigned int texture, unsigned int target);
static void draw_pquad();
static void draw_quad();
static int init_gbuffer(int width, int height);
static int init_ssao(int width, int height);
static int init_blit();
static int init_minimal();
static int init_lighting(int width, int height);
static int init_tonemap();
static void blit_to_screen(unsigned int texture, float w, float h, float x, float y, float scale, float offset);
static void blit_cube_to_screen(unsigned int texture, float w, float h, float x, float y, float scale, float offset);
static void bind_ubo(unsigned int program, char *name, unsigned int binding);

static unsigned int rbo_depth;
static unsigned int tex_shadow;
static unsigned int fbo_shadow;
static unsigned int ubo_envlight;
static unsigned int ubo_scene;
static unsigned int tex_noise;

static unsigned int lighting_fshader;
static unsigned int lighting_vshader;
static unsigned int lighting_program;
static int lighting_uniform_tdiffuse;
static int lighting_uniform_tspecular;
static int lighting_uniform_tambient;
static int lighting_uniform_talbedo;
static int lighting_uniform_tocclusion;
static int lighting_uniform_temissive;
static unsigned int lighting_tex_diffuse;
static unsigned int lighting_tex_specular;
static unsigned int lighting_tex_ambient;
static unsigned int lighting_fbo_intermediate;
static unsigned int lighting_tex_final;
static unsigned int lighting_fbo_final;
static unsigned int lighting_rbo_indirect; /* half-resolution */
static unsigned int lighting_tex_indirect;
static unsigned int lighting_fbo_indirect;

/* first-bounce VPL generation for omnidirecitonal source */
static unsigned int pointbounce_fshader;
static unsigned int pointbounce_vshader;
static unsigned int pointbounce_program;
static int pointbounce_uniform_tdiffuse;
static int pointbounce_uniform_tnormal;
static int pointbounce_uniform_cubeproj;
static int pointbounce_uniform_face;
static unsigned int pointbounce_tex_position;
static unsigned int pointbounce_tex_radiosity;
static unsigned int pointbounce_tex_normal;
static unsigned int pointbounce_rbo_depth;
static unsigned int pointbounce_fbo[6];
static bool *pointbounce_mask; /* used to correct for perspective distortion when choosing VPLs */

static unsigned int surfacelight_fshader;
static unsigned int surfacelight_vshader;
static unsigned int surfacelight_program;
static int surfacelight_uniform_tdepth;
static int surfacelight_uniform_tnormal;
static int surfacelight_uniform_position;
static int surfacelight_uniform_direction;
static int surfacelight_uniform_radiosity;

static unsigned int gbuffer_fshader;
static unsigned int gbuffer_vshader;
static unsigned int gbuffer_program;
static unsigned int gbufferback_fshader;
static unsigned int gbufferback_vshader;
static unsigned int gbufferback_program;
static int gbuffer_uniform_tdiffuse;
static int gbuffer_uniform_tnormal;
static int gbuffer_uniform_tspecular;
static int gbuffer_uniform_temissive;
static int gbufferback_uniform_tdiffuse;
static unsigned int gbuffer_tex_depth[MAXSSAOLEVELS];
static unsigned int gbuffer_tex_back[MAXSSAOLEVELS];
static unsigned int gbuffer_tex_normal[MAXSSAOLEVELS];
static unsigned int gbuffer_tex_albedo;
static unsigned int gbuffer_tex_specular;
static unsigned int gbuffer_tex_emissive;
static unsigned int gbuffer_fbo_front;
static unsigned int gbuffer_fbo_back;

/* used to downsample the g-buffer for ssao */
static unsigned int downsample_vshader;
static unsigned int downsample_fshader;
static unsigned int downsample_program;
static int downsample_uniform_tdepth;
static int downsample_uniform_tback;
static int downsample_uniform_tnormal;
static unsigned int downsample_fbo[MAXSSAOLEVELS-1];

static unsigned int ssao_vshader;
static unsigned int ssao_fshader;
static unsigned int ssao_program;
static int ssao_uniform_tdepth;
static int ssao_uniform_tnoise;
static int ssao_uniform_tback;
static unsigned int ssao_tex_occlusion[MAXSSAOLEVELS];
static unsigned int ssao_fbo[MAXSSAOLEVELS];

/* bilateral upsampling/blur */
static unsigned int bilateral_vshader;
static unsigned int bilateral_fshader;
static unsigned int bilateral_program;
static int bilateral_uniform_thdepth;
static int bilateral_uniform_tldepth;
static int bilateral_uniform_timage;

/* "minimal" shader program with no fragment output (used for stencil masks) */
static unsigned int minimal_vshader;
static unsigned int minimal_program;
static int minimal_uniform_modelmatrix;

/* flat color shader */
static unsigned int flatcolor_fshader;
static unsigned int flatcolor_program;
static int flatcolor_uniform_modelmatrix;
static int flatcolor_uniform_color;

static unsigned int pointlight_fshader;
static unsigned int pointlight_vshader;
static unsigned int pointlight_program;
static int pointlight_uniform_tdepth;
static int pointlight_uniform_tnormal;
static int pointlight_uniform_tspecular;
static int pointlight_uniform_tshadow;

static unsigned int cubedepth_fshader;
static unsigned int cubedepth_vshader;
static unsigned int cubedepth_program;
static int cubedepth_uniform_center;
static int cubedepth_uniform_cubeproj;
static int cubedepth_uniform_face;
static int cubedepth_uniform_tdiffuse;
static unsigned int cubedepth_tex_shadow;
static unsigned int cubedepth_rbo_depth;
static unsigned int cubedepth_fbo[6];

static unsigned int pointshadow_fshader;
static unsigned int pointshadow_vshader;
static unsigned int pointshadow_program;
static int pointshadow_uniform_tscreendepth;
static int pointshadow_uniform_tcubedepth;
static int pointshadow_uniform_center;

static unsigned int shadowfilter_fshader;
static unsigned int shadowfilter_vshader;
static unsigned int shadowfilter_program;
static int shadowfilter_uniform_tdepth;
static int shadowfilter_uniform_tnormal;
static int shadowfilter_uniform_tshadow;
static int shadowfilter_uniform_pass;
static unsigned int shadowfilter_tex_shadow;
static unsigned int shadowfilter_fbo;

static unsigned int envlight_fshader;
static unsigned int envlight_vshader;
static unsigned int envlight_program;
static int envlight_uniform_tdepth;
static int envlight_uniform_tnormal;
static int envlight_uniform_tspecular;

static unsigned int tonemap_fshader;
static unsigned int tonemap_vshader;
static unsigned int tonemap_program;
static int tonemap_uniform_tcomposite;
static int tonemap_uniform_sigma; /* half-brightness point */

static unsigned int blit_fshader;
static unsigned int blit_vshader;
static unsigned int blit_program;
static int blit_uniform_image;
static int blit_uniform_size;
static int blit_uniform_offset;
static int blit_uniform_colorscale;
static int blit_uniform_coloroffset;

static unsigned int blit_cube_fshader;
static unsigned int blit_cube_program;
static int blit_cube_uniform_image;
static int blit_cube_uniform_size;
static int blit_cube_uniform_offset;
static int blit_cube_uniform_colorscale;
static int blit_cube_uniform_coloroffset;

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
  kl_vec4f_t   ray_eye[4];
  kl_vec4f_t   ray_world[4];
  kl_vec4f_t   viewport;
  float        near, far;
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

typedef struct surfacelight {
  kl_vec3f_t position;
  float radius;
  kl_vec3f_t normal;
  float r, g, b;
} surfacelight_t;

#define LOGBUFFER_SIZE 0x4000
static char logbuffer[LOGBUFFER_SIZE];


/* ----------------------------------------------- */

int kl_gl3_init() {
  int w, h;
  kl_vid_size(&w, &h);

  glewExperimental = GL_TRUE; /* Dunno why this is necessary.  Renderbuffers shouldn't be "experimental"... :\ */
  GLenum err = glewInit();
  if (err != GLEW_OK) {
    fprintf(stderr, "Failed to load GLEW: %s\n", glewGetErrorString(err));
    return -1;
  }

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

  /* create shared depth/stencil renderbuffer */
  rbo_depth = create_renderbuffer(GL_DEPTH24_STENCIL8, w, h);

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
    },
    .ray_eye = { 
      { .x = scene->ray_eye[0].x, .y = scene->ray_eye[0].y, .z = scene->ray_eye[0].z, .w = 0.0f },
      { .x = scene->ray_eye[1].x, .y = scene->ray_eye[1].y, .z = scene->ray_eye[1].z, .w = 0.0f },
      { .x = scene->ray_eye[2].x, .y = scene->ray_eye[2].y, .z = scene->ray_eye[2].z, .w = 0.0f },
      { .x = scene->ray_eye[3].x, .y = scene->ray_eye[3].y, .z = scene->ray_eye[3].z, .w = 0.0f }
    },
    .ray_world = { 
      { .x = scene->ray_world[0].x, .y = scene->ray_world[0].y, .z = scene->ray_world[0].z, .w = 0.0f },
      { .x = scene->ray_world[1].x, .y = scene->ray_world[1].y, .z = scene->ray_world[1].z, .w = 0.0f },
      { .x = scene->ray_world[2].x, .y = scene->ray_world[2].y, .z = scene->ray_world[2].z, .w = 0.0f },
      { .x = scene->ray_world[3].x, .y = scene->ray_world[3].y, .z = scene->ray_world[3].z, .w = 0.0f }
    },
	  .viewport = scene->viewport,
	  .near     = scene->near,
	  .far      = scene->far
  };

  glBindBuffer(GL_UNIFORM_BUFFER, ubo_scene);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(uniform_scene_t), &data, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);

  kl_gl3_update_vertdata(vbo_pquad_rays_eye,   scene->ray_eye,   4*sizeof(kl_vec3f_t));
  kl_gl3_update_vertdata(vbo_pquad_rays_world, scene->ray_world, 4*sizeof(kl_vec3f_t));
}

void kl_gl3_pass_gbuffer(kl_array_t *models) {
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  /* back faces */
  glCullFace(GL_FRONT);
  
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gbuffer_fbo_back);
  unsigned int attachments_back[] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffers(1, attachments_back);

  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  glUseProgram(gbufferback_program);

  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_scene);
  
  glUniform1i(gbufferback_uniform_tdiffuse, 0);
  
  for (int i = 0; i < kl_array_size(models); i++) {
    kl_model_t *model;
    kl_array_get(models, i, &model);
    
    glFrontFace(convertenum(model->winding));
    glBindVertexArray(model->attribs);
    for (int i=0; i < model->mesh_n; i++) {
      kl_mesh_t *mesh = model->mesh + i;

      set_texture(0, mesh->material->diffuse->id, GL_TEXTURE_2D);
      
      glDrawElements(GL_TRIANGLES, 3*mesh->tris_n, GL_UNSIGNED_INT, (void*)(3*mesh->tris_i*sizeof(int)));
    }
    glFrontFace(GL_CCW);
  }

  /* front faces */
  glCullFace(GL_BACK);
  
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gbuffer_fbo_front);
  unsigned int attachments_front[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4};
  glDrawBuffers(5, attachments_front);

  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  glUseProgram(gbuffer_program);

  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_scene);

  glUniform1i(gbuffer_uniform_tdiffuse, 0);
  glUniform1i(gbuffer_uniform_tnormal, 1);
  glUniform1i(gbuffer_uniform_tspecular, 2);
  glUniform1i(gbuffer_uniform_temissive, 3);
  
  for (int i = 0; i < kl_array_size(models); i++) {
    kl_model_t *model;
    kl_array_get(models, i, &model);
    
    glFrontFace(convertenum(model->winding));
    glBindVertexArray(model->attribs);
    for (int i=0; i < model->mesh_n; i++) {
      kl_mesh_t *mesh = model->mesh + i;

      set_texture(0, mesh->material->diffuse->id,  GL_TEXTURE_2D);
      set_texture(1, mesh->material->normal->id,   GL_TEXTURE_2D);
      set_texture(2, mesh->material->specular->id, GL_TEXTURE_2D);
      set_texture(3, mesh->material->emissive->id, GL_TEXTURE_2D);

      glDrawElements(GL_TRIANGLES, 3*mesh->tris_n, GL_UNSIGNED_INT, (void*)(3*mesh->tris_i*sizeof(int)));
    }
    glFrontFace(GL_CCW);
  }
  
  /* half-resolution lighting depth pass */
  int w, h;
  kl_vid_size(&w, &h);
  glViewport(0, 0, w/2, h/2);
  
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, lighting_fbo_indirect);
  unsigned int attachments_indirect[] = {GL_NONE};
  glDrawBuffers(1, attachments_indirect);
  
  glClear(GL_DEPTH_BUFFER_BIT);
  
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  
  glUseProgram(gbufferback_program);

  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_scene);
  glUniform1i(gbufferback_uniform_tdiffuse, 0);
  
  kl_mat4f_t identity = KL_MAT4F_IDENTITY;
  glUniformMatrix4fv(minimal_uniform_modelmatrix, 1, GL_FALSE, (float*)&identity);
  
  for (int i = 0; i < kl_array_size(models); i++) {
    kl_model_t *model;
    kl_array_get(models, i, &model);
    
    glFrontFace(convertenum(model->winding));
    glBindVertexArray(model->attribs);
    for (int i=0; i < model->mesh_n; i++) {
      kl_mesh_t *mesh = model->mesh + i;

      set_texture(0, mesh->material->diffuse->id, GL_TEXTURE_2D);
      
      glDrawElements(GL_TRIANGLES, 3*mesh->tris_n, GL_UNSIGNED_INT, (void*)(3*mesh->tris_i*sizeof(int)));
    }
    glFrontFace(GL_CCW);
  }
  
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glViewport(0, 0, w, h);
  
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);

  /* downsample the depth buffers */
  glUseProgram(downsample_program);
  glUniform1i(downsample_uniform_tdepth, 0);
  glUniform1i(downsample_uniform_tback, 1);
  glUniform1i(downsample_uniform_tnormal, 2);

  for (int i=0; i < 5; i++) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, downsample_fbo[i]);
    unsigned int attachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, attachments);

    set_texture(0, gbuffer_tex_depth[i], GL_TEXTURE_RECTANGLE);
    set_texture(1, gbuffer_tex_back[i],  GL_TEXTURE_RECTANGLE);
    set_texture(2, gbuffer_tex_normal[i],  GL_TEXTURE_RECTANGLE);

    draw_quad();
  }

  /* render MSSAO */
  glUseProgram(ssao_program);
  
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_scene);
  glUniform1i(ssao_uniform_tdepth, 0);
  glUniform1i(ssao_uniform_tback, 1);
  glUniform1i(ssao_uniform_tnoise, 2);

  for (int i=0; i < 6; i++) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ssao_fbo[i]);
    unsigned int attachments[] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, attachments);

    set_texture(0, gbuffer_tex_depth[i], GL_TEXTURE_RECTANGLE);
    set_texture(1, gbuffer_tex_back[i],  GL_TEXTURE_RECTANGLE);
    set_texture(2, tex_noise, GL_TEXTURE_2D);

    draw_pquad();
  }

  glUseProgram(bilateral_program);
  
  glUniform1i(bilateral_uniform_thdepth, 0);
  glUniform1i(bilateral_uniform_tldepth, 1);
  glUniform1i(bilateral_uniform_timage, 2);

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);

  for (int i=ssaolevels-2; i >= 0; i--) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ssao_fbo[i]);
    unsigned int attachments[] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, attachments);

    set_texture(0, gbuffer_tex_depth[i], GL_TEXTURE_RECTANGLE);
    set_texture(1, gbuffer_tex_depth[i+1], GL_TEXTURE_RECTANGLE);
    set_texture(2, ssao_tex_occlusion[i+1], GL_TEXTURE_RECTANGLE);

    draw_quad();
  }

  glDisable(GL_BLEND);
  
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ssao_fbo[0]);
  unsigned int attachments[] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffers(1, attachments);
  
  set_texture(0, gbuffer_tex_depth[0], GL_TEXTURE_RECTANGLE);
  set_texture(1, gbuffer_tex_depth[0], GL_TEXTURE_RECTANGLE);
  set_texture(2, ssao_tex_occlusion[0], GL_TEXTURE_RECTANGLE);

  draw_quad();

  glUseProgram(0);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void kl_gl3_pass_pointshadow(kl_light_t *light) {
  //glEnable(GL_CULL_FACE);
  //glCullFace(GL_FRONT);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  
  glUseProgram(cubedepth_program);
  glUniform3f(cubedepth_uniform_center, light->position.x, light->position.y, light->position.z);
  glUseProgram(0);
  
  kl_array_t models;
  kl_array_init(&models, sizeof(kl_model_t*));
  for (int i=0; i < 6; i++) {
    kl_array_clear(&models);
    kl_render_query_models(&models);
    
    kl_gl3_pass_pointshadow_face(i, &models);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, light->id);
    kl_gl3_pass_pointbounce_face(i, &models);
  }
  kl_array_free(&models);
  
  glCullFace(GL_BACK);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_shadow);
  unsigned int attachments[] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffers(1, attachments);
  
  glUseProgram(pointshadow_program);

  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_scene);
  
  glUniform1i(pointshadow_uniform_tscreendepth, 0);
  glUniform1i(pointshadow_uniform_tcubedepth, 1);
  glUniform3f(pointshadow_uniform_center, light->position.x, light->position.y, light->position.z);
  
  set_texture(0, gbuffer_tex_depth[0], GL_TEXTURE_RECTANGLE);
  set_texture(1, cubedepth_tex_shadow, GL_TEXTURE_CUBE_MAP);
  
  draw_pquad();
  
  kl_gl3_pass_shadowfilter();
  
  /* load up VPL data */
  kl_vec4f_t *position  = malloc(bouncemapsize * bouncemapsize * 6 * sizeof(kl_vec4f_t));
  kl_vec4f_t *normal    = malloc(bouncemapsize * bouncemapsize * 6 * sizeof(kl_vec4f_t));
  kl_vec4f_t *radiosity = malloc(bouncemapsize * bouncemapsize * 6 * sizeof(kl_vec4f_t));
  glBindTexture(GL_TEXTURE_CUBE_MAP, pointbounce_tex_position);
  for (int i = 0; i < 6; i++) {
    int offset = i * bouncemapsize * bouncemapsize;
    glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, GL_FLOAT, position + offset);
  }
  glBindTexture(GL_TEXTURE_CUBE_MAP, pointbounce_tex_normal);
  for (int i = 0; i < 6; i++) {
    int offset = i * bouncemapsize * bouncemapsize;
    glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, GL_FLOAT, normal + offset);
  }
  glBindTexture(GL_TEXTURE_CUBE_MAP, pointbounce_tex_radiosity);
  for (int i = 0; i < 6; i++) {
    int offset = i * bouncemapsize * bouncemapsize;
    glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, GL_FLOAT, radiosity + offset);
  }
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  
  /* normalize */
  float attenuation = 0.0f;
  for (int i = 0; i < bouncemapsize * bouncemapsize * 6; i++) {
    attenuation += radiosity[i].w;
  }
  kl_array_t surflights;
  kl_array_init(&surflights, sizeof(surfacelight_t));
  for (int i = 0; i < bouncemapsize * bouncemapsize * 6; i++) {
    if (!pointbounce_mask[i % bouncemapsize * bouncemapsize]) continue;
    
    const float multiplier = 24.0f; /* compensates for lack of additonal bounces */
    radiosity[i].x *= multiplier / attenuation;
    radiosity[i].y *= multiplier / attenuation;
    radiosity[i].z *= multiplier / attenuation;
    
    float r = radiosity[i].x;
    float g = radiosity[i].y;
    float b = radiosity[i].z;
    if (r + g + b < 1.0f) continue; /* skip dim lights */
    float radius = 32.0f * sqrtf(r > g ? (r > b ? r : b) : (g > b ? g : b));
    
    surfacelight_t surflight = (surfacelight_t) {
      .position = {
        .x = position[i].x, position[i].y, position[i].z
      },
      .radius = radius,
      .normal = {
        .x = normal[i].x, .y = normal[i].y, .z = normal[i].z
      },
      .r = r, .g = g, .b = b
    };
    kl_array_push(&surflights, &surflight);
  }
  free(position);
  free(normal);
  free(radiosity);
  
  /* render */
  kl_gl3_pass_surfacelight(&surflights);
  kl_array_free(&surflights);
}

void kl_gl3_pass_surfacelight(kl_array_t *lights) {
  glUseProgram(minimal_program);

  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_scene);
  
  glUseProgram(surfacelight_program);

  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_scene);

  glUniform1i(surfacelight_uniform_tdepth, 0);
  glUniform1i(surfacelight_uniform_tnormal, 1);
  
  glUseProgram(0);
  
  glEnable(GL_STENCIL_TEST);
  
  int w, h;
  kl_vid_size(&w, &h);
  glViewport(0, 0, w/2, h/2);
  
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, lighting_fbo_indirect);
  unsigned int attachments_indirect[] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffers(1, attachments_indirect);
  
  glClear(GL_COLOR_BUFFER_BIT);
  
  for (int i = 0; i < kl_array_size(lights); i++) {
    surfacelight_t light;
    kl_array_get(lights, i, &light);
    
    kl_vec3f_t offset, center;
    kl_vec3f_scale(&offset, &light.normal, 0.9f * light.radius);
    kl_vec3f_add(&center, &light.position, &offset);
    
    kl_mat4f_t scale, translation, modelmatrix;
    kl_mat4f_translation(&translation, &center);
    kl_mat4f_scale(&scale, light.radius, light.radius, light.radius);
    kl_mat4f_mul(&modelmatrix, &translation, &scale);

    glClear(GL_STENCIL_BUFFER_BIT);

    /* stencil pass */
    glUseProgram(minimal_program);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_scene);
    
    glUniformMatrix4fv(minimal_uniform_modelmatrix, 1, GL_FALSE, (float*)&modelmatrix);

    glDepthMask(GL_FALSE); /* disable depth writes */
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    
    glCullFace(GL_FRONT);
    glDepthFunc(GL_GREATER);
    glStencilFunc(GL_ALWAYS, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

    glBindVertexArray(vao_sphere);
    glDrawElements(GL_TRIANGLES, SPHERE_NUMTRIS * 3, GL_UNSIGNED_INT, 0);
    
    glCullFace(GL_BACK);
    glDepthFunc(GL_GREATER);
    glStencilFunc(GL_ALWAYS, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);

    glBindVertexArray(vao_sphere);
    glDrawElements(GL_TRIANGLES, SPHERE_NUMTRIS * 3, GL_UNSIGNED_INT, 0);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    
    
    /* lighting pass */
    glUseProgram(surfacelight_program);
    
    glUniform3f(surfacelight_uniform_position, light.position.x, light.position.y, light.position.z);
    glUniform3f(surfacelight_uniform_direction, light.normal.x, light.normal.y, light.normal.z);
    glUniform3f(surfacelight_uniform_radiosity, light.r, light.g, light.b);
    
    set_texture(0, gbuffer_tex_depth[1],  GL_TEXTURE_RECTANGLE);
    set_texture(1, gbuffer_tex_normal[1], GL_TEXTURE_RECTANGLE);

    glStencilMask(0x00);
    glStencilFunc(GL_NOTEQUAL, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE); /* additive blending */
    draw_pquad();
    glDisable(GL_BLEND);
    
    glStencilMask(0xFF);
  }

  glDisable(GL_STENCIL_TEST);
  
  
  /* upsample to full-resolution */
  glUseProgram(bilateral_program);
  
  glUniform1i(bilateral_uniform_thdepth, 0);
  glUniform1i(bilateral_uniform_tldepth, 1);
  glUniform1i(bilateral_uniform_timage, 2);
  
  glViewport(0, 0, w, h);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, lighting_fbo_intermediate);
  unsigned int attachments_upsample[] = {GL_COLOR_ATTACHMENT2}; /* write to ambient buffer only */
  glDrawBuffers(1, attachments_upsample);
  
  set_texture(0, gbuffer_tex_depth[0], GL_TEXTURE_RECTANGLE);
  set_texture(1, gbuffer_tex_depth[1], GL_TEXTURE_RECTANGLE);
  set_texture(2, lighting_tex_indirect, GL_TEXTURE_RECTANGLE);

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  draw_quad();
  glDisable(GL_BLEND);

  glUseProgram(0);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void kl_gl3_pass_pointshadow_face(int face, kl_array_t *models) {
  glUseProgram(cubedepth_program);
  glUniform1i(cubedepth_uniform_face, face);
  glUniform1i(cubedepth_uniform_tdiffuse, 0);
  
  glViewport(0, 0, shadowsize, shadowsize);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, cubedepth_fbo[face]);
  unsigned int attachments[] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffers(1, attachments);
  
  float clearcolor[] = { 10000.0f, 10000.0f, 10000.0f, 10000.0f };
  glClearBufferfv(GL_COLOR, 0, clearcolor);
  glClear(GL_DEPTH_BUFFER_BIT);
  
  for (int i = 0; i < kl_array_size(models); i++) {
    kl_model_t *model;
    kl_array_get(models, i, &model);
    
    glFrontFace(convertenum(model->winding));
    glBindVertexArray(model->attribs);
    for (int i=0; i < model->mesh_n; i++) {
      kl_mesh_t *mesh = model->mesh + i;

      set_texture(0, mesh->material->diffuse->id, GL_TEXTURE_2D);
      
      glDrawElements(GL_TRIANGLES, 3*mesh->tris_n, GL_UNSIGNED_INT, (void*)(3*mesh->tris_i*sizeof(int)));
    }
    glFrontFace(GL_CCW);
  }
  
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  
  int w, h;
  kl_vid_size(&w, &h);
  glViewport(0, 0, w, h);
  
  glUseProgram(0);
}

void kl_gl3_pass_pointbounce_face(int face, kl_array_t *models) {
  glUseProgram(pointbounce_program);

  glUniform1i(pointbounce_uniform_face, face);
  glUniform1i(pointbounce_uniform_tdiffuse, 0);
  glUniform1i(pointbounce_uniform_tnormal, 1);
  
  glViewport(0, 0, bouncemapsize, bouncemapsize);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pointbounce_fbo[face]);
  unsigned int attachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
  glDrawBuffers(3, attachments);
  
  float clearcolor[] = { 10000.0f, 10000.0f, 10000.0f, 10000.0f };
  glClearBufferfv(GL_COLOR, 0, clearcolor);
  glClear(GL_DEPTH_BUFFER_BIT);
  
  for (int i = 0; i < kl_array_size(models); i++) {
    kl_model_t *model;
    kl_array_get(models, i, &model);
    
    glFrontFace(convertenum(model->winding));
    glBindVertexArray(model->attribs);
    for (int i=0; i < model->mesh_n; i++) {
      kl_mesh_t *mesh = model->mesh + i;

      set_texture(0, mesh->material->diffuse->id, GL_TEXTURE_2D);
      set_texture(1, mesh->material->normal->id, GL_TEXTURE_2D);
      
      glDrawElements(GL_TRIANGLES, 3*mesh->tris_n, GL_UNSIGNED_INT, (void*)(3*mesh->tris_i*sizeof(int)));
    }
    glFrontFace(GL_CCW);
  }
  
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  
  int w, h;
  kl_vid_size(&w, &h);
  glViewport(0, 0, w, h);
  
  glUseProgram(0);
}

void kl_gl3_pass_shadowfilter() {
  glDepthMask(GL_FALSE); /* disable depth writes */
  
  glBindVertexArray(vao_pquad);
  
  glUseProgram(shadowfilter_program);
  
  glUniform1i(shadowfilter_uniform_tdepth, 0);
  glUniform1i(shadowfilter_uniform_tnormal, 1);
  glUniform1i(shadowfilter_uniform_tshadow, 2);
  
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, shadowfilter_fbo);
  unsigned int attachments[] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffers(1, attachments);
  
  set_texture(0, gbuffer_tex_depth[0], GL_TEXTURE_RECTANGLE);
  set_texture(1, gbuffer_tex_normal[0], GL_TEXTURE_RECTANGLE);
  set_texture(2, tex_shadow, GL_TEXTURE_RECTANGLE);
  
  glUniform1i(shadowfilter_uniform_pass, 0);
  
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_shadow);
  glDrawBuffers(1, attachments);

  set_texture(2, tex_shadow, GL_TEXTURE_RECTANGLE);
  
  glUniform1i(shadowfilter_uniform_pass, 1);
  
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  
  glUseProgram(0);
  
  glDepthMask(GL_TRUE);
}

void kl_gl3_pass_envlight() {
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE); /* additive blending */

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, lighting_fbo_intermediate);
  unsigned int attachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
  glDrawBuffers(3, attachments);

  float clearcolor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
  glClearBufferfv(GL_COLOR, 0, clearcolor);
  glClearBufferfv(GL_COLOR, 1, clearcolor);
  glClearBufferfv(GL_COLOR, 2, clearcolor);

  /* do environmental lighting pass */
  glUseProgram(envlight_program);

  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_scene);
  glBindBufferBase(GL_UNIFORM_BUFFER, 1, ubo_envlight);

  glUniform1i(envlight_uniform_tdepth,     0);
  glUniform1i(envlight_uniform_tnormal,    1);
  glUniform1i(envlight_uniform_tspecular,  2);
  
  set_texture(0, gbuffer_tex_depth[0], GL_TEXTURE_RECTANGLE);
  set_texture(1, gbuffer_tex_normal[0],   GL_TEXTURE_RECTANGLE);
  set_texture(2, gbuffer_tex_specular, GL_TEXTURE_RECTANGLE);

  draw_pquad();
  
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

  glDisable(GL_BLEND);
}

void kl_gl3_pass_pointlight(kl_array_t *lights) {
  /* initialize draw pass uniforms */
  glUseProgram(minimal_program);

  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_scene);
  
  glUseProgram(pointlight_program);

  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_scene);
  
  glUniform1i(pointlight_uniform_tdepth, 0);
  glUniform1i(pointlight_uniform_tnormal, 1);
  glUniform1i(pointlight_uniform_tspecular, 2);
  glUniform1i(pointlight_uniform_tshadow, 3);
  
  glUseProgram(0);
  
  for (int i = 0; i < kl_array_size(lights); i++) {
    kl_light_t *light;
    kl_array_get(lights, i, &light);
  
    kl_mat4f_t scale, translation, modelmatrix;
    kl_mat4f_translation(&translation, &light->position);
    kl_mat4f_scale(&scale, light->scale, light->scale, light->scale);
    kl_mat4f_mul(&modelmatrix, &translation, &scale);
  
    /* draw shadows -- this clobbers the GL state */
    kl_gl3_pass_pointshadow(light);

    /* setup lighting pass */
    glEnable(GL_STENCIL_TEST);
    
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, lighting_fbo_intermediate);
    unsigned int attachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, attachments);

    /* stencil pass */
    glUseProgram(minimal_program);
    
    glUniformMatrix4fv(minimal_uniform_modelmatrix, 1, GL_FALSE, (float*)&modelmatrix);

    glClear(GL_STENCIL_BUFFER_BIT);

    glDepthMask(GL_FALSE); /* disable depth writes */
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    
    glCullFace(GL_FRONT);
    glDepthFunc(GL_GREATER);
    glStencilFunc(GL_ALWAYS, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

    glBindVertexArray(vao_sphere);
    glDrawElements(GL_TRIANGLES, SPHERE_NUMTRIS * 3, GL_UNSIGNED_INT, 0);
  
    glCullFace(GL_BACK);
    glDepthFunc(GL_GREATER);
    glStencilFunc(GL_ALWAYS, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);

    glBindVertexArray(vao_sphere);
    glDrawElements(GL_TRIANGLES, SPHERE_NUMTRIS * 3, GL_UNSIGNED_INT, 0);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    /* draw pass */
    glUseProgram(pointlight_program);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE); /* additive blending */
    glStencilMask(0x00);
    glStencilFunc(GL_NOTEQUAL, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    glBindBufferBase(GL_UNIFORM_BUFFER, 1, light->id);
    
    set_texture(0, gbuffer_tex_depth[0], GL_TEXTURE_RECTANGLE);
    set_texture(1, gbuffer_tex_normal[0], GL_TEXTURE_RECTANGLE);
    set_texture(2, gbuffer_tex_specular, GL_TEXTURE_RECTANGLE);
    set_texture(3, tex_shadow,           GL_TEXTURE_RECTANGLE);

    draw_pquad();

    glStencilMask(0xFF);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
  }
  
  glBindVertexArray(0);

  glUseProgram(0);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void kl_gl3_begin_pass_debug() {
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

void kl_gl3_draw_pass_debug(kl_mat4f_t *modelmatrix, float r, float g, float b) {
  glUniformMatrix4fv(flatcolor_uniform_modelmatrix, 1, GL_FALSE, (float*)modelmatrix);
  glUniform3f(flatcolor_uniform_color, r, g, b);

  glDrawElements(GL_TRIANGLES, SPHERE_NUMTRIS * 3, GL_UNSIGNED_INT, 0);
}

void kl_gl3_composite(float dt) {
  glUseProgram(lighting_program);

  glUniform1i(lighting_uniform_tdiffuse, 0);
  glUniform1i(lighting_uniform_tspecular, 1);
  glUniform1i(lighting_uniform_tambient, 2);
  glUniform1i(lighting_uniform_talbedo, 3);
  glUniform1i(lighting_uniform_tocclusion, 4);
  glUniform1i(lighting_uniform_temissive, 5);
  
  set_texture(0, lighting_tex_diffuse, GL_TEXTURE_RECTANGLE);
  set_texture(1, lighting_tex_specular, GL_TEXTURE_RECTANGLE);
  set_texture(2, lighting_tex_ambient, GL_TEXTURE_RECTANGLE);
  set_texture(3, gbuffer_tex_albedo, GL_TEXTURE_RECTANGLE);
  set_texture(4, ssao_tex_occlusion[0], GL_TEXTURE_RECTANGLE);
  set_texture(5, gbuffer_tex_emissive, GL_TEXTURE_RECTANGLE);
  
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, lighting_fbo_final);
  unsigned int attachments[] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffers(1, attachments);
  
  draw_quad();
  
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);


  static float luminance = 1.0f;

  /* compute geometric mean of luminance */
  const int level = 4;
  int w, h;
  glBindTexture(GL_TEXTURE_2D, lighting_tex_final);
  glGenerateMipmap(GL_TEXTURE_2D);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_WIDTH,  &w);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_HEIGHT, &h);
  float *samples = (float*)malloc(w*h*3*sizeof(float));
  glGetTexImage(GL_TEXTURE_2D, level, GL_RGB, GL_FLOAT, samples);
  glBindTexture(GL_TEXTURE_2D, 0);
  
  float num = 0.0f;
  float div = 0.0f;
  for (int i = 0; i < w * h; i++) {
    float r = samples[i*3+0];
    float g = samples[i*3+1];
    float b = samples[i*3+2];
    if (isnan(r) || isnan(g) || isnan(b)) continue;
    
    //float Y = 0.299f * r + 0.587f * g + 0.114f * b;
    float Y = 0.333f * r + 0.333f * g + 0.333f * b;
    
    /* calculate gaussian weight */
    float x = i % w - w/2;
    float y = i / w - h/2;
    float d2 = x*x + y*y;
    float weight = expf(-d2 / (w*w / 8.0f));
    
    num += weight * logf(0.0001f + Y);
    div += weight;
  }
  float mean = expf(num / div);
  free(samples);
  
  /* determine half-brightness level */
  const float lambda   = 0.5f; /* rate of decay */
  const float exposure = 1.0f / 8.0f;
  const float minsigma = 0.05f; /* the minimum light level that the viewer can adapt to */
  float decay = exp(-lambda * dt);
  luminance = decay * luminance + (1.0f - decay) * mean;
  float sigma = luminance / exposure;
  if (sigma < minsigma) sigma = minsigma;
  
  /* draw tonemapped image */
  glUseProgram(tonemap_program);

  glUniform1i(tonemap_uniform_tcomposite, 0);
  glUniform1f(tonemap_uniform_sigma, sigma);
  
  set_texture(0, lighting_tex_final, GL_TEXTURE_2D);
  
  glEnable(GL_FRAMEBUFFER_SRGB);
  draw_quad();
  glDisable(GL_FRAMEBUFFER_SRGB);

  glUseProgram(0);
}

void kl_gl3_debugtex(int mode) {
  switch (mode) {
    case 0:
      blit_to_screen(gbuffer_tex_depth[0],  0.18, 0.18, -0.78, -0.785, 0.001, 0.0);
      blit_to_screen(gbuffer_tex_normal[0], 0.18, 0.18, -0.39, -0.785, 1.0, 0.0);
      blit_to_screen(gbuffer_tex_albedo,    0.18, 0.18,  0.0,  -0.785, 1.0, 0.0);
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
      blit_to_screen(gbuffer_tex_back[0], 1.0, 1.0, 0.0, 0.0, 0.001, 0.0);
      blit_to_screen(gbuffer_tex_back[1], 0.18, 0.18, -0.78, -0.785, 0.001, 0.0);
      blit_to_screen(gbuffer_tex_back[2], 0.18, 0.18, -0.39, -0.785, 0.001, 0.0);
      blit_to_screen(gbuffer_tex_back[3], 0.18, 0.18,  0.0,  -0.785, 0.001, 0.0);
      blit_to_screen(gbuffer_tex_back[4], 0.18, 0.18,  0.39, -0.785, 0.001, 0.0);
      blit_to_screen(gbuffer_tex_back[5], 0.18, 0.18,  0.78, -0.785, 0.001, 0.0);
      break;
    case 4:
      blit_to_screen(ssao_tex_occlusion[0], 1.0, 1.0, 0.0, 0.0, -1.0, 1.0);
      blit_to_screen(ssao_tex_occlusion[1], 0.18, 0.18, -0.78, -0.785, -1.0, 1.0);
      blit_to_screen(ssao_tex_occlusion[2], 0.18, 0.18, -0.39, -0.785, -1.0, 1.0);
      blit_to_screen(ssao_tex_occlusion[3], 0.18, 0.18,  0.0,  -0.785, -1.0, 1.0);
      blit_to_screen(ssao_tex_occlusion[4], 0.18, 0.18,  0.39, -0.785, -1.0, 1.0);
      blit_to_screen(ssao_tex_occlusion[5], 0.18, 0.18,  0.78, -0.785, -1.0, 1.0);
      break;
    case 5:
      blit_to_screen(lighting_tex_diffuse, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0);
      break;
    case 6:
      blit_to_screen(lighting_tex_specular, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0);
      break;
    case 7:
      blit_to_screen(lighting_tex_ambient, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0);
      break;
    case 8:
      blit_to_screen(tex_shadow, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      blit_cube_to_screen(cubedepth_tex_shadow,      0.2, 0.356, 0.0, -0.6, 0.001, 0.0);
      blit_cube_to_screen(pointbounce_tex_position,  0.2, 0.356, -0.4, -0.6, 0.001, 0.0);
      blit_cube_to_screen(pointbounce_tex_normal,    0.2, 0.356, 0.4, -0.6, 0.5, 0.5);
      blit_cube_to_screen(pointbounce_tex_radiosity, 0.2, 0.356, 0.8, -0.6, 0.0001, 0.0);
      glDisable(GL_BLEND);
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

static void downsample(uint8_t *buf, int w, int h, int c, int bias) {
  for (int i=0; i < bias; i++) {
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
}

unsigned int kl_gl3_upload_texture(void *data, int w, int h, int format, bool clamp, bool filter) {
  unsigned int texture;
  
  int glfmt, glifmt, channels;
  switch (format) {
    case KL_TEXFMT_I:
	  glfmt    = GL_RED;
	  glifmt   = GL_RED;
    channels = 1;
	  break;
	case KL_TEXFMT_IA:
	  glfmt    = GL_RG;
	  glifmt   = GL_RG;
    channels = 2;
	  break;
	case KL_TEXFMT_RGB:
	  glfmt    = GL_RGB;
	  glifmt   = GL_SRGB;
    channels = 3;
	  break;
	case KL_TEXFMT_BGR:
	  glfmt    = GL_BGR;
	  glifmt   = GL_SRGB;
    channels = 3;
	  break;
	case KL_TEXFMT_RGBA:
	  glfmt    = GL_RGBA;
	  glifmt   = GL_SRGB_ALPHA;
    channels = 4;
	  break;
	case KL_TEXFMT_BGRA:
	  glfmt    = GL_BGRA;
	  glifmt   = GL_SRGB_ALPHA;
    channels = 4;
	  break;
	case KL_TEXFMT_X:
	  glfmt    = GL_RED;
	  glifmt   = GL_RED;
    channels = 1;
	  break;
	case KL_TEXFMT_XY:
	  glfmt    = GL_RG;
	  glifmt   = GL_RG;
    channels = 2;
	  break;
	case KL_TEXFMT_XYZ:
	  glfmt    = GL_RGB;
	  glifmt   = GL_RGB;
    channels = 3;
	  break;
	case KL_TEXFMT_XYZW:
	  glfmt    = GL_RGBA;
	  glifmt   = GL_RGBA;
    channels = 4;
	  break;
	default:
      fprintf(stderr, "Render: Bad texture format! (%x)\n", format);
	  return 0;
  }

  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  int bytes = w * h * channels;
  uint8_t *buf = malloc(bytes);
  memcpy(buf, data, bytes);
  if (filter) {
    downsample(buf, w, h, channels, mipbias);
  }
  glTexImage2D(GL_TEXTURE_2D, 0, glifmt, w, h, 0, glfmt, GL_UNSIGNED_BYTE, buf);
  free(buf);
  
  if (filter) {
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
  } else {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
  
  if (clamp) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  } else {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  }

  /* greyscale swizzles */
  switch (format) {
    case KL_TEXFMT_IA:
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_GREEN);
    case KL_TEXFMT_I:
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
      break;
  }

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
    case KL_RENDER_CW:
      return GL_CW;
    case KL_RENDER_CCW:
      return GL_CCW;
    case KL_RENDER_UINT8:
      return GL_UNSIGNED_BYTE;
    case KL_RENDER_UINT16:
      return GL_UNSIGNED_SHORT;
    case KL_RENDER_FLOAT:
      return GL_FLOAT;
  }
  return GL_FALSE;
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

static int create_program(char *name, unsigned int vshader, unsigned int gshader, unsigned int fshader, unsigned int *program) {
  int status;

  int id = glCreateProgram();
  *program = id;
  if (vshader != 0) glAttachShader(id, vshader);
  if (gshader != 0) glAttachShader(id, gshader);
  if (fshader != 0) glAttachShader(id, fshader);
  glLinkProgram(id);
  glGetProgramiv(id, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    glGetProgramInfoLog(id, LOGBUFFER_SIZE, NULL, logbuffer);
    fprintf(stderr, "Render: Failed to link %s\n\tDetails: %s\n", name, logbuffer);
    return -1;
  }
  return 0;
}

static unsigned int create_rendertexture(unsigned int target, bool filter, bool swizzle) {
  unsigned int texture;
  glGenTextures(1, &texture);
  glBindTexture(target, texture);
  if (filter) {
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  } else {
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
  glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  if (swizzle) {
    glTexParameteri(target, GL_TEXTURE_SWIZZLE_R, GL_RED);
    glTexParameteri(target, GL_TEXTURE_SWIZZLE_G, GL_RED);
    glTexParameteri(target, GL_TEXTURE_SWIZZLE_B, GL_RED);
  }
  glBindTexture(target, 0);
  return texture;
}

static void set_texture(int index, unsigned int texture, unsigned int target) {
  glActiveTexture(GL_TEXTURE0 + index);
  glBindTexture(target, texture);
}

static void initialize_rendertexture(unsigned int texture, unsigned int target, unsigned int format, int width, int height) {
  glBindTexture(target, texture);
  glTexImage2D(target, 0, format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glBindTexture(target, 0);
}

static void initialize_rendertexturecube(unsigned int texture, unsigned int format, int size) {
  glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
  for (int i=0; i < 6; i++) {
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  }
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

static unsigned int create_renderbuffer(unsigned int format, int width, int height) {
  unsigned int renderbuffer;
  glGenRenderbuffers(1, &renderbuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, format, width, height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  return renderbuffer;
}

static void draw_pquad() {
  GLboolean depthtest, depthwrite;
  glGetBooleanv(GL_DEPTH_TEST, &depthtest);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthwrite);
  
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  
  glBindVertexArray(vao_pquad);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
  
  if (depthtest == GL_TRUE) glEnable(GL_DEPTH_TEST);
  glDepthMask(depthwrite);
}

static void draw_quad() {
  GLboolean depthtest, depthwrite;
  glGetBooleanv(GL_DEPTH_TEST, &depthtest);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthwrite);
  
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  
  glBindVertexArray(vao_quad);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
  
  if (depthtest == GL_TRUE) glEnable(GL_DEPTH_TEST);
  glDepthMask(depthwrite);
}

static int init_gbuffer(int width, int height) {
  /* g-buffer format: */
  /* depth: 32f */
  /* depth (back): 32f */
  /* normal: 16f X, 16f Y (Lambert azimuthal equal-area projection) */
  /* diffuse: 8 red, 8 blue, 8 green, 8 unused */
  /* specular: 8 intensity, 8 exponent, 8 fresnel coeff (perpendicular reflectance), 8 unused */
  /* emissive: 8 red, 8 blue, 8 green, 8 intensity exponent */

  int w = width;
  int h = height;
  for (int i=0; i < MAXSSAOLEVELS; i++) {
    unsigned int texture;

    texture = create_rendertexture(GL_TEXTURE_RECTANGLE, true, true);
    initialize_rendertexture(texture, GL_TEXTURE_RECTANGLE, GL_R32F, w, h);
    gbuffer_tex_depth[i] = texture;
    
    texture = create_rendertexture(GL_TEXTURE_RECTANGLE, true, true);
    initialize_rendertexture(texture, GL_TEXTURE_RECTANGLE, GL_R32F, w, h);
    gbuffer_tex_back[i] = texture;
    
    texture = create_rendertexture(GL_TEXTURE_RECTANGLE, false, false);
    initialize_rendertexture(texture, GL_TEXTURE_RECTANGLE, GL_RG16, w, h);
    gbuffer_tex_normal[i] = texture;

    w >>= 1;
    h >>= 1;
  }

  gbuffer_tex_albedo  = create_rendertexture(GL_TEXTURE_RECTANGLE, false, false);
  gbuffer_tex_specular = create_rendertexture(GL_TEXTURE_RECTANGLE, false, false);
  gbuffer_tex_emissive = create_rendertexture(GL_TEXTURE_RECTANGLE, false, false);
  initialize_rendertexture(gbuffer_tex_albedo,  GL_TEXTURE_RECTANGLE, GL_RGBA8, width, height);
  initialize_rendertexture(gbuffer_tex_specular, GL_TEXTURE_RECTANGLE, GL_RGBA8, width, height);
  initialize_rendertexture(gbuffer_tex_emissive, GL_TEXTURE_RECTANGLE, GL_RGBA8, width, height);
 
  glGenFramebuffers(1, &gbuffer_fbo_front);
  glGenFramebuffers(1, &gbuffer_fbo_back);
  glGenFramebuffers(MAXSSAOLEVELS-1, downsample_fbo);

  glBindFramebuffer(GL_FRAMEBUFFER, gbuffer_fbo_front);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, gbuffer_tex_depth[0], 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_RECTANGLE, gbuffer_tex_normal[0], 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_RECTANGLE, gbuffer_tex_albedo,  0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_RECTANGLE, gbuffer_tex_specular, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_RECTANGLE, gbuffer_tex_emissive, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_depth);
  int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "Render: G-buffer is incomplete.\n\tDetails: %x\n", status);
    return -1;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, gbuffer_fbo_back);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, gbuffer_tex_back[0], 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_depth);
  status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "Render: G-buffer is incomplete.\n\tDetails: %x\n", status);
    return -1;
  }

  for (int i=0; i<MAXSSAOLEVELS-1; i++) {
    glBindFramebuffer(GL_FRAMEBUFFER, downsample_fbo[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, gbuffer_tex_depth[i+1], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_RECTANGLE, gbuffer_tex_back[i+1], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_RECTANGLE, gbuffer_tex_normal[i+1], 0);
    int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      fprintf(stderr, "Render: Downsampling g-buffer is incomplete.\n\tDetails: %x\n", status);
      return -1;
    }
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  if (create_shader("g-buffer vertex shader", GL_VERTEX_SHADER, vshader_gbuffer_src, &gbuffer_vshader) < 0) return -1;
  if (create_shader("g-buffer fragment shader", GL_FRAGMENT_SHADER, fshader_gbuffer_src, &gbuffer_fshader) < 0) return -1;
  if (create_program("g-buffer shader program", gbuffer_vshader, 0, gbuffer_fshader, &gbuffer_program) < 0) return -1;

  bind_ubo(gbuffer_program, "scene", 0);
  gbuffer_uniform_tdiffuse  = glGetUniformLocation(gbuffer_program, "tdiffuse");
  gbuffer_uniform_tnormal   = glGetUniformLocation(gbuffer_program, "tnormal");
  gbuffer_uniform_tspecular = glGetUniformLocation(gbuffer_program, "tspecular");
  gbuffer_uniform_temissive = glGetUniformLocation(gbuffer_program, "temissive");

  if (create_shader("g-buffer back face vertex shader", GL_VERTEX_SHADER, vshader_gbufferback_src, &gbufferback_vshader) < 0) return -1;
  if (create_shader("g-buffer back face fragment shader", GL_FRAGMENT_SHADER, fshader_gbufferback_src, &gbufferback_fshader) < 0) return -1;
  if (create_program("g-buffer back face shader program", gbufferback_vshader, 0, gbufferback_fshader, &gbufferback_program) < 0) return -1;

  bind_ubo(gbufferback_program, "scene", 0);
  gbufferback_uniform_tdiffuse  = glGetUniformLocation(gbufferback_program, "tdiffuse");

  if (create_shader("downsampling vertex shader", GL_VERTEX_SHADER, vshader_downsample_src, &downsample_vshader) < 0) return -1;
  if (create_shader("downsampling fragment shader", GL_FRAGMENT_SHADER, fshader_downsample_src, &downsample_fshader) < 0) return -1;
  if (create_program("downsampling shader program", downsample_vshader, 0, downsample_fshader, &downsample_program) < 0) return -1;

  downsample_uniform_tdepth  = glGetUniformLocation(downsample_program, "tdepth");
  downsample_uniform_tback   = glGetUniformLocation(downsample_program, "tback");
  downsample_uniform_tnormal = glGetUniformLocation(downsample_program, "tnormal");

  return 0;
}

static int init_ssao(int width, int height) {
  float noise[256*256*4];
  for (int i = 0; i < 256*256*4; i++) {
    noise[i] = (float)rand() / (float)RAND_MAX;
  }
  glGenTextures(1, &tex_noise);
  glBindTexture(GL_TEXTURE_2D, tex_noise);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 256, 256, 0, GL_RGBA, GL_FLOAT, noise);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glBindTexture(GL_TEXTURE_2D, 0);
  
  int w = width;
  int h = height;
  for (int i=0; i < 6; i++) {
    unsigned int texture = create_rendertexture(GL_TEXTURE_RECTANGLE, true, true);
    initialize_rendertexture(texture, GL_TEXTURE_RECTANGLE, GL_R8, w, h);
    ssao_tex_occlusion[i] = texture;

    w >>= 1;
    h >>= 1;
  }

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
  if (create_program("ssao shader program", ssao_vshader, 0, ssao_fshader, &ssao_program) < 0) return -1;

  bind_ubo(ssao_program, "scene", 0);
  ssao_uniform_tdepth = glGetUniformLocation(ssao_program, "tdepth");
  ssao_uniform_tback  = glGetUniformLocation(ssao_program, "tback");
  ssao_uniform_tnoise = glGetUniformLocation(ssao_program, "tnoise");

  if (create_shader("bilateral filter vertex shader", GL_VERTEX_SHADER, vshader_bilateral_src, &bilateral_vshader) < 0) return -1;
  if (create_shader("bilateral filter fragment shader", GL_FRAGMENT_SHADER, fshader_bilateral_src, &bilateral_fshader) < 0) return -1;
  if (create_program("bilateral filter shader program", bilateral_vshader, 0, bilateral_fshader, &bilateral_program) < 0) return -1;

  bilateral_uniform_thdepth = glGetUniformLocation(bilateral_program, "thdepth");
  bilateral_uniform_tldepth = glGetUniformLocation(bilateral_program, "tldepth");
  bilateral_uniform_timage  = glGetUniformLocation(bilateral_program, "timage");

  return 0;
}

static int init_blit() {
  /* screen blitting shader program */
  if (create_shader("screen vertex shader", GL_VERTEX_SHADER, vshader_blit_src, &blit_vshader) < 0) return -1;
  if (create_shader("screen fragment shader", GL_FRAGMENT_SHADER, fshader_blit_src, &blit_fshader) < 0) return -1;
  if (create_program("screen shader program", blit_vshader, 0, blit_fshader, &blit_program) < 0) return -1;

  blit_uniform_image  = glGetUniformLocation(blit_program, "image");
  blit_uniform_size   = glGetUniformLocation(blit_program, "size");
  blit_uniform_offset = glGetUniformLocation(blit_program, "offset");
  blit_uniform_colorscale  = glGetUniformLocation(blit_program, "colorscale");
  blit_uniform_coloroffset = glGetUniformLocation(blit_program, "coloroffset");
  
  if (create_shader("screen cube fragment shader", GL_FRAGMENT_SHADER, fshader_blit_cube_src, &blit_cube_fshader) < 0) return -1;
  if (create_program("screen cube shader program", blit_vshader, 0, blit_cube_fshader, &blit_cube_program) < 0) return -1;

  blit_cube_uniform_image  = glGetUniformLocation(blit_cube_program, "image");
  blit_cube_uniform_size   = glGetUniformLocation(blit_cube_program, "size");
  blit_cube_uniform_offset = glGetUniformLocation(blit_cube_program, "offset");
  blit_cube_uniform_colorscale  = glGetUniformLocation(blit_cube_program, "colorscale");
  blit_cube_uniform_coloroffset = glGetUniformLocation(blit_cube_program, "coloroffset");

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

  bind_ubo(minimal_program, "scene", 0);
  minimal_uniform_modelmatrix = glGetUniformLocation(minimal_program, "modelmatrix");

  if (create_shader("flat color fragment shader", GL_FRAGMENT_SHADER, fshader_flatcolor_src, &flatcolor_fshader) < 0) return -1;
  if (create_program("flat color shader program", minimal_vshader, 0, flatcolor_fshader, &flatcolor_program) < 0) return -1;

  bind_ubo(flatcolor_program, "scene", 0);
  flatcolor_uniform_modelmatrix = glGetUniformLocation(flatcolor_program, "modelmatrix");
  flatcolor_uniform_color     = glGetUniformLocation(flatcolor_program, "color");

  return 0;
}

static int init_lighting(int width, int height) {
  lighting_tex_final = create_rendertexture(GL_TEXTURE_2D, true, false);
  initialize_rendertexture(lighting_tex_final, GL_TEXTURE_2D, GL_RGBA16F, width, height);

  glGenFramebuffers(1, &lighting_fbo_final);
  glBindFramebuffer(GL_FRAMEBUFFER, lighting_fbo_final);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lighting_tex_final, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo_depth);
  int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "Render: HDR lighting framebuffer is incomplete.\n\tDetails: %x\n", status);
    return -1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  
  lighting_tex_diffuse = create_rendertexture(GL_TEXTURE_RECTANGLE, true, false);
  initialize_rendertexture(lighting_tex_diffuse, GL_TEXTURE_RECTANGLE, GL_RGBA16F, width, height);
  lighting_tex_specular = create_rendertexture(GL_TEXTURE_RECTANGLE, true, false);
  initialize_rendertexture(lighting_tex_specular, GL_TEXTURE_RECTANGLE, GL_RGBA16F, width, height);
  lighting_tex_ambient = create_rendertexture(GL_TEXTURE_RECTANGLE, true, false);
  initialize_rendertexture(lighting_tex_ambient, GL_TEXTURE_RECTANGLE, GL_RGBA16F, width, height);

  glGenFramebuffers(1, &lighting_fbo_intermediate);
  glBindFramebuffer(GL_FRAMEBUFFER, lighting_fbo_intermediate);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, lighting_tex_diffuse, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_RECTANGLE, lighting_tex_specular, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_RECTANGLE, lighting_tex_ambient, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo_depth);
  status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "Render: Lighting component framebuffer is incomplete.\n\tDetails: %x\n", status);
    return -1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  
  /* half-resolution lighting buffer -- for gathering indirect light */
  lighting_rbo_indirect = create_renderbuffer(GL_DEPTH24_STENCIL8, width/2, height/2);
  lighting_tex_indirect = create_rendertexture(GL_TEXTURE_RECTANGLE, true, false);
  initialize_rendertexture(lighting_tex_indirect, GL_TEXTURE_RECTANGLE, GL_RGBA16F, width/2, height/2);

  glGenFramebuffers(1, &lighting_fbo_indirect);
  glBindFramebuffer(GL_FRAMEBUFFER, lighting_fbo_indirect);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, lighting_tex_indirect, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, lighting_rbo_indirect);
  status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "Render: Indirect lighting framebuffer is incomplete.\n\tDetails: %x\n", status);
    return -1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  
  
  tex_shadow = create_rendertexture(GL_TEXTURE_RECTANGLE, true, true);
  initialize_rendertexture(tex_shadow, GL_TEXTURE_RECTANGLE, GL_R8, width, height);

  glGenFramebuffers(1, &fbo_shadow);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_shadow);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, tex_shadow, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo_depth);
  status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "Render: shadow framebuffer is incomplete.\n\tDetails: %x\n", status);
    return -1;
  }
  
  shadowfilter_tex_shadow = create_rendertexture(GL_TEXTURE_RECTANGLE, true, true);
  initialize_rendertexture(shadowfilter_tex_shadow, GL_TEXTURE_RECTANGLE, GL_R8, width, height);

  glGenFramebuffers(1, &shadowfilter_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, shadowfilter_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, shadowfilter_tex_shadow, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo_depth);
  status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "Render: shadow framebuffer is incomplete.\n\tDetails: %x\n", status);
    return -1;
  }
  
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  
  cubedepth_tex_shadow = create_rendertexture(GL_TEXTURE_CUBE_MAP, true, true);
  initialize_rendertexturecube(cubedepth_tex_shadow, GL_R32F, shadowsize);
  cubedepth_rbo_depth  = create_renderbuffer(GL_DEPTH_COMPONENT32, shadowsize, shadowsize);

  glGenFramebuffers(6, cubedepth_fbo);
  for (int i=0; i < 6; i++) {
    glBindFramebuffer(GL_FRAMEBUFFER, cubedepth_fbo[i]);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, cubedepth_rbo_depth);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubedepth_tex_shadow, 0);
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      fprintf(stderr, "Render: Point light shadow mapping framebuffer is incomplete.\n\tDetails: %x\n", status);
      return -1;
    }
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  
  /* bounce format: */
  /* position:  32f X, 32f Y, 32f Z, 32f unused */
  /* normal:    32f X, 32f Y, 32f Z, 32f unused */
  /* radiosity: 32f R, 32f G, 32f B, 32f falloff (used to normalize VPLs) */
  /* high-resolution formats can be used because bounce maps will be very low resolution */
  
  pointbounce_tex_position  = create_rendertexture(GL_TEXTURE_CUBE_MAP, false, false);
  pointbounce_tex_normal    = create_rendertexture(GL_TEXTURE_CUBE_MAP, false, false);
  pointbounce_tex_radiosity = create_rendertexture(GL_TEXTURE_CUBE_MAP, false, false);
  initialize_rendertexturecube(pointbounce_tex_position,  GL_RGBA32F, bouncemapsize);
  initialize_rendertexturecube(pointbounce_tex_normal,    GL_RGBA32F, bouncemapsize);
  initialize_rendertexturecube(pointbounce_tex_radiosity, GL_RGBA32F, bouncemapsize);
  
  pointbounce_rbo_depth = create_renderbuffer(GL_DEPTH_COMPONENT32, bouncemapsize, bouncemapsize);

  glGenFramebuffers(6, pointbounce_fbo);
  for (int i=0; i < 6; i++) {
    glBindFramebuffer(GL_FRAMEBUFFER, pointbounce_fbo[i]);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, pointbounce_rbo_depth);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, pointbounce_tex_position, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, pointbounce_tex_normal, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, pointbounce_tex_radiosity, 0);
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      fprintf(stderr, "Render: Point light shadow mapping framebuffer is incomplete.\n\tDetails: %x\n", status);
      return -1;
    }
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  
  pointbounce_mask = malloc(bouncemapsize * bouncemapsize * sizeof(bool));
  for (int i = 0; i < bouncemapsize; i++) {
    float x = 2.0f * ((float)i + 0.5f) / bouncemapsize - 1.0f;
    for (int j = 0; j < bouncemapsize; j++) {
      float y = 2.0f * ((float)j + 0.5f) / bouncemapsize - 1.0f;
      float r = sqrtf(x*x + y*y + 1.0f);
      float prob = 1.0f / r;
      
      float val = (float)rand() / (float)RAND_MAX;
      
      int index = j * bouncemapsize + i;
      pointbounce_mask[index] = val <= prob;
      printf("%d", pointbounce_mask[index]);
    }
    printf("\n");
  }

  if (create_shader("final lighting vertex shader", GL_VERTEX_SHADER, vshader_lighting_src, &lighting_vshader) < 0) return -1;
  if (create_shader("final lighting fragment shader", GL_FRAGMENT_SHADER, fshader_lighting_src, &lighting_fshader) < 0) return -1;
  if (create_program("final lighting shader program", lighting_vshader, 0, lighting_fshader, &lighting_program) < 0) return -1;

  lighting_uniform_tdiffuse   = glGetUniformLocation(lighting_program, "tdiffuse");
  lighting_uniform_tspecular  = glGetUniformLocation(lighting_program, "tspecular");
  lighting_uniform_tambient   = glGetUniformLocation(lighting_program, "tambient");
  lighting_uniform_talbedo    = glGetUniformLocation(lighting_program, "talbedo");
  lighting_uniform_tocclusion = glGetUniformLocation(lighting_program, "tocclusion");
  lighting_uniform_temissive  = glGetUniformLocation(lighting_program, "temissive");

  /* point lighting */
  if (create_shader("point lighting vertex shader", GL_VERTEX_SHADER, vshader_pointlight_src, &pointlight_vshader) < 0) return -1;
  if (create_shader("point lighting fragment shader", GL_FRAGMENT_SHADER, fshader_pointlight_src, &pointlight_fshader) < 0) return -1;
  if (create_program("point lighting shader program", pointlight_vshader, 0, pointlight_fshader, &pointlight_program) < 0) return -1;

  bind_ubo(pointlight_program, "scene", 0);
  bind_ubo(pointlight_program, "pointlight", 1);
  pointlight_uniform_tdepth     = glGetUniformLocation(pointlight_program, "tdepth");
  pointlight_uniform_tnormal    = glGetUniformLocation(pointlight_program, "tnormal");
  pointlight_uniform_tspecular  = glGetUniformLocation(pointlight_program, "tspecular");
  pointlight_uniform_tshadow    = glGetUniformLocation(pointlight_program, "tshadow");

  /* point light shadow mapping (cubemap render) */
  if (create_shader("omnidirectional depth vertex shader", GL_VERTEX_SHADER, vshader_cubedepth_src, &cubedepth_vshader) < 0) return -1;
  if (create_shader("omnidirectional depth fragment shader", GL_FRAGMENT_SHADER, fshader_cubedepth_src, &cubedepth_fshader) < 0) return -1;
  if (create_program("omnidirectional depth shader program", cubedepth_vshader, 0, cubedepth_fshader, &cubedepth_program) < 0) return -1;
  
  kl_mat4f_t cubeproj[6];
  kl_mat4f_t proj;
  kl_mat4f_frustum(&proj, -0.1f, 0.1f, -0.1f, 0.1f, 0.1f, 10000.0f);
  kl_mat4f_t view;

  view = (kl_mat4f_t)KL_MAT4F_POSX;
  kl_mat4f_mul(&cubeproj[0], &proj, &view);
  view = (kl_mat4f_t)KL_MAT4F_NEGX;
  kl_mat4f_mul(&cubeproj[1], &proj, &view);
  view = (kl_mat4f_t)KL_MAT4F_POSY;
  kl_mat4f_mul(&cubeproj[2], &proj, &view);
  view = (kl_mat4f_t)KL_MAT4F_NEGY;
  kl_mat4f_mul(&cubeproj[3], &proj, &view);
  view = (kl_mat4f_t)KL_MAT4F_POSZ;
  kl_mat4f_mul(&cubeproj[4], &proj, &view);
  view = (kl_mat4f_t)KL_MAT4F_NEGZ;
  kl_mat4f_mul(&cubeproj[5], &proj, &view);
  
  cubedepth_uniform_center   = glGetUniformLocation(cubedepth_program, "center");
  cubedepth_uniform_cubeproj = glGetUniformLocation(cubedepth_program, "cubeproj");
  cubedepth_uniform_face     = glGetUniformLocation(cubedepth_program, "face");
  cubedepth_uniform_tdiffuse = glGetUniformLocation(cubedepth_program, "tdiffuse");
  
  glUseProgram(cubedepth_program);
  glUniformMatrix4fv(cubedepth_uniform_cubeproj, 6, GL_FALSE, (float*)cubeproj);
  glUseProgram(0);
  
  if (create_shader("point-light shadow vertex shader", GL_VERTEX_SHADER, vshader_pointshadow_src, &pointshadow_vshader) < 0) return -1;
  if (create_shader("point-light shadow fragment shader", GL_FRAGMENT_SHADER, fshader_pointshadow_src, &pointshadow_fshader) < 0) return -1;
  if (create_program("point-light shadow shader program", pointshadow_vshader, 0, pointshadow_fshader, &pointshadow_program) < 0) return -1;
  
  bind_ubo(pointshadow_program, "scene", 0);
  pointshadow_uniform_tscreendepth = glGetUniformLocation(pointshadow_program, "tscreendepth");
  pointshadow_uniform_tcubedepth   = glGetUniformLocation(pointshadow_program, "tcubedepth");
  pointshadow_uniform_center       = glGetUniformLocation(pointshadow_program, "center");
  
  if (create_shader("screen-space shadow filter vertex shader", GL_VERTEX_SHADER, vshader_shadowfilter_src, &shadowfilter_vshader) < 0) return -1;
  if (create_shader("screen-space shadow filter fragment shader", GL_FRAGMENT_SHADER, fshader_shadowfilter_src, &shadowfilter_fshader) < 0) return -1;
  if (create_program("screen-space shadow filter shader program", shadowfilter_vshader, 0, shadowfilter_fshader, &shadowfilter_program) < 0) return -1;
  
  shadowfilter_uniform_tdepth  = glGetUniformLocation(shadowfilter_program, "tdepth");
  shadowfilter_uniform_tnormal = glGetUniformLocation(shadowfilter_program, "tnormal");
  shadowfilter_uniform_tshadow = glGetUniformLocation(shadowfilter_program, "tshadow");
  shadowfilter_uniform_pass    = glGetUniformLocation(shadowfilter_program, "pass");
  
  if (create_shader("point-light bounce vertex shader", GL_VERTEX_SHADER, vshader_pointbounce_src, &pointbounce_vshader) < 0) return -1;
  if (create_shader("point-light bounce fragment shader", GL_FRAGMENT_SHADER, fshader_pointbounce_src, &pointbounce_fshader) < 0) return -1;
  if (create_program("point-light bounce shader program", pointbounce_vshader, 0, pointbounce_fshader, &pointbounce_program) < 0) return -1;
  
  bind_ubo(pointbounce_program, "pointlight", 1);
  pointbounce_uniform_tdiffuse = glGetUniformLocation(pointbounce_program, "tdiffuse");
  pointbounce_uniform_tnormal  = glGetUniformLocation(pointbounce_program, "tnormal");
  pointbounce_uniform_cubeproj = glGetUniformLocation(pointbounce_program, "cubeproj");
  pointbounce_uniform_face     = glGetUniformLocation(pointbounce_program, "face");
  
  glUseProgram(pointbounce_program);
  glUniformMatrix4fv(pointbounce_uniform_cubeproj, 6, GL_FALSE, (float*)cubeproj);
  glUseProgram(0);

  if (create_shader("indirect lighting vertex shader", GL_VERTEX_SHADER, vshader_surfacelight_src, &surfacelight_vshader) < 0) return -1;
  if (create_shader("indirect lighting fragment shader", GL_FRAGMENT_SHADER, fshader_surfacelight_src, &surfacelight_fshader) < 0) return -1;
  if (create_program("indirect lighting shader program", surfacelight_vshader, 0, surfacelight_fshader, &surfacelight_program) < 0) return -1;
  
  bind_ubo(surfacelight_program, "scene", 0);
  surfacelight_uniform_tdepth = glGetUniformLocation(surfacelight_program, "tdepth");
  surfacelight_uniform_tnormal = glGetUniformLocation(surfacelight_program, "tnormal");
  surfacelight_uniform_position = glGetUniformLocation(surfacelight_program, "position");
  surfacelight_uniform_direction = glGetUniformLocation(surfacelight_program, "direction");
  surfacelight_uniform_radiosity = glGetUniformLocation(surfacelight_program, "radiosity");

  /* environmental lighting */
  if (create_shader("environmental lighting vertex shader", GL_VERTEX_SHADER, vshader_envlight_src, &envlight_vshader) < 0) return -1;
  if (create_shader("environmental lighting fragment shader", GL_FRAGMENT_SHADER, fshader_envlight_src, &envlight_fshader) < 0) return -1;
  if (create_program("environmental lighting shader program", envlight_vshader, 0, envlight_fshader, &envlight_program) < 0) return -1;

  bind_ubo(envlight_program, "scene", 0);
  bind_ubo(envlight_program, "envlight", 1);
  envlight_uniform_tdepth     = glGetUniformLocation(envlight_program, "tdepth");
  envlight_uniform_tnormal    = glGetUniformLocation(envlight_program, "tnormal");
  envlight_uniform_tspecular  = glGetUniformLocation(envlight_program, "tspecular");

  return 0;
}

static int init_tonemap() {
  if (create_shader("tonemapping vertex shader", GL_VERTEX_SHADER, vshader_tonemap_src, &tonemap_vshader) < 0) return -1;
  if (create_shader("tonemapping fragment shader", GL_FRAGMENT_SHADER, fshader_tonemap_src, &tonemap_fshader) < 0) return -1;
  if (create_program("tonemapping shader program", tonemap_vshader, 0, tonemap_fshader, &tonemap_program) < 0) return -1;

  tonemap_uniform_tcomposite = glGetUniformLocation(tonemap_program, "tcomposite");
  tonemap_uniform_sigma      = glGetUniformLocation(tonemap_program, "sigma");

  return 0;
}

static void blit_to_screen(unsigned int texture, float w, float h, float x, float y, float scale, float offset) {
  glUseProgram(blit_program);

  glUniform1i(blit_uniform_image, 0);
  
  glUniform2f(blit_uniform_size, w, h);
  glUniform2f(blit_uniform_offset, x, y);
  glUniform1f(blit_uniform_colorscale, scale);
  glUniform1f(blit_uniform_coloroffset, offset);
  
  set_texture(0, texture, GL_TEXTURE_RECTANGLE);

  draw_quad();

  glUseProgram(0);
}

static void blit_cube_to_screen(unsigned int texture, float w, float h, float x, float y, float scale, float offset) {
  glUseProgram(blit_cube_program);

  glUniform1i(blit_cube_uniform_image, 0);

  glUniform2f(blit_cube_uniform_size, w, h);
  glUniform2f(blit_cube_uniform_offset, x, y);
  glUniform1f(blit_cube_uniform_colorscale, scale);
  glUniform1f(blit_cube_uniform_coloroffset, offset);
  
  set_texture(0, texture, GL_TEXTURE_CUBE_MAP);

  draw_quad();

  glUseProgram(0);
}

static void bind_ubo(unsigned int program, char *name, unsigned int binding) {
  unsigned int index = glGetUniformBlockIndex(program, name);
  glUniformBlockBinding(program, index, binding);
}

/* vim: set ts=2 sw=2 et */
