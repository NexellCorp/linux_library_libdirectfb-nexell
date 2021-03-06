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

#ifndef __NEXELL_G2D_H__
#define __NEXELL_G2D_H__

#include <dfb_types.h>

#include "nexell_g2d.h"

/* ADD to /etc/directfbrc: accelerator = 12832 */
#define FB_ACCEL_ID_NXP3220			0x3220

#define DFB_G2D_DRIVER_VENDOR			"Nexell"
#define DFB_G2D_DEVICE_INFO_NAME		"nxp3220"
#define DFB_G2D_DRIVER_NAME			"Nexell/G2D"
#define DFB_G2D_DRIVER_URL			"nexell.co.kr"
#define DFB_G2D_DRIVER_LICENSE			"LGPL"
#define DFB_G2D_DRIVER_VERSION_MAJOR		1
#define DFB_G2D_DRIVER_VERSION_MINOR		0

#define DFB_G2D_SURFACE_BYTEOFFSET_ALIGN	1
#define DFB_G2D_SURFACE_PIXELPITCH_ALIGN	1

#define NXG2D_FLAGS_OPEN			(1<<0)

/* CAPT: DRAWING: DFXL_NONE, DFXL_FILLRECTANGLE */
#define NXG2D_SUPPORTED_DRAWINGFUNCTIONS   \
		(DFXL_NONE)
#define NXG2D_SUPPORTED_DRAWINGFLAGS	\
		(DSDRAW_NOFX)

/* CAPT: BLIT : DFXL_NONE, DFXL_BLIT */
#define NXG2D_SUPPORTED_BLITTINGFUNCTIONS  \
		(DFXL_BLIT)
#define NXG2D_SUPPORTED_BLITTINGFLAGS   \
		(DSBLIT_NOFX)

typedef struct nx_g2d_image_obj NXG2DImageObject;

typedef struct {
	NXG2DImageObject source;
	NXG2DImageObject destination;
	unsigned int fillcolor;
	DFBRegion clip;
	/* validation flags */
	u32 v_flags;
} NXG2DDeviceData;

typedef struct {
	NXG2DDeviceData *dev;
	DRMKMSData *drmkms;
	struct nx_g2d_ctx *ctx;
	u32 flags;
} NXG2DDriverData;

#endif /* __NEXELL_G2D_H__ */
