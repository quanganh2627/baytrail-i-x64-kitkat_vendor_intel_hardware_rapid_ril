////////////////////////////////////////////////////////////////////////////
// te_oem.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the OEM overrides for the CTE class
//
// Author:  Mike Worth
// Created: 2009-09-30
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Sept 30/09  FV      1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#include "te_oem.h"
#include "oemhookids.h"
#include <telephony/ril.h>

CTEOem::CTEOem()
{
}

CTEOem::~CTEOem()
{
}

//
// RIL_REQUEST_GET_SIM_STATUS 1
//
long CTEOem::OEMGetSimStatus(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseGetSimStatus(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_ENTER_SIM_PIN 2
//
long CTEOem::OEMEnterSimPin(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseEnterSimPin(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_ENTER_SIM_PUK 3
//
long CTEOem::OEMEnterSimPuk(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseEnterSimPuk(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_ENTER_SIM_PIN2 4
//
long CTEOem::OEMEnterSimPin2(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseEnterSimPin2(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_ENTER_SIM_PUK2 5
//
long CTEOem::OEMEnterSimPuk2(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseEnterSimPuk2(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CHANGE_SIM_PIN 6
//
long CTEOem::OEMChangeSimPin(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseChangeSimPin(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CHANGE_SIM_PIN2 7
//
long CTEOem::OEMChangeSimPin2(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseChangeSimPin2(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION 8
//
long CTEOem::OEMEnterNetworkDepersonalization(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseEnterNetworkDepersonalization(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_GET_CURRENT_CALLS 9
//
long CTEOem::OEMGetCurrentCalls(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseGetCurrentCalls(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_DIAL 10
//
long CTEOem::OEMDial(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseDial(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_GET_IMSI 11
//
long CTEOem::OEMGetImsi(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseGetImsi(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_HANGUP 12
//
long CTEOem::OEMHangup(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseHangup(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND 13
//
long CTEOem::OEMHangupWaitingOrBackground(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseHangupWaitingOrBackground(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND 14
//
long CTEOem::OEMHangupForegroundResumeBackground(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseHangupForegroundResumeBackground(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE 15
// RIL_REQUEST_SWITCH_HOLDING_AND_ACTIVE 15
//
long CTEOem::OEMSwitchHoldingAndActive(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSwitchHoldingAndActive(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CONFERENCE 16
//
long CTEOem::OEMConference(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseConference(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_UDUB 17
//
long CTEOem::OEMUdub(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseUdub(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_LAST_CALL_FAIL_CAUSE 18
//
long CTEOem::OEMLastCallFailCause(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseLastCallFailCause(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SIGNAL_STRENGTH 19
//
long CTEOem::OEMSignalStrength(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSignalStrength(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_VOICE_REGISTRATION_STATE 20
//
long CTEOem::OEMRegistrationState(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseRegistrationState(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_DATA_REGISTRATION_STATE 21
//
long CTEOem::OEMGPRSRegistrationState(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseGPRSRegistrationState(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_OPERATOR 22
//
long CTEOem::OEMOperator(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseOperator(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_RADIO_POWER 23
//
long CTEOem::OEMRadioPower(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseRadioPower(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_DTMF 24
//
long CTEOem::OEMDtmf(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseDtmf(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SEND_SMS 25
//
long CTEOem::OEMSendSms(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSendSms(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SEND_SMS_EXPECT_MORE 26
//
long CTEOem::OEMSendSmsExpectMore(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSendSmsExpectMore(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SETUP_DATA_CALL 27
//
long CTEOem::OEMSetupDataCall(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize, unsigned int uiCID)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSetupDataCall(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SIM_IO 28
//
long CTEOem::OEMSimIo(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSimIo(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SEND_USSD 29
//
long CTEOem::OEMSendUssd(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSendUssd(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CANCEL_USSD 30
//
long CTEOem::OEMCancelUssd(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseCancelUssd(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_GET_CLIR 31
//
long CTEOem::OEMGetClir(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseGetClir(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SET_CLIR 32
//
long CTEOem::OEMSetClir(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSetClir(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_QUERY_CALL_FORWARD_STATUS 33
//
long CTEOem::OEMQueryCallForwardStatus(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseQueryCallForwardStatus(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SET_CALL_FORWARD 34
//
long CTEOem::OEMSetCallForward(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSetCallForward(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_QUERY_CALL_WAITING 35
//
long CTEOem::OEMQueryCallWaiting(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseQueryCallWaiting(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SET_CALL_WAITING 36
//
long CTEOem::OEMSetCallWaiting(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSetCallWaiting(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SMS_ACKNOWLEDGE  37
//
long CTEOem::OEMSmsAcknowledge(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSmsAcknowledge(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_GET_IMEI 38
//
long CTEOem::OEMGetImei(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseGetImei(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_GET_IMEISV 39
//
long CTEOem::OEMGetImeisv(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseGetImeisv(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_ANSWER 40
//
long CTEOem::OEMAnswer(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseAnswer(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_DEACTIVATE_DATA_CALL 41
//
long CTEOem::OEMDeactivateDataCall(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseDeactivateDataCall(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_QUERY_FACILITY_LOCK 42
//
long CTEOem::OEMQueryFacilityLock(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseQueryFacilityLock(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SET_FACILITY_LOCK 43
//
long CTEOem::OEMSetFacilityLock(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSetFacilityLock(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CHANGE_BARRING_PASSWORD 44
//
long CTEOem::OEMChangeBarringPassword(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseChangeBarringPassword(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE 45
//
long CTEOem::OEMQueryNetworkSelectionMode(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseQueryNetworkSelectionMode(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC 46
//
long CTEOem::OEMSetNetworkSelectionAutomatic(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSetNetworkSelectionAutomatic(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL 47
//
long CTEOem::OEMSetNetworkSelectionManual(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSetNetworkSelectionManual(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_QUERY_AVAILABLE_NETWORKS 48
//
long CTEOem::OEMQueryAvailableNetworks(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseQueryAvailableNetworks(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_DTMF_START 49
//
long CTEOem::OEMDtmfStart(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseDtmfStart(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_DTMF_STOP 50
//
long CTEOem::OEMDtmfStop(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseDtmfStop(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_BASEBAND_VERSION 51
//
long CTEOem::OEMBasebandVersion(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseBasebandVersion(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SEPARATE_CONNECTION 52
//
long CTEOem::OEMSeparateConnection(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSeparateConnection(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SET_MUTE 53
//
long CTEOem::OEMSetMute(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSetMute(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_GET_MUTE 54
//
long CTEOem::OEMGetMute(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseGetMute(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_QUERY_CLIP 55
//
long CTEOem::OEMQueryClip(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseQueryClip(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE 56
//
long CTEOem::OEMLastDataCallFailCause(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseLastDataCallFailCause(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_DATA_CALL_LIST 57
//
long CTEOem::OEMDataCallList(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseDataCallList(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_RESET_RADIO 58
//
long CTEOem::OEMResetRadio(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseResetRadio(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_OEM_HOOK_RAW 59
//
long CTEOem::OEMHookRaw(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseHookRaw(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_OEM_HOOK_STRINGS 60
//
long CTEOem::OEMHookStrings(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseHookStrings(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SCREEN_STATE 61
//
long CTEOem::OEMScreenState(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseScreenState(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION 62
//
long CTEOem::OEMSetSuppSvcNotification(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSetSuppSvcNotification(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_WRITE_SMS_TO_SIM 63
//
long CTEOem::OEMWriteSmsToSim(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseWriteSmsToSim(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_DELETE_SMS_ON_SIM 64
//
long CTEOem::OEMDeleteSmsOnSim(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseDeleteSmsOnSim(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SET_BAND_MODE 65
//
long CTEOem::OEMSetBandMode(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSetBandMode(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE 66
//
long CTEOem::OEMQueryAvailableBandMode(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseQueryAvailableBandMode(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_STK_GET_PROFILE 67
//
long CTEOem::OEMStkGetProfile(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseStkGetProfile(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_STK_SET_PROFILE 68
//
long CTEOem::OEMStkSetProfile(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseStkSetProfile(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND 69
//
long CTEOem::OEMStkSendEnvelopeCommand(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseStkSendEnvelopeCommand(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE 70
//
long CTEOem::OEMStkSendTerminalResponse(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseStkSendTerminalResponse(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM 71
//
long CTEOem::OEMStkHandleCallSetupRequestedFromSim(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseStkHandleCallSetupRequestedFromSim(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_EXPLICIT_CALL_TRANSFER 72
//
long CTEOem::OEMExplicitCallTransfer(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseExplicitCallTransfer(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
//
long CTEOem::OEMSetPreferredNetworkType(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSetPreferredNetworkType(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
//
long CTEOem::OEMGetPreferredNetworkType(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseGetPreferredNetworkType(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_GET_NEIGHBORING_CELL_IDS 75
//
long CTEOem::OEMGetNeighboringCellIDs(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseGetNeighboringCellIDs(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SET_LOCATION_UPDATES 76
//
long CTEOem::OEMSetLocationUpdates(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSetLocationUpdates(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CDMA_SET_SUBSCRIPTION_SOURCE 77
//
long CTEOem::OEMCdmaSetSubscription(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseCdmaSetSubscription(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE 78
//
long CTEOem::OEMCdmaSetRoamingPreference(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseCdmaSetRoamingPreference(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE 79
//
long CTEOem::OEMCdmaQueryRoamingPreference(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseCdmaQueryRoamingPreference(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SET_TTY_MODE 80
//
long CTEOem::OEMSetTtyMode(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSetTtyMode(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_QUERY_TTY_MODE 81
//
long CTEOem::OEMQueryTtyMode(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseQueryTtyMode(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE 82
//
long CTEOem::OEMCdmaSetPreferredVoicePrivacyMode(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseCdmaSetPreferredVoicePrivacyMode(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE 83
//
long CTEOem::OEMCdmaQueryPreferredVoicePrivacyMode(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseCdmaQueryPreferredVoicePrivacyMode(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CDMA_FLASH 84
//
long CTEOem::OEMCdmaFlash(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseCdmaFlash(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CDMA_BURST_DTMF 85
//
long CTEOem::OEMCdmaBurstDtmf(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseCdmaBurstDtmf(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CDMA_VALIDATE_AND_WRITE_AKEY 86
//
long CTEOem::OEMCdmaValidateAndWriteAkey(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseCdmaValidateAndWriteAkey(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CDMA_SEND_SMS 87
//
long CTEOem::OEMCdmaSendSms(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseCdmaSendSms(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE 88
//
long CTEOem::OEMCdmaSmsAcknowledge(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseCdmaSmsAcknowledge(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG 89
//
long CTEOem::OEMGsmGetBroadcastSmsConfig(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseGsmGetBroadcastSmsConfig(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG 90
//
long CTEOem::OEMGsmSetBroadcastSmsConfig(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseGsmSetBroadcastSmsConfig(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION 91
//
long CTEOem::OEMGsmSmsBroadcastActivation(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseGsmSmsBroadcastActivation(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG 92
//
long CTEOem::OEMCdmaGetBroadcastSmsConfig(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseCdmaGetBroadcastSmsConfig(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG 93
//
long CTEOem::OEMCdmaSetBroadcastSmsConfig(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseCdmaSetBroadcastSmsConfig(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION 94
//
long CTEOem::OEMCdmaSmsBroadcastActivation(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseCdmaSmsBroadcastActivation(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CDMA_SUBSCRIPTION 95
//
long CTEOem::OEMCdmaSubscription(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseCdmaSubscription(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM 96
//
long CTEOem::OEMCdmaWriteSmsToRuim(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseCdmaWriteSmsToRuim(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM 97
//
long CTEOem::OEMCdmaDeleteSmsOnRuim(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseCdmaDeleteSmsOnRuim(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_DEVICE_IDENTITY 98
//
long CTEOem::OEMDeviceIdentity(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseDeviceIdentity(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE 99
//
long CTEOem::OEMExitEmergencyCallbackMode(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseExitEmergencyCallbackMode(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_GET_SMSC_ADDRESS 100
//
long CTEOem::OEMGetSmscAddress(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseGetSmscAddress(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SET_SMSC_ADDRESS 101
//
long CTEOem::OEMSetSmscAddress(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSetSmscAddress(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_REPORT_SMS_MEMORY_STATUS 102
//
long CTEOem::OEMReportSmsMemoryStatus(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseReportSmsMemoryStatus(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING 103
//
long CTEOem::OEMReportStkServiceRunning(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseReportStkServiceRunning(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SIM_TRANSMIT_BASIC 104
//
long CTEOem::OEMSimTransmitBasic(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSimTransmitBasic(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SIM_OPEN_CHANNEL 105
//
long CTEOem::OEMSimOpenChannel(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSimOpenChannel(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SIM_CLOSE_CHANNEL 106
//
long CTEOem::OEMSimCloseChannel(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSimCloseChannel(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_REQUEST_SIM_TRANSMIT_CHANNEL 107
//
long CTEOem::OEMSimTransmitChannel(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

long CTEOem::OEMParseSimTransmitChannel(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}


//
// RIL_UNSOL_SIGNAL_STRENGTH  1009
//
long CTEOem::OEMParseUnsolicitedSignalStrength(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// RIL_UNSOL_DATA_CALL_LIST_CHANGED  1010
//
long CTEOem::OEMParseDataCallListChanged(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// GET IP ADDRESS (sent interally)
//
long CTEOem::OEMParseIpAddress(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// GET DNS (sent interally)
//
long CTEOem::OEMParseDns(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

//
// PARSE CPIN2 QUERY (sent interally)
//
long CTEOem::OEMParseQueryPIN2(RESPONSE_DATA & rRspData)
{
    return RIL_E_REQUEST_NOT_SUPPORTED;
}

