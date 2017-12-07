/**
 *author: Anmol Vatsa<anvatsa@cs.utah.edu>
 *common utilities in user and kernel space
 *mostly taken from ulib
 */
#include "types.h"
#include "util.h"

int
atoi(const char *s)
{
  int n;

  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
}


int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}
