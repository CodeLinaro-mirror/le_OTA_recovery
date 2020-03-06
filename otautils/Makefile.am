AM_CFLAGS := -Wundef \
        -Wstrict-prototypes \
        -Wno-trigraphs \
        -g -O0 \
        -fno-inline \
        -fno-short-enums \
        -fpic \
        -DUSE_LE_MODE

AM_CPPFLAGS := \
              -DUSE_LE_MODE \
              -I.

cpp_sources := \
        ota_utils.cpp

h_sources := \
        ota_utils.h

library_includedir := $(pkgincludedir)
library_include_HEADERS := $(h_sources)

lib_LTLIBRARIES := libotautils.la
libotautils_la_SOURCES := $(cpp_sources) $(h_sources)

libotautils_la_CFLAGS := $(AM_CFLAGS)
libotautils_la_CPPFLAGS := $(AM_CPPFLAGS)
libotautils_la_LDFLAGS := -all-static