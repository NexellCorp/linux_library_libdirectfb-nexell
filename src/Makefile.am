## Makefile.am for DirectFB/src/core/gfxcards/nexell

AM_CFLAGS = \
	$(WARN_CFLAGS) \
	-I${includedir} \
	-I${includedir}/nexell \
        -I$(includedir)/directfb \
        -I$(includedir)/directfb-internal \
	-I${includedir}/libdrm

nexell_LTLIBRARIES = libdirectfb_nexell.la

nexelldir = $(MODULEDIR)/gfxdrivers

libdirectfb_nexell_la_SOURCES = \
	nexell_debug.c \
	nexell_g2d.c \
	nexell_g2d_gfxdriver.c

libdirectfb_nexell_la_LDFLAGS = \
	-ldrm

include $(top_srcdir)/rules/libobject.make
