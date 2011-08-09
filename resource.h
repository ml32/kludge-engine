#ifndef KL_RESOURCE_H
#define KL_RESOURCE_H

/* resource cache system */

#define KL_RESITEM_PATHLEN 0x100
typedef struct kl_resource_item {
  char  path[KL_RESITEM_PATHLEN];
  int   refs;
  void *item;
  struct kl_resource_item *next;
} kl_resource_item_t;

typedef void* (*kl_resources_load_cb)(char *path);
typedef void  (*kl_resources_free_cb)(void *item);

#define KL_RESOURCE_BUCKETS 0x1000
typedef struct kl_resources {
  kl_resources_load_cb load;
  kl_resources_free_cb free;
  kl_resource_item_t *buckets[KL_RESOURCE_BUCKETS];
} kl_resources_t;

/* increments references for existing resource if it exists or load it if it doesn't */
void *kl_resources_incref(kl_resources_t *cache, char *path);
/* decrements refs and unloads resource if it isn't in use */
void  kl_resources_decref(kl_resources_t *cache, char *path);

#endif /* KL_RESOURCE_H */

/* vim: set ts=2 sw=2 et */
