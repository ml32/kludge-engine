#include "material-mtl.h"

#include "material.h"
#include "resource.h"
#include "array.h"
#include "strsep.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define MTL_PATHLEN 0x100
typedef struct mtl_entry {
  char path[MTL_PATHLEN];
  char map_diffuse[MTL_PATHLEN];
  char map_specular[MTL_PATHLEN];
  char map_normal[MTL_PATHLEN];
  char map_emissive[MTL_PATHLEN];
} mtl_entry_t;

static void parseline(kl_array_t *entries, mtl_entry_t *entry, char *line);
static void* mtl_load(char *path, char *vpath);
static void mtl_free(void *item);

static kl_resource_loader_t *loader = NULL;

/* -------------- */

kl_array_t *kl_material_mtl_incref(char *path) {
  if (loader == NULL) {
    loader = kl_resource_loader_new((kl_resources_load_cb)&mtl_load, (kl_resources_free_cb)&mtl_free);
  }
  kl_resource_id_t resid = kl_resource_getid(path);
  kl_array_t *entries = kl_resource_incref(loader, resid);
  return entries;
}

void kl_material_mtl_decref(char *path) {
  kl_resource_id_t resid = kl_resource_getid(path);
  kl_resource_decref(resid);
}

bool kl_material_loadfrommtl(kl_array_t *entries, char *vpath, char *entvpath, kl_material_t *material) {
  mtl_entry_t entry;
  /* FIXME: Use hash table implementation (already written for resource manager) for MTL entries */
  for (int i=0; i < kl_array_size(entries); i++) {
    kl_array_get(entries, i, &entry);
    if (strcmp(entry.path, entvpath) == 0) {
      strncpy(material->path, vpath, KL_MATERIAL_PATHLEN);
      kl_texture_t *diffuse  = kl_texture_incref(entry.map_diffuse);
      kl_texture_t *normal   = kl_texture_incref(entry.map_normal);
      kl_texture_t *specular = kl_texture_incref(entry.map_specular);
      kl_texture_t *emissive = kl_texture_incref(entry.map_emissive);
      if (diffuse  == NULL) diffuse  = kl_texture_incref("DEFAULT_DIFFUSE");
      if (normal   == NULL) normal   = kl_texture_incref("DEFAULT_NORMAL");
      if (specular == NULL) specular = kl_texture_incref("DEFAULT_SPECULAR");
      if (emissive == NULL) emissive = kl_texture_incref("DEFAULT_EMISSIVE");
      material->diffuse  = diffuse;
      material->normal   = normal;
      material->specular = specular;
      material->emissive = emissive;
      return true;
    }
  }
  return false;
}

/* ------------------ */
static void parseline(kl_array_t *entries, mtl_entry_t *entry, char *line) {
  while (line[0] == ' ' || line[0] == '\t') { line++; } /* skip leading whitespace */
  char *cur = line;
  char *def;
  def = strsep(&cur, " \t");
  if (strcmp(def, "newmtl") == 0) {
    if (entry->path != '\0') {
      kl_array_push(entries, entry);
    }
    strncpy(entry->path, strsep(&cur, " \t"), MTL_PATHLEN);
    strncpy(entry->map_diffuse,  "DEFAULT_DIFFUSE",  MTL_PATHLEN);
    strncpy(entry->map_normal,   "DEFAULT_NORMAL",   MTL_PATHLEN);
    strncpy(entry->map_specular, "DEFAULT_SPECULAR", MTL_PATHLEN);
    strncpy(entry->map_emissive, "DEFAULT_EMISSIVE", MTL_PATHLEN);
  } else if (strcmp(def, "map_Kd") == 0) {
    snprintf(entry->map_diffuse, MTL_PATHLEN, "/%s", strsep(&cur, " \t"));
  } else if (strcmp(def, "map_Ks") == 0) {
    snprintf(entry->map_specular, MTL_PATHLEN, "/%s", strsep(&cur, " \t"));
  } else if (strcmp(def, "map_bump") == 0) {
    snprintf(entry->map_normal, MTL_PATHLEN, "/%s", strsep(&cur, " \t"));
  } else if (strcmp(def, "bump") == 0) {
    snprintf(entry->map_normal, MTL_PATHLEN, "/%s", strsep(&cur, " \t"));
  } else if (strcmp(def, "map_emissive") == 0) {
    snprintf(entry->map_emissive, MTL_PATHLEN, "/%s", strsep(&cur, " \t"));
  }
}

static void* mtl_load(char *path, char *vpath) {
  static char line[0x400];
  static char ent_vpath[KL_RESITEM_PATHLEN];

  FILE *file = fopen(path, "r");
  if (file == NULL) {
    return NULL;
  }

  kl_array_t *entries = malloc(sizeof(kl_array_t));
  kl_array_init(entries, sizeof(mtl_entry_t));

  mtl_entry_t entry;
  entry.path[0] = '\0';
  while (!feof(file)) {
    for (int i=0; i < 0x400; i++) {
      char c = fgetc(file);
      if (c == '\n' || c == '\r') {
        line[i] = '\0';
        break;
      } else {
        line[i] = c;
      }
    }
    line[0x399] = '\0';
    parseline(entries, &entry, line);
  }
  if (entry.path[0] != '\0') {
    kl_array_push(entries, &entry);
  }
  
  for (int i=0; i < kl_array_size(entries); i++) {
    kl_array_get(entries, i, &entry);
    snprintf(ent_vpath, KL_RESITEM_PATHLEN, "%s|%s", vpath, entry.path);
    kl_resource_add_entry(path, ent_vpath);
  }

  return (void*)entries;
}

static void mtl_free(void *item) {
  kl_array_t *entries = item;
  kl_array_free(entries);
  free(item);
}
  

/* vim: set ts=2 sw=2 et */
