/*
 * 'shim' is a shitty p9p to wld compat layer
 */

#include "shim.h"

Display *display;
Font *font;
Image *screen;
Screen *_screen;
Point ZP = {0, 0};
Rectangle ZR = {{0, 0}, {0, 0}};
int drawmousemask;
int _cursorfd = -1;
char *winsize;
int debug_wayland;

u32int
p9rgba_to_argb(u32int c)
{
	return (c >> 8) | (c << 24);
}

int
initdraw(void(*error)(Display*, char*), char *fontname, char *label)
{
	Display *d;
	Image *i;
	ImageWld *iw;

	if (debug_wayland == 0 && getenv("ACME_WAYLAND_DEBUG") != nil)
		debug_wayland = 1;

	if (wayland_init(label ? label : "acme", 1024, 768) < 0)
		return -1;

	/* plan 9 display structure */
	d = mallocz(sizeof(Display), 1);
	d->error = error;
	d->bufsize = 8000;
	d->buf = malloc(d->bufsize);
	d->bufp = d->buf;
	d->dpi = DefaultDPI;

	i = mallocz(sizeof(ImageWld), 1);
	i->display = d;
	i->id = 0;
	i->r = Rect(0, 0, wl_state->width, wl_state->height);
	i->clipr = i->r;
	i->chan = wl_state->wld_format == WLD_FORMAT_ARGB8888 ? ARGB32 : XRGB32;
	i->depth = 32;
	i->repl = 0;
	iw = (ImageWld*)i;
	iw->wld_buf = wl_state->wld_buf;

	d->image = i;
	d->screenimage = i;

	/* globals */
	display = d;
	screen = i;

	/* colors */
	d->white = allocimage(d, Rect(0,0,1,1), XRGB32, 1, DWhite);
	d->black = allocimage(d, Rect(0,0,1,1), XRGB32, 1, DBlack);
	d->opaque = allocimage(d, Rect(0,0,1,1), XRGB32, 1, DOpaque);
	d->transparent = allocimage(d, Rect(0,0,1,1), XRGB32, 1, DTransparent);

	d->defaultfont = openfont(d, fontname ? fontname : "*default*");
	font = d->defaultfont;

	wl_state->eventdone = chancreate(sizeof(int), 0);
	proccreate(wayland_event_thread, nil, 64 * 1024);

	return 0;
}

void
closedisplay(Display *d)
{
	if (wl_state) {
		wl_state->running = 0;
		if (wl_state->eventdone) {
			int done;
			recv(wl_state->eventdone, &done);
			chanfree(wl_state->eventdone);
			wl_state->eventdone = nil;
		}

		if (wl_state->renderer)
			wld_destroy_renderer(wl_state->renderer);
		if (wl_state->wld_oldbuf)
			wld_buffer_unreference(wl_state->wld_oldbuf);
		if (wl_state->wld_buf)
			wld_buffer_unreference(wl_state->wld_buf);
		if (wl_state->font_ctx)
			wld_font_destroy_context(wl_state->font_ctx);
		if (wl_state->wld_ctx)
			wld_destroy_context(wl_state->wld_ctx);
		if (wl_state->xdg_toplevel)
			xdg_toplevel_destroy(wl_state->xdg_toplevel);
		if (wl_state->xdg_surface)
			xdg_surface_destroy(wl_state->xdg_surface);
		if (wl_state->wm_base)
			xdg_wm_base_destroy(wl_state->wm_base);
		if (wl_state->wl_display)
			wl_display_disconnect(wl_state->wl_display);

		free(wl_state);
		wl_state = nil;
	}

	if (d) {
		free(d->buf);
		free(d);
	}
}

Image*
allocimage(Display *d, Rectangle r, u32int chan, int repl, u32int val)
{
	ImageWld *img;
	int w, h;

	w = Dx(r);
	h = Dy(r);

	img = mallocz(sizeof(ImageWld), 1);
	img->base.display = d;
	img->base.r = r;
	img->base.clipr = r;
	img->base.chan = chan;
	img->base.depth = 32;
	img->base.repl = repl;

	/* for 1x1 images store as solid color */
	if (w == 1 && h == 1) {
		img->is_solid = 1;
		img->solid_color = val;
	} else if (wl_state && wl_state->wld_ctx) {
		img->wld_buf = wld_create_buffer(wl_state->wld_ctx, w, h,
			wl_state->wld_format, 0);

		if (val != DNofill && img->wld_buf) {
			wld_set_target_buffer(wl_state->renderer, img->wld_buf);
			wld_fill_rectangle(wl_state->renderer, p9rgba_to_argb(val), 0, 0, w, h);
			wld_flush(wl_state->renderer);
		}
	}

	return &img->base;
}

int
freeimage(Image *i)
{
	ImageWld *img = (ImageWld*)i;

	if (img == nil)
		return 0;
	if (img->wld_buf) {
		wld_buffer_unreference(img->wld_buf);
	}

	free(img);
	return 1;
}

int
loadimage(Image *i, Rectangle r, uchar *data, int ndata)
{
	return ndata;  /* todo*/
}

int
cloadimage(Image *i, Rectangle r, uchar *data, int ndata)
{
	return ndata;  /* todo */
}

int
bytesperline(Rectangle r, int depth)
{
	return (Dx(r) * depth + 7) / 8;
}

uchar*
bufimage(Display *d, int n)
{
	return d->buf;
}

int
geninitdraw(char *a, void(*err)(Display*, char*), char *b, char *c, char *d, int e)
{
	return initdraw(err, b, c);
}

void
drawerror(Display *d, char *msg)
{
	fprint(2, "draw error: %s\n", msg);
}
