#include "shim.h"

/*
 * wm stubs
 */

Screen*
allocscreen(Image *image, Image *fill, int refresh)
{
	Screen *s;

	s = mallocz(sizeof(Screen), 1);
	s->display = image->display;
	s->image = image;
	s->fill = fill;

	return s;
}

int
freescreen(Screen *s)
{
	free(s);
	return 1;
}

Image*
allocwindow(Screen *s, Rectangle r, int ref, u32int val)
{
	return allocimage(s->display, r, XRGB32, 0, val);
}

int
getwindow(Display *d, int ref)
{
	Rectangle r;

	USED(ref);
	if (wl_state == nil || screen == nil || d == nil)
		return -1;
	if (Dx(screen->r) != wl_state->width || Dy(screen->r) != wl_state->height) {
		if (wayland_resize_surface(wl_state->width, wl_state->height) < 0)
			return -1;
	}
	r = Rect(0, 0, wl_state->width, wl_state->height);
	screen->r = r;
	screen->clipr = r;
	d->screenimage = screen;
	return 1;
}

int
gengetwindow(Display *d, char *label, Image **img, Screen **scr, int ref)
{
	USED(label);
	if (img)
		*img = screen;
	if (scr)
		*scr = _screen;
	return getwindow(d, ref);
}

int
scalesize(Display *d, int n)
{
	if (d == nil || d->dpi == 0)
		return n;
	return (n * d->dpi + DefaultDPI / 2) / DefaultDPI;
}

void
drawtopwindow(void)
{
}

int
mousescrollsize(int maxlines)
{
	static int lines, pcnt;
	char *mss;

	if (lines == 0 && pcnt == 0) {
		mss = getenv("mousescrollsize");
		if (mss) {
			if (strchr(mss, '%') != nil)
				pcnt = atof(mss);
			else
				lines = atoi(mss);
			free(mss);
		}
		if (lines == 0 && pcnt == 0)
			lines = 1;
		if (pcnt >= 100)
			pcnt = 100;
	}

	if (lines)
		return lines;
	return pcnt * maxlines / 100.0;
}
