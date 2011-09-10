#include "material.h"

#include "material-mtl.h"
#include "resource.h"
#include "texture.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static kl_material_t* material_load(char *path);
static void material_free(kl_material_t *material);
static void material_load_raw(char *path, kl_material_t *material);
static void material_load_default(char *path, kl_material_t *material);

static kl_resources_t resources = {
  .load = (kl_resources_load_cb)&material_load,
  .free = (kl_resources_free_cb)&material_free
};

/* --------------- */
kl_material_t *kl_material_incref(char *path) {
  kl_material_t *material = kl_resources_incref(&resources, path);
  if (material == NULL) return 0;
  return material;
}

void kl_material_decref(kl_material_t *material) {
  kl_resources_decref(&resources, material->path);
}

/* --------------- */

static kl_material_t *material_load(char *path) {
  kl_material_t *material = malloc(sizeof(kl_material_t));

  if (strcmp(path, "DEFAULT_MATERIAL") == 0) {
    material_load_default(path, material);
    return material;
  }
  if (kl_material_loadfrommtl(path, material)) return material;
  material_load_raw(path, material);
  return material;
}

static void material_free(kl_material_t *material) {
  kl_texture_decref(material->diffuse);
  kl_texture_decref(material->normal);
  kl_texture_decref(material->specular);
  kl_texture_decref(material->emissive);
  free(material);
}

static void material_load_raw(char *path, kl_material_t *material) {
  char  buf[256];
  kl_texture_t *diffuse  = kl_texture_incref(path);
  snprintf(buf, 256, "%s_n", path);
  kl_texture_t *normal   = kl_texture_incref(buf);
  snprintf(buf, 256, "%s_s", path);
  kl_texture_t *specular = kl_texture_incref(buf);
  snprintf(buf, 256, "%s_e", path);
  kl_texture_t *emissive = kl_texture_incref(buf);

  strncpy(material->path, path, KL_MATERIAL_PATHLEN);
  material->diffuse  = diffuse;
  material->normal   = normal;
  material->specular = specular;
  material->emissive = emissive;
}

static void material_load_default(char *path, kl_material_t *material) {
  strncpy(material->path, path, KL_MATERIAL_PATHLEN);
  material->diffuse  = kl_texture_incref("DEFAULT_DIFFUSE");
  material->normal   = kl_texture_incref("DEFAULT_NORMAL");
  material->specular = kl_texture_incref("DEFAULT_SPECULAR");
  material->emissive = kl_texture_incref("DEFAULT_EMISSIVE");
}

/* vim: set ts=2 sw=2 et */
