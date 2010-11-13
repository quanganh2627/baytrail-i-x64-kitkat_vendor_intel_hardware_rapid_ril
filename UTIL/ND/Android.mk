#
# Copyright 2010 Intrinsyc Software International, Inc.  All rights reserved.
#

ifeq ($(CUSTOM_BOARD),mrst_edv)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    notification.cpp \
    rillog.cpp \
    extract.cpp \
    util.cpp \
    repository.cpp

LOCAL_SHARED_LIBRARIES := \
	libutils

LOCAL_CFLAGS = -DRIL_RADIO_RESILIENCE
LOCAL_CFLAGS += -DRIL_ENABLE_CHANNEL_DATA1
LOCAL_CFLAGS += -DDEBUG

LOCAL_C_INCLUDES :=  \
    $(KERNEL_HEADERS) \
    $(LOCAL_PATH)/../../INC \
    $(LOCAL_PATH)/../../CORE \
    $(LOCAL_PATH)/../../CORE/ND \
    $(LOCAL_PATH)/../../OEM/ND

#build shared library
LOCAL_PRELINK_MODULE := false
LOCAL_STRIP_MODULE := true
LOCAL_CFLAGS += -DRIL_SHLIB -Os
LOCAL_MODULE:= librapid-ril-util
include $(BUILD_SHARED_LIBRARY)

endif
