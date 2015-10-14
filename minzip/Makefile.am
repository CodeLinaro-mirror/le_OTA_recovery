AM_CFLAGS := -Wundef \
        -Wstrict-prototypes \
        -Wno-trigraphs \
        -g -O0 \
        -fno-inline \
        -fno-short-enums \
        -fpic \
        -D_GNU_SOURCE

AM_CPPFLAGS := \
              -I. \
              -I$(WORKSPACE)/external/safe-iop/include

c_sources := \
        Hash.c \
        SysUtil.c \
        DirUtil.c \
        Inlines.c \
        Zip.c

h_sources := \
        Bits.h \
        DirUtil.h \
        Hash.h \
        Log.h \
        SysUtil.h \
        Zip.h \
        inline_magic.h

required_libs := $(MINCRYPT_LIBS)

library_includedir := $(pkgincludedir)
library_include_HEADERS := $(h_sources)

lib_LTLIBRARIES := libminzip.la
libminzip_la_SOURCES := $(c_sources) $(h_sources)

libminzip_la_CFLAGS := $(AM_CFLAGS)
libminzip_la_CPPFLAGS := $(AM_CPPFLAGS)
libminzip_la_LDFLAGS := -all-static -lm -lpthread -lselinux
