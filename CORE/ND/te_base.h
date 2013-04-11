////////////////////////////////////////////////////////////////////////////
// te_base.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the CTEBase class which handles all requests and
//    basic behavior for responses
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_TE_BASE_H
#define RRIL_TE_BASE_H

#include "types.h"
#include "rril_OEM.h"
#include "rril.h"
#include "radio_state.h"
#include "sim_state.h"
#include <utils/Vector.h>

class CChannel_Data;
class CTE;

class CTEBase
{
protected:
    CTE& m_cte;
    char m_cTerminator;
    char m_szNewLine[3];
    char m_szNetworkInterfaceNamePrefix[MAX_BUFFER_SIZE];
    int m_nNetworkRegistrationType;  //  0 = automatic, 1 = manual
    char m_szManualMCCMNC[MAX_BUFFER_SIZE];  //  If manual, this holds the MCCMNC string.
    char m_szPIN[MAX_PIN_SIZE];
    int m_nSimAppType;
    android::Vector<RIL_GSM_BroadcastSmsConfigInfo> m_vBroadcastSmsConfigInfo;
    // This tracks the radio state and handles notifications
    CRadioState m_RadioState;
    // This class tracks the SIM state and handles notifications
    CSimState m_SIMState;

    S_PIN_RETRY_COUNT m_PinRetryCount;
    RIL_PinState m_ePin2State;

    P_ND_PDP_CONTEXT_DATA m_pPDPListData;

public:

    CTEBase(CTE& cte);
    virtual ~CTEBase();

private:

    CTEBase();

public:

    //
    // Member buffer contains ip addresses separated by blank.
    // This function extract the first ip address from the buffer
    // corresponding to the cid.
    //
    virtual char* GetPDNFirstIpV4Dns(UINT32 cid, char* ret);

    //
    // Member buffer contains ip addresses separated by blank.
    // This function extract the second ip address from the buffer
    // corresponding to the cid.
    //
    virtual char* GetPDNSecIpV4Dns(UINT32 cid, char* ret);
    //
    // Member buffer contains ip addresses separated by blank.
    // This function extract the first ip address from the buffer
    // corresponding to the cid.
    //
    virtual char* GetPDNFirstIpV6Dns(UINT32 cid, char* ret);

    //
    // Member buffer contains ip addresses separated by blank.
    // This function extract the second ip address from the buffer
    // corresponding to the cid.
    //
    virtual char* GetPDNSecIpV6Dns(UINT32 cid, char* ret);

    //
    // Member buffer contains ip addresses separated by blank.
    // This function extract the first ip address from the buffer
    // corresponding to the cid.
    //
    virtual char* GetPDNFirstIpV4(UINT32 cid, char* ret);

    //
    // Member buffer contains ip addresses separated by blank.
    // This function extract the second ip address from the buffer
    // corresponding to the cid.
    //
    virtual char* GetPDNSecIpV4(UINT32 cid, char* ret);

    //
    // Member buffer contains one ip address.
    // This function returns the gw ip address from the buffer
    // corresponding to the cid.
    //
    virtual char* GetPDNGwIpV4(UINT32 cid, char* ret);

    virtual char* GetBasicInitCommands(UINT32 uiChannelType);
    virtual char* GetUnlockInitCommands(UINT32 uiChannelType);

    virtual BOOL IsRequestSupported(int requestId);

    // RIL_REQUEST_GET_SIM_STATUS 1
    virtual RIL_RESULT_CODE CoreGetSimStatus(REQUEST_DATA& rReqData,
                                                        void* pData,
                                                        UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseGetSimStatus(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_ENTER_SIM_PIN 2
    virtual RIL_RESULT_CODE CoreEnterSimPin(REQUEST_DATA& rReqData,
                                                       void* pData,
                                                       UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseEnterSimPin(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_ENTER_SIM_PUK 3
    virtual RIL_RESULT_CODE CoreEnterSimPuk(REQUEST_DATA& rReqData,
                                                       void* pData,
                                                       UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseEnterSimPuk(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_ENTER_SIM_PIN2 4
    virtual RIL_RESULT_CODE CoreEnterSimPin2(REQUEST_DATA& rReqData,
                                                        void* pData,
                                                        UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseEnterSimPin2(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_ENTER_SIM_PUK2 5
    virtual RIL_RESULT_CODE CoreEnterSimPuk2(REQUEST_DATA& rReqData,
                                                        void* pData,
                                                        UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseEnterSimPuk2(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CHANGE_SIM_PIN 6
    virtual RIL_RESULT_CODE CoreChangeSimPin(REQUEST_DATA& rReqData,
                                                        void* pData,
                                                        UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseChangeSimPin(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CHANGE_SIM_PIN2 7
    virtual RIL_RESULT_CODE CoreChangeSimPin2(REQUEST_DATA& rReqData,
                                                         void* pData,
                                                         UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseChangeSimPin2(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION 8
    virtual RIL_RESULT_CODE CoreEnterNetworkDepersonalization(REQUEST_DATA& rReqData,
                                                                         void* pData,
                                                                         UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseEnterNetworkDepersonalization(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_GET_CURRENT_CALLS 9
    virtual RIL_RESULT_CODE CoreGetCurrentCalls(REQUEST_DATA& rReqData,
                                                           void* pData,
                                                           UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseGetCurrentCalls(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_DIAL 10
    virtual RIL_RESULT_CODE CoreDial(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseDial(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_GET_IMSI 11
    virtual RIL_RESULT_CODE CoreGetImsi(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGetImsi(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_HANGUP 12
    virtual RIL_RESULT_CODE CoreHangup(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseHangup(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND 13
    virtual RIL_RESULT_CODE CoreHangupWaitingOrBackground(REQUEST_DATA& rReqData,
                                                                     void* pData,
                                                                     UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseHangupWaitingOrBackground(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND 14
    virtual RIL_RESULT_CODE CoreHangupForegroundResumeBackground(REQUEST_DATA& rReqData,
                                                                            void* pData,
                                                                            UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseHangupForegroundResumeBackground(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE 15
    // RIL_REQUEST_SWITCH_HOLDING_AND_ACTIVE 15
    virtual RIL_RESULT_CODE CoreSwitchHoldingAndActive(REQUEST_DATA& rReqData,
                                                                  void* pData,
                                                                  UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseSwitchHoldingAndActive(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CONFERENCE 16
    virtual RIL_RESULT_CODE CoreConference(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseConference(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_UDUB 17
    virtual RIL_RESULT_CODE CoreUdub(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseUdub(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_LAST_CALL_FAIL_CAUSE 18
    virtual RIL_RESULT_CODE CoreLastCallFailCause(REQUEST_DATA& rReqData,
                                                             void* pData,
                                                             UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseLastCallFailCause(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SIGNAL_STRENGTH 19
    virtual RIL_RESULT_CODE CoreSignalStrength(REQUEST_DATA& rReqData,
                                                          void* pData,
                                                          UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseSignalStrength(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_VOICE_REGISTRATION_STATE 20
    virtual RIL_RESULT_CODE CoreRegistrationState(REQUEST_DATA& rReqData,
                                                             void* pData,
                                                             UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseRegistrationState(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_DATA_REGISTRATION_STATE 21
    virtual RIL_RESULT_CODE CoreGPRSRegistrationState(REQUEST_DATA& rReqData,
                                                                 void* pData,
                                                                 UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseGPRSRegistrationState(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_OPERATOR 22
    virtual RIL_RESULT_CODE CoreOperator(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseOperator(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_RADIO_POWER 23
    virtual RIL_RESULT_CODE CoreRadioPower(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseRadioPower(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_DTMF 24
    virtual RIL_RESULT_CODE CoreDtmf(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseDtmf(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SEND_SMS 25
    virtual RIL_RESULT_CODE CoreSendSms(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSendSms(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SEND_SMS_EXPECT_MORE 26
    virtual RIL_RESULT_CODE CoreSendSmsExpectMore(REQUEST_DATA& rReqData,
                                                             void* pData,
                                                             UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseSendSmsExpectMore(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SETUP_DATA_CALL 27
    virtual RIL_RESULT_CODE CoreSetupDataCall(REQUEST_DATA& rReqData,
                                                         void* pData,
                                                         UINT32 uiDataSize,
                                                         UINT32 uiCID);

    virtual RIL_RESULT_CODE ParseSetupDataCall(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SIM_IO 28
    virtual RIL_RESULT_CODE CoreSimIo(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSimIo(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SEND_USSD 29
    virtual RIL_RESULT_CODE CoreSendUssd(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSendUssd(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CANCEL_USSD 30
    virtual RIL_RESULT_CODE CoreCancelUssd(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseCancelUssd(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_GET_CLIR 31
    virtual RIL_RESULT_CODE CoreGetClir(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGetClir(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SET_CLIR 32
    virtual RIL_RESULT_CODE CoreSetClir(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSetClir(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_QUERY_CALL_FORWARD_STATUS 33
    virtual RIL_RESULT_CODE CoreQueryCallForwardStatus(REQUEST_DATA& rReqData,
                                                                  void* pData,
                                                                  UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseQueryCallForwardStatus(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SET_CALL_FORWARD 34
    virtual RIL_RESULT_CODE CoreSetCallForward(REQUEST_DATA& rReqData,
                                                          void* pData,
                                                          UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseSetCallForward(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_QUERY_CALL_WAITING 35
    virtual RIL_RESULT_CODE CoreQueryCallWaiting(REQUEST_DATA& rReqData,
                                                            void* pData,
                                                            UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseQueryCallWaiting(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SET_CALL_WAITING 36
    virtual RIL_RESULT_CODE CoreSetCallWaiting(REQUEST_DATA& rReqData,
                                                          void* pData,
                                                          UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseSetCallWaiting(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SMS_ACKNOWLEDGE 37
    virtual RIL_RESULT_CODE CoreSmsAcknowledge(REQUEST_DATA& rReqData,
                                                          void* pData,
                                                          UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseSmsAcknowledge(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_GET_IMEI 38
    virtual RIL_RESULT_CODE CoreGetImei(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGetImei(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_GET_IMEISV 39
    virtual RIL_RESULT_CODE CoreGetImeisv(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGetImeisv(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_ANSWER 40
    virtual RIL_RESULT_CODE CoreAnswer(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseAnswer(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_DEACTIVATE_DATA_CALL 41
    virtual RIL_RESULT_CODE CoreDeactivateDataCall(REQUEST_DATA& rReqData,
                                                              void* pData,
                                                              UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseDeactivateDataCall(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_QUERY_FACILITY_LOCK 42
    virtual RIL_RESULT_CODE CoreQueryFacilityLock(REQUEST_DATA& rReqData,
                                                             void* pData,
                                                             UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseQueryFacilityLock(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SET_FACILITY_LOCK 43
    virtual RIL_RESULT_CODE CoreSetFacilityLock(REQUEST_DATA& rReqData,
                                                           void* pData,
                                                           UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseSetFacilityLock(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CHANGE_BARRING_PASSWORD 44
    virtual RIL_RESULT_CODE CoreChangeBarringPassword(REQUEST_DATA& rReqData,
                                                                 void* pData,
                                                                 UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseChangeBarringPassword(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE 45
    virtual RIL_RESULT_CODE CoreQueryNetworkSelectionMode(REQUEST_DATA& rReqData,
                                                                     void* pData,
                                                                     UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseQueryNetworkSelectionMode(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC 46
    virtual RIL_RESULT_CODE CoreSetNetworkSelectionAutomatic(REQUEST_DATA& rReqData,
                                                                        void* pData,
                                                                        UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseSetNetworkSelectionAutomatic(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL 47
    virtual RIL_RESULT_CODE CoreSetNetworkSelectionManual(REQUEST_DATA& rReqData,
                                                                     void* pData,
                                                                     UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseSetNetworkSelectionManual(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_QUERY_AVAILABLE_NETWORKS 48
    virtual RIL_RESULT_CODE CoreQueryAvailableNetworks(REQUEST_DATA& rReqData,
                                                                  void* pData,
                                                                  UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseQueryAvailableNetworks(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_DTMF_START 49
    virtual RIL_RESULT_CODE CoreDtmfStart(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseDtmfStart(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_DTMF_STOP 50
    virtual RIL_RESULT_CODE CoreDtmfStop(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseDtmfStop(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_BASEBAND_VERSION 51
    virtual RIL_RESULT_CODE CoreBasebandVersion(REQUEST_DATA& rReqData,
                                                           void* pData,
                                                           UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseBasebandVersion(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SEPARATE_CONNECTION 52
    virtual RIL_RESULT_CODE CoreSeparateConnection(REQUEST_DATA& rReqData,
                                                              void* pData,
                                                              UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseSeparateConnection(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SET_MUTE 53
    virtual RIL_RESULT_CODE CoreSetMute(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSetMute(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_GET_MUTE 54
    virtual RIL_RESULT_CODE CoreGetMute(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGetMute(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_QUERY_CLIP 55
    virtual RIL_RESULT_CODE CoreQueryClip(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseQueryClip(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE 56
    virtual RIL_RESULT_CODE CoreLastDataCallFailCause(REQUEST_DATA& rReqData,
                                                                 void* pData,
                                                                 UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseLastDataCallFailCause(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_DATA_CALL_LIST 57
    virtual RIL_RESULT_CODE ParseEstablishedPDPList(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_RESET_RADIO 58
    virtual RIL_RESULT_CODE CoreResetRadio(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseResetRadio(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_OEM_HOOK_RAW 59
    virtual RIL_RESULT_CODE CoreHookRaw(REQUEST_DATA& rReqData,
                                                   void* pData,
                                                   UINT32 uiDataSize,
                                                   UINT32& uiRilChannel);

    virtual RIL_RESULT_CODE ParseHookRaw(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_OEM_HOOK_STRINGS 60
    virtual RIL_RESULT_CODE CoreHookStrings(REQUEST_DATA& rReqData,
                                                       void* pData,
                                                       UINT32 uiDataSize,
                                                       UINT32& uiRilChannel);

    virtual RIL_RESULT_CODE ParseHookStrings(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SCREEN_STATE 61
    virtual RIL_RESULT_CODE CoreScreenState(REQUEST_DATA& rReqData,
                                                       void* pData,
                                                       UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseScreenState(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION 62
    virtual RIL_RESULT_CODE CoreSetSuppSvcNotification(REQUEST_DATA& rReqData,
                                                                  void* pData,
                                                                  UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseSetSuppSvcNotification(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_WRITE_SMS_TO_SIM 63
    virtual RIL_RESULT_CODE CoreWriteSmsToSim(REQUEST_DATA& rReqData,
                                                         void* pData,
                                                         UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseWriteSmsToSim(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_DELETE_SMS_ON_SIM 64
    virtual RIL_RESULT_CODE CoreDeleteSmsOnSim(REQUEST_DATA& rReqData,
                                                          void* pData,
                                                          UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseDeleteSmsOnSim(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SET_BAND_MODE 65
    virtual RIL_RESULT_CODE CoreSetBandMode(REQUEST_DATA& rReqData,
                                                       void* pData,
                                                       UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseSetBandMode(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE 66
    virtual RIL_RESULT_CODE CoreQueryAvailableBandMode(REQUEST_DATA& rReqData,
                                                                  void* pData,
                                                                  UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseQueryAvailableBandMode(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_STK_GET_PROFILE 67
    virtual RIL_RESULT_CODE CoreStkGetProfile(REQUEST_DATA& rReqData,
                                                         void* pData,
                                                         UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseStkGetProfile(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_STK_SET_PROFILE 68
    virtual RIL_RESULT_CODE CoreStkSetProfile(REQUEST_DATA& rReqData,
                                                         void* pData,
                                                         UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseStkSetProfile(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND 69
    virtual RIL_RESULT_CODE CoreStkSendEnvelopeCommand(REQUEST_DATA& rReqData,
                                                                  void* pData,
                                                                  UINT32 uiDataSize);

    virtual RIL_RESULT_CODE ParseStkSendEnvelopeCommand(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE 70
    virtual RIL_RESULT_CODE CoreStkSendTerminalResponse(REQUEST_DATA& rReqData,
                                                                   void* pData,
                                                                   UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseStkSendTerminalResponse(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM 71
    virtual RIL_RESULT_CODE CoreStkHandleCallSetupRequestedFromSim(REQUEST_DATA& rReqData,
                                                                              void* pData,
                                                                              UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseStkHandleCallSetupRequestedFromSim(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_EXPLICIT_CALL_TRANSFER 72
    virtual RIL_RESULT_CODE CoreExplicitCallTransfer(REQUEST_DATA& rReqData,
                                                                void* pData,
                                                                UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseExplicitCallTransfer(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
    virtual RIL_RESULT_CODE CoreSetPreferredNetworkType(REQUEST_DATA& rReqData,
                                                                   void* pData,
                                                                   UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSetPreferredNetworkType(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
    virtual RIL_RESULT_CODE CoreGetPreferredNetworkType(REQUEST_DATA& rReqData,
                                                                   void* pData,
                                                                   UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGetPreferredNetworkType(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_GET_NEIGHBORING_CELL_IDS 75
    virtual RIL_RESULT_CODE CoreGetNeighboringCellIDs(REQUEST_DATA& rReqData,
                                                                 void* pData,
                                                                 UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGetNeighboringCellIDs(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SET_LOCATION_UPDATES 76
    virtual RIL_RESULT_CODE CoreSetLocationUpdates(REQUEST_DATA& rReqData,
                                                void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSetLocationUpdates(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CDMA_SET_SUBSCRIPTION 77
    virtual RIL_RESULT_CODE CoreCdmaSetSubscription(REQUEST_DATA& rReqData,
                                                               void* pData,
                                                               UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseCdmaSetSubscription(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE 78
    virtual RIL_RESULT_CODE CoreCdmaSetRoamingPreference(REQUEST_DATA& rReqData,
                                                                    void* pData,
                                                                    UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseCdmaSetRoamingPreference(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE 79
    virtual RIL_RESULT_CODE CoreCdmaQueryRoamingPreference(REQUEST_DATA& rReqData,
                                                                      void* pData,
                                                                      UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseCdmaQueryRoamingPreference(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SET_TTY_MODE 80
    virtual RIL_RESULT_CODE CoreSetTtyMode(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSetTtyMode(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_QUERY_TTY_MODE 81
    virtual RIL_RESULT_CODE CoreQueryTtyMode(REQUEST_DATA& rReqData,
                                                        void* pData,
                                                        UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseQueryTtyMode(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE 82
    virtual RIL_RESULT_CODE CoreCdmaSetPreferredVoicePrivacyMode(REQUEST_DATA& rReqData,
                                                                            void* pData,
                                                                            UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseCdmaSetPreferredVoicePrivacyMode(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE 83
    virtual RIL_RESULT_CODE CoreCdmaQueryPreferredVoicePrivacyMode(REQUEST_DATA& rReqData,
                                                                              void* pData,
                                                                              UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseCdmaQueryPreferredVoicePrivacyMode(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CDMA_FLASH 84
    virtual RIL_RESULT_CODE CoreCdmaFlash(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseCdmaFlash(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CDMA_BURST_DTMF 85
    virtual RIL_RESULT_CODE CoreCdmaBurstDtmf(REQUEST_DATA& rReqData,
                                                         void* pData,
                                                         UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseCdmaBurstDtmf(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CDMA_VALIDATE_AND_WRITE_AKEY 86
    virtual RIL_RESULT_CODE CoreCdmaValidateAndWriteAkey(REQUEST_DATA& rReqData,
                                                                    void* pData,
                                                                    UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseCdmaValidateAndWriteAkey(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CDMA_SEND_SMS 87
    virtual RIL_RESULT_CODE CoreCdmaSendSms(REQUEST_DATA& rReqData,
                                                       void* pData,
                                                       UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseCdmaSendSms(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE 88
    virtual RIL_RESULT_CODE CoreCdmaSmsAcknowledge(REQUEST_DATA& rReqData,
                                                              void* pData,
                                                              UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseCdmaSmsAcknowledge(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG 89
    virtual RIL_RESULT_CODE CoreGsmGetBroadcastSmsConfig(REQUEST_DATA& rReqData,
                                                                    void* pData,
                                                                    UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGsmGetBroadcastSmsConfig(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG 90
    virtual RIL_RESULT_CODE CoreGsmSetBroadcastSmsConfig(REQUEST_DATA& rReqData,
                                                                    void* pData,
                                                                    UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGsmSetBroadcastSmsConfig(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION 91
    virtual RIL_RESULT_CODE CoreGsmSmsBroadcastActivation(REQUEST_DATA& rReqData,
                                                                     void* pData,
                                                                     UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGsmSmsBroadcastActivation(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG 92
    virtual RIL_RESULT_CODE CoreCdmaGetBroadcastSmsConfig(REQUEST_DATA& rReqData,
                                                                     void* pData,
                                                                     UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseCdmaGetBroadcastSmsConfig(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG 93
    virtual RIL_RESULT_CODE CoreCdmaSetBroadcastSmsConfig(REQUEST_DATA& rReqData,
                                                                     void* pData,
                                                                     UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseCdmaSetBroadcastSmsConfig(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION 94
    virtual RIL_RESULT_CODE CoreCdmaSmsBroadcastActivation(REQUEST_DATA& rReqData,
                                                                      void* pData,
                                                                      UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseCdmaSmsBroadcastActivation(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CDMA_SUBSCRIPTION 95
    virtual RIL_RESULT_CODE CoreCdmaSubscription(REQUEST_DATA& rReqData,
                                                            void* pData,
                                                            UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseCdmaSubscription(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM 96
    virtual RIL_RESULT_CODE CoreCdmaWriteSmsToRuim(REQUEST_DATA& rReqData,
                                                              void* pData,
                                                              UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseCdmaWriteSmsToRuim(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM 97
    virtual RIL_RESULT_CODE CoreCdmaDeleteSmsOnRuim(REQUEST_DATA& rReqData,
                                                               void* pData,
                                                               UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseCdmaDeleteSmsOnRuim(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_DEVICE_IDENTITY 98
    virtual RIL_RESULT_CODE CoreDeviceIdentity(REQUEST_DATA& rReqData,
                                                          void* pData,
                                                          UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseDeviceIdentity(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE 99
    virtual RIL_RESULT_CODE CoreExitEmergencyCallbackMode(REQUEST_DATA& rReqData,
                                                                     void* pData,
                                                                     UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseExitEmergencyCallbackMode(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_GET_SMSC_ADDRESS 100
    virtual RIL_RESULT_CODE CoreGetSmscAddress(REQUEST_DATA& rReqData,
                                                          void* pData,
                                                          UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGetSmscAddress(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SET_SMSC_ADDRESS 101
    virtual RIL_RESULT_CODE CoreSetSmscAddress(REQUEST_DATA& rReqData,
                                                          void* pData,
                                                          UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSetSmscAddress(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_REPORT_SMS_MEMORY_STATUS 102
    virtual RIL_RESULT_CODE CoreReportSmsMemoryStatus(REQUEST_DATA& rReqData,
                                                                 void* pData,
                                                                 UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseReportSmsMemoryStatus(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING 103
    virtual RIL_RESULT_CODE CoreReportStkServiceRunning(REQUEST_DATA& rReqData,
                                                                   void* pData,
                                                                   UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseReportStkServiceRunning(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU 106
    virtual RIL_RESULT_CODE CoreAckIncomingGsmSmsWithPdu(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseAckIncomingGsmSmsWithPdu(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_STK_SEND_ENVELOPE_WITH_STATUS 107
    virtual RIL_RESULT_CODE ParseStkSendEnvelopeWithStatus(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_VOICE_RADIO_TECH 108
    virtual RIL_RESULT_CODE CoreVoiceRadioTech(REQUEST_DATA& rReqData,
                                                          void* pData,
                                                          UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseVoiceRadioTech(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SIM_TRANSMIT_BASIC 109
    virtual RIL_RESULT_CODE CoreSimTransmitBasic(REQUEST_DATA& rReqData,
                                                            void* pData,
                                                            UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSimTransmitBasic(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SIM_OPEN_CHANNEL 110
    virtual RIL_RESULT_CODE CoreSimOpenChannel(REQUEST_DATA& rReqData,
                                                          void* pData,
                                                          UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSimOpenChannel(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SIM_CLOSE_CHANNEL 111
    virtual RIL_RESULT_CODE CoreSimCloseChannel(REQUEST_DATA& rReqData,
                                                           void* pData,
                                                           UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSimCloseChannel(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_SIM_TRANSMIT_CHANNEL 112
    virtual RIL_RESULT_CODE CoreSimTransmitChannel(REQUEST_DATA& rReqData,
                                                              void* pData,
                                                              UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSimTransmitChannel(RESPONSE_DATA& rRspData);

#if defined(M2_VT_FEATURE_ENABLED)
    // RIL_REQUEST_HANGUP_VT 113
    virtual RIL_RESULT_CODE CoreHangupVT(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseHangupVT(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_DIAL_VT 114
    virtual RIL_RESULT_CODE CoreDialVT(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseDialVT(RESPONSE_DATA& rRspData);
#endif // M2_VT_FEATURE_ENABLED

#if defined(M2_GET_SIM_SMS_STORAGE_ENABLED)
    // RIL_REQUEST_GET_SIM_SMS_STORAGE 115
    virtual RIL_RESULT_CODE CoreGetSimSmsStorage(REQUEST_DATA& rReqData,
                                                            void* pData,
                                                            UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGetSimSmsStorage(RESPONSE_DATA& rRspData);
#endif // M2_GET_SIM_SMS_STORAGE_ENABLED

    // RIL_UNSOL_SIGNAL_STRENGTH  1009
    virtual RIL_RESULT_CODE ParseUnsolicitedSignalStrength(RESPONSE_DATA& rRspData);

    // QUERY SIM SMS STORE STATUS
    virtual RIL_RESULT_CODE ParseQuerySimSmsStoreStatus(RESPONSE_DATA& rRspData);

    virtual RIL_RESULT_CODE ParsePdpContextActivate(RESPONSE_DATA& rRspData);
    virtual RIL_RESULT_CODE ParseQueryIpAndDns(RESPONSE_DATA& rRspData);
    virtual RIL_RESULT_CODE ParseEnterDataState(RESPONSE_DATA& rRspData);

    virtual RIL_RESULT_CODE ParseDeactivateAllDataCalls(RESPONSE_DATA& rRspData);

    virtual void SetIncomingCallStatus(UINT32 uiCallId, UINT32 uiStatus);
    virtual UINT32 GetIncomingCallId();

    virtual BOOL ParseCEER(RESPONSE_DATA& rRspData, UINT32& rUICause);

    // Manage SIM and Radio states
    virtual RIL_RadioState GetRadioState();
    virtual RRIL_SIM_State GetSIMState();
    virtual void SetRadioState(const RRIL_Radio_State eRadioState);
    virtual void SetSIMState(const RRIL_SIM_State eSIMState);

    // Returns true on PIN entry required
    virtual BOOL IsPinEnabled(RIL_CardStatus_v6* pCardStatus);

    // Silent Pin Entry request and response handler
    virtual BOOL HandleSilentPINEntry(void* pRilToken, void* pContextData, int dataSize);
    virtual RIL_RESULT_CODE ParseSilentPinEntry(RESPONSE_DATA& rRspData);

    // PIN retry count request and response handler
    virtual RIL_RESULT_CODE QueryPinRetryCount(REQUEST_DATA& rReqData,
                                                          void* pData,
                                                          UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSimPinRetryCount(RESPONSE_DATA& rRspData);

    // Post command handlers for RIL_REQUEST_SETUP_DATA_CALL request
    virtual void PostSetupDataCallCmdHandler(POST_CMD_HANDLER_DATA& rData);
    virtual void PostPdpContextActivateCmdHandler(POST_CMD_HANDLER_DATA& rData);
    virtual void PostQueryIpAndDnsCmdHandler(POST_CMD_HANDLER_DATA& rData);
    virtual void PostEnterDataStateCmdHandler(POST_CMD_HANDLER_DATA& rData);

    // Post command handler for RIL_REQUEST_DEACTIVATE_DATA_CALL request
    virtual void PostDeactivateDataCallCmdHandler(POST_CMD_HANDLER_DATA& rData);

    // Get functions returning number of retry counts left for respective locks
    virtual int GetPinRetryCount() { return m_PinRetryCount.pin; };
    virtual int GetPin2RetryCount() { return m_PinRetryCount.pin2; };
    virtual int GetPukRetryCount() { return m_PinRetryCount.puk; };
    virtual int GetPuk2RetryCount() { return m_PinRetryCount.puk2; };

    // Setter and getter functions for PIN2 state
    virtual void SetPin2State(RIL_PinState ePin2State) { m_ePin2State = ePin2State; };
    virtual RIL_PinState GetPin2State() { return m_ePin2State; };

    // Functions for configuring data connections
    virtual BOOL DataConfigUp(char* pszNetworkInterfaceName, CChannel_Data* pChannelData,
                                                PDP_TYPE eDataConnectionType);
    virtual BOOL DataConfigDown(UINT32 uiCID) = 0;
    virtual void CleanupAllDataConnections();
    virtual BOOL DataConfigUpIpV4(char* pszNetworkInterfaceName, CChannel_Data* pChannelData);
    virtual BOOL DataConfigUpIpV6(char* pszNetworkInterfaceName, CChannel_Data* pChannelData);
    virtual BOOL DataConfigUpIpV4V6(char* pszNetworkInterfaceName, CChannel_Data* pChannelData);

    virtual RIL_RadioTechnology MapAccessTechnology(UINT32 uiStdAct);

    /*
     * AT commands which will disable detailed registration status reporting,
     * signal strength, fast dormancy etc are added to the command queue.
     */
    virtual RIL_RESULT_CODE HandleScreenStateReq(int screenState);

    virtual int GetCurrentCallId();

    BOOL IsDtmfAllowed(int callId);
    void SetDtmfAllowed(int callId, BOOL bDtmfAllowed);

protected:
    RIL_RESULT_CODE ParseSimPin(const char*& pszRsp, RIL_CardStatus_v6*& pCardStatus);

private:
    RIL_SignalStrength_v6* ParseQuerySignalStrength(RESPONSE_DATA& rRspData);
    void DeactivateAllDataCalls();

    typedef struct
    {
        UINT32 callId;
        UINT32 status;
        BOOL isAnswerReqSent;
    } INCOMING_CALL_INFO;
    INCOMING_CALL_INFO m_IncomingCallInfo;

    S_VOICECALL_STATE_INFO m_VoiceCallInfo[RRIL_MAX_CALL_ID_COUNT];
};

#endif
