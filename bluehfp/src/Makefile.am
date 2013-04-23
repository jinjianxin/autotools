## Process this file with automake to produce Makefile.in

## Created by Anjuta



AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" $(PHONE_DAEMON_CFLAGS)

AM_CFLAGS =\
	 -Wall\
	 -g 

#-fPIC \
#	 --share

#bin_PROGRAMS = libbluehfp.so
lib_LTLIBRARIES=libbluehfp.la
bindir = /usr/lib

libbluehfp_la_SOURCES = \
	blueHFPApi.cpp \
	blueHFPApi.h \
	blueutils.cpp \
	blueutils.h \
	unicode.cpp \
	unicode.h

libbluehfp_la_LDFLAGS =

libbluehfp_la_LIBADD = $(PHONE_DAEMON_LIBS)

include_HEADERS = \
	blueHFPApi.h \
	blueutils.h \
	unicode.h 

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libbluehfp-1.0.pc

EXTRA_DIST = \
	libbluehfp-1.0.pc.in
