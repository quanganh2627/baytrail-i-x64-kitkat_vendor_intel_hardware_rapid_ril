#
# Copyright 2010 Intrinsyc Software International, Inc.  All rights reserved.
#

ifeq ($(strip $(BOARD_HAVE_IFX6160)),true)

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
    ND/silo_misc.cpp \
    ND/channel_nd.cpp \
    channelbase.cpp \
    channel_atcmd.cpp \
    channel_data.cpp \
    channel_DLC2.cpp \
    channel_DLC6.cpp \
    channel_DLC8.cpp \
    channel_URC.cpp \
    channel_OEM.cpp \
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
    ND/MODEMS/data_util.cpp \
    ND/MODEMS/te_inf_6260.cpp \
    ND/MODEMS/silo_voice_inf.cpp \
    ND/MODEMS/silo_sim_inf.cpp \
    ND/MODEMS/silo_sms_inf.cpp \
    ND/MODEMS/silo_data_inf.cpp \
    ND/MODEMS/silo_network_inf.cpp \
    ND/MODEMS/silo_phonebook_inf.cpp \
    ND/MODEMS/silo_misc_inf.cpp


LOCAL_SHARED_LIBRARIES := libcutils libutils


# Activating this macro enables the optional Video Telephony feature
#LOCAL_CFLAGS += -DM2_VT_FEATURE_ENABLED

# Activating this macro enables the Call Failed Cause Notification feature
#LOCAL_CFLAGS += -DM2_CALL_FAILED_CAUSE_FEATURE_ENABLED

# Activating this macro enables PIN retry count feature
#LOCAL_CFLAGS += -DM2_PIN_RETRIES_FEATURE_ENABLED

# Activating this macro enables SEEK for Android (for Ice Cream Sandwich)
LOCAL_CFLAGS += -DM2_SEEK_FEATURE_ENABLED

# Activating this macro enables PIN caching (for modem cold reboot)
#LOCAL_CFLAGS += -DM2_PIN_CACHING_FEATURE_ENABLED

# Activating this macro enables Dual SIM 1S1S commands in RIL_REQUEST_OEM_HOOK_RAW
#LOCAL_CFLAGS += -DM2_DUALSIM_FEATURE_ENABLED

# Activating this macro enables the Get SIM SMS Storage feature
#LOCAL_CFLAGS += -DM2_GET_SIM_SMS_STORAGE_ENABLED

ifeq ($(strip $(BOARD_HAVE_IFX7060)),true)
LOCAL_CFLAGS += -DBOARD_HAVE_IFX7060
endif

LOCAL_C_INCLUDES :=  \
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
