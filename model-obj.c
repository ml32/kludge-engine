#define _BSD_SOURCE /* need strsep() */

#include "model-obj.h"

#include "model.h"
#include "renderer.h"
#include "array.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* Wavefront OBJ doesn't have a magic number so I'm making one up */
#define OBJ_MAGIC 0x4a424f23 

#define INDEXMAP_MAXENTRIES 8
typedef struct indexmap {
  int n;
  int texidx[INDEXMAP_MAXENTRIES];
  int normidx[INDEXMAP_MAXENTRIES];
  int vertidx[INDEXMAP_MAXENTRIES];
} indexmap_t;

typedef struct triangle {
  unsigned int vert[3];
} triangle_t;

typedef struct bufinfo {
  uint8_t *start;
  uint8_t *end;
  uint8_t *cur;
} bufinfo_t;

typedef struct obj_face_vert {
  int posidx;
  int normidx;
  int texidx;
} obj_face_vert_t;

#define FACE_MAXVERTS 8
typedef struct obj_face {
  int num_verts;
  obj_face_vert_t vert[FACE_MAXVERTS];
} obj_face_t;

typedef struct obj_data {
  /* raw entries, as written in the obj file */
  kl_array_t rawposition, rawnormal, rawtexcoord, rawtris;
  /* maps from separate position/normal/texcoord indices to combined vertex data: */
  kl_array_t indexmap;
  /* vertex data to be loaded into renderer */
  kl_array_t bufposition, bufnormal, buftangent, buftexcoord, buftris;
} obj_data_t;

static void objdata_init(obj_data_t *data);
static void objdata_free(obj_data_t *data);
static int  objdata_getvertidx(obj_data_t *objdata, obj_face_vert_t *vert);
static int parseline(obj_data_t *objdata, char *line);
static int parsevertex(obj_data_t *objdata, char *line);
static int parsenormal(obj_data_t *objdata, char *line);
static int parsetexcoord(obj_data_t *objdata, char *line);
static int parseface(obj_data_t *objdata, char *line);
static int parsefacevert(char *str, obj_face_vert_t *dst);

/* ------------------------ */
int kl_model_isobj(uint8_t *data, int size) {
  if (size < 4) return 0;
  uint32_t magic = *((uint32_t*)data);
  return magic == OBJ_MAGIC;
}
  
kl_model_t* kl_model_loadobj(uint8_t *data, int size) {
  kl_model_t *model = NULL;

  obj_data_t objdata;
  objdata_init(&objdata);

  /* load data from file */
  char *buf  = malloc(size+1);
  memcpy(buf, data, size);
  buf[size] = '\0';

  char *cur = buf;
  char *line;
  do {
    line = strsep(&cur, "\n\r");
    if (parseline(&objdata, line) < 0) goto cleanup;
  } while (cur != NULL);
  free(buf);

  printf("verts: %d\nnorms: %d\ntexcoords: %d\nfaces: %d\n",
    kl_array_size(&objdata.rawposition),
    kl_array_size(&objdata.rawnormal),
    kl_array_size(&objdata.rawtexcoord),
    kl_array_size(&objdata.rawtris));

  /* initialize vert index mappings */
  indexmap_t blankmap = { .n = 0 };
  for (int i=0; i < kl_array_size(&objdata.rawposition); i++) {
    kl_array_set_expand(&objdata.indexmap, i, &blankmap);
  }

  /* build list of unique vertices from tris */
  for (int i=0; i < kl_array_size(&objdata.rawtris); i++) {
    obj_face_t objface;
    kl_array_get(&objdata.rawtris, i, &objface);
   
    for (int j=0; j < objface.num_verts - 2; j++) { 
      triangle_t tri;
      int vert;

      vert = objdata_getvertidx(&objdata, &objface.vert[0]);
      if (vert < 0) goto cleanup;
      tri.vert[0] = vert;

      vert = objdata_getvertidx(&objdata, &objface.vert[j+1]);
      if (vert < 0) goto cleanup;
      tri.vert[1] = vert;

      vert = objdata_getvertidx(&objdata, &objface.vert[j+2]);
      if (vert < 0) goto cleanup;
      tri.vert[2] = vert;

      kl_array_push(&objdata.buftris, &tri);
    }
  }

  /* load model data */
  model = malloc(sizeof(kl_model_t) + sizeof(kl_mesh_t));

  kl_sphere_bounds(&model->bounds, (kl_vec3f_t*)kl_array_data(&objdata.bufposition), kl_array_size(&objdata.bufposition));
  model->winding = KL_RENDER_CCW;

  model->bufs[KL_BUFFER_POSITION] = 0;
  model->bufs[KL_BUFFER_TEXCOORD] = 0;
  model->bufs[KL_BUFFER_NORMAL]   = 0;
  model->bufs[KL_BUFFER_TANGENT]  = 0;
  model->bufs[KL_BUFFER_BLENDIDX] = 0;
  model->bufs[KL_BUFFER_BLENDWT]  = 0;

  int bytes;
  unsigned int vbo;

  bytes = kl_array_size(&objdata.bufposition) * sizeof(kl_vec3f_t);
  vbo   = kl_render_upload_vertdata(kl_array_data(&objdata.bufposition), bytes);
  model->bufs[KL_BUFFER_POSITION] = vbo;

  bytes = kl_array_size(&objdata.bufnormal) * sizeof(kl_vec3f_t);
  vbo   = kl_render_upload_vertdata(kl_array_data(&objdata.bufnormal), bytes);
  model->bufs[KL_BUFFER_NORMAL] = vbo;

  bytes = kl_array_size(&objdata.buftangent) * sizeof(kl_vec4f_t);
  vbo   = kl_render_upload_vertdata(kl_array_data(&objdata.buftangent), bytes);
  model->bufs[KL_BUFFER_TANGENT] = vbo;

  bytes = kl_array_size(&objdata.buftexcoord) * sizeof(kl_vec2f_t);
  vbo   = kl_render_upload_vertdata(kl_array_data(&objdata.buftexcoord), bytes);
  model->bufs[KL_BUFFER_TEXCOORD] = vbo;

  bytes = kl_array_size(&objdata.buftris) * sizeof(triangle_t);
  model->tris = kl_render_upload_tris((unsigned int*)kl_array_data(&objdata.buftris), bytes);

  kl_render_attrib_t cfg[4];
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

  model->attribs = kl_render_define_attribs(model->tris, cfg, 4);
  
  kl_texture_t *diffuse  = kl_texture_incref("DEFAULT_DIFFUSE");
  kl_texture_t *normal   = kl_texture_incref("DEFAULT_NORMAL");
  kl_texture_t *specular = kl_texture_incref("DEFAULT_SPECULAR");
  kl_texture_t *emissive = kl_texture_incref("DEFAULT_EMISSIVE");
  model->mesh_n = 1;
  model->mesh[0] = (kl_mesh_t){
    .material = {
      .diffuse  = diffuse,
      .normal   = normal,
      .specular = specular,
      .emissive = emissive
    },
    .tris_i = 0,
    .tris_n = kl_array_size(&objdata.buftris)
  };

  cleanup:
  objdata_free(&objdata);
  return model;
}

/* -------------------- */
static void objdata_init(obj_data_t *data) {
  kl_array_init(&data->rawposition, sizeof(kl_vec3f_t));
  kl_array_init(&data->rawnormal,   sizeof(kl_vec3f_t));
  kl_array_init(&data->rawtexcoord, sizeof(kl_vec2f_t));
  kl_array_init(&data->rawtris,     sizeof(obj_face_t));
  kl_array_init(&data->indexmap,    sizeof(indexmap_t));
  kl_array_init(&data->bufposition, sizeof(kl_vec3f_t));
  kl_array_init(&data->bufnormal,   sizeof(kl_vec3f_t));
  kl_array_init(&data->buftangent,  sizeof(kl_vec4f_t));
  kl_array_init(&data->buftexcoord, sizeof(kl_vec2f_t));
  kl_array_init(&data->buftris,     sizeof(triangle_t));
}

static void objdata_free(obj_data_t *data) {
  kl_array_free(&data->rawposition);
  kl_array_free(&data->rawnormal);
  kl_array_free(&data->rawtexcoord);
  kl_array_free(&data->rawtris);
  kl_array_free(&data->indexmap);
  kl_array_free(&data->bufposition);
  kl_array_free(&data->bufnormal);
  kl_array_free(&data->buftangent);
  kl_array_free(&data->buftexcoord);
  kl_array_free(&data->buftris);
}

static int objdata_getvertidx(obj_data_t *objdata, obj_face_vert_t *vert) {
  indexmap_t map;
  kl_array_get(&objdata->indexmap, vert->posidx, &map);
  for (int i=0; i < map.n; i++) {
    if (map.texidx[i] == vert->texidx && map.normidx[i] == vert->normidx) {
      return map.vertidx[i];
    }
  }

  if (map.n >= INDEXMAP_MAXENTRIES) {
    fprintf(stderr, "Mesh-OBJ: Overloaded vertex!  Increase INDEX_MAXENTRIES or simplify mesh!\n");
    return -1;
  }

  kl_vec3f_t position;
  kl_vec3f_t normal;
  kl_vec4f_t tangent = { .x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f };
  kl_vec2f_t texcoord;

  kl_array_get(&objdata->rawposition, vert->posidx,  &position);
  if (vert->normidx >= 0) {
    kl_array_get(&objdata->rawnormal,   vert->normidx, &normal);
  } else {
    /* todo: construct default normal from face verts */
    normal.x = 0.0f;
    normal.y = 0.0f;
    normal.z = 1.0f;
  }
  if (vert->texidx >= 0) {
    kl_array_get(&objdata->rawtexcoord, vert->texidx,  &texcoord);
  } else {
    /* todo: better default texture projection based on face normals */
    texcoord.x = position.x;
    texcoord.y = position.y;
  }
  
  int vertidx = kl_array_push(&objdata->bufposition, &position);
  kl_array_set_expand(&objdata->bufnormal,   vertidx, &normal);
  kl_array_set_expand(&objdata->buftangent,  vertidx, &tangent);
  kl_array_set_expand(&objdata->buftexcoord, vertidx, &texcoord);

  int i = map.n++;
  map.texidx[i]  = vert->texidx;
  map.normidx[i] = vert->normidx;
  map.vertidx[i] = vertidx;
  kl_array_set(&objdata->indexmap, vert->posidx, &map);

  return vertidx;
}

static int parseline(obj_data_t *objdata, char *line) {
  switch (line[0]) {
    case 'v':
      switch (line[1]) {
        case 't':
          return parsetexcoord(objdata, line);
        case 'n':
          return parsenormal(objdata, line);
        default:
          return parsevertex(objdata, line);
      }
      break;
    case 'f':
      return parseface(objdata, line);
  }
  return 0;
}

static int parsevertex(obj_data_t *objdata, char *line) {
  kl_vec3f_t position;
  if (sscanf(line, "v %f %f %f", &position.x, &position.y, &position.z) < 3) {
    fprintf(stderr, "Mesh-OBJ: Failed to read vertex coordinate!\n");
    return -1;
  }
  kl_array_push(&objdata->rawposition, &position);
  return 0;
} 

static int parsenormal(obj_data_t *objdata, char *line) {
  kl_vec3f_t normal;
  if (sscanf(line, "vn %f %f %f", &normal.x, &normal.y, &normal.z) < 3) {
    fprintf(stderr, "Mesh-OBJ: Failed to read vertex normal!\n");
    return -1;
  }
  kl_array_push(&objdata->rawnormal, &normal);
  return 0;
}
 
static int parsetexcoord(obj_data_t *objdata, char *line) {
  kl_vec2f_t texcoord;
  if (sscanf(line, "vt %f %f", &texcoord.x, &texcoord.y) < 2) {
    fprintf(stderr, "Mesh-OBJ: Failed to read texture coordinate!\n");
    return -1;
  }
  kl_array_push(&objdata->rawtexcoord, &texcoord);
  return 0;
}

static int parseface(obj_data_t *objdata, char *line) {
  obj_face_t face;
  face.num_verts = 0;

  char *str = line + 1;
  char *vertstr;
  do {
    vertstr = strsep(&str, " \t");
    if (parsefacevert(vertstr, &face.vert[face.num_verts]) < 0) continue;
    if (++face.num_verts >= FACE_MAXVERTS) break;
  } while (str != NULL);
  if (face.num_verts < 3) {
    fprintf(stderr, "Mesh-OBJ: Too few vertices for polygon! (invalid format?)\n");
    return -1;
  }
  
  kl_array_push(&objdata->rawtris, &face);
  return 0;
}

static int parsefacevert(char *str, obj_face_vert_t *dst) {
  if (sscanf(str, "%d/%d/%d", &dst->posidx, &dst->texidx, &dst->normidx) == 3) {
    dst->posidx--;
    dst->texidx--;
    dst->normidx--;
    return 0;
  }
  if (sscanf(str, "%d/%d", &dst->posidx, &dst->texidx) == 2) {
    dst->posidx--;
    dst->texidx--;
    dst->normidx = -1;
    return 0;
  }
  if (sscanf(str, "%d//%d", &dst->posidx, &dst->normidx) == 2) {
    dst->posidx--;
    dst->texidx = -1;
    dst->normidx--;
    return 0;
  }
  if (sscanf(str, "%d", &dst->posidx) == 1) {
    dst->posidx--;
    dst->normidx = -1;
    dst->texidx  = -1;
    return 0;
  }

  return -1;
}

/* vim: set ts=2 sw=2 et */
