/*
 * Copyright (C) 2019 Nexell Co.Ltd
 * Authors:
 *	JungHyun Kim <jhkim@nexell.co.kr>
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef NEXELL_DEBUG_H_
#define NEXELL_DEBUG_H_

#include <stdio.h>
#include <stdarg.h>

#ifndef NDEBUG
	#ifndef D_DEBUG
	typedef void (*nx_debug_msg_handler_t)(const char *file, int line,
						const char *function,
						int err, const char *fmt, ...);
	extern nx_debug_msg_handler_t nx_debug_msg;

	#define D_DEBUG(args...) \
		nx_debug_msg(__FILE__, __LINE__, __func__, errno, ##args)
#endif
#else
	#ifndef D_DEBUG
	#define D_DEBUG(args...) /* nop */
#endif
#endif

#ifndef D_INFO
	#define D_INFO(format, ...)	(fprintf(stdout, format, ##__VA_ARGS__))
#endif

#ifndef D_ERROR
	#define D_ERROR(format, ...) do { \
		fprintf(stderr, "Error : %s:%d: ", __func__, __LINE__); \
		fprintf(stderr, format, ##__VA_ARGS__); \
		} while (0)
#endif

#endif
