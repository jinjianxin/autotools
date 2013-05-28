AM_CFLAGS = $(WARNING_CFLAGS) -I$(top_builddir) -DUSE_SECURITY

sbin_PROGRAMS  = asm-bridge

asm_bridge_SOURCES = asm-bridge.c asm-bridge.h
#asm_bridge_CFLAGS  = $(AM_CFLAGS) $(AUDIO_SESSION_MANAGER_CFLAGS)
asm_bridge_CFLAGS  = $(AM_CFLAGS) -I/usr/include/mmf
asm_bridge_LDADD   = ../../libmurphy-common.la -lpthread
