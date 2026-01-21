WORKSPACE := $(TOPDIR)/src

AM_CFLAGS = -Wundef \
            -Wstrict-prototypes \
            -Wno-trigraphs \
            -g -O0 \
            -fno-inline \
            -fno-short-enums \
            -fpic \
            -I$(WORKSPACE)/system/core/include \
            -I$(WORKSPACE)/system/core/fs_mgr/include \
            -I$(WORKSPACE)/system/extras/ext4_utils \
            -D_GNU_SOURCE \
            -D_LARGEFILE64_SOURCE \
            -I..

AM_CPPFLAGS = \
              -DUSE_LE_MODE \
              -DUSE_MUSL \
              -I.\
              -I../otafault \
              -I.. \
              -I$(WORKSPACE)/system/core/base/include \
              -I$(WORKSPACE)/system/core/include \
              -I$(WORKSPACE)/system/core/fs_mgr/include \
              -I$(WORKSPACE)/system/extras/ext4_utils \
              -I$(WORKSPACE)/OTA/system/core/base/include \
              -std=c++11

SELINUX_FLAGS = -DCONFIG_PACKAGE_SELINUX_POLICY \
                $(LIBSELINUX_CFLAGS)

if ENABLE_SELINUX_POLICY
AM_CPPFLAGS += $(SELINUX_FLAGS)
AM_CFLAGS   += $(SELINUX_FLAGS)
endif

AM_LDFLAGS = -shared -version-info 1:0:0
c_sources = install.cpp \
            blockimg.cpp \
            updater.cpp

library_includedir = $(pkgincludedir)
library_include_HEADERS = $(h_sources)

requiredlibs = -lm \
               -lpthread \
               -lz \
               -lcutils \
               -lext4_utils \
               -lcrypto \
               -llog \
               -lsparse \
               -lrecovery_updater_msm \
               ../mtdutils/libmtdutils.la \
               ../edify/libedify.la \
               ../minzip/libminzip.la \
               ../applypatch/libapplypatch.la

if ENABLE_SELINUX_POLICY
requiredlibs += -lselinux
endif

bin_PROGRAMS = updater
if USE_GLIB
updater_SOURCES = $(c_sources) $(h_sources)
updater_CFLAGS = $(AM_CFLAGS) -DUSE_GLIB @GLIB_CFLAGS@
updater_CPPFLAGS = $(AM_CPPFLAGS) -DUSE_GLIB @GLIB_CFLAGS@
updater_LDADD = $(requiredlibs) @GLIB_LIBS@
else
updater_SOURCES = $(c_sources) $(h_sources)
updater_CFLAGS = $(AM_CFLAGS)
updater_CPPFLAGS = $(AM_CPPFLAGS)
updater_LDADD = $(requiredlibs)
endif

if TARGET_SUPPORTS_AB
updater_CPPFLAGS += -DTARGET_SUPPORTS_AB
updater_LDADD += -labctl
endif

if TARGET_SUPPORTS_MIRROR_AB_COPY
updater_CPPFLAGS += -DTARGET_SUPPORTS_MIRROR_AB_COPY
endif

if TARGET_SUPPORTS_LVM
updater_CPPFLAGS += -DTARGET_SUPPORTS_LVM
endif

updater_CPPFLAGS += -DTARGET_SUPPORTS_NAND_DM_VERITY
