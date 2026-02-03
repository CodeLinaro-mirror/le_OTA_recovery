ACLOCAL_AMFLAGS := -I m4

SUBDIRS := minzip mtdutils edify otafault applypatch updater bootloader_message mirror_copy

WORKSPACE := $(TOPDIR)/src

AM_CFLAGS := -Wundef \
             -Wstrict-prototypes \
             -Wno-trigraphs \
             -g -O0 \
             -fno-inline \
             -fno-short-enums \
             -fpic \
             -D__reboot=reboot \
             -D_GNU_SOURCE \
             -D_LARGEFILE64_SOURCE \
             -Dstrlcpy=strlcpy \
             -Dstrlcat=strlcat \
             -DUSE_MUSL

AM_CPPFLAGS := \
              -DUSE_LE_MODE \
              -I. \
              -I$(WORKSPACE)/system/core/include \
              -I$(WORKSPACE)/system/core/base/include \
              -I$(WORKSPACE)/system/core/fs_mgr/include \
              -I$(WORKSPACE)/system/core/adb \
              -I$(WORKSPACE)/system/extras/ext4_utils \
              -I./bootloader_message/include \
              -I$(WORKSPACE)/OTA/system/core/base/include \
              -std=c++11 \
              -DENABLE_LEGACY_BOOTLOADER_MSG_UTILS \
              -Dstrlcpy=strlcpy \
              -Dstrlcat=strlcat \
              -DUSE_MUSL

#AM_LDFLAGS := -shared -version-info 1:0:0

SELINUX_FLAGS = -DCONFIG_PACKAGE_SELINUX_POLICY \
                $(LIBSELINUX_CFLAGS)

if ENABLE_SELINUX_POLICY
AM_CPPFLAGS += $(SELINUX_FLAGS)
AM_CFLAGS   += $(SELINUX_FLAGS)
endif

fuse_sideload_sources := fuse_sideload.cpp

library_includedir := $(pkgincludedir)

lib_LTLIBRARIES := libfusesideload.la
libfusesideload_la_SOURCES := $(fuse_sideload_sources)

if USE_GLIB
libfusesideload_la_CFLAGS := $(AM_CFLAGS) -DUSE_GLIB @GLIB_CFLAGS@
libfusesideload_la_CPPFLAGS := $(AM_CPPFLAGS) -DUSE_GLIB @GLIB_CFLAGS@
libfusesideload_la_LIBADD := -lm -lpthread @GLIB_LIBS@
libfusesideload_la_LDFLAGS := -all-static
else
libfusesideload_la_CFLAGS := $(AM_CFLAGS)
libfusesideload_la_CPPFLAGS := $(AM_CPPFLAGS)
libfusesideload_la_LIBADD := -lm -lpthread
libfusesideload_la_LDFLAGS := -all-static
endif

cpp_sources := adb_install.cpp \
    asn1_decoder.cpp \
    device.cpp \
    default_device.cpp \
    fuse_sdcard_provider.cpp \
    install.cpp \
    recovery.cpp \
    roots.cpp \
    screen_ui.cpp \
    ui.cpp

requiredlibs := -lm \
               -lpthread \
               -lz \
               -lcutils \
               -lmincrypt \
               -lfs_mgr \
               -lext4_utils \
               ./mtdutils/libmtdutils.la \
               ./libfusesideload.la \
               ./edify/libedify.la \
               ./minzip/libminzip.la \
               ./applypatch/libapplypatch.la \
               ./otafault/libotafault.la \
               ./bootloader_message/libbootloader_message.la

if ENABLE_SELINUX_POLICY
requiredlibs += -lselinux
endif

RECOVERY_API_VERSION := 3

if USE_GLIB
recovery_SOURCES := $(cpp_sources)
recovery_CFLAGS := $(AM_CFLAGS) -DRECOVERY_API_VERSION=$(RECOVERY_API_VERSION) -DUSE_GLIB @GLIB_CFLAGS@
recovery_CPPFLAGS := $(AM_CPPFLAGS) -DRECOVERY_API_VERSION=$(RECOVERY_API_VERSION) -DUSE_GLIB @GLIB_CFLAGS@
recovery_LDADD := $(requiredlibs) -lcrypto @GLIB_LIBS@
else
recovery_SOURCES := $(cpp_sources)
recovery_CFLAGS := $(AM_CFLAGS) -DRECOVERY_API_VERSION=$(RECOVERY_API_VERSION)
recovery_CPPFLAGS := $(AM_CPPFLAGS) -DRECOVERY_API_VERSION=$(RECOVERY_API_VERSION)
recovery_LDADD := $(requiredlibs) -lcrypto
endif


verifier_test_libs := -lmincrypt ./minzip/libminzip.la -lm -lpthread -lcutils
verifier_test_CFLAGS := $(AM_CFLAGS)

verifier_test_CPPFLAGS := $(AM_CPPFLAGS)
verifier_test_LDADD := $(verifier_test_libs)

verifier_test_SOURCES := verifier_test.cpp asn1_decoder.cpp verifier.cpp ui.cpp

bin_PROGRAMS := recovery

pkgconfigdir := $(libdir)/pkgconfig
pkgconfig_DATA := recovery.pc
EXTRA_DIST := $(pkgconfig_DATA)

if TARGET_SUPPORTS_AB
recovery_CPPFLAGS += -DTARGET_SUPPORTS_AB
recovery_LDADD += -labctl
endif

if TARGET_SUPPORTS_MIRROR_AB_COPY
recovery_CPPFLAGS += -DTARGET_SUPPORTS_MIRROR_AB_COPY
endif

if TARGET_SUPPORTS_OTA_VERIFICATION
recovery_CPPFLAGS += -DTARGET_SUPPORTS_OTA_VERIFICATION
endif

recovery_CPPFLAGS += -DTARGET_SUPPORTS_NAND_DM_VERITY
