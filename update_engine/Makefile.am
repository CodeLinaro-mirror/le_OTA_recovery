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
              -I.. \
               $(LIBSELINUX_CFLAGS) \
              -I$(WORKSPACE)/system/core/base/include \
              -I$(WORKSPACE)/system/core/include \
              -I$(WORKSPACE)/system/core/fs_mgr/include \
              -I$(WORKSPACE)/system/extras/ext4_utils \
              -I$(WORKSPACE)/OTA/system/core/base/include \
              -std=c++11

AM_LDFLAGS = -shared -version-info 1:0:0
c_sources = update_engine.cpp

library_includedir = $(pkgincludedir)
library_include_HEADERS = $(h_sources)

bin_PROGRAMS = update_engine
if USE_GLIB
update_engine_SOURCES = $(c_sources) $(h_sources)
update_engine_CFLAGS = $(AM_CFLAGS) -DUSE_GLIB @GLIB_CFLAGS@
update_engine_CPPFLAGS = $(AM_CPPFLAGS) -DUSE_GLIB @GLIB_CFLAGS@
else
update_engine_SOURCES = $(c_sources) $(h_sources)
update_engine_CFLAGS = $(AM_CFLAGS)
update_engine_CPPFLAGS = $(AM_CPPFLAGS)
endif

if TARGET_NAD_OTA
update_engine_CPPFLAGS += -DTARGET_NAD_OTA
endif

if TARGET_SUPPORTS_AB
update_engine_CPPFLAGS += -DTARGET_SUPPORTS_AB
if TARGET_NAD_OTA
update_engine_LDADD = -lnad_ab_al
else
update_engine_LDADD = -labctl
endif
endif

if TARGET_SUPPORTS_ABC
update_engine_CPPFLAGS += -DTARGET_SUPPORTS_ABC
if TARGET_NAD_OTA
update_engine_LDADD = -lnad_ab_al
else
update_engine_LDADD = -labctl
endif
endif
