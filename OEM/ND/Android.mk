#
# Copyright 2010 Intrinsyc Software International, Inc.  All rights reserved.
#

ifneq (,$(findstring $(CUSTOM_BOARD),mrst_edv mfld_cdk mfld_pr1 mfld_pr2))

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= te_oem.cpp

LOCAL_CFLAGS += -DRIL_ENABLE_CHANNEL_DATA1

LOCAL_C_INCLUDES :=  \
    $(KERNEL_HEADERS) \
    $(LOCAL_PATH)/../../CORE \
    $(LOCAL_PATH)/../../CORE/ND \
    $(LOCAL_PATH)/../../INC \


#build shared library
LOCAL_PRELINK_MODULE := false
LOCAL_STRIP_MODULE := true
LOCAL_SHARED_LIBRARIES = librapid-ril-util
LOCAL_CFLAGS += -DRIL_SHLIB -Os
LOCAL_MODULE:= librapid-ril-oem
LOCAL_MODULE_TAGS:= optional
include $(BUILD_SHARED_LIBRARY)

endif
