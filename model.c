#include "model.h"

#include "model-iqm2.h"
#include "model-obj.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

kl_model_t *kl_model_load(char *path) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "Model: Failed to open %s!\n\tDetails: %s\n", path, strerror(errno));
    return NULL;
  }
  struct stat s;
  if (fstat(fd, &s) < 0) {
    fprintf(stderr, "Model: Failed to get info for %s!\n\tDetails: %s\n", path, strerror(errno));
    return NULL;
  }
  int size = s.st_size;
  void *data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data == MAP_FAILED) {
    fprintf(stderr, "Model: Failed to memory-map %s!\n\tDetails: %s\n", path, strerror(errno));
    return NULL;
  }

  
  kl_model_t *model = NULL;
  if (kl_model_isiqm2(data, size)) {
    model = kl_model_loadiqm2(data, size);
  } else if (kl_model_isobj(data, size)) {
    model = kl_model_loadobj(data, size);
  }


  if (model == NULL) {
    fprintf(stderr, "Model: Failed to load %s!\n", path);
  }
  if (munmap(data, size) < 0) {
    fprintf(stderr, "Model: Failed to unmap %s!\n\tDetails: %s\n", path, strerror(errno));
  }
  if (close(fd) < 0) {
    fprintf(stderr, "Model: Failed to close %s!\n\tDetails: %s\n", path, strerror(errno));
  }
  return model;
}


/* vim: set ts=2 sw=2 et */
