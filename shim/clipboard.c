
/*
 * wayland clipboard (wl_data_device) integration
 *
 * copy/paste between wave and other wayland clients.
 * all wl_display interaction happens on the event thread.
 */

#include "shim.h"

#include <unistd.h>
#include <poll.h>

static void
offer_mime(void *data, struct wl_data_offer *offer, const char *mime_type)
{
	USED(data);
	USED(offer);
	if (wl_state == nil || mime_type == nil)
		return;
	if (strcmp(mime_type, "text/plain;charset=utf-8") == 0)
		wl_state->offer_has_utf8 = 1;
	else if (strcmp(mime_type, "text/plain") == 0)
		wl_state->offer_has_plain = 1;
}

static const struct wl_data_offer_listener clipboard_offer_listener = {
	.offer = offer_mime,
};

static void
device_data_offer(void *data, struct wl_data_device *dd, struct wl_data_offer *offer)
{
	USED(data);
	USED(dd);
	if (wl_state == nil)
		return;
	wl_state->offer_has_utf8 = 0;
	wl_state->offer_has_plain = 0;
	wl_data_offer_add_listener(offer, &clipboard_offer_listener, nil);
}

static void
device_selection(void *data, struct wl_data_device *dd, struct wl_data_offer *offer)
{
	USED(data);
	USED(dd);
	if (wl_state == nil)
		return;
	/* retire the old offer instead of destroying it while a read may
	 * be queued against it; it is destroyed once the read is done. */
	if (wl_state->selection_offer) {
		if (wl_state->clip_read_queued) {
			if (wl_state->clip_retired)
				wl_data_offer_destroy(wl_state->clip_retired);
			wl_state->clip_retired = wl_state->selection_offer;
		} else {
			wl_data_offer_destroy(wl_state->selection_offer);
		}
	}
	wl_state->selection_offer = offer;
	if (offer) {
		if (wl_state->offer_has_utf8)
			wl_state->selection_mime = "text/plain;charset=utf-8";
		else if (wl_state->offer_has_plain)
			wl_state->selection_mime = "text/plain";
		else
			wl_state->selection_mime = nil;
	} else {
		wl_state->selection_mime = nil;
	}
}

static const struct wl_data_device_listener device_listener = {
	.data_offer = device_data_offer,
	.selection = device_selection,
};

static void
source_send(void *data, struct wl_data_source *source,
	const char *mime_type, int32_t fd)
{
	int n;

	USED(data);
	USED(source);
	USED(mime_type);
	if (wl_state != nil && wl_state->snarf_copy) {
		n = strlen(wl_state->snarf_copy);
		if (n > 0)
			if (write(fd, wl_state->snarf_copy, n) < 0)
				fprint(2, "clipboard: write failed: %r\n");
	}
	close(fd);
}

static void
source_cancelled(void *data, struct wl_data_source *source)
{
	USED(data);
	if (wl_state != nil && wl_state->selection_source == source)
		wl_state->selection_source = nil;
	wl_data_source_destroy(source);
}

static const struct wl_data_source_listener source_listener = {
	.send = source_send,
	.cancelled = source_cancelled,
};

int
clipboard_init(void)
{
	if (wl_state == nil || wl_state->ddmgr == nil || wl_state->seat == nil)
		return -1;
	wl_state->data_device =
		wl_data_device_manager_get_data_device(wl_state->ddmgr, wl_state->seat);
	wl_data_device_add_listener(wl_state->data_device, &device_listener, nil);
	return 0;
}

/*
 * called from the wayland event thread; processes pending clipboard
 * requests (set selection / read selection).
 */
void
clipboard_dispatch_cmds(void)
{
	ClipCmd cmd;
	uint32_t serial;

	if (wl_state == nil || wl_state->clipreq == nil)
		return;

	while (channbrecv(wl_state->clipreq, &cmd) > 0) {
		switch (cmd.op) {
		case ClipSet:
			if (wl_state->selection_source) {
				wl_data_source_destroy(wl_state->selection_source);
				wl_state->selection_source = nil;
			}
			if (wl_state->snarf_copy) {
				free(wl_state->snarf_copy);
				wl_state->snarf_copy = nil;
			}
			if (cmd.text && cmd.text[0] &&
			    wl_state->data_device && wl_state->ddmgr) {
				wl_state->snarf_copy = cmd.text;
				wl_state->selection_source =
					wl_data_device_manager_create_data_source(wl_state->ddmgr);
				wl_data_source_add_listener(wl_state->selection_source,
					&source_listener, nil);
				wl_data_source_offer(wl_state->selection_source,
					"text/plain;charset=utf-8");
				wl_data_source_offer(wl_state->selection_source,
					"text/plain");
				serial = wl_state->last_serial ?
					wl_state->last_serial : wl_state->kb_enter_serial;
				wl_data_device_set_selection(wl_state->data_device,
					wl_state->selection_source, serial);
			} else {
				free(cmd.text);
			}
			if (wl_state->wl_display)
				wl_display_flush(wl_state->wl_display);
			break;
		case ClipRead:
			if (cmd.offer && cmd.mime && cmd.fd >= 0) {
				wl_data_offer_receive(cmd.offer, cmd.mime, cmd.fd);
				if (wl_state->wl_display)
					wl_display_flush(wl_state->wl_display);
			}
			free(cmd.mime);
			if (cmd.fd >= 0)
				close(cmd.fd);
			wl_state->clip_read_queued = 0;
			if (wl_state->clip_retired) {
				wl_data_offer_destroy(wl_state->clip_retired);
				wl_state->clip_retired = nil;
			}
			break;
		}
	}
}

/*
 * advertise the given text as the current selection.
 * called from the acme threads; the actual wl requests run on the
 * event thread.
 */
void
clipboard_put(char *s)
{
	ClipCmd cmd;

	if (wl_state == nil || wl_state->clipreq == nil ||
	    wl_state->data_device == nil)
		return;
	if (s == nil || s[0] == 0)
		return;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op = ClipSet;
	cmd.text = strdup(s);
	if (cmd.text == nil) {
		fprint(2, "clipboard: strdup failed\n");
		return;
	}
	chansend(wl_state->clipreq, &cmd);
}

/*
 * fetch the current selection text. may block briefly while the event
 * thread pulls the data; falls back to the in-process copy.
 */
char*
clipboard_get(void)
{
	ClipCmd cmd;
	struct wl_data_offer *offer;
	const char *mime;
	char *data, *result;
	size_t len, cap;
	ssize_t n;
	int fds[2];
	struct pollfd pfd;
	int timeout;

	if (wl_state == nil || wl_state->clipreq == nil ||
	    wl_state->data_device == nil)
		return nil;

	/* keep the current offer alive while we read it */
	wl_state->clip_read_queued = 1;
	mime = wl_state->selection_mime;
	offer = wl_state->selection_offer;
	if (offer == nil || mime == nil) {
		wl_state->clip_read_queued = 0;
		return nil;
	}

	if (pipe(fds) < 0) {
		wl_state->clip_read_queued = 0;
		return nil;
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.op = ClipRead;
	cmd.offer = offer;
	cmd.mime = strdup((char*)mime);
	cmd.fd = fds[1];
	chansend(wl_state->clipreq, &cmd);

	data = nil;
	len = 0;
	cap = 0;
	timeout = 1000;
	for (;;) {
		pfd.fd = fds[0];
		pfd.events = POLLIN;
		pfd.revents = 0;
		if (poll(&pfd, 1, timeout) <= 0 || !(pfd.revents & POLLIN))
			break;
		if (len == cap) {
			cap = cap ? cap * 2 : 4096;
			data = realloc(data, cap);
			if (data == nil) {
				len = 0;
				break;
			}
		}
		n = read(fds[0], data + len, cap - len);
		if (n <= 0)
			break;
		len += n;
	}
	close(fds[0]);

	if (data) {
		data[len] = 0;
		result = strdup(data);
		free(data);
		if (result && result[0] == 0) {
			free(result);
			result = nil;
		}
		return result;
	}
	return nil;
}
