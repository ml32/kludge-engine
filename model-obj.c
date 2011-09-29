#define _BSD_SOURCE /* need strsep() */

#include "model-obj.h"

#include "model.h"
#include "material-mtl.h"
#include "renderer.h"
#include "array.h"
#include "vec.h"

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

#define OBJ_PATHLEN 0x100
typedef struct obj_mesh {
  char material[OBJ_PATHLEN];
  unsigned int tris_i, tris_n;
} obj_mesh_t;

typedef struct obj_data {
  /* raw entries, as written in the obj file */
  kl_array_t rawposition, rawnormal, rawtexcoord;
  /* maps from separate position/normal/texcoord indices to combined vertex data: */
  kl_array_t indexmap;
  /* vertex data to be loaded into renderer */
  kl_array_t bufposition, bufnormal, buftangent, buftexcoord;
  kl_array_t tris, meshes;
} obj_data_t;

static obj_mesh_t obj_curmesh;

static void objdata_init(obj_data_t *data);
static void objdata_free(obj_data_t *data);
static int  objdata_getvertidx(obj_data_t *objdata, obj_face_vert_t *vert);
static int parseline(obj_data_t *objdata, char *line);
static int parsevertex(obj_data_t *objdata, char *line);
static int parsenormal(obj_data_t *objdata, char *line);
static int parsetexcoord(obj_data_t *objdata, char *line);
static int parseface(obj_data_t *objdata, char *line);
static int parsefacevert(char *str, obj_face_vert_t *dst);
static void gentangent(obj_data_t *objdata, unsigned int idx1, unsigned int idx2, unsigned int idx3);

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

  obj_curmesh.material[0] = '\0';
  obj_curmesh.tris_i = 0;
  obj_curmesh.tris_n = 0;

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

  int tris_i = kl_array_size(&objdata.tris);
  if (tris_i > obj_curmesh.tris_i) {
    obj_curmesh.tris_n = tris_i - obj_curmesh.tris_i;
    kl_array_push(&objdata.meshes, &obj_curmesh);
  }

  /* generate tangent data */
  for (int i=0; i < kl_array_size(&objdata.tris); i++) {
    triangle_t tri;
    kl_array_get(&objdata.tris, i, &tri);
    gentangent(&objdata, tri.vert[0], tri.vert[1], tri.vert[2]);
    gentangent(&objdata, tri.vert[1], tri.vert[2], tri.vert[0]);
    gentangent(&objdata, tri.vert[2], tri.vert[0], tri.vert[1]);
  }

  printf("verts: %d\nnorms: %d\ntexcoords: %d\nmeshes: %d\n",
    kl_array_size(&objdata.rawposition),
    kl_array_size(&objdata.rawnormal),
    kl_array_size(&objdata.rawtexcoord),
    kl_array_size(&objdata.meshes));

  /* load model data */
  int num_meshes = kl_array_size(&objdata.meshes);
  model = malloc(sizeof(kl_model_t) + num_meshes * sizeof(kl_mesh_t));

  model->type = KL_MODEL_PROP;
  kl_sphere_bounds(&model->bounds, (kl_vec3f_t*)kl_array_data(&objdata.bufposition), kl_array_size(&objdata.bufposition));
  model->winding = KL_RENDER_CCW;

  kl_model_bufs_prop_t *bufs = &model->bufs.prop;
  bufs->position = 0;
  bufs->texcoord = 0;
  bufs->normal   = 0;
  bufs->tangent  = 0;

  int bytes;
  unsigned int vbo;

  bytes = kl_array_size(&objdata.bufposition) * sizeof(kl_vec3f_t);
  vbo   = kl_render_upload_vertdata(kl_array_data(&objdata.bufposition), bytes);
  bufs->position = vbo;

  bytes = kl_array_size(&objdata.bufnormal) * sizeof(kl_vec3f_t);
  vbo   = kl_render_upload_vertdata(kl_array_data(&objdata.bufnormal), bytes);
  bufs->normal = vbo;

  bytes = kl_array_size(&objdata.buftangent) * sizeof(kl_vec4f_t);
  vbo   = kl_render_upload_vertdata(kl_array_data(&objdata.buftangent), bytes);
  bufs->tangent = vbo;

  bytes = kl_array_size(&objdata.buftexcoord) * sizeof(kl_vec2f_t);
  vbo   = kl_render_upload_vertdata(kl_array_data(&objdata.buftexcoord), bytes);
  bufs->texcoord = vbo;

  bytes = kl_array_size(&objdata.tris) * sizeof(triangle_t);
  model->tris = kl_render_upload_tris((unsigned int*)kl_array_data(&objdata.tris), bytes);

  kl_render_attrib_t cfg[4];
  cfg[0] = (kl_render_attrib_t){
    .index  = 0,
    .size   = 3,
    .type   = KL_RENDER_FLOAT,
    .buffer = bufs->position
  };
  cfg[1] = (kl_render_attrib_t){
    .index  = 1,
    .size   = 2,
    .type   = KL_RENDER_FLOAT,
    .buffer = bufs->texcoord
  };
  cfg[2] = (kl_render_attrib_t){
    .index  = 2,
    .size   = 3,
    .type   = KL_RENDER_FLOAT,
    .buffer = bufs->normal
  };
  cfg[3] = (kl_render_attrib_t){
    .index  = 3,
    .size   = 4,
    .type   = KL_RENDER_FLOAT,
    .buffer = bufs->tangent
  };

  model->attribs = kl_render_define_attribs(model->tris, cfg, 4);
  
  model->mesh_n = num_meshes;
  for (int i=0; i < num_meshes; i++) {
    obj_mesh_t mesh;
    kl_array_get(&objdata.meshes, i, &mesh);
    model->mesh[i].material = kl_material_incref(mesh.material);
    kl_array_get(&objdata.meshes, i, &mesh);
    model->mesh[i].tris_i   = mesh.tris_i;
    model->mesh[i].tris_n   = mesh.tris_n;
  }

  cleanup:
  objdata_free(&objdata);
  return model;
}

/* -------------------- */
static void objdata_init(obj_data_t *data) {
  kl_array_init(&data->rawposition, sizeof(kl_vec3f_t));
  kl_array_init(&data->rawnormal,   sizeof(kl_vec3f_t));
  kl_array_init(&data->rawtexcoord, sizeof(kl_vec2f_t));
  kl_array_init(&data->indexmap,    sizeof(indexmap_t));
  kl_array_init(&data->bufposition, sizeof(kl_vec3f_t));
  kl_array_init(&data->bufnormal,   sizeof(kl_vec3f_t));
  kl_array_init(&data->buftangent,  sizeof(kl_vec4f_t));
  kl_array_init(&data->buftexcoord, sizeof(kl_vec2f_t));
  kl_array_init(&data->tris,        sizeof(triangle_t));
  kl_array_init(&data->meshes,      sizeof(obj_mesh_t));
}

static void objdata_free(obj_data_t *data) {
  kl_array_free(&data->rawposition);
  kl_array_free(&data->rawnormal);
  kl_array_free(&data->rawtexcoord);
  kl_array_free(&data->meshes);
  kl_array_free(&data->indexmap);
  kl_array_free(&data->bufposition);
  kl_array_free(&data->bufnormal);
  kl_array_free(&data->buftangent);
  kl_array_free(&data->buftexcoord);
  kl_array_free(&data->tris);
}

static int objdata_getvertidx(obj_data_t *objdata, obj_face_vert_t *vert) {
  indexmap_t map;
  if (vert->posidx < kl_array_size(&objdata->indexmap)) {
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
  } else {
    map.n = 0;
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
  kl_array_set_expand(&objdata->bufnormal,   vertidx, &normal,   0);
  kl_array_set_expand(&objdata->buftangent,  vertidx, &tangent,  0);
  kl_array_set_expand(&objdata->buftexcoord, vertidx, &texcoord, 0);

  int i = map.n++;
  map.texidx[i]  = vert->texidx;
  map.normidx[i] = vert->normidx;
  map.vertidx[i] = vertidx;
  kl_array_set_expand(&objdata->indexmap, vert->posidx, &map, 0);

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

  char *cur = line;
  char *def;
  def = strsep(&cur, " \t");
  if (strcmp(def, "mtllib") == 0) {
    char *path = strsep(&cur, " \t");
    kl_material_setmtl(path);
  } else if (strcmp(def, "usemtl") == 0) {
    int tris_i = kl_array_size(&objdata->tris);
    if (tris_i > obj_curmesh.tris_i) {
      obj_curmesh.tris_n = tris_i - obj_curmesh.tris_i;
      kl_array_push(&objdata->meshes, &obj_curmesh);
    }
    char *path = strsep(&cur, " \t");
    strncpy(obj_curmesh.material, path, OBJ_PATHLEN);
    obj_curmesh.tris_i = tris_i;
    obj_curmesh.tris_n = 0;
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
  kl_vec3f_norm(&normal, &normal);
  kl_array_push(&objdata->rawnormal, &normal);
  return 0;
}
 
static int parsetexcoord(obj_data_t *objdata, char *line) {
  kl_vec2f_t texcoord;
  if (sscanf(line, "vt %f %f", &texcoord.x, &texcoord.y) < 2) {
    fprintf(stderr, "Mesh-OBJ: Failed to read texture coordinate!\n");
    return -1;
  }
  /* these are backwards for some reason... */
  //texcoord.x = -texcoord.x;
  texcoord.y = -texcoord.y;
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
  
  for (int i=0; i < face.num_verts - 2; i++) { 
    triangle_t tri;
    int vert;

    vert = objdata_getvertidx(objdata, &face.vert[0]);
    if (vert < 0) return -1;
    tri.vert[0] = vert;

    vert = objdata_getvertidx(objdata, &face.vert[i+1]);
    if (vert < 0) return -1;
    tri.vert[1] = vert;

    vert = objdata_getvertidx(objdata, &face.vert[i+2]);
    if (vert < 0) return -1;
    tri.vert[2] = vert;

    kl_array_push(&objdata->tris, &tri);
  }
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
static void gentangent(obj_data_t *objdata, unsigned int idx1, unsigned int idx2, unsigned int idx3) {
  kl_vec3f_t p0, p1, p2;
  kl_array_get(&objdata->bufposition, idx1, &p0);
  kl_array_get(&objdata->bufposition, idx2, &p1);
  kl_array_get(&objdata->bufposition, idx3, &p2);
  kl_vec3f_t normal;
  kl_array_get(&objdata->bufposition, idx1, &normal);
  kl_vec2f_t t0, t1, t2;
  kl_array_get(&objdata->buftexcoord, idx1, &t0);
  kl_array_get(&objdata->buftexcoord, idx2, &t1);
  kl_array_get(&objdata->buftexcoord, idx3, &t2);

  kl_vec3f_t dp1, dp2;
  kl_vec3f_sub(&dp1, &p1, &p0);
  kl_vec3f_sub(&dp2, &p2, &p0);

  float du1 = t1.x - t0.x;
  float du2 = t2.x - t0.x;
  float dv1 = t1.y - t0.y;
  float dv2 = t2.y - t0.y;

  float scale = 1.0f / (du1 * dv2 - du2 * dv1);

  kl_vec3f_t dv2dp1, dv1dp2, du1dp2, du2dp1;
  kl_vec3f_scale(&dv2dp1, &dp1, dv2);
  kl_vec3f_scale(&dv1dp2, &dp2, dv1);
  kl_vec3f_scale(&du1dp2, &dp2, du1);
  kl_vec3f_scale(&du2dp1, &dp1, du2);

  kl_vec3f_t tangent_prime, bitangent_prime;
  kl_vec3f_sub(&tangent_prime, &dv2dp1, &dv1dp2);
  kl_vec3f_scale(&tangent_prime, &tangent_prime, scale);
  kl_vec3f_sub(&bitangent_prime, &du1dp2, &du2dp1);
  kl_vec3f_scale(&bitangent_prime, &bitangent_prime, scale);

  kl_vec3f_t tangent, bitangent;
  kl_vec3f_cross(&tangent, &bitangent_prime, &normal);
  kl_vec3f_norm(&tangent, &tangent);
  kl_vec3f_cross(&bitangent, &normal, &tangent_prime);
  kl_vec3f_norm(&bitangent, &bitangent);

  kl_vec4f_t final;
  final.x = tangent.x;
  final.y = tangent.y;
  final.z = tangent.z;
  final.w = 1.0f;

  kl_array_set(&objdata->buftangent, idx1, &final);
}

/* vim: set ts=2 sw=2 et */
