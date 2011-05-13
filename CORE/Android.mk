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
    ND/atchannel.c \
    ND/at_tok.c \
    ND/stk.c \
    ND/misc.c \
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
    port.cpp \
    ND/callbacks.cpp \
    ND/file_ops.cpp \
    ND/rildmain.cpp \
    ND/sync_ops.cpp \
    ND/thread_ops.cpp \
    ND/ril_result_nd.cpp \
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
LOCAL_CFLAGS += -DRIL_ENABLE_CHANNEL_DATA1
LOCAL_CFLAGS += -DRIL_ENABLE_SIMTK
LOCAL_CFLAGS += -DUSE_STK_RAW_MODE


LOCAL_C_INCLUDES :=  \
    $(KERNEL_HEADERS) \
    hardware/intel/linux-2.6 \
    $(LOCAL_PATH)/ND  \
    $(LOCAL_PATH)/ND/MODEMS  \
    $(LOCAL_PATH)/MODEMS  \
    $(LOCAL_PATH)/../INC \
    $(LOCAL_PATH)/../OEM/ND \
    $(LOCAL_PATH)/../UTIL/ND


#build shared library
LOCAL_PRELINK_MODULE := false
LOCAL_STRIP_MODULE := true
LOCAL_SHARED_LIBRARIES += librapid-ril-util librapid-ril-oem
LOCAL_LDLIBS += -lpthread
LOCAL_CFLAGS += -DRIL_SHLIB -Os
LOCAL_MODULE:= librapid-ril-core
LOCAL_MODULE_TAGS:= optional
include $(BUILD_SHARED_LIBRARY)

endif
