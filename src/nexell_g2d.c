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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <xf86drm.h>

#include "nexell_g2d.h"

struct nx_g2d_ctx {
	int fd;
	int major;
	int minor;
	struct nx_g2d_cmd cmd;
};

#define	COMMAND(c, v, t) do { \
		(c)->cmd[t] |= v; \
		(c)->cmd_mask |= BIT(t); \
	} while (0)

#define	DITHER_ON(v)	(v < 3 ? true : false)

#ifdef DRM_IOCTL_NX_G2D_EXEC
#define DRM_IOCTL_EXEC	DRM_IOCTL_NX_G2D_EXEC
#else
#define DRM_IOCTL_EXEC	DRM_IOCTL_NX_G2D_DMA_EXEC
#endif
#ifdef DRM_IOCTL_NX_G2D_SYNC
#define DRM_IOCTL_SYNC	DRM_IOCTL_NX_G2D_SYNC
#else
#define DRM_IOCTL_SYNC	DRM_IOCTL_NX_G2D_DMA_SYNC
#endif

static void
g2d_op_initialize(struct nx_g2d_cmd *cmd, struct nx_g2d_image *img)
{
	struct nx_g2d_image_obj *src = &img->src;
	struct nx_g2d_image_obj *dst = &img->dst;

	/* clear commands */
	memset(cmd, 0, sizeof(*cmd));

	cmd->flags = img->flags;

	/* source buffer */
	cmd->src.type = src->buf_type;
	cmd->src.handle = src->handle;
	cmd->src.offset = src->offset;

	/* destination buffer */
	cmd->dst.type = dst->buf_type;
	cmd->dst.handle = dst->handle;
	cmd->dst.offset = dst->offset;

	COMMAND(cmd,
		BITS(0, 1, 18) |		/* DISCARD : Test purpose */
		BITS(0, 1, 7),			/* SRC_CPU_ENB : 1: CPU FIFO */
		NX_G2D_CMD_SRC_CTRL);
}

static void
g2d_op_blend_write_mask(struct nx_g2d_cmd *cmd, int wmask)
{
	COMMAND(cmd,
		BITS(wmask, 4, 28),		/* WRITE_MASK */
		NX_G2D_CMD_BLEND_EQUAT_ALPHA);
}

static void
g2d_op_blend_rop_mode(struct nx_g2d_cmd *cmd,
		      enum nx_g2d_blend_rop_mode rop, bool enb)
{
	COMMAND(cmd,
		BITS((enb ? 1 : 0), 1, 27) |	/* ROP_ENB */
		BITS(rop, 4, 23),		/* BLEND_ROP_MODE */
		NX_G2D_CMD_BLEND_EQUAT_ALPHA);
}

static void
g2d_op_blend_function(struct nx_g2d_cmd *cmd,
		int src_rgb, int dst_rgb,
		int src_alpha, int dst_alpha,
		int equat_rgb, int equat_alpha)
{
	COMMAND(cmd,
		BITS(src_rgb, 4, 18) |		/* BLEND_SRC_RGB */
		BITS(dst_rgb, 4, 14) |		/* BLEND_DST_RGB */
		BITS(src_alpha, 4, 10) |	/* BLEND_SRC_ALPHA */
		BITS(dst_alpha, 4, 6) |		/* BLEND_DST_ALPHA */
		BITS(equat_rgb, 3, 3) |		/* BLEND_EQUAT_RGB */
		BITS(equat_alpha, 3, 0),	/* BLEND_EQUAT_ALPHA */
		NX_G2D_CMD_BLEND_EQUAT_ALPHA);
}

static void
g2d_op_blend_color(struct nx_g2d_cmd *cmd, unsigned int RGBA, bool enb)
{
	COMMAND(cmd,
		RGBA,				/* BLEND_COLOR */
		NX_G2D_CMD_BLEND_COLOR);

	COMMAND(cmd,
		BITS(enb ? 1 : 0, 1, 22),	/* BLEND_ENB */
		NX_G2D_CMD_BLEND_EQUAT_ALPHA);
}

static void
g2d_op_image_size(struct nx_g2d_cmd *cmd, int width, int height)
{
	COMMAND(cmd,
		BITS(height - 1, 12, 16) |	/* HEIGHT */
		BITS(width - 1, 12, 0),		/* WIDTH */
		NX_G2D_CMD_SIZE);
}

static void
g2d_op_src_read_enb(struct nx_g2d_cmd *cmd, bool enb)
{
	COMMAND(cmd,
		BITS(enb ? 1 : 0, 1, 6),	/* SRC_RD_ENB */
		NX_G2D_CMD_SRC_CTRL);
}

static void
g2d_op_src_force_alpha(struct nx_g2d_cmd *cmd, int alpha, bool enb)
{
	COMMAND(cmd,
		BITS(enb, 1, 16) |		/* SRC_FORCE_ALPHA_ENB */
		BITS(alpha, 8, 8),		/* SRC_FORCE_ALPHA */
		NX_G2D_CMD_SRC_CTRL);
}

static void
g2d_op_src_image_format(struct nx_g2d_cmd *cmd, int pixelorder, int pixelformat)
{
	COMMAND(cmd,
		BITS(pixelorder, 2, 4) |	/* SRC_COLOR_ORDER */
		BITS(pixelformat, 4, 0),	/* SRC_COLOR_FMT */
		NX_G2D_CMD_SRC_CTRL);
}

static void
g2d_op_src_image_size(struct nx_g2d_cmd *cmd, int width, int pitch,
		      int pixelbyte)
{
	COMMAND(cmd,
		pitch,				/* SRC_STRIDE */
		NX_G2D_CMD_SRC_STRIDE);
	COMMAND(cmd,
		(width * pixelbyte),		/* SRC_BLKSIZE */
		NX_G2D_CMD_SRC_BLKSIZE);
}

static void
g2d_op_dst_read_enb(struct nx_g2d_cmd *cmd, bool enb)
{
	COMMAND(cmd,
		BITS(enb ? 1 : 0, 1, 6),	/* DST_RD_ENB */
		NX_G2D_CMD_DST_CTRL);
}

static void
g2d_op_dst_dither(struct nx_g2d_cmd *cmd, int x_offs, int y_offs, bool enb)
{
	COMMAND(cmd,
		BITS(x_offs, 2, 10) |		/* DITER_X_OFFSET */
		BITS(y_offs, 2, 8) |		/* DITER_Y_OFFSET */
		BITS(enb, 1, 7),		/* DITHER_ENB */
		NX_G2D_CMD_DST_CTRL);
}

static void
g2d_op_dst_image_format(struct nx_g2d_cmd *cmd, int pixelorder, int pixelformat)
{
	COMMAND(cmd,
		BITS(pixelorder, 2, 4) |	/* DST_COLOR_ORDER */
		BITS(pixelformat, 4, 0),	/* DST_COLOR_FMT */
		NX_G2D_CMD_DST_CTRL);
}

static void
g2d_op_dst_image_size(struct nx_g2d_cmd *cmd, int width, int pitch,
		      int pixelbyte)
{
	COMMAND(cmd,
		pitch,				/* DST_STRIDE */
		NX_G2D_CMD_DST_STRIDE);
	COMMAND(cmd,
		(width * pixelbyte),		/* DST_BLKSIZE */
		NX_G2D_CMD_DST_BLKSIZE);
}

static void
g2d_op_solid_color(struct nx_g2d_cmd *cmd, unsigned int color, bool enb)
{
	COMMAND(cmd,
		BITS(enb ? 1 : 0, 1, 17),	/* SOLID_ENB */
		NX_G2D_CMD_SRC_CTRL);
	COMMAND(cmd,
		color,				/* SOLID_COLOR */
		NX_G2D_CMD_SOLID_COLOR);
}

static int
g2d_submit(struct nx_g2d_ctx *ctx, struct nx_g2d_cmd *cmd)
{
	struct nx_g2d_cmd arg = *cmd;
	int ret;

	ret = drmIoctl(ctx->fd, DRM_IOCTL_EXEC, &arg);
	if (ret < 0) {
		D_ERROR("%s() Failed DRM_IOCTL_EXEC\n", __func__);
		return ret;
	}

	return ret;
}

static int
g2d_sync(struct nx_g2d_ctx *ctx, struct nx_g2d_cmd *cmd)
{
	struct nx_g2d_cmd arg = *cmd;
	int ret;

	ret = drmIoctl(ctx->fd, DRM_IOCTL_SYNC, &cmd);
	if (ret < 0) {
		D_ERROR("%s() Failed DRM_IOCTL_SYNC\n", __func__);
		return ret;
	}

	return ret;
}

static int
g2d_get_ver(int fd, struct nx_g2d_ver *ver)
{
	struct nx_g2d_ver arg = { 0 };
	int ret;

	ret = drmIoctl(fd, DRM_IOCTL_NX_G2D_GET_VER, &arg);
	if (ret < 0) {
		D_ERROR("%s() Failed DRM_IOCTL_NX_G2D_GET_VER\n", __func__);
		return ret;
	}

	*ver = arg;

	return ret;
}

drm_public
struct nx_g2d_ctx *nexell_g2d_alloc(int fd, int *major, int *minor)
{
	struct nx_g2d_ctx *ctx;
	struct nx_g2d_ver ver = { 0 };
	int ret;

	ret = g2d_get_ver(fd, &ver);
	if (ret || ver.major != NX_G2D_DRIVER_VERSION_MAJOR) {
		D_ERROR("%s() Not Support VERSION GFX:%d-%d, DRIVER:%d-%d\n",
			__func__,
			NX_G2D_DRIVER_VERSION_MAJOR, NX_G2D_DRIVER_VERSION_MINOR,
			ver.major, ver.minor);
		return NULL;
	}

	ctx = calloc(1, sizeof(*ctx));
	if (!ctx)
		return NULL;

	ctx->fd = fd;

	if (major)
		*major = ver.major;

	if (minor)
		*minor = ver.minor;

	return ctx;
}

drm_public
void nexell_g2d_free(struct nx_g2d_ctx *ctx)
{
	free(ctx);
}

drm_public
int nexell_g2d_fillrect(struct nx_g2d_ctx *ctx, struct nx_g2d_image *img)
{
	struct nx_g2d_cmd *cmd = &ctx->cmd;
	struct nx_g2d_image_obj *src = &img->src;
	struct nx_g2d_image_obj *dst = &img->dst;
	unsigned int solidcolor = ARGB_COLOR(img->color.a,
					img->color.r,
					img->color.g,
					img->color.b);

	/* clear source buffer type */
	src->buf_type = NX_G2D_BUF_TYPE_NONE;

	g2d_op_initialize(cmd, img);

	g2d_op_blend_write_mask(cmd, 0xf);
	g2d_op_blend_rop_mode(cmd, 0, false);

	/* must be false */
	g2d_op_blend_color(cmd, img->blendcolor, false);

	g2d_op_image_size(cmd, img->width, img->height);
	g2d_op_solid_color(cmd, solidcolor, true);

	/* set with destination format to source format */
	g2d_op_src_image_format(cmd, dst->pixelorder, dst->pixelformat);
	g2d_op_src_force_alpha(cmd, 0, false);
	g2d_op_src_read_enb(cmd, false);

	g2d_op_dst_image_format(cmd, dst->pixelorder, dst->pixelformat);
	g2d_op_dst_image_size(cmd, img->width, dst->pitch, dst->pixelbyte);
	g2d_op_dst_dither(cmd, 0, 0, DITHER_ON(img->dst.pixelbyte));
	g2d_op_dst_read_enb(cmd, false);

	return g2d_submit(ctx, cmd);
}

drm_public int
nexell_g2d_blit(struct nx_g2d_ctx *ctx, struct nx_g2d_image *img)
{
	struct nx_g2d_cmd *cmd = &ctx->cmd;
	struct nx_g2d_image_obj *src = &img->src;
	struct nx_g2d_image_obj *dst = &img->dst;

	int src_rgb = src->blend_func;
	int dst_rgb = dst->blend_func;
	int src_alpha = src->blend_func;
	int dst_alpha = dst->blend_func;
	int equat_rgb = GL_BLEND_EQUAT_FUNC_ADD;
	int equat_alpha = GL_BLEND_EQUAT_FUNC_ADD;

	g2d_op_initialize(cmd, img);

	g2d_op_blend_write_mask(cmd, 0xf);
	g2d_op_blend_rop_mode(cmd, 0, false);
	g2d_op_blend_function(cmd,
			src_rgb, dst_rgb,
			src_alpha, dst_alpha,
			equat_rgb, equat_alpha);
	g2d_op_blend_color(cmd, img->blendcolor, true); /* true */

	g2d_op_image_size(cmd, img->width, img->height);

	/* must be set false */
	g2d_op_solid_color(cmd, 0, false);

	g2d_op_src_image_format(cmd, src->pixelorder, src->pixelformat);
	g2d_op_src_image_size(cmd, img->width, src->pitch, src->pixelbyte);
	g2d_op_src_force_alpha(cmd, 0, false);
	g2d_op_src_read_enb(cmd, true);

	g2d_op_dst_image_format(cmd, dst->pixelorder, dst->pixelformat);
	g2d_op_dst_image_size(cmd, img->width, dst->pitch, dst->pixelbyte);
	g2d_op_dst_dither(cmd, 0, 0, DITHER_ON(img->dst.pixelbyte));
	g2d_op_dst_read_enb(cmd, true);

	return g2d_submit(ctx, cmd);
}

drm_public
int nexell_g2d_sync(struct nx_g2d_ctx *ctx)
{
	return g2d_sync(ctx, &ctx->cmd);
}
