#include "renderer.h"

#include "model.h"
#include "vid.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#define GL3_PROTOTYPES
#include <GL3/gl3.h>

static const char *vshader_gbuffer_src =
"#version 330\n"
"layout(std140) uniform scene {\n"
"  mat4 mvmatrix;\n"
"  mat4 mvpmatrix;\n"
"};\n"
"in vec3 vposition;\n"
"in vec2 vtexcoord;\n"
"in vec3 vnormal;\n"
"in vec4 vtangent;\n"
"smooth out float fdepth;\n"
"smooth out vec2 ftexcoord;\n"
"smooth out mat3 tbnmatrix;\n"
"void main() {\n"
"  fdepth    = -(mvmatrix * vec4(vposition, 1)).z;\n"
"  ftexcoord = vtexcoord;\n"
"\n"
"  vec3 vbitangent = cross(vnormal, vtangent.xyz) * vtangent.w;\n"
"  tbnmatrix = mat3(mvmatrix[0].xyz, mvmatrix[1].xyz, mvmatrix[2].xyz) * mat3(vtangent.xyz, vbitangent, vnormal);\n"
"\n"
"  gl_Position = mvpmatrix * vec4(vposition, 1);\n"
"}\n";

static const char *fshader_gbuffer_src =
"#version 330\n"
"uniform sampler2D tdiffuse;\n"
"uniform sampler2D tnormal;\n"
"uniform sampler2D tspecular;\n"
"smooth in float fdepth;\n"
"smooth in vec2  ftexcoord;\n"
"smooth in mat3  tbnmatrix;\n"
"out float gdepth;\n"
"out vec4  gdiffuse;\n"
"out vec2  gnormal;\n"
"out vec4  gspecular;\n"
"out vec4  gemissive;\n"
"void main() {\n"
"  vec3 n_local = texture2D(tnormal, ftexcoord).xyz * 2 - 1;\n"
"  vec3 n_world = normalize(tbnmatrix * n_local);\n"
"\n"
"  gdepth    = fdepth/100.0;\n"
"  gdiffuse  = texture2D(tdiffuse, ftexcoord);\n"
"  gnormal   = n_world.xy;\n"
"  gspecular = texture2D(tspecular, ftexcoord);\n"
"  gemissive = vec4(0.0, 0.0, 0.0, 1.0);\n"
"}\n";

static const char *vshader_composite_src = 
"#version 330\n"
"in vec2 vcoord;\n"
"smooth out vec2 ftexcoord;\n"
"void main () {\n"
"  ftexcoord = vcoord;\n"
"  gl_Position = vec4(vcoord, 0.0, 1.0);\n"
"}\n";

static const char *fshader_composite_src =
"#version 330\n"
"uniform sampler2D tdepth;\n"
"uniform sampler2D tdiffuse;\n"
"uniform sampler2D tnormal;\n"
"uniform sampler2D tspecular;\n"
"uniform sampler2D temissive;\n"
"smooth in vec2 ftexcoord;\n"
"out vec4 color;\n"
"void main () {\n"
""  
"}\n";

static const char *vshader_blit_src =
"#version 330\n"
"uniform vec2 size;\n"
"uniform vec2 offset;\n"
"in vec2 vcoord;\n"
"smooth out vec2 ftexcoord;\n"
"void main () {\n"
"  ftexcoord = vcoord;\n"
"  gl_Position = vec4(vcoord * size + offset, 0.0, 1.0);\n"
"}\n";

static const char *fshader_blit_src = 
"#version 330\n"
"uniform sampler2D image;\n"
"smooth in vec2 ftexcoord;\n"
"out vec4 color;\n"
"void main() {\n"
"  color = texture2D(image, ftexcoord);\n"
"}\n";

typedef struct gbuffer {
  unsigned int fshader;
  unsigned int vshader;
  unsigned int program;
  int uniform_scene;
  int uniform_tdiffuse;
  int uniform_tnormal;
  int uniform_tspecular;
  unsigned int tex_depth;
  unsigned int tex_diffuse;
  unsigned int tex_normal;
  unsigned int tex_specular;
  unsigned int tex_emissive;
  unsigned int rbo_depth;
  unsigned int fbo_gbuffer;
} gbuffer_t;
static gbuffer_t gbuffer_info;

typedef struct blit {
  unsigned int fshader;
  unsigned int vshader;
  unsigned int program;
  int uniform_image;
  int uniform_size;
  int uniform_offset;
  int vbo_coords;
  int vbo_tris;
  int vao;
} blit_t;
static blit_t blit_info;

typedef struct uniform_scene {
  kl_mat4f_t mv;
  kl_mat4f_t mvp;
} uniform_scene_t;
static unsigned int ubo_scene = 0;

#define LOGBUFFER_SIZE 0x4000
static char logbuffer[LOGBUFFER_SIZE];

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

static int init_gbuffer(gbuffer_t *g, int w, int h) {
  int status;

  /* g-buffer format: */
  /* depth: 32f */
  /* diffuse: 8 red, 8 blue, 8 green, 8 unused */
  /* normal: 16f x, 16f y (z reconstructed) */
  /* specular: 8 red, 8 blue, 8 green, 8 specular exponent */
  /* emissive: 8 red, 8 blue, 8 green, 8 intensity exponent */
  
  glGenTextures(1, &g->tex_depth);
  glBindTexture(GL_TEXTURE_2D, g->tex_depth);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, w, h, 0, GL_RED, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);

  glGenTextures(1, &g->tex_diffuse);
  glBindTexture(GL_TEXTURE_2D, g->tex_diffuse);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  glGenTextures(1, &g->tex_normal);
  glBindTexture(GL_TEXTURE_2D, g->tex_normal);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, w, h, 0, GL_RG, GL_UNSIGNED_SHORT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  glGenTextures(1, &g->tex_specular);
  glBindTexture(GL_TEXTURE_2D, g->tex_specular);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  glGenTextures(1, &g->tex_emissive);
  glBindTexture(GL_TEXTURE_2D, g->tex_emissive);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  glBindTexture(GL_TEXTURE_2D, 0);

  glGenRenderbuffers(1, &g->rbo_depth);
  glBindRenderbuffer(GL_RENDERBUFFER, g->rbo_depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, w, h);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  
  glGenFramebuffers(1, &g->fbo_gbuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, g->fbo_gbuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g->tex_depth, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g->tex_diffuse, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, g->tex_normal, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, g->tex_specular, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, g->tex_emissive, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g->rbo_depth);
  status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "Render: G-buffer is incomplete.\n\tDetails: %x\n", status);
    return -1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  if (create_shader("g-buffer vertex shader", GL_VERTEX_SHADER, vshader_gbuffer_src, &g->vshader) < 0) return -1;
  if (create_shader("g-buffer fragment shader", GL_FRAGMENT_SHADER, fshader_gbuffer_src, &g->fshader) < 0) return -1;

  g->program = glCreateProgram();
  glAttachShader(g->program, g->vshader);
  glAttachShader(g->program, g->fshader);
  glBindAttribLocation(g->program, 0, "vposition");
  glBindAttribLocation(g->program, 1, "vtexcoord");
  glBindAttribLocation(g->program, 2, "vnormal");
  glBindAttribLocation(g->program, 3, "vtangent");
  glBindAttribLocation(g->program, 4, "vblendidx");
  glBindAttribLocation(g->program, 5, "vblendwt");
  glBindFragDataLocation(g->program, 0, "gdepth");
  glBindFragDataLocation(g->program, 1, "gdiffuse");
  glBindFragDataLocation(g->program, 2, "gnormal");
  glBindFragDataLocation(g->program, 3, "gspecular");
  glBindFragDataLocation(g->program, 4, "gemissive");
  glLinkProgram(g->program);
  glGetProgramiv(g->program, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    glGetProgramInfoLog(g->program, LOGBUFFER_SIZE, NULL, logbuffer);
    fprintf(stderr, "Render: Failed to compile gbuffer shader program.\n\tDetails: %s\n", logbuffer);
    return -1;
  }

  g->uniform_scene     = glGetUniformBlockIndex(g->program, "scene");
  g->uniform_tdiffuse  = glGetUniformLocation(g->program, "tdiffuse");
  g->uniform_tnormal   = glGetUniformLocation(g->program, "tnormal");
  g->uniform_tspecular = glGetUniformLocation(g->program, "tspecular");
  return 0;
}

static int init_blit(blit_t *b) {
  /* screen blitting shader program */
  if (create_shader("screen vertex shader", GL_VERTEX_SHADER, vshader_blit_src, &b->vshader) < 0) return -1;
  if (create_shader("screen fragment shader", GL_FRAGMENT_SHADER, fshader_blit_src, &b->fshader) < 0) return -1;

  int status;
  b->program = glCreateProgram();
  glAttachShader(b->program, b->vshader);
  glAttachShader(b->program, b->fshader);
  glBindAttribLocation(b->program, 0, "vcoord");
  glLinkProgram(b->program);
  glGetProgramiv(b->program, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    glGetProgramInfoLog(b->program, LOGBUFFER_SIZE, NULL, logbuffer);
    fprintf(stderr, "Render: Failed to compile screen shader program.\n\tDetails: %s\n", logbuffer);
    return -1;
  }
  b->uniform_image  = glGetUniformLocation(b->program, "image");
  b->uniform_size   = glGetUniformLocation(b->program, "size");
  b->uniform_offset = glGetUniformLocation(b->program, "offset");

  /* create screen blitting VBO/VAO */
  float verts_rect[8] = {
    0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f
  };
  unsigned int tris_rect[6] = { 0, 1, 2, 0, 2, 3 };
  b->vbo_coords = kl_render_upload_vertdata(verts_rect, 8*sizeof(float));
  b->vbo_tris   = kl_render_upload_tris(tris_rect, 6*sizeof(unsigned int));

  kl_render_attrib_t attrib = {
    .index  = 0,
    .size   = 2,
    .type   = KL_RENDER_FLOAT,
    .buffer = b->vbo_coords
  };
  b->vao = kl_render_define_attribs(b->vbo_tris, &attrib, 1);
  return 0;
}

static void blit_to_screen(unsigned int texture, float w, float h, float x, float y) {
  glUseProgram(blit_info.program);

  glUniform1i(blit_info.uniform_image, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);

  glUniform2f(blit_info.uniform_size, w, h);
  glUniform2f(blit_info.uniform_offset, x, y);

  glBindVertexArray(blit_info.vao);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0); 

  glBindVertexArray(0);

  glUseProgram(0);
}


int kl_render_init() {
  int w, h;
  kl_vid_size(&w, &h);

  glGenBuffers(1, (unsigned int*)&ubo_scene);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glDisable(GL_CULL_FACE);

  if (init_gbuffer(&gbuffer_info, w, h) < 0) return -1;
  if (init_blit(&blit_info) < 0) return -1;

  return 0;
}

void kl_render_draw(kl_camera_t *cam, kl_model_t *model) {
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gbuffer_info.fbo_gbuffer);
  unsigned int attachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4};
  glDrawBuffers(5, attachments);

  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  kl_mat4f_t view, proj;
  kl_camera_view(cam, &view);
  kl_camera_proj(cam, &proj);
  uniform_scene_t scene;
  scene.mv = view;
  kl_mat4f_mul(&scene.mvp, &proj, &view);
  glBindBuffer(GL_UNIFORM_BUFFER, ubo_scene);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(uniform_scene_t), &scene, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);

  glUseProgram(gbuffer_info.program);
  glBindBufferBase(GL_UNIFORM_BUFFER, gbuffer_info.uniform_scene, ubo_scene);
  
  glUniform1i(gbuffer_info.uniform_tdiffuse, 0);
  glUniform1i(gbuffer_info.uniform_tnormal, 1);
  glUniform1i(gbuffer_info.uniform_tspecular, 2);

  glBindVertexArray(model->attribs);
  for (int i=0; i < model->mesh_n; i++) {
    kl_mesh_t *mesh = model->mesh + i;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mesh->material.diffuse->id);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, mesh->material.normal->id);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, mesh->material.specular->id);

    glDrawElements(GL_TRIANGLES, 3*mesh->tris_n, GL_UNSIGNED_INT, (void*)(3*mesh->tris_i*sizeof(int)));
  }
  glBindVertexArray(0);

  glBindBufferBase(GL_UNIFORM_BUFFER, gbuffer_info.uniform_scene, 0);
  glUseProgram(0);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}


void kl_render_composite() {
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  blit_to_screen(gbuffer_info.tex_depth,    0.4, 0.4, -1.0, -1.0);
  blit_to_screen(gbuffer_info.tex_diffuse,  0.4, 0.4, -0.6, -1.0);
  blit_to_screen(gbuffer_info.tex_normal,   0.4, 0.4, -0.2, -1.0);
  blit_to_screen(gbuffer_info.tex_specular, 0.4, 0.4,  0.2, -1.0);
  blit_to_screen(gbuffer_info.tex_emissive, 0.4, 0.4,  0.6, -1.0);
}

unsigned int kl_render_upload_vertdata(void *data, int n) {
  unsigned int vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, n, data, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  return vbo;
}

unsigned int kl_render_upload_tris(unsigned int *data, int n) {
  unsigned int ebo;
  glGenBuffers(1, &ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, n, data, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  return ebo;
}

unsigned int kl_render_upload_texture(void *data, int w, int h, int format, int type) {
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
  glTexImage2D(GL_TEXTURE_2D, 0, glformat, w, h, 0, glformat, gltype, data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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

  glBindTexture(GL_TEXTURE_2D, 0);

  return texture;
}

void kl_render_free_texture(unsigned int texture) {
  glDeleteTextures(1, &texture);
}

unsigned int kl_render_define_attribs(int tris, kl_render_attrib_t *cfg, int n) {
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
/* vim: set ts=2 sw=2 et */
