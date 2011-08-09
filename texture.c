#include "texture.h"

#include "texture-png.h"
#include "resource.h"
#include "renderer.h"

#include <stdlib.h>
#include <stdio.h>

static kl_texture_t *texture_load(char *path) {
  kl_texture_t *texture = malloc(sizeof(kl_texture_t));
  char buf[256];
  snprintf(buf, 256, "test_assets/%s.png", path);
  kl_texture_loadpng(buf, texture);
  return texture;
}

static void texture_free(kl_texture_t *texture) {
  kl_render_free_texture(texture->id);
  free(texture);
}

static kl_resources_t resources = {
  .load = (kl_resources_load_cb)&texture_load,
  .free = (kl_resources_free_cb)&texture_free,
  /* the C standard requires .buckets to be set to zero automatically */
};

kl_texture_t *kl_texture_incref(char *path) {
  kl_texture_t *texture = kl_resources_incref(&resources, path);
  if (texture == NULL) return 0;
  return texture;
}

void kl_texture_decref(kl_texture_t *texture) {
  kl_resources_decref(&resources, texture->path);
}
  
/* vim: set ts=2 sw=2 et */
