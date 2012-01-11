#
# Copyright 2010 Intrinsyc Software International, Inc.  All rights reserved.
#

ifeq ($(strip $(BOARD_HAVE_IFX6160)),true)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    rillog.cpp \
    extract.cpp \
    util.cpp \
    repository.cpp

LOCAL_SHARED_LIBRARIES := \
    libutils libcutils

# Activating this macro enables the optional Video Telephony feature
#LOCAL_CFLAGS += -DM2_VT_FEATURE_ENABLED

LOCAL_C_INCLUDES :=  \
    $(LOCAL_PATH)/../../INC \
    $(LOCAL_PATH)/../../CORE \
    $(LOCAL_PATH)/../../CORE/ND \
    $(LOCAL_PATH)/../../OEM/ND

#build shared library
LOCAL_PRELINK_MODULE := false
LOCAL_STRIP_MODULE := true
LOCAL_CFLAGS += -DRIL_SHLIB -Os
LOCAL_MODULE:= librapid-ril-util
LOCAL_MODULE_TAGS:= optional
include $(BUILD_SHARED_LIBRARY)

endif
