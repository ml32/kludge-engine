#include "renderer.h"

#include "model.h"
#include "vid.h"

#include <stdio.h>
#include <string.h>
#define GL3_PROTOTYPES
#include <GL3/gl3.h>

kl_mat4f_t kl_render_mat_view = {
  .cell = {
     0.0f,  0.0f,  1.0f, 0.0f,
     1.0f,  0.0f,  0.0f, 0.0f,
     0.0f,  1.0f,  0.0f, 0.0f,
     0.0f,  0.0f,  0.0f, 1.0f
  }
};
kl_mat4f_t kl_render_mat_proj = KL_MAT4F_IDENTITY;

static const char *vshader_default_src =
"#version 330\n"
"layout(std140) uniform scene {\n"
"  mat4 mvmatrix;\n"
"  mat4 mvpmatrix;\n"
"};\n"
"in vec3 vposition;\n"
"in vec2 vtexcoord;\n"
"in vec3 vnormal;\n"
"in vec4 vtangent;\n"
"smooth out vec2 ftexcoord;\n"
"smooth out mat3 tbnmatrix;\n"
"void main() {\n"
"  ftexcoord = vtexcoord;\n"
"\n"
"  vec3 vbitangent = cross(vnormal, vtangent.xyz) * vtangent.w;\n"
"  tbnmatrix = mat3(vtangent.xyz, vbitangent, vnormal);\n"
"\n"
"  gl_Position = mvpmatrix * vec4(vposition, 1);\n"
"}\n";

static const char *fshader_default_src =
"#version 330\n"
"uniform sampler2D diffuse;\n"
"uniform sampler2D normal;\n"
"uniform sampler2D specular;\n"
"smooth in vec2 ftexcoord;\n"
"smooth in mat3 tbnmatrix;\n"
"out vec4 color;\n"
"void main() {\n"
"  vec3 n_local = texture2D(normal, ftexcoord).xyz * 2 - 1;\n"
"  vec3 n_world = normalize(tbnmatrix * n_local);\n"
"  vec4 d = texture2D(diffuse, ftexcoord);\n"
"  color = d * dot(n_world, normalize(vec3(-2, -1, 1)));\n"
"}\n";

static int fshader_default = 0;
static int vshader_default = 0;
static int program_default = 0;
static int program_default_scene    = 0;
static int program_default_diffuse  = 0;
static int program_default_normal   = 0;
static int program_default_specular = 0;
static int ubo_scene    = 0;

typedef struct uniform_scene {
  kl_mat4f_t mv;
  kl_mat4f_t mvp;
} uniform_scene_t;

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

int kl_render_init() {
  int w, h;
  kl_vid_size(&w, &h);

  float ratio = (float)w/(float)h;
  kl_mat4f_ortho(&kl_render_mat_proj, -50.0f*ratio, 50.0f*ratio, -50.0f, 50.0f, 50.0f, -50.0f);

  int status, len;

  len = strlen(vshader_default_src);
  vshader_default = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vshader_default, 1, &vshader_default_src, &len);
  glCompileShader(vshader_default);
  glGetShaderiv(vshader_default, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    glGetShaderInfoLog(vshader_default, LOGBUFFER_SIZE, NULL, logbuffer);
    fprintf(stderr, "Render: Failed to compile default vertex shader.\n\tDetails: %s\n", logbuffer);
    return -1;
  }

  len = strlen(fshader_default_src);
  fshader_default = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fshader_default, 1, &fshader_default_src, &len);;
  glCompileShader(fshader_default);
  glGetShaderiv(fshader_default, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    glGetShaderInfoLog(fshader_default, LOGBUFFER_SIZE, NULL, logbuffer);
    fprintf(stderr, "Render: Failed to compile default fragment shader.\n\tDetails: %s\n", logbuffer);
    return -1;
  }

  program_default = glCreateProgram();
  glAttachShader(program_default, vshader_default);
  glAttachShader(program_default, fshader_default);
  glBindAttribLocation(program_default, 0, "vposition");
  glBindAttribLocation(program_default, 1, "vtexcoord");
  glBindAttribLocation(program_default, 2, "vnormal");
  glBindAttribLocation(program_default, 3, "vtangent");
  glBindAttribLocation(program_default, 4, "vblendidx");
  glBindAttribLocation(program_default, 5, "vblendwt");
  glLinkProgram(program_default);
  glGetProgramiv(program_default, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    glGetProgramInfoLog(program_default, LOGBUFFER_SIZE, NULL, logbuffer);
    fprintf(stderr, "Render: Failed to compile default shader program.\n\tDetails: %s\n", logbuffer);
    return -1;
  }

  program_default_scene    = glGetUniformBlockIndex(program_default, "scene");
  program_default_diffuse  = glGetUniformLocation(program_default, "diffuse");
  program_default_normal   = glGetUniformLocation(program_default, "normal");
  program_default_specular = glGetUniformLocation(program_default, "specular");

  glGenBuffers(1, (unsigned int*)&ubo_scene);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  return 0;
}

void kl_render_draw(kl_model_t *model) {
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  uniform_scene_t scene;
  scene.mv = kl_render_mat_view;
  kl_mat4f_mul(&scene.mvp, &kl_render_mat_proj, &kl_render_mat_view);
  glBindBuffer(GL_UNIFORM_BUFFER, ubo_scene);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(uniform_scene_t), &scene, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);

  glUseProgram(program_default);
  glBindBufferBase(GL_UNIFORM_BUFFER, program_default_scene, ubo_scene);
  
  glUniform1i(program_default_diffuse, 0);
  glUniform1i(program_default_normal, 1);
  glUniform1i(program_default_specular, 2);

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

  glBindBufferBase(GL_UNIFORM_BUFFER, program_default_scene, 0);
  glUseProgram(0);
}

unsigned int kl_render_upload_vertdata(void *data, int n) {
  unsigned int vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, n, data, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  return vbo;
}

unsigned int kl_render_upload_tris(int *data, int n) {
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
