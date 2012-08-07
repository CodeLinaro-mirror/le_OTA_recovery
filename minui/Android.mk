LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := graphics.c events.c resources.c

LOCAL_C_INCLUDES +=\
    $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include\
    external/libpng\
    external/zlib

LOCAL_MODULE := libminui

# This used to compare against values in double-quotes (which are just
# ordinary characters in this context).  Strip double-quotes from the
# value so that either will work.

ifeq ($(subst ",,$(TARGET_RECOVERY_PIXEL_FORMAT)),RGBX_8888)
  LOCAL_CFLAGS += -DRECOVERY_RGBX
endif
ifeq ($(subst ",,$(TARGET_RECOVERY_PIXEL_FORMAT)),BGRA_8888)
  LOCAL_CFLAGS += -DRECOVERY_BGRA
endif

ifeq ($(call is-vendor-board-platform,QCOM),true)
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
endif

include $(BUILD_STATIC_LIBRARY)
