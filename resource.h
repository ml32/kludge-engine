#ifndef KL_RESOURCE_H
#define KL_RESOURCE_H

/* resource cache system */

#include <stdint.h>
#include <stdbool.h>

typedef uint64_t kl_resource_id_t;

typedef void* (*kl_resources_load_cb)(char *path, char *vpath);
typedef void  (*kl_resources_free_cb)(void *item);

typedef struct kl_resource_loader {
  int type;
  kl_resources_load_cb load;
  kl_resources_free_cb free;
} kl_resource_loader_t;

#define KL_RESITEM_PATHLEN 0x100
typedef struct kl_resource_item {
  char path[KL_RESITEM_PATHLEN];
  char vpath[KL_RESITEM_PATHLEN];
  kl_resource_id_t resid;
  int refs;
  kl_resource_loader_t *loader;
  void *item;
  struct kl_resource_item *next;
} kl_resource_item_t;

/* a simple case-insensitive hashing function */
kl_resource_id_t kl_resource_getid(char *str);
/* registers a new resource type */
kl_resource_loader_t* kl_resource_loader_new(kl_resources_load_cb load, kl_resources_free_cb free);
/* self-explanitory... */
bool kl_resource_exists(kl_resource_id_t resid);
/* adds an entry or placeholder */
int kl_resource_add_entry(char *path, char *vpath);
/* adds the contents of a directory resource system */
int kl_resource_add_dir(char *path, char *vpath);
/* increments reference count for existing resource and loads it if necessary */
void *kl_resource_incref(kl_resource_loader_t *loader, kl_resource_id_t resid);
/* decrements refs and unloads resource if it isn't in use */
void kl_resource_decref(kl_resource_id_t resid);
/* for debugging: */
void kl_resource_printall();

#endif /* KL_RESOURCE_H */

/* vim: set ts=2 sw=2 et */
