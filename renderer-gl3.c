#include "renderer.h"

#include "model.h"
#include "vid.h"

#include <stdio.h>
#include <string.h>
#define GL3_PROTOTYPES
#include <GL3/gl3.h>

kl_mat4f_t kl_render_mat_view = {
  .cell = {
     0.0f, 0.0f, -1.0f, 0.0f,
    -1.0f, 0.0f,  0.0f, 0.0f,
     0.0f, 1.0f,  0.0f, 0.0f,
     0.0f, 0.0f,  0.0f, 1.0f
  }
};
kl_mat4f_t kl_render_mat_proj = KL_MAT4F_IDENTITY;

static const char *vshader_default_src =
"#version 330\n"
"layout(std140) uniform scene {\n"
"  uniform mat4 mvmatrix;\n"
"  uniform mat4 mvpmatrix;\n"
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
"  tbnmatrix = mat3(mvmatrix[0].xyz, mvmatrix[1].xyz, mvmatrix[2].xyz) * mat3(vtangent.xyz, vbitangent, vnormal);\n"
"\n"
"  gl_Position = mvpmatrix * vec4(vposition, 1);\n"
"}\n";

static const char *fshader_default_src =
"#version 330\n"
"smooth in vec2 ftexcoord;\n"
"smooth in mat3 tbnmatrix;\n"
"out vec4 color;\n"
"void main() {\n"
"  color.rgb = (tbnmatrix * vec3(0, 0, 1) + 1)/2;\n"
"  color.b   = 1.0-color.b;\n"
"  color.a   = 1;\n"
"}\n";

static int fshader_default = 0;
static int vshader_default = 0;
static int program_default = 0;
static int program_default_scene = 0;
static int ubo_scene = 0;

typedef struct uniform_scene {
  kl_mat4f_t mv;
  kl_mat4f_t mvp;
} uniform_scene_t;

#define LOGBUFFER_SIZE 0x4000
static char logbuffer[LOGBUFFER_SIZE];

int kl_render_init() {
  int w, h;
  kl_vid_size(&w, &h);

  float ratio = (float)w/(float)h;
  kl_mat4f_ortho(&kl_render_mat_proj, -10.0f*ratio, 10.0f*ratio, -10.0f, 10.0f, 10.0f, -10.0f);

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

  program_default_scene = glGetUniformBlockIndex(program_default, "scene");

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

  glBindVertexArray(model->attribs);
  for (int i=0; i < model->mesh_n; i++) {
    kl_mesh_t *mesh = model->mesh + i;
    glDrawElements(GL_TRIANGLES, 3*mesh->tris_n, GL_UNSIGNED_INT, (void*)(3*mesh->tris_i*sizeof(int)));
  }
  glBindVertexArray(0);

  glBindBufferBase(GL_UNIFORM_BUFFER, program_default_scene, 0);
  glUseProgram(0);
}

int kl_render_upload_vertdata(void *data, int n) {
  int vbo;
  glGenBuffers(1, (unsigned int*)&vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, n, data, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  return vbo;
}

int kl_render_upload_tris(int *data, int n) {
  int ebo;
  glGenBuffers(1, (unsigned int*)&ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, n, data, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  return ebo;
}

static int convertenum(int value) {
  switch (value) {
    case KL_RENDER_FLOAT:
      return GL_FLOAT;
    case KL_RENDER_UINT8:
      return GL_UNSIGNED_BYTE;
  }
  return KL_RENDER_FLOAT;
}

int kl_render_define_attribs(int tris, kl_render_attrib_t *cfg, int n) {
  int vao;
  glGenVertexArrays(1, (unsigned int*)&vao);
  glBindVertexArray(vao);
  for (int i=0; i < n; i++) {
    kl_render_attrib_t *c = cfg + i;
    if (c->index < 0 || c->buffer < 0) continue;
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
