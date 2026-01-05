/*
 * wayland event handling to plan 9 channel
 */

#include "shim.h"

#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>
#include <sys/mman.h>
#include <unistd.h>

/* xkb keyboard state */
static struct xkb_context *xkb_ctx;
static struct xkb_keymap *xkb_keymap;
static struct xkb_state *xkb_state;
static int debug_input;

static void
mouse_send(Mouse *m)
{
	Mouse drop;

	if (!wl_state || !wl_state->mousec)
		return;
	if (channbsend(wl_state->mousec, m) <= 0) {
		while (channbrecv(wl_state->mousec, &drop) > 0)
			;
		channbsend(wl_state->mousec, m);
	}
}

/*
 * mouse event handlers
 */

static void
pointer_enter(void *data, struct wl_pointer *pointer,
	uint32_t serial, struct wl_surface *surface,
	wl_fixed_t sx, wl_fixed_t sy)
{
	if (!wl_state || !wl_state->mousec)
		return;

	wl_state->mouse_state.xy.x = wl_fixed_to_int(sx);
	wl_state->mouse_state.xy.y = wl_fixed_to_int(sy);
	mouse_send(&wl_state->mouse_state);
	if (debug_input) {
		fprint(2, "wayland-input: enter %d,%d\n",
			wl_state->mouse_state.xy.x,
			wl_state->mouse_state.xy.y);
	}
}

static void
pointer_leave(void *data, struct wl_pointer *pointer,
	uint32_t serial, struct wl_surface *surface)
{
	/* todo */
}

static void
pointer_motion(void *data, struct wl_pointer *pointer,
	uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
	if (!wl_state || !wl_state->mousec)
		return;

	wl_state->mouse_state.xy.x = wl_fixed_to_int(sx);
	wl_state->mouse_state.xy.y = wl_fixed_to_int(sy);
	wl_state->mouse_state.msec = time;

	mouse_send(&wl_state->mouse_state);
	if (debug_input) {
		fprint(2, "wayland-input: motion %d,%d buttons=%d\n",
			wl_state->mouse_state.xy.x,
			wl_state->mouse_state.xy.y,
			wl_state->mouse_state.buttons);
	}
}

static void
pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial,
	uint32_t time, uint32_t button, uint32_t state)
{
	int plan9_button = 0;

	if (!wl_state || !wl_state->mousec)
		return;

	/* map wl buttons to plan 9 buttons */
	switch (button) {
	case BTN_LEFT:
		plan9_button = 1;
		break;
	case BTN_MIDDLE:
		plan9_button = 2;
		break;
	case BTN_RIGHT:
		plan9_button = 4;
		break;
	}

	if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
		wl_state->mouse_buttons |= plan9_button;
	} else {
		wl_state->mouse_buttons &= ~plan9_button;
	}

	wl_state->mouse_state.buttons = wl_state->mouse_buttons;
	wl_state->mouse_state.msec = time;

	if (debug_input) {
		fprint(2, "wayland-input: button %u state=%u p9=%d mask=%d\n",
			button, state, plan9_button, wl_state->mouse_state.buttons);
	}
	mouse_send(&wl_state->mouse_state);
}

static void
pointer_axis(void *data, struct wl_pointer *pointer,
	uint32_t time, uint32_t axis, wl_fixed_t value)
{
}

static const struct wl_pointer_listener pointer_listener = {
	.enter = pointer_enter,
	.leave = pointer_leave,
	.motion = pointer_motion,
	.button = pointer_button,
	.axis = pointer_axis,
};


static void
keyboard_keymap(void *data, struct wl_keyboard *keyboard,
	uint32_t format, int fd, uint32_t size)
{
	char *map_str;

	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
		close(fd);
		return;
	}

	map_str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	if (map_str == MAP_FAILED) {
		close(fd);
		return;
	}

	xkb_keymap = xkb_keymap_new_from_string(xkb_ctx, map_str,
		XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	munmap(map_str, size);
	close(fd);

	if (!xkb_keymap)
		return;

	xkb_state = xkb_state_new(xkb_keymap);
}

static void
keyboard_enter(void *data, struct wl_keyboard *keyboard,
	uint32_t serial, struct wl_surface *surface,
	struct wl_array *keys)
{
	/* kb focus gained */
}

static void
keyboard_leave(void *data, struct wl_keyboard *keyboard,
	uint32_t serial, struct wl_surface *surface)
{
	/* kb focus lost */
}

static void
keyboard_key(void *data, struct wl_keyboard *keyboard,
	uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
	xkb_keysym_t keysym;
	Rune rune;
	char buf[8];
	int len;

	if (!wl_state || !wl_state->kbdc || !xkb_state)
		return;

	if (state != WL_KEYBOARD_KEY_STATE_PRESSED)
		return;

	/* keysym */
	keysym = xkb_state_key_get_one_sym(xkb_state, key + 8);

	/* utf-8 */
	len = xkb_keysym_to_utf8(keysym, buf, sizeof(buf));
	if (len <= 0 || len >= sizeof(buf))
		return;

	/* rune*/
	buf[len] = 0;
	chartorune(&rune, buf);

	if (keysym >= XKB_KEY_F1 && keysym <= XKB_KEY_F12) {
		rune = KF | (keysym - XKB_KEY_F1 + 1);
	} else if (keysym == XKB_KEY_Home) {
		rune = Khome;
	} else if (keysym == XKB_KEY_End) {
		rune = Kend;
	} else if (keysym == XKB_KEY_Up) {
		rune = Kup;
	} else if (keysym == XKB_KEY_Down) {
		rune = Kdown;
	} else if (keysym == XKB_KEY_Left) {
		rune = Kleft;
	} else if (keysym == XKB_KEY_Right) {
		rune = Kright;
	} else if (keysym == XKB_KEY_Page_Up) {
		rune = Kpgup;
	} else if (keysym == XKB_KEY_Page_Down) {
		rune = Kpgdown;
	} else if (keysym == XKB_KEY_Return) {
		rune = '\n';
	} else if (keysym == XKB_KEY_BackSpace) {
		rune = '\b';
	} else if (keysym == XKB_KEY_Delete) {
		rune = Kdel;
	} else if (keysym == XKB_KEY_Escape) {
		rune = Kesc;
	}

	/* send to keybaord channel */
	send(wl_state->kbdc, &rune);
}

static void
keyboard_modifiers(void *data, struct wl_keyboard *keyboard,
	uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
	uint32_t mods_locked, uint32_t group)
{
	if (xkb_state) {
		xkb_state_update_mask(xkb_state, mods_depressed, mods_latched,
			mods_locked, 0, 0, group);
	}
}

static void
keyboard_repeat_info(void *data, struct wl_keyboard *keyboard,
	int32_t rate, int32_t delay)
{
}

static const struct wl_keyboard_listener keyboard_listener = {
	.keymap = keyboard_keymap,
	.enter = keyboard_enter,
	.leave = keyboard_leave,
	.key = keyboard_key,
	.modifiers = keyboard_modifiers,
	.repeat_info = keyboard_repeat_info,
};


static void
seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps)
{
	if (!wl_state)
		return;
	if (debug_input)
		fprint(2, "wayland-input: seat caps=0x%x\n", caps);

	if (caps & WL_SEAT_CAPABILITY_POINTER) {
		if (!wl_state->pointer) {
			wl_state->pointer = wl_seat_get_pointer(seat);
			wl_pointer_add_listener(wl_state->pointer, &pointer_listener, NULL);
			if (debug_input)
				fprint(2, "wayland-input: pointer ready\n");
		}
	}

	if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
		if (!wl_state->keyboard) {
			wl_state->keyboard = wl_seat_get_keyboard(seat);
			wl_keyboard_add_listener(wl_state->keyboard, &keyboard_listener, NULL);
			if (debug_input)
				fprint(2, "wayland-input: keyboard ready\n");

			if (!xkb_ctx) {
				xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
			}
		}
	}
}

static void
seat_name(void *data, struct wl_seat *seat, const char *name)
{
}

static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_capabilities,
	.name = seat_name,
};


void
wayland_input_init(void)
{
	if (!wl_state || !wl_state->seat)
		return;

	if (wl_state->seat_inited)
		return;
	debug_input = getenv("ACME_WAYLAND_INPUT_DEBUG") != nil;
	if (debug_input)
		fprint(2, "wayland-input: init seat listener\n");
	wl_seat_add_listener(wl_state->seat, &seat_listener, NULL);
	wl_state->seat_inited = 1;
}
