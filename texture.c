#include "texture.h"

#include "texture-png.h"
#include "resource.h"
#include "renderer.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static kl_texture_t *texture_load(char *path);
static void texture_free(kl_texture_t *texture);
static void texture_load_default_diffuse(char* path, kl_texture_t *texture);
static void texture_load_default_specular(char* path, kl_texture_t *texture);
static void texture_load_default_normal(char* path, kl_texture_t *texture);
static void texture_load_default_emissive(char* path, kl_texture_t *texture);

static kl_resources_t resources = {
  .load = (kl_resources_load_cb)&texture_load,
  .free = (kl_resources_free_cb)&texture_free
};

/* ----------------- */
kl_texture_t *kl_texture_incref(char *path) {
  kl_texture_t *texture = kl_resources_incref(&resources, path);
  if (texture == NULL) return 0;
  return texture;
}

void kl_texture_decref(kl_texture_t *texture) {
  kl_resources_decref(&resources, texture->path);
}

/* ---------------- */
static kl_texture_t *texture_load(char *path) {
  kl_texture_t *texture = malloc(sizeof(kl_texture_t));

  if (strcmp(path, "DEFAULT_DIFFUSE") == 0) {
    texture_load_default_diffuse(path, texture);
    return texture;
  }
  if (strcmp(path, "DEFAULT_SPECULAR") == 0) {
    texture_load_default_specular(path, texture);
    return texture;
  }
  if (strcmp(path, "DEFAULT_NORMAL") == 0) {
    texture_load_default_normal(path, texture);
    return texture;
  }
  if (strcmp(path, "DEFAULT_EMISSIVE") == 0) {
    texture_load_default_emissive(path, texture);
    return texture;
  }

  char buf[256];
  snprintf(buf, 256, "test_assets/%s.png", path);
  kl_texture_loadpng(buf, texture);
  return texture;
}

static void texture_free(kl_texture_t *texture) {
  kl_render_free_texture(texture->id);
  free(texture);
}

static void texture_load_default_diffuse(char *path, kl_texture_t *texture) {
  uint32_t *buf = malloc(0x4000);
  for (int i=0; i < 0x40; i++) {
    for (int j=0; j < 0x40; j++) {
      if ((i & 0x20) ^ (j & 0x20)) {
        buf[i*0x40 + j] = 0xFF00FF40;
      } else {
        buf[i*0x40 + j] = 0xFF202020;
      }
    }
  }
  strncpy(texture->path, path, KL_TEXTURE_PATHLEN);
  texture->w  = 0x40;
  texture->h  = 0x40;
  texture->id = kl_render_upload_texture(buf, 0x40, 0x40, KL_RENDER_RGBA, KL_RENDER_UINT8);
  free(buf);
}

static void texture_load_default_specular(char *path, kl_texture_t *texture) {
  uint32_t *buf = malloc(0x400);
  for (int i=0; i < 0x10; i++) {
    for (int j=0; j < 0x10; j++) {
      buf[i*0x10 + j] = 0x10808080;
    }
  }
  strncpy(texture->path, path, KL_TEXTURE_PATHLEN);
  texture->w  = 0x10;
  texture->h  = 0x10;
  texture->id = kl_render_upload_texture(buf, 0x10, 0x10, KL_RENDER_RGBA, KL_RENDER_UINT8);
  free(buf);
}
  
static void texture_load_default_normal(char *path, kl_texture_t *texture) {
  uint32_t *buf = malloc(0x400);
  for (int i=0; i < 0x10; i++) {
    for (int j=0; j < 0x10; j++) {
      buf[i*0x10 + j] = 0xFFFF8080;
    }
  }
  strncpy(texture->path, path, KL_TEXTURE_PATHLEN);
  texture->w  = 0x10;
  texture->h  = 0x10;
  texture->id = kl_render_upload_texture(buf, 0x10, 0x10, KL_RENDER_RGBA, KL_RENDER_UINT8);
  free(buf);
}

static void texture_load_default_emissive(char *path, kl_texture_t *texture) {
  uint32_t *buf = malloc(0x400);
  for (int i=0; i < 0x10; i++) {
    for (int j=0; j < 0x10; j++) {
      buf[i*0x10 + j] = 0x00000000;
    }
  }
  strncpy(texture->path, path, KL_TEXTURE_PATHLEN);
  texture->w  = 0x10;
  texture->h  = 0x10;
  texture->id = kl_render_upload_texture(buf, 0x10, 0x10, KL_RENDER_RGBA, KL_RENDER_UINT8);
  free(buf);
}

/* vim: set ts=2 sw=2 et */
