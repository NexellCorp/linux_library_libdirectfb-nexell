/*
 * Copyright (C) 2019 Nexell Co.Ltd
 * Authors:
 *      JungHyun Kim <jhkim@nexell.co.kr>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _NXP3220_G2D_H_
#define _NXP3220_G2D_H_

#include "nexell_drm.h"
#include "nexell_debug.h"

#define NX_G2D_DRIVER_VER_MAJOR		1
#define NX_G2D_DRIVER_VER_MINOR		0

#ifndef drm_public
#  define drm_public
#endif

enum nx_g2d_pixel_order {
	NX_G2D_PIXEL_ORDER_ARGB = 0,
	NX_G2D_PIXEL_ORDER_RGBA = 1,
	NX_G2D_PIXEL_ORDER_ABGR = 2,
	NX_G2D_PIXEL_ORDER_BGRA = 3,
};

enum nx_g2d_pixel_format {
	NX_G2D_PIXEL_FMT_RGB565 = 0,
	NX_G2D_PIXEL_FMT_XRGB1555 = 1,
	NX_G2D_PIXEL_FMT_ARGB1555 = 2,
	NX_G2D_PIXEL_FMT_XRGB4444 = 3,
	NX_G2D_PIXEL_FMT_ARGB4444 = 4,
	NX_G2D_PIXEL_FMT_RGB888 = 8,
	NX_G2D_PIXEL_FMT_XRGB8888 = 12,
	NX_G2D_PIXEL_FMT_ARGB8888 = 13,
};

struct nx_g2d_image_obj {
	unsigned int type;
	unsigned int handle;
	/* buffer offset */
	__u32 offset;
	int pitch;
	int pixelorder;
	int pixelformat;
	int pixelbyte;
};

struct nx_g2d_image {
	int x, y;
	int width, height;

	struct nx_g2d_image_obj src;
	struct nx_g2d_image_obj dst;

	unsigned int fillcolor;
	unsigned int blendcolor;
	unsigned int flags;
	void *data;
};

#define BIT(n)		(1UL << (n))
#define BITS(v, n, s)	((v & ((1 << n) - 1)) << (s))

#define RGBA_COLOR(r, g, b, a) \
	(((r & 0xff) << 24) | \
	 ((g & 0xff) << 16) | \
	 ((b & 0xff) << 8) | \
	  (a & 0xff))

struct nx_g2d_ctx;

struct nx_g2d_ctx *nexell_g2d_alloc(int fd, int *major, int *minor);
void nexell_g2d_free(struct nx_g2d_ctx *ctx);

int nexell_g2d_fillrect(struct nx_g2d_ctx *ctx, struct nx_g2d_image *img);

int nexell_g2d_blit(struct nx_g2d_ctx *ctx, struct nx_g2d_image *img);

int nexell_g2d_sync(struct nx_g2d_ctx *ctx);

#endif /* _NXP3220_G2D_H_ */
