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
"uniform sampler2D temissive;\n"
"smooth in float fdepth;\n"
"smooth in vec2  ftexcoord;\n"
"smooth in mat3  tbnmatrix;\n"
"out float gdepth;\n"
"out vec4  gdiffuse;\n"
"out vec2  gnormal;\n"
"out vec4  gspecular;\n"
"out vec4  gemissive;\n"
"void main() {\n"
"  vec3 n_local = texture(tnormal, ftexcoord).xyz * 2.0 - 1.0;\n"
"  vec3 n_world = normalize(tbnmatrix * n_local);\n"
"\n"
"  gdepth    = fdepth;\n"
"  gdiffuse  = texture(tdiffuse, ftexcoord);\n"
"  gnormal   = n_world.xy;\n"
"  gspecular = texture(tspecular, ftexcoord);\n"
"  gemissive = texture(temissive, ftexcoord);\n"
"}\n";

static const char *vshader_composite_src = 
"#version 330\n"
"in vec2 vcoord;\n"
"smooth out vec2 ftexcoord;\n"
"void main () {\n"
"  ftexcoord = vcoord;\n"
"  gl_Position = vec4(vcoord * 2.0 - 1.0, 0.0, 1.0);\n"
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
"  float depth = texture(tdepth, ftexcoord).r;\n"
"  float luminance = 1000.0/(depth*depth);\n"
"  vec3 norm;\n"
"  norm.xy = texture(tnormal, ftexcoord).xy;\n"
"  norm.z  = 1.0 - dot(norm.xy, norm.xy);\n"
"  vec3 diff = texture(tdiffuse, ftexcoord).rgb;\n"
"  vec4 spec = texture(tspecular, ftexcoord);\n"
"  vec4 glow = texture(temissive, ftexcoord);\n"
"  color.rgb = luminance * diff * dot(norm, vec3(0.0, 0.0, 1.0)) + luminance * spec.rgb * pow(dot(norm, vec3(0.0, 0.0, 1.0)), spec.a*255.0) + glow.rgb * exp2(glow.a * 16.0 - 8.0);\n"
"  color.a   = 1.0;\n"
"}\n";

static const char *vshader_tonemap_src = 
"#version 330\n"
"in vec2 vcoord;\n"
"smooth out vec2 ftexcoord;\n"
"void main () {\n"
"  ftexcoord = vcoord;\n"
"  gl_Position = vec4(vcoord * 2.0 - 1.0, 0.0, 1.0);\n"
"}\n";

static const char *fshader_tonemap_src = 
"#version 330\n"
"uniform sampler2D tcomposite;\n"
"smooth in vec2 ftexcoord;\n"
"out vec4 color;\n"
"void main() {\n"
"  vec3 c = texture(tcomposite, ftexcoord).rgb;\n"
"  color = vec4(1.0 / (1.0 + exp(-8.0 * pow(c, vec3(0.82)) + 4.0)) - 0.018, 1.0);\n"
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
"  color = texture(image, ftexcoord);\n"
"}\n";

static unsigned int gbuffer_fshader;
static unsigned int gbuffer_vshader;
static unsigned int gbuffer_program;
static int gbuffer_uniform_scene;
static int gbuffer_uniform_tdiffuse;
static int gbuffer_uniform_tnormal;
static int gbuffer_uniform_tspecular;
static int gbuffer_uniform_temissive;
static unsigned int gbuffer_tex_depth;
static unsigned int gbuffer_tex_diffuse;
static unsigned int gbuffer_tex_normal;
static unsigned int gbuffer_tex_specular;
static unsigned int gbuffer_tex_emissive;
static unsigned int gbuffer_rbo_depth;
static unsigned int gbuffer_fbo;

static unsigned int composite_fshader;
static unsigned int composite_vshader;
static unsigned int composite_program;
static int composite_uniform_tdepth;
static int composite_uniform_tdiffuse;
static int composite_uniform_tnormal;
static int composite_uniform_tspecular;
static int composite_uniform_temissive;
static unsigned int composite_tex_color;
static unsigned int composite_fbo;

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

  glGenRenderbuffers(1, &gbuffer_rbo_depth);
  glBindRenderbuffer(GL_RENDERBUFFER, gbuffer_rbo_depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, w, h);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  
  glGenFramebuffers(1, &gbuffer_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, gbuffer_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gbuffer_tex_depth, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gbuffer_tex_diffuse, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gbuffer_tex_normal, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, gbuffer_tex_specular, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, gbuffer_tex_emissive, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gbuffer_rbo_depth);
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
  glBindAttribLocation(gbuffer_program, 0, "vposition");
  glBindAttribLocation(gbuffer_program, 1, "vtexcoord");
  glBindAttribLocation(gbuffer_program, 2, "vnormal");
  glBindAttribLocation(gbuffer_program, 3, "vtangent");
  glBindAttribLocation(gbuffer_program, 4, "vblendidx");
  glBindAttribLocation(gbuffer_program, 5, "vblendwt");
  glBindFragDataLocation(gbuffer_program, 0, "gdepth");
  glBindFragDataLocation(gbuffer_program, 1, "gdiffuse");
  glBindFragDataLocation(gbuffer_program, 2, "gnormal");
  glBindFragDataLocation(gbuffer_program, 3, "gspecular");
  glBindFragDataLocation(gbuffer_program, 4, "gemissive");
  glLinkProgram(gbuffer_program);
  glGetProgramiv(gbuffer_program, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    glGetProgramInfoLog(gbuffer_program, LOGBUFFER_SIZE, NULL, logbuffer);
    fprintf(stderr, "Render: Failed to compile gbuffer shader program.\n\tDetails: %s\n", logbuffer);
    return -1;
  }

  gbuffer_uniform_scene     = glGetUniformBlockIndex(gbuffer_program, "scene");
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
  glBindAttribLocation(blit_program, 0, "vcoord");
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

static int init_composite(int w, int h) {
  int status;

  glGenTextures(1, &composite_tex_color);
  glBindTexture(GL_TEXTURE_2D, composite_tex_color);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenFramebuffers(1, &composite_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, composite_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, composite_tex_color, 0);
  status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "Render: Composite HDR framebuffer is incomplete.\n\tDetails: %x\n", status);
    return -1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  if (create_shader("compositing vertex shader", GL_VERTEX_SHADER, vshader_composite_src, &composite_vshader) < 0) return -1;
  if (create_shader("compositing fragment shader", GL_FRAGMENT_SHADER, fshader_composite_src, &composite_fshader) < 0) return -1;

  composite_program = glCreateProgram();
  glAttachShader(composite_program, composite_vshader);
  glAttachShader(composite_program, composite_fshader);
  glBindAttribLocation(composite_program, 0, "vcoord");
  glBindFragDataLocation(composite_program, 0, "color");
  glLinkProgram(composite_program);
  glGetProgramiv(composite_program, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    glGetProgramInfoLog(composite_program, LOGBUFFER_SIZE, NULL, logbuffer);
    fprintf(stderr, "Render: Failed to compile compositing shader program.\n\tDetails: %s\n", logbuffer);
    return -1;
  }
  composite_uniform_tdepth    = glGetUniformLocation(composite_program, "tdepth");
  composite_uniform_tdiffuse  = glGetUniformLocation(composite_program, "tdiffuse");
  composite_uniform_tnormal   = glGetUniformLocation(composite_program, "tnormal");
  composite_uniform_tspecular = glGetUniformLocation(composite_program, "tspecular");
  composite_uniform_temissive = glGetUniformLocation(composite_program, "temissive");

  return 0;
}

static int init_tonemap() {
  if (create_shader("tonemapping vertex shader", GL_VERTEX_SHADER, vshader_tonemap_src, &tonemap_vshader) < 0) return -1;
  if (create_shader("tonemapping fragment shader", GL_FRAGMENT_SHADER, fshader_tonemap_src, &tonemap_fshader) < 0) return -1;

  int status;
  tonemap_program = glCreateProgram();
  glAttachShader(tonemap_program, tonemap_vshader);
  glAttachShader(tonemap_program, tonemap_fshader);
  glBindAttribLocation(tonemap_program, 0, "vcoord");
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


int kl_render_init() {
  int w, h;
  kl_vid_size(&w, &h);

  glGenBuffers(1, (unsigned int*)&ubo_scene);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  //glEnable(GL_CULL_FACE);
  //glFrontFace(GL_CCW);
  //glCullFace(GL_BACK);

  if (init_gbuffer(w, h) < 0) return -1;
  if (init_composite(w, h) < 0) return -1;
  if (init_tonemap() < 0) return -1;
  if (init_blit() < 0) return -1;

  /* create screen blitting VBO/VAO */
  float verts_rect[8] = {
    0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f
  };
  unsigned int tris_rect[6] = { 0, 1, 2, 0, 2, 3 };
  vbo_rect_coords = kl_render_upload_vertdata(verts_rect, 8*sizeof(float));
  vbo_rect_tris   = kl_render_upload_tris(tris_rect, 6*sizeof(unsigned int));

  kl_render_attrib_t attrib = {
    .index  = 0,
    .size   = 2,
    .type   = KL_RENDER_FLOAT,
    .buffer = vbo_rect_coords
  };
  vao_rect = kl_render_define_attribs(vbo_rect_tris, &attrib, 1);

  return 0;
}

void kl_render_draw(kl_camera_t *cam, kl_model_t *model) {
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gbuffer_fbo);
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

  glUseProgram(gbuffer_program);
  glBindBufferBase(GL_UNIFORM_BUFFER, gbuffer_uniform_scene, ubo_scene);
  
  glUniform1i(gbuffer_uniform_tdiffuse, 0);
  glUniform1i(gbuffer_uniform_tnormal, 1);
  glUniform1i(gbuffer_uniform_tspecular, 2);
  glUniform1i(gbuffer_uniform_temissive, 3);

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

  glBindBufferBase(GL_UNIFORM_BUFFER, gbuffer_uniform_scene, 0);
  glUseProgram(0);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}


void kl_render_composite() {
  glDisable(GL_DEPTH_TEST);

  /* draw HDR composite buffer */
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, composite_fbo);
  unsigned int attachments[] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffers(1, attachments);

  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  glUseProgram(composite_program);

  glUniform1i(composite_uniform_tdepth, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, gbuffer_tex_depth);

  glUniform1i(composite_uniform_tdiffuse, 1);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, gbuffer_tex_diffuse);

  glUniform1i(composite_uniform_tnormal, 2);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, gbuffer_tex_normal);

  glUniform1i(composite_uniform_tspecular, 3);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, gbuffer_tex_specular);

  glUniform1i(composite_uniform_temissive, 4);
  glActiveTexture(GL_TEXTURE4);
  glBindTexture(GL_TEXTURE_2D, gbuffer_tex_emissive);

  glBindVertexArray(vao_rect);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0); 

  glBindVertexArray(0);

  glUseProgram(0);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

  /* apply tonemapping */
  glUseProgram(tonemap_program);

  glUniform1i(tonemap_uniform_tcomposite, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, composite_tex_color);

  glBindVertexArray(vao_rect);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0); 

  glBindVertexArray(0);

  glUseProgram(0);

  /* blit debug images */
  blit_to_screen(gbuffer_tex_depth,    0.4, 0.4, -1.0, -1.0);
  blit_to_screen(gbuffer_tex_diffuse,  0.4, 0.4, -0.6, -1.0);
  blit_to_screen(gbuffer_tex_normal,   0.4, 0.4, -0.2, -1.0);
  blit_to_screen(gbuffer_tex_specular, 0.4, 0.4,  0.2, -1.0);
  blit_to_screen(gbuffer_tex_emissive, 0.4, 0.4,  0.6, -1.0);

  glEnable(GL_DEPTH_TEST);
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
