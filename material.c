#include "material.h"

#include "material-mtl.h"
#include "resource.h"
#include "texture.h"
#include "array.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static kl_material_t* material_load(char *path, char *vpath);
static void material_free(kl_material_t *material);
static void material_load_raw(char *path, kl_material_t *material);
static void material_load_default(char *path, kl_material_t *material);
static bool path_split(char *vpath, char **mtlvpath, char **entvpath); /* this is destructive, like strtok */

static kl_resource_loader_t *loader = NULL;
static kl_array_t *mtl_entries = NULL;

/* --------------- */
kl_material_t *kl_material_incref(char *path) {
  if (loader == NULL) {
    loader = kl_resource_loader_new((kl_resources_load_cb)&material_load, (kl_resources_free_cb)&material_free);
    kl_resource_add_entry("", "DEFAULT_MATERIAL");
  }

  /* MTL must be loaded prior to loading materials */
  static char buf[KL_RESITEM_PATHLEN];
  strncpy(buf, path, KL_RESITEM_PATHLEN);
  buf[KL_RESITEM_PATHLEN-1] = '\0';
  char *mtlvpath;
  char *entvpath;
  if (path_split(buf, &mtlvpath, &entvpath)) {
    mtl_entries = kl_material_mtl_incref(mtlvpath);
  } else {
    mtl_entries = NULL;
  }

  kl_resource_id_t resid = kl_resource_getid(path);
  kl_material_t *material = kl_resource_incref(loader, resid);
  if (mtl_entries != NULL && material == NULL) {
    /* this causes a double-free, why? */
    //kl_material_mtl_decref(mtlvpath);
  }
  return material;
}

void kl_material_decref(kl_material_t *material) {
  static char buf[KL_RESITEM_PATHLEN];
  strncpy(buf, material->path, KL_RESITEM_PATHLEN);
  buf[KL_RESITEM_PATHLEN-1] = '\0';
  char *mtlvpath;
  char *entvpath;
  if (path_split(buf, &mtlvpath, &entvpath)) {
    kl_material_mtl_decref(mtlvpath);
  }

  kl_resource_id_t resid = kl_resource_getid(material->path);
  kl_resource_decref(resid);
}

/* --------------- */
static bool path_split(char *vpath, char **mtlvpath, char **entvpath) {
  *mtlvpath = vpath;
  *entvpath = NULL;
  for (int i=0; vpath[i] != '\0'; i++) {
    if (vpath[i] == '|') {
      vpath[i] = '\0';
      *entvpath = &vpath[i+1];
      break;
    }
  }
  if (*entvpath == NULL) return false;
  return true;
}

static kl_material_t *material_load(char *path, char *vpath) {
  kl_material_t *material = malloc(sizeof(kl_material_t));

  if (strcmp(vpath, "DEFAULT_MATERIAL") == 0) {
    material_load_default(vpath, material);
    return material;
  }
  if (mtl_entries != NULL) {
    static char buf[KL_RESITEM_PATHLEN];
    strncpy(buf, vpath, KL_RESITEM_PATHLEN);
    buf[KL_RESITEM_PATHLEN-1] = '\0';
    char *mtlvpath;
    char *entvpath;
    path_split(buf, &mtlvpath, &entvpath);
    if (kl_material_loadfrommtl(mtl_entries, vpath, entvpath, material)) {
      return material;
    }
  }
  material_load_raw(vpath, material);
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
  snprintf(buf, 256, "%s", path);
  kl_texture_t *diffuse  = kl_texture_incref(buf);
  snprintf(buf, 256, "%s_n", path);
  kl_texture_t *normal   = kl_texture_incref(buf);
  snprintf(buf, 256, "%s_s", path);
  kl_texture_t *specular = kl_texture_incref(buf);
  snprintf(buf, 256, "%s_e", path);
  kl_texture_t *emissive = kl_texture_incref(buf);

  if (diffuse == NULL) {
    diffuse  = kl_texture_incref("DEFAULT_DIFFUSE");
  }
  if (normal == NULL) {
    normal   = kl_texture_incref("DEFAULT_NORMAL");
  }
  if (specular == NULL) {
    specular = kl_texture_incref("DEFAULT_SPECULAR");
  }
  if (emissive == NULL) {
    emissive = kl_texture_incref("DEFAULT_EMISSIVE");
  }
  assert(diffuse != NULL);
  assert(normal != NULL);
  assert(specular != NULL);
  assert(emissive != NULL);

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
  assert(material->diffuse != NULL);
  assert(material->normal != NULL);
  assert(material->specular != NULL);
  assert(material->emissive != NULL);
}

/* vim: set ts=2 sw=2 et */
