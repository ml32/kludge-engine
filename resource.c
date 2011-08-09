#include "resource.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint32_t hash(char *str) {
  uint32_t h = 0;
  for (int i=0; ; i++) {
    char c = str[i];
    if (c == '\0') return h;
    h ^= c;
    h  = (h << 11) | (h >> 21);
  }
}

void *kl_resources_incref(kl_resources_t *cache, char *path) {
  uint32_t h = hash(path) % KL_RESOURCE_BUCKETS;
  kl_resource_item_t *first = cache->buckets[h];
  kl_resource_item_t *curr;

  for (curr = first; curr != NULL; curr = curr->next) {
    if (strncmp(path, curr->path, KL_RESITEM_PATHLEN) == 0) {
      curr->refs++;
      return curr->item;
    }
  }

  kl_resource_item_t *new = malloc(sizeof(kl_resource_item_t));
  strncpy(new->path, path, KL_RESITEM_PATHLEN);
  new->refs = 1;
  new->item = cache->load(path);
  new->next = first;
  cache->buckets[h] = new;
  
  return new->item;
}

void kl_resources_decref(kl_resources_t *cache, char *path) {
  uint32_t h = hash(path) % KL_RESOURCE_BUCKETS;
  kl_resource_item_t *prev = NULL;
  kl_resource_item_t *curr = cache->buckets[h];

  while (curr != NULL) {
    if (strncmp(path, curr->path, KL_RESITEM_PATHLEN) == 0) {
      curr->refs--;
      if (curr->refs <= 0) {
        if (prev == NULL) {
          cache->buckets[h] = curr->next;
        } else {
          prev->next = curr->next;
        }
        cache->free(curr->item);
        free(curr);
      }
    }
    
    prev = curr;
    curr = curr->next;
  }
}

/* vim: set ts=2 sw=2 et */
