LOCAL_PATH := $(call my-dir)
bootmenu_local_path := $(LOCAL_PATH)

include $(CLEAR_VARS)

bootmenu_sources := \
    extendedcommands.c \
    bootmenu.c \
    checkup.c \
    default_bootmenu_ui.c \
    ui.c \

BOOTMENU_VERSION:=2.3

ifeq ($(TARGET_CPU_SMP),true)
    EXTRA_CFLAGS += -DUSE_DUALCORE_DIRTY_HACK
endif
ifneq ($(BOARD_DATA_DEVICE),)
    EXTRA_CFLAGS += -DDATA_DEVICE="\"$(BOARD_DATA_DEVICE)\""
endif
ifneq ($(BOARD_SYSTEM_DEVICE),)
    EXTRA_CFLAGS += -DSYSTEM_DEVICE="\"$(BOARD_SYSTEM_DEVICE)\""
endif
ifneq ($(BOARD_MMC_DEVICE),)
    EXTRA_CFLAGS += -DBOARD_MMC_DEVICE="\"$(BOARD_MMC_DEVICE)\""
endif
ifneq ($(BOARD_SDCARD_DEVICE_SECONDARY),)
    EXTRA_CFLAGS += -DSDCARD_DEVICE="\"$(BOARD_SDCARD_DEVICE_SECONDARY)\""
endif
ifneq ($(BOARD_SDEXT_DEVICE),)
    EXTRA_CFLAGS += -DSDEXT_DEVICE="\"$(BOARD_SDEXT_DEVICE)\""
endif

ifneq ($(TARGET_USE_CUSTOM_LUN_FILE_PATH),)
    EXTRA_CFLAGS += -DBOARD_UMS_LUNFILE="\"$(TARGET_USE_CUSTOM_LUN_FILE_PATH)\""
else
  ifneq ($(BOARD_MASS_STORAGE_FILE_PATH),)
    EXTRA_CFLAGS += -DBOARD_UMS_LUNFILE="\"$(BOARD_MASS_STORAGE_FILE_PATH)\""
  endif
endif

ifneq ($(BOARD_BOOTMODE_CONFIG_FILE),)
    EXTRA_CFLAGS += -DBOOTMODE_CONFIG_FILE="\"$(BOARD_BOOTMODE_CONFIG_FILE)\""
endif

LOCAL_MODULE := bootmenu
LOCAL_MODULE_TAGS := eng

LOCAL_SRC_FILES := $(bootmenu_sources)

BOOTMENU_SUFFIX :=

LOCAL_CFLAGS += \
    -DBOOTMENU_VERSION="\"${BOOTMENU_VERSION}${BOOTMENU_SUFFIX}\"" -DSTOCK_VERSION=0 \
    -DMAX_ROWS=44 -DMAX_COLS=96 ${EXTRA_CFLAGS}

LOCAL_STATIC_LIBRARIES := \
	libminui_bm \
	libreboot \
	libpixelflinger_twrp \
	libpng \
	libz \
	libstdc++ \
	libc \
	libcutils \
	liblog \
	libutils \
	libm

LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_PACK_MODULE_RELOCATIONS := false 
LOCAL_MODULE_PATH := $(PRODUCT_OUT)

include $(BUILD_EXECUTABLE)

include $(call all-makefiles-under,$(bootmenu_local_path))
