#include "model.h"

#include "model-iqm2.h"
#include "model-obj.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#endif

kl_model_t *kl_model_load(char *path) {
#ifdef _WIN32
  HANDLE fd   = CreateFile(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (fd == INVALID_HANDLE_VALUE) {
    fprintf(stderr, "Model: Failed to open %s!\n", path);
    return NULL;
  }
  int    size = GetFileSize(fd, NULL);
  HANDLE fm   = CreateFileMapping(fd, NULL, PAGE_READONLY, 0, 0, NULL);
  if (fm == NULL) {
    fprintf(stderr, "Model: Failed to create file mapping for %s!\n", path);
    return NULL;
  }
  void  *data = MapViewOfFile(fm, FILE_MAP_READ, 0, 0, 0);
  if (data == NULL) {
    fprintf(stderr, "Model: Failed to memory-map %s!\n", path);
    return NULL;
  }
#else
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
#endif


  kl_model_t *model = NULL;
  if (kl_model_isiqm2(data, size)) {
    model = kl_model_loadiqm2(data, size);
  } else if (kl_model_isobj(data, size)) {
    model = kl_model_loadobj(data, size);
  }

  if (model == NULL) {
    fprintf(stderr, "Model: Failed to load %s!\n", path);
  }

#ifdef _WIN32
  UnmapViewOfFile(data);
  CloseHandle(fm);
  CloseHandle(fd);
#else
  if (munmap(data, size) < 0) {
    fprintf(stderr, "Model: Failed to unmap %s!\n\tDetails: %s\n", path, strerror(errno));
  }
  if (close(fd) < 0) {
    fprintf(stderr, "Model: Failed to close %s!\n\tDetails: %s\n", path, strerror(errno));
  }
#endif
  return model;
}


/* vim: set ts=2 sw=2 et */
