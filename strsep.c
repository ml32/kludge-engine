#include "strsep.h"

#include <stdlib.h>

char* strsep(char **stringp, const char *delim) {
  if (*stringp == NULL) return NULL;
  char *next = *stringp;
  char *s    = *stringp;
  while (*s != '\0') {
    for (char *d = (char*)delim; *d != '\0'; d++) {
      if (*s == *d) {
        *s = '\0';
        *stringp = ++s;
        return next;
      }
    }
    *stringp = ++s;
  }
  *stringp = NULL;
  return next;
}
