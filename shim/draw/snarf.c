#include "shim.h"

static char *snarfbuf;

char*
getsnarf(void)
{
	if (snarfbuf == nil)
		return nil;
	return strdup(snarfbuf);
}

void
putsnarf(char *s)
{
	if (snarfbuf) {
		free(snarfbuf);
		snarfbuf = nil;
	}
	if (s)
		snarfbuf = strdup(s);
}
