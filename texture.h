#ifndef KL_TEXTURE_H
#define KL_TEXTURE_H

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
