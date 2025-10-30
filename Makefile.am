ACLOCAL_AMFLAGS := -I m4

SUBDIRS := minzip mtdutils edify otafault applypatch updater  bootloader_message update_engine recovery_wrapper

AM_CFLAGS := -Wundef \
             -Wstrict-prototypes \
             -Wno-trigraphs \
             -g -O0 \
             -fno-inline \
             -fno-short-enums \
             -fpic \
             -D__reboot=reboot \
             $(LIBSELINUX_CFLAGS) \
             -D_GNU_SOURCE \
             -D_LARGEFILE64_SOURCE

AM_CPPFLAGS := \
              -DUSE_LE_MODE \
              -I. \
              -I$(WORKSPACE)/OTA/system/core/include \
              -I$(WORKSPACE)/OTA/system/core/base/include \
              -I$(WORKSPACE)/OTA/system/core/fs_mgr/include \
              -I$(WORKSPACE)/OTA/system/core/adb \
              -I$(WORKSPACE)/OTA/system/extras/ext4_utils \
              -I./bootloader_message/include \
              $(LIBSELINUX_CFLAGS) \
              -I$(WORKSPACE)/OTA/system/core/base/include \
              -std=c++11 \
              -DENABLE_LEGACY_BOOTLOADER_MSG_UTILS

#AM_LDFLAGS := -shared -version-info 1:0:0

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

if USE_G_STRLCPY_STRLCAT
libfusesideload_la_CFLAGS += -Dstrlcpy=g_strlcpy -Dstrlcat=g_strlcat
libfusesideload_la_CPPFLAGS += -Dstrlcpy=g_strlcpy -Dstrlcat=g_strlcat
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

if SELINUX_ENABLE
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

if USE_G_STRLCPY_STRLCAT
recovery_CFLAGS += -Dstrlcpy=g_strlcpy -Dstrlcat=g_strlcat
recovery_CPPFLAGS += -Dstrlcpy=g_strlcpy -Dstrlcat=g_strlcat
endif

if SELINUX_ENABLE
recovery_CPPFLAGS += -DSELINUX_IS_ENABLE
endif

verifier_test_libs := -lmincrypt ./minzip/libminzip.la -lm -lpthread -lcutils
verifier_test_CFLAGS := $(AM_CFLAGS)

verifier_test_CPPFLAGS := $(AM_CPPFLAGS)
verifier_test_LDADD := $(verifier_test_libs)

if USE_G_STRLCPY_STRLCAT
verifier_test_CFLAGS += -Dstrlcpy=g_strlcpy -Dstrlcat=g_strlcat
verifier_test_CPPFLAGS += -Dstrlcpy=g_strlcpy -Dstrlcat=g_strlcat
endif

verifier_test_SOURCES := verifier_test.cpp asn1_decoder.cpp verifier.cpp ui.cpp

bin_PROGRAMS := recovery

pkgconfigdir := $(libdir)/pkgconfig
pkgconfig_DATA := recovery.pc
EXTRA_DIST := $(pkgconfig_DATA)

if TARGET_SUPPORTS_AB
recovery_CPPFLAGS += -DTARGET_SUPPORTS_AB
if TARGET_NAD_OTA
recovery_LDADD += -lnad_ab_al
else
recovery_LDADD += -labctl
endif
endif

if TARGET_SUPPORTS_ABC
recovery_CPPFLAGS += -DTARGET_SUPPORTS_ABC
endif

if TARGET_SUPPORTS_OTA_VERIFICATION
recovery_CPPFLAGS += -DTARGET_SUPPORTS_OTA_VERIFICATION
endif

if TARGET_NAND_BOOT
recovery_CPPFLAGS += -DTARGET_NAND_BOOT
endif

if TARGET_SUPPORTS_NAND_DM_VERITY
recovery_CPPFLAGS += -DTARGET_SUPPORTS_NAND_DM_VERITY
endif

if TARGET_NAD_OTA
recovery_CPPFLAGS += -DTARGET_NAD_OTA
endif

if TARGET_SUPPORTS_MIRROR_AB_COPY
recovery_CPPFLAGS += -DTARGET_SUPPORTS_MIRROR_AB_COPY
endif