#ifndef KL_ARRAY_H
#define KL_ARRAY_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

typedef struct kl_array {
  uint8_t *data;
  int      size;
  int      item_size;
  int      num_items;
} kl_array_t;

void  kl_array_init(kl_array_t *array, int size);
void  kl_array_clear(kl_array_t *array);
void  kl_array_free(kl_array_t *array);
int   kl_array_push(kl_array_t *array, void *item);

static inline int kl_array_pop(kl_array_t *array, void* item) {
  if (array->num_items <= 0) return -1;
  int bytes = array->item_size;
  memcpy(item, array->data + (--array->num_items) * bytes, bytes);
  return 0;
}

static inline void kl_array_get(kl_array_t *array, int i, void* item) {
  assert(i < array->num_items);
  int bytes = array->item_size;
  memcpy(item, array->data + i * bytes, bytes);
}

static inline void  kl_array_set(kl_array_t *array, int i, void *item) {
  assert(i < array->num_items);
  int bytes = array->item_size;
  memcpy(array->data + i * bytes, item, bytes);
}
void kl_array_set_expand(kl_array_t *array, int i, void *item, uint8_t clearbyte);

static inline void* kl_array_data(kl_array_t *array) {
  return array->data;
}

static inline int kl_array_size(kl_array_t *array) {
  return array->num_items;
}

#endif /* KL_ARRAY_H */

/* vim: set ts=2 sw=2 et */
