#include "bvhtree.h"

#include <stdlib.h>

static float node_dist(kl_bvh_node_t *s1, kl_bvh_node_t *s2);
static kl_bvh_leaf_t* leaf_new(kl_sphere_t *bounds, void *item);
static kl_bvh_node_t* leaf_insert(kl_bvh_node_t *curr, kl_bvh_leaf_t *leaf);
static kl_bvh_branch_t* branch_new(kl_bvh_node_t *left, kl_bvh_node_t *right);

/* -------------------------- */

void kl_bvh_insert(kl_bvh_node_t **root, kl_sphere_t *bounds, void *item) {
  kl_bvh_leaf_t *leaf = leaf_new(bounds, item);
  *root = leaf_insert(*root, leaf); 
}

void kl_bvh_search(kl_bvh_node_t *root, kl_bvh_filter_cb filtercb, void *filter_data, kl_array_t *results) {
  if (root == NULL) return;
  
  if (!filtercb(&root->header.bounds, filter_data)) return;

  switch (root->header.type) {
    case KL_BVH_LEAF:
      kl_array_push(results, &root->leaf.item);
      return;
    case KL_BVH_BRANCH:
      kl_bvh_search(root->branch.children[0], filtercb, filter_data, results);
      kl_bvh_search(root->branch.children[1], filtercb, filter_data, results);
      return;
  }
}

void kl_bvh_debug(kl_bvh_node_t* root, kl_array_t *results) {
  if (root == NULL) return;
  
  kl_array_push(results, &root);

  if (root->header.type == KL_BVH_BRANCH) {
      kl_bvh_debug(root->branch.children[0], results);
      kl_bvh_debug(root->branch.children[1], results);
  }
}


/* --------------------------- */
 

static float node_dist(kl_bvh_node_t *s1, kl_bvh_node_t *s2) {
  return kl_vec3f_dist(&s1->header.bounds.center, &s2->header.bounds.center);
}

static kl_bvh_leaf_t* leaf_new(kl_sphere_t *bounds, void *item) {
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

static kl_bvh_node_t* leaf_insert(kl_bvh_node_t *curr, kl_bvh_leaf_t *leaf) {
  if (curr == NULL) return (kl_bvh_node_t*)leaf;

  kl_bvh_node_t *l;
  kl_bvh_node_t *r;
  switch (curr->header.type) {
    case KL_BVH_LEAF:
      return (kl_bvh_node_t*)branch_new(curr, (kl_bvh_node_t*)leaf);
    case KL_BVH_BRANCH:
      l = curr->branch.children[0];
      r = curr->branch.children[1];

      float dl = node_dist((kl_bvh_node_t*)leaf, l);
      float dr = node_dist((kl_bvh_node_t*)leaf, r);
      if (dl < dr) {
        l = leaf_insert(l, leaf);
      } else {
        r = leaf_insert(r, leaf);
      }

      curr->branch.children[0] = l;
      curr->branch.children[1] = r;
      kl_sphere_merge(&curr->header.bounds, &l->header.bounds, &r->header.bounds);

      return curr;
  }
  return NULL; /* should never be reached */ 
}

static kl_bvh_branch_t* branch_new(kl_bvh_node_t *left, kl_bvh_node_t *right) {
  kl_bvh_branch_t *branch = malloc(sizeof(kl_bvh_branch_t));

  kl_sphere_t bounds;
  kl_sphere_merge(&bounds, &left->header.bounds, &right->header.bounds);

  *branch = (kl_bvh_branch_t){
    .header = {
      .type   = KL_BVH_BRANCH,
      .bounds = bounds
    },
    .children = { left, right }
  };

  return branch;
}


/* vim: set ts=2 sw=2 et */
