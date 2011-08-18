#ifndef KL_BVHTREE_H
#define KL_BVHTREE_H

#define KL_BVH_LEAF   0x01
#define KL_BVH_BRANCH 0x02

typedef struct kl_bvh_bounds {
  kl_vec3f_t center;
  float      radius;
} kl_bvh_bounds_t;

typedef struct kl_bvh_header {
  int type;
  struct kl_bvh_bounds bounds;
} kl_bvh_header_t;

typedef struct kl_bvh_branch {
  struct kl_bvh_header  header;
  struct kl_bvh_node   *children[2]; /* these should never be NULL */
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

void kl_bvh_insert(kl_bvh_node **root, kl_bvh_bounds *bounds, void *item);

#endif /* KL_BVHTREE_H */

/* vim: set ts=2 sw=2 et */
