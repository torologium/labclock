#define _GNU_SOURCE
#include <ctype.h>
#include <fcft/fcft.h>
#include <fcntl.h>
#include <limits.h>
#include <pixman.h>
#include <wayland-client.h>
#include "utf8.h"
#include "wlr-layer-shell-unstable-v1-protocol.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
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

static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer) {
	/* sent by the compositor when it's no longer using this buffer */
	wl_buffer_destroy(wl_buffer);
}

static const struct wl_buffer_listener wl_buffer_listener = {
	.release = wl_buffer_release,
};


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
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, bufsize);
	struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0,
			width, height, stride, WL_SHM_FORMAT_ARGB8888);
	wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);
	wl_shm_pool_destroy(pool);
	close(fd);

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

	/* start drawing at center-left (ypos sets the text baseline) */
	uint32_t xpos = 0, maxxpos = 0;
	uint32_t ypos = (height + font->ascent - font->descent) / 2;

	uint32_t codepoint, lastcp = 0, state = UTF8_ACCEPT;
	for (char *p = text; *p; p++) {
		/* check for inline ^ commands */
		if (state == UTF8_ACCEPT && *p == '^') p++; 

		/* return nonzero if more bytes are needed */
		if (utf8decode(&state, &codepoint, *p)) continue;

    /* rasterize a glyph for a single UTF-32 codepoint */
		const struct fcft_glyph *glyph = fcft_rasterize_char_utf32(font, codepoint,
				FCFT_SUBPIXEL_DEFAULT);
		if (!glyph) continue;

		/* adjust x position based on kerning with previous glyph */
		long x_kern = 0;
		if (lastcp)
			fcft_kerning(font, lastcp, codepoint, &x_kern, NULL);
		xpos += x_kern;
		lastcp = codepoint;

    /* apply foreground for subpixel rendered text */
 		pixman_image_composite32(
 			PIXMAN_OP_OVER, fgfill, glyph->pix, foreground, 0, 0, 0, 0,
 			xpos + glyph->x, ypos - glyph->y, glyph->width, glyph->height);

	/* start drawing at center-left (ypos sets the text baseline) */
	uint32_t xpos = 0, maxxpos = 0;
	uint32_t ypos = (height + font->ascent - font->descent) / 2;

	uint32_t codepoint, lastcp = 0, state = UTF8_ACCEPT;
	for (char *p = text; *p; p++) {
		/* check for inline ^ commands */
		if (state == UTF8_ACCEPT && *p == '^') p++; 

		/* return nonzero if more bytes are needed */
		if (utf8decode(&state, &codepoint, *p)) continue;

    /* rasterize a glyph for a single UTF-32 codepoint */
		const struct fcft_glyph *glyph = fcft_rasterize_char_utf32(font, codepoint,
				FCFT_SUBPIXEL_DEFAULT);
		if (!glyph) continue;

		/* adjust x position based on kerning with previous glyph */
		long x_kern = 0;
		if (lastcp)
			fcft_kerning(font, lastcp, codepoint, &x_kern, NULL);
		xpos += x_kern;
		lastcp = codepoint;

    /* apply foreground for subpixel rendered text */
 		pixman_image_composite32(
 			PIXMAN_OP_OVER, fgfill, glyph->pix, foreground, 0, 0, 0, 0,
 			xpos + glyph->x, ypos - glyph->y, glyph->width, glyph->height);

		/* increment pen position */
		xpos += glyph->advance.x;
		ypos += glyph->advance.y;
		maxxpos = MAX(maxxpos, xpos);
  }
 	pixman_image_unref(fgfill);

	if (state != UTF8_ACCEPT)
		fprintf(stderr, "malformed UTF-8 sequence\n");

	/* make it colorful */
	int32_t xdraw = (width - maxxpos) / 2;
	pixman_image_composite32(PIXMAN_OP_OVER, background, NULL, bar, 0, 0, 0, 0,
			xdraw, 0, width, height);
	pixman_image_composite32(PIXMAN_OP_OVER, foreground, NULL, bar, 0, 0, 0, 0,
			xdraw, 0, width, height);

	pixman_image_unref(foreground);
	pixman_image_unref(background);
	pixman_image_unref(bar);
	munmap(data, bufsize);
	return buffer;
}

/* layer-surface setup adapted from wlroots */
static void layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t w, uint32_t h) {
	width = w;
	height = h;
	stride = width * 4;
	bufsize = stride * height;

	if (on_top > 0)
		on_top = height;

	zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, on_top);
	zwlr_layer_surface_v1_ack_configure(surface, serial);

	struct wl_buffer *buffer = draw_frame(lastline);
	if (!buffer)
		return;
	wl_surface_attach(wl_surface, buffer, 0, 0);
	wl_surface_damage_buffer(wl_surface, 0, 0, width, height);
	wl_surface_commit(wl_surface);
}

static void layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *surface) {
	zwlr_layer_surface_v1_destroy(surface);
	wl_surface_destroy(wl_surface);
	run_display = false;
}

static struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};
