#include "shim.h"

static char *snarfbuf;

char *getsnarf(void) {
  char *s;

  s = clipboard_get();
  if (s != nil) {
    if (snarfbuf) {
      free(snarfbuf);
      snarfbuf = nil;
    }
    snarfbuf = strdup(s);
    return s;
  }

  if (snarfbuf == nil)
    return nil;
  return strdup(snarfbuf);
}

void putsnarf(char *s) {
  if (snarfbuf) {
    free(snarfbuf);
    snarfbuf = nil;
  }
  if (s && s[0])
    snarfbuf = strdup(s);

  clipboard_put(s);
}
