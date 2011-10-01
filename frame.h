#ifndef KL_FRAME_H
#define KL_FRAME_H

#include "array.h"
#include "vec.h"
#include "material.h"

#include <stdint.h>

#define KL_FRAME_COORD_NORMALIZED 0x01
#define KL_FRAME_COORD_PIXELS     0x02

typedef struct kl_frame_coord {
  int   type;
  float x, y;
} kl_frame_coord_t;

#define KL_FRAME_ANCHOR_UNDEFINED 0xFF
#define KL_FRAME_ANCHOR_HMASK     0x0F
#define KL_FRAME_ANCHOR_VMASK     0xF0
#define KL_FRAME_ANCHOR_CENTER    0x00
#define KL_FRAME_ANCHOR_LEFT      0x01
#define KL_FRAME_ANCHOR_RIGHT     0x02
#define KL_FRAME_ANCHOR_BOTTOM    0x10
#define KL_FRAME_ANCHOR_TOP       0x20

typedef struct kl_frame_anchor {
  int point;          /* anchor point on this frame */
  uint32_t target_id; /* frame this anchor is relative to -- must be either zero for parent frame, or the id of a sibling */
  int target_point;   /* anchor point on target frame */
  kl_frame_coord_t offset;
} kl_frame_anchor_t;

#define KL_FRAME_TYPE_NONE    0x00
#define KL_FRAME_TYPE_GRAPHIC 0x01
#define KL_FRAME_TYPE_TEXT    0x02

typedef struct kl_frame_header {
  int type;
  int hidden;
  uint32_t id;

  kl_frame_coord_t  preferred_size;   /* size used when only one anchor is defined */
  kl_frame_anchor_t anchor_primary;   /* primary anchor used to determine position */
  kl_frame_anchor_t anchor_secondary; /* secondary anchor, overrides preferred size */

  /* effective position and size are in normalized device coordinates, and are modified by the 'update' routine */
  kl_vec2f_t effective_position;
  kl_vec2f_t effective_size;

  kl_array_t children;
} kl_frame_header_t;

typedef struct kl_frame_graphic {
  kl_frame_header_t header;
  kl_material_t *material;
} kl_frame_graphic_t;

typedef struct kl_frame_text {
  kl_frame_header_t header;
  int   n;
  char *str;
} kl_frame_text_t;

typedef union kl_frame {
  kl_frame_header_t  header;
  kl_frame_graphic_t graphic;
  kl_frame_text_t    text;
} kl_frame_t;

kl_frame_t* kl_frame_new(char *id, kl_frame_coord_t *preferred_size, kl_frame_anchor_t *anchor_primary, kl_frame_anchor_t *anchor_secondary);
void kl_frame_delete(kl_frame_t *frame);
void kl_frame_add(kl_frame_t *frame, kl_frame_t *child);
void kl_frame_update(kl_frame_t *frame, kl_frame_t *parent, int screen_w, int screen_h);

#endif /* KL_FRAME_H */
/* vim: set ts=2 sw=2 et */
