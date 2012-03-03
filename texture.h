#ifndef KL_TEXTURE_H
#define KL_TEXTURE_H

/* sRGB color texture formats */
#define KL_TEXFMT_I    0x01
#define KL_TEXFMT_IA   0x02
#define KL_TEXFMT_RGB  0x03
#define KL_TEXFMT_BGR  0x04
#define KL_TEXFMT_RGBA 0x05
#define KL_TEXFMT_BGRA 0x06
/* linear texture formats */
#define KL_TEXFMT_X    0x11
#define KL_TEXFMT_XY   0x12
#define KL_TEXFMT_XYZ  0x13
#define KL_TEXFMT_XYZW 0x14

#define KL_TEXTURE_PATHLEN 0x100
typedef struct kl_texture_t {
  char path[KL_TEXTURE_PATHLEN];
  unsigned int w, h;
  unsigned int id;
} kl_texture_t;

kl_texture_t *kl_texture_incref(char *path);
void kl_texture_decref(kl_texture_t *texture);

#endif /* KL_TEXTURE_H */
/* vim: set ts=2 sw=2 et */
