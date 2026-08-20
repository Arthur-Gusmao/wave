#ifndef WAYLAND_STATE_H
#define WAYLAND_STATE_H

#include <draw.h>
#include <mouse.h>
#include <thread.h>
#include <wayland-client.h>

struct wld_context;
struct wld_renderer;
struct wld_font_context;
struct wld_buffer;
struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;

typedef struct WaylandState WaylandState;
typedef struct ClipCmd ClipCmd;
struct ClipCmd {
  int op;                      /* ClipSet or ClipRead */
  char *text;                  /* ClipSet: owned copy of the new text */
  struct wl_data_offer *offer; /* ClipRead: borrowed selection offer */
  char *mime;                  /* ClipRead: owned mime type */
  int fd;                      /* ClipRead: pipe write end for the compositor */
};
enum { ClipSet = 1, ClipRead = 2 };

struct WaylandState {
  struct wl_display *wl_display;
  struct wl_registry *registry;
  struct wl_compositor *compositor;
  struct wl_surface *surface;
  struct xdg_wm_base *wm_base;
  struct xdg_surface *xdg_surface;
  struct xdg_toplevel *xdg_toplevel;
  struct wl_seat *seat;
  struct wl_keyboard *keyboard;
  struct wl_pointer *pointer;

  struct wld_context *wld_ctx;
  struct wld_renderer *renderer;
  struct wld_font_context *font_ctx;
  u32int wld_format;
  struct wld_buffer *wld_buf;
  struct wld_buffer *wld_oldbuf;
  struct wl_buffer *wl_buf;

  int width, height;
  int configured;
  int running;

  Channel *mousec;
  Channel *resizec;
  Channel *kbdc;
  Channel *eventdone;

  /* clipboard */
  struct wl_data_device_manager *ddmgr;
  struct wl_data_device *data_device;
  struct wl_data_offer *selection_offer;
  struct wl_data_offer *clip_retired;
  struct wl_data_source *selection_source;
  char *snarf_copy;
  const char *selection_mime;
  uint32_t last_serial;
  uint32_t kb_enter_serial;
  Channel *clipreq;
  int clip_read_queued;
  int offer_has_utf8;
  int offer_has_plain;

  int seat_inited;

  Mouse mouse_state;
  int mouse_buttons;
};

extern WaylandState *wl_state;

#endif
