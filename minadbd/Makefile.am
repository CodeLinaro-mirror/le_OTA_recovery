AM_CFLAGS := -O2 -g -DADB_HOST=0 -Wall -Wno-unused-parameter \
            -D_XOPEN_SOURCE -D_GNU_SOURCE -D_LINUX_

c_sources := adb.c \
	    fdevent.c \
	    transport.c \
	    transport_usb.c \
	    sockets.c \
	    services.c \
	    usb_linux_client.c \
	    utils.c

requiredlibs := -lcutils

lib_LTLIBRARIES := libminadbd.la

if USE_GLIB
libminadbd_la_SOURCES := $(c_sources) $(h_sources)
libminadbd_la_CFLAGS := $(AM_CFLAGS) -DUSE_GLIB @GLIB_CFLAGS@
libminadbd_la_CPPFLAGS := $(AM_CPPFLAGS) -DUSE_GLIB @GLIB_CFLAGS@
libminadbd_la_LIBADD := $(requiredlibs) @GLIB_LIBS@
libminadbd_la_LDFLAGS := -all-static
else
libminadbd_la_SOURCES := $(c_sources) $(h_sources)
libminadbd_la_CFLAGS := $(AM_CFLAGS)
libminadbd_la_CPPFLAGS := $(AM_CPPFLAGS)
libminadbd_la_LIBADD := $(requiredlibs)
libminadbd_la_LDFLAGS := -all-static
endif
