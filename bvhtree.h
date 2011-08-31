#ifndef KL_BVHTREE_H
#define KL_BVHTREE_H

#include "sphere.h"

#define KL_BVH_LEAF   0x01
#define KL_BVH_BRANCH 0x02

typedef struct kl_bvh_header {
  int type;
  kl_sphere_t bounds;
} kl_bvh_header_t;

typedef struct kl_bvh_branch {
  struct kl_bvh_header  header;
  union  kl_bvh_node   *children[2]; /* these should never be NULL */
} kl_bvh_branch_t;

typedef struct kl_bvh_leaf {
  struct kl_bvh_header header;
  void *item;
} kl_bvh_leaf_t;

typedef union kl_bvh_node {
  struct kl_bvh_header header;
  struct kl_bvh_branch branch;
  struct kl_bvh_leaf   leaf;
} kl_bvh_node_t;

typedef int  (*kl_bvh_filter_cb)(kl_sphere_t*, void*);
typedef void (*kl_bvh_result_cb)(void*, void*);

void kl_bvh_insert(kl_bvh_node_t **root, kl_sphere_t *bounds, void *item);
void kl_bvh_search(kl_bvh_node_t *root, kl_bvh_filter_cb filtercb, void *filter_data, kl_bvh_result_cb resultcb, void *result_data);

#endif /* KL_BVHTREE_H */

/* vim: set ts=2 sw=2 et */
