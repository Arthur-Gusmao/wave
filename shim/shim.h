#ifndef ACME_WAYLAND_SHIM_H
#define ACME_WAYLAND_SHIM_H

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>
#include <cursor.h>

#include <wayland-client.h>
#include <pixman.h>
#include <wld/wld.h>
#include <wld/wayland.h>
#include <fontconfig/fontconfig.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <poll.h>

#include "wayland-state.h"
#include "xdg-shell-client-protocol.h"

typedef struct ImageWld ImageWld;
struct ImageWld {
	Image base;  /* plan 9 image structure */
	struct wld_buffer *wld_buf;
	u32int *pixels;  
	int is_solid;
	u32int solid_color;
};

typedef struct FontWld FontWld;
struct FontWld {
	Font base;
	struct wld_font *wld_font;
};

extern WaylandState *wl_state;
extern Display *display;
extern Font *font;
extern Image *screen;
extern Screen *_screen;
extern Point ZP;
extern Rectangle ZR;
extern int drawmousemask;
extern int _cursorfd;
extern char *winsize;
extern int debug_wayland;

u32int p9rgba_to_argb(u32int c);
int wayland_init(char *label, int width, int height);
void wayland_event_thread(void *arg);
int wayland_resize_surface(int width, int height);
void wayland_input_init(void);

#endif
