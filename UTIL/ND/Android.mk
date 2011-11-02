#
# Copyright 2010 Intrinsyc Software International, Inc.  All rights reserved.
#

ifneq (,$(findstring $(CUSTOM_BOARD),mrst_edv mfld_cdk mfld_pr1 mfld_pr2))

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

LOCAL_CFLAGS += -DDEBUG

# remove comment character to enable M2 "Video Telephony" feature
#LOCAL_CFLAGS += -DM2_VT_FEATURE_ENABLED

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
LOCAL_MODULE_TAGS:= optional
include $(BUILD_SHARED_LIBRARY)

endif
