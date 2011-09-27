#include "terrain.h"

#include "sphere.h"
#include "array.h"
#include "vec.h"

#include <stdio.h>

#define FLAG_EMPTY (1 << 0)
#define FLAG_FULL  (1 << 1)

typedef struct mesh {
  kl_array_t verts;
  kl_array_t norms;
  kl_array_t tris;
} mesh_t;

typedef struct triangle {
  unsigned int vert[3];
} triangle_t;

typedef struct voxel_info {
  int flags;
  kl_vec3f_t position;
  kl_vec3f_t normal;
} voxel_info_t;

typedef struct svo_node {
  struct voxel_info info;
  struct svo_node*  children[8];
} svo_node_t;

static void svo_free(svo_node_t *node) {
  if (node == NULL) return;
  for (int i=0; i < 8; i++) {
    svo_free(node->children[i]);
  }
  free(node);
}

static void mesh_init(mesh_t *mesh) {
  kl_array_init(&mesh->verts, sizeof(kl_vec3f_t));
  kl_array_init(&mesh->norms, sizeof(kl_vec3f_t));
  kl_array_init(&mesh->tris, sizeof(triangle_t));
}

static void mesh_free(mesh_t *mesh) {
  kl_array_free(&mesh->verts);
  kl_array_free(&mesh->norms);
  kl_array_free(&mesh->tris);
}

static svo_node_t* testsphere(kl_sphere_t *sphere, int depth, int x, int y, int z) {
  if (depth < 0) return NULL;
  int size   = 1 << depth;
  int center = depth > 0 ? 1 << (depth - 1) : 0;
  svo_node_t *node = malloc(sizeof(svo_node_t));

  kl_vec3f_t position, normal;
  position.x = (float)(x + center);
  position.y = (float)(y + center);
  position.z = (float)(z + center);
  kl_vec3f_sub(&normal, &position, &sphere->center);
  kl_vec3f_norm(&normal, &normal);
  node->info.position = position;
  node->info.normal   = normal;

  kl_vec3f_t corners[8] = {
    { .x = x,      .y = y,      .z = z },
    { .x = x+size, .y = y,      .z = z },
    { .x = x,      .y = y+size, .z = z },
    { .x = x+size, .y = y+size, .z = z },
    { .x = x,      .y = y,      .z = z+size },
    { .x = x+size, .y = y,      .z = z+size },
    { .x = x,      .y = y+size, .z = z+size },
    { .x = x+size, .y = y+size, .z = z+size }
  };
  int test = 0;
  for (int i=0; i < 8; i++) {
    test |= kl_sphere_test(sphere, &corners[i]) << i;
  }

  if (test == 0xFF) {
    node->info.flags = FLAG_FULL;
    for (int i=0; i < 8; i++) {
      node->children[i] = NULL;
    }
  } else {
    kl_vec3f_t offset, nearest;
    kl_vec3f_sub(&offset, &position, &sphere->center);
    if (kl_vec3f_magnitude(&offset) > sphere->radius) {
      kl_vec3f_norm(&offset, &offset);
      kl_vec3f_scale(&offset, &offset, sphere->radius);
    }
    kl_vec3f_add(&nearest, &sphere->center, &offset);

    if (nearest.x < x || nearest.x > x + size ||
        nearest.y < y || nearest.y > y + size ||
        nearest.z < z || nearest.z > z + size)
    {
      node->info.flags = FLAG_EMPTY;
      for (int i=0; i < 8; i++) {
        node->children[i] = NULL;
      }
    } else {
      node->info.flags = 0;
      for (int i=0; i < 8; i++) {
        node->children[i] = testsphere(sphere, depth-1, i & 0x01 ? x + center : x, i & 0x02 ? y + center : y, i & 0x04 ? z + center : z);
      }
    }
  }
  return node;
}

static void getvoxel(svo_node_t *root, int depth, int x, int y, int z, voxel_info_t *info) {
  if (depth == 0) {
    *info = root->info;
    return; 
  }
  int mask  = 1 << depth;
  int index = (mask & z ? 0x04 : 0x00) | (mask & y ? 0x02 : 0x00) | (mask & x ? 0x01 : 0x00);
  svo_node_t *node = root->children[index];
  if (node == NULL) {
    *info = root->info;
    return;
  }
  getvoxel(node, depth-1, x, y, z, info);
}

static int genvert(mesh_t *mesh, voxel_info_t *v0, voxel_info_t *v1) {
  kl_vec3f_t vert;
  kl_vec3f_t norm;
  kl_vec3f_midpoint(&vert, &v0->position, &v1->position);
  kl_vec3f_midpoint(&norm, &v0->normal,   &v1->normal);
  kl_vec3f_norm(&norm, &norm);
  int i = kl_array_push(&mesh->verts, &vert);
  kl_array_set_expand(&mesh->norms, i, &norm, 0);
  return i;
}

static void genfaces(mesh_t *mesh, voxel_info_t *voxel) {
  int cubetype = 0;
  for (int i=0; i < 8; i++) {
    cubetype |= (voxel[i].flags & FLAG_EMPTY ? 0 : 1) << i;
  }
  triangle_t tri;
  switch (cubetype) {
    case 0x01:
      tri.vert[0] = genvert(mesh, &voxel[0], &voxel[1]);
      tri.vert[1] = genvert(mesh, &voxel[0], &voxel[2]);
      tri.vert[2] = genvert(mesh, &voxel[0], &voxel[4]);
      kl_array_push(&mesh->tris, &tri);
      break;
    case 0x02:
      tri.vert[0] = genvert(mesh, &voxel[1], &voxel[0]);
      tri.vert[1] = genvert(mesh, &voxel[1], &voxel[5]);
      tri.vert[2] = genvert(mesh, &voxel[1], &voxel[3]);
      kl_array_push(&mesh->tris, &tri);
      break;
    case 0x03:
      tri.vert[0] = genvert(mesh, &voxel[0], &voxel[2]);
      tri.vert[1] = genvert(mesh, &voxel[0], &voxel[4]);
      tri.vert[2] = genvert(mesh, &voxel[1], &voxel[5]);
      kl_array_push(&mesh->tris, &tri);
      tri.vert[0] = genvert(mesh, &voxel[1], &voxel[3]);
      tri.vert[1] = genvert(mesh, &voxel[0], &voxel[2]);
      tri.vert[2] = genvert(mesh, &voxel[1], &voxel[5]);
      kl_array_push(&mesh->tris, &tri);
      break;
    case 0x04:
      tri.vert[0] = genvert(mesh, &voxel[2], &voxel[0]);
      tri.vert[1] = genvert(mesh, &voxel[2], &voxel[3]);
      tri.vert[2] = genvert(mesh, &voxel[2], &voxel[6]);
      kl_array_push(&mesh->tris, &tri);
      break;
    case 0x05:
      tri.vert[0] = genvert(mesh, &voxel[0], &voxel[1]);
      tri.vert[1] = genvert(mesh, &voxel[2], &voxel[6]);
      tri.vert[2] = genvert(mesh, &voxel[0], &voxel[4]);
      kl_array_push(&mesh->tris, &tri);
      tri.vert[0] = genvert(mesh, &voxel[2], &voxel[3]);
      tri.vert[1] = genvert(mesh, &voxel[2], &voxel[6]);
      tri.vert[2] = genvert(mesh, &voxel[0], &voxel[1]);
      kl_array_push(&mesh->tris, &tri);
      break;
    case 0x06:
      tri.vert[0] = genvert(mesh, &voxel[1], &voxel[0]);
      tri.vert[1] = genvert(mesh, &voxel[1], &voxel[5]);
      tri.vert[2] = genvert(mesh, &voxel[1], &voxel[3]);
      kl_array_push(&mesh->tris, &tri);
      tri.vert[0] = genvert(mesh, &voxel[2], &voxel[0]);
      tri.vert[1] = genvert(mesh, &voxel[2], &voxel[3]);
      tri.vert[2] = genvert(mesh, &voxel[2], &voxel[6]);
      kl_array_push(&mesh->tris, &tri);
      break;
    case 0x07:
      tri.vert[0] = genvert(mesh, &voxel[0], &voxel[4]);
      tri.vert[1] = genvert(mesh, &voxel[1], &voxel[5]);
      tri.vert[2] = genvert(mesh, &voxel[2], &voxel[6]);
      kl_array_push(&mesh->tris, &tri);
      tri.vert[0] = genvert(mesh, &voxel[1], &voxel[3]);
      tri.vert[1] = genvert(mesh, &voxel[2], &voxel[3]);
      tri.vert[2] = genvert(mesh, &voxel[1], &voxel[5]);
      kl_array_push(&mesh->tris, &tri);
      tri.vert[0] = genvert(mesh, &voxel[2], &voxel[3]);
      tri.vert[1] = genvert(mesh, &voxel[2], &voxel[6]);
      tri.vert[2] = genvert(mesh, &voxel[1], &voxel[5]);
      kl_array_push(&mesh->tris, &tri);
      break;
  }
}

static void meshify(svo_node_t *root, mesh_t *mesh, int depth, int x, int y, int z) {
  if (depth < 1) return;
  int size   = 1 << depth;
  int center = 1 << (depth - 1);

  voxel_info_t voxel[8];
  int x0, x1, y0, y1, z0, z1;
  /* meshify xy-seam */
  z0 = z+center-1;
  z1 = z+center;
  for (int i=0; i < size-1; i++) {
    x0 = x+i;
    x1 = x+i+1;
    for (int j=0; j < size-1; j++) {
      y0 = y+j;
      y1 = y+j+1;
      getvoxel(root, depth, x0, y0, z0, &voxel[0]);
      getvoxel(root, depth, x1, y0, z0, &voxel[1]);
      getvoxel(root, depth, x0, y1, z0, &voxel[2]);
      getvoxel(root, depth, x1, y1, z0, &voxel[3]);
      getvoxel(root, depth, x0, y0, z1, &voxel[4]);
      getvoxel(root, depth, x1, y0, z1, &voxel[5]);
      getvoxel(root, depth, x0, y1, z1, &voxel[6]);
      getvoxel(root, depth, x1, y1, z1, &voxel[7]);
      genfaces(mesh, voxel);
    }
  }
  /* meshify xz-seam */
  y0 = y+center-1;
  y1 = y+center;
  for (int i=0; i < size-1; i++) {
    z0 = z+i;
    z1 = z+i+1;
    if (z0 == z+center-1) continue;
    for (int j=0; j < size-1; j++) {
      x0 = x+j;
      x1 = x+j+1;
      getvoxel(root, depth, x0, y0, z0, &voxel[0]);
      getvoxel(root, depth, x1, y0, z0, &voxel[1]);
      getvoxel(root, depth, x0, y1, z0, &voxel[2]);
      getvoxel(root, depth, x1, y1, z0, &voxel[3]);
      getvoxel(root, depth, x0, y0, z1, &voxel[4]);
      getvoxel(root, depth, x1, y0, z1, &voxel[5]);
      getvoxel(root, depth, x0, y1, z1, &voxel[6]);
      getvoxel(root, depth, x1, y1, z1, &voxel[7]);
      genfaces(mesh, voxel);
    }
  }
  /* meshify yz-seam */
  x0 = x+center-1;
  x1 = x+center;
  for (int i=0; i < size-1; i++) {
    z0 = z+i;
    z1 = z+i+1;
    if (z0 == z+center-1) continue;
    for (int j=0; j < size-1; j++) {
      y0 = y+j;
      y1 = y+j+1;
      if (y0 == y+center-1) continue;
      getvoxel(root, depth, x0, y0, z0, &voxel[0]);
      getvoxel(root, depth, x1, y0, z0, &voxel[1]);
      getvoxel(root, depth, x0, y1, z0, &voxel[2]);
      getvoxel(root, depth, x1, y1, z0, &voxel[3]);
      getvoxel(root, depth, x0, y0, z1, &voxel[4]);
      getvoxel(root, depth, x1, y0, z1, &voxel[5]);
      getvoxel(root, depth, x0, y1, z1, &voxel[6]);
      getvoxel(root, depth, x1, y1, z1, &voxel[7]);
      genfaces(mesh, voxel);
    }
  }

  meshify(root, mesh, depth-1, x,        y,        z);
  meshify(root, mesh, depth-1, x+center, y,        z);
  meshify(root, mesh, depth-1, x,        y+center, z);
  meshify(root, mesh, depth-1, x+center, y+center, z);
  meshify(root, mesh, depth-1, x,        y,        z+center);
  meshify(root, mesh, depth-1, x+center, y,        z+center);
  meshify(root, mesh, depth-1, x,        y+center, z+center);
  meshify(root, mesh, depth-1, x+center, y+center, z+center);
}

void kl_terrain_testsphere() {
  kl_sphere_t sphere = {
    .center = { .x = 0.0f, .y = 0.0f, .z = 0.0f },
    .radius = 50.0f
  };
  int depth = 7;
  svo_node_t *root = testsphere(&sphere, depth, 0, 0, 0);
  mesh_t mesh;
  mesh_init(&mesh);
  meshify(root, &mesh, depth, 0, 0, 0);
  svo_free(root);
  mesh_free(&mesh);
}
/* vim: set ts=2 sw=2 et */
