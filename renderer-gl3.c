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

static const float anisotropy = 4.0f;

static int convertenum(int value);
static int typesize(int value);
static int channels(int value);
static int create_shader(char *name, int type, const char *src, unsigned int *shader);
static int init_gbuffer(int w, int h);
static int init_blit();
static int init_lighting(int w, int h);
static int init_tonemap();
static void blit_to_screen(unsigned int texture, float w, float h, float x, float y);

static unsigned int gbuffer_fshader;
static unsigned int gbuffer_vshader;
static unsigned int gbuffer_program;
static int gbuffer_uniform_vmatrix;
static int gbuffer_uniform_vpmatrix;
static int gbuffer_uniform_tdiffuse;
static int gbuffer_uniform_tnormal;
static int gbuffer_uniform_tspecular;
static int gbuffer_uniform_temissive;
static unsigned int gbuffer_tex_depth;
static unsigned int gbuffer_tex_diffuse;
static unsigned int gbuffer_tex_normal;
static unsigned int gbuffer_tex_specular;
static unsigned int gbuffer_tex_emissive;
static unsigned int gbuffer_fbo;

static unsigned int rbo_depth;
static unsigned int tex_lighting;
static unsigned int fbo_lighting;

static unsigned int pointlight_fshader;
static unsigned int pointlight_vshader;
static unsigned int pointlight_program;
static int pointlight_uniform_vmatrix;
static int pointlight_uniform_vpmatrix;
static int pointlight_uniform_ivpmatrix;
static int pointlight_uniform_pointlight;
static int pointlight_uniform_tdepth;
static int pointlight_uniform_tdiffuse;
static int pointlight_uniform_tnormal;
static int pointlight_uniform_tspecular;

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
  
static int vbo_rect_coords;
static int vbo_rect_tris;
static int vao_rect;
static int vbo_sphere_coords;
static int vbo_sphere_tris;
static int vao_sphere;

typedef struct uniform_light {
  kl_vec4f_t position;
  float r, g, b;
  float intensity;
} uniform_light_t;

#define LOGBUFFER_SIZE 0x4000
static char logbuffer[LOGBUFFER_SIZE];


/* ----------------------------------------------- */

int kl_gl3_init() {
  int w, h;
  kl_vid_size(&w, &h);

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CCW);
  glCullFace(GL_BACK);

  /* create a shared depth renderbuffer */
  glGenRenderbuffers(1, &rbo_depth);
  glBindRenderbuffer(GL_RENDERBUFFER, rbo_depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, w, h);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);

  if (init_gbuffer(w, h) < 0) return -1;
  if (init_lighting(w, h) < 0) return -1;
  if (init_tonemap() < 0) return -1;
  if (init_blit() < 0) return -1;

  /* create screen blitting VBO/VAO */
  float verts_rect[8] = {
    0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f
  };
  unsigned int tris_rect[6] = { 0, 1, 2, 0, 2, 3 };
  vbo_rect_coords = kl_gl3_upload_vertdata(verts_rect, 8*sizeof(float));
  vbo_rect_tris   = kl_gl3_upload_tris(tris_rect, 6*sizeof(unsigned int));

  kl_render_attrib_t attrib_rect = {
    .index  = 0,
    .size   = 2,
    .type   = KL_RENDER_FLOAT,
    .buffer = vbo_rect_coords
  };
  vao_rect = kl_gl3_define_attribs(vbo_rect_tris, &attrib_rect, 1);

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

void kl_gl3_begin_pass_gbuffer(kl_mat4f_t *vmatrix, kl_mat4f_t *vpmatrix) {
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gbuffer_fbo);
  unsigned int attachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4};
  glDrawBuffers(5, attachments);

  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  glUseProgram(gbuffer_program);
  
  glUniformMatrix4fv(gbuffer_uniform_vmatrix,  1, GL_FALSE, (float*)vmatrix);
  glUniformMatrix4fv(gbuffer_uniform_vpmatrix, 1, GL_FALSE, (float*)vpmatrix);

  glUniform1i(gbuffer_uniform_tdiffuse, 0);
  glUniform1i(gbuffer_uniform_tnormal, 1);
  glUniform1i(gbuffer_uniform_tspecular, 2);
  glUniform1i(gbuffer_uniform_temissive, 3);
}

void kl_gl3_end_pass_gbuffer() {
  glUseProgram(0);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void kl_gl3_draw_pass_gbuffer(kl_model_t *model) {
  glFrontFace(convertenum(model->winding));
  glBindVertexArray(model->attribs);
  for (int i=0; i < model->mesh_n; i++) {
    kl_mesh_t *mesh = model->mesh + i;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mesh->material.diffuse->id);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, mesh->material.normal->id);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, mesh->material.specular->id);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, mesh->material.emissive->id);

    glDrawElements(GL_TRIANGLES, 3*mesh->tris_n, GL_UNSIGNED_INT, (void*)(3*mesh->tris_i*sizeof(int)));
  }
  glBindVertexArray(0);
  glFrontFace(GL_CCW);
}

void kl_gl3_begin_pass_lighting(kl_mat4f_t *vmatrix, kl_mat4f_t *vpmatrix, kl_mat4f_t *ivpmatrix) {
  glDepthFunc(GL_GREATER);
  glDepthMask(GL_FALSE); /* disable depth writes */
  glCullFace(GL_FRONT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE); /* additive blending */

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_lighting);
  unsigned int attachments[] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffers(1, attachments);

  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(pointlight_program);

  glUniformMatrix4fv(pointlight_uniform_vmatrix,  1, GL_FALSE, (float*)vmatrix);
  glUniformMatrix4fv(pointlight_uniform_vpmatrix, 1, GL_FALSE, (float*)vpmatrix);
  glUniformMatrix4fv(pointlight_uniform_ivpmatrix, 1, GL_FALSE, (float*)ivpmatrix);

  glUniform1i(pointlight_uniform_tdepth, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, gbuffer_tex_depth);

  glUniform1i(pointlight_uniform_tdiffuse, 1);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, gbuffer_tex_diffuse);

  glUniform1i(pointlight_uniform_tnormal, 2);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, gbuffer_tex_normal);

  glUniform1i(pointlight_uniform_tspecular, 3);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, gbuffer_tex_specular);

  //glUniform1i(pointlight_uniform_temissive, 4);
  //glActiveTexture(GL_TEXTURE4);
  //glBindTexture(GL_TEXTURE_2D, gbuffer_tex_emissive);

  glBindVertexArray(vao_sphere);
}

void kl_gl3_end_pass_lighting() {
  glBindVertexArray(0);

  glUseProgram(0);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

  glDisable(GL_BLEND);
  glCullFace(GL_BACK);
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LEQUAL);
}

void kl_gl3_draw_pass_lighting(unsigned int *light) {
  glBindBufferBase(GL_UNIFORM_BUFFER, pointlight_uniform_pointlight, *light);

  glDrawElements(GL_TRIANGLES, SPHERE_NUMTRIS * 3, GL_UNSIGNED_INT, 0);
} 

void kl_gl3_composite() {
  glUseProgram(tonemap_program);

  glUniform1i(tonemap_uniform_tcomposite, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex_lighting);

  glBindVertexArray(vao_rect);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0); 

  glBindVertexArray(0);

  glUseProgram(0);
}

void kl_gl3_debugtex() {
  /* blit debug images */
  blit_to_screen(gbuffer_tex_depth,    0.4, 0.4, -1.0, -1.0);
  blit_to_screen(gbuffer_tex_diffuse,  0.4, 0.4, -0.6, -1.0);
  blit_to_screen(gbuffer_tex_normal,   0.4, 0.4, -0.2, -1.0);
  blit_to_screen(gbuffer_tex_specular, 0.4, 0.4,  0.2, -1.0);
  blit_to_screen(gbuffer_tex_emissive, 0.4, 0.4,  0.6, -1.0);
}

unsigned int kl_gl3_upload_vertdata(void *data, int n) {
  unsigned int vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, n, data, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  return vbo;
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
  int glformat = convertenum(format);
  if (glformat == GL_FALSE) {
    fprintf(stderr, "Render: Bad texture format! (%x)\n", format);
    return 0;
  }

  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  assert(type == KL_RENDER_UINT8);
  int c = channels(format);
  int bytes = w * h * c * typesize(type);
  uint8_t *buf = malloc(bytes);
  memcpy(buf, data, bytes);
  for (int i=0; ; i++){
    glTexImage2D(GL_TEXTURE_2D, i, glformat, w, h, 0, glformat, gltype, buf);
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
  free(buf);

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

static int init_gbuffer(int w, int h) {
  int status;

  /* g-buffer format: */
  /* depth: 32f */
  /* diffuse: 8 red, 8 blue, 8 green, 8 unused */
  /* normal: 16f x, 16f y (z reconstructed) */
  /* specular: 8 red, 8 blue, 8 green, 8 specular exponent */
  /* emissive: 8 red, 8 blue, 8 green, 8 intensity exponent */
  
  glGenTextures(1, &gbuffer_tex_depth);
  glBindTexture(GL_TEXTURE_2D, gbuffer_tex_depth);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, w, h, 0, GL_RED, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);

  glGenTextures(1, &gbuffer_tex_diffuse);
  glBindTexture(GL_TEXTURE_2D, gbuffer_tex_diffuse);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glGenTextures(1, &gbuffer_tex_normal);
  glBindTexture(GL_TEXTURE_2D, gbuffer_tex_normal);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, w, h, 0, GL_RG, GL_UNSIGNED_SHORT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glGenTextures(1, &gbuffer_tex_specular);
  glBindTexture(GL_TEXTURE_2D, gbuffer_tex_specular);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glGenTextures(1, &gbuffer_tex_emissive);
  glBindTexture(GL_TEXTURE_2D, gbuffer_tex_emissive);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindTexture(GL_TEXTURE_2D, 0);

  
  glGenFramebuffers(1, &gbuffer_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, gbuffer_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gbuffer_tex_depth, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gbuffer_tex_diffuse, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gbuffer_tex_normal, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, gbuffer_tex_specular, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, gbuffer_tex_emissive, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_depth);
  status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "Render: G-buffer is incomplete.\n\tDetails: %x\n", status);
    return -1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  if (create_shader("g-buffer vertex shader", GL_VERTEX_SHADER, vshader_gbuffer_src, &gbuffer_vshader) < 0) return -1;
  if (create_shader("g-buffer fragment shader", GL_FRAGMENT_SHADER, fshader_gbuffer_src, &gbuffer_fshader) < 0) return -1;

  gbuffer_program = glCreateProgram();
  glAttachShader(gbuffer_program, gbuffer_vshader);
  glAttachShader(gbuffer_program, gbuffer_fshader);
  glLinkProgram(gbuffer_program);
  glGetProgramiv(gbuffer_program, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    glGetProgramInfoLog(gbuffer_program, LOGBUFFER_SIZE, NULL, logbuffer);
    fprintf(stderr, "Render: Failed to compile gbuffer shader program.\n\tDetails: %s\n", logbuffer);
    return -1;
  }

  gbuffer_uniform_vmatrix   = glGetUniformLocation(gbuffer_program, "vmatrix");
  gbuffer_uniform_vpmatrix  = glGetUniformLocation(gbuffer_program, "vpmatrix");
  gbuffer_uniform_tdiffuse  = glGetUniformLocation(gbuffer_program, "tdiffuse");
  gbuffer_uniform_tnormal   = glGetUniformLocation(gbuffer_program, "tnormal");
  gbuffer_uniform_tspecular = glGetUniformLocation(gbuffer_program, "tspecular");
  gbuffer_uniform_temissive = glGetUniformLocation(gbuffer_program, "temissive");
  return 0;
}

static int init_blit() {
  /* screen blitting shader program */
  if (create_shader("screen vertex shader", GL_VERTEX_SHADER, vshader_blit_src, &blit_vshader) < 0) return -1;
  if (create_shader("screen fragment shader", GL_FRAGMENT_SHADER, fshader_blit_src, &blit_fshader) < 0) return -1;

  int status;
  blit_program = glCreateProgram();
  glAttachShader(blit_program, blit_vshader);
  glAttachShader(blit_program, blit_fshader);
  glLinkProgram(blit_program);
  glGetProgramiv(blit_program, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    glGetProgramInfoLog(blit_program, LOGBUFFER_SIZE, NULL, logbuffer);
    fprintf(stderr, "Render: Failed to compile screen shader program.\n\tDetails: %s\n", logbuffer);
    return -1;
  }
  blit_uniform_image  = glGetUniformLocation(blit_program, "image");
  blit_uniform_size   = glGetUniformLocation(blit_program, "size");
  blit_uniform_offset = glGetUniformLocation(blit_program, "offset");

  return 0;
}

static int init_lighting(int w, int h) {
  int status;

  glGenTextures(1, &tex_lighting);
  glBindTexture(GL_TEXTURE_2D, tex_lighting);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenFramebuffers(1, &fbo_lighting);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_lighting);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_lighting, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_depth);
  status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "Render: HDR lighting framebuffer is incomplete.\n\tDetails: %x\n", status);
    return -1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  /* point lighting */
  if (create_shader("point lighting vertex shader", GL_VERTEX_SHADER, vshader_pointlight_src, &pointlight_vshader) < 0) return -1;
  if (create_shader("point lighting fragment shader", GL_FRAGMENT_SHADER, fshader_pointlight_src, &pointlight_fshader) < 0) return -1;

  pointlight_program = glCreateProgram();
  glAttachShader(pointlight_program, pointlight_vshader);
  glAttachShader(pointlight_program, pointlight_fshader);
  glLinkProgram(pointlight_program);
  glGetProgramiv(pointlight_program, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    glGetProgramInfoLog(pointlight_program, LOGBUFFER_SIZE, NULL, logbuffer);
    fprintf(stderr, "Render: Failed to compile point lighting shader program.\n\tDetails: %s\n", logbuffer);
    return -1;
  }

  pointlight_uniform_pointlight = glGetUniformBlockIndex(pointlight_program, "pointlight");
  pointlight_uniform_vmatrix    = glGetUniformLocation(pointlight_program, "vmatrix");
  pointlight_uniform_vpmatrix   = glGetUniformLocation(pointlight_program, "vpmatrix");
  pointlight_uniform_ivpmatrix  = glGetUniformLocation(pointlight_program, "ivpmatrix");
  pointlight_uniform_tdepth     = glGetUniformLocation(pointlight_program, "tdepth");
  pointlight_uniform_tdiffuse   = glGetUniformLocation(pointlight_program, "tdiffuse");
  pointlight_uniform_tnormal    = glGetUniformLocation(pointlight_program, "tnormal");
  pointlight_uniform_tspecular  = glGetUniformLocation(pointlight_program, "tspecular");
  //pointlight_uniform_temissive  = glGetUniformLocation(pointlight_program, "temissive");

  return 0;
}

static int init_tonemap() {
  if (create_shader("tonemapping vertex shader", GL_VERTEX_SHADER, vshader_tonemap_src, &tonemap_vshader) < 0) return -1;
  if (create_shader("tonemapping fragment shader", GL_FRAGMENT_SHADER, fshader_tonemap_src, &tonemap_fshader) < 0) return -1;

  int status;
  tonemap_program = glCreateProgram();
  glAttachShader(tonemap_program, tonemap_vshader);
  glAttachShader(tonemap_program, tonemap_fshader);
  glLinkProgram(tonemap_program);
  glGetProgramiv(tonemap_program, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    glGetProgramInfoLog(tonemap_program, LOGBUFFER_SIZE, NULL, logbuffer);
    fprintf(stderr, "Render: Failed to compile tonemapping shader program.\n\tDetails: %s\n", logbuffer);
    return -1;
  }
  tonemap_uniform_tcomposite = glGetUniformLocation(tonemap_program, "tcomposite");

  return 0;
}

static void blit_to_screen(unsigned int texture, float w, float h, float x, float y) {
  glUseProgram(blit_program);

  glUniform1i(blit_uniform_image, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);

  glUniform2f(blit_uniform_size, w, h);
  glUniform2f(blit_uniform_offset, x, y);

  glBindVertexArray(vao_rect);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0); 

  glBindVertexArray(0);

  glUseProgram(0);
}
/* vim: set ts=2 sw=2 et */
