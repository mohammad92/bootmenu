LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifndef RECOVERY_INCLUDE_DIR
    RECOVERY_INCLUDE_DIR := $(commands_recovery_local_path)/minui/include
endif

LOCAL_SRC_FILES := events.c resources.c

ifneq ($(BOARD_CUSTOM_BOOTMENU_GRAPHICS),)
  LOCAL_SRC_FILES += $(BOARD_CUSTOM_BOOTMENU_GRAPHICS)
else
  LOCAL_SRC_FILES += graphics.c
endif

LOCAL_C_INCLUDES +=\
    external/libpng\
    external/zlib \
    system/core/libpixelflinger/include \
    $(RECOVERY_INCLUDE_DIR)

LOCAL_MODULE := libminui_bm
LOCAL_MODULE_TAGS := eng debug

# Defy use this :
ifeq ($(TARGET_RECOVERY_PIXEL_FORMAT),"BGRA_8888")
    LOCAL_CFLAGS += -DPIXELS_BGRA
endif

ifeq ($(TARGET_RECOVERY_PIXEL_FORMAT),"RGBA_8888")
    LOCAL_CFLAGS += -DPIXELS_RGBA
endif

ifeq ($(TARGET_RECOVERY_PIXEL_FORMAT),"RGBX_8888")
    LOCAL_CFLAGS += -DPIXELS_RGBX
endif

ifeq ($(TARGET_RECOVERY_PIXEL_FORMAT),"RGB_565")
    LOCAL_CFLAGS += -DPIXELS_RGB565
endif

# Reversed 16bits RGB (ics software gralloc)
#LOCAL_CFLAGS += -DPIXELS_BGR_16BPP

include $(BUILD_STATIC_LIBRARY)

#include $(CLEAR_VARS)
#LOCAL_MODULE := bm_mkfont
#LOCAL_MODULE_STEM := mkfont
#LOCAL_MODULE_TAGS := optional
#LOCAL_SRC_FILES := mkfont.c
#include $(BUILD_EXECUTABLE)
