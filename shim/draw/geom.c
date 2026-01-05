#include "shim.h"

/*
 * geom helpers
 */

int
drawreplxy(int min, int max, int x)
{
	int sx;

	sx = (x-min)%(max-min);
	if(sx < 0)
		sx += max-min;
	return sx+min;
}

Point
drawrepl(Rectangle r, Point p)
{
	p.x = drawreplxy(r.min.x, r.max.x, p.x);
	p.y = drawreplxy(r.min.y, r.max.y, p.y);
	return p;
}

Point Pt(int x, int y) { return (Point){x, y}; }
Rectangle Rect(int x0, int y0, int x1, int y1) { return (Rectangle){Pt(x0, y0), Pt(x1, y1)}; }
Rectangle Rpt(Point min, Point max) { return (Rectangle){min, max}; }
Point addpt(Point a, Point b) { return Pt(a.x+b.x, a.y+b.y); }
Point subpt(Point a, Point b) { return Pt(a.x-b.x, a.y-b.y); }
Point divpt(Point a, int b) { return b ? Pt(a.x/b, a.y/b) : Pt(0, 0); }
Point mulpt(Point a, int b) { return Pt(a.x*b, a.y*b); }
int eqpt(Point a, Point b) { return a.x==b.x && a.y==b.y; }
int eqrect(Rectangle a, Rectangle b) { return eqpt(a.min, b.min) && eqpt(a.max, b.max); }
Rectangle insetrect(Rectangle r, int n) { r.min.x+=n; r.min.y+=n; r.max.x-=n; r.max.y-=n; return r; }
Rectangle rectaddpt(Rectangle r, Point p) { return (Rectangle){addpt(r.min, p), addpt(r.max, p)}; }
Rectangle rectsubpt(Rectangle r, Point p) { return (Rectangle){subpt(r.min, p), subpt(r.max, p)}; }
Rectangle canonrect(Rectangle r)
{
	int t;

	if (r.max.x < r.min.x) { t = r.min.x; r.min.x = r.max.x; r.max.x = t; }
	if (r.max.y < r.min.y) { t = r.min.y; r.min.y = r.max.y; r.max.y = t; }
	return r;
}
int rectXrect(Rectangle a, Rectangle b) { return a.min.x < b.max.x && a.max.x > b.min.x && a.min.y < b.max.y && a.max.y > b.min.y; }
int rectinrect(Rectangle a, Rectangle b) { return a.min.x >= b.min.x && a.min.y >= b.min.y && a.max.x <= b.max.x && a.max.y <= b.max.y; }

static int
rectisempty(Rectangle r)
{
	return r.min.x >= r.max.x || r.min.y >= r.max.y;
}

void combinerect(Rectangle *r, Rectangle s)
{
	if (r == nil)
		return;
	if (rectisempty(*r)) { *r = s; return; }
	if (rectisempty(s))
		return;
	if (s.min.x < r->min.x) r->min.x = s.min.x;
	if (s.min.y < r->min.y) r->min.y = s.min.y;
	if (s.max.x > r->max.x) r->max.x = s.max.x;
	if (s.max.y > r->max.y) r->max.y = s.max.y;
}

int rectclip(Rectangle *r, Rectangle s)
{
	if (r == nil)
		return 0;
	if (r->min.x < s.min.x) r->min.x = s.min.x;
	if (r->min.y < s.min.y) r->min.y = s.min.y;
	if (r->max.x > s.max.x) r->max.x = s.max.x;
	if (r->max.y > s.max.y) r->max.y = s.max.y;
	return !rectisempty(*r);
}

int ptinrect(Point p, Rectangle r) { return p.x >= r.min.x && p.x < r.max.x && p.y >= r.min.y && p.y < r.max.y; }
void replclipr(Image *i, int repl, Rectangle clipr) { if (i == nil) return; i->repl = repl; i->clipr = clipr; }
