////////////////////////////////////////////////////////////////////////////
// te_oem.h
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

#ifndef RRIL_TE_OEM_H
#define RRIL_TE_OEM_H

#include "rril_OEM.h"

// pData information matched to telephony/ril.h for each request
// ie: OEMGetSimStatus -> RIL_REQUEST_GET_SIM_STATUS
// (from ril.h)

/**
 * RIL_REQUEST_GET_SIM_STATUS
 *
 * Requests status of the SIM interface and the SIM card
 *
 * "data" is NULL
 *
 * "response" is const RIL_CardStatus *
 *
 * Valid errors:
 *  Must never fail
 */

// Therefore, pData will be NULL for the request input and in the parse function
// rRspData.pVoid will point to a RIL_CardStatus and rRspData.cbVoid will be set
// to the size of a RIL_CardStatus pointer. This memory will be freed by RapidRIL
// provided it was allocated as a single contiguous piece of memory with malloc.

class CTEOem
{
public:

    CTEOem();
    ~CTEOem();

    // RIL_REQUEST_GET_SIM_STATUS 1
    long OEMGetSimStatus(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseGetSimStatus(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_ENTER_SIM_PIN 2
    long OEMEnterSimPin(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseEnterSimPin(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_ENTER_SIM_PUK 3
    long OEMEnterSimPuk(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseEnterSimPuk(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_ENTER_SIM_PIN2 4
    long OEMEnterSimPin2(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseEnterSimPin2(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_ENTER_SIM_PUK2 5
    long OEMEnterSimPuk2(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseEnterSimPuk2(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CHANGE_SIM_PIN 6
    long OEMChangeSimPin(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseChangeSimPin(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CHANGE_SIM_PIN2 7
    long OEMChangeSimPin2(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseChangeSimPin2(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION 8
    long OEMEnterNetworkDepersonalization(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseEnterNetworkDepersonalization(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_GET_CURRENT_CALLS 9
    long OEMGetCurrentCalls(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseGetCurrentCalls(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_DIAL 10
    long OEMDial(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseDial(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_GET_IMSI 11
    long OEMGetImsi(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseGetImsi(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_HANGUP 12
    long OEMHangup(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseHangup(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND 13
    long OEMHangupWaitingOrBackground(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseHangupWaitingOrBackground(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND 14
    long OEMHangupForegroundResumeBackground(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseHangupForegroundResumeBackground(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE 15
    // RIL_REQUEST_SWITCH_HOLDING_AND_ACTIVE 15
    long OEMSwitchHoldingAndActive(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSwitchHoldingAndActive(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CONFERENCE 16
    long OEMConference(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseConference(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_UDUB 17
    long OEMUdub(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseUdub(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_LAST_CALL_FAIL_CAUSE 18
    long OEMLastCallFailCause(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseLastCallFailCause(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SIGNAL_STRENGTH 19
    long OEMSignalStrength(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSignalStrength(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_REGISTRATION_STATE 20
    long OEMRegistrationState(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseRegistrationState(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_GPRS_REGISTRATION_STATE 21
    long OEMGPRSRegistrationState(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseGPRSRegistrationState(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_OPERATOR 22
    long OEMOperator(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseOperator(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_RADIO_POWER 23
    long OEMRadioPower(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseRadioPower(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_DTMF 24
    long OEMDtmf(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseDtmf(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SEND_SMS 25
    long OEMSendSms(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSendSms(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SEND_SMS_EXPECT_MORE 26
    long OEMSendSmsExpectMore(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSendSmsExpectMore(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SETUP_DATA_CALL 27
    long OEMSetupDataCall(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize, unsigned int uiCID);
    long OEMParseSetupDataCall(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SIM_IO 28
    long OEMSimIo(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSimIo(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SEND_USSD 29
    long OEMSendUssd(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSendUssd(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CANCEL_USSD 30
    long OEMCancelUssd(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseCancelUssd(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_GET_CLIR 31
    long OEMGetClir(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseGetClir(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SET_CLIR 32
    long OEMSetClir(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSetClir(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_QUERY_CALL_FORWARD_STATUS 33
    long OEMQueryCallForwardStatus(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseQueryCallForwardStatus(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SET_CALL_FORWARD 34
    long OEMSetCallForward(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSetCallForward(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_QUERY_CALL_WAITING 35
    long OEMQueryCallWaiting(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseQueryCallWaiting(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SET_CALL_WAITING 36
    long OEMSetCallWaiting(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSetCallWaiting(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SMS_ACKNOWLEDGE 37
    long OEMSmsAcknowledge(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSmsAcknowledge(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_GET_IMEI 38
    long OEMGetImei(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseGetImei(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_GET_IMEISV 39
    long OEMGetImeisv(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseGetImeisv(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_ANSWER 40
    long OEMAnswer(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseAnswer(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_DEACTIVATE_DATA_CALL 41
    long OEMDeactivateDataCall(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseDeactivateDataCall(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_QUERY_FACILITY_LOCK 42
    long OEMQueryFacilityLock(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseQueryFacilityLock(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SET_FACILITY_LOCK 43
    long OEMSetFacilityLock(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSetFacilityLock(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CHANGE_BARRING_PASSWORD 44
    long OEMChangeBarringPassword(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseChangeBarringPassword(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE 45
    long OEMQueryNetworkSelectionMode(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseQueryNetworkSelectionMode(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC 46
    long OEMSetNetworkSelectionAutomatic(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSetNetworkSelectionAutomatic(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL 47
    long OEMSetNetworkSelectionManual(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSetNetworkSelectionManual(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_QUERY_AVAILABLE_NETWORKS 48
    long OEMQueryAvailableNetworks(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseQueryAvailableNetworks(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_DTMF_START 49
    long OEMDtmfStart(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseDtmfStart(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_DTMF_STOP 50
    long OEMDtmfStop(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseDtmfStop(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_BASEBAND_VERSION 51
    long OEMBasebandVersion(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseBasebandVersion(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SEPARATE_CONNECTION 52
    long OEMSeparateConnection(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSeparateConnection(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SET_MUTE 53
    long OEMSetMute(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSetMute(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_GET_MUTE 54
    long OEMGetMute(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseGetMute(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_QUERY_CLIP 55
    long OEMQueryClip(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseQueryClip(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE 56
    long OEMLastDataCallFailCause(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseLastDataCallFailCause(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_DATA_CALL_LIST 57
    long OEMDataCallList(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseDataCallList(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_RESET_RADIO 58
    long OEMResetRadio(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseResetRadio(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_OEM_HOOK_RAW 59
    long OEMHookRaw(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseHookRaw(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_OEM_HOOK_STRINGS 60
    long OEMHookStrings(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseHookStrings(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SCREEN_STATE 61
    long OEMScreenState(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseScreenState(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION 62
    long OEMSetSuppSvcNotification(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSetSuppSvcNotification(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_WRITE_SMS_TO_SIM 63
    long OEMWriteSmsToSim(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseWriteSmsToSim(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_DELETE_SMS_ON_SIM 64
    long OEMDeleteSmsOnSim(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseDeleteSmsOnSim(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SET_BAND_MODE 65
    long OEMSetBandMode(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSetBandMode(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE 66
    long OEMQueryAvailableBandMode(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseQueryAvailableBandMode(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_STK_GET_PROFILE 67
    long OEMStkGetProfile(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseStkGetProfile(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_STK_SET_PROFILE 68
    long OEMStkSetProfile(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseStkSetProfile(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND 69
    long OEMStkSendEnvelopeCommand(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseStkSendEnvelopeCommand(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE 70
    long OEMStkSendTerminalResponse(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseStkSendTerminalResponse(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM 71
    long OEMStkHandleCallSetupRequestedFromSim(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseStkHandleCallSetupRequestedFromSim(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_EXPLICIT_CALL_TRANSFER 72
    long OEMExplicitCallTransfer(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseExplicitCallTransfer(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
    long OEMSetPreferredNetworkType(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSetPreferredNetworkType(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
    long OEMGetPreferredNetworkType(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseGetPreferredNetworkType(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_GET_NEIGHBORING_CELL_IDS 75
    long OEMGetNeighboringCellIDs(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseGetNeighboringCellIDs(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SET_LOCATION_UPDATES 76
    long OEMSetLocationUpdates(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSetLocationUpdates(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CDMA_SET_SUBSCRIPTION 77
    long OEMCdmaSetSubscription(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseCdmaSetSubscription(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE 78
    long OEMCdmaSetRoamingPreference(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseCdmaSetRoamingPreference(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE 79
    long OEMCdmaQueryRoamingPreference(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseCdmaQueryRoamingPreference(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SET_TTY_MODE 80
    long OEMSetTtyMode(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSetTtyMode(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_QUERY_TTY_MODE 81
    long OEMQueryTtyMode(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseQueryTtyMode(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE 82
    long OEMCdmaSetPreferredVoicePrivacyMode(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseCdmaSetPreferredVoicePrivacyMode(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE 83
    long OEMCdmaQueryPreferredVoicePrivacyMode(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseCdmaQueryPreferredVoicePrivacyMode(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CDMA_FLASH 84
    long OEMCdmaFlash(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseCdmaFlash(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CDMA_BURST_DTMF 85
    long OEMCdmaBurstDtmf(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseCdmaBurstDtmf(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CDMA_VALIDATE_AND_WRITE_AKEY 86
    long OEMCdmaValidateAndWriteAkey(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseCdmaValidateAndWriteAkey(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CDMA_SEND_SMS 87
    long OEMCdmaSendSms(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseCdmaSendSms(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE 88
    long OEMCdmaSmsAcknowledge(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseCdmaSmsAcknowledge(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG 89
    long OEMGsmGetBroadcastSmsConfig(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseGsmGetBroadcastSmsConfig(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG 90
    long OEMGsmSetBroadcastSmsConfig(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseGsmSetBroadcastSmsConfig(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION 91
    long OEMGsmSmsBroadcastActivation(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseGsmSmsBroadcastActivation(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG 92
    long OEMCdmaGetBroadcastSmsConfig(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseCdmaGetBroadcastSmsConfig(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG 93
    long OEMCdmaSetBroadcastSmsConfig(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseCdmaSetBroadcastSmsConfig(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION 94
    long OEMCdmaSmsBroadcastActivation(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseCdmaSmsBroadcastActivation(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CDMA_SUBSCRIPTION 95
    long OEMCdmaSubscription(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseCdmaSubscription(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM 96
    long OEMCdmaWriteSmsToRuim(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseCdmaWriteSmsToRuim(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM 97
    long OEMCdmaDeleteSmsOnRuim(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseCdmaDeleteSmsOnRuim(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_DEVICE_IDENTITY 98
    long OEMDeviceIdentity(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseDeviceIdentity(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE 99
    long OEMExitEmergencyCallbackMode(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseExitEmergencyCallbackMode(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_GET_SMSC_ADDRESS 100
    long OEMGetSmscAddress(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseGetSmscAddress(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SET_SMSC_ADDRESS 101
    long OEMSetSmscAddress(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseSetSmscAddress(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_REPORT_SMS_MEMORY_STATUS 102
    long OEMReportSmsMemoryStatus(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseReportSmsMemoryStatus(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING 103
    long OEMReportStkServiceRunning(REQUEST_DATA & rReqData, void * pData, unsigned int uiDataSize);
    long OEMParseReportStkServiceRunning(RESPONSE_DATA & rRspData);

    // RIL_UNSOL_SIGNAL_STRENGTH  1009
    long OEMParseUnsolicitedSignalStrength(RESPONSE_DATA & rRspData);

    // RIL_UNSOL_DATA_CALL_LIST_CHANGED  1010
    long OEMParseDataCallListChanged(RESPONSE_DATA & rRspData);

    // GET IP ADDRESS (sent internally)
    long OEMParseIpAddress(RESPONSE_DATA & rRspData);

    // GET DNS (sent internally)
    long OEMParseDns(RESPONSE_DATA & rRspData);

    // PARSE CPIN2 Query (sent internally)
    long OEMParseQueryPIN2(RESPONSE_DATA & rRspData);
};

#endif
