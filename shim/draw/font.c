#include "shim.h"
#include "config.h"

/*
 * font handling
 */

Font*
openfont(Display *d, char *name)
{
	FontWld *f;
	struct wld_font *wf;
	const char *fontfile;
	FcPattern *pattern;
	double pixelsize;

	if (!wl_state || !wl_state->font_ctx)
		return nil;

	fontfile = ACME_WAYLAND_FONT;
	pixelsize = ACME_WAYLAND_FONT_SIZE;
	pattern = FcPatternCreate();
	if (pattern) {
		FcPatternAddString(pattern, FC_FILE, (const FcChar8 *)fontfile);
		FcPatternAddInteger(pattern, FC_INDEX, 0);
		FcPatternAddDouble(pattern, FC_PIXEL_SIZE, pixelsize);
		FcConfigSubstitute(NULL, pattern, FcMatchPattern);
		FcDefaultSubstitute(pattern);
		wf = wld_font_open_pattern(wl_state->font_ctx, pattern);
		if (debug_wayland) {
			FcChar8 *loaded = nil;
			double px = 0.0;

			if (FcPatternGetString(pattern, FC_FILE, 0, &loaded) == FcResultMatch)
				fprint(2, "wayland-font: file=%s\n", loaded);
			if (FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &px) == FcResultMatch)
				fprint(2, "wayland-font: pixel_size=%.1f\n", px);
		}
		FcPatternDestroy(pattern);
	} else {
		wf = nil;
	}

	if (!wf)
		return nil;

	f = mallocz(sizeof(FontWld), 1);
	f->base.display = d;
	f->base.name = strdup(name ? name : (char *)fontfile);
	f->base.height = wf->height ? wf->height : 13;
	f->base.ascent = wf->ascent ? wf->ascent : 11;
	f->base.width = wf->max_advance ? wf->max_advance : f->base.height;
	f->wld_font = wf;

	return &f->base;
}

void
freefont(Font *f)
{
	FontWld *fw = (FontWld*)f;

	if (fw->wld_font)
		wld_font_close(fw->wld_font);
	if (fw->base.name)
		free(fw->base.name);
	free(fw);
}

Point
stringsize(Font *f, char *s)
{
	FontWld *fw = (FontWld*)f;
	struct wld_extents ext;
	Point p;

	if (!wl_state || !fw->wld_font) {
		p.x = strlen(s) * 7;
		p.y = f->height;
		return p;
	}

	wld_font_text_extents_n(fw->wld_font, s, strlen(s), &ext);

	p.x = ext.advance;
	p.y = f->height;
	return p;
}

int
stringwidth(Font *f, char *s)
{
	return stringsize(f, s).x;
}

static char*
dupbytes(const char *s, int n)
{
	char *buf;

	if (n < 0)
		n = 0;
	buf = malloc(n + 1);
	if (buf == nil)
		return nil;
	if (n > 0)
		memmove(buf, s, n);
	buf[n] = 0;
	return buf;
}

static int
runecount(Rune *r)
{
	int n;

	if (r == nil)
		return 0;
	for (n = 0; r[n] != 0; n++)
		;
	return n;
}

Point
stringn(Image *dst, Point p, Image *src, Point sp, Font *f, char *s, int n)
{
	char *buf;

	buf = dupbytes(s, n);
	if (buf == nil)
		return p;
	p = string(dst, p, src, sp, f, buf);
	free(buf);
	return p;
}

int
stringnwidth(Font *f, char *s, int n)
{
	Point p;
	int len;

	if (s == nil)
		return 0;
	len = strlen(s);
	if (n < 0)
		n = 0;
	if (n > len)
		n = len;
	char *buf = dupbytes(s, n);
	if (buf == nil)
		return 0;
	p = stringsize(f, buf);
	free(buf);
	return p.x;
}

static char*
runes2utf8(Rune *r, int n, int *outlen)
{
	int i;
	int len = 0;
	char *buf;

	if (r == nil || n <= 0) {
		if (outlen)
			*outlen = 0;
		return strdup("");
	}

	buf = malloc(n * UTFmax + 1);
	for (i = 0; i < n && r[i] != 0; i++)
		len += runetochar(buf + len, &r[i]);
	buf[len] = 0;
	if (outlen)
		*outlen = len;
	return buf;
}

int
runestringnwidth(Font *f, Rune *r, int n)
{
	char *buf;
	int width;

	buf = runes2utf8(r, n, nil);
	width = stringsize(f, buf).x;
	free(buf);
	return width;
}

Point
runestring(Image *dst, Point p, Image *src, Point sp, Font *f, Rune *r)
{
	char *buf;
	int n;

	n = runecount(r);
	if (n == 0)
		return p;
	buf = runes2utf8(r, n, nil);
	p = string(dst, p, src, sp, f, buf);
	free(buf);
	return p;
}

Point
runestringn(Image *dst, Point p, Image *src, Point sp, Font *f, Rune *r, int n)
{
	char *buf;

	buf = runes2utf8(r, n, nil);
	p = string(dst, p, src, sp, f, buf);
	free(buf);
	return p;
}

Point
runestringsize(Font *f, Rune *r)
{
	char *buf;
	Point p;
	int n;

	n = runecount(r);
	if (n == 0)
		return Pt(0, f->height);
	buf = runes2utf8(r, n, nil);
	p = stringsize(f, buf);
	free(buf);
	return p;
}

int
runestringwidth(Font *f, Rune *r)
{
	return runestringsize(f, r).x;
}

Point
stringbg(Image *dst, Point p, Image *src, Point sp, Font *f, char *s, Image *bg, Point bgp)
{
	Point size;
	Rectangle r;

	size = stringsize(f, s);
	r = Rect(p.x, p.y, p.x + size.x, p.y + size.y);
	if (bg != nil)
		draw(dst, r, bg, nil, bgp);
	return string(dst, p, src, sp, f, s);
}

Point
stringnbg(Image *dst, Point p, Image *src, Point sp, Font *f, char *s, int n, Image *bg, Point bgp)
{
	int w;
	Rectangle r;

	w = stringnwidth(f, s, n);
	r = Rect(p.x, p.y, p.x + w, p.y + f->height);
	if (bg != nil)
		draw(dst, r, bg, nil, bgp);
	return stringn(dst, p, src, sp, f, s, n);
}
