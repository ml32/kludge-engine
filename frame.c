#include "frame.h"

#include "vec.h"

#include <stdint.h>

/* DRY -- this same hashing routine is used in resource.c */
static uint32_t hash(char *str) {
  uint32_t h = 0;
  for (int i=0; ; i++) {
    char c = str[i];
    if (c == '\0') return h;
    h ^= c;
    h  = (h << 11) | (h >> 21);
  }
}

kl_frame_t* kl_frame_new(char *id, kl_frame_coord_t *preferred_size, kl_frame_anchor_t *anchor_primary, kl_frame_anchor_t *anchor_secondary) {
  kl_frame_t *frame = malloc(sizeof(kl_frame_t));

  kl_frame_coord_t coord_none = {
    .type = KL_FRAME_COORD_NORMALIZED,
    .x = 0.0f,
    .y = 0.0f
  };
  kl_frame_anchor_t anchor_none = {
    .point = KL_FRAME_ANCHOR_UNDEFINED,
    .target_id = 0,
    .target_point = KL_FRAME_ANCHOR_UNDEFINED,
    .offset = coord_none
  };

  kl_frame_header_t *header = &frame->header;

  header->type   = KL_FRAME_TYPE_NONE;
  header->id     = hash(id);
  header->hidden = 0;
  if (preferred_size != NULL) {
    header->preferred_size = *preferred_size;
  } else {
    header->preferred_size = coord_none;
  }
  if (anchor_primary != NULL) {
    header->anchor_primary   = *anchor_primary;
    if (anchor_secondary != NULL) {
      header->anchor_secondary = *anchor_secondary;
    } else {
      header->anchor_secondary = anchor_none;
    }
  } else {
    header->anchor_primary   = anchor_none;
    header->anchor_secondary = anchor_none;
  }
  header->effective_position.x = 0.0f;
  header->effective_position.y = 0.0f;
  header->effective_size.x = 1.0f;
  header->effective_size.y = 1.0f;

  kl_array_init(&header->children, sizeof(kl_frame_t*));

  return frame;
}

void kl_frame_delete(kl_frame_t *frame) {
  kl_frame_header_t *header = &frame->header;

  kl_frame_t *child;
  for (int i=0; i < kl_array_size(&header->children); i++) {
    kl_array_get(&header->children, i, &child);
    kl_frame_delete(child);
  }
  kl_array_free(&header->children);
  
  switch (header->type) {
    case KL_FRAME_TYPE_GRAPHIC:
      if (frame->graphic.material != NULL) {
        kl_material_decref(frame->graphic.material);
      }
      break;
    case KL_FRAME_TYPE_TEXT:
      if (frame->text.str != NULL) {
        free(frame->text.str);
      }
      break;
  }

  free(frame);
}

void kl_frame_add(kl_frame_t *frame, kl_frame_t *child) {
  kl_array_push(&frame->header.children, &child);
}

/* point in normalized local frame coordinates */
static void anchor_point(int point, kl_vec2f_t *result) {
  switch (point & KL_FRAME_ANCHOR_HMASK) {
    case KL_FRAME_ANCHOR_LEFT:
      result->x = 0.0f;
      break;
    case KL_FRAME_ANCHOR_RIGHT:
      result->x = 1.0f;
      break;
    default:
      result->x = 0.5f;
      break;
  }
    
  switch (point & KL_FRAME_ANCHOR_VMASK) {
    case KL_FRAME_ANCHOR_BOTTOM:
      result->y = 0.0f;
      break;
    case KL_FRAME_ANCHOR_TOP:
      result->y = 1.0f;
      break;
    default:
      result->y = 0.5f;
      break;
  }
}

static void coord_normalize(kl_frame_coord_t *coord, kl_vec2f_t *result, int screen_w, int screen_h) {
  switch (coord->type) {
    case KL_FRAME_COORD_PIXELS:
      result->x = coord->x / (float)screen_w;
      result->y = coord->y / (float)screen_h;
      break;
    case KL_FRAME_COORD_NORMALIZED:
    default:
      result->x = coord->x;
      result->y = coord->y;
      break;
  }
}

static kl_frame_t* frame_lookup(kl_frame_t *parent, uint32_t id) {
  if (id == 0) return parent;

  kl_frame_t *child;
  for (int i=0; i < kl_array_size(&parent->header.children); i++) {
    kl_array_get(&parent->header.children, i, &child);
    if (child->header.id == id) {
      return child;
    }
  }
  return NULL;
}

void kl_frame_update(kl_frame_t *frame, kl_frame_t *parent, int screen_w, int screen_h) {
  kl_frame_header_t *header = &frame->header;
  if (header->anchor_primary.point == KL_FRAME_ANCHOR_UNDEFINED) return;
  if (header->hidden) return;

  kl_frame_anchor_t *anchor_primary   = &header->anchor_primary;
  kl_frame_anchor_t *anchor_secondary = &header->anchor_secondary;

  kl_vec2f_t preferred_size;
  coord_normalize(&header->preferred_size, &preferred_size, screen_w, screen_h);

  /* get normalized coords for primary target and local anchor points */
  kl_frame_t *primary_target = frame_lookup(parent, anchor_primary->target_id);

  kl_vec2f_t primary_target_point;
  anchor_point(anchor_primary->target_point, &primary_target_point);
  kl_vec2f_mul(&primary_target_point, &primary_target_point, &primary_target->header.effective_size);
  kl_vec2f_add(&primary_target_point, &primary_target_point, &primary_target->header.effective_position);
  kl_vec2f_t primary_offset;
  coord_normalize(&anchor_primary->offset, &primary_offset, screen_w, screen_h);
  kl_vec2f_add(&primary_target_point, &primary_target_point, &primary_offset);

  kl_vec2f_t primary_local_point;
  anchor_point(anchor_primary->point, &primary_local_point);
  
  /* determine size */
  kl_vec2f_t size, position;
  if (anchor_secondary->point == KL_FRAME_ANCHOR_UNDEFINED) { /* one anchor point */
    size = preferred_size;
  } else { /* two anchor points */

    /* get normalized coords for secondary target and local anchor points */
    kl_frame_t *secondary_target = frame_lookup(parent, anchor_secondary->target_id);

    kl_vec2f_t secondary_target_point;
    anchor_point(anchor_secondary->target_point, &secondary_target_point);
    kl_vec2f_mul(&secondary_target_point, &secondary_target_point, &secondary_target->header.effective_size);
    kl_vec2f_add(&secondary_target_point, &secondary_target_point, &secondary_target->header.effective_position);
    kl_vec2f_t secondary_offset;
    coord_normalize(&anchor_secondary->offset, &secondary_offset, screen_w, screen_h);
    kl_vec2f_add(&secondary_target_point, &secondary_target_point, &secondary_offset);
  
    kl_vec2f_t secondary_local_point;
    anchor_point(anchor_secondary->point, &secondary_local_point);
    
    /* calculate distances */
    kl_vec2f_t target_dist;
    kl_vec2f_sub(&target_dist, &primary_target_point, &secondary_target_point);
    kl_vec2f_t local_dist;
    kl_vec2f_sub(&local_dist, &primary_local_point, &secondary_local_point);

    /* determine size */
    if ((anchor_primary->target_point   & KL_FRAME_ANCHOR_HMASK) ==
        (anchor_secondary->target_point & KL_FRAME_ANCHOR_HMASK))
    {
      size.x = preferred_size.x;
    } else {
      size.x = target_dist.x / local_dist.x;
    }

    if ((anchor_primary->target_point   & KL_FRAME_ANCHOR_VMASK) ==
        (anchor_secondary->target_point & KL_FRAME_ANCHOR_VMASK))
    {
      size.y = preferred_size.y;
    } else {
      size.y = target_dist.y / local_dist.y;
    }
  }

  /* determine position */
  kl_vec2f_mul(&position, &size, &primary_local_point);
  kl_vec2f_sub(&position, &primary_target_point, &position);

  /* update children */
  kl_frame_t *child;
  for (int i=0; i < kl_array_size(&header->children); i++) {
    kl_array_get(&header->children, i, &child);
    kl_frame_update(child, frame, screen_w, screen_h);
  }
}
/* vim: set ts=2 sw=2 et */
