/*
 * Plan 9-style SHA1 wrapper using OpenSSL.
 */

#include <u.h>
#include <libc.h>
#include <openssl/sha.h>

typedef struct P9Sha1State P9Sha1State;
struct P9Sha1State {
	SHA_CTX ctx;
};

uchar*
sha1(uchar *p, int n, uchar *digest, uchar *state)
{
	P9Sha1State *s;

	s = (P9Sha1State*)state;
	if (s == nil) {
		s = mallocz(sizeof(*s), 1);
		if (s == nil)
			return nil;
		SHA1_Init(&s->ctx);
	}

	if (p != nil && n > 0)
		SHA1_Update(&s->ctx, p, n);

	if (digest != nil) {
		SHA1_Final(digest, &s->ctx);
		free(s);
		return digest;
	}

	return (uchar*)s;
}
