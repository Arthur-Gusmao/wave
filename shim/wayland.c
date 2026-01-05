/*
 * wayland window creation and events
 */

#include "shim.h"

WaylandState *wl_state = nil;

static void registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name);
static void xdg_wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial);
static void xdg_surface_configure(void *data, struct xdg_surface *surface, uint32_t serial);
static void xdg_toplevel_configure(void *data, struct xdg_toplevel *toplevel, int32_t width, int32_t height, struct wl_array *states);
static void xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel);

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
	.ping = xdg_wm_base_ping,
};

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_configure,
};

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_toplevel_configure,
	.close = xdg_toplevel_close,
};

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
	if (strcmp(interface, "wl_compositor") == 0) {
		wl_state->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 1);
	} else if (strcmp(interface, "xdg_wm_base") == 0) {
		wl_state->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(wl_state->wm_base, &xdg_wm_base_listener, NULL);
	} else if (strcmp(interface, "wl_seat") == 0) {
		wl_state->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
		wayland_input_init();
	}
}

static void
registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
	USED(data);
	USED(registry);
	USED(name);
}

static void
xdg_wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial)
{
	USED(data);
	xdg_wm_base_pong(wm_base, serial);
}

static void
xdg_surface_configure(void *data, struct xdg_surface *surface, uint32_t serial)
{
	USED(data);
	xdg_surface_ack_configure(surface, serial);
}

static void
xdg_toplevel_configure(void *data, struct xdg_toplevel *toplevel, int32_t width, int32_t height, struct wl_array *states)
{
	USED(data);
	USED(toplevel);
	USED(states);
	if (width > 0 && height > 0 && wl_state) {
		wl_state->width = width;
		wl_state->height = height;
		wl_state->configured = 1;

		if (wl_state->resizec) {
			int val = 1;

			if (channbsend(wl_state->resizec, &val) <= 0) {
				while (channbrecv(wl_state->resizec, &val) > 0)
					;
				channbsend(wl_state->resizec, &val);
			}
		}
	}
}

static void
xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
	USED(data);
	USED(toplevel);
	if (wl_state)
		wl_state->running = 0;
}

int
wayland_init(char *label, int width, int height)
{
	wl_state = mallocz(sizeof(WaylandState), 1);
	if (!wl_state)
		return -1;

	wl_state->width = width;
	wl_state->height = height;
	wl_state->running = 1;

	wl_state->wl_display = wl_display_connect(NULL);
	if (!wl_state->wl_display) {
		werrstr("wl_display_connect failed: %s", strerror(errno));
		free(wl_state);
		return -1;
	}

	wl_state->registry = wl_display_get_registry(wl_state->wl_display);
	wl_registry_add_listener(wl_state->registry, &registry_listener, NULL);

	wl_display_roundtrip(wl_state->wl_display);

	if (!wl_state->compositor || !wl_state->wm_base) {
		if (!wl_state->compositor)
			werrstr("wl_compositor not available");
		else
			werrstr("xdg_wm_base not available");
		wl_display_disconnect(wl_state->wl_display);
		free(wl_state);
		return -1;
	}

	wl_state->surface = wl_compositor_create_surface(wl_state->compositor);
	if (!wl_state->surface) {
		werrstr("wl_compositor_create_surface failed");
		wl_display_disconnect(wl_state->wl_display);
		free(wl_state);
		return -1;
	}
	wl_state->xdg_surface = xdg_wm_base_get_xdg_surface(wl_state->wm_base, wl_state->surface);
	if (!wl_state->xdg_surface) {
		werrstr("xdg_wm_base_get_xdg_surface failed");
		wl_display_disconnect(wl_state->wl_display);
		free(wl_state);
		return -1;
	}
	xdg_surface_add_listener(wl_state->xdg_surface, &xdg_surface_listener, NULL);
	wl_state->xdg_toplevel = xdg_surface_get_toplevel(wl_state->xdg_surface);
	if (!wl_state->xdg_toplevel) {
		werrstr("xdg_surface_get_toplevel failed");
		wl_display_disconnect(wl_state->wl_display);
		free(wl_state);
		return -1;
	}
	xdg_toplevel_add_listener(wl_state->xdg_toplevel, &xdg_toplevel_listener, NULL);
	xdg_toplevel_set_title(wl_state->xdg_toplevel, label);
	wl_surface_commit(wl_state->surface);

	wl_display_roundtrip(wl_state->wl_display);

	wl_state->wld_ctx = wld_wayland_create_context(wl_state->wl_display, WLD_SHM, WLD_NONE);
	if (!wl_state->wld_ctx) {
		werrstr("wld_wayland_create_context failed");
		wl_display_disconnect(wl_state->wl_display);
		free(wl_state);
		return -1;
	}

	wl_state->wld_format = WLD_FORMAT_XRGB8888;
	if (!wld_wayland_has_format(wl_state->wld_ctx, wl_state->wld_format))
		wl_state->wld_format = WLD_FORMAT_ARGB8888;
	if (!wld_wayland_has_format(wl_state->wld_ctx, wl_state->wld_format)) {
		werrstr("no supported Wayland buffer format");
		wld_destroy_context(wl_state->wld_ctx);
		wl_display_disconnect(wl_state->wl_display);
		free(wl_state);
		return -1;
	}

	if (wayland_resize_surface(wl_state->width, wl_state->height) < 0) {
		werrstr("wld_create_buffer failed");
		wld_destroy_context(wl_state->wld_ctx);
		wl_display_disconnect(wl_state->wl_display);
		free(wl_state);
		return -1;
	}

	wl_state->renderer = wld_create_renderer(wl_state->wld_ctx);
	if (!wl_state->renderer) {
		werrstr("wld_create_renderer failed");
		if (wl_state->wld_buf)
			wld_buffer_unreference(wl_state->wld_buf);
		wld_destroy_context(wl_state->wld_ctx);
		wl_display_disconnect(wl_state->wl_display);
		free(wl_state);
		return -1;
	}
	wl_state->font_ctx = wld_font_create_context();
	if (!wl_state->font_ctx) {
		werrstr("wld_font_create_context failed");
		wld_destroy_renderer(wl_state->renderer);
		if (wl_state->wld_buf)
			wld_buffer_unreference(wl_state->wld_buf);
		wld_destroy_context(wl_state->wld_ctx);
		wl_display_disconnect(wl_state->wl_display);
		free(wl_state);
		return -1;
	}
	wayland_input_init();
	if (debug_wayland) {
		fprint(2, "wayland: size=%dx%d format=%s\n",
			wl_state->width, wl_state->height,
			wl_state->wld_format == WLD_FORMAT_ARGB8888 ? "ARGB8888" : "XRGB8888");
	}
	if (debug_wayland && getenv("ACME_WAYLAND_CLEAR") != nil) {
		if (wl_state->wld_buf && wl_state->wl_buf &&
			wld_set_target_buffer(wl_state->renderer, wl_state->wld_buf)) {
			u32int color;

			color = p9rgba_to_argb(0xFF00FFFF);
			wld_fill_rectangle(wl_state->renderer, color, 0, 0,
				wl_state->width, wl_state->height);
			wld_flush(wl_state->renderer);
			wl_surface_damage(wl_state->surface, 0, 0,
				wl_state->width, wl_state->height);
			wl_surface_attach(wl_state->surface, wl_state->wl_buf, 0, 0);
			wl_surface_commit(wl_state->surface);
			wl_display_flush(wl_state->wl_display);
			fprint(2, "wayland: clear test done\n");
		} else if (wl_state->wld_buf && wl_state->wl_buf) {
			fprint(2, "clear: wld_set_target_buffer failed\n");
		} else {
			fprint(2, "clear: no Wayland buffer\n");
		}
	}

	return 0;
}

void
wayland_event_thread(void *arg)
{
	while (wl_state && wl_state->running) {
		int fd;
		struct pollfd pfd;

		wl_display_dispatch_pending(wl_state->wl_display);
		wl_display_flush(wl_state->wl_display);

		fd = wl_display_get_fd(wl_state->wl_display);
		pfd.fd = fd;
		pfd.events = POLLIN;
		pfd.revents = 0;

		if (poll(&pfd, 1, 10) > 0 && (pfd.revents & POLLIN))
			wl_display_dispatch(wl_state->wl_display);
	}
	if (wl_state && wl_state->eventdone) {
		int done = 1;
		send(wl_state->eventdone, &done);
	}
	threadexits(nil);
}

int
wayland_resize_surface(int width, int height)
{
	union wld_object object;

	if (!wl_state || !wl_state->wld_ctx || !wl_state->surface)
		return -1;
	if (width <= 0 || height <= 0)
		return -1;
	if (wl_state->wld_buf) {
		wl_state->wld_oldbuf = wl_state->wld_buf;
		wl_state->wld_buf = nil;
	}
	wl_state->wld_buf = wld_create_buffer(wl_state->wld_ctx, width, height,
		wl_state->wld_format, 0);
	if (!wl_state->wld_buf) {
		wl_state->wld_buf = wl_state->wld_oldbuf;
		wl_state->wld_oldbuf = nil;
		return -1;
	}
	if (!wld_export(wl_state->wld_buf, WLD_WAYLAND_OBJECT_BUFFER, &object)) {
		wld_buffer_unreference(wl_state->wld_buf);
		wl_state->wld_buf = wl_state->wld_oldbuf;
		wl_state->wld_oldbuf = nil;
		return -1;
	}
	wl_state->wl_buf = object.ptr;
	if (screen)
		((ImageWld*)screen)->wld_buf = wl_state->wld_buf;
	return 0;
}
