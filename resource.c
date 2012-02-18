#define _BSD_SOURCE /* need d_type */
#include <sys/types.h>
#include <dirent.h>

#include "resource.h"

#include <errno.h>

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


#define KL_RESOURCE_BUCKETS 0x1000
typedef struct resources {
  kl_resource_item_t *buckets[KL_RESOURCE_BUCKETS];
} resources_t;
static resources_t resource_cache;

kl_resource_id_t kl_resource_getid(char *str) {
  kl_resource_id_t h = 0;
  char p = '\0';
  for (int i=0; str[i] != '\0'; i++) {
    char c = str[i];
    /* case-insensitivity */
    if (c >= 'A' && c <= 'Z') {
      c |= 0x20;
    }
    /* normalize path separators */
    if (c == '\\') {
      c = '/';
    }
    /* skip duplicate path separators */
    if (c == '/' && p == '/') {
      continue;
    }

    h ^= str[i];
    h  = (h << 11) | (h >> 53); /* shift of 11 bits is coprime to 64 */
  }
  return h;
}

bool kl_resource_exists(kl_resource_id_t resid) {
  kl_resource_id_t bucket = resid % KL_RESOURCE_BUCKETS;
  kl_resource_item_t *first = resource_cache.buckets[bucket];
  kl_resource_item_t *curr;

  for (curr = first; curr != NULL; curr = curr->next) {
    if (curr->resid == resid) {
      return true;
    }
  }
  return false;
}

int kl_resource_add_entry(char *path, char *vpath) {
  kl_resource_id_t resid = kl_resource_getid(vpath);
  if (kl_resource_exists(resid)) return -1; /* possible hash collision */

  int bucket = resid % KL_RESOURCE_BUCKETS;

  kl_resource_item_t *item = malloc(sizeof(kl_resource_item_t));
  strncpy(item->path, path, KL_RESITEM_PATHLEN);
  strncpy(item->vpath, vpath, KL_RESITEM_PATHLEN);
  item->resid  = resid;
  item->refs   = 0;
  item->loader = NULL;
  item->item   = NULL;
  item->next   = resource_cache.buckets[bucket];
  resource_cache.buckets[bucket] = item;
  return 0;
}

int kl_resource_add_dir(char *path, char *vpath) {
  DIR *dir = opendir(path);
  if (dir == NULL) {
    fprintf(stderr, "Resource Manager: Failed to add directory %s!\n\tDetails: %s\n", path, strerror(errno));
    return -1;
  }

  char *subpath   = malloc(KL_RESITEM_PATHLEN);
  char *subvpath  = malloc(KL_RESITEM_PATHLEN);

  int err = 0;

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    snprintf(subpath, KL_RESITEM_PATHLEN, "%s/%s", path, entry->d_name);
    subpath[KL_RESITEM_PATHLEN-1] = '\0';

    snprintf(subvpath, KL_RESITEM_PATHLEN, "%s/%s", vpath, entry->d_name);
    subvpath[KL_RESITEM_PATHLEN-1] = '\0';

    switch (entry->d_type) {
      case DT_DIR:
        if (strcmp(entry->d_name, "..") == 0) break;
        if (strcmp(entry->d_name, ".") == 0) break;

        err = kl_resource_add_dir(subpath, subvpath);
        if (err < 0) goto cleanup;

        break;
      case DT_REG:
        /* strip pipe characters from virtual path (used internally to indicate
         * resources within another resource) */
        for (int i=0; i < 0x100; i++) {
          if (subvpath[i] == '|') {
            subvpath[i] = ':';
          }
        }
        err = kl_resource_add_entry(subpath, subvpath);
        if (err < 0) goto cleanup;
        break;
    }
  }

  cleanup:

  free(subpath);
  free(subvpath);

  closedir(dir);

  return err;
}

kl_resource_loader_t* kl_resource_loader_new(kl_resources_load_cb load, kl_resources_free_cb free) {
  static int type = 0;
  kl_resource_loader_t *loader = malloc(sizeof(kl_resource_loader_t));
  loader->type = type++;
  loader->load = load;
  loader->free = free;
  return loader;
}
 
void *kl_resource_incref(kl_resource_loader_t *loader, kl_resource_id_t resid) {
  kl_resource_id_t bucket = resid % KL_RESOURCE_BUCKETS;
  kl_resource_item_t *first = resource_cache.buckets[bucket];
  kl_resource_item_t *curr;

  for (curr = first; curr != NULL; curr = curr->next) {
    if (curr->resid == resid) {
      if (curr->item == NULL) {
        curr->item   = loader->load(curr->path, curr->vpath);
        curr->loader = loader;
      } else if (curr->loader->type != loader->type) {
        return NULL;
      }
      curr->refs++;
      return curr->item;
    }
  }
  
  return NULL;
}

void kl_resource_decref(kl_resource_id_t resid) {
  int bucket = resid % KL_RESOURCE_BUCKETS;
  kl_resource_item_t *first = resource_cache.buckets[bucket];
  kl_resource_item_t *curr;

  for (curr = first; curr != NULL; curr = curr->next) {
    if (curr->resid == resid) {
      curr->refs--;
      assert(curr->refs >= 0);
      if (curr->refs <= 0 && curr->item != NULL) {
        curr->loader->free(curr->item);
      }
      break;
    }
  }
}

void kl_resource_printall() {
  for (int i=0; i < KL_RESOURCE_BUCKETS; i++) {
    kl_resource_item_t *item = resource_cache.buckets[i];
    while (item != NULL) {
      item = item->next;
    }
  }
}

void kl_resource_strip_extension(char *path) {
  int n = strlen(path);
  for (int i = n-1; i >= 0; i--) {
    if (path[i] == '/' || path[i] == '\\' || path[i] == '|') { /* stop at last path separator */
      break;
    }
    if (path[i] == '.') {
      path[i] = '\0';
      break;
    }
  }
}

/* vim: set ts=2 sw=2 et */
