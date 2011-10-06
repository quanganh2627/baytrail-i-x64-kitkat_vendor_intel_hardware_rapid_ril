#
# Copyright 2010 Intrinsyc Software International, Inc.  All rights reserved.
#

ifneq (,$(findstring $(CUSTOM_BOARD),mrst_edv mfld_cdk mfld_pr1 mfld_pr2))

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    ND/te_base.cpp \
    ND/te.cpp \
    ND/systemmanager.cpp \
    ND/radio_state.cpp \
    silo.cpp \
    ND/silo_voice.cpp \
    ND/silo_network.cpp \
    ND/silo_sim.cpp \
    ND/silo_data.cpp \
    ND/silo_phonebook.cpp \
    ND/silo_sms.cpp \
    ND/channel_nd.cpp \
    channelbase.cpp \
    channel_atcmd.cpp \
    channel_data.cpp \
    channel_DLC2.cpp \
    channel_DLC6.cpp \
    channel_DLC8.cpp \
    channel_URC.cpp \
    port.cpp \
    ND/callbacks.cpp \
    ND/file_ops.cpp \
    ND/reset.cpp \
    ND/rildmain.cpp \
    ND/sync_ops.cpp \
    ND/thread_ops.cpp \
    cmdcontext.cpp \
    command.cpp \
    globals.cpp \
    rilchannels.cpp \
    response.cpp \
    request_info_table.cpp \
    thread_manager.cpp \
    ND/silo_factory.cpp \
    ND/MODEMS/te_inf_6260.cpp \
    ND/MODEMS/silo_voice_inf.cpp \
    ND/MODEMS/silo_sim_inf.cpp \
    ND/MODEMS/silo_sms_inf.cpp \
    ND/MODEMS/silo_data_inf.cpp \
    ND/MODEMS/silo_network_inf.cpp \
    ND/MODEMS/silo_phonebook_inf.cpp


LOCAL_SHARED_LIBRARIES := libcutils libutils

LOCAL_CFLAGS += -DDEBUG

# Remove comment character to enable M2 "Multiple Primary PDP Contexts" feature
LOCAL_CFLAGS += -DM2_MULTIPLE_PDP_FEATURE_ENABLED

# Remove comment character to enable M2 "Video Telephony" feature
LOCAL_CFLAGS += -DM2_VT_FEATURE_ENABLED

# Remove comment character to enable M2 "Call Failed Cause Notification" feature
LOCAL_CFLAGS += -DM2_CALL_FAILED_CAUSE_FEATURE_ENABLED

# Remove comment character to enable M2 "Rx Diversity" feature
LOCAL_CFLAGS += -DM2_RXDIV_FEATURE_ENABLED

# Remove comment character to enable M2 "Cell Broadcast" feature
LOCAL_CFLAGS += -DM2_CELL_BROADCAST_FEATURE_ENABLED

# Remove comment character to enable M2 "IPv6" feature
LOCAL_CFLAGS += -DM2_IPV6_FEATURE_ENABLED

# Remove comment character to enable M2 "PIN retries" feature
#LOCAL_CFLAGS += -DM2_PIN_RETRIES_FEATURE_ENABLED


# Enable new reset management code
LOCAL_CFLAGS += -DRESET_MGMT


LOCAL_C_INCLUDES :=  \
    $(KERNEL_HEADERS) \
    hardware/intel/linux-2.6 \
    $(LOCAL_PATH)/ND  \
    $(LOCAL_PATH)/ND/MODEMS  \
    $(LOCAL_PATH)/MODEMS  \
    $(LOCAL_PATH)/../INC \
    $(LOCAL_PATH)/../UTIL/ND


#build shared library
LOCAL_PRELINK_MODULE := false
LOCAL_STRIP_MODULE := true
LOCAL_SHARED_LIBRARIES += librapid-ril-util
LOCAL_LDLIBS += -lpthread
LOCAL_CFLAGS += -DRIL_SHLIB -Os
LOCAL_MODULE:= librapid-ril-core
LOCAL_MODULE_TAGS:= optional
include $(BUILD_SHARED_LIBRARY)

endif
