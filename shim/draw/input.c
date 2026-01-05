#include "shim.h"

/*
 * mouse and keyboard, stubs
 * more in shim/input.c
 */

Mousectl*
initmouse(char *file, Image *i)
{
	Mousectl *mc;

	mc = mallocz(sizeof(Mousectl), 1);
	mc->c = chancreate(sizeof(Mouse), 8);
	mc->resizec = chancreate(sizeof(int), 32);
	mc->display = display;

	if (wl_state) {
		wl_state->mousec = mc->c;
		wl_state->resizec = mc->resizec;
	}

	return mc;
}

Keyboardctl*
initkeyboard(char *file)
{
	Keyboardctl *kc;

	kc = mallocz(sizeof(Keyboardctl), 1);
	kc->c = chancreate(sizeof(Rune), 0);

	if (wl_state) {
		wl_state->kbdc = kc->c;
	}

	return kc;
}

/*
 * mouse/keyboard helpers
 */

int
readmouse(Mousectl *mc)
{
	if (mc == nil)
		return -1;
	if (recv(mc->c, &mc->m) <= 0)
		return -1;
	return 0;
}

void
moveto(Mousectl *mc, Point p)
{
	if (mc == nil)
		return;
	mc->m.xy = p;
}

void
closemouse(Mousectl *mc)
{
	if (mc == nil)
		return;
	if (wl_state) {
		if (wl_state->mousec == mc->c)
			wl_state->mousec = nil;
		if (wl_state->resizec == mc->resizec)
			wl_state->resizec = nil;
	}
	chanfree(mc->c);
	chanfree(mc->resizec);
	free(mc);
}

int
ctlkeyboard(Keyboardctl *kc, char *s)
{
	USED(kc);
	USED(s);
	return 0;
}

void
closekeyboard(Keyboardctl *kc)
{
	if (kc == nil)
		return;
	if (wl_state && wl_state->kbdc == kc->c)
		wl_state->kbdc = nil;
	chanfree(kc->c);
	free(kc);
}

void
setcursor(Mousectl *mc, Cursor *c)
{
	USED(mc);
	USED(c);
}

void
setcursor2(Mousectl *mc, Cursor *c, Cursor2 *c2)
{
	USED(mc);
	USED(c);
	USED(c2);
}
