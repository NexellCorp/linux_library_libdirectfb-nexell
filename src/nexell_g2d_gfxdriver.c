/*
   Nexell driver - 2D Acceleration

   (c) Copyright 2019 Nexell Co.

   Written by JungHyun Kim <jhkim@nexell.co.kr>

   All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <dfb_types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <directfb.h>
#include <core/system.h>
#include <gfx/convert.h>
#include <drmkms_system/drmkms_system.h>

#include "nexell_g2d_gfxdriver.h"

D_DEBUG_DOMAIN(NEXELL_2D, "Nexell/G2D", "Nexell G2D Acceleration");

#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER(nexell)

#define	DRM_DEVICE_NAME	"/dev/dri/card0"

/* refer to include/directfb.h */
typedef struct {
	DFBSurfacePixelFormat dfb_pixelformat;
	enum nx_g2d_pixel_format pixelformat;
	int pixelbyte, pixelorder;
} NXG2DSurfacePixelFormat;

static NXG2DSurfacePixelFormat NXG2DSupportPixelFormats[] = {
	{ DSPF_RGB16, NX_G2D_PIXEL_FMT_RGB565, 2, NX_G2D_PIXEL_ORDER_ARGB },
	{ DSPF_RGB555, NX_G2D_PIXEL_FMT_XRGB1555, 2, NX_G2D_PIXEL_ORDER_ARGB },
	{ DSPF_BGR555, NX_G2D_PIXEL_FMT_XRGB1555, 2, NX_G2D_PIXEL_ORDER_ABGR },
	{ DSPF_ARGB1555, NX_G2D_PIXEL_FMT_ARGB1555, 2, NX_G2D_PIXEL_ORDER_ARGB },
	{ DSPF_RGBA5551, NX_G2D_PIXEL_FMT_ARGB1555, 2, NX_G2D_PIXEL_ORDER_RGBA },
	{ DSPF_RGB444, NX_G2D_PIXEL_FMT_XRGB4444, 2, NX_G2D_PIXEL_ORDER_ARGB },
	{ DSPF_ARGB4444, NX_G2D_PIXEL_FMT_ARGB4444, 2, NX_G2D_PIXEL_ORDER_ARGB },
	{ DSPF_RGBA4444, NX_G2D_PIXEL_FMT_ARGB4444, 2, NX_G2D_PIXEL_ORDER_RGBA },
	{ DSPF_RGB24, NX_G2D_PIXEL_FMT_RGB888, 3, NX_G2D_PIXEL_ORDER_ARGB },
	{ DSPF_RGB32, NX_G2D_PIXEL_FMT_XRGB8888, 4, NX_G2D_PIXEL_ORDER_ARGB },
	{ DSPF_ARGB, NX_G2D_PIXEL_FMT_ARGB8888, 4, NX_G2D_PIXEL_ORDER_ARGB },
	{ DSPF_ABGR, NX_G2D_PIXEL_FMT_ARGB8888, 4, NX_G2D_PIXEL_ORDER_ABGR },
};

#define DFB_SUPPORT_FORMAT_SIZE	D_ARRAY_SIZE(NXG2DSupportPixelFormats)

enum {
	DESTINATION	= BIT(0),
	SOURCE		= BIT(1),
	BLEND_FUNC	= BIT(2),
	BLITTING	= BIT(3),
	CLIP		= BIT(4),
	RENDER_OPTS	= BIT(5),
	COLOR		= BIT(6),
	ALL		= BIT(7) - 1,
};

#define NXG2D_VALIDATE(flags)		(nxdev->v_flags |= (flags))
#define NXG2D_INVALIDATE(flag)		(nxdev->v_flags &= ~(flag))
#define NXG2D_CHECK_VALIDATE(flag)	do { \
		if (!(nxdev->v_flags & flag)) {  \
			nx_##flag(nxdrv, nxdev, state);   \
			NXG2D_VALIDATE(flag); \
		} \
	} while (0)

static NXG2DBlendFunction
nxBlendFunction(DFBSurfaceBlendFunction blend)
{
	NXG2DBlendFunction nxfunc = GL_BLEND_ZERO;

	switch (blend) {
	case DSBF_ZERO:
		nxfunc = GL_BLEND_ZERO;
		break;
	case DSBF_ONE:
		nxfunc = GL_BLEND_ONE;
		break;
	case DSBF_SRCCOLOR:
		nxfunc = GL_BLEND_SRC_COLOR;
		break;
	case DSBF_INVSRCCOLOR:
		nxfunc = GL_BLEND_ONE_MINUS_SRC_COLOR;
		break;
	case DSBF_SRCALPHA:
		nxfunc = GL_BLEND_SRC_ALPHA;
		break;
	case DSBF_INVSRCALPHA:
		nxfunc = GL_BLEND_ONE_MINUS_SRC_ALPHA;
		break;
	case DSBF_DESTCOLOR:
		nxfunc = GL_BLEND_DST_COLOR;
		break;
	case DSBF_INVDESTCOLOR:
		nxfunc = GL_BLEND_ONE_MINUS_DST_COLOR;
		break;
	case DSBF_DESTALPHA:
		nxfunc = GL_BLEND_DST_ALPHA;
		break;
	case DSBF_INVDESTALPHA:
		nxfunc = GL_BLEND_ONE_MINUS_DST_ALPHA;
		break;
	case DSBF_SRCALPHASAT:
		nxfunc = GL_BLEND_SRC_ALPHA_SATURATE;
		break;
	default:
		D_BUG("unexpected src blend function %d", blend);
		break;
	}

	return nxfunc;
}

/*
 * Set State routines
 */
static inline void
nx_SOURCE(NXG2DDriverData *nxdrv,
	  NXG2DDeviceData *nxdev,
	  CardState *state)
{
	CoreSurface *surface = state->source;
	DFBSurfacePixelFormat format = surface->config.format;
	NXG2DSurfacePixelFormat *nxformat = NXG2DSupportPixelFormats;
	NXG2DImageObject *obj = &nxdev->source;
	int i;

	D_DEBUG_AT(NEXELL_2D,
		"%s() %s:%s, addr:0x%08x:0x%08x, size:%d, handle:0x%x\n",
		__FUNCTION__, dfb_pixelformat_name(format),
		dfb_pixelformat_name(state->src.buffer->format),
		state->src.addr, state->src.phys, state->src.allocation->size,
		state->src.handle);

	for (i  = 0; i < DFB_SUPPORT_FORMAT_SIZE; i++, nxformat++) {
		if (format == nxformat->dfb_pixelformat) {
			obj->pixelbyte = nxformat->pixelbyte;
			obj->pixelformat = nxformat->pixelformat;
			obj->pixelorder = nxformat->pixelorder;
			obj->pitch = state->src.pitch;
			obj->buf_type = nxdev->buf_type;

			if (obj->buf_type == NX_G2D_BUF_TYPE_GEM)
				obj->handle = (u32)state->src.handle;
			else
				obj->handle = state->src.phys;
			break;
		}
	}

	if (i == DFB_SUPPORT_FORMAT_SIZE)
		D_BUG("Unexpected source pixelformat: %s\n",
			dfb_pixelformat_name(format));

	D_DEBUG_AT(NEXELL_2D, "%s() %s (%d:%d), byte:%d, pitch:%d, handle:%d\n",
		__FUNCTION__, dfb_pixelformat_name(format),
		obj->pixelformat, obj->pixelorder, obj->pixelbyte,
		obj->pitch, obj->handle);
}

static inline void
nx_DESTINATION(NXG2DDriverData *nxdrv,
	       NXG2DDeviceData *nxdev,
	       CardState *state)
{
	CoreSurface *surface = state->destination;
	DFBSurfacePixelFormat format = surface->config.format;
	NXG2DSurfacePixelFormat *nxformat = NXG2DSupportPixelFormats;
	NXG2DImageObject *obj = &nxdev->destination;
	int i;

	D_DEBUG_AT(NEXELL_2D,
		"%s() %s:%s, addr:0x%08x:0x%08x, size:%d, handle:0x%x\n",
		__FUNCTION__, dfb_pixelformat_name(format),
		dfb_pixelformat_name(state->dst.buffer->format),
		state->dst.addr, state->dst.phys, state->dst.allocation->size, state->dst.handle);

	for (i  = 0; i < DFB_SUPPORT_FORMAT_SIZE; i++, nxformat++) {
		if (format == nxformat->dfb_pixelformat) {
			obj->pixelbyte = nxformat->pixelbyte;
			obj->pixelformat = nxformat->pixelformat;
			obj->pixelorder = nxformat->pixelorder;
			obj->pitch = state->dst.pitch;
			obj->buf_type = nxdev->buf_type;

			if (obj->buf_type == NX_G2D_BUF_TYPE_GEM)
				obj->handle = (u32)state->dst.handle;
			else
				obj->handle = state->dst.phys;
			break;
		}
	}

	if (i == DFB_SUPPORT_FORMAT_SIZE)
		D_BUG("Unexpected destination pixelformat: %s\n",
			dfb_pixelformat_name(format));

	D_DEBUG_AT(NEXELL_2D, "%s() %s (%d:%d), byte:%d, pitch:%d, handle:%d\n",
		__FUNCTION__, dfb_pixelformat_name(format),
		obj->pixelformat, obj->pixelorder, obj->pixelbyte,
		obj->pitch, obj->handle);
}

static inline void
nx_COLOR(NXG2DDriverData *nxdrv,
	 NXG2DDeviceData *nxdev,
	 CardState *state)
{
	nxdev->color = state->color;

	D_DEBUG_AT(NEXELL_2D,
		"%s() A:0x%02x, R:0x%02x, G:0x%02x, B:0x%02x, color:0x%08x\n",
		__FUNCTION__,
		state->color.a, state->color.r, state->color.g, state->color.b,
		PIXEL_ARGB(state->color.a, state->color.r,
			   state->color.g, state->color.b));
}

static inline void
nx_BLEND_FUNC(NXG2DDriverData *nxdrv,
	      NXG2DDeviceData *nxdev,
	      CardState *state)
{
	NXG2DImageObject *src = &nxdev->source;
	NXG2DImageObject *dst = &nxdev->destination;

	src->blend_func = nxBlendFunction(state->src_blend);
	dst->blend_func = nxBlendFunction(state->dst_blend);

	D_DEBUG_AT(NEXELL_2D, "%s() src: %d -> %d, dst: %d -> %d\n",
		__FUNCTION__, state->src_blend, src->blend_func,
		state->dst_blend, dst->blend_func);
}

static inline void
nx_CLIP(NXG2DDriverData *nxdrv,
	NXG2DDeviceData *nxdev,
	CardState *state)
{
	DFBRegion *clip = &state->clip;

	nxdev->clip.x1 = clip->x1;
	nxdev->clip.x2 = clip->x2 + 1;
	nxdev->clip.y1 = clip->y1;
	nxdev->clip.y2 = clip->y2 + 1;

	D_DEBUG_AT(NEXELL_2D, "%s() clip = L:%d T:%d W:%d H:%d\n",
		__FUNCTION__, clip->x1, clip->y1,
		nxdev->clip.x2 - nxdev->clip.x1, nxdev->clip.y2 - nxdev->clip.y1);
}

static void
nxCheckState(void *drv,
	     void *dev,
	     CardState *state,
	     DFBAccelerationMask accel)
{
	DFBSurfacePixelFormat dst_format = state->destination->config.format;
	DFBSurfacePixelFormat src_format =
		DFB_BLITTING_FUNCTION(accel) ? state->source->config.format : DSPF_UNKNOWN;
	NXG2DSurfacePixelFormat *nxformat = NXG2DSupportPixelFormats;
	int i;

	D_DEBUG_AT(NEXELL_2D,
		"%s() accel:0x%x(state:0x%x), drawing:0x%x, blitting:0x%x\n",
		__FUNCTION__, accel, state->accel,
		state->drawingflags, state->blittingflags);
	D_DEBUG_AT(NEXELL_2D, "%s() %s: format:%s -> %s\n",
		__FUNCTION__, DFB_DRAWING_FUNCTION(accel) ? "DRAW" : "BLIT",
		dfb_pixelformat_name(dst_format),
		src_format != DSPF_UNKNOWN ? dfb_pixelformat_name(src_format) : "None");

	/*
	 * Check Source Format
	 */
	if (src_format != DSPF_UNKNOWN && DFB_COLOR_IS_YUV(src_format))
		return;

	for (i  = 0; src_format != DSPF_UNKNOWN &&
	     i < DFB_SUPPORT_FORMAT_SIZE; i++, nxformat++) {
		if (src_format == nxformat->dfb_pixelformat)
			break;
	}

	if (i == DFB_SUPPORT_FORMAT_SIZE)
		return;

	/*
	 * Check Destination Format
	 */
	if (DFB_COLOR_IS_YUV(dst_format))
		return;

	for (i  = 0; i < DFB_SUPPORT_FORMAT_SIZE; i++, nxformat++) {
		if (dst_format == nxformat->dfb_pixelformat)
			break;
	}

	if (i == DFB_SUPPORT_FORMAT_SIZE)
		return;

	if (!(accel & ~NXG2D_SUPPORTED_DRAWINGFUNCTIONS) &&
	    !(state->drawingflags & ~NXG2D_SUPPORTED_DRAWINGFLAGS))
		state->accel |= NXG2D_SUPPORTED_DRAWINGFUNCTIONS;

	if (!(accel & ~NXG2D_SUPPORTED_BLITTINGFUNCTIONS) &&
	    !(state->blittingflags & ~NXG2D_SUPPORTED_BLITTINGFLAGS)) {
		if (state->source->config.format == state->destination->config.format)
			state->accel |= NXG2D_SUPPORTED_BLITTINGFUNCTIONS;
	}

	D_DEBUG_AT(NEXELL_2D, "%s() accel:0x%x(state:0x%x), %s\n",
		__FUNCTION__, accel, state->accel, state->accel ? "G2D" : "SW");
}

static void
nxSetState(void *drv,
	   void *dev,
	   GraphicsDeviceFuncs *funcs,
	   CardState *state,
	   DFBAccelerationMask accel)
{
	NXG2DDriverData *nxdrv = (NXG2DDriverData *)drv;
	NXG2DDeviceData *nxdev = (NXG2DDeviceData *)dev;
	StateModificationFlags modified = state->mod_hw;

	D_DEBUG_AT(NEXELL_2D, "%s() accel:0x%x(0x%x), modified:0x%x\n",
		__FUNCTION__, accel, state->accel, modified);
	D_DEBUG_AT(NEXELL_2D, "%s() blitting:0x%x, ROP:0x%x\n",
		__FUNCTION__, state->blittingflags, state->render_options);

	/*
	 * Invalidate hardware states
	 * Each modification to the hw independent state invalidates
	 * one or more hardware states.
	 */
	if (modified == SMF_ALL) {
		D_DEBUG_AT(NEXELL_2D, "  <- ALL\n");
		NXG2D_INVALIDATE(ALL);
	} else if (modified) {
		if (modified & SMF_DESTINATION) {
			D_DEBUG_AT(NEXELL_2D, "  <- DESTINATION\n");
			NXG2D_INVALIDATE(DESTINATION);
		}

		if ((modified & SMF_SOURCE) && state->source) {
			D_DEBUG_AT(NEXELL_2D, "  <- SOURCE\n");
			NXG2D_INVALIDATE(SOURCE);
		}

		if (modified & SMF_COLOR) {
			D_DEBUG_AT(NEXELL_2D, "  <- COLOR\n");
			NXG2D_INVALIDATE(COLOR);
		}

		if (modified & (SMF_BLITTING_FLAGS | SMF_SRC_BLEND | SMF_DST_BLEND)) {
			D_DEBUG_AT(NEXELL_2D, "  <- BLIT_BLEND\n");
			NXG2D_INVALIDATE(BLEND_FUNC);
		}

		if (modified & (SMF_DRAWING_FLAGS | SMF_SRC_BLEND | SMF_DST_BLEND)) {
			D_DEBUG_AT(NEXELL_2D, "  <- DRAW_BLEND\n");
			NXG2D_INVALIDATE(BLEND_FUNC);
		}
	}

	/* Always requiring valid destination... */
	NXG2D_CHECK_VALIDATE(DESTINATION);

	switch (accel) {
	case DFXL_FILLRECTANGLE:
		D_DEBUG_AT(NEXELL_2D, "  -> FILLRECTANGLE\n");
		NXG2D_CHECK_VALIDATE(COLOR);
		NXG2D_CHECK_VALIDATE(BLEND_FUNC);
		state->set |= DFXL_FILLRECTANGLE;
		break;
	case DFXL_BLIT:
		D_DEBUG_AT(NEXELL_2D, "  -> BLIT\n");
		NXG2D_CHECK_VALIDATE(SOURCE);
		NXG2D_CHECK_VALIDATE(COLOR);
		NXG2D_CHECK_VALIDATE(BLEND_FUNC);
		state->set |= DFXL_BLIT;
		break;
	default:
		D_BUG("unexpected drawing/blitting function");
	}

	/*
	 * Clear modification flags
	 *
	 * All flags have been evaluated in 1) and remembered for further validation.
	 * If the hw independent state is not modified, this function won't get called
	 * for subsequent rendering functions, unless they aren't defined by 3).
	 */
	state->mod_hw = 0;
}

static bool
nxBlit(void *drv, void *dev, DFBRectangle *rect, int dx, int dy)
{
	NXG2DDriverData *nxdrv = (NXG2DDriverData *)drv;
	NXG2DDeviceData *nxdev = (NXG2DDeviceData *)dev;
	NXG2DImageObject *src = &nxdev->source;
	NXG2DImageObject *dst = &nxdev->destination;
	struct nx_g2d_image img = { 0, };

	D_DEBUG_AT(NEXELL_2D,
		"%s() X:%d, Y:%d, L:%d T:%d W:%d H:%d\n",
		__FUNCTION__, dx, dy, rect->x, rect->y, rect->w, rect->h);
	D_DEBUG_AT(NEXELL_2D,
		"%s() L:%d T:%d W:%d H:%d\n",
		__FUNCTION__, rect->x, rect->y, rect->w, rect->h);

	dst->offset = (dx * dst->pixelbyte) + (dy * dst->pitch);
	src->offset = (rect->x * src->pixelbyte) + (rect->y * src->pitch);

	img.width = rect->w;
	img.height = rect->h;
	img.dst = nxdev->destination;
	img.src = nxdev->source;

	img.color.a = nxdev->color.a;
	img.color.r = nxdev->color.r;
	img.color.g = nxdev->color.g;
	img.color.b = nxdev->color.b;

	img.blendcolor = RGBA_COLOR(0xff, 0xff, 0xff, 0xff);

	return nexell_g2d_blit(nxdrv->ctx, &img) ? false : true;
}

static bool
nxFillRectangle(void *drv, void *dev, DFBRectangle *rect)
{
	NXG2DDriverData *nxdrv = (NXG2DDriverData *)drv;
	NXG2DDeviceData *nxdev = (NXG2DDeviceData *)dev;
	NXG2DImageObject *src = &nxdev->source;
	NXG2DImageObject *dst = &nxdev->destination;
	struct nx_g2d_image img = { 0, };
	unsigned int color = PIXEL_ARGB(nxdev->color.a,
					nxdev->color.r,
					nxdev->color.g,
					nxdev->color.b);

	D_DEBUG_AT(NEXELL_2D,
		"%s() color:0x%x, L:%d T:%d W:%d H:%d, %dbpp, %dpitch\n",
		__FUNCTION__, color, rect->x, rect->y, rect->w, rect->h,
		dst->pixelbyte * 8, dst->pitch);

	dst->offset = (rect->x * dst->pixelbyte) + (rect->y * dst->pitch);

	img.width = rect->w;
	img.height = rect->h;

	img.src = nxdev->source;
	img.dst = nxdev->destination;

	img.color.a = nxdev->color.a;
	img.color.r = nxdev->color.r;
	img.color.g = nxdev->color.g;
	img.color.b = nxdev->color.b;

	img.blendcolor = RGBA_COLOR(0xff, 0xff, 0xff, 0xff);

	return nexell_g2d_fillrect(nxdrv->ctx, &img) ? false : true;
}

static DFBResult
nxEngineSync(void *drv, void *dev)
{
	NXG2DDriverData *nxdrv = (NXG2DDriverData *)drv;

	D_DEBUG_AT(NEXELL_2D, "%s()\n", __FUNCTION__);

	return nexell_g2d_sync(nxdrv->ctx) ? DFB_FAILURE : DFB_OK;
}

static DFBResult
nxDRMInitialize(CoreGraphicsDevice *device,
	void *driver_data)
{
	NXG2DDriverData *nxdrv = driver_data;

	nxdrv->drmfd = open(DRM_DEVICE_NAME, O_RDWR);
	if (nxdrv->drmfd < 0) {
		D_ERROR("%s() Failed to open '%s'!\n", __FUNCTION__, DRM_DEVICE_NAME);
		return  errno2result(errno);
	}

	return DFB_OK;
}

static void
nxDRMDeInitialize(void *driver_data)
{
	NXG2DDriverData *nxdrv = driver_data;

	close(nxdrv->drmfd);
}

static DFBResult
nxOpen(CoreGraphicsDevice *device,
	NXG2DDriverData *nxdrv)
{
	NXG2DDeviceData *nxdev = nxdrv->dev;
	int fd, major, minor;
	int ret;

	D_DEBUG_AT(NEXELL_2D, "%s() %s\n", __FUNCTION__, dfb_config->system);

	if (!strcmp(dfb_config->system, "drmkms"))
		nxdrv->system_type = DFB_SYSTEM_DRMKMS;
	else if (!strcmp(dfb_config->system, "fbdev"))
		nxdrv->system_type = DFB_SYSTEM_FBDEV;
	else if (!strcmp(dfb_config->system, "devmem"))
		nxdrv->system_type = DFB_SYSTEM_DEVMEM;
	else
		nxdrv->system_type = DFB_SYSTEM_OTHER;

	if (nxdrv->system_type == DFB_SYSTEM_DEVMEM ||
	    nxdrv->system_type == DFB_SYSTEM_OTHER) {
		D_ERROR("%s() Not support: %s\n", __FUNCTION__, dfb_config->system);
		return DFB_FAILURE;
	}

	if (nxdrv->system_type == DFB_SYSTEM_DRMKMS) {
		nxdrv->drmkms = dfb_system_data();
		nxdev->buf_type = NX_G2D_BUF_TYPE_GEM;
		fd = nxdrv->drmkms->fd;
	} else {
		ret = nxDRMInitialize(device, nxdrv);
		if (ret)
			return ret;

		nxdev->buf_type = NX_G2D_BUF_TYPE_CPU;
		fd = nxdrv->drmfd;
	}

	nxdrv->ctx = nexell_g2d_alloc(fd, &major, &minor);
	if (!nxdrv->ctx)
		return DFB_IO;

	D_INFO("%s VERSION GFX:%d-%d, DRIVER:%d-%d\n",
		DFB_G2D_DRIVER_NAME,
		NX_G2D_DRIVER_VERSION_MAJOR, NX_G2D_DRIVER_VERSION_MINOR,
		major, minor);

	D_FLAGS_SET(nxdrv->flags, NXG2D_FLAGS_OPEN);

	return DFB_OK;
}

static void
nxClose(NXG2DDriverData *nxdrv)
{
	D_DEBUG_AT(NEXELL_2D, "%s()\n", __FUNCTION__);

	if (!(nxdrv->flags & NXG2D_FLAGS_OPEN))
		return;

	nexell_g2d_free(nxdrv->ctx);
	nxdrv->flags &= ~NXG2D_FLAGS_OPEN;

	if (nxdrv->system_type == DFB_SYSTEM_DRMKMS)
		return;

	nxDRMDeInitialize(nxdrv);
}

static int
driver_probe(CoreGraphicsDevice *device)
{
	unsigned long accel = dfb_gfxcard_get_accelerator(device);

	D_DEBUG_AT(NEXELL_2D, "%s() accelator:0x%x\n", __FUNCTION__, accel);

	switch (accel) {
	case FB_ACCEL_ID_NXP3220: /* accelerator = 12832 */
		return 1;
	}

	return 0;
}


static void
driver_get_info(CoreGraphicsDevice *device,
		GraphicsDriverInfo *info)
{
	D_DEBUG_AT(NEXELL_2D, "%s()\n", __FUNCTION__);

	/* fill driver info structure */
	snprintf(info->name,
		DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
		DFB_G2D_DRIVER_NAME);

	snprintf(info->vendor,
		DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
		DFB_G2D_DRIVER_VENDOR);

	snprintf(info->url,
		DFB_GRAPHICS_DRIVER_INFO_URL_LENGTH,
		DFB_G2D_DRIVER_URL);

	snprintf(info->license,
		DFB_GRAPHICS_DRIVER_INFO_LICENSE_LENGTH,
		DFB_G2D_DRIVER_LICENSE);

	info->version.major = NX_G2D_DRIVER_VERSION_MAJOR;
	info->version.minor = NX_G2D_DRIVER_VERSION_MINOR;

	info->driver_data_size = sizeof(NXG2DDriverData);
	info->device_data_size = sizeof(NXG2DDeviceData);
}

static DFBResult
driver_init_driver(CoreGraphicsDevice *device,
		   GraphicsDeviceFuncs *funcs,
		   void *driver_data,
		   void *device_data,
		   CoreDFB *core)
{
	NXG2DDriverData *nxdrv = driver_data;
	NXG2DDeviceData *nxdev = device_data;
	DFBResult ret;

	D_DEBUG_AT(NEXELL_2D, "%s()\n", __FUNCTION__);

	nxdrv->dev = device_data;

	ret = nxOpen(device, nxdrv);
	if (ret)
		return ret;

	funcs->CheckState	= nxCheckState;
	funcs->SetState         = nxSetState;
	funcs->FillRectangle    = nxFillRectangle;
	funcs->Blit             = nxBlit;

	return DFB_OK;
}

static DFBResult
driver_init_device(CoreGraphicsDevice *device,
		   GraphicsDeviceInfo *device_info,
		   void *driver_data,
		   void *device_data)
{
	NXG2DDriverData *nxdrv = driver_data;
	NXG2DDeviceData *nxdev = device_data;

	D_DEBUG_AT(NEXELL_2D, "%s()\n", __FUNCTION__);

	/* fill device info */
	snprintf(device_info->name,
		 DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH,
		 DFB_G2D_DEVICE_INFO_NAME);

	snprintf(device_info->vendor,
		 DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH,
		 DFB_G2D_DRIVER_VENDOR);

	device_info->caps.flags = NXG2D_SUPPORTED_CARDFLAGS;
	device_info->caps.accel = NXG2D_SUPPORTED_DRAWINGFUNCTIONS |
					NXG2D_SUPPORTED_BLITTINGFUNCTIONS;
	device_info->caps.drawing = NXG2D_SUPPORTED_DRAWINGFLAGS;
	device_info->caps.blitting = NXG2D_SUPPORTED_BLITTINGFLAGS;

	device_info->limits.surface_byteoffset_alignment =
					DFB_G2D_SURFACE_BYTEOFFSET_ALIGN;
	device_info->limits.surface_pixelpitch_alignment =
					DFB_G2D_SURFACE_PIXELPITCH_ALIGN;

	return DFB_OK;
}

static void
driver_close_device(CoreGraphicsDevice *device,
		     void *driver_data,
		     void *device_data)
{
	NXG2DDeviceData *nxdev = (NXG2DDeviceData *) device_data;
	NXG2DDriverData *nxdrv = (NXG2DDriverData *) driver_data;

	D_DEBUG_AT(NEXELL_2D, "%s()\n", __FUNCTION__);
}

static void
driver_close_driver(CoreGraphicsDevice *device,
		    void *driver_data)
{
	NXG2DDriverData *nxdrv = (NXG2DDriverData *) driver_data;

	D_DEBUG_AT(NEXELL_2D, "%s()\n", __FUNCTION__);

	nxClose(nxdrv);
}

