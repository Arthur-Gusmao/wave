#include "shim.h"
#include "../config.h"

static Rectangle dirtyr;
static int dirtyvalid;

/* track background color */
#define MAX_BG_LINES 256
static struct { int y; int h; u32int color; } bg_lines[MAX_BG_LINES];
static int bg_line_count = 0;

static int last_cleared_y = -1;

static void
record_bg(int y, int h, u32int color)
{
	int i;
	for (i = 0; i < bg_line_count; i++) {
		if (bg_lines[i].y == y && bg_lines[i].h == h) {
			bg_lines[i].color = color;
			return;
		}
	}
	if (bg_line_count < MAX_BG_LINES) {
		bg_lines[bg_line_count].y = y;
		bg_lines[bg_line_count].h = h;
		bg_lines[bg_line_count].color = color;
		bg_line_count++;
	}
}

static u32int
lookup_bg(int y, int h)
{
	int i;
	for (i = 0; i < bg_line_count; i++) {
		if (y >= bg_lines[i].y && y + h <= bg_lines[i].y + bg_lines[i].h)
			return bg_lines[i].color;
	}
	return ACME_TEXT_BACK;
}

static int rectisempty(Rectangle r);
static void markdirty(Rectangle r);

void
paint_invalidate_cache(void)
{
	/* no-op for now cuz wtv */
}

static int
rectisempty(Rectangle r)
{
	return r.min.x >= r.max.x || r.min.y >= r.max.y;
}

static void
markdirty(Rectangle r)
{
	if (rectisempty(r))
		return;
	if (!dirtyvalid) {
		dirtyr = r;
		dirtyvalid = 1;
		return;
	}
	combinerect(&dirtyr, r);
}

void
draw(Image *dst, Rectangle r, Image *src, Image *mask, Point p)
{
	ImageWld *d = (ImageWld*)dst;
	ImageWld *s = (ImageWld*)src;
	struct wld_buffer *srcbuf;
	Rectangle dr, sr;
	int w, h;
	u32int color;

	if (!wl_state || !wl_state->renderer)
		return;

	dr = r;
	if (!rectclip(&dr, dst->clipr))
		return;
	if (dr.min.x != r.min.x || dr.min.y != r.min.y) {
		p.x += dr.min.x - r.min.x;
		p.y += dr.min.y - r.min.y;
	}

	w = Dx(dr);
	h = Dy(dr);
	if (w <= 0 || h <= 0)
		return;

	/* handle draw to screen */
	if (dst == screen || dst == display->screenimage) {
		if (!wl_state->wld_buf ||
			!wld_set_target_buffer(wl_state->renderer, wl_state->wld_buf)) {
			fprint(2, "draw: wld_set_target_buffer failed\n");
			return;
		}
		markdirty(dr);
		if (debug_wayland == 1) {
			fprint(2, "draw: screen r=%d,%d %dx%d\n",
				dr.min.x, dr.min.y, w, h);
			debug_wayland = 2;
		}
	} else if (d->wld_buf) {
		wld_set_target_buffer(wl_state->renderer, d->wld_buf);
	} else {
		return;
	}

	if (s->is_solid) {
		color = p9rgba_to_argb(s->solid_color);
		if (h >= 10 && w >= 200)
			record_bg(dr.min.y, h, s->solid_color);
		/* bounds check against actual bufsize, maybe fix segfaut on reszie */
		if (wl_state->wld_buf) {
			int bw = wl_state->wld_buf->width;
			int bh = wl_state->wld_buf->height;
			if (dr.min.x < 0) { w += dr.min.x; dr.min.x = 0; }
			if (dr.min.y < 0) { h += dr.min.y; dr.min.y = 0; }
			if (dr.min.x + w > bw) w = bw - dr.min.x;
			if (dr.min.y + h > bh) h = bh - dr.min.y;
			if (w <= 0 || h <= 0)
				return;
		}
		wld_fill_rectangle(wl_state->renderer, color, dr.min.x, dr.min.y, w, h);
	} else {
		srcbuf = s->wld_buf;
		if (!srcbuf && (src == screen || src == display->screenimage))
			srcbuf = wl_state->wld_buf;
		if (!srcbuf)
			return;
		sr = Rect(p.x, p.y, p.x + w, p.y + h);
		if (!rectclip(&sr, src->clipr))
			return;
		if (sr.min.x != p.x || sr.min.y != p.y) {
			dr.min.x += sr.min.x - p.x;
			dr.min.y += sr.min.y - p.y;
		}
		w = Dx(sr);
		h = Dy(sr);
		if (w <= 0 || h <= 0)
			return;

		if (!srcbuf)
			return;
		/* bounds check against actual bufsize */
		if (wl_state->wld_buf) {
			int bw = wl_state->wld_buf->width;
			int bh = wl_state->wld_buf->height;
			if (dr.min.x + w > bw) w = bw - dr.min.x;
			if (dr.min.y + h > bh) h = bh - dr.min.y;
			if (dr.min.x < 0 || dr.min.y < 0 || w <= 0 || h <= 0)
				return;
		}
		/* handle overlapping copies!!!!!!!!!!! */
		if (srcbuf == wl_state->wld_buf &&
		    !(sr.min.x >= dr.min.x + w || dr.min.x >= sr.min.x + w ||
		      sr.min.y >= dr.min.y + h || dr.min.y >= sr.min.y + h)) {
			int row;
			/* vert overlap */
			if (sr.min.y != dr.min.y) {
				if (sr.min.y < dr.min.y) {
					for (row = h - 1; row >= 0; row--)
						wld_copy_rectangle(wl_state->renderer, srcbuf,
							dr.min.x, dr.min.y + row, sr.min.x, sr.min.y + row, w, 1);
				} else {
					for (row = 0; row < h; row++)
						wld_copy_rectangle(wl_state->renderer, srcbuf,
							dr.min.x, dr.min.y + row, sr.min.x, sr.min.y + row, w, 1);
				}
			} else if (sr.min.x != dr.min.x) {
				/* horizontal overlap*/
				int col;
				for (row = 0; row < h; row++) {
					if (sr.min.x < dr.min.x) {
						for (col = w - 1; col >= 0; col--)
							wld_copy_rectangle(wl_state->renderer, srcbuf,
								dr.min.x + col, dr.min.y + row,
								sr.min.x + col, sr.min.y + row, 1, 1);
					} else {
						for (col = 0; col < w; col++)
							wld_copy_rectangle(wl_state->renderer, srcbuf,
								dr.min.x + col, dr.min.y + row,
								sr.min.x + col, sr.min.y + row, 1, 1);
					}
				}
			} else {
				wld_copy_rectangle(wl_state->renderer, srcbuf,
					dr.min.x, dr.min.y, sr.min.x, sr.min.y, w, h);
			}
		} else {
			wld_copy_rectangle(wl_state->renderer, srcbuf,
				dr.min.x, dr.min.y, sr.min.x, sr.min.y, w, h);
		}
	}
}

void
fillellipse(Image *dst, Point c, int a, int b, Image *src, Point sp)
{
	ImageWld *s = (ImageWld*)src;
	Rectangle r;

	if (!wl_state || !wl_state->renderer)
		return;

	if (a <= 0 || b <= 0)
		return;

	if (dst == screen || dst == display->screenimage) {
		if (!wl_state->wld_buf ||
			!wld_set_target_buffer(wl_state->renderer, wl_state->wld_buf)) {
			fprint(2, "fillellipse: wld_set_target_buffer failed\n");
			return;
		}
		r = Rect(c.x - a, c.y - b, c.x + a, c.y + b);
		if (rectclip(&r, dst->clipr))
			markdirty(r);
	}

	/*wld has no ellipse, fuck*/
	if (s->is_solid) {
		wld_fill_rectangle(wl_state->renderer, p9rgba_to_argb(s->solid_color),
			c.x - a, c.y - b, a*2, b*2);
	}
}

void
line(Image *dst, Point p0, Point p1, int end0, int end1, int thick, Image *src, Point sp)
{
	ImageWld *s = (ImageWld*)src;
	Rectangle r;

	if (!wl_state || !wl_state->renderer)
		return;

	if (dst == screen || dst == display->screenimage) {
		if (!wl_state->wld_buf ||
			!wld_set_target_buffer(wl_state->renderer, wl_state->wld_buf)) {
			fprint(2, "line: wld_set_target_buffer failed\n");
			return;
		}
		r = Rect(
			p0.x < p1.x ? p0.x : p1.x,
			p0.y < p1.y ? p0.y : p1.y,
			p0.x > p1.x ? p0.x : p1.x,
			p0.y > p1.y ? p0.y : p1.y);
		r.max.x += thick;
		r.max.y += thick;
		if (rectclip(&r, dst->clipr))
			markdirty(r);
	}

	/* draw lines as thin rectangle, hacky, sue me */
	if (s->is_solid) {
		int dx = p1.x - p0.x;
		int dy = p1.y - p0.y;
		int w = dx > 0 ? dx : -dx;
		int h = dy > 0 ? dy : -dy;
		w = w > thick ? w : thick;
		h = h > thick ? h : thick;
		if (w <= 0 || h <= 0)
			return;

		wld_fill_rectangle(wl_state->renderer, p9rgba_to_argb(s->solid_color),
			p0.x, p0.y, w, h);
	}
}

int
flushimage(Display *d, int visible)
{
	Rectangle r;

	if (!wl_state || !wl_state->renderer || !wl_state->surface)
		return -1;

	if (dirtyvalid) {
		r = dirtyr;
		dirtyvalid = 0;
	} else if (screen) {
		r = screen->r;
	} else {
		return 1;
	}
	if (Dx(r) <= 0 || Dy(r) <= 0)
		return 1;

	if (!wl_state->surface)
		return -1;
	wl_surface_damage(wl_state->surface, r.min.x, r.min.y, Dx(r), Dy(r));

	wld_flush(wl_state->renderer);
	if (wl_state->wl_buf && wl_state->surface)
		wl_surface_attach(wl_state->surface, wl_state->wl_buf, 0, 0);
	if (debug_wayland == 2) {
		fprint(2, "flushimage: commit ok\n");
		debug_wayland = 3;
	}
	if (wl_state->surface)
		wl_surface_commit(wl_state->surface);
	if (wl_state->wl_display)
		wl_display_flush(wl_state->wl_display);
	if (wl_state->wld_oldbuf) {
		wld_buffer_unreference(wl_state->wld_oldbuf);
		wl_state->wld_oldbuf = nil;
	}

	/* reset line clear tracking for next frame */
	last_cleared_y = -1;

	return 1;
}

Point
string(Image *dst, Point p, Image *src, Point sp, Font *f, char *s)
{
	FontWld *fw = (FontWld*)f;
	ImageWld *si = (ImageWld*)src;
	struct wld_extents ext;
	Rectangle r;
	u32int color;
	int len;

	if (!wl_state || !wl_state->renderer || !fw->wld_font)
		return p;

	if (dst == screen || dst == display->screenimage) {
		if (!wl_state->wld_buf ||
			!wld_set_target_buffer(wl_state->renderer, wl_state->wld_buf))
			return p;
	}

	color = p9rgba_to_argb(si->is_solid ? si->solid_color : DBlack);
	len = strlen(s);

	wld_font_text_extents_n(fw->wld_font, s, len, &ext);

	if (ext.advance > 0) {
		u32int bg = lookup_bg(p.y, f->height);
		r = Rect(p.x, p.y, p.x + (int)ext.advance, p.y + f->height);
		if (dst == screen || dst == display->screenimage) {
			if (rectclip(&r, dst->clipr) && Dx(r) > 0 && Dy(r) > 0) {
				wld_fill_rectangle(wl_state->renderer, p9rgba_to_argb(bg),
					r.min.x, r.min.y, Dx(r), Dy(r));
			}
		}
	}

	wld_draw_text(wl_state->renderer, fw->wld_font, color,
		p.x, p.y + f->ascent, s, len, &ext);

	if (dst == screen || dst == display->screenimage) {
		r = Rect(p.x, p.y, p.x + (int)ext.advance, p.y + f->height);
		if (rectclip(&r, dst->clipr))
			markdirty(r);
	}

	p.x += ext.advance;
	return p;
}
