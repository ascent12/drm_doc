#include <drm_fourcc.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "util.h"

struct dumb_framebuffer {
	uint32_t id;     // DRM object ID
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t handle; // driver-specific handle
	uint64_t size;   // size of mapping

	uint8_t *data;   // mmapped data we can write to
};

struct connector {
	uint32_t id;
	char name[16];
	bool connected;

	drmModeCrtc *saved;

	uint32_t crtc_id;
	drmModeModeInfo mode;

	uint32_t width;
	uint32_t height;
	uint32_t rate;

	struct dumb_framebuffer fb[2];
	struct dumb_framebuffer *front;
	struct dumb_framebuffer *back;

	uint8_t colour[4]; // B G R X
	int inc;
	int dec;

	struct connector *next;
};

static uint32_t find_crtc(int drm_fd, drmModeRes *res, drmModeConnector *conn,
		uint32_t *taken_crtcs)
{
	for (int i = 0; i < conn->count_encoders; ++i) {
		drmModeEncoder *enc = drmModeGetEncoder(drm_fd, conn->encoders[i]);
		if (!enc)
			continue;

		for (int i = 0; i < res->count_crtcs; ++i) {
			uint32_t bit = 1 << i;
			// Not compatible
			if ((enc->possible_crtcs & bit) == 0)
				continue;

			// Already taken
			if (*taken_crtcs & bit)
				continue;

			drmModeFreeEncoder(enc);
			*taken_crtcs |= bit;
			return res->crtcs[i];
		}

		drmModeFreeEncoder(enc);
	}

	return 0;
}

static bool create_fb(int drm_fd, uint32_t width, uint32_t height,
		struct dumb_framebuffer *fb)
{
	int ret;

	struct drm_mode_create_dumb create = {
		.width = width,
		.height = height,
		.bpp = 32,
	};

	ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);
	if (ret < 0) {
		perror("DRM_IOCTL_MODE_CREATE_DUMB");
		return false;
	}

	fb->height = height;
	fb->width = width;
	fb->stride = create.pitch;
	fb->handle = create.handle;
	fb->size = create.size;

	uint32_t handles[4] = { fb->handle };
	uint32_t strides[4] = { fb->stride };
	uint32_t offsets[4] = { 0 };

	ret = drmModeAddFB2(drm_fd, width, height, DRM_FORMAT_XRGB8888,
		handles, strides, offsets, &fb->id, 0);
	if (ret < 0) {
		perror("drmModeAddFB2");
		goto error_dumb;
	}

	struct drm_mode_map_dumb map = { .handle = fb->handle };
	ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map);
	if (ret < 0) {
		perror("DRM_IOCTL_MODE_MAP_DUMB");
		goto error_fb;
	}

	fb->data = mmap(0, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED,
		drm_fd, map.offset);
	if (!fb->data) {
		perror("mmap");
		goto error_fb;
	}

	memset(fb->data, 0xff, fb->size);

	return true;

error_fb:
	drmModeRmFB(drm_fd, fb->id);
error_dumb:
	;
	struct drm_mode_destroy_dumb destroy = { .handle = fb->handle };
	drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
	return false;
}

static void destroy_fb(int drm_fd, struct dumb_framebuffer *fb)
{
	munmap(fb->data, fb->size);
	drmModeRmFB(drm_fd, fb->id);
	struct drm_mode_destroy_dumb destroy = { .handle = fb->handle };
	drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

static void page_flip_handler(int drm_fd, unsigned sequence, unsigned tv_sec,
		unsigned tv_usec, void *data)
{
	(void)sequence;
	(void)tv_sec;
	(void)tv_usec;

	struct connector *conn = data;

	conn->colour[conn->inc] += 15;
	conn->colour[conn->dec] -= 15;

	if (conn->colour[conn->dec] == 0) {
		conn->dec = conn->inc;
		conn->inc = (conn->inc + 2) % 3;
	}

	struct dumb_framebuffer *fb = conn->back;

	for (uint32_t y = 0; y < fb->height; ++y) {
		uint8_t *row = fb->data + fb->stride * y;

		for (uint32_t x = 0; x < fb->width; ++x) {
			row[x * 4 + 0] = conn->colour[0];
			row[x * 4 + 1] = conn->colour[1];
			row[x * 4 + 2] = conn->colour[2];
			row[x * 4 + 3] = conn->colour[3];
		}
	}

	if (drmModePageFlip(drm_fd, conn->crtc_id, fb->id,
			DRM_MODE_PAGE_FLIP_EVENT, conn) < 0) {
		perror("drmModePageFlip");
	}

	// Comment these two lines out to remove double buffering
	conn->back = conn->front;
	conn->front = fb;
}

int main(void)
{
	/* We just take the first GPU that exists. */
	int drm_fd = open("/dev/dri/card0", O_RDWR | O_NONBLOCK);
	if (drm_fd < 0) {
		perror("/dev/dri/card0");
		return 1;
	}

	drmModeRes *res = drmModeGetResources(drm_fd);
	if (!res) {
		perror("drmModeGetResources");
		return 1;
	}

	struct connector *conn_list = NULL;
	uint32_t taken_crtcs = 0;

	for (int i = 0; i < res->count_connectors; ++i) {
		drmModeConnector *drm_conn = drmModeGetConnector(drm_fd, res->connectors[i]);
		if (!drm_conn) {
			perror("drmModeGetConnector");
			continue;
		}

		struct connector *conn = malloc(sizeof *conn);
		if (!conn) {
			perror("malloc");
			goto cleanup;
		}

		conn->id = drm_conn->connector_id;
		snprintf(conn->name, sizeof conn->name, "%s-%"PRIu32,
			conn_str(drm_conn->connector_type),
			drm_conn->connector_type_id);
		conn->connected = drm_conn->connection == DRM_MODE_CONNECTED;

		conn->next = conn_list;
		conn_list = conn;

		printf("Found display %s\n", conn->name);

		if (!conn->connected) {
			printf("  Disconnected\n");
			goto cleanup;
		}

		if (drm_conn->count_modes == 0) {
			printf("No valid modes\n");
			conn->connected = false;
			goto cleanup;
		}

		conn->crtc_id = find_crtc(drm_fd, res, drm_conn, &taken_crtcs);
		if (!conn->crtc_id) {
			fprintf(stderr, "Could not find CRTC for %s\n", conn->name);
			conn->connected = false;
			goto cleanup;
		}

		printf("  Using CRTC %"PRIu32"\n", conn->crtc_id);

		// [0] is the best mode, so we'll just use that.
		conn->mode = drm_conn->modes[0];

		conn->width = conn->mode.hdisplay;
		conn->height = conn->mode.vdisplay;
		conn->rate = refresh_rate(&conn->mode);

		printf("  Using mode %"PRIu32"x%"PRIu32"@%"PRIu32"\n",
			conn->width, conn->height, conn->rate);

		if (!create_fb(drm_fd, conn->width, conn->height, &conn->fb[0])) {
			conn->connected = false;
			goto cleanup;
		}

		printf("  Created frambuffer with ID %"PRIu32"\n", conn->fb[0].id);

		if (!create_fb(drm_fd, conn->width, conn->height, &conn->fb[1])) {
			destroy_fb(drm_fd, &conn->fb[0]);
			conn->connected = false;
			goto cleanup;
		}

		printf("  Created frambuffer with ID %"PRIu32"\n", conn->fb[1].id);

		conn->front = &conn->fb[0];
		conn->back = &conn->fb[1];

		conn->colour[0] = 0x00; // B
		conn->colour[1] = 0x00; // G
		conn->colour[2] = 0xff; // R
		conn->colour[3] = 0x00; // X
		conn->inc = 1;
		conn->dec = 2;

		// Save the previous CRTC configuration
		conn->saved = drmModeGetCrtc(drm_fd, conn->crtc_id);

		// Perform the modeset
		int ret = drmModeSetCrtc(drm_fd, conn->crtc_id, conn->front->id, 0, 0,
			&conn->id, 1, &conn->mode);
		if (ret < 0) {
			perror("drmModeSetCrtc");
			goto cleanup;
		}

		// Start the page flip cycle
		if (drmModePageFlip(drm_fd, conn->crtc_id, conn->front->id,
				DRM_MODE_PAGE_FLIP_EVENT, conn) < 0) {
			perror("drmModePageFlip");
		}

cleanup:
		drmModeFreeConnector(drm_conn);
	}

	drmModeFreeResources(res);

	struct pollfd pollfd = {
		.fd = drm_fd,
		.events = POLLIN,
	};

	// Incredibly inaccurate, but it doesn't really matter for this example
	time_t end = time(NULL) + 5;
	while (time(NULL) <= end) {
		int ret = poll(&pollfd, 1, 5000);
		if (ret < 0 && errno != EAGAIN) {
			perror("poll");
			break;
		}

		if (pollfd.revents & POLLIN) {
			drmEventContext context = {
				.version = DRM_EVENT_CONTEXT_VERSION,
				.page_flip_handler = page_flip_handler,
			};

			if (drmHandleEvent(drm_fd, &context) < 0) {
				perror("drmHandleEvent");
				break;
			}
		}
	}

	// Cleanup
	struct connector *conn = conn_list;
	while (conn) {
		if (conn->connected) {
			destroy_fb(drm_fd, &conn->fb[0]);
			destroy_fb(drm_fd, &conn->fb[1]);

			// Restore the old CRTC
			drmModeCrtc *crtc = conn->saved;
			if (crtc) {
				drmModeSetCrtc(drm_fd, crtc->crtc_id, crtc->buffer_id,
					crtc->x, crtc->y, &conn->id, 1, &crtc->mode);
				drmModeFreeCrtc(crtc);
			}
		}

		struct connector *tmp = conn->next;
		free(conn);
		conn = tmp;
	}

	close(drm_fd);
}
