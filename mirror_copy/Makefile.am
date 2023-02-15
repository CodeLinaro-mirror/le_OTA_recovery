AM_CFLAGS = -Wundef \
            -Wstrict-prototypes \
            -Wno-trigraphs \
            -g -O0 \
            -fno-inline \
            -fno-short-enums \
            -fpic \
            $(LIBSELINUX_CFLAGS) \
            -I$(WORKSPACE)/system/core/include \
            -I$(WORKSPACE)/system/core/fs_mgr/include \
            -I$(WORKSPACE)/system/extras/ext4_utils \
            -D_GNU_SOURCE \
            -D_LARGEFILE64_SOURCE \
            -I..

AM_CPPFLAGS = \
              -DUSE_LE_MODE \
              -I.\
              -I../otafault \
              -I../updater \
              -I.. \
               $(LIBSELINUX_CFLAGS) \
              -I$(WORKSPACE)/system/core/base/include \
              -I$(WORKSPACE)/system/core/include \
              -I$(WORKSPACE)/system/core/fs_mgr/include \
              -I$(WORKSPACE)/system/extras/ext4_utils \
              -I$(WORKSPACE)/OTA/system/core/base/include \
              -std=c++11

AM_LDFLAGS = -shared -version-info 1:0:0
c_sources = mirror_copy.cpp
library_includedir = $(pkgincludedir)
library_include_HEADERS = $(h_sources)

bin_PROGRAMS = mirror_copy
if USE_GLIB
mirror_copy_SOURCES = $(c_sources) $(h_sources)
mirror_copy_CFLAGS = $(AM_CFLAGS) -DUSE_GLIB @GLIB_CFLAGS@
mirror_copy_CPPFLAGS = $(AM_CPPFLAGS) -DUSE_GLIB @GLIB_CFLAGS@
else
mirror_copy_SOURCES = $(c_sources) $(h_sources)
mirror_copy_CFLAGS = $(AM_CFLAGS)
mirror_copy_CPPFLAGS = $(AM_CPPFLAGS)
endif

if TARGET_SUPPORTS_AB
mirror_copy_CPPFLAGS += -DTARGET_SUPPORTS_AB
mirror_copy_LDADD = -labctl
endif

if TARGET_SUPPORTS_MIRROR_AB_COPY
mirror_copy_CPPFLAGS += -DTARGET_SUPPORTS_MIRROR_AB_COPY
endif

