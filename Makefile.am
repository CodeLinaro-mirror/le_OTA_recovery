ACLOCAL_AMFLAGS := -I m4

SUBDIRS := minadbd minelf mtdutils edify minzip applypatch oem-recovery updater

AM_CPPFLAGS := -Wundef \
	    -Wstrict-prototypes \
	    -Wno-trigraphs \
	    -g -O0 \
	    -fno-inline \
	    -fno-short-enums \
	    -fpic \
	    -D__reboot=reboot \
	    -Dstrlcat=strncat \
	    -Dstrlcpy=strncpy \
	    -DRECOVERY_API_VERSION=3 \
	    -DENABLE_SIGNING \
	    -DUSE_EXT4 \
	    -I. \
	    -I$(WORKSPACE)/system/core/include \
	    -fpermissive \
	    -DNO_NFTW

c_sources := recovery.cpp \
    bootloader.cpp \
    install.cpp \
    roots.cpp \
    ui.cpp \
    screen_ui.cpp \
    verifier.cpp \
    adb_install.cpp \
    bootselect.cpp \
    default_device.cpp \
    updater/install.c

requiredlibs := -lm \
               -lpthread \
               -lcutils \
               -lmincrypt \
               -lfs_mgr \
               -lrt \
               -lext4_utils \
               -lsparse \
               -lz \
               -lselinux \
               ./minelf/libminelf.la \
               ./mtdutils/libmtdutils.la \
               ./edify/libedify.la \
               ./minzip/libminzip.la \
               ./minadbd/libminadbd.la \
               ./applypatch/libapplypatch.la

recovery_SOURCES := $(c_sources)
recovery_CFLAGS := $(AM_CFLAGS)
recovery_CPPFLAGS := $(AM_CPPFLAGS)
recovery_LDADD := $(requiredlibs)

verifier_test_libs := -lmincrypt -lm -lpthread -lcutils
verifier_test_CFLAGS := $(AM_CFLAGS)

verifier_test_CPPFLAGS := $(AM_CPPFLAGS)
verifier_test_LDADD := $(verifier_test_libs)

verifier_test_SOURCES := verifier_test.cpp verifier.cpp

bin_PROGRAMS := verifier_test recovery

pkgconfigdir := $(libdir)/pkgconfig
pkgconfig_DATA := recovery.pc
