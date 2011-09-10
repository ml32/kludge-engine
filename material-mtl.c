#define _BSD_SOURCE /* need strsep() */

#include "material-mtl.h"

#include "material.h"
#include "array.h"

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

static void parseline(mtl_entry_t *entry, char *line);

static int loaded = 0;
static kl_array_t entries;

/* -------------- */
void kl_material_setmtl(char *path) {
  if (loaded) {
    kl_array_clear(&entries);
  } else {
    kl_array_init(&entries, sizeof(mtl_entry_t));
  }

  char buf[256];
  snprintf(buf, 256, "test_assets/%s", path);
  FILE *file = fopen(buf, "r");

  mtl_entry_t entry;
  entry.path[0] = '\0';
  char line[0x400];
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
    parseline(&entry, line);
  }
  if (entry.path != '\0') {
    kl_array_push(&entries, &entry);
  }

  loaded = 1;
}

int kl_material_loadfrommtl(char *path, kl_material_t *material) {
  if (!loaded) return 0;
  mtl_entry_t entry;
  for (int i=0; i < kl_array_size(&entries); i++) {
    kl_array_get(&entries, i, &entry);
    if (strcmp(entry.path, path) == 0) {
      strncpy(material->path, path, KL_MATERIAL_PATHLEN);
      material->diffuse  = kl_texture_incref(entry.map_diffuse);
      material->normal   = kl_texture_incref(entry.map_normal);
      material->specular = kl_texture_incref(entry.map_specular);
      material->emissive = kl_texture_incref(entry.map_emissive);
      return 1;
    }
  }
  return 0;
}

/* ------------------ */
static void parseline(mtl_entry_t *entry, char *line) {
  while (line[0] == ' ' || line[0] == '\t') { line++; } /* skip leading whitespace */
  char *cur = line;
  char *def;
  def = strsep(&cur, " \t");
  if (strcmp(def, "newmtl") == 0) {
    if (entry->path != '\0') {
      kl_array_push(&entries, entry);
    }
    strncpy(entry->path, strsep(&cur, " \t"), MTL_PATHLEN);
    strncpy(entry->map_diffuse,  "DEFAULT_DIFFUSE",  MTL_PATHLEN);
    strncpy(entry->map_normal,   "DEFAULT_NORMAL",   MTL_PATHLEN);
    strncpy(entry->map_specular, "DEFAULT_SPECULAR", MTL_PATHLEN);
    strncpy(entry->map_emissive, "DEFAULT_EMISSIVE", MTL_PATHLEN);
  } else if (strcmp(def, "map_Kd") == 0) {
    strncpy(entry->map_diffuse, strsep(&cur, " \t"), MTL_PATHLEN);
  } else if (strcmp(def, "map_Ks") == 0) {
    strncpy(entry->map_specular, strsep(&cur, " \t"), MTL_PATHLEN);
  } else if (strcmp(def, "map_bump") == 0) {
    strncpy(entry->map_normal, strsep(&cur, " \t"), MTL_PATHLEN);
  } else if (strcmp(def, "bump") == 0) {
    strncpy(entry->map_normal, strsep(&cur, " \t"), MTL_PATHLEN);
  } else if (strcmp(def, "map_emissive") == 0) {
    strncpy(entry->map_emissive, strsep(&cur, " \t"), MTL_PATHLEN);
  }
}

/* vim: set ts=2 sw=2 et */
