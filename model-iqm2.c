#include "model-iqm2.h"

#include "renderer.h"

#include <stdio.h>
#include <stdlib.h>

#define IQM_VERSION 2

#define IQM_POSITION 0x00
#define IQM_TEXCOORD 0x01
#define IQM_NORMAL   0x02
#define IQM_TANGENT  0x03
#define IQM_BLENDIDX 0x04
#define IQM_BLENDWT  0x05
#define IQM_COLOR    0x06
#define IQM_CUSTOM   0x10

#define IQM_INT8     0x00
#define IQM_UINT8    0x01
#define IQM_INT16    0x02
#define IQM_UINT16   0x03
#define IQM_INT32    0x04
#define IQM_UINT32   0x05
#define IQM_HALF     0x06
#define IQM_FLOAT    0x07
#define IQM_DOUBLE   0x08

/* "INTERQUAKEMODEL" as little-endian integers */
static const uint32_t iqm_magic[4] = {
  0x45544e49,
  0x41555152,
  0x4f4d454b,
  0x004c4544
};

typedef struct iqm_header {
  uint32_t magic[4];
  uint32_t version;
  uint32_t filesize;
  uint32_t flags;
  uint32_t text_n, text_o;
  uint32_t mesh_n, mesh_o;
  uint32_t vertarray_n, vert_n, vertarray_o;
  uint32_t tris_n, tris_o, adj_o;
  uint32_t joints_n, joints_o;
  uint32_t poses_n, poses_o;
  uint32_t anims_n, anims_o;
  uint32_t frames_n, framech_n, frames_o, bounds_o;
  uint32_t comment_n, comment_o;
  uint32_t ext_n, ext_o;
} iqm_header_t;

typedef struct iqm_mesh {
  uint32_t name_i;
  uint32_t material_i;
  uint32_t vert_i, vert_n;
  uint32_t tris_i, tris_n;
} iqm_mesh_t;

typedef struct iqm_triangle {
  uint32_t vert_i[3];
} iqm_triangle_t;

typedef struct iqm_joint {
  uint32_t name_i;
  int32_t  parent_i;
  float    pos[3], rot[4], scale[3];
} iqm_joint_t;

typedef struct iqm_pose {
  int32_t  parent_i;
  uint32_t mask;
  float    channeloffset[10];
  float    channelscale[10];
} iqm_pose_t;

typedef struct iqm_anim {
  uint32_t name_i;
  uint32_t frame_i, frame_n;
  float    framerate;
  uint32_t flags;
} iqm_anim_t;

typedef struct iqm_vertarray {
  uint32_t type;
  uint32_t flags;
  uint32_t format;
  uint32_t size;
  uint32_t offset;
} iqm_vertarray_t;

typedef struct iqm_bounds {
  float bbmin[3], bbmax[3];
  float xyradius, radius;
} iqm_bounds_t;

typedef struct mat3 {
  float data[9];
} mat3_t;

int kl_model_isiqm2(uint8_t *data, int size) {
  iqm_header_t *header = (iqm_header_t*)data;
  if (size < sizeof(iqm_header_t)) return 0;
  return header->magic[0] == iqm_magic[0] && header->magic[1] == iqm_magic[1] &&
         header->magic[2] == iqm_magic[2] && header->magic[3] == iqm_magic[3];
}

kl_model_t* kl_model_loadiqm2(uint8_t *data) {
  iqm_header_t *header = (iqm_header_t*)data;
  if (header->magic[0] != iqm_magic[0] || header->magic[1] != iqm_magic[1] ||
      header->magic[2] != iqm_magic[2] || header->magic[3] != iqm_magic[3])
  {
    fprintf(stderr, "Mesh-IQM2: Not an IQM model!\n");
    return NULL;
  }
  if (header->version != IQM_VERSION) {
    fprintf(stderr, "Mesh-IQM2: Wrong version!  (Got %d, expected %d)\n", header->version, IQM_VERSION);
    return NULL;
  }

  iqm_vertarray_t *vertarrays = (iqm_vertarray_t*)(data + header->vertarray_o);
  iqm_mesh_t *meshes = (iqm_mesh_t*)(data + header->mesh_o);
  char *text = (char*)(data + header->text_o);

  iqm_vertarray_t *va_position = NULL;
  iqm_vertarray_t *va_texcoord = NULL;
  iqm_vertarray_t *va_normal   = NULL;
  iqm_vertarray_t *va_tangent  = NULL;
  iqm_vertarray_t *va_blendidx = NULL;
  iqm_vertarray_t *va_blendwt  = NULL;

  for (int j=0; j < header->vertarray_n; j++) {
    iqm_vertarray_t *vertarray = vertarrays + j;
    switch (vertarray->type) {
      case IQM_POSITION:
        if (vertarray->format != IQM_FLOAT || vertarray->size != 3) {
          fprintf(stderr, "Mesh-IQM2: Bad vertex position format!\n");
          return NULL;
        }
        va_position = vertarray;
        break;
      case IQM_TEXCOORD:
        if (vertarray->format != IQM_FLOAT || vertarray->size != 2) {
          fprintf(stderr, "Mesh-IQM2: Bad texture coordinate format!\n");
          return NULL;
        }
        va_texcoord = vertarray;
        break;
      case IQM_NORMAL:
        if (vertarray->format != IQM_FLOAT || vertarray->size != 3) {
          fprintf(stderr, "Mesh-IQM2: Bad vertex normal format!\n");
          return NULL;
        }
        va_normal = vertarray;
        break;
      case IQM_TANGENT:
        if (vertarray->format != IQM_FLOAT || vertarray->size != 4) {
          fprintf(stderr, "Mesh-IQM2: Bad vertex tangent format!\n");
          return NULL;
        }
        va_tangent = vertarray;
        break;
      case IQM_BLENDIDX:
        if (vertarray->format != IQM_UINT8 || vertarray->size != 4) {
          fprintf(stderr, "Mesh-IQM2: Bad blend index format!\n");
          return NULL;
        }
        va_blendidx = vertarray;
        break;
      case IQM_BLENDWT:
        if (vertarray->format != IQM_UINT8 || vertarray->size != 4) {
          fprintf(stderr, "Mesh-IQM2: Bad blend weight format!\n");
          return NULL;
        }
        va_blendwt = vertarray;
        break;
    }
  }

  kl_model_t *model = malloc(sizeof(kl_model_t) + header->mesh_n * sizeof(kl_mesh_t));

  model->winding = KL_RENDER_CW;

  model->bufs[KL_BUFFER_POSITION] = 0;
  model->bufs[KL_BUFFER_TEXCOORD] = 0;
  model->bufs[KL_BUFFER_NORMAL]   = 0;
  model->bufs[KL_BUFFER_TANGENT]  = 0;
  model->bufs[KL_BUFFER_BLENDIDX] = 0;
  model->bufs[KL_BUFFER_BLENDWT]  = 0;

  if (va_position != NULL) {
    void *attr = data + va_position->offset;
    int   size = 3 * header->vert_n * sizeof(float);
    int   vbo  = kl_render_upload_vertdata(attr, size);
    model->bufs[KL_BUFFER_POSITION] = vbo;
  }
  if (va_texcoord != NULL) {
    void *attr = data + va_texcoord->offset;
    int   size = 2 * header->vert_n * sizeof(float);
    int   vbo  = kl_render_upload_vertdata(attr, size);
    model->bufs[KL_BUFFER_TEXCOORD] = vbo;
  }
  if (va_normal != NULL) {
    void *attr = data + va_normal->offset;
    int   size = 3 * header->vert_n * sizeof(float);
    int   vbo  = kl_render_upload_vertdata(attr, size);
    model->bufs[KL_BUFFER_NORMAL] = vbo;
  }
  if (va_tangent != NULL) {
    void *attr = data + va_tangent->offset;
    int   size = 4 * header->vert_n * sizeof(float);
    int   vbo  = kl_render_upload_vertdata(attr, size);
    model->bufs[KL_BUFFER_TANGENT] = vbo;
  }
  if (va_blendidx != NULL) {
    void *attr = data + va_blendidx->offset;
    int   size = 4 * header->vert_n * sizeof(uint8_t);
    int   vbo  = kl_render_upload_vertdata(attr, size);
    model->bufs[KL_BUFFER_BLENDIDX] = vbo;
  }
  if (va_blendwt  != NULL) {
    void *attr = data + va_blendwt->offset;
    int   size = 4 * header->vert_n * sizeof(uint8_t);
    int   vbo  = kl_render_upload_vertdata(attr, size);
    model->bufs[KL_BUFFER_BLENDWT] = vbo;
  }
  
  /* portability issue: assumes uint32_t and unsigned int are equivalent */
  model->tris = kl_render_upload_tris((unsigned int*)(data + header->tris_o), header->tris_n * sizeof(iqm_triangle_t));

  kl_render_attrib_t cfg[6];
  cfg[0] = (kl_render_attrib_t){
    .index  = 0,
    .size   = 3,
    .type   = KL_RENDER_FLOAT,
    .buffer = model->bufs[KL_BUFFER_POSITION]
  };
  cfg[1] = (kl_render_attrib_t){
    .index  = 1,
    .size   = 2,
    .type   = KL_RENDER_FLOAT,
    .buffer = model->bufs[KL_BUFFER_TEXCOORD]
  };
  cfg[2] = (kl_render_attrib_t){
    .index  = 2,
    .size   = 3,
    .type   = KL_RENDER_FLOAT,
    .buffer = model->bufs[KL_BUFFER_NORMAL]
  };
  cfg[3] = (kl_render_attrib_t){
    .index  = 3,
    .size   = 4,
    .type   = KL_RENDER_FLOAT,
    .buffer = model->bufs[KL_BUFFER_TANGENT]
  };
  cfg[4] = (kl_render_attrib_t){
    .index  = 4,
    .size   = 4,
    .type   = KL_RENDER_UINT8,
    .buffer = model->bufs[KL_BUFFER_BLENDIDX]
  };
  cfg[5] = (kl_render_attrib_t){
    .index  = 5,
    .size   = 4,
    .type   = KL_RENDER_UINT8,
    .buffer = model->bufs[KL_BUFFER_BLENDWT]
  };

  model->attribs = kl_render_define_attribs(model->tris, cfg, 6);
  
  model->mesh_n = header->mesh_n;
  for (int i=0; i < header->mesh_n; i++) {
    iqm_mesh_t *mesh = meshes + i;
    
    char *path = text + mesh->material_i;
    char  buf[256];
    kl_texture_t *diffuse  = kl_texture_incref(path);
    snprintf(buf, 256, "%s_n", path);
    kl_texture_t *normal   = kl_texture_incref(buf);
    snprintf(buf, 256, "%s_s", path);
    kl_texture_t *specular = kl_texture_incref(buf);
    snprintf(buf, 256, "%s_e", path);
    kl_texture_t *emissive = kl_texture_incref(buf);

    model->mesh[i] = (kl_mesh_t){
      .material = {
        .diffuse  = diffuse,
        .normal   = normal,
        .specular = specular,
        .emissive = emissive
      },
      .tris_i = mesh->tris_i,
      .tris_n = mesh->tris_n
    };
  }

  return model;
}   


/* vim: set ts=2 sw=2 et */
