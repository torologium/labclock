#define _GNU_SOURCE
#include <ctype.h>
#include <fcft/fcft.h>
#include <fcntl.h>
#include <limits.h>
#include <pixman.h>
#include "wlr-layer-shell-unstable-v1-protocol.h"

#define LINE_LEN 128

static struct fcft_font *font;

/* foreground text colors */
static pixman_color_t
	fgcolor = {
		.red = 0xb3b3,
		.green = 0xb3b3,
		.blue = 0xb3b3,
		.alpha = 0xffff,
	};

static const struct wl_buffer_listener wl_buffer_listener = {
	.release = wl_buffer_release,
};

static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer) {
	/* sent by the compositor when it's no longer using this buffer */
	wl_buffer_destroy(wl_buffer);
}

static struct wl_buffer *draw_frame(char *text) {
	/* allocate buffer to be attached to the surface */
	int fd = allocate_shm_file(bufsize);
	if (fd == -1)
		return NULL;

	uint32_t *data = mmap(NULL, bufsize,
			PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		close(fd);
		return NULL;

  /* premultiplied colors */
  pixman_color_t textfgcolor = fgcolor;

	/* Pixman image corresponding to main buffer */
	pixman_image_t *bar = pixman_image_create_bits(PIXMAN_a8r8g8b8,
			width, height, data, width * 4);

	/* text background and foreground layers */
	pixman_image_t *background = pixman_image_create_bits(PIXMAN_a8r8g8b8,
			width, height, NULL, width * 4);

  pixman_image_t *foreground = pixman_image_create_bits(PIXMAN_a8r8g8b8,
			width, height, NULL, width * 4);

	pixman_image_t *fgfill = pixman_image_create_solid_fill(&textfgcolor);
  }
}
