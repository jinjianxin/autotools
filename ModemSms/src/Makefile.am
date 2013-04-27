## Process this file with automake to produce Makefile.in

## Created by Anjuta

AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" $(MODEMSMS_CFLAGS)

#AM_CFLAGS =\
#	-std=c++0x \
#	 -Wall\
#	 -g 

AM_CXXFLAGS = \
			  -std=c++0x \
			  -Wall\
			  -g

bin_PROGRAMS = modemsms

modemsms_SOURCES = \
	main.cpp \
	blueHFPApi.cpp \
	blueHFPApi.h \
	blueutils.cpp \
	blueutils.h \
	unicode.cpp \
	unicode.h

modemsms_LDFLAGS = 

modemsms_LDADD = $(MODEMSMS_LIBS) -lX11 -lpthread

