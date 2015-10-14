AM_CFLAGS = -Wundef \
	    -Wstrict-prototypes \
	    -Wno-trigraphs \
	    -g -O0 \
	    -fno-inline \
	    -fno-short-enums \
	    -fpic \
	    -DUSE_EXT4 \
	    -D_XOPEN_SOURCE=500

c_sources = install.c \
            updater.c

library_includedir = $(pkgincludedir)
library_include_HEADERS = $(h_sources)
requiredlibs = -lm \
               -lpthread \
               -lcutils \
               -lselinux \
               -lext4_utils \
               -lsparse \
               -lmincrypt \
               ../minelf/libminelf.la \
               ../mtdutils/libmtdutils.la \
               ../edify/libedify.la \
               ../minzip/libminzip.la \
               ../applypatch/libapplypatch.la \
               ../oem-recovery/librecovery_updater_msm.la \
               -lz

bin_PROGRAMS = updater

updater_SOURCES = $(c_sources) $(h_sources)
updater_CFLAGS = $(AM_CFLAGS)
updater_CPPFLAGS = $(AM_CPPFLAGS)
updater_LDADD = $(requiredlibs)
