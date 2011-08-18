#include "bvhtree.h"

static void bounds_merge(kl_bvh_bounds_t *dst, kl_bvh_bounds_t *s1, kl_bvh_bounds_t *s2) {
  kl_vec3f_t temp, dir, max1, max2, center;
  float radius;

  kl_vec3f_sub(&dir, &s2->center, &s1->center);
  kl_vec3f_norm(&dir, &dir);
  
  kl_vec3f_scale(&temp, &dir, s1->radius);
  kl_vec3f_sub(&max1, &s1->center, &temp);

  kl_vec3f_scale(&temp, &dir, s2->radius);
  kl_vec3f_add(&max2, &s2->center, &temp);

  kl_vec3f_add(&center, &max1, &max2);
  kl_vec3f_scale(&center, &center, 0.5f);

  kl_vec3f_sub(&temp, &max2, &max1);
  radius = kl_vec3f_magnitude(&temp) / 2.0f;

  *dst = (kl_bvh_bounds_t){
    .center = center,
    .radius = radius
  }
} 

static float node_dist(kl_bvh_node_t *s1, kl_bvh_node_t *s2) {
  return kl_vec3f_dist(&s1->header.center, &s2->header.center);
}

static kl_bvh_leaf_t* leaf_new(kl_bvh_bounds_t *bounds, void *item) {
  kl_bvh_leaf_t *leaf = malloc(sizeof(kl_bvh_leaf_t));
  *leaf = (kl_bvh_leaf_t){
    .header = {
      .type   = KL_BVH_LEAF,
      .bounds = *bounds
    },
    .item = item,
  };

  return leaf;
}

static kl_bvh_branch_t* branch_new(kl_bvh_node_t *left, kl_bvh_node_t *right) {
  kl_bvh_branch_t *branch = malloc(sizeof(kl_bvh_branch_t));

  kl_bvh_bounds_t bounds;
  bounds_merge(&bounds, &left->header.bounds, &right->header.bounds);

  *branch = (kl_bvh_branch_t){
    .header = {
      .type   = KL_BVH_BRANCH,
      .bounds = bounds
    },
    .children = { left, right }
  };

  return branch;
}

static kl_bvh_node_t* leaf_insert(kl_bvh_node_t *curr, kl_bvh_leaf_t *leaf) {
  if (curr == NULL) return leaf;

  kl_bvh_node_t *l;
  kl_bvh_node_t *r;
  switch (curr->header.type) {
    case KL_BVH_LEAF:
      return branch_new(*curr, leaf);
    case KL_BVH_BRANCH:
      l = curr->branch.children[0];
      r = curr->branch.children[1];

      float dl = node_dist(leaf, l);
      float dr = node_dist(leaf, r);
      if (dl < dr) {
        l = leaf_insert(l, leaf);
      } else {
        r = leaf_insert(r, leaf);
      }

      curr->branch.children[0] = l;
      curr->branch.children[1] = r;
      bounds_merge(&curr->header.bounds, &l->header.bounds, &r->header.bounds);

      return curr;
  }
}

void kl_bvh_insert(kl_bvh_node_t **root, kl_bvh_bounds_t *bounds, void *item) {
  kl_bvh_leaf_t *leaf = leaf_new(bounds, item);
  *root = leaf_insert(root, leaf); 
}

/* vim: set ts=2 sw=2 et */
