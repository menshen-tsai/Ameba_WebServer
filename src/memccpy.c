#include <string.h>


void *
__mempcpy (void *dest, const void *src, size_t len)
{
  return (char *)memcpy (dest, src, len) + len;
}

/* Copy no more than N bytes of SRC to DEST, stopping when C is found.
   Return the position in DEST one byte past where C was copied, or
   NULL if C was not found in the first N bytes of SRC.  */
void *
__memccpy (void *dest, const void *src, int c, size_t n)
{
  void *p = memchr (src, c, n);
  if (p != NULL)
    return __mempcpy (dest, src, p - src + 1);
  memcpy (dest, src, n);
  return NULL;
}



