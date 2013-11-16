////////////////////////////////////////////////////////////////////////////
// te.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the CTE class which handles all overrides to requests and
//    basic behavior for responses for a specific modem
//
/////////////////////////////////////////////////////////////////////////////

#include <wchar.h>
#include <cutils/properties.h>

#include "util.h"
#include "extract.h"
#include "sync_ops.h"
#include "types.h"
#include "rillog.h"
#include "systemmanager.h"
#include "te.h"
#include "te_base.h"
#include "command.h"
#include "cmdcontext.h"
#include "rril_OEM.h"
#include "repository.h"
#include "oemhookids.h"
#include "channel_data.h"
#include "data_util.h"
#include "te_xmm6260.h"
#include "te_xmm6360.h"
#include "te_xmm7160.h"
#include "te_xmm7260.h"
#include "ril_result.h"
#include "callbacks.h"
#include "reset.h"
#include "extract.h"

CTE* CTE::m_pTEInstance = NULL;

CTE::CTE(UINT32 modemType) :
    m_pTEBaseInstance(NULL),
    m_uiModemType(modemType),
    m_bCSStatusCached(FALSE),
    m_bPSStatusCached(FALSE),
    m_bIsSetupDataCallOngoing(FALSE),
    m_bSpoofCommandsStatus(TRUE),
    m_LastModemEvent(MODEM_STATE_UNKNOWN),
    m_bModemOffInFlightMode(FALSE),
    m_enableLocationUpdates(0),
    m_bRestrictedMode(FALSE),
    m_bRadioRequestPending(FALSE),
    m_bIsSimError(FALSE),
    m_bIsManualNetworkSearchOn(FALSE),
    m_bIsDataSuspended(FALSE),
    m_bIsClearPendingCHLD(FALSE),
    m_FastDormancyMode(FAST_DORMANCY_MODE_DEFAULT),
    m_uiMTU(MTU_SIZE),
    m_bVoiceCapable(TRUE),
    m_bSmsOverCSCapable(TRUE),
    m_bSmsOverPSCapable(TRUE),
    m_bStkCapable(TRUE),
    m_bXDATASTATEnabled(FALSE),
    m_bIMSCapable(FALSE),
    m_bSMSOverIPCapable(FALSE),
    m_bSupportCGPIAF(FALSE),
    m_bNwInitiatedContextActSupport(FALSE),
    m_bSignalStrengthReporting(FALSE),
    m_bCellInfoEnabled(TRUE),
    m_uiModeOfOperation(MODE_CS_PS_VOICE_CENTRIC),
    m_uiTimeoutCmdInit(TIMEOUT_INITIALIZATION_COMMAND),
    m_uiTimeoutAPIDefault(TIMEOUT_API_DEFAULT),
    m_uiTimeoutWaitForInit(TIMEOUT_WAITFORINIT),
    m_uiTimeoutThresholdForRetry(TIMEOUT_THRESHOLDFORRETRY),
    m_uiDtmfState(E_DTMF_STATE_STOP),
    m_ScreenState(SCREEN_STATE_UNKNOWN),
    m_pPrefNetTypeReqInfo(NULL),
    m_RequestedRadioPower(RADIO_POWER_UNKNOWN),
    m_RadioOffReason(E_RADIO_OFF_REASON_NONE),
    m_pRadioStateChangedEvent(NULL),
    m_bCallDropReporting(FALSE),
    m_uiDefaultPDNCid(0),
    m_cTerminator('\r'),
    m_bDataCleanupStatus(FALSE),
    m_pDataCleanupStatusLock(NULL),
    m_nCellInfoListRate(INT_MAX),
    m_bIsCellInfoTimerRunning(FALSE),
    m_uiPinCacheMode(E_PIN_CACHE_MODE_FS)
{
    m_pTEBaseInstance = CreateModemTE(this);

    if (NULL == m_pTEBaseInstance)
    {
        RIL_LOG_CRITICAL("CTE::CTE() - Unable to construct base terminal equipment!!!!!!"
                " EXIT!\r\n");
        exit(0);
    }

    memset(&m_sCSStatus, 0, sizeof(S_ND_REG_STATUS));
    memset(&m_sPSStatus, 0, sizeof(S_ND_GPRS_REG_STATUS));
    memset(&m_sEPSStatus, 0, sizeof(S_ND_GPRS_REG_STATUS));

    CopyStringNullTerminate(m_szNewLine, "\r\n", sizeof(m_szNewLine));

    for (int i = 0; i < LAST_NETWORK_DATA_COUNT; i++)
    {
        m_szLastNetworkData[i][0] = '\0';
    }
    m_szLastCEER[0] = '\0';

    m_pDtmfStateAccess = new CMutex();

    m_pRadioStateChangedEvent = new CEvent(NULL, TRUE);

    m_pDataCleanupStatusLock = new CMutex();
}

CTE::~CTE()
{
    delete m_pRadioStateChangedEvent;
    m_pRadioStateChangedEvent = NULL;

    delete m_pTEBaseInstance;
    m_pTEBaseInstance = NULL;

    if (m_pPrefNetTypeReqInfo)
    {
        free(m_pPrefNetTypeReqInfo);
        m_pPrefNetTypeReqInfo = NULL;
    }

    if (m_pDtmfStateAccess)
    {
        CMutex::Unlock(m_pDtmfStateAccess);
        delete m_pDtmfStateAccess;
        m_pDtmfStateAccess = NULL;
    }

    if (m_pDataCleanupStatusLock)
    {
        CMutex::Unlock(m_pDataCleanupStatusLock);
        delete m_pDataCleanupStatusLock;
        m_pDataCleanupStatusLock = NULL;
    }
}

CTEBase* CTE::CreateModemTE(CTE* pTEInstance)
{
    switch (m_uiModemType)
    {
        case MODEM_TYPE_XMM6260:
            RIL_LOG_INFO("CTE::CreateModemTE() - Using XMM6260\r\n");
            return new CTE_XMM6260(*pTEInstance);

        case MODEM_TYPE_XMM6360:
            RIL_LOG_INFO("CTE::CreateModemTE() - Using XMM6360\r\n");
            return new CTE_XMM6360(*pTEInstance);

        case MODEM_TYPE_XMM7160:
            RIL_LOG_INFO("CTE::CreateModemTE() - Using XMM7160\r\n");
            return new CTE_XMM7160(*pTEInstance);

        case MODEM_TYPE_XMM7260:
            RIL_LOG_INFO("CTE::CreateModemTE() - Using XMM7260\r\n");
            return new CTE_XMM7260(*pTEInstance);

        default: // unsupported modem
            RIL_LOG_INFO("CTE::CreateModemTE() - No modem specified, returning NULL\r\n");
            break;
    }

    return NULL;
}

// Creates the Modem specific TE Singlton Object
void CTE::CreateTE(UINT32 modemType)
{
    CMutex::Lock(CSystemManager::GetInstance().GetTEAccessMutex());
    if (NULL == m_pTEInstance)
    {
        m_pTEInstance = new CTE(modemType);
        if (NULL == m_pTEInstance)
        {
            CMutex::Unlock(CSystemManager::GetInstance().GetTEAccessMutex());
            RIL_LOG_CRITICAL("CTE::CreateTE() - Unable to create terminal equipment!!!!!!"
                    " EXIT!\r\n");
            exit(0);
        }
    }
    CMutex::Unlock(CSystemManager::GetInstance().GetTEAccessMutex());
}

//
// Return an appropriate initializer compatible with the IPC used by the platform
//
CInitializer* CTE::GetInitializer()
{
    return m_pTEBaseInstance->GetInitializer();
}

CTE& CTE::GetTE()
{
    CMutex::Lock(CSystemManager::GetInstance().GetTEAccessMutex());
    if (NULL == m_pTEInstance)
    {
        CMutex::Unlock(CSystemManager::GetInstance().GetTEAccessMutex());
        RIL_LOG_CRITICAL("CTE::GetTE() - Unable to get terminal equipment!!!!!! EXIT!\r\n");
        exit(0);
    }
    CMutex::Unlock(CSystemManager::GetInstance().GetTEAccessMutex());
    return *m_pTEInstance;
}

void CTE::DeleteTEObject()
{
    RIL_LOG_INFO("CTE::DeleteTEObject() - Deleting TE instance\r\n");
    CMutex::Lock(CSystemManager::GetInstance().GetTEAccessMutex());
    delete m_pTEInstance;
    m_pTEInstance = NULL;
    CMutex::Unlock(CSystemManager::GetInstance().GetTEAccessMutex());
}

char* CTE::GetBasicInitCommands(UINT32 uiChannelType)
{
    return m_pTEBaseInstance->GetBasicInitCommands(uiChannelType);
}

char* CTE::GetUnlockInitCommands(UINT32 uiChannelType)
{
    return m_pTEBaseInstance->GetUnlockInitCommands(uiChannelType);
}

BOOL CTE::IsRequestSupported(int requestId)
{
    return m_pTEBaseInstance->IsRequestSupported(requestId);
}

BOOL CTE::IsRequestAllowedInSpoofState(int requestId)
{
    BOOL bAllowed = FALSE;

    switch (requestId)
    {
        case RIL_REQUEST_RADIO_POWER:
        {
            int modemState = GetLastModemEvent();
            if (E_MMGR_EVENT_MODEM_OUT_OF_SERVICE != modemState
                    && E_MMGR_NOTIFY_PLATFORM_REBOOT != modemState)
            {
                bAllowed = TRUE;
            }
            break;
        }

        default:
            bAllowed = FALSE;
    }

    return bAllowed;
}

BOOL CTE::IsRequestAllowedInRadioOff(int requestId)
{
    BOOL bAllowed;

    switch (requestId)
    {
        case RIL_REQUEST_RADIO_POWER:
        case RIL_REQUEST_SCREEN_STATE:
            bAllowed = TRUE;
            break;

        case RIL_REQUEST_GET_IMEI:
        case RIL_REQUEST_GET_IMEISV:
        case RIL_REQUEST_BASEBAND_VERSION:
        case RIL_REQUEST_SET_TTY_MODE:
        case RIL_REQUEST_QUERY_TTY_MODE:
        case RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE:
        case RIL_REQUEST_GET_SIM_STATUS:
        case RIL_REQUEST_OEM_HOOK_STRINGS:
            if (E_MMGR_EVENT_MODEM_UP == GetLastModemEvent())
            {
                bAllowed = TRUE;
            }
            else
            {
                bAllowed = FALSE;
            }
            break;

        case RIL_REQUEST_ENTER_SIM_PIN:
        case RIL_REQUEST_ENTER_SIM_PUK:
        case RIL_REQUEST_ENTER_SIM_PIN2:
        case RIL_REQUEST_ENTER_SIM_PUK2:
        case RIL_REQUEST_CHANGE_SIM_PIN:
        case RIL_REQUEST_CHANGE_SIM_PIN2:
        case RIL_REQUEST_QUERY_FACILITY_LOCK:
        case RIL_REQUEST_SET_FACILITY_LOCK:
            if (RRIL_SIM_STATE_NOT_AVAILABLE != (int) GetSIMState())
                bAllowed = TRUE;
            else
                bAllowed = FALSE;
            break;

        case RIL_REQUEST_GET_IMSI:
        case RIL_REQUEST_SIM_IO:
        case RIL_REQUEST_SIM_TRANSMIT_BASIC:
        case RIL_REQUEST_SIM_OPEN_CHANNEL:
        case RIL_REQUEST_SIM_CLOSE_CHANNEL:
        case RIL_REQUEST_SIM_TRANSMIT_CHANNEL:
        case RIL_REQUEST_WRITE_SMS_TO_SIM:
        case RIL_REQUEST_DELETE_SMS_ON_SIM:
        case RIL_REQUEST_GET_SMSC_ADDRESS:
        case RIL_REQUEST_SET_SMSC_ADDRESS:
        case RIL_REQUEST_ISIM_AUTHENTICATION:
        case RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND:
        case RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE:
        case RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING:
        case RIL_REQUEST_STK_SEND_ENVELOPE_WITH_STATUS:
#if defined(M2_GET_SIM_SMS_STORAGE_ENABLED)
        case RIL_REQUEST_GET_SIM_SMS_STORAGE:
#endif
            if (RRIL_SIM_STATE_READY == (int) GetSIMState())
                bAllowed = TRUE;
            else
                bAllowed = FALSE;
            break;

        default:
            bAllowed = FALSE;
    }

    return bAllowed;
}

BOOL CTE::IsRequestAllowedInSimNotReady(int requestId)
{
    BOOL bAllowed;

    switch (requestId)
    {
        case RIL_REQUEST_GET_IMSI:
        case RIL_REQUEST_SIM_IO:
        case RIL_REQUEST_SIM_TRANSMIT_BASIC:
        case RIL_REQUEST_SIM_OPEN_CHANNEL:
        case RIL_REQUEST_SIM_CLOSE_CHANNEL:
        case RIL_REQUEST_SIM_TRANSMIT_CHANNEL:
        case RIL_REQUEST_WRITE_SMS_TO_SIM:
        case RIL_REQUEST_DELETE_SMS_ON_SIM:
        case RIL_REQUEST_GET_SMSC_ADDRESS:
        case RIL_REQUEST_SET_SMSC_ADDRESS:
        case RIL_REQUEST_ISIM_AUTHENTICATION:
        case RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND:
        case RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE:
        case RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING:
        case RIL_REQUEST_STK_SEND_ENVELOPE_WITH_STATUS:
#if defined(M2_GET_SIM_SMS_STORAGE_ENABLED)
        case RIL_REQUEST_GET_SIM_SMS_STORAGE:
#endif
            bAllowed = FALSE;
            break;

        default:
            bAllowed = TRUE;
    }

    return bAllowed;
}

BOOL CTE::IsRequestAllowedWhenNotRegistered(int requestId)
{
    BOOL bAllowed = FALSE;

    switch (requestId)
    {
        case RIL_REQUEST_OPERATOR:
            break;

        default:
            bAllowed = TRUE;
            break;
    }

    return bAllowed;
}

BOOL CTE::IsModemPowerOffRequest(int requestId, void* pData, size_t uiDataSize)
{
    RIL_LOG_VERBOSE("CTE::IsModemPowerOffRequest - ENTER\r\n");

    char** pszRequest = ((char**)pData);
    UINT32 uiCommand = 0;
    BOOL bRet = FALSE;

    if (RIL_REQUEST_OEM_HOOK_STRINGS != requestId)
    {
        goto Error;
    }

    if (pszRequest == NULL || '\0' == pszRequest[0])
    {
        RIL_LOG_CRITICAL("CTE::IsModemPowerOffRequest() - pszRequest was NULL\r\n");
        goto Error;
    }

    if ((uiDataSize < (1 * sizeof(char *))) || (0 != (uiDataSize % sizeof(char*))))
    {
        RIL_LOG_CRITICAL("CTE::IsModemPowerOffRequest() -"
                " Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    // Get command
    if (sscanf(pszRequest[0], "%u", &uiCommand) == EOF)
    {
        RIL_LOG_CRITICAL("CTE::IsModemPowerOffRequest() - cannot convert %s to int\r\n",
                pszRequest);
        goto Error;
    }

    RIL_LOG_INFO("CTE::IsModemPowerOffRequest(), uiCommand: %u", uiCommand);

    switch (uiCommand)
    {
        case RIL_OEM_HOOK_STRING_POWEROFF_MODEM:
        {
            int modemState = GetLastModemEvent();
            if (E_MMGR_EVENT_MODEM_OUT_OF_SERVICE != modemState
                    && E_MMGR_NOTIFY_PLATFORM_REBOOT != modemState)
            {
                bRet = TRUE;
            }
            break;
        }

        default:
        break;
    }

Error:
    RIL_LOG_VERBOSE("CTE::IsModemPowerOffRequest - Exit\r\n");
    return bRet;
}

RIL_Errno CTE::HandleRequestWhenNoModem(int requestId, RIL_Token hRilToken)
{
    RIL_LOG_INFO("CTE::HandleRequestWhenNoModem - REQID=%d, token=0x%08x\r\n",
            requestId, (int) hRilToken);

    RIL_Errno eRetVal = RIL_E_SUCCESS;

    switch (requestId)
    {
        case RIL_REQUEST_GET_CURRENT_CALLS:
        case RIL_REQUEST_DEACTIVATE_DATA_CALL:
            RIL_onRequestComplete(hRilToken, RIL_E_SUCCESS, NULL, 0);
            break;

        case RIL_REQUEST_SETUP_DATA_CALL:
            RIL_Data_Call_Response_v6 dataCallResp;
            memset(&dataCallResp, 0, sizeof(RIL_Data_Call_Response_v6));
            dataCallResp.status = PDP_FAIL_SIGNAL_LOST;
            dataCallResp.suggestedRetryTime = MAX_INT;
            RIL_onRequestComplete(hRilToken, RIL_E_SUCCESS, &dataCallResp,
                    sizeof(RIL_Data_Call_Response_v6));
            break;

        case RIL_REQUEST_GET_SIM_STATUS:
            CompleteGetSimStatusRequest(hRilToken);
            break;

        default:
            eRetVal = RIL_E_RADIO_NOT_AVAILABLE;
            break;
    }

    return eRetVal;
}

RIL_Errno CTE::HandleRequestInRadioOff(int requestId, RIL_Token hRilToken)
{
    RIL_LOG_INFO("CTE::HandleRequestInRadioOff - REQID=%d, token=0x%08x\r\n",
            requestId, (int) hRilToken);

    RIL_Errno eRetVal = RIL_E_SUCCESS;

    switch (requestId)
    {
        case RIL_REQUEST_GET_CURRENT_CALLS:
        case RIL_REQUEST_DEACTIVATE_DATA_CALL:
            RIL_onRequestComplete(hRilToken, RIL_E_SUCCESS, NULL, 0);
            break;

        case RIL_REQUEST_SETUP_DATA_CALL:
            RIL_Data_Call_Response_v6 dataCallResp;
            memset(&dataCallResp, 0, sizeof(RIL_Data_Call_Response_v6));
            dataCallResp.status = PDP_FAIL_RADIO_POWER_OFF;
            dataCallResp.suggestedRetryTime = MAX_INT;
            RIL_onRequestComplete(hRilToken, RIL_E_SUCCESS, &dataCallResp,
                    sizeof(RIL_Data_Call_Response_v6));
            break;

        /*
         * If any of the following request fails with RIL_E_RADIO_NOT_AVAILABLE,
         * then framework cancels the polling.
         */
        case RIL_REQUEST_VOICE_REGISTRATION_STATE:
        case RIL_REQUEST_DATA_REGISTRATION_STATE:
        case RIL_REQUEST_OPERATOR:
        case RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE:
        case RIL_REQUEST_OEM_HOOK_STRINGS:
        case RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC:
        case RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL:
            eRetVal = RIL_E_RADIO_NOT_AVAILABLE;
            break;

        case RIL_REQUEST_GET_SIM_STATUS:
            CompleteGetSimStatusRequest(hRilToken);
            break;

        default:
            eRetVal = RIL_E_GENERIC_FAILURE;
            break;
    }

    return eRetVal;
}

RIL_Errno CTE::HandleRequestWhenNotRegistered(int requestId, RIL_Token hRilToken)
{
    RIL_LOG_INFO("CTE::HandleRequestWhenNotRegistered - REQID=%d, token=0x%08x\r\n",
            requestId, (int) hRilToken);

    RIL_Errno eRetVal = RIL_E_SUCCESS;

    /*
     * If request is not allowed when modem is not registered on a netwrk, return immediately
     * with specific error code to stop command from being sent to modem (to save time and
     * resources).
     */
    switch (requestId)
    {
        case RIL_REQUEST_OPERATOR:
            eRetVal = RIL_E_OP_NOT_ALLOWED_BEFORE_REG_TO_NW;
            break;

        default:
            eRetVal = RIL_E_GENERIC_FAILURE;
            break;
    }

    return eRetVal;
}

void CTE::HandleRequest(int requestId, void* pData, size_t datalen, RIL_Token hRilToken)
{
    RIL_RESULT_CODE eRetVal = RIL_E_SUCCESS;
    RIL_LOG_INFO("CTE::HandleRequest() - id=%d token: 0x%08x\r\n", requestId, (int) hRilToken);

    //  If we're in the middle of Radio error or radio off request handling, spoof all commands.
    if ((GetSpoofCommandsStatus() ||  RADIO_STATE_UNAVAILABLE == GetRadioState())
            && !IsRequestAllowedInSpoofState(requestId)
            && !IsModemPowerOffRequest(requestId, pData, datalen))
    {
        eRetVal = HandleRequestWhenNoModem(requestId, hRilToken);
    }
    else if ((m_bRadioRequestPending || RADIO_STATE_OFF == GetRadioState())
            && !IsRequestAllowedInRadioOff(requestId)
            && !IsModemPowerOffRequest(requestId, pData, datalen))
    {
        eRetVal = HandleRequestInRadioOff(requestId, hRilToken);
    }
    else if (RRIL_SIM_STATE_NOT_READY == GetSIMState()
            && !IsRequestAllowedInSimNotReady(requestId))
    {
        eRetVal = RIL_E_GENERIC_FAILURE;
    }
    else if (!m_pTEBaseInstance->IsRequestSupported(requestId))
    {
        eRetVal = RIL_E_REQUEST_NOT_SUPPORTED;
    }
    else if (!IsRegistered() && !IsRequestAllowedWhenNotRegistered(requestId))
    {
        eRetVal = HandleRequestWhenNotRegistered(requestId, hRilToken);
    }
    else
    {
        switch (requestId)
        {
            case RIL_REQUEST_GET_SIM_STATUS:  // 1
                eRetVal = RequestGetSimStatus(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_ENTER_SIM_PIN:  // 2
                eRetVal = RequestEnterSimPin(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_ENTER_SIM_PUK:  // 3
                eRetVal = RequestEnterSimPuk(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_ENTER_SIM_PIN2:  // 4
                eRetVal = RequestEnterSimPin2(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_ENTER_SIM_PUK2:  // 5
                eRetVal = RequestEnterSimPuk2(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_CHANGE_SIM_PIN:  // 6
                eRetVal = RequestChangeSimPin(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_CHANGE_SIM_PIN2:  // 7
                eRetVal = RequestChangeSimPin2(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION:  // 8
                eRetVal = RequestEnterNetworkDepersonalization(hRilToken,
                        pData, datalen);
                break;

            case RIL_REQUEST_GET_CURRENT_CALLS:  // 9
                eRetVal = RequestGetCurrentCalls(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_DIAL:  // 10
                eRetVal = RequestDial(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_GET_IMSI:  // 11
                eRetVal = RequestGetImsi(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_HANGUP:  // 12
                eRetVal = RequestHangup(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND:  // 13
                eRetVal = RequestHangupWaitingOrBackground(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND:  // 14
                eRetVal = RequestHangupForegroundResumeBackground(hRilToken,
                        pData, datalen);
                break;

            case RIL_REQUEST_SWITCH_HOLDING_AND_ACTIVE:  // 15
                eRetVal = RequestSwitchHoldingAndActive(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_CONFERENCE:  // 16
                eRetVal = RequestConference(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_UDUB:  // 17
                eRetVal = RequestUdub(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_LAST_CALL_FAIL_CAUSE:  // 18
                eRetVal = RequestLastCallFailCause(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_SIGNAL_STRENGTH:  // 19
                eRetVal = RequestSignalStrength(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_VOICE_REGISTRATION_STATE:  // 20
                eRetVal = RequestRegistrationState(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_DATA_REGISTRATION_STATE:  // 21
                eRetVal = RequestGPRSRegistrationState(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_OPERATOR:  // 22
                eRetVal = RequestOperator(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_RADIO_POWER:  // 23
                eRetVal = RequestRadioPower(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_DTMF:  // 24
                eRetVal = RequestDtmf(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_SEND_SMS:  // 25
                eRetVal = RequestSendSms(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_SEND_SMS_EXPECT_MORE:  // 26
                eRetVal = RequestSendSmsExpectMore(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_SETUP_DATA_CALL:  // 27
                eRetVal = RequestSetupDataCall(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_SIM_IO:  // 28
                eRetVal = RequestSimIo(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_SEND_USSD:  // 29
                eRetVal = (RIL_Errno)CTE::GetTE().RequestSendUssd(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_CANCEL_USSD:  // 30
                eRetVal = (RIL_Errno)CTE::GetTE().RequestCancelUssd(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_GET_CLIR:  // 31
                eRetVal = RequestGetClir(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_SET_CLIR:  // 32
                eRetVal = RequestSetClir(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_QUERY_CALL_FORWARD_STATUS:  // 33
                eRetVal = RequestQueryCallForwardStatus(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_SET_CALL_FORWARD:  // 34
                eRetVal = RequestSetCallForward(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_QUERY_CALL_WAITING:  // 35
                eRetVal = RequestQueryCallWaiting(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_SET_CALL_WAITING:  // 36
                eRetVal = RequestSetCallWaiting(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_SMS_ACKNOWLEDGE:  // 37
                eRetVal = RequestSmsAcknowledge(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_GET_IMEI:  // 38
                eRetVal = RequestGetImei(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_GET_IMEISV:  // 39
                eRetVal = RequestGetImeisv(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_ANSWER:  // 40
                eRetVal = RequestAnswer(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_DEACTIVATE_DATA_CALL:  // 41
                eRetVal = RequestDeactivateDataCall(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_QUERY_FACILITY_LOCK:  // 42
                eRetVal = RequestQueryFacilityLock(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_SET_FACILITY_LOCK:  // 43
                eRetVal = RequestSetFacilityLock(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_CHANGE_BARRING_PASSWORD:  // 44
                eRetVal = RequestChangeBarringPassword(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE:  // 45
                eRetVal = RequestQueryNetworkSelectionMode(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC:  // 46
                eRetVal = RequestSetNetworkSelectionAutomatic(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL:  // 47
                eRetVal = RequestSetNetworkSelectionManual(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_QUERY_AVAILABLE_NETWORKS:  // 48
                eRetVal = RequestQueryAvailableNetworks(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_DTMF_START:  // 49
                eRetVal = RequestDtmfStart(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_DTMF_STOP:  // 50
                eRetVal = RequestDtmfStop(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_BASEBAND_VERSION:  // 51
                eRetVal = RequestBasebandVersion(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_SEPARATE_CONNECTION:  // 52
                eRetVal = RequestSeparateConnection(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_SET_MUTE:  // 53
                eRetVal = RequestSetMute(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_GET_MUTE:  // 54
                eRetVal = RequestGetMute(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_QUERY_CLIP:  // 55
                eRetVal = RequestQueryClip(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE:  // 56
                eRetVal = RequestLastDataCallFailCause(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_DATA_CALL_LIST:  // 57
                eRetVal = RequestDataCallList(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_RESET_RADIO:  // 58
                eRetVal = RequestResetRadio(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_OEM_HOOK_RAW:  // 59
                eRetVal = RequestHookRaw(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_OEM_HOOK_STRINGS:  // 60
                eRetVal = RequestHookStrings(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_SCREEN_STATE:  // 61
                eRetVal = RequestScreenState(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION:  // 62
                eRetVal = RequestSetSuppSvcNotification(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_WRITE_SMS_TO_SIM:  // 63
                eRetVal = RequestWriteSmsToSim(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_DELETE_SMS_ON_SIM:  // 64
                eRetVal = RequestDeleteSmsOnSim(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_SET_BAND_MODE:  // 65
                eRetVal = RequestSetBandMode(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE:  // 66
                eRetVal = RequestQueryAvailableBandMode(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_STK_GET_PROFILE:  // 67
                eRetVal = RequestStkGetProfile(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_STK_SET_PROFILE:  // 68
                eRetVal = RequestStkSetProfile(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND:  // 69
                eRetVal = RequestStkSendEnvelopeCommand(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE:  // 70
                eRetVal = RequestStkSendTerminalResponse(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM:  // 71
                eRetVal = RequestStkHandleCallSetupRequestedFromSim(hRilToken,
                        pData, datalen);
                break;

            case RIL_REQUEST_EXPLICIT_CALL_TRANSFER:  // 72
                eRetVal = RequestExplicitCallTransfer(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE:  // 73
                // Delay request if radio state is Off until it is On
                if (RADIO_STATE_OFF == GetRadioState())
                {
                    eRetVal = DelaySetPrefNetTypeRequest(pData, datalen, hRilToken);
                }
                else
                {
                    eRetVal = RequestSetPreferredNetworkType(hRilToken, pData, datalen);
                }
                break;

            case RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE:  // 74
                eRetVal = RequestGetPreferredNetworkType(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_GET_NEIGHBORING_CELL_IDS:  // 75
            {
                if (IsCellInfoEnabled())
                {
                    eRetVal = RequestGetNeighboringCellIDs(hRilToken, pData, datalen);
                }
                else
                {
                    RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                }
            }
            break;

            case RIL_REQUEST_SET_LOCATION_UPDATES:  // 76
                eRetVal = (RIL_Errno)CTE::GetTE().RequestSetLocationUpdates(
                        hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_CDMA_SET_SUBSCRIPTION_SOURCE:  // 77 - CDMA, not supported
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                break;

            case RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE:  // 78 - CDMA, not supported
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                break;

            case RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE:  // 79 - CDMA, not supported
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                break;

            case RIL_REQUEST_SET_TTY_MODE:  // 80
                eRetVal = RequestSetTtyMode(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_QUERY_TTY_MODE:  // 81
                eRetVal = RequestQueryTtyMode(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE:  // 82 - CDMA, not supported
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                break;

            case RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE:  // 83 - CDMA, not supported
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                break;

            case RIL_REQUEST_CDMA_FLASH:  // 84 - CDMA, not supported
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                break;

            case RIL_REQUEST_CDMA_BURST_DTMF:  // 85 - CDMA, not supported
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                break;

            case RIL_REQUEST_CDMA_VALIDATE_AND_WRITE_AKEY:  // 86 - CDMA, not supported
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                break;

            case RIL_REQUEST_CDMA_SEND_SMS:  // 87 - CDMA, not supported
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                break;

            case RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE:  // 88 - CDMA, not supported
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                break;

            case RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG:  // 89
                eRetVal = RequestGsmGetBroadcastSmsConfig(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG:  // 90
                eRetVal = RequestGsmSetBroadcastSmsConfig(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION:  // 91
                eRetVal = RequestGsmSmsBroadcastActivation(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG:  // 92 - CDMA, not supported
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                break;

            case RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG:  // 93 - CDMA, not supported
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                break;

            case RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION:  // 94 - CDMA, not supported
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                break;

            case RIL_REQUEST_CDMA_SUBSCRIPTION:  // 95 - CDMA, not supported
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                break;

            case RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM:  // 96 - CDMA, not supported
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                break;

            case RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM:  // 97 - CDMA, not supported
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                break;

            case RIL_REQUEST_DEVICE_IDENTITY:  // 98 - CDMA, not supported
                eRetVal = RequestDeviceIdentity(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE:  // 99 - CDMA, not supported
                eRetVal = RequestExitEmergencyCallbackMode(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_GET_SMSC_ADDRESS:  // 100
                eRetVal = RequestGetSmscAddress(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_SET_SMSC_ADDRESS:  // 101
                eRetVal = RequestSetSmscAddress(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_REPORT_SMS_MEMORY_STATUS:  // 102
                eRetVal = RequestReportSmsMemoryStatus(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING:  // 103
                eRetVal = RequestReportStkServiceRunning(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_CDMA_GET_SUBSCRIPTION_SOURCE:  // 104 - CDMA, not supported
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                break;

            case RIL_REQUEST_ISIM_AUTHENTICATION:  // 105 - not supported
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                break;

            case RIL_REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU:  // 106
                eRetVal = RequestAckIncomingGsmSmsWithPdu(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_STK_SEND_ENVELOPE_WITH_STATUS:  // 107
                eRetVal = RequestStkSendEnvelopeWithStatus(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_VOICE_RADIO_TECH:  // 108
                eRetVal = RequestVoiceRadioTech(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_GET_CELL_INFO_LIST:  // 109
                {
                    if (IsCellInfoEnabled())
                    {
                        eRetVal = RequestGetCellInfoList(hRilToken, pData, datalen);
                    }
                    else
                    {
                        RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                    }
                }
                break;

            case RIL_REQUEST_SET_UNSOL_CELL_INFO_LIST_RATE:  // 110
                {
                    if (IsCellInfoEnabled())
                    {
                        eRetVal = RequestSetCellInfoListRate(hRilToken, pData, datalen);
                    }
                    else
                    {
                        RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
                    }
                }
                break;

            //  ************************* END OF REGULAR REQUESTS *******************************

            case RIL_REQUEST_SIM_TRANSMIT_BASIC:  // 111
#if defined(M2_SEEK_FEATURE_ENABLED)
                eRetVal = RequestSimTransmitBasic(hRilToken, pData, datalen);
#else
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
#endif
                break;

            case RIL_REQUEST_SIM_OPEN_CHANNEL:  // 112
#if defined(M2_SEEK_FEATURE_ENABLED)
                eRetVal = RequestSimOpenChannel(hRilToken, pData, datalen);
#else
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
#endif
                break;

            case RIL_REQUEST_SIM_CLOSE_CHANNEL:  // 113
#if defined(M2_SEEK_FEATURE_ENABLED)
                eRetVal = RequestSimCloseChannel(hRilToken, pData, datalen);
#else
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
#endif
                break;

            case RIL_REQUEST_SIM_TRANSMIT_CHANNEL:  // 114
#if defined(M2_SEEK_FEATURE_ENABLED)
                eRetVal = RequestSimTransmitChannel(hRilToken, pData, datalen);
#else
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
#endif
                break;

#if defined(M2_VT_FEATURE_ENABLED)

            case RIL_REQUEST_HANGUP_VT:  // 115
                eRetVal = RequestHangupVT(hRilToken, pData, datalen);
                break;

            case RIL_REQUEST_DIAL_VT:  // 116
                eRetVal = RequestDialVT(hRilToken, pData, datalen);
                break;

#endif // M2_VT_FEATURE_ENABLED

#if defined(M2_GET_SIM_SMS_STORAGE_ENABLED)

            case RIL_REQUEST_GET_SIM_SMS_STORAGE:  // 115 or 117
                eRetVal = RequestGetSimSmsStorage(hRilToken, pData, datalen);
                break;

#endif // M2_GET_SIM_SMS_STORAGE_ENABLED

            default:
                RIL_LOG_INFO("onRequest() - Unknown Request ID id=%d\r\n", requestId);
                RIL_onRequestComplete(hRilToken, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
            break;
        }
    }

    if (RIL_E_SUCCESS != eRetVal)
    {
        RIL_onRequestComplete(hRilToken, (RIL_Errno)eRetVal, NULL, 0);
    }
}

RIL_RESULT_CODE CTE::DelaySetPrefNetTypeRequest(void* pData, size_t datalen, RIL_Token hRilToken)
{
    RIL_RESULT_CODE eRetVal = RIL_E_GENERIC_FAILURE;

    if (NULL != pData
            && sizeof(RIL_PreferredNetworkType*) == datalen
            && NULL == m_pPrefNetTypeReqInfo)
    {
        RIL_LOG_INFO("CTE::DelaySetPrefNetTypeRequest - Waiting for radioPower On "
                "before setting preferred network type...\r\n");

        m_pPrefNetTypeReqInfo = (PREF_NET_TYPE_REQ_INFO*)malloc(
                                            sizeof(PREF_NET_TYPE_REQ_INFO));
        if (m_pPrefNetTypeReqInfo)
        {
            // Save request info
            memset(m_pPrefNetTypeReqInfo, 0, sizeof(PREF_NET_TYPE_REQ_INFO));
            m_pPrefNetTypeReqInfo->token = hRilToken;
            m_pPrefNetTypeReqInfo->type = ((RIL_PreferredNetworkType*)pData)[0];
            m_pPrefNetTypeReqInfo->datalen = datalen;

            eRetVal = RIL_E_SUCCESS;
        }
    }
    return eRetVal;
}

void CTE::SendSetPrefNetTypeRequest()
{
    RIL_RESULT_CODE res = RIL_E_GENERIC_FAILURE;
    RIL_Token rilToken = NULL;

    // Send request to SetPreferredNetworkType if previously received before radio power on
    if (NULL != m_pPrefNetTypeReqInfo)
    {
        RIL_LOG_INFO("CTE::SendSetPrefNetTypeRequest() - RadioPower On, Calling "
                "RequestSetPreferredNetworkType()...\r\n");

        rilToken = m_pPrefNetTypeReqInfo->token;
        res = RequestSetPreferredNetworkType(rilToken,
                                             (void*)&m_pPrefNetTypeReqInfo->type,
                                             m_pPrefNetTypeReqInfo->datalen);
        if (RRIL_RESULT_OK != res)
        {
            RIL_LOG_CRITICAL("CTE::SendSetPrefNetTypeRequest() - RequestSetPreferredNetworkType "
                    "failed!\r\n");
        }

        free(m_pPrefNetTypeReqInfo);
        m_pPrefNetTypeReqInfo = NULL;
    }

    if (RIL_E_SUCCESS != res)
    {
        RIL_onRequestComplete(rilToken, (RIL_Errno)res, NULL, 0);
    }
}

//
// RIL_REQUEST_GET_SIM_STATUS 1
//
RIL_RESULT_CODE CTE::RequestGetSimStatus(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetSimStatus() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetSimStatus(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetSimStatus() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_GET_SIM_STATUS].uiChannel,
                rilToken, RIL_REQUEST_GET_SIM_STATUS, reqData, &CTE::ParseGetSimStatus,
                &CTE::PostGetSimStatusCmdHandler);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetSimStatus() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetSimStatus() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetSimStatus() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetSimStatus(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetSimStatus() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetSimStatus(rRspData);
}

//
// RIL_REQUEST_ENTER_SIM_PIN 2
//
RIL_RESULT_CODE CTE::RequestEnterSimPin(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestEnterSimPin() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreEnterSimPin(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestEnterSimPin() : Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_ENTER_SIM_PIN].uiChannel,
                rilToken, RIL_REQUEST_ENTER_SIM_PIN, reqData, &CTE::ParseEnterSimPin,
                &CTE::PostSimPinCmdHandler);

        if (pCmd)
        {
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestEnterSimPin() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestEnterSimPin() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestEnterSimPin() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseEnterSimPin(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseEnterSimPin() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseEnterSimPin(rRspData);
}

//
// RIL_REQUEST_ENTER_SIM_PUK 3
//
RIL_RESULT_CODE CTE::RequestEnterSimPuk(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestEnterSimPuk() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreEnterSimPuk(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestEnterSimPuk() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_ENTER_SIM_PUK].uiChannel,
                rilToken, RIL_REQUEST_ENTER_SIM_PUK, reqData, &CTE::ParseEnterSimPuk,
                &CTE::PostSimPinCmdHandler);

        if (pCmd)
        {
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestEnterSimPuk() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestEnterSimPuk() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestEnterSimPuk() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseEnterSimPuk(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseEnterSimPuk() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseEnterSimPuk(rRspData);
}

//
// RIL_REQUEST_ENTER_SIM_PIN2 4
//
RIL_RESULT_CODE CTE::RequestEnterSimPin2(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestEnterSimPin2() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreEnterSimPin2(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestEnterSimPin2() : Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_ENTER_SIM_PIN2].uiChannel,
                rilToken, RIL_REQUEST_ENTER_SIM_PIN2, reqData, &CTE::ParseEnterSimPin2,
                &CTE::PostSimPin2CmdHandler);

        if (pCmd)
        {
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestEnterSimPin2() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestEnterSimPin2() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestEnterSimPin2() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseEnterSimPin2(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseEnterSimPin2() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseEnterSimPin2(rRspData);
}

//
// RIL_REQUEST_ENTER_SIM_PUK2 5
//
RIL_RESULT_CODE CTE::RequestEnterSimPuk2(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestEnterSimPuk2() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreEnterSimPuk2(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestEnterSimPuk2() : Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_ENTER_SIM_PUK2].uiChannel,
                rilToken, RIL_REQUEST_ENTER_SIM_PUK2, reqData, &CTE::ParseEnterSimPuk2,
                &CTE::PostSimPin2CmdHandler);

        if (pCmd)
        {
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestEnterSimPuk2() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestEnterSimPuk2() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestEnterSimPuk2() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseEnterSimPuk2(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseEnterSimPuk2() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseEnterSimPuk2(rRspData);
}

//
// RIL_REQUEST_CHANGE_SIM_PIN 6
//
RIL_RESULT_CODE CTE::RequestChangeSimPin(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestChangeSimPin() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreChangeSimPin(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestChangeSimPin() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_CHANGE_SIM_PIN].uiChannel,
                rilToken, RIL_REQUEST_CHANGE_SIM_PIN, reqData, &CTE::ParseChangeSimPin,
                &CTE::PostSimPinCmdHandler);

        if (pCmd)
        {
            //  Call when radio is off.
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestChangeSimPin() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestChangeSimPin() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestChangeSimPin() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseChangeSimPin(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseChangeSimPin() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseChangeSimPin(rRspData);
}

//
// RIL_REQUEST_CHANGE_SIM_PIN2 7
//
RIL_RESULT_CODE CTE::RequestChangeSimPin2(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestChangeSimPin2() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreChangeSimPin2(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestChangeSimPin2() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_CHANGE_SIM_PIN2].uiChannel,
                rilToken, RIL_REQUEST_CHANGE_SIM_PIN2, reqData, &CTE::ParseChangeSimPin2,
                &CTE::PostSimPin2CmdHandler);

        if (pCmd)
        {
            //  Call when radio is off.
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestChangeSimPin2() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestChangeSimPin2() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestChangeSimPin2() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseChangeSimPin2(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseChangeSimPin2() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseChangeSimPin2(rRspData);
}

//
// RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION 8
//
RIL_RESULT_CODE CTE::RequestEnterNetworkDepersonalization(RIL_Token rilToken,
                                                                 void* pData,
                                                                 size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestEnterNetworkDepersonalization() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreEnterNetworkDepersonalization(reqData,
            pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestEnterNetworkDepersonalization() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION].uiChannel,
                rilToken, RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION, reqData,
                &CTE::ParseEnterNetworkDepersonalization, &CTE::PostNtwkPersonalizationCmdHandler);

        if (pCmd)
        {
            //  Call when radio is off.
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestEnterNetworkDepersonalization() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestEnterNetworkDepersonalization() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestEnterNetworkDepersonalization() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseEnterNetworkDepersonalization(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseEnterNetworkDepersonalization() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseEnterNetworkDepersonalization(rRspData);
}

//
// RIL_REQUEST_GET_CURRENT_CALLS 9
//
RIL_RESULT_CODE CTE::RequestGetCurrentCalls(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetCurrentCalls() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetCurrentCalls(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetCurrentCalls() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_GET_CURRENT_CALLS].uiChannel,
                rilToken, RIL_REQUEST_GET_CURRENT_CALLS, reqData, &CTE::ParseGetCurrentCalls,
                &CTE::PostGetCurrentCallsCmdHandler);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetCurrentCalls() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetCurrentCalls() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetCurrentCalls() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetCurrentCalls(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetCurrentCalls() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetCurrentCalls(rRspData);
}

//
// RIL_REQUEST_DIAL 10
//
RIL_RESULT_CODE CTE::RequestDial(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDial() - Enter\r\n");

    REQUEST_DATA reqData;

    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreDial(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDial() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_DIAL].uiChannel,
                rilToken, RIL_REQUEST_DIAL, reqData, &CTE::ParseDial, &CTE::PostDialCmdHandler);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDial() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDial() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestDial() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDial(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDial() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseDial(rRspData);
}

//
// RIL_REQUEST_GET_IMSI 11
//
RIL_RESULT_CODE CTE::RequestGetImsi(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetImsi() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetImsi(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetImsi() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_GET_IMSI].uiChannel,
                 rilToken, RIL_REQUEST_GET_IMSI, reqData, &CTE::ParseGetImsi);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetImsi() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetImsi() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetImsi() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetImsi(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetImsi() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetImsi(rRspData);
}

//
// RIL_REQUEST_HANGUP 12
//
RIL_RESULT_CODE CTE::RequestHangup(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestHangup() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreHangup(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestHangup() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_HANGUP].uiChannel,
                rilToken, RIL_REQUEST_HANGUP, reqData, &CTE::ParseHangup,
                &CTE::PostHangupCmdHandler);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd,TRUE))
            {
                RIL_LOG_CRITICAL("CTE::RequestHangup() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestHangup() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    if (RRIL_RESULT_OK == res)
    {
        m_pTEBaseInstance->SetDtmfAllowed(m_pTEBaseInstance->GetCurrentCallId(), FALSE);
    }

    RIL_LOG_VERBOSE("CTE::RequestHangup() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseHangup(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseHangup() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseHangup(rRspData);
}

//
// RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND 13
//
RIL_RESULT_CODE CTE::RequestHangupWaitingOrBackground(RIL_Token rilToken,
                                                             void* pData,
                                                             size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestHangupWaitingOrBackground() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreHangupWaitingOrBackground(reqData,
            pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestHangupWaitingOrBackground() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND].uiChannel,
                rilToken, RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND, reqData,
                &CTE::ParseHangupWaitingOrBackground);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd,TRUE))
            {
                RIL_LOG_CRITICAL("CTE::RequestHangupWaitingOrBackground() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestHangupWaitingOrBackground() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestHangupWaitingOrBackground() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseHangupWaitingOrBackground(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseHangupWaitingOrBackground() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseHangupWaitingOrBackground(rRspData);
}

//
// RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND 14
//
RIL_RESULT_CODE CTE::RequestHangupForegroundResumeBackground(RIL_Token rilToken,
                                                                    void* pData,
                                                                    size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestHangupForegroundResumeBackground() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreHangupForegroundResumeBackground(reqData,
            pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestHangupForegroundResumeBackground() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND].uiChannel,
                rilToken, RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND, reqData,
                &CTE::ParseHangupForegroundResumeBackground, &CTE::PostHangupCmdHandler);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd,TRUE))
            {
                RIL_LOG_CRITICAL("CTE::RequestHangupForegroundResumeBackground() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestHangupForegroundResumeBackground() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    if (RRIL_RESULT_OK == res)
    {
        m_pTEBaseInstance->SetDtmfAllowed(m_pTEBaseInstance->GetCurrentCallId(), FALSE);
    }

    RIL_LOG_VERBOSE("CTE::RequestHangupForegroundResumeBackground() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseHangupForegroundResumeBackground(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseHangupForegroundResumeBackground() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseHangupForegroundResumeBackground(rRspData);
}

//
// RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE 15
// RIL_REQUEST_SWITCH_HOLDING_AND_ACTIVE 15
//
RIL_RESULT_CODE CTE::RequestSwitchHoldingAndActive(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSwitchHoldingAndActive() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSwitchHoldingAndActive(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSwitchHoldingAndActive() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE].uiChannel,
                rilToken, RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE, reqData,
                &CTE::ParseSwitchHoldingAndActive, &CTE::PostSwitchHoldingAndActiveCmdHandler);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSwitchHoldingAndActive() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSwitchHoldingAndActive() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    if (RRIL_RESULT_OK == res)
    {
        m_pTEBaseInstance->SetDtmfAllowed(m_pTEBaseInstance->GetCurrentCallId(), FALSE);
    }

    RIL_LOG_VERBOSE("CTE::RequestSwitchHoldingAndActive() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSwitchHoldingAndActive(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSwitchHoldingAndActive() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSwitchHoldingAndActive(rRspData);
}

//
// RIL_REQUEST_CONFERENCE 16
//
RIL_RESULT_CODE CTE::RequestConference(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestConference() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreConference(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestConference() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_CONFERENCE].uiChannel,
                rilToken, RIL_REQUEST_CONFERENCE, reqData, &CTE::ParseConference,
                &CTE::PostConferenceCmdHandler);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestConference() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestConference() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    if (RRIL_RESULT_OK == res)
    {
        m_pTEBaseInstance->SetDtmfAllowed(m_pTEBaseInstance->GetCurrentCallId(), FALSE);
    }

    RIL_LOG_VERBOSE("CTE::RequestConference() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseConference(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseConference() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseConference(rRspData);
}

//
// RIL_REQUEST_UDUB 17
//
RIL_RESULT_CODE CTE::RequestUdub(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestUdub() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreUdub(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestUdub() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_UDUB].uiChannel,
                rilToken, RIL_REQUEST_UDUB, reqData, &CTE::ParseUdub);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestUdub() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestUdub() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestUdub() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseUdub(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseUdub() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseUdub(rRspData);
}

//
// RIL_REQUEST_LAST_CALL_FAIL_CAUSE 18
//
RIL_RESULT_CODE CTE::RequestLastCallFailCause(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestLastCallFailCause() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreLastCallFailCause(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestLastCallFailCause() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_LAST_CALL_FAIL_CAUSE].uiChannel,
                rilToken, RIL_REQUEST_LAST_CALL_FAIL_CAUSE, reqData,
                &CTE::ParseLastCallFailCause);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestLastCallFailCause() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestLastCallFailCause() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestLastCallFailCause() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseLastCallFailCause(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseLastCallFailCause() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseLastCallFailCause(rRspData);
}

//
// RIL_REQUEST_SIGNAL_STRENGTH 19
//
RIL_RESULT_CODE CTE::RequestSignalStrength(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSignalStrength() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSignalStrength(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSignalStrength() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_SIGNAL_STRENGTH].uiChannel,
                rilToken, RIL_REQUEST_SIGNAL_STRENGTH, reqData, &CTE::ParseSignalStrength);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSignalStrength() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSignalStrength() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSignalStrength() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSignalStrength(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSignalStrength() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSignalStrength(rRspData);
}

//
// RIL_REQUEST_VOICE_REGISTRATION_STATE 20
//
RIL_RESULT_CODE CTE::RequestRegistrationState(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestRegistrationState() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    if (m_bCSStatusCached && !IsLocationUpdatesEnabled())
    {
        S_ND_REG_STATUS regStatus;

        CopyCachedRegistrationInfo(&regStatus, FALSE);
        /*
         * cheat with the size here.
         * Even though the response size is sizeof(S_ND_REG_STATUS) inform
         * android that the response size is sizeof(S_ND_REG_STATUS_POINTERS).
         * This is because Android is expecting to receive an array of
         * string pointers.
         */
        RIL_onRequestComplete(rilToken, RIL_E_SUCCESS, (void*)&regStatus,
                sizeof(S_ND_REG_STATUS_POINTERS));

        return RRIL_RESULT_OK;
    }

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreRegistrationState(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestRegistrationState() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_VOICE_REGISTRATION_STATE].uiChannel,
                rilToken, RIL_REQUEST_VOICE_REGISTRATION_STATE, reqData,
                &CTE::ParseRegistrationState, &CTE::PostNetworkInfoCmdHandler);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestRegistrationState() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestRegistrationState() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestRegistrationState() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseRegistrationState(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseRegistrationState() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseRegistrationState(rRspData);
}

//
// RIL_REQUEST_DATA_REGISTRATION_STATE 21
//
RIL_RESULT_CODE CTE::RequestGPRSRegistrationState(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGPRSRegistrationState() - Enter\r\n");

    if (m_bPSStatusCached && !IsLocationUpdatesEnabled())
    {
        S_ND_GPRS_REG_STATUS regStatus;

        CopyCachedRegistrationInfo(&regStatus, TRUE);
        /*
         * cheat with the size here.
         * Even though the response size is sizeof(S_ND_GPRS_REG_STATUS) inform
         * android that the response size is sizeof(S_ND_GPRS_REG_STATUS_POINTERS).
         * This is because Android is expecting to receive an array of
         * string pointers.
         */
        RIL_onRequestComplete(rilToken, RIL_E_SUCCESS, (void*)&regStatus,
                sizeof(S_ND_GPRS_REG_STATUS_POINTERS));

        return RRIL_RESULT_OK;
    }

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGPRSRegistrationState(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGPRSRegistrationState() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_DATA_REGISTRATION_STATE].uiChannel,
                rilToken, RIL_REQUEST_DATA_REGISTRATION_STATE, reqData,
                &CTE::ParseGPRSRegistrationState, &CTE::PostNetworkInfoCmdHandler);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGPRSRegistrationState() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGPRSRegistrationState() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGPRSRegistrationState() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGPRSRegistrationState(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGPRSRegistrationState() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGPRSRegistrationState(rRspData);
}

//
// RIL_REQUEST_OPERATOR 22
//
RIL_RESULT_CODE CTE::RequestOperator(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestOperator() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreOperator(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestOperator() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_OPERATOR].uiChannel,
                rilToken, RIL_REQUEST_OPERATOR, reqData, &CTE::ParseOperator, &CTE::PostOperator);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestOperator() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestOperator() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestOperator() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseOperator(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseOperator() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseOperator(rRspData);
}

//
// RIL_REQUEST_RADIO_POWER 23
//
RIL_RESULT_CODE CTE::RequestRadioPower(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestRadioPower() - Enter\r\n");

    bool bTurnRadioOn = false;
    RIL_RESULT_CODE res = RRIL_RESULT_OK;
    REQUEST_DATA reqData; // Not used
    char szResetActionProperty[PROPERTY_VALUE_MAX] = {'\0'};

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE::RequestRadioPower() - Data pointer is NULL.\r\n");
        goto Error;
    }

    if (datalen >= (2 * sizeof(int*)))
    {
        if (0 == ((int*)pData)[0])
        {
            RIL_LOG_INFO("CTE::RequestRadioPower() - Turn Radio OFF\r\n");
            bTurnRadioOn = false;
            m_RadioOffReason = ((int*)pData)[1];
            RIL_LOG_INFO("CTE::RequestRadioPower() - mRadioOffReason:%d\r\n", m_RadioOffReason);
        }
        else
        {
            RIL_LOG_INFO("CTE::RequestRadioPower() - Turn Radio ON\r\n");
            bTurnRadioOn = true;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CTE::RequestRadioPower() - No Radio power reason supplied,"
                "Fall back to default behavior\r\n");

        if (0 == ((int*)pData)[0])
        {
            RIL_LOG_INFO("CTE::RequestRadioPower() - Turn Radio OFF\r\n");
            bTurnRadioOn = false;
            m_RadioOffReason = IsPlatformShutDownRequested() ?
                    E_RADIO_OFF_REASON_SHUTDOWN : m_RadioOffReason;
            RIL_LOG_INFO("CTE::RequestRadioPower() - mRadioOffReason:%d\r\n", m_RadioOffReason);
        }
        else
        {
            RIL_LOG_INFO("CTE::RequestRadioPower() - Turn Radio ON\r\n");
            bTurnRadioOn = true;
        }

    }

    if (property_get("gsm.radioreset", szResetActionProperty, "false")
            && (strncmp("false", szResetActionProperty, 5) != 0)
            && (false == bTurnRadioOn))
    {
        property_set("gsm.radioreset", "false");
        RIL_LOG_INFO("CTE::RequestRadioPower() - Reset requested, do clean-up request\r\n");

        /*
         * In case of data stall, fill the operator in cause[2] so as to keep a single CrashTool
         * signature for all data stalls.
         */
        DO_REQUEST_CLEAN_UP(3, "Data stall", "",
                GetNetworkData(LAST_NETWORK_OP_NAME_NUMERIC));
    }
    else
    {
        m_bRadioRequestPending = TRUE;
        m_RequestedRadioPower = bTurnRadioOn ? RADIO_POWER_ON : RADIO_POWER_OFF;

        res = m_pTEBaseInstance->CoreRadioPower(reqData, pData, datalen);
    }

Error:
    if (RRIL_RESULT_OK == res)
    {
        int mode = RIL_RESTRICTED_STATE_NONE;
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESTRICTED_STATE_CHANGED, &mode, sizeof(int));
    }
    else
    {
        /*
         * Timeout waiting for modem power on and initialization means
         * that there is no change in radio state neither in rapid ril nor
         * in android framework side. Since the requested radio state is
         * not yet reached, device will remain in not registered state.
         * In order to recover from this state, framework should be informed
         * that the requested radio state is not yet reached. This can be
         * done only by notifying the framework of radio state different
         * from the current state. So, set the radio state to unavailable and
         * notify the framework of the radio state change. Framework doesn't
         * take any actions on radio unavailable state. In order to force
         * the framework to take any action , set the radio state to off and
         * notify it after 1seconds. This will force the framework to trigger
         * RADIO_POWER request again with the desired power state.
         *
         * e.g.: Time out Sequence is described as follows:
         *     RADIO_POWER ON request from framework.
         *     Acquire modem resource.
         *     Timeout waiting for MODEM_UP and initialization.
         *     RADIO_POWER ON request completed.
         *     SetRadioState to RADIO_UNAVAILABLE and notify framework.
         *     After 1second, set radio state to RADIO_OFF and notify framework.
         *     Framework will trigger RADIO_POWER ON request again
         */
        SetRadioStateAndNotify(RRIL_RADIO_STATE_UNAVAILABLE);
        RIL_requestTimedCallback(triggerRadioOffInd, NULL, 1, 0);

        if (IsRestrictedMode())
        {
            int mode = RIL_RESTRICTED_STATE_CS_ALL | RIL_RESTRICTED_STATE_PS_ALL;
            RIL_onUnsolicitedResponse(RIL_UNSOL_RESTRICTED_STATE_CHANGED, &mode, sizeof(int));
        }
    }

    m_bRadioRequestPending = FALSE;
    RIL_onRequestComplete(rilToken, RRIL_RESULT_OK, NULL, 0);

    RIL_LOG_VERBOSE("CTE::RequestRadioPower() - Exit\r\n");
    return RRIL_RESULT_OK;
}

RIL_RESULT_CODE CTE::ParseRadioPower(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseRadioPower() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseRadioPower(rRspData);
}

//
// RIL_REQUEST_DTMF 24
//
RIL_RESULT_CODE CTE::RequestDtmf(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDtmf() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreDtmf(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDtmf() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_DTMF].uiChannel,
                 rilToken, RIL_REQUEST_DTMF, reqData, &CTE::ParseDtmf);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDtmf() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDtmf() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestDtmf() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDtmf(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDtmf() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseDtmf(rRspData);
}

//
// RIL_REQUEST_SEND_SMS 25
//
RIL_RESULT_CODE CTE::RequestSendSms(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSendSms() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSendSms(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSendSms() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_SEND_SMS].uiChannel,
                rilToken, RIL_REQUEST_SEND_SMS, reqData, &CTE::ParseSendSms,
                &CTE::PostSendSmsCmdHandler);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSendSms() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSendSms() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSendSms() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSendSms(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSendSms() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSendSms(rRspData);
}

//
// RIL_REQUEST_SEND_SMS_EXPECT_MORE 26
//
RIL_RESULT_CODE CTE::RequestSendSmsExpectMore(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSendSmsExpectMore() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSendSmsExpectMore(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSendSmsExpectMore() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_SEND_SMS_EXPECT_MORE].uiChannel,
                rilToken, RIL_REQUEST_SEND_SMS_EXPECT_MORE, reqData,
                &CTE::ParseSendSmsExpectMore, &CTE::PostSendSmsCmdHandler);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSendSmsExpectMore() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSendSmsExpectMore() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSendSmsExpectMore() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSendSmsExpectMore(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSendSmsExpectMore() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSendSmsExpectMore(rRspData);
}

//
// RIL_REQUEST_SETUP_DATA_CALL 27
//
RIL_RESULT_CODE CTE::RequestSetupDataCall(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetupDataCall() - Enter\r\n");

    REQUEST_DATA reqData;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    UINT32 uiCID = 0;
    CChannel_Data* pChannelData = NULL;
    int retryTime = -1;

    memset(&reqData, 0, sizeof(REQUEST_DATA));

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetupDataCall() - Data pointer is NULL.\r\n");
        goto Error;
    }

    if (datalen < (6 * sizeof(char*)))
    {
        RIL_LOG_CRITICAL("CTE::RequestSetupDataCall() -"
                " Invalid data size. Was given %d bytes\r\n", datalen);
        goto Error;
    }

    if (!IsSetupDataCallAllowed(retryTime))
    {
        RIL_Data_Call_Response_v6 dataCallResp;
        memset(&dataCallResp, 0, sizeof(RIL_Data_Call_Response_v6));
        dataCallResp.status = PDP_FAIL_ERROR_UNSPECIFIED;
        dataCallResp.suggestedRetryTime = retryTime;
        RIL_onRequestComplete(rilToken, RIL_E_SUCCESS, &dataCallResp,
                sizeof(RIL_Data_Call_Response_v6));

        return RRIL_RESULT_OK;
    }

    if (IsEPSRegistered())
    {
        pChannelData = CChannel_Data::GetChnlFromContextID(m_uiDefaultPDNCid);

        if (NULL != pChannelData)
        {
            int dataState = pChannelData->GetDataState();
            RIL_LOG_INFO("CTE::RequestSetupDataCall() - dataState: %d\r\n", dataState);

            switch (dataState)
            {
                case E_DATA_STATE_ACTIVATING:
                case E_DATA_STATE_ACTIVE:
                    if (pChannelData->IsApnEqual(((char**)pData)[2]))
                    {
                        res = m_pTEBaseInstance->HandleSetupDefaultPDN(rilToken, pChannelData);
                        if (RRIL_RESULT_OK == res)
                        {
                            /*
                             * Provided APN matches with the default PDN APN. Interface bring
                             * up is handled in modem specific classes.
                             */
                            return res;
                        }
                    }
                    break;
                case E_DATA_STATE_INITING:
                    /*
                     * TODO: Query default PDN context parameters. Currently,
                     * default PDN context parameters reading is done on CGEV: ME PDN ACT.
                     */
                    break;
                default:
                    break;
            }
        }
    }

    res = m_pTEBaseInstance->CoreSetupDataCall(reqData, pData, datalen, uiCID);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetupDataCall() - Unable to create AT command data\r\n");
        goto Error;
    }
    else
    {
        CCommand* pCmd = NULL;

        pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
        if (NULL == pChannelData)
        {
            RIL_LOG_INFO("CTE::RequestSetupDataCall() -"
                    " No Data Channel for CID %u.\r\n", uiCID);
            goto Error;
        }

        pCmd = new CCommand(pChannelData->GetRilChannel(), rilToken,
                RIL_REQUEST_SETUP_DATA_CALL, reqData, &CTE::ParseSetupDataCall,
                &CTE::PostSetupDataCallCmdHandler);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetupDataCall() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetupDataCall() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }


Error:
    if (RRIL_RESULT_OK != res)
    {
        m_bIsSetupDataCallOngoing = FALSE;
        CleanRequestData(reqData);

        if (pChannelData)
            pChannelData->ResetDataCallInfo();
    }
    else
    {
        m_bIsSetupDataCallOngoing = TRUE;
    }
    RIL_LOG_VERBOSE("CTE::RequestSetupDataCall() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetupDataCall(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetupDataCall() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetupDataCall(rRspData);
}

//
// RIL_REQUEST_SIM_IO 28
//
RIL_RESULT_CODE CTE::RequestSimIo(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSimIo() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));
    CCommand* pCmd = NULL;

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSimIo(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSimIo() - Unable to create AT command data\r\n");
    }
    else
    {
        pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_SIM_IO].uiChannel,
                rilToken, RIL_REQUEST_SIM_IO, reqData, &CTE::ParseSimIo, &CTE::PostSimIOCmdHandler);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSimIo() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSimIo() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    if (RRIL_RESULT_OK != res)
    {
        free(reqData.pContextData);
        delete pCmd;
    }

    RIL_LOG_VERBOSE("CTE::RequestSimIo() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSimIo(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSimIo() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSimIo(rRspData);
}

//
// RIL_REQUEST_SEND_USSD 29
//
RIL_RESULT_CODE CTE::RequestSendUssd(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSendUssd() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSendUssd(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSendUssd() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_SEND_USSD].uiChannel,
                rilToken, RIL_REQUEST_SEND_USSD, reqData, &CTE::ParseSendUssd);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSendUssd() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSendUssd() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSendUssd() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSendUssd(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSendUssd() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSendUssd(rRspData);
}

//
// RIL_REQUEST_CANCEL_USSD 30
//
RIL_RESULT_CODE CTE::RequestCancelUssd(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCancelUssd() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreCancelUssd(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestCancelUssd() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_CANCEL_USSD].uiChannel,
                rilToken, RIL_REQUEST_CANCEL_USSD, reqData, &CTE::ParseCancelUssd);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestCancelUssd() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestCancelUssd() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestCancelUssd() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCancelUssd(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCancelUssd() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCancelUssd(rRspData);
}

//
// RIL_REQUEST_GET_CLIR 31
//
RIL_RESULT_CODE CTE::RequestGetClir(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetClir() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetClir(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetClir() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_GET_CLIR].uiChannel,
                rilToken, RIL_REQUEST_GET_CLIR, reqData, &CTE::ParseGetClir);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetClir() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetClir() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetClir() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetClir(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetClir() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetClir(rRspData);
}

//
// RIL_REQUEST_SET_CLIR 32
//
RIL_RESULT_CODE CTE::RequestSetClir(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetClir() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetClir(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetClir() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_SET_CLIR].uiChannel,
                rilToken, RIL_REQUEST_SET_CLIR, reqData, &CTE::ParseSetClir);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetClir() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetClir() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetClir() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetClir(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetClir() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetClir(rRspData);
}

//
// RIL_REQUEST_QUERY_CALL_FORWARD_STATUS 33
//
RIL_RESULT_CODE CTE::RequestQueryCallForwardStatus(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryCallForwardStatus() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreQueryCallForwardStatus(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryCallForwardStatus() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_QUERY_CALL_FORWARD_STATUS].uiChannel,
                rilToken, RIL_REQUEST_QUERY_CALL_FORWARD_STATUS, reqData,
                &CTE::ParseQueryCallForwardStatus);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryCallForwardStatus() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryCallForwardStatus() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryCallForwardStatus() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryCallForwardStatus(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryCallForwardStatus() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseQueryCallForwardStatus(rRspData);
}

//
// RIL_REQUEST_SET_CALL_FORWARD 34
//
RIL_RESULT_CODE CTE::RequestSetCallForward(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetCallForward() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetCallForward(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetCallForward() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_SET_CALL_FORWARD].uiChannel,
                rilToken, RIL_REQUEST_SET_CALL_FORWARD, reqData, &CTE::ParseSetCallForward);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetCallForward() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetCallForward() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetCallForward() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetCallForward(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetCallForward() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetCallForward(rRspData);
}

//
// RIL_REQUEST_QUERY_CALL_WAITING 35
//
RIL_RESULT_CODE CTE::RequestQueryCallWaiting(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryCallWaiting() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreQueryCallWaiting(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryCallWaiting() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_QUERY_CALL_WAITING].uiChannel,
                rilToken, RIL_REQUEST_QUERY_CALL_WAITING, reqData, &CTE::ParseQueryCallWaiting);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryCallWaiting() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryCallWaiting() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryCallWaiting() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryCallWaiting(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryCallWaiting() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseQueryCallWaiting(rRspData);
}

//
// RIL_REQUEST_SET_CALL_WAITING 36
//
RIL_RESULT_CODE CTE::RequestSetCallWaiting(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetCallWaiting() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetCallWaiting(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetCallWaiting() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_SET_CALL_WAITING].uiChannel,
                rilToken, RIL_REQUEST_SET_CALL_WAITING, reqData, &CTE::ParseSetCallWaiting);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetCallWaiting() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetCallWaiting() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetCallWaiting() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetCallWaiting(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetCallWaiting() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetCallWaiting(rRspData);
}

//
// RIL_REQUEST_SMS_ACKNOWLEDGE 37
//
RIL_RESULT_CODE CTE::RequestSmsAcknowledge(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSmsAcknowledge() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSmsAcknowledge(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSmsAcknowledge() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_SMS_ACKNOWLEDGE].uiChannel,
                rilToken, RIL_REQUEST_SMS_ACKNOWLEDGE, reqData, &CTE::ParseSmsAcknowledge);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSmsAcknowledge() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSmsAcknowledge() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSmsAcknowledge() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSmsAcknowledge(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSmsAcknowledge() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSmsAcknowledge(rRspData);
}

//
// RIL_REQUEST_GET_IMEI 38
//
RIL_RESULT_CODE CTE::RequestGetImei(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetImei() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetImei(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetImei() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_GET_IMEI].uiChannel,
                rilToken, RIL_REQUEST_GET_IMEI, reqData, &CTE::ParseGetImei);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetImei() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetImei() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetImei() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetImei(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetImei() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetImei(rRspData);
}

//
// RIL_REQUEST_GET_IMEISV 39
//
RIL_RESULT_CODE CTE::RequestGetImeisv(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetImeisv() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetImeisv(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetImeisv() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_GET_IMEISV].uiChannel,
                rilToken, RIL_REQUEST_GET_IMEISV, reqData, &CTE::ParseGetImeisv);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetImeisv() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetImeisv() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetImeisv() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetImeisv(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetImeisv() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetImeisv(rRspData);
}

//
// RIL_REQUEST_ANSWER 40
//
RIL_RESULT_CODE CTE::RequestAnswer(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestAnswer() - Enter\r\n");

    REQUEST_DATA reqData;

    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreAnswer(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestAnswer() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_ANSWER].uiChannel,
                rilToken, RIL_REQUEST_ANSWER, reqData, &CTE::ParseAnswer);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestAnswer() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestAnswer() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestAnswer() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseAnswer(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseAnswer() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseAnswer(rRspData);
}

//
// RIL_REQUEST_DEACTIVATE_DATA_CALL 41
//
RIL_RESULT_CODE CTE::RequestDeactivateDataCall(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDeactivateDataCall() - Enter\r\n");

    REQUEST_DATA reqData;
    RIL_RESULT_CODE res;

    if (IsManualNetworkSearchOn())
    {
        RIL_onRequestComplete(rilToken, RIL_E_SUCCESS, NULL, 0);
        return RRIL_RESULT_OK;
    }

    memset(&reqData, 0, sizeof(REQUEST_DATA));

    res = m_pTEBaseInstance->CoreDeactivateDataCall(reqData, pData, datalen);
    if (RRIL_RESULT_OK_IMMEDIATE == res)
    {
        RIL_onRequestComplete(rilToken, RIL_E_SUCCESS, NULL, 0);
        return RRIL_RESULT_OK;
    }
    else if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDeactivateDataCall() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_DEACTIVATE_DATA_CALL].uiChannel,
                rilToken, RIL_REQUEST_DEACTIVATE_DATA_CALL, reqData,
                &CTE::ParseDeactivateDataCall,
                &CTE::PostDeactivateDataCallCmdHandler);

        if (pCmd)
        {
            //  Call when radio is off.
            //  This happens when in data mode, and we go to flight mode.
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDeactivateDataCall() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDeactivateDataCall() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    if (RRIL_RESULT_OK != res)
    {
        CleanRequestData(reqData);
    }

    RIL_LOG_VERBOSE("CTE::RequestDeactivateDataCall() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDeactivateDataCall(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDeactivateDataCall() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseDeactivateDataCall(rRspData);
}

//
// RIL_REQUEST_QUERY_FACILITY_LOCK 42
//
RIL_RESULT_CODE CTE::RequestQueryFacilityLock(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryFacilityLock() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreQueryFacilityLock(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryFacilityLock() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_QUERY_FACILITY_LOCK].uiChannel,
                rilToken, RIL_REQUEST_QUERY_FACILITY_LOCK, reqData,
                &CTE::ParseQueryFacilityLock);

        if (pCmd)
        {
            //  Call when radio is off.
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryFacilityLock() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryFacilityLock() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryFacilityLock() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryFacilityLock(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryFacilityLock() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseQueryFacilityLock(rRspData);
}

//
// RIL_REQUEST_SET_FACILITY_LOCK 43
//
RIL_RESULT_CODE CTE::RequestSetFacilityLock(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetFacilityLock() - Enter\r\n");

    REQUEST_DATA reqData;
    CCommand* pCmd = NULL;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetFacilityLock(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetFacilityLock() - Unable to create AT command data\r\n");
    }
    else
    {
        pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_SET_FACILITY_LOCK].uiChannel,
                rilToken, RIL_REQUEST_SET_FACILITY_LOCK, reqData, &CTE::ParseSetFacilityLock,
                &CTE::PostSetFacilityLockCmdHandler);

        if (pCmd)
        {
            //  Call when radio is off.
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetFacilityLock() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetFacilityLock() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    if (RRIL_RESULT_OK != res)
    {
        CleanRequestData(reqData);
        delete pCmd;
    }

    RIL_LOG_VERBOSE("CTE::RequestSetFacilityLock() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetFacilityLock(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetFacilityLock() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetFacilityLock(rRspData);
}

//
// RIL_REQUEST_CHANGE_BARRING_PASSWORD 44
//
RIL_RESULT_CODE CTE::RequestChangeBarringPassword(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestChangeBarringPassword() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreChangeBarringPassword(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestChangeBarringPassword() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_CHANGE_BARRING_PASSWORD].uiChannel,
                rilToken, RIL_REQUEST_CHANGE_BARRING_PASSWORD, reqData,
                &CTE::ParseChangeBarringPassword);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestChangeBarringPassword() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestChangeBarringPassword() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestChangeBarringPassword() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseChangeBarringPassword(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseChangeBarringPassword() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseChangeBarringPassword(rRspData);
}

//
// RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE 45
//
RIL_RESULT_CODE CTE::RequestQueryNetworkSelectionMode(RIL_Token rilToken,
        void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryNetworkSelectionMode() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreQueryNetworkSelectionMode(reqData,
            pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryNetworkSelectionMode() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE].uiChannel,
                rilToken, RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE, reqData,
                &CTE::ParseQueryNetworkSelectionMode, &CTE::PostNetworkInfoCmdHandler);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryNetworkSelectionMode() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryNetworkSelectionMode() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryNetworkSelectionMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryNetworkSelectionMode(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryNetworkSelectionMode() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseQueryNetworkSelectionMode(rRspData);
}

//
// RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC 46
//
RIL_RESULT_CODE CTE::RequestSetNetworkSelectionAutomatic(RIL_Token rilToken,
                                                                void* pData,
                                                                size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetNetworkSelectionAutomatic() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetNetworkSelectionAutomatic(reqData,
            pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetNetworkSelectionAutomatic() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC].uiChannel,
                rilToken, RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC, reqData,
                &CTE::ParseSetNetworkSelectionAutomatic,
                &CTE::PostSetNetworkSelectionCmdHandler);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetNetworkSelectionAutomatic() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetNetworkSelectionAutomatic() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetNetworkSelectionAutomatic() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetNetworkSelectionAutomatic(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetNetworkSelectionAutomatic() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetNetworkSelectionAutomatic(rRspData);
}

//
// RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL 47
//
RIL_RESULT_CODE CTE::RequestSetNetworkSelectionManual(RIL_Token rilToken,
                                                             void* pData,
                                                             size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetNetworkSelectionManual() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetNetworkSelectionManual(reqData,
            pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetNetworkSelectionManual() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL].uiChannel,
                rilToken, RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL, reqData,
                &CTE::ParseSetNetworkSelectionManual,
                &CTE::PostSetNetworkSelectionCmdHandler);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetNetworkSelectionManual() - "
                        "Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetNetworkSelectionManual() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetNetworkSelectionManual() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetNetworkSelectionManual(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetNetworkSelectionManual() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetNetworkSelectionManual(rRspData);
}

//
// RIL_REQUEST_QUERY_AVAILABLE_NETWORKS 48
//
RIL_RESULT_CODE CTE::RequestQueryAvailableNetworks(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryAvailableNetworks() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    // If a setup data call is ongoing, delay the handling of this query (1 second)
    if (m_bIsSetupDataCallOngoing)
    {
        RIL_requestTimedCallback(triggerManualNetworkSearch, (void*)rilToken, 1, 0);
        return RRIL_RESULT_OK;
    }

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreQueryAvailableNetworks(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryAvailableNetworks() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_QUERY_AVAILABLE_NETWORKS].uiChannel,
                rilToken, RIL_REQUEST_QUERY_AVAILABLE_NETWORKS, reqData,
                &CTE::ParseQueryAvailableNetworks,
                &CTE::PostQueryAvailableNetworksCmdHandler);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryAvailableNetworks() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryAvailableNetworks() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    if (RRIL_RESULT_OK == res)
    {
        SetManualNetworkSearchOn(TRUE);
    }
    else
    {
        SetManualNetworkSearchOn(FALSE);
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryAvailableNetworks() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryAvailableNetworks(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryAvailableNetworks() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseQueryAvailableNetworks(rRspData);
}

//
// RIL_REQUEST_DTMF_START 49
//
RIL_RESULT_CODE CTE::RequestDtmfStart(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDtmfStart() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreDtmfStart(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDtmfStart() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_DTMF_START].uiChannel,
                NULL, RIL_REQUEST_DTMF_START, reqData, &CTE::ParseDtmfStart, &CTE::PostDtmfStart);

        if (pCmd)
        {
            pCmd->SetCallId(m_pTEBaseInstance->GetCurrentCallId());
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDtmfStart() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDtmfStart() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    if (RRIL_RESULT_OK == res)
    {
        RIL_onRequestComplete(rilToken, RIL_E_SUCCESS, NULL, 0);
    }

    RIL_LOG_VERBOSE("CTE::RequestDtmfStart() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDtmfStart(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDtmfStart() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseDtmfStart(rRspData);
}

//
// RIL_REQUEST_DTMF_STOP 50
//
RIL_RESULT_CODE CTE::RequestDtmfStop(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDtmfStop() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreDtmfStop(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDtmfStop() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_DTMF_STOP].uiChannel,
                NULL, RIL_REQUEST_DTMF_STOP, reqData, &CTE::ParseDtmfStop, &CTE::PostDtmfStop);

        if (pCmd)
        {
            pCmd->SetCallId(m_pTEBaseInstance->GetCurrentCallId());
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDtmfStop() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDtmfStop() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    if (RRIL_RESULT_OK == res)
    {
        RIL_onRequestComplete(rilToken, RIL_E_SUCCESS, NULL, 0);
    }

    RIL_LOG_VERBOSE("CTE::RequestDtmfStop() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDtmfStop(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDtmfStop() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseDtmfStop(rRspData);
}

//
// RIL_REQUEST_BASEBAND_VERSION 51
//
RIL_RESULT_CODE CTE::RequestBasebandVersion(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestBasebandVersion() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreBasebandVersion(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestBasebandVersion() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_BASEBAND_VERSION].uiChannel,
                rilToken, RIL_REQUEST_BASEBAND_VERSION, reqData, &CTE::ParseBasebandVersion);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestBasebandVersion() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestBasebandVersion() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestBasebandVersion() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseBasebandVersion(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseBasebandVersion() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseBasebandVersion(rRspData);
}

//
// RIL_REQUEST_SEPARATE_CONNECTION 52
//
RIL_RESULT_CODE CTE::RequestSeparateConnection(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSeparateConnection() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSeparateConnection(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSeparateConnection() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_SEPARATE_CONNECTION].uiChannel,
                rilToken, RIL_REQUEST_SEPARATE_CONNECTION, reqData,
                &CTE::ParseSeparateConnection);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSeparateConnection() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSeparateConnection() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    if (RRIL_RESULT_OK == res)
    {
        m_pTEBaseInstance->SetDtmfAllowed(m_pTEBaseInstance->GetCurrentCallId(), FALSE);
    }

    RIL_LOG_VERBOSE("CTE::RequestSeparateConnection() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSeparateConnection(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSeparateConnection() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSeparateConnection(rRspData);
}

//
// RIL_REQUEST_SET_MUTE 53
//
RIL_RESULT_CODE CTE::RequestSetMute(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetMute() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetMute(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetMute() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_SET_MUTE].uiChannel,
                rilToken, RIL_REQUEST_SET_MUTE, reqData, &CTE::ParseSetMute);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetMute() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetMute() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetMute() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetMute(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetMute() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetMute(rRspData);
}

//
// RIL_REQUEST_GET_MUTE 54
//
RIL_RESULT_CODE CTE::RequestGetMute(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetMute() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetMute(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetMute() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_GET_MUTE].uiChannel,
                rilToken, RIL_REQUEST_GET_MUTE, reqData, &CTE::ParseGetMute);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetMute() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetMute() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetMute() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetMute(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetMute() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetMute(rRspData);
}

//
// RIL_REQUEST_QUERY_CLIP 55
//
RIL_RESULT_CODE CTE::RequestQueryClip(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryClip() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreQueryClip(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryClip() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_QUERY_CLIP].uiChannel,
                rilToken, RIL_REQUEST_QUERY_CLIP, reqData, &CTE::ParseQueryClip);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryClip() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryClip() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryClip() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryClip(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryClip() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseQueryClip(rRspData);
}

//
// RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE 56
//
RIL_RESULT_CODE CTE::RequestLastDataCallFailCause(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestLastDataCallFailCause() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreLastDataCallFailCause(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestLastDataCallFailCause() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE].uiChannel,
                rilToken, RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE, reqData,
                &CTE::ParseLastDataCallFailCause);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestLastDataCallFailCause() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestLastDataCallFailCause() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestLastDataCallFailCause() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseLastDataCallFailCause(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseLastDataCallFailCause() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseLastDataCallFailCause(rRspData);
}

//
// RIL_REQUEST_DATA_CALL_LIST 57
//
RIL_RESULT_CODE CTE::RequestDataCallList(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDataCallList() - Enter\r\n");

    UINT32 usiCount = 0;
    P_ND_PDP_CONTEXT_DATA pPDPListData =
                (P_ND_PDP_CONTEXT_DATA)malloc(sizeof(S_ND_PDP_CONTEXT_DATA));
    if (NULL == pPDPListData)
    {
        RIL_LOG_CRITICAL("CTE::RequestDataCallList() -"
                " Could not allocate memory for a P_ND_PDP_CONTEXT_DATA struct.\r\n");
        goto Error;
    }
    memset(pPDPListData, 0, sizeof(S_ND_PDP_CONTEXT_DATA));

    usiCount = GetActiveDataCallInfoList(pPDPListData);

Error:
    if (usiCount > 0)
    {
        RIL_onRequestComplete(rilToken, RIL_E_SUCCESS, pPDPListData,
                                usiCount * sizeof(RIL_Data_Call_Response_v6));
    }
    else
    {
        RIL_onRequestComplete(rilToken, RIL_E_SUCCESS, NULL, 0);
    }
    free(pPDPListData);
    pPDPListData = NULL;

    RIL_LOG_VERBOSE("CTE::RequestDataCallList() - Exit\r\n");
    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_RESET_RADIO 58
//
RIL_RESULT_CODE CTE::RequestResetRadio(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestResetRadio() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreResetRadio(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestResetRadio() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_RESET_RADIO].uiChannel,
                rilToken, RIL_REQUEST_RESET_RADIO, reqData, &CTE::ParseResetRadio);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestResetRadio() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestResetRadio() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestResetRadio() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseResetRadio(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseResetRadio() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseResetRadio(rRspData);
}

//
// RIL_REQUEST_OEM_HOOK_RAW 59
//
RIL_RESULT_CODE CTE::RequestHookRaw(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestHookRaw() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    //  CoreHookRaw API chooses what RIL Channel to send command on.
    //  Channel is passed back through uiChannel parameter.
    //  Default is value defined in rilchannels.cpp.
    UINT32 uiRilChannel = g_pReqInfo[RIL_REQUEST_OEM_HOOK_RAW].uiChannel;

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreHookRaw(reqData, pData, datalen, uiRilChannel);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestHookRaw() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(uiRilChannel, rilToken, RIL_REQUEST_OEM_HOOK_RAW,
                reqData, &CTE::ParseHookRaw);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestHookRaw() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestHookRaw() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestHookRaw() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseHookRaw(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseHookRaw() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseHookRaw(rRspData);
}

//
// RIL_REQUEST_OEM_HOOK_STRINGS 60
//
RIL_RESULT_CODE CTE::RequestHookStrings(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestHookStrings() - Enter\r\n");

    UINT32 uiCommand = 0;
    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    //  CoreHookStrings API chooses what RIL Channel to send command on.
    //  Channel is passed back through uiChannel parameter.
    //  Default is value defined in rilchannels.cpp.
    UINT32 uiRilChannel = g_pReqInfo[RIL_REQUEST_OEM_HOOK_STRINGS].uiChannel;

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreHookStrings(reqData,
            pData, datalen, uiRilChannel);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestHookStrings() - Unable to create AT command data\r\n");
    }
    else
    {
        uiCommand = (UINT32)reqData.pContextData;
        int reqID = RIL_REQUEST_OEM_HOOK_STRINGS;
        if (RIL_OEM_HOOK_STRING_POWEROFF_MODEM == (int) uiCommand)
        {
            /*
             * Request Id is assigned to RIL_REQUEST_RADIO_POWER in order to use
             * the same request timeout value as RIL_REQUEST_RADIO_POWER and also
             * to allow this request always.
             */
            reqID = RIL_REQUEST_RADIO_POWER;
        }

        CCommand* pCmd = new CCommand(uiRilChannel, rilToken, reqID,
                reqData, &CTE::ParseHookStrings, &CTE::PostHookStrings);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestHookStrings() - "
                        "Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestHookStrings() - "
                    "Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    if (RIL_OEM_HOOK_STRING_POWEROFF_MODEM == (int) uiCommand)
    {
        if (RRIL_RESULT_OK == res)
        {
            CEvent* pModemPoweredOffEvent =
                    CSystemManager::GetInstance().GetModemPoweredOffEvent();
            if (NULL != pModemPoweredOffEvent)
            {
                CEvent::Reset(pModemPoweredOffEvent);

                CEvent::Wait(pModemPoweredOffEvent, WAIT_FOREVER);
            }
        }
        else
        {
            res = RRIL_RESULT_OK;
        }

        RIL_onRequestComplete(rilToken, RRIL_RESULT_OK, NULL, 0);
    }

    RIL_LOG_VERBOSE("CTE::RequestHookStrings() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseHookStrings(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseHookStrings() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseHookStrings(rRspData);
}


//
// RIL_REQUEST_SCREEN_STATE 61
//
RIL_RESULT_CODE CTE::RequestScreenState(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestScreenState() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE::RequestScreenState() - Data pointer is NULL.\r\n");
        goto Done;
    }

    switch (((int*)pData)[0])
    {
        case 0:
            m_ScreenState = SCREEN_STATE_OFF;
            break;
        case 1:
            m_ScreenState = SCREEN_STATE_ON;
            break;
        default:
            goto Done;
    }

    if (E_MMGR_EVENT_MODEM_UP == GetLastModemEvent())
    {
        m_pTEBaseInstance->CoreScreenState(reqData, pData, datalen);
    }

Done:
    RIL_onRequestComplete(rilToken, RRIL_RESULT_OK, NULL, 0);

    RIL_LOG_VERBOSE("CTE::RequestScreenState() - Exit\r\n");
    return RRIL_RESULT_OK;
}

RIL_RESULT_CODE CTE::ParseScreenState(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseScreenState() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseScreenState(rRspData);
}

//
// RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION 62
//
RIL_RESULT_CODE CTE::RequestSetSuppSvcNotification(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetSuppSvcNotification() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetSuppSvcNotification(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetSuppSvcNotification() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION].uiChannel,
                rilToken, RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION, reqData,
                &CTE::ParseSetSuppSvcNotification);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetSuppSvcNotification() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetSuppSvcNotification() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetSuppSvcNotification() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetSuppSvcNotification(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetSuppSvcNotification() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetSuppSvcNotification(rRspData);
}

//
// RIL_REQUEST_WRITE_SMS_TO_SIM 63
//
RIL_RESULT_CODE CTE::RequestWriteSmsToSim(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestWriteSmsToSim() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreWriteSmsToSim(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestWriteSmsToSim() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_WRITE_SMS_TO_SIM].uiChannel,
                rilToken, RIL_REQUEST_WRITE_SMS_TO_SIM, reqData, &CTE::ParseWriteSmsToSim,
                &CTE::PostWriteSmsToSimCmdHandler);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestWriteSmsToSim() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestWriteSmsToSim() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestWriteSmsToSim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseWriteSmsToSim(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseWriteSmsToSim() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseWriteSmsToSim(rRspData);
}

//
// RIL_REQUEST_DELETE_SMS_ON_SIM 64
//
RIL_RESULT_CODE CTE::RequestDeleteSmsOnSim(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDeleteSmsOnSim() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreDeleteSmsOnSim(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDeleteSmsOnSim() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_DELETE_SMS_ON_SIM].uiChannel,
                rilToken, RIL_REQUEST_DELETE_SMS_ON_SIM, reqData, &CTE::ParseDeleteSmsOnSim);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDeleteSmsOnSim() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDeleteSmsOnSim() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestDeleteSmsOnSim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDeleteSmsOnSim(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDeleteSmsOnSim() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseDeleteSmsOnSim(rRspData);
}

//
// RIL_REQUEST_SET_BAND_MODE 65
//
RIL_RESULT_CODE CTE::RequestSetBandMode(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetBandMode() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetBandMode(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetBandMode() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_SET_BAND_MODE].uiChannel,
                rilToken, RIL_REQUEST_SET_BAND_MODE, reqData, &CTE::ParseSetBandMode);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetBandMode() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetBandMode() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetBandMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetBandMode(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetBandMode() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetBandMode(rRspData);
}

//
// RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE 66
//
RIL_RESULT_CODE CTE::RequestQueryAvailableBandMode(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryAvailableBandMode() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreQueryAvailableBandMode(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryAvailableBandMode() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE].uiChannel,
                rilToken, RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE, reqData,
                &CTE::ParseQueryAvailableBandMode);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryAvailableBandMode() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryAvailableBandMode() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryAvailableBandMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryAvailableBandMode(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryAvailableBandMode() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseQueryAvailableBandMode(rRspData);
}

//
// RIL_REQUEST_STK_GET_PROFILE 67
//
RIL_RESULT_CODE CTE::RequestStkGetProfile(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestStkGetProfile() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreStkGetProfile(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestStkGetProfile() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_STK_GET_PROFILE].uiChannel,
                rilToken, RIL_REQUEST_STK_GET_PROFILE, reqData, &CTE::ParseStkGetProfile);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestStkGetProfile() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestStkGetProfile() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestStkGetProfile() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseStkGetProfile(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseStkGetProfile() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseStkGetProfile(rRspData);
}

//
// RIL_REQUEST_STK_SET_PROFILE 68
//
RIL_RESULT_CODE CTE::RequestStkSetProfile(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestStkSetProfile() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreStkSetProfile(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestStkSetProfile() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_STK_SET_PROFILE].uiChannel,
                rilToken, RIL_REQUEST_STK_SET_PROFILE, reqData, &CTE::ParseStkSetProfile);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestStkSetProfile() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestStkSetProfile() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestStkSetProfile() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseStkSetProfile(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseStkSetProfile() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseStkSetProfile(rRspData);
}

//
// RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND 69
//
RIL_RESULT_CODE CTE::RequestStkSendEnvelopeCommand(RIL_Token rilToken,
                                                          void* pData,
                                                          size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestStkSendEnvelopeCommand() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreStkSendEnvelopeCommand(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestStkSendEnvelopeCommand() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND].uiChannel,
                rilToken, RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND, reqData,
                &CTE::ParseStkSendEnvelopeCommand);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestStkSendEnvelopeCommand() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestStkSendEnvelopeCommand() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestStkSendEnvelopeCommand() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseStkSendEnvelopeCommand(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseStkSendEnvelopeCommand() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseStkSendEnvelopeCommand(rRspData);
}

//
// RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE 70
//
RIL_RESULT_CODE CTE::RequestStkSendTerminalResponse(RIL_Token rilToken,
                                                           void* pData,
                                                           size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestStkSendTerminalResponse() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreStkSendTerminalResponse(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestStkSendTerminalResponse() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE].uiChannel,
                rilToken, RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE, reqData,
                &CTE::ParseStkSendTerminalResponse);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestStkSendTerminalResponse() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestStkSendTerminalResponse() -"
                     " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestStkSendTerminalResponse() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseStkSendTerminalResponse(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseStkSendTerminalResponse() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseStkSendTerminalResponse(rRspData);
}

//
// RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM 71
//
RIL_RESULT_CODE CTE::RequestStkHandleCallSetupRequestedFromSim(RIL_Token rilToken,
                                                                      void* pData,
                                                                      size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestStkHandleCallSetupRequestedFromSim() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreStkHandleCallSetupRequestedFromSim(reqData,
            pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestStkHandleCallSetupRequestedFromSim() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM].uiChannel,
                rilToken, RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM, reqData,
                &CTE::ParseStkHandleCallSetupRequestedFromSim);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestStkHandleCallSetupRequestedFromSim() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestStkHandleCallSetupRequestedFromSim() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestStkHandleCallSetupRequestedFromSim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseStkHandleCallSetupRequestedFromSim(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseStkHandleCallSetupRequestedFromSim() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseStkHandleCallSetupRequestedFromSim(rRspData);
}

//
// RIL_REQUEST_EXPLICIT_CALL_TRANSFER 72
//
RIL_RESULT_CODE CTE::RequestExplicitCallTransfer(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestExplicitCallTransfer() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreExplicitCallTransfer(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestExplicitCallTransfer() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_EXPLICIT_CALL_TRANSFER].uiChannel,
                rilToken, RIL_REQUEST_EXPLICIT_CALL_TRANSFER, reqData,
                &CTE::ParseExplicitCallTransfer);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestExplicitCallTransfer() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestExplicitCallTransfer() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestExplicitCallTransfer() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseExplicitCallTransfer(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseExplicitCallTransfer() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseExplicitCallTransfer(rRspData);
}

//
// RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
//
RIL_RESULT_CODE CTE::RequestSetPreferredNetworkType(RIL_Token rilToken,
                                                           void* pData,
                                                           size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetPreferredNetworkType() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetPreferredNetworkType(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetPreferredNetworkType() :"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE].uiChannel,
                rilToken, RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE, reqData,
                &CTE::ParseSetPreferredNetworkType);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetPreferredNetworkType() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetPreferredNetworkType() -"
                     " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetPreferredNetworkType() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetPreferredNetworkType(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetPreferredNetworkType() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetPreferredNetworkType(rRspData);
}

//
// RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
//
RIL_RESULT_CODE CTE::RequestGetPreferredNetworkType(RIL_Token rilToken,
                                                           void* pData,
                                                           size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetPreferredNetworkType() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetPreferredNetworkType(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetPreferredNetworkType() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE].uiChannel,
                rilToken, RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE, reqData,
                &CTE::ParseGetPreferredNetworkType);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetPreferredNetworkType() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetPreferredNetworkType() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetPreferredNetworkType() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetPreferredNetworkType(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetPreferredNetworkType() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetPreferredNetworkType(rRspData);
}

//
// RIL_REQUEST_GET_NEIGHBORING_CELL_IDS 75
//
RIL_RESULT_CODE CTE::RequestGetNeighboringCellIDs(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetNeighboringCellIDs() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetNeighboringCellIDs(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetNeighboringCellIDs() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_GET_NEIGHBORING_CELL_IDS].uiChannel,
                rilToken, RIL_REQUEST_GET_NEIGHBORING_CELL_IDS, reqData,
                &CTE::ParseGetNeighboringCellIDs, &CTE::PostGetNeighboringCellIDs);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetNeighboringCellIDs() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetNeighboringCellIDs() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetNeighboringCellIDs() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetNeighboringCellIDs(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetNeighboringCellIDs() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetNeighboringCellIDs(rRspData);
}

//
// RIL_REQUEST_SET_LOCATION_UPDATES 76
//
RIL_RESULT_CODE CTE::RequestSetLocationUpdates(RIL_Token rilToken, void* pData,
        size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetLocationUpdates() - Enter\r\n");

    REQUEST_DATA reqData;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int enableLocationUpdates = 0;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTEBase::RequestSetLocationUpdates() - Data pointer is NULL.\r\n");
        goto Error;
    }

    enableLocationUpdates = ((int*)pData)[0];
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    res = m_pTEBaseInstance->CoreSetLocationUpdates(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetLocationUpdates() - Unable to create AT command"
                "data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_SET_LOCATION_UPDATES].uiChannel,
                rilToken, RIL_REQUEST_SET_LOCATION_UPDATES, reqData,
                &CTE::ParseSetLocationUpdates, &CTE::PostSetLocationUpdates);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetLocationUpdates() - Unable to add command to "
                        "queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetLocationUpdates() - Unable to allocate memory for "
                    "command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

Error:
    if (RRIL_RESULT_OK == res)
    {
        m_enableLocationUpdates = enableLocationUpdates;
    }

    RIL_LOG_VERBOSE("CTE::RequestSetLocationUpdates() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetLocationUpdates(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetLocationUpdates() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetLocationUpdates(rRspData);
}

//
// RIL_REQUEST_CDMA_SET_SUBSCRIPTION_SOURCE 77
//
RIL_RESULT_CODE CTE::RequestCdmaSetSubscription(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSetSubscription() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreCdmaSetSubscription(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestCdmaSetSubscription() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_CDMA_SET_SUBSCRIPTION_SOURCE].uiChannel,
                rilToken, RIL_REQUEST_CDMA_SET_SUBSCRIPTION_SOURCE, reqData,
                &CTE::ParseCdmaSetSubscription);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestCdmaSetSubscription() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestCdmaSetSubscription() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestCdmaSetSubscription() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaSetSubscription(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSetSubscription() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaSetSubscription(rRspData);
}

//
// RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE 78
//
RIL_RESULT_CODE CTE::RequestCdmaSetRoamingPreference(RIL_Token rilToken,
                                                            void* pData,
                                                            size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSetRoamingPreference() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreCdmaSetRoamingPreference(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestCdmaSetRoamingPreference() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE].uiChannel,
                rilToken, RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE, reqData,
                &CTE::ParseCdmaSetRoamingPreference);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestCdmaSetRoamingPreference() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestCdmaSetRoamingPreference() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestCdmaSetRoamingPreference() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaSetRoamingPreference(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSetRoamingPreference() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaSetRoamingPreference(rRspData);
}

//
// RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE 79
//
RIL_RESULT_CODE CTE::RequestCdmaQueryRoamingPreference(RIL_Token rilToken,
                                                              void* pData,
                                                              size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaQueryRoamingPreference() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreCdmaQueryRoamingPreference(reqData,
            pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestCdmaQueryRoamingPreference() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE].uiChannel,
                rilToken, RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE, reqData,
                &CTE::ParseCdmaQueryRoamingPreference);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestCdmaQueryRoamingPreference() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestCdmaQueryRoamingPreference() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestCdmaQueryRoamingPreference() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseCdmaQueryRoamingPreference(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaQueryRoamingPreference() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaQueryRoamingPreference(rRspData);
}

//
// RIL_REQUEST_SET_TTY_MODE 80
//
RIL_RESULT_CODE CTE::RequestSetTtyMode(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetTtyMode() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetTtyMode(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetTtyMode() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_SET_TTY_MODE].uiChannel,
                rilToken, RIL_REQUEST_SET_TTY_MODE, reqData, &CTE::ParseSetTtyMode);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetTtyMode() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetTtyMode() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetTtyMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSetTtyMode(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetTtyMode() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetTtyMode(rRspData);
}

//
// RIL_REQUEST_QUERY_TTY_MODE 81
//
RIL_RESULT_CODE CTE::RequestQueryTtyMode(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryTtyMode() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreQueryTtyMode(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryTtyMode() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_QUERY_TTY_MODE].uiChannel,
                rilToken, RIL_REQUEST_QUERY_TTY_MODE, reqData, &CTE::ParseQueryTtyMode);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryTtyMode() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryTtyMode() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestQueryTtyMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseQueryTtyMode(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryTtyMode() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseQueryTtyMode(rRspData);
}

//
// RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE 82
//
RIL_RESULT_CODE CTE::RequestCdmaSetPreferredVoicePrivacyMode(RIL_Token rilToken,
                                                                    void* pData,
                                                                    size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSetPreferredVoicePrivacyMode() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaSetPreferredVoicePrivacyMode(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSetPreferredVoicePrivacyMode() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaSetPreferredVoicePrivacyMode(rRspData);
}

//
// RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE 83
//
RIL_RESULT_CODE CTE::RequestCdmaQueryPreferredVoicePrivacyMode(RIL_Token rilToken,
                                                                      void* pData,
                                                                      size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaQueryPreferredVoicePrivacyMode() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaQueryPreferredVoicePrivacyMode(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaQueryPreferredVoicePrivacyMode() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaQueryPreferredVoicePrivacyMode(rRspData);
}

//
// RIL_REQUEST_CDMA_FLASH 84
//
RIL_RESULT_CODE CTE::RequestCdmaFlash(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaFlash() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaFlash(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaFlash() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaFlash(rRspData);
}

//
// RIL_REQUEST_CDMA_BURST_DTMF 85
//
RIL_RESULT_CODE CTE::RequestCdmaBurstDtmf(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaBurstDtmf() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaBurstDtmf(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaBurstDtmf() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaBurstDtmf(rRspData);
}

//
// RIL_REQUEST_CDMA_VALIDATE_AND_WRITE_AKEY 86
//
RIL_RESULT_CODE CTE::RequestCdmaValidateAndWriteAkey(RIL_Token rilToken,
                                                            void* pData,
                                                            size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaValidateAndWriteAkey() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaValidateAndWriteAkey(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaValidateAndWriteAkey() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaValidateAndWriteAkey(rRspData);
}

//
// RIL_REQUEST_CDMA_SEND_SMS 87
//
RIL_RESULT_CODE CTE::RequestCdmaSendSms(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSendSms() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaSendSms(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSendSms() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaSendSms(rRspData);
}

//
// RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE 88
//
RIL_RESULT_CODE CTE::RequestCdmaSmsAcknowledge(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSmsAcknowledge() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaSmsAcknowledge(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSmsAcknowledge() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaSmsAcknowledge(rRspData);
}

//
// RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG 89
//
RIL_RESULT_CODE CTE::RequestGsmGetBroadcastSmsConfig(RIL_Token rilToken,
                                                            void* pData,
                                                            size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGsmGetBroadcastSmsConfig() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGsmGetBroadcastSmsConfig(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGsmGetBroadcastSmsConfig() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG].uiChannel,
                rilToken, RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG, reqData,
                &CTE::ParseGsmGetBroadcastSmsConfig);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGsmGetBroadcastSmsConfig() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGsmGetBroadcastSmsConfig() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGsmGetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGsmGetBroadcastSmsConfig(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGsmGetBroadcastSmsConfig() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGsmGetBroadcastSmsConfig(rRspData);
}

//
// RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG 90
//
RIL_RESULT_CODE CTE::RequestGsmSetBroadcastSmsConfig(RIL_Token rilToken,
                                                            void* pData,
                                                            size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGsmSetBroadcastSmsConfig() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGsmSetBroadcastSmsConfig(reqData, pData, datalen);
    if (RRIL_RESULT_OK_IMMEDIATE == res)
    {
        res = RRIL_RESULT_OK;
        RIL_onRequestComplete(rilToken, RRIL_RESULT_OK, NULL, 0);
    }
    else if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGsmSetBroadcastSmsConfig() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG].uiChannel,
                rilToken, RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG, reqData,
                &CTE::ParseGsmSetBroadcastSmsConfig);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGsmSetBroadcastSmsConfig() -"
                         " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGsmSetBroadcastSmsConfig() -"
                     " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGsmSetBroadcastSmsConfig() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGsmSetBroadcastSmsConfig(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGsmSetBroadcastSmsConfig() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGsmSetBroadcastSmsConfig(rRspData);
}

//
// RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION 91
//
RIL_RESULT_CODE CTE::RequestGsmSmsBroadcastActivation(RIL_Token rilToken,
                                                             void* pData,
                                                             size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGsmSmsBroadcastActivation() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGsmSmsBroadcastActivation(reqData,
            pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGsmSmsBroadcastActivation() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION].uiChannel,
                rilToken, RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION, reqData,
                &CTE::ParseGsmSmsBroadcastActivation);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGsmSmsBroadcastActivation() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGsmSmsBroadcastActivation() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGsmSmsBroadcastActivation() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGsmSmsBroadcastActivation(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGsmSmsBroadcastActivation() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGsmSmsBroadcastActivation(rRspData);
}

//
// RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG 92
//
RIL_RESULT_CODE CTE::RequestCdmaGetBroadcastSmsConfig(RIL_Token rilToken,
                                                             void* pData,
                                                             size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaGetBroadcastSmsConfig() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaGetBroadcastSmsConfig(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaGetBroadcastSmsConfig() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaGetBroadcastSmsConfig(rRspData);
}

//
// RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG 93
//
RIL_RESULT_CODE CTE::RequestCdmaSetBroadcastSmsConfig(RIL_Token rilToken,
                                                             void* pData,
                                                             size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSetBroadcastSmsConfig() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaSetBroadcastSmsConfig(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSetBroadcastSmsConfig() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaSetBroadcastSmsConfig(rRspData);
}

//
// RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION 94
//
RIL_RESULT_CODE CTE::RequestCdmaSmsBroadcastActivation(RIL_Token rilToken,
                                                              void* pData,
                                                              size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSmsBroadcastActivation() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaSmsBroadcastActivation(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSmsBroadcastActivation() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaSmsBroadcastActivation(rRspData);
}

//
// RIL_REQUEST_CDMA_SUBSCRIPTION 95
//
RIL_RESULT_CODE CTE::RequestCdmaSubscription(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaSubscription() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaSubscription(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaSubscription() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaSubscription(rRspData);
}

//
// RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM 96
//
RIL_RESULT_CODE CTE::RequestCdmaWriteSmsToRuim(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaWriteSmsToRuim() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaWriteSmsToRuim(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaWriteSmsToRuim() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaWriteSmsToRuim(rRspData);
}

//
// RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM 97
//
RIL_RESULT_CODE CTE::RequestCdmaDeleteSmsOnRuim(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestCdmaDeleteSmsOnRuim() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseCdmaDeleteSmsOnRuim(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseCdmaDeleteSmsOnRuim() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCdmaDeleteSmsOnRuim(rRspData);
}

//
// RIL_REQUEST_DEVICE_IDENTITY 98
//
RIL_RESULT_CODE CTE::RequestDeviceIdentity(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDeviceIdentity() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseDeviceIdentity(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDeviceIdentity() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseDeviceIdentity(rRspData);
}

//
// RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE 99
//
RIL_RESULT_CODE CTE::RequestExitEmergencyCallbackMode(RIL_Token rilToken,
                                                             void* pData,
                                                             size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestExitEmergencyCallbackMode() - Enter / Exit\r\n");

    return RRIL_RESULT_NOTSUPPORTED;
}

RIL_RESULT_CODE CTE::ParseExitEmergencyCallbackMode(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseExitEmergencyCallbackMode() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseExitEmergencyCallbackMode(rRspData);
}

//
// RIL_REQUEST_GET_SMSC_ADDRESS 100
//
RIL_RESULT_CODE CTE::RequestGetSmscAddress(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetSmscAddress() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetSmscAddress(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetSmscAddress() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_GET_SMSC_ADDRESS].uiChannel,
                rilToken, RIL_REQUEST_GET_SMSC_ADDRESS, reqData, &CTE::ParseGetSmscAddress);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetSmscAddress() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetSmscAddress() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetSmscAddress() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetSmscAddress(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetSmscAddress() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetSmscAddress(rRspData);
}

//
// RIL_REQUEST_SET_SMSC_ADDRESS 101
//
RIL_RESULT_CODE CTE::RequestSetSmscAddress(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetSmscAddress() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetSmscAddress(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetSmscAddress() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_SET_SMSC_ADDRESS].uiChannel,
                rilToken, RIL_REQUEST_SET_SMSC_ADDRESS, reqData, &CTE::ParseSetSmscAddress);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSetSmscAddress() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSetSmscAddress() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSetSmscAddress() - Exit\r\n");
    return res;
}


RIL_RESULT_CODE CTE::ParseSetSmscAddress(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetSmscAddress() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetSmscAddress(rRspData);
}

//
// RIL_REQUEST_REPORT_SMS_MEMORY_STATUS 102
//
RIL_RESULT_CODE CTE::RequestReportSmsMemoryStatus(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestReportSmsMemoryStatus() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreReportSmsMemoryStatus(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestReportSmsMemoryStatus() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_REPORT_SMS_MEMORY_STATUS].uiChannel,
                rilToken, RIL_REQUEST_REPORT_SMS_MEMORY_STATUS, reqData,
                &CTE::ParseReportSmsMemoryStatus);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestReportSmsMemoryStatus() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestReportSmsMemoryStatus() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestReportSmsMemoryStatus() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseReportSmsMemoryStatus(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseReportSmsMemoryStatus() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseReportSmsMemoryStatus(rRspData);
}

//
// RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING 103
//
RIL_RESULT_CODE CTE::RequestReportStkServiceRunning(RIL_Token rilToken,
                                                           void* pData,
                                                           size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestReportStkServiceRunning() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreReportStkServiceRunning(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestReportStkServiceRunning() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING].uiChannel,
                rilToken, RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING, reqData,
                &CTE::ParseReportStkServiceRunning);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestReportStkServiceRunning() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestReportStkServiceRunning() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestReportStkServiceRunning() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseReportStkServiceRunning(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseReportStkServiceRunning() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseReportStkServiceRunning(rRspData);
}

//
// RIL_REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU 106
//
RIL_RESULT_CODE CTE::RequestAckIncomingGsmSmsWithPdu(RIL_Token rilToken,
                                                            void* pData,
                                                            size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestAckIncomingGsmSmsWithPdu() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreAckIncomingGsmSmsWithPdu(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestAckIncomingGsmSmsWithPdu() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU].uiChannel,
                rilToken, RIL_REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU, reqData,
                &CTE::ParseAckIncomingGsmSmsWithPdu);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestAckIncomingGsmSmsWithPdu() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestAckIncomingGsmSmsWithPdu() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestAckIncomingGsmSmsWithPdu() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseAckIncomingGsmSmsWithPdu(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseAckIncomingGsmSmsWithPdu() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseAckIncomingGsmSmsWithPdu(rRspData);
}

//
// RIL_REQUEST_STK_SEND_ENVELOPE_WITH_STATUS 107
//
RIL_RESULT_CODE CTE::RequestStkSendEnvelopeWithStatus(RIL_Token rilToken,
                                                             void* pData,
                                                             size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestStkSendEnvelopeWithStatus() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreStkSendEnvelopeCommand(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestStkSendEnvelopeWithStatus() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_STK_SEND_ENVELOPE_WITH_STATUS].uiChannel,
                rilToken, RIL_REQUEST_STK_SEND_ENVELOPE_WITH_STATUS, reqData,
                &CTE::ParseStkSendEnvelopeWithStatus);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestStkSendEnvelopeWithStatus() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestStkSendEnvelopeWithStatus() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestStkSendEnvelopeWithStatus() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseStkSendEnvelopeWithStatus(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseStkSendEnvelopeWithStatus() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseStkSendEnvelopeWithStatus(rRspData);
}

//
// RIL_REQUEST_VOICE_RADIO_TECH 108
//
RIL_RESULT_CODE CTE::RequestVoiceRadioTech(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestVoiceRadioTech() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;
    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    if (RADIO_STATE_ON != GetRadioState())
    {
        RIL_LOG_INFO("CTE::RequestVoiceRadioTech() - Radio state is not ON!\r\n");
        res = RIL_E_RADIO_NOT_AVAILABLE;
    }
    else
    {
        // for now the voice radio technology is arbitrarily hardcoded to one
        // of the defined GSM RIL_RadioTechnology values defined in ril.h
        // It is used by Android to differentiate between CDMA or GSM radio technologies.
        // See RIL_REQUEST_VOICE_RADIO_TECH in ril.h for more info.
        int voiceRadioTech = RADIO_TECH_GSM;

        RIL_onRequestComplete(rilToken, RIL_E_SUCCESS, &voiceRadioTech, sizeof(int*));
    }

    RIL_LOG_VERBOSE("CTE::RequestVoiceRadioTech() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_CELL_INFO_LIST 109
//
RIL_RESULT_CODE CTE::RequestGetCellInfoList(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetCellInfoList() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetCellInfoList(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetCellInfoList() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_GET_CELL_INFO_LIST].uiChannel,
                rilToken, RIL_REQUEST_GET_CELL_INFO_LIST, reqData,
                &CTE::ParseGetCellInfoList, &CTE::PostGetCellInfoList);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetCellInfoList() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetCellInfoList() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetCellInfoList() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetCellInfoList(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetCellInfoList() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseCellInfoList(rRspData);
}

//
// RIL_REQUEST_SET_UNSOL_CELL_INFO_LIST_RATE 110
//
RIL_RESULT_CODE CTE::RequestSetCellInfoListRate(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSetCellInfoListRate() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSetCellInfoListRate(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSetCellInfoListRate() -"
                " Unable to set the timer for cellinfoListRate\r\n");
    }
    else
    {
        RIL_onRequestComplete(rilToken, RRIL_RESULT_OK, NULL, 0);
    }

    RIL_LOG_VERBOSE("CTE::RequestSetCellInfoListRate() - Exit\r\n");
    return res;
}


//
// RIL_REQUEST_SIM_TRANSMIT_BASIC 111
//
RIL_RESULT_CODE CTE::RequestSimTransmitBasic(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSimTransmitBasic() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSimTransmitBasic(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSimTransmitBasic() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_SIM_TRANSMIT_BASIC].uiChannel,
                rilToken, RIL_REQUEST_SIM_TRANSMIT_BASIC, reqData, &CTE::ParseSimTransmitBasic);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSimTransmitBasic() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSimTransmitBasic() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSimTransmitBasic() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSimTransmitBasic(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSimTransmitBasic() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSimTransmitBasic(rRspData);
}

//
// RIL_REQUEST_SIM_OPEN_CHANNEL 112
//
RIL_RESULT_CODE CTE::RequestSimOpenChannel(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSimOpenChannel() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSimOpenChannel(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSimOpenChannel() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_SIM_OPEN_CHANNEL].uiChannel,
                rilToken, RIL_REQUEST_SIM_OPEN_CHANNEL, reqData, &CTE::ParseSimOpenChannel);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSimOpenChannel() - "
                        "Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSimOpenChannel() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSimOpenChannel() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSimOpenChannel(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSimOpenChannel() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSimOpenChannel(rRspData);
}

//
// RIL_REQUEST_SIM_CLOSE_CHANNEL 113
//
RIL_RESULT_CODE CTE::RequestSimCloseChannel(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSimCloseChannel() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSimCloseChannel(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSimCloseChannel() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_SIM_CLOSE_CHANNEL].uiChannel,
                rilToken, RIL_REQUEST_SIM_CLOSE_CHANNEL, reqData, &CTE::ParseSimCloseChannel);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSimCloseChannel() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSimCloseChannel() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSimCloseChannel() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSimCloseChannel(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSimCloseChannel() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSimCloseChannel(rRspData);
}

//
// RIL_REQUEST_SIM_TRANSMIT_CHANNEL 114
//
RIL_RESULT_CODE CTE::RequestSimTransmitChannel(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestSimTransmitChannel() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreSimTransmitChannel(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSimTransmitChannel() -"
                " Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_SIM_TRANSMIT_CHANNEL].uiChannel,
                rilToken, RIL_REQUEST_SIM_TRANSMIT_CHANNEL, reqData,
                &CTE::ParseSimTransmitChannel);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestSimTransmitChannel() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSimTransmitChannel() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSimTransmitChannel() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSimTransmitChannel(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSimTransmitChannel() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSimTransmitChannel(rRspData);
}


#if defined(M2_VT_FEATURE_ENABLED)
//
// RIL_REQUEST_HANGUP_VT 115
//
RIL_RESULT_CODE CTE::RequestHangupVT(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestHangupVT() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreHangupVT(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestHangupVT() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_HANGUP_VT].uiChannel,
                rilToken, RIL_REQUEST_HANGUP_VT, reqData, &CTE::ParseHangupVT,
                &CTE::PostHangupCmdHandler);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestHangupVT() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestHangupVT() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestHangupVT() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseHangupVT(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseHangupVT() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseHangupVT(rRspData);
}


//
// RIL_REQUEST_DIAL_VT 116
//
RIL_RESULT_CODE CTE::RequestDialVT(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestDialVT() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreDialVT(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestDialVT() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[RIL_REQUEST_DIAL_VT].uiChannel,
                rilToken, RIL_REQUEST_DIAL_VT, reqData, &CTE::ParseDialVT,
                &CTE::PostDialCmdHandler);

        if (pCmd)
        {
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestDialVT() - Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestDialVT() - Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestDialVT() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseDialVT(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDialVT() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseDialVT(rRspData);
}
#endif // M2_VT_FEATURE_ENABLED


#if defined(M2_GET_SIM_SMS_STORAGE_ENABLED)
//
// RIL_REQUEST_GET_SIM_SMS_STORAGE 117
//
RIL_RESULT_CODE CTE::RequestGetSimSmsStorage(RIL_Token rilToken, void* pData, size_t datalen)
{
    RIL_LOG_VERBOSE("CTE::RequestGetSimSmsStorage() - Enter\r\n");

    REQUEST_DATA reqData;
    memset(&reqData, 0, sizeof(REQUEST_DATA));

    RIL_RESULT_CODE res = m_pTEBaseInstance->CoreGetSimSmsStorage(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestGetSimSmsStorage() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(
                g_pReqInfo[RIL_REQUEST_GET_SIM_SMS_STORAGE].uiChannel,
                rilToken, RIL_REQUEST_GET_SIM_SMS_STORAGE, reqData,
                &CTE::ParseGetSimSmsStorage);

        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestGetSimSmsStorage() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestGetSimSmsStorage() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestGetSimSmsStorage() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseGetSimSmsStorage(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetSimSmsStorage() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseGetSimSmsStorage(rRspData);
}
#endif // M2_GET_SIM_SMS_STORAGE_ENABLED

//
// RIL_UNSOL_SIGNAL_STRENGTH  1009
//
RIL_RESULT_CODE CTE::ParseUnsolicitedSignalStrength(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseUnsolicitedSignalStrength() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseUnsolicitedSignalStrength(rRspData);
}

RIL_RadioTechnology CTE::MapAccessTechnology(UINT32 uiStdAct)
{
    RIL_LOG_VERBOSE("CTE::MapAccessTechnology() - Enter\r\n");

    return m_pTEBaseInstance->MapAccessTechnology(uiStdAct);
}

BOOL CTE::ParseCREG(const char*& rszPointer, const BOOL bUnSolicited,
                                   S_ND_REG_STATUS& rCSRegStatusInfo)
{
    RIL_LOG_VERBOSE("CTE::ParseCREG() - Enter\r\n");

    UINT32 uiNum;
    UINT32 uiStatus = 0;
    UINT32 uiLAC = 0;
    UINT32 uiCID = 0;
    UINT32 uiAct = 0;
    UINT32 uiCauseType = 0;
    UINT32 uiRejectCause = 0;
    RIL_RadioTechnology rtAct = RADIO_TECH_UNKNOWN;
    BOOL bRet = false;
    char szNewLine[3] = "\r\n";

    if (!bUnSolicited)
    {
        // Skip "<prefix>"
        if (!SkipRspStart(rszPointer, szNewLine, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCREG() - Could not skip response prefix.\r\n");
            goto Error;
        }

        // Skip "<,prefix> string"
        if (!SkipString(rszPointer, "+CREG: ", rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCREG() - Could not skip \"+CREG: \".\r\n");
            goto Error;
        }

        // Extract <n> and throw away
        if (!ExtractUInt32(rszPointer, uiNum, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCREG() - Could not extract <n>.\r\n");
            goto Error;
        }

        // Skip ","
        if (!SkipString(rszPointer, ",", rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCREG() - Could not extract <stat>.\r\n");
            goto Error;
        }
    }

    // "<stat>"
    if (!ExtractUInt32(rszPointer, uiStatus, rszPointer))
    {
        RIL_LOG_CRITICAL("CTE::ParseCREG() - Could not extract <stat>.\r\n");
        goto Error;
    }

    // Do we have more to parse?
    if (SkipString(rszPointer, ",", rszPointer))
    {
        // Extract "<lac>"
        if (SkipString(rszPointer, "\"", rszPointer))
        {
            if (!ExtractHexUInt32(rszPointer, uiLAC, rszPointer))
            {
                RIL_LOG_INFO("CTE::ParseCREG() - Could not extract <lac>.\r\n");
            }
            SkipString(rszPointer, "\"", rszPointer);
        }

        // Extract ",<cid>"
        if (SkipString(rszPointer, ",", rszPointer))
        {
            if (SkipString(rszPointer, "\"", rszPointer))
            {
                if (!ExtractHexUInt32(rszPointer, uiCID, rszPointer))
                {
                    RIL_LOG_INFO("CTE::ParseCREG() - Could not extract <cid>.\r\n");
                }
                SkipString(rszPointer, "\"", rszPointer);
            }
        }

        // Extract ",<Act>"
        if (SkipString(rszPointer, ",", rszPointer))
        {
            if (!ExtractUInt32(rszPointer, uiAct, rszPointer))
            {
                RIL_LOG_INFO("CTE::ParseCREG() - Could not extract <act>.\r\n");
            }

            /*
             * Maps the 3GPP standard access technology values to android specific access
             * technology values.
             */
            rtAct = MapAccessTechnology(uiAct);
            snprintf(rCSRegStatusInfo.szNetworkType, REG_STATUS_LENGTH, "%d", (int)rtAct);
        }

        // Extract ",cause_type and reject_cause" only if registration status is denied
        if (E_REGISTRATION_DENIED == uiStatus)
        {
            if (SkipString(rszPointer, ",", rszPointer))
            {
                if (!ExtractUInt32(rszPointer, uiCauseType, rszPointer))
                {
                    RIL_LOG_CRITICAL("CTE::ParseCREG() - Could not extract <cause_type>.\r\n");
                }

                if (!SkipString(rszPointer, ",", rszPointer)
                        || !ExtractUInt32(rszPointer, uiRejectCause, rszPointer))
                {
                    RIL_LOG_CRITICAL("CTE::ParseCREG() - Could not extract <reject_cause>.\r\n");
                }
            }
        }
    }

    /*
     * In order to show emergency calls only, Android telephony stack expects
     * the registration status value to be one of 10,12,13,14. Add 10 to the
     * reported registration status in which emergency calls are possible.
     */
    if (E_REGISTRATION_DENIED == uiStatus)
    {
        uiStatus += 10;
    }

    snprintf(rCSRegStatusInfo.szStat,        REG_STATUS_LENGTH, "%u", uiStatus);
    snprintf(rCSRegStatusInfo.szNetworkType, REG_STATUS_LENGTH, "%d", (int)rtAct);
    /*
     * With respect to android telephony framework, LAC and CID should be -1 if unknown or it
     * should be of length 0.
     */
    (uiLAC == 0) ? rCSRegStatusInfo.szLAC[0] = '\0' :
                    snprintf(rCSRegStatusInfo.szLAC, REG_STATUS_LENGTH, "%x", uiLAC);

    (uiLAC == 0) ? rCSRegStatusInfo.szCID[0] = '\0' :
                    snprintf(rCSRegStatusInfo.szCID, REG_STATUS_LENGTH, "%x", uiCID);
    (uiRejectCause == 0) ? rCSRegStatusInfo.szReasonDenied[0] = '\0' :
            snprintf(rCSRegStatusInfo.szReasonDenied, REG_STATUS_LENGTH, "%u", uiRejectCause);

    bRet = TRUE;
Error:
    if (!bUnSolicited)
    {
        // Skip "<postfix>"
        if (!FindAndSkipRspEnd(rszPointer, szNewLine, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCREG() - Could not skip response postfix.\r\n");
        }
    }

    RIL_LOG_VERBOSE("CTE::ParseCREG() - Exit\r\n");
    return bRet;
}

BOOL CTE::ParseCGREG(const char*& rszPointer, const BOOL bUnSolicited,
                               S_ND_GPRS_REG_STATUS& rPSRegStatusInfo)
{
    RIL_LOG_VERBOSE("CTE::ParseCGREG() - Enter\r\n");

    UINT32 uiNum;
    UINT32 uiStatus = 0;
    UINT32 uiLAC = 0;
    UINT32 uiCID = 0;
    UINT32 uiAct = 0;
    UINT32 uiCauseType = 0;
    UINT32 uiRejectCause = 0;
    RIL_RadioTechnology rtAct = RADIO_TECH_UNKNOWN;
    UINT32 uiRAC = 0;
    BOOL bRet = false;
    char szNewLine[3] = "\r\n";

    if (!bUnSolicited)
    {
        // Skip "<prefix>"
        if (!SkipRspStart(rszPointer, szNewLine, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCGREG() - Could not skip response prefix.\r\n");
            goto Error;
        }

        // Skip "<,prefix> string"
        if (!SkipString(rszPointer, "+CGREG: ", rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCGREG() - Could not skip \"+CGREG: \".\r\n");
            goto Error;
        }

        // Extract <n> and throw away
        if (!ExtractUInt32(rszPointer, uiNum, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCGREG() - Could not extract <n>.\r\n");
            goto Error;
        }

        // Skip ","
        if (!SkipString(rszPointer, ",", rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCGREG() - Could not extract <stat>.\r\n");
            goto Error;
        }
    }

    // "<stat>"
    if (!ExtractUInt32(rszPointer, uiStatus, rszPointer))
    {
        RIL_LOG_CRITICAL("CTE::ParseCGREG() - Could not extract <stat>.\r\n");
        goto Error;
    }

    // Do we have more to parse?
    if (SkipString(rszPointer, ",", rszPointer))
    {
        // Extract "<lac>"
        if (SkipString(rszPointer, "\"", rszPointer))
        {
            if (!ExtractHexUInt32(rszPointer, uiLAC, rszPointer))
            {
                RIL_LOG_INFO("CTE::ParseCGREG() - Could not extract <lac>.\r\n");
            }
            SkipString(rszPointer, "\"", rszPointer);
        }

        // Extract ",<cid>"
        if (SkipString(rszPointer, ",", rszPointer))
        {
            if (SkipString(rszPointer, "\"", rszPointer))
            {
                if (!ExtractHexUInt32(rszPointer, uiCID, rszPointer))
                {
                    RIL_LOG_INFO("CTE::ParseCGREG() - Could not extract <cid>.\r\n");
                }
                SkipString(rszPointer, "\"", rszPointer);
            }
        }

        // Extract ",<Act>"
        if (SkipString(rszPointer, ",", rszPointer))
        {
            if (!ExtractUInt32(rszPointer, uiAct, rszPointer))
            {
                RIL_LOG_INFO("CTE::ParseCGREG() - Could not extract <act>.\r\n");
            }

            /*
             * Maps the 3GPP standard access technology values to android specific access
             * technology values.
             */
            rtAct = MapAccessTechnology(uiAct);
        }

        // Extract ",<rac>"
        if (SkipString(rszPointer, ",", rszPointer))
        {
            if (SkipString(rszPointer, "\"", rszPointer))
            {
                if (!ExtractHexUInt32(rszPointer, uiRAC, rszPointer))
                {
                    RIL_LOG_INFO("CTE::ParseCGREG() - Could not extract \",<rac>\".\r\n");
                }
                SkipString(rszPointer, "\"", rszPointer);
            }
        }

        // Extract ",cause_type and reject_cause" only if registration status is denied
        if (E_REGISTRATION_DENIED == uiStatus)
        {
            if (SkipString(rszPointer, ",", rszPointer))
            {
                if (!ExtractUInt32(rszPointer, uiCauseType, rszPointer))
                {
                    RIL_LOG_CRITICAL("CTE::ParseCGREG() - "
                            "Could not extract <cause_type>.\r\n");
                }

                if (!SkipString(rszPointer, ",", rszPointer)
                        || !ExtractUInt32(rszPointer, uiRejectCause, rszPointer))
                {
                    RIL_LOG_CRITICAL("CTE::ParseCGREG() - "
                            "Could not extract <reject_cause>.\r\n");
                }
            }
        }
    }

    snprintf(rPSRegStatusInfo.szStat, REG_STATUS_LENGTH, "%u", uiStatus);
    snprintf(rPSRegStatusInfo.szNetworkType, REG_STATUS_LENGTH, "%d", (int)rtAct);
    /*
     * With respect to android telephony framework, LAC and CID should be -1 if unknown or it
     * should be of length 0.
     */
    (uiLAC == 0) ? rPSRegStatusInfo.szLAC[0] = '\0' :
                    snprintf(rPSRegStatusInfo.szLAC, REG_STATUS_LENGTH, "%x", uiLAC);

    (uiLAC == 0) ? rPSRegStatusInfo.szCID[0] = '\0' :
                    snprintf(rPSRegStatusInfo.szCID, REG_STATUS_LENGTH, "%x", uiCID);

    (uiRejectCause == 0) ? rPSRegStatusInfo.szReasonDenied[0] = '\0' :
            snprintf(rPSRegStatusInfo.szReasonDenied, REG_STATUS_LENGTH, "%u", uiRejectCause);

    bRet = TRUE;
Error:
    if (!bUnSolicited)
    {
        // Skip "<postfix>"
        if (!FindAndSkipRspEnd(rszPointer, szNewLine, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCGREG() - Could not skip response postfix.\r\n");
        }
    }

    RIL_LOG_VERBOSE("CTE::ParseCGREG() - Exit\r\n");
    return bRet;
}

BOOL CTE::ParseXREG(const char*& rszPointer, const BOOL bUnSolicited,
                              S_ND_GPRS_REG_STATUS& rPSRegStatusInfo)
{
    RIL_LOG_VERBOSE("CTE::ParseXREG() - Enter\r\n");

    UINT32 uiNum;
    UINT32 uiStatus = 0;
    UINT32 uiLAC = 0;
    UINT32 uiCID = 0;
    UINT32 uiAct = 0;
    char szBand[MAX_BUFFER_SIZE] = {0};
    UINT32 uiRAC = 0;
    UINT32 uiCauseType = 0;
    UINT32 uiRejectCause = 0;
    BOOL bRet = false;
    char szNewLine[3] = "\r\n";

    if (!bUnSolicited)
    {
        // Skip "<prefix>"
        if (!SkipRspStart(rszPointer, szNewLine, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseXREG() - Could not skip response prefix.\r\n");
            goto Error;
        }

        // Skip "<,prefix> string"
        if (!SkipString(rszPointer, "+XREG: ", rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseXREG() - Could not skip \"+XREG: \".\r\n");
            goto Error;
        }

        // Extract <n> and throw away
        if (!ExtractUInt32(rszPointer, uiNum, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseXREG() - Could not extract <n>.\r\n");
            goto Error;
        }

        // Skip ","
        if (!SkipString(rszPointer, ",", rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseXREG() - Could not extract <stat>.\r\n");
            goto Error;
        }
    }

    // "<stat>"
    if (!ExtractUInt32(rszPointer, uiStatus, rszPointer))
    {
        RIL_LOG_CRITICAL("CTE::ParseXREG() - Could not extract <stat>.\r\n");
        goto Error;
    }

    if (E_REGISTRATION_NOT_REGISTERED_NOT_SEARCHING == uiStatus
        || E_REGISTRATION_NOT_REGISTERED_SEARCHING == uiStatus
        || E_REGISTRATION_UNKNOWN == uiStatus)
    {
        goto Done;
    }

    //  Parse <AcT>
    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractUInt32(rszPointer, uiAct, rszPointer))
    {
        RIL_LOG_INFO("CTE::ParseXREG() - Could not extract <AcT>\r\n");
    }

    /*
     * Maps the 3GPP standard access technology values to android specific access
     * technology values.
     */
    uiAct = CTE::MapAccessTechnology(uiAct);

    //  Extract <Band> and throw away
    if (!SkipString(rszPointer, ",", rszPointer) ||
            !ExtractUnquotedString(rszPointer, ",", szBand, MAX_BUFFER_SIZE, rszPointer))
    {
        RIL_LOG_INFO("CTE::ParseXREG() - Could not extract <Band>\r\n");
    }

    // Do we have more to parse?
    if (SkipString(rszPointer, ",", rszPointer))
    {
        // Extract "<lac>"
        if (SkipString(rszPointer, "\"", rszPointer))
        {
            if (!ExtractHexUInt32(rszPointer, uiLAC, rszPointer))
            {
                RIL_LOG_INFO("CTE::ParseXREG() - Could not extract <lac>.\r\n");
            }
            SkipString(rszPointer, "\"", rszPointer);
        }

        // Extract ",<cid>"
        if (SkipString(rszPointer, ",", rszPointer))
        {
            if (SkipString(rszPointer, "\"", rszPointer))
            {
                if (!ExtractHexUInt32(rszPointer, uiCID, rszPointer))
                {
                    RIL_LOG_INFO("CTE::ParseXREG() - Could not extract <cid>.\r\n");
                }
                SkipString(rszPointer, "\"", rszPointer);
            }
        }

        // Extract ",<rac>"
        if (SkipString(rszPointer, ",", rszPointer))
        {
            // Currently, <rac> is an integer type but may be a string type (hexidecimal)
            // for future releases.
            if (SkipString(rszPointer, "\"", rszPointer))
            {
                if (!ExtractHexUInt32(rszPointer, uiRAC, rszPointer))
                {
                    RIL_LOG_INFO("CTE::ParseXREG() - Could not extract <rac> (hexadecimal).\r\n");
                }
                SkipString(rszPointer, "\"", rszPointer);
            }
            else if (!ExtractUInt32(rszPointer, uiRAC, rszPointer))
            {
                RIL_LOG_INFO("CTE::ParseXREG() - Could not extract <rac>.\r\n");
            }
        }

        // Extract ",cause_type and reject_cause" only if registration status is denied
        if (E_REGISTRATION_DENIED == uiStatus)
        {
            if (SkipString(rszPointer, ",", rszPointer))
            {
                if (!ExtractUInt32(rszPointer, uiCauseType, rszPointer))
                {
                    RIL_LOG_CRITICAL("CTE::ParseXREG() - "
                            "Could not extract <cause_type>.\r\n");
                }

                if (!SkipString(rszPointer, ",", rszPointer)
                        || !ExtractUInt32(rszPointer, uiRejectCause, rszPointer))
                {
                    RIL_LOG_CRITICAL("CTE::ParseXREG() - "
                            "Could not extract <reject_cause>.\r\n");
                }
            }
        }
    }

Done:
    snprintf(rPSRegStatusInfo.szStat, REG_STATUS_LENGTH, "%u", uiStatus);
    snprintf(rPSRegStatusInfo.szNetworkType, REG_STATUS_LENGTH, "%d", (int)uiAct);
    /*
     * With respect to android telephony framework, LAC and CID should be -1 if unknown or it
     * should be of length 0.
     */
    (uiLAC == 0) ? rPSRegStatusInfo.szLAC[0] = '\0' :
                    snprintf(rPSRegStatusInfo.szLAC, REG_STATUS_LENGTH, "%x", uiLAC);

    (uiLAC == 0) ? rPSRegStatusInfo.szCID[0] = '\0' :
                    snprintf(rPSRegStatusInfo.szCID, REG_STATUS_LENGTH, "%x", uiCID);

    (uiRejectCause == 0) ? rPSRegStatusInfo.szReasonDenied[0] = '\0' :
            snprintf(rPSRegStatusInfo.szReasonDenied, REG_STATUS_LENGTH, "%u", uiRejectCause);

    bRet = TRUE;
Error:
    if (!bUnSolicited)
    {
        // Skip "<postfix>"
        if (!FindAndSkipRspEnd(rszPointer, szNewLine, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseXREG() - Could not skip response postfix.\r\n");
        }
    }

    RIL_LOG_VERBOSE("CTE::ParseXREG() - Exit\r\n");
    return bRet;
}

BOOL CTE::ParseCEREG(const char*& rszPointer, const BOOL bUnSolicited,
                              S_ND_GPRS_REG_STATUS& rPSRegStatusInfo)
{
    RIL_LOG_VERBOSE("CTE::ParseCEREG() - Enter\r\n");

    UINT32 uiNum;
    UINT32 uiStatus = 0;
    UINT32 uiTac = 0;
    UINT32 uiCid = 0;
    UINT32 uiAct = 0;
    BOOL bRet = false;
    char szNewLine[3] = "\r\n";
    UINT32 uiCauseType = 0;
    UINT32 uiRejectCause = 0;

    if (!bUnSolicited)
    {
        // Skip "<prefix>"
        if (!SkipRspStart(rszPointer, szNewLine, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCEREG() - Could not skip response prefix.\r\n");
            goto Error;
        }

        // Skip "<,prefix> string"
        if (!SkipString(rszPointer, "+CEREG: ", rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCEREG() - Could not skip \"+CEREG: \".\r\n");
            goto Error;
        }

        // Extract <n> and throw away
        if (!ExtractUInt32(rszPointer, uiNum, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCEREG() - Could not extract <n>.\r\n");
            goto Error;
        }

        // Skip ","
        if (!SkipString(rszPointer, ",", rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCEREG() - Could not extract <stat>.\r\n");
            goto Error;
        }
    }

    // "<stat>"
    if (!ExtractUInt32(rszPointer, uiStatus, rszPointer))
    {
        RIL_LOG_CRITICAL("CTE::ParseCEREG() - Could not extract <stat>.\r\n");
        goto Error;
    }

    // Do we have more to parse?
    if (SkipString(rszPointer, ",", rszPointer))
    {
        //  Parse <tac>
        if (SkipString(rszPointer, "\"", rszPointer))
        {
            if (!ExtractHexUInt32(rszPointer, uiTac, rszPointer))
            {
                RIL_LOG_INFO("CTE::ParseCEREG() - Could not extract <tac>\r\n");
            }
            SkipString(rszPointer, "\"", rszPointer);
        }

        // Extract ",<cid>"
        if (SkipString(rszPointer, ",", rszPointer))
        {
            if (SkipString(rszPointer, "\"", rszPointer))
            {
                if (!ExtractHexUInt32(rszPointer, uiCid, rszPointer))
                {
                    RIL_LOG_INFO("CTE::ParseCEREG() - Could not extract <cid>.\r\n");
                }
                SkipString(rszPointer, "\"", rszPointer);
            }
        }
    }

    // Do we have more to parse?
    if (SkipString(rszPointer, ",", rszPointer))
    {
        //  Parse <AcT>
        if (!ExtractUInt32(rszPointer, uiAct, rszPointer))
        {
            RIL_LOG_INFO("CTE::ParseCEREG() - Cound not extract <AcT>\r\n");
        }
        /*
        * Maps the 3GPP standard access technology values to android specific access
        * technology values.
        */
        uiAct = CTE::MapAccessTechnology(uiAct);
    }

    // Extract ",cause_type and reject_cause" only if registration status is denied
    if (E_REGISTRATION_DENIED == uiStatus)
    {
        // Extract ",cause_type and reject_cause"
        if (SkipString(rszPointer, ",", rszPointer))
        {
            if (!ExtractUInt32(rszPointer, uiCauseType, rszPointer))
            {
                RIL_LOG_CRITICAL("CTE::ParseCEREG() - Could not extract <cause_type>.\r\n");
                goto Error;
            }

            if (!SkipString(rszPointer, ",", rszPointer)
                    || !ExtractUInt32(rszPointer, uiRejectCause, rszPointer))
            {
                RIL_LOG_CRITICAL("CTE::ParseCEREG() - Could not extract <reject_cause>.\r\n");
                goto Error;
            }
        }
    }

    snprintf(rPSRegStatusInfo.szStat, REG_STATUS_LENGTH, "%u", uiStatus);
    snprintf(rPSRegStatusInfo.szNetworkType, REG_STATUS_LENGTH, "%d", (int)uiAct);

    (uiTac == 0) ? rPSRegStatusInfo.szLAC[0] = '\0' :
                    snprintf(rPSRegStatusInfo.szLAC, REG_STATUS_LENGTH, "%x", uiTac);

    (uiTac == 0) ? rPSRegStatusInfo.szCID[0] = '\0' :
                    snprintf(rPSRegStatusInfo.szCID, REG_STATUS_LENGTH, "%x", uiCid);

    (uiRejectCause == 0) ? rPSRegStatusInfo.szReasonDenied[0] = '\0' :
            snprintf(rPSRegStatusInfo.szReasonDenied, REG_STATUS_LENGTH, "%u", uiRejectCause);

    bRet = TRUE;
Error:
    if (!bUnSolicited)
    {
        // Skip "<postfix>"
        if (!FindAndSkipRspEnd(rszPointer, szNewLine, rszPointer))
        {
            RIL_LOG_CRITICAL("CTE::ParseCEREG() - Could not skip response postfix.\r\n");
        }
    }

    RIL_LOG_VERBOSE("CTE::ParseCEREG() - Exit\r\n");
    return bRet;
}

void CTE::StoreRegistrationInfo(void* pRegStruct, int regType)
{
    RIL_LOG_VERBOSE("CTE::StoreRegistrationInfo() - Enter\r\n");

    /*
     * LAC and CID reported as part of the CS and PS registration status changed URCs
     * are supposed to be the same. But there is nothing wrong in keeping it separately.
     */
    if (E_REGISTRATION_TYPE_CGREG == regType
            || E_REGISTRATION_TYPE_XREG == regType)
    {
        P_ND_GPRS_REG_STATUS psRegStatus = (P_ND_GPRS_REG_STATUS) pRegStruct;

        RIL_LOG_INFO("[RIL STATE] GPRS REG STATUS = %s  RAT = %s\r\n",
                PrintGPRSRegistrationInfo(psRegStatus->szStat),
                PrintRAT(psRegStatus->szNetworkType));

        if (E_REGISTRATION_DENIED == GetPsRegistrationState(psRegStatus->szStat))
        {
            m_bPSStatusCached = FALSE;
        }
        else
        {
            m_bPSStatusCached = TRUE;
        }

        strncpy(m_sPSStatus.szStat, psRegStatus->szStat, sizeof(psRegStatus->szStat));
        strncpy(m_sPSStatus.szLAC, psRegStatus->szLAC, sizeof(psRegStatus->szLAC));
        strncpy(m_sPSStatus.szCID, psRegStatus->szCID, sizeof(psRegStatus->szCID));
        strncpy(m_sPSStatus.szNetworkType, psRegStatus->szNetworkType,
                sizeof(psRegStatus->szNetworkType));
        strncpy(m_sPSStatus.szReasonDenied, psRegStatus->szReasonDenied,
                sizeof(psRegStatus->szReasonDenied));
    }
    else if (E_REGISTRATION_TYPE_CREG == regType)
    {
        P_ND_REG_STATUS csRegStatus = (P_ND_REG_STATUS) pRegStruct;

        RIL_LOG_INFO("[RIL STATE] REG STATUS = %s\r\n",
                PrintRegistrationInfo(csRegStatus->szStat));

        int regDenied = E_REGISTRATION_DENIED + 10;

        if (regDenied == GetCsRegistrationState(csRegStatus->szStat))
        {
            m_bCSStatusCached = FALSE;
        }
        else
        {
            m_bCSStatusCached = TRUE;
        }

        strncpy(m_sCSStatus.szStat, csRegStatus->szStat, sizeof(csRegStatus->szStat));
        strncpy(m_sCSStatus.szLAC, csRegStatus->szLAC, sizeof(csRegStatus->szLAC));
        strncpy(m_sCSStatus.szCID, csRegStatus->szCID, sizeof(csRegStatus->szCID));

        SaveNetworkData(LAST_NETWORK_LAC, csRegStatus->szLAC);
        SaveNetworkData(LAST_NETWORK_CID, csRegStatus->szCID);

        /*
         * 20111025: framework doesn't make use of technology information
         * sent as part of RIL_REQUEST_REGISTRATION_STATE response.
         */
        strncpy(m_sCSStatus.szNetworkType, csRegStatus->szNetworkType,
                sizeof(csRegStatus->szNetworkType));

        strncpy(m_sCSStatus.szReasonDenied, csRegStatus->szReasonDenied,
                sizeof(csRegStatus->szReasonDenied));
    }
    else if (E_REGISTRATION_TYPE_CEREG == regType)
    {
        /*
         * LTE registration status information is mapped to the GPRS registration
         * status information structure. Eventhough the registration status information
         * structure is named as S_ND_GPRS_REG_STATUS, structure is for holding the
         * basic data(either GPRS or LTE) registration status information.
         *
         * Note: Currently, TAC is mapped to LAC.
         */
        P_ND_GPRS_REG_STATUS epsRegStatus = (P_ND_GPRS_REG_STATUS) pRegStruct;

        RIL_LOG_INFO("[RIL STATE] EPS REG STATUS = %s  RAT = %s\r\n",
                PrintGPRSRegistrationInfo(epsRegStatus->szStat),
                PrintRAT(epsRegStatus->szNetworkType));

        if (E_REGISTRATION_DENIED == GetPsRegistrationState(epsRegStatus->szStat))
        {
            m_bPSStatusCached = FALSE;
        }
        else
        {
            m_bPSStatusCached = TRUE;
        }

        strncpy(m_sEPSStatus.szStat, epsRegStatus->szStat, sizeof(epsRegStatus->szStat));
        strncpy(m_sEPSStatus.szLAC, epsRegStatus->szLAC, sizeof(epsRegStatus->szLAC));
        strncpy(m_sEPSStatus.szCID, epsRegStatus->szCID, sizeof(epsRegStatus->szCID));
        strncpy(m_sEPSStatus.szNetworkType, epsRegStatus->szNetworkType,
                sizeof(epsRegStatus->szNetworkType));
        strncpy(m_sEPSStatus.szReasonDenied, epsRegStatus->szReasonDenied,
                sizeof(epsRegStatus->szReasonDenied));
    }

    // If cell info rate is 0 and Cell info is enabled, query cell info
    if (IsCellInfoEnabled())
    {
        UINT32 uiNewRate = GetCellInfoListRate();
        if (!IsCellInfoTimerRunning() && (uiNewRate == 0))
        {
            RIL_LOG_INFO("CTEBase::StoreRegistrationInfo() - read cell info now!\r\n");
            SetCellInfoTimerRunning(TRUE);
            RIL_requestTimedCallback(triggerCellInfoList, (void*)uiNewRate, 0, 0);
        }
    }

    RIL_LOG_VERBOSE("CTE::StoreRegistrationInfo() - Exit\r\n");
}

void CTE::CopyCachedRegistrationInfo(void* pRegStruct, BOOL bPSStatus)
{
    RIL_LOG_VERBOSE("CTE::CopyCachedRegistrationInfo() - Enter\r\n");

    if (bPSStatus)
    {
        int currentAct = GetCurrentAct();
        P_ND_GPRS_REG_STATUS psRegStatus = (P_ND_GPRS_REG_STATUS) pRegStruct;

        memset(psRegStatus, 0, sizeof(S_ND_GPRS_REG_STATUS));

        /*
         * Copy the cached EPS registration status only if the device is EPS registered,
         * current access technology is LTE and default PDN context parameters are read.
         *
         * Note: When the device is EPS registered but if the current access technology is
         * not known, then EPS registration status will be updated as the data registration
         * state to the telephony framework. This is possible when the device is in screen
         * off state.
         */
        if (IsEPSRegistered() && (RADIO_TECH_LTE == currentAct
                || RADIO_TECH_UNKNOWN == currentAct))
        {
            /*
             * Report the EPS registration status only after the default PDN context
             * parameters are read(i.e. Default PDN context is in ACTIVATING/ACTIVE state).
             * If EPS registration status is reported earlier, android telephony framework
             * will trigger the SETUP_DATA_CALL request resulting in establishing a second
             * pdp context even when the requested SETUP_DATA_CALL is for the same APN as
             * default PDN APN.
             */
            CChannel_Data* pChannelData =
                    CChannel_Data::GetChnlFromContextID(m_uiDefaultPDNCid);
            if (NULL != pChannelData)
            {
                int dataState = pChannelData->GetDataState();
                if (E_DATA_STATE_ACTIVATING == dataState
                        || E_DATA_STATE_ACTIVE == dataState)
                {
                    RIL_LOG_VERBOSE("CTE::CopyCachedRegistrationInfo() - Default PDN ready\r\n");
                    strncpy(psRegStatus->szStat, m_sEPSStatus.szStat, sizeof(psRegStatus->szStat));
                    // TAC is mapped to LAC
                    strncpy(psRegStatus->szLAC, m_sEPSStatus.szLAC, sizeof(psRegStatus->szLAC));
                    strncpy(psRegStatus->szCID, m_sEPSStatus.szCID, sizeof(psRegStatus->szCID));
                    strncpy(psRegStatus->szNetworkType, m_sEPSStatus.szNetworkType,
                            sizeof(psRegStatus->szNetworkType));
                    strncpy(psRegStatus->szReasonDenied, m_sEPSStatus.szReasonDenied,
                            sizeof(psRegStatus->szReasonDenied));
                }
            }
        }
        else
        {
            RIL_LOG_VERBOSE("CTE::CopyCachedRegistrationInfo() - not on LTE\r\n");
            strncpy(psRegStatus->szStat, m_sPSStatus.szStat, sizeof(psRegStatus->szStat));
            strncpy(psRegStatus->szLAC, m_sPSStatus.szLAC, sizeof(psRegStatus->szLAC));
            strncpy(psRegStatus->szCID, m_sPSStatus.szCID, sizeof(psRegStatus->szCID));
            strncpy(psRegStatus->szNetworkType, m_sPSStatus.szNetworkType,
                    sizeof(psRegStatus->szNetworkType));
            strncpy(psRegStatus->szReasonDenied, m_sPSStatus.szReasonDenied,
                    sizeof(psRegStatus->szReasonDenied));
        }

        //  Ice Cream Sandwich has new field ((const char **)response)[5] which is
        //  the maximum number of simultaneous data calls.
        snprintf(psRegStatus->szNumDataCalls, REG_STATUS_LENGTH, "%d",
                (g_uiRilChannelCurMax - RIL_CHANNEL_DATA1));

        psRegStatus->sStatusPointers.pszStat = psRegStatus->szStat;
        psRegStatus->sStatusPointers.pszLAC = psRegStatus->szLAC;
        psRegStatus->sStatusPointers.pszCID = psRegStatus->szCID;
        psRegStatus->sStatusPointers.pszNetworkType = psRegStatus->szNetworkType;
        psRegStatus->sStatusPointers.pszNumDataCalls = psRegStatus->szNumDataCalls; // ICS new field
        psRegStatus->sStatusPointers.pszReasonDenied = psRegStatus->szReasonDenied;
    }
    else
    {
        P_ND_REG_STATUS csRegStatus = (P_ND_REG_STATUS) pRegStruct;

        memset(csRegStatus, 0, sizeof(S_ND_REG_STATUS));

        if (E_REGISTRATION_EMERGENCY_SERVICES_ONLY == GetCsRegistrationState(m_sCSStatus.szStat))
        {
            // Android do not manage the new value state +CREG: 8, so we use the
            // case 10 (0+10) which means no network but emergency call possible
            snprintf(csRegStatus->szStat, REG_STATUS_LENGTH, "%u", 10);
        }
        else
        {
            strncpy(csRegStatus->szStat, m_sCSStatus.szStat, sizeof(csRegStatus->szStat));
        }

        strncpy(csRegStatus->szLAC, m_sCSStatus.szLAC, sizeof(csRegStatus->szLAC));
        strncpy(csRegStatus->szCID, m_sCSStatus.szCID, sizeof(csRegStatus->szCID));
        // Always copy the access technology received as part of XREG URC as it reports
        // access technology also during call.
        strncpy(csRegStatus->szNetworkType, m_sPSStatus.szNetworkType,
                sizeof(csRegStatus->szNetworkType));
        strncpy(csRegStatus->szReasonDenied, m_sCSStatus.szReasonDenied,
                sizeof(csRegStatus->szReasonDenied));

        csRegStatus->sStatusPointers.pszStat = csRegStatus->szStat;
        csRegStatus->sStatusPointers.pszLAC = csRegStatus->szLAC;
        csRegStatus->sStatusPointers.pszCID = csRegStatus->szCID;
        csRegStatus->sStatusPointers.pszNetworkType = csRegStatus->szNetworkType;
        csRegStatus->sStatusPointers.pszReasonDenied = csRegStatus->szReasonDenied;
        // Note that the remaining fields in the structure have been previously set to NULL (0)
        // by memset().  They are not used in this RIL.
    }

    RIL_LOG_VERBOSE("CTE::CopyCachedRegistrationInfo() - Exit\r\n");
}

void CTE::ResetRegistrationCache()
{
    m_bCSStatusCached = FALSE;
    m_bPSStatusCached = FALSE;
}

BOOL CTE::IsRegistered()
{
    BOOL bRet = FALSE;
    LONG csRegState = strtol(m_sCSStatus.szStat, NULL, 10);
    LONG psRegState = strtol(m_sPSStatus.szStat, NULL, 10);
    LONG epsRegState = strtol(m_sEPSStatus.szStat, NULL, 10);

    if (E_REGISTRATION_REGISTERED_HOME_NETWORK == csRegState
            || E_REGISTRATION_REGISTERED_ROAMING == csRegState
            || E_REGISTRATION_REGISTERED_HOME_NETWORK == psRegState
            || E_REGISTRATION_REGISTERED_ROAMING == psRegState
            || E_REGISTRATION_REGISTERED_HOME_NETWORK == epsRegState
            || E_REGISTRATION_REGISTERED_ROAMING == epsRegState)
    {
        bRet = TRUE;
    }

    return bRet;
}

LONG CTE::GetCsRegistrationState(char* pCsRegState)
{
    return strtol(pCsRegState, NULL, 10);
}

LONG CTE::GetPsRegistrationState(char* pPsRegState)
{
    return strtol(pPsRegState, NULL, 10);
}

BOOL CTE::IsEPSRegistered()
{
    BOOL bRet = FALSE;
    LONG regState = strtol(m_sEPSStatus.szStat, NULL, 10);

    if (E_REGISTRATION_REGISTERED_HOME_NETWORK == regState
            || E_REGISTRATION_REGISTERED_ROAMING == regState)
    {
        bRet = TRUE;
    }

    return bRet;
}

LONG CTE::GetCurrentAct()
{
    return strtol(m_sPSStatus.szNetworkType, NULL, 10);
}

const char* CTE::PrintRegistrationInfo(char* szRegInfo) const
{
    int nRegInfo = atoi(szRegInfo);

    switch (nRegInfo)
    {
        case E_REGISTRATION_NOT_REGISTERED_NOT_SEARCHING:
            return "NOT REGISTERED, NOT SEARCHING";
        case E_REGISTRATION_REGISTERED_HOME_NETWORK:
            return "REGISTERED, HOME NETWORK";
        case E_REGISTRATION_NOT_REGISTERED_SEARCHING:
            return "NOT REGISTERED, SEARCHING";
        case E_REGISTRATION_DENIED:
        case E_REGISTRATION_DENIED + 10: // Android specific emergency possible
            return "REGISTRATION DENIED";
        case E_REGISTRATION_UNKNOWN:
            return "UNKNOWN";
        case E_REGISTRATION_REGISTERED_ROAMING:
            return "REGISTERED, IN ROAMING";
        // applicable only when <AcT> indicates E-UTRAN
        case E_REGISTRATION_REGISTERED_FOR_SMS_ONLY_HOME_NETWORK:
            return "REGISTERED FOR SMS ONLY, HOME NETWORK";
        // applicable only when <AcT> indicates E-UTRAN
        case E_REGISTRATION_REGISTERED_FOR_SMS_ONLY_ROAMING:
            return "REGISTERED FOR SMS ONLY, IN ROAMING";
        // Used by IMC modems to indicate emergency call possible in CS network
        case E_REGISTRATION_EMERGENCY_SERVICES_ONLY:
            return "EMERGENCY SERVICE ONLY";
        // applicable only when <AcT> indicates E-UTRAN
        case E_REGISTRATION_REGISTERED_FOR_CSFB_NP_HOME_NETWORK:
            return "REGISTERED FOR CSFB NOT PREFERRED, HOME NETWORK";
        // applicable only when <AcT> indicates E-UTRAN
        case E_REGISTRATION_REGISTERED_FOR_CSFB_NP_ROAMING:
            return "REGISTERED FOR CSFB NOT PREFERRED, IN ROAMING";
        default:
            return "UNKNOWN REG STATUS";
    }
}

const char* CTE::PrintGPRSRegistrationInfo(char* szGPRSInfo) const
{
    int nGPRSInfo = atoi(szGPRSInfo);

    switch (nGPRSInfo)
    {
        case E_REGISTRATION_NOT_REGISTERED_NOT_SEARCHING:
            return "NOT REGISTERED, HOME NETWORK";
        case E_REGISTRATION_REGISTERED_HOME_NETWORK:
            return "REGISTERED, HOME NETWORK";
        case E_REGISTRATION_NOT_REGISTERED_SEARCHING:
            return "NOT REGISTERED, SEARCHING";
        case E_REGISTRATION_DENIED:
            return "REGISTRATION DENIED";
        case E_REGISTRATION_UNKNOWN:
            return "UNKNOWN";
        case E_REGISTRATION_REGISTERED_ROAMING:
            return "REGISTERED, IN ROAMING";
        case E_REGISTRATION_EMERGENCY_SERVICES_ONLY:
            return "ATTACHED FOR EMERGENCY BEARER SERVICES";
        default: return "UNKNOWN REG STATUS";
    }
}

const char* CTE::PrintRAT(char* szRAT) const
{
    int nRAT = atoi(szRAT);

    switch(nRAT)
    {
        case RADIO_TECH_UNKNOWN: return "RADIO_TECH_UNKNOWN";
        case RADIO_TECH_GPRS: return "RADIO_TECH_GPRS";
        case RADIO_TECH_EDGE: return "RADIO_TECH_EDGE";
        case RADIO_TECH_UMTS: return "RADIO_TECH_UMTS";
        case RADIO_TECH_IS95A: return "RADIO_TECH_IS95A";
        case RADIO_TECH_IS95B: return "RADIO_TECH_IS95B";
        case RADIO_TECH_1xRTT: return "RADIO_TECH_1xRTT";
        case RADIO_TECH_EVDO_0: return "RADIO_TECH_EVDO_0";
        case RADIO_TECH_EVDO_A: return "RADIO_TECH_EVDO_A";
        case RADIO_TECH_HSDPA: return "RADIO_TECH_HSDPA";
        case RADIO_TECH_HSUPA: return "RADIO_TECH_HSUPA";
        case RADIO_TECH_HSPA: return "RADIO_TECH_HSPA";
        case RADIO_TECH_EVDO_B: return "RADIO_TECH_EVDO_B";
        case RADIO_TECH_EHRPD: return "RADIO_TECH_EHRPD";
        case RADIO_TECH_LTE: return "RADIO_TECH_LTE";
        case RADIO_TECH_HSPAP: return "RADIO_TECH_HSPAP";
        default: return "UNKNOWN RAT";
    }
}


//
// QUERY_SIM_SMS_STORE_STATUS (sent internally)
//
RIL_RESULT_CODE CTE::ParseQuerySimSmsStoreStatus(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQuerySimSmsStoreStatus() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseQuerySimSmsStoreStatus(rRspData);
}

RIL_RESULT_CODE CTE::ParsePdpContextActivate(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParsePdpContextActivate() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParsePdpContextActivate(rRspData);
}

RIL_RESULT_CODE CTE::ParseQueryIpAndDns(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryIpAndDns() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseQueryIpAndDns(rRspData);
}

RIL_RESULT_CODE CTE::ParseEnterDataState(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseEnterDataState() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseEnterDataState(rRspData);
}

void CTE::SetIncomingCallStatus(UINT32 uiCallId, UINT32 uiStatus)
{
    RIL_LOG_VERBOSE("CTE::SetIncomingCallStatus() - Enter / Exit\r\n");

    m_pTEBaseInstance->SetIncomingCallStatus(uiCallId, uiStatus);
}

UINT32 CTE::GetIncomingCallId()
{
    RIL_LOG_VERBOSE("CTE::GetIncomingCallId() - Enter / Exit\r\n");

    return m_pTEBaseInstance->GetIncomingCallId();
}

void CTE::SetupDataCallOngoing(BOOL bStatus)
{
    RIL_LOG_VERBOSE("CTE::SetupDataCallOngoing() - Enter / Exit\r\n");
    m_bIsSetupDataCallOngoing = bStatus;
}

BOOL CTE::IsSetupDataCallOnGoing()
{
    RIL_LOG_VERBOSE("CTE::IsSetupDataCallOnGoing() - Enter / Exit\r\n");
    return m_bIsSetupDataCallOngoing;
}

BOOL CTE::IsLocationUpdatesEnabled()
{
    RIL_LOG_VERBOSE("CTE::IsLocationUpdatesEnabled() - Enter / Exit\r\n");
    return (1 == m_enableLocationUpdates);
}

RIL_RadioState CTE::GetRadioState()
{
    return m_pTEBaseInstance->GetRadioState();
}

RRIL_SIM_State CTE::GetSIMState()
{
    return m_pTEBaseInstance->GetSIMState();
}

void CTE::SetRadioState(const RRIL_Radio_State eRadioState)
{
    m_pTEBaseInstance->SetRadioState(eRadioState);
}

void CTE::SetRadioStateAndNotify(const RRIL_Radio_State eRadioState)
{
    m_pTEBaseInstance->SetRadioStateAndNotify(eRadioState);
}

void CTE::SetSIMState(const RRIL_SIM_State eRadioState)
{
    m_pTEBaseInstance->SetSIMState(eRadioState);
}

void CTE::ResetInternalStates()
{
    RIL_LOG_VERBOSE("CTE::ResetInternalStates() - Enter / Exit\r\n");
    m_bCSStatusCached = FALSE;
    m_bPSStatusCached = FALSE;
    m_bIsSetupDataCallOngoing = FALSE;
    m_bIsSimError = FALSE;
    m_bIsManualNetworkSearchOn = FALSE;
    m_bIsClearPendingCHLD = FALSE;
    m_bIsDataSuspended = FALSE;
    m_bRadioRequestPending = FALSE;
}

BOOL CTE::IsSetupDataCallAllowed(int& retryTime)
{
    BOOL bAllowed = TRUE;

    if (IsManualNetworkSearchOn())
    {
        bAllowed = FALSE;
        retryTime = 10000; // 10seconds
    }
    else if (IsDataSuspended())
    {
        bAllowed = FALSE;
        retryTime = 3000; // 3seconds
    }

    return bAllowed;
}

void CTE::SetDtmfState(UINT32 uiDtmfState)
{
    m_uiDtmfState = uiDtmfState;
}

UINT32 CTE::TestAndSetDtmfState(UINT32 uiDtmfState)
{
    CMutex::Lock(m_pDtmfStateAccess);
    UINT32 uiPrevDtmfState = m_uiDtmfState;

    if (m_uiDtmfState != uiDtmfState)
        m_uiDtmfState = uiDtmfState;

    CMutex::Unlock(m_pDtmfStateAccess);
    return uiPrevDtmfState;
}

UINT32 CTE::GetDtmfState()
{
    return m_uiDtmfState;
}

BOOL CTE::IsInternalRequestsAllowedInRadioOff(int requestId)
{
    RIL_LOG_VERBOSE("CTE::IsInternalRequestsAllowedInRadioOff() - Enter\r\n");

    BOOL bAllowed;

    switch (requestId)
    {
        case E_REQ_ID_INTERNAL_SILENT_PIN_ENTRY:
            if (RRIL_SIM_STATE_NOT_READY == (int) GetSIMState())
                bAllowed = TRUE;
            else
                bAllowed = FALSE;
            break;
        case E_REQ_ID_INTERNAL_QUERY_SIM_SMS_STORE_STATUS:
            if (RRIL_SIM_STATE_READY == (int) GetSIMState())
                bAllowed = TRUE;
            else
                bAllowed = FALSE;
            break;

        default:
            bAllowed = FALSE;
    }

    RIL_LOG_VERBOSE("CTE::IsInternalRequestsAllowedInRadioOff() - Exit\r\n");
    return bAllowed;
}


BOOL CTE::IsRequestAllowed(int requestId, RIL_Token rilToken, UINT32 uiChannelId,
        BOOL bIsInitCommand, int callId)
{
    RIL_Errno eRetVal = RIL_E_SUCCESS;
    BOOL bIsReqAllowed;

    //  If we're in the middle of TriggerRadioError(), spoof all commands.
    if (E_MMGR_EVENT_MODEM_UP != GetLastModemEvent())
    {
        bIsReqAllowed = FALSE;
        eRetVal = RIL_E_RADIO_NOT_AVAILABLE;
    }
    else if (GetSpoofCommandsStatus() && !IsRequestAllowedInSpoofState(requestId))
    {
        eRetVal = HandleRequestWhenNoModem(requestId, rilToken);
        bIsReqAllowed = FALSE;
    }
    else if (RADIO_STATE_OFF == GetRadioState()
            && !IsRequestAllowedInRadioOff(requestId)
            && !bIsInitCommand
            && !IsInternalRequestsAllowedInRadioOff(requestId))
    {
        eRetVal = HandleRequestInRadioOff(requestId, rilToken);
        bIsReqAllowed = FALSE;
    }
    else if (IsPlatformShutDownRequested())
    {
        /*
         * If there is data or voice call, then the framwork will
         * first send requests to hangup those before sending
         * radio power off request. So, allow only requests which
         * are sent to hangup the voice/data call.
         */
        if (RIL_REQUEST_RADIO_POWER == requestId
                || RIL_REQUEST_HANGUP == requestId
                || RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND == requestId
                || RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND == requestId
#if defined(M2_VT_FEATURE_ENABLED)
                || RIL_REQUEST_HANGUP_VT == requestId
#endif
                || RIL_REQUEST_DEACTIVATE_DATA_CALL == requestId)
        {
            bIsReqAllowed = TRUE;
        }
        else
        {
            bIsReqAllowed = FALSE;
        }
    }
    else
    {
        bIsReqAllowed = TRUE;
    }

    if (RIL_E_SUCCESS != eRetVal && NULL != rilToken)
    {
        RIL_onRequestComplete(rilToken, (RIL_Errno)eRetVal, NULL, 0);
    }

    switch (requestId)
    {
        case RIL_REQUEST_DTMF_START:
            RIL_LOG_INFO("CTE::IsRequestAllowed() - RIL_REQUEST_DTMF_START\r\n");

            bIsReqAllowed = FALSE;
            CMutex::Lock(m_pDtmfStateAccess);

            if (E_DTMF_STATE_STOP == GetDtmfState()
                    && m_pTEBaseInstance->IsDtmfAllowed(callId))
            {
                SetDtmfState(E_DTMF_STATE_START);
                bIsReqAllowed = TRUE;
            }
            else
            {
                // Current request
                if (NULL != rilToken)
                {
                    RIL_onRequestComplete(rilToken, RIL_E_GENERIC_FAILURE, NULL, 0);
                }

                /*
                 * Incase of multi-party call, first call id in the multi-party
                 * call is added as the call id for DTMF requests. So, if dtmf is
                 * not allowed for the first call, then it implicitly means that DTMF
                 * is not allowed for all the calls in the multi-party call.
                 */
                if (!m_pTEBaseInstance->IsDtmfAllowed(callId))
                {
                    // Complete pending DTMF start and stop request
                    CompleteIdenticalRequests(uiChannelId,
                            RIL_REQUEST_DTMF_START, RIL_E_GENERIC_FAILURE, NULL, 0, callId);

                    CompleteIdenticalRequests(uiChannelId,
                            RIL_REQUEST_DTMF_STOP, RIL_E_GENERIC_FAILURE, NULL, 0, callId);

                    SetDtmfState(E_DTMF_STATE_STOP);
                }
            }

            CMutex::Unlock(m_pDtmfStateAccess);
            break;

        case RIL_REQUEST_DTMF_STOP:
        {
            RIL_LOG_INFO("CTE::IsRequestAllowed() - RIL_REQUEST_DTMF_STOP\r\n");

            bIsReqAllowed = FALSE;

            int dtmfState = TestAndSetDtmfState(E_DTMF_STATE_STOP);
            if (E_DTMF_STATE_START == dtmfState
                    && m_pTEBaseInstance->IsDtmfAllowed(callId))
            {
                bIsReqAllowed = TRUE;
            }
            else
            {
                // Current request
                if (NULL != rilToken)
                {
                    RIL_onRequestComplete(rilToken, RIL_E_GENERIC_FAILURE, NULL, 0);
                }

                if (!m_pTEBaseInstance->IsDtmfAllowed(callId))
                {
                    // Complete pending DTMF start and stop request
                    CompleteIdenticalRequests(uiChannelId,
                            RIL_REQUEST_DTMF_START, RIL_E_GENERIC_FAILURE, NULL, 0, callId);

                    CompleteIdenticalRequests(uiChannelId,
                            RIL_REQUEST_DTMF_STOP, RIL_E_GENERIC_FAILURE, NULL, 0, callId);
                }
            }
            break;
        }

        default:
            break;
    }

    return bIsReqAllowed;
}

BOOL CTE::isRetryPossible(UINT32 uiErrorCode)
{
    switch (uiErrorCode)
    {
        case CMS_ERROR_NETWORK_FAILURE:
        case CMS_ERROR_NO_ROUTE_TO_DESTINATION:
        case CMS_ERROR_ACM_MAX:
        case CMS_ERROR_CALLED_PARTY_BLACKLISTED:
        case CMS_ERROR_NUMBER_INCORRECT:
        case CMS_ERROR_SIM_ABSENT:
        case CMS_ERROR_MO_SMS_REJECTED_BY_SIM_MO_SMS_CONTROL:
        case CMS_ERROR_CM_SERVICE_REJECT_FROM_NETWORK:
        case CMS_ERROR_TIMER_EXPIRY:
        case CMS_ERROR_IMSI_DETACH_INITIATED:
            return FALSE;
        default:
            return TRUE;
    }
}

void CTE::CleanRequestData(REQUEST_DATA& rReqData)
{
    free(rReqData.pContextData);
    rReqData.pContextData = NULL;

    free(rReqData.pContextData2);
    rReqData.pContextData2 = NULL;
}

BOOL CTE::isFDNRequest(int fileId)
{
    switch (fileId)
    {
        case EF_FDN:
        case EF_EXT2:
            return TRUE;
        default:
            return FALSE;
    }
}

BOOL CTE::TestAndSetSpoofCommandsStatus(BOOL bStatus)
{
    CMutex::Lock(CSystemManager::GetInstance().GetSpoofCommandsStatusAccessMutex());
    BOOL bPrevSpoofCommandsStatus = m_bSpoofCommandsStatus;

    if (m_bSpoofCommandsStatus != bStatus)
        m_bSpoofCommandsStatus = bStatus;

    CMutex::Unlock(CSystemManager::GetInstance().GetSpoofCommandsStatusAccessMutex());
    return bPrevSpoofCommandsStatus;
}

BOOL CTE::TestAndSetDataCleanupStatus(BOOL bCleanupStatus)
{
    CMutex::Lock(m_pDataCleanupStatusLock);
    BOOL bPrevDataCleanupStatus = m_bDataCleanupStatus;

    m_bDataCleanupStatus =
            m_bDataCleanupStatus != bCleanupStatus ? bCleanupStatus: m_bDataCleanupStatus;

    CMutex::Unlock(m_pDataCleanupStatusLock);
    return bPrevDataCleanupStatus;
}

//
// Silent PIN Entry (sent internally)
//
RIL_RESULT_CODE CTE::ParseSilentPinEntry(RESPONSE_DATA& rRspData)
{
    return m_pTEBaseInstance->ParseSilentPinEntry(rRspData);
}

//
// Create Extended Error Report request (called internally)
//
BOOL CTE::RequestQueryNEER(UINT32 uiChannel, RIL_Token rilToken, int reqId)
{
    RIL_LOG_VERBOSE("CTE::RequestQueryNEER() Enter\r\n");

    BOOL bRet = FALSE;
    REQUEST_DATA reqData;

    memset(&reqData, 0, sizeof(REQUEST_DATA));

    if (!m_pTEBaseInstance->CreateQueryNEER(reqData))
    {
        RIL_LOG_CRITICAL("CTE::RequestQueryNEER() -Unable to create AT command data\r\n");
        goto Error;
    }
    else
    {
        CCommand* pCmd = new CCommand(uiChannel, rilToken, reqId, reqData,
                &CTE::ParseQueryNEER);
        if (pCmd)
        {
            pCmd->SetHighPriority();

            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE::RequestQueryNEER() -Unable to add command to queue\r\n");
                delete pCmd;
                pCmd = NULL;
                goto Error;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestQueryNEER() - Unable to allocate memory "
                    "for command\r\n");
            goto Error;
        }
    }

    bRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CTE::RequestQueryNEER() - Exit\r\n");
    return bRet;
}

RIL_RESULT_CODE CTE::ParseQueryNEER(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseQueryNEER() - Enter / Exit\r\n");
    return m_pTEBaseInstance->ParseQueryNEER(rRspData);
}

RIL_RESULT_CODE CTE::ParseReadContextParams(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseReadContextParams() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseReadContextParams(rRspData);
}

RIL_RESULT_CODE CTE::ParseReadBearerTFTParams(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseReadBearerTFTParams() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseReadBearerTFTParams(rRspData);
}

RIL_RESULT_CODE CTE::ParseReadBearerQOSParams(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseReadBearerQOSParams() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseReadBearerQOSParams(rRspData);
}

void CTE::PostCmdHandlerCompleteRequest(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostCmdHandlerCompleteRequest() Enter\r\n");

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_INFO("CTE::PostCmdHandlerCompleteRequest() RilToken NULL -"
                " May be internal request, RequestID: %d\r\n", rData.requestId);
        return;
    }

    RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                                                rData.pData, rData.uiDataSize);

    RIL_LOG_VERBOSE("CTE::PostCmdHandlerCompleteRequest() Exit\r\n");
}

void CTE::PostGetSimStatusCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostGetSimStatusCmdHandler() Enter\r\n");
    RIL_CardStatus_v6 cardStatus;

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostGetSimStatusCmdHandler() rData.pRilToken NULL!\r\n");
        return;
    }

    // Fill in the default values
    cardStatus.gsm_umts_subscription_app_index = -1;
    cardStatus.cdma_subscription_app_index = -1;
    cardStatus.ims_subscription_app_index = -1;
    cardStatus.universal_pin_state = RIL_PINSTATE_UNKNOWN;
    // Report card state error for SIM error state (6) from +XSIM and +XSIMSTATE
    if (IsSimError())
    {
        cardStatus.card_state = RIL_CARDSTATE_ERROR;
    }
    else
    {
        cardStatus.card_state = RIL_CARDSTATE_PRESENT;
    }
    cardStatus.num_applications = 1;
    cardStatus.applications[0].app_state = RIL_APPSTATE_DETECTED;
    cardStatus.applications[0].perso_substate = RIL_PERSOSUBSTATE_UNKNOWN;
    cardStatus.applications[0].aid_ptr = NULL;
    cardStatus.applications[0].app_label_ptr = NULL;
    cardStatus.applications[0].pin1_replaced = 0;
    cardStatus.applications[0].pin1 = RIL_PINSTATE_UNKNOWN;
    cardStatus.applications[0].pin2 = RIL_PINSTATE_UNKNOWN;
#if defined(M2_PIN_RETRIES_FEATURE_ENABLED)
    cardStatus.applications[0].pin1_num_retries = -1;
    cardStatus.applications[0].puk1_num_retries = -1;
    cardStatus.applications[0].pin2_num_retries = -1;
    cardStatus.applications[0].puk2_num_retries = -1;
#endif // M2_PIN_RETRIES_FEATURE_ENABLED

    if (RIL_E_SUCCESS != rData.uiResultCode)
    {
        RIL_LOG_INFO("CTE::PostGetSimStatusCmdHandler() :"
                " Found CME Error on AT+CPIN? request.\r\n");

        switch (rData.uiErrorCode)
        {
            case CME_ERROR_SIM_NOT_INSERTED:
            {
                RIL_LOG_INFO("CTE::PostGetSimStatusCmdHandler() : SIM Card is absent!\r\n");

                cardStatus.gsm_umts_subscription_app_index = -1;
                cardStatus.cdma_subscription_app_index = -1;
                cardStatus.ims_subscription_app_index = -1;
                cardStatus.universal_pin_state = RIL_PINSTATE_UNKNOWN;
                cardStatus.card_state = RIL_CARDSTATE_ABSENT;
                cardStatus.num_applications = 0;
            }
            break;

            case CME_ERROR_SIM_NOT_READY:
            {
                RIL_LOG_INFO("CTE::PostGetSimStatusCmdHandler() : SIM Card is not ready!\r\n");

                cardStatus.gsm_umts_subscription_app_index = 0;
                cardStatus.cdma_subscription_app_index = -1;
                cardStatus.ims_subscription_app_index = -1;
                cardStatus.universal_pin_state = RIL_PINSTATE_UNKNOWN;

                cardStatus.num_applications = 1;

                cardStatus.applications[0].app_type = RIL_APPTYPE_UNKNOWN;
                cardStatus.applications[0].app_state = RIL_APPSTATE_DETECTED;
                cardStatus.applications[0].perso_substate =
                                                    RIL_PERSOSUBSTATE_UNKNOWN;
                cardStatus.applications[0].aid_ptr = NULL;
                cardStatus.applications[0].app_label_ptr = NULL;
                cardStatus.applications[0].pin1_replaced = 0;
                cardStatus.applications[0].pin1 = RIL_PINSTATE_UNKNOWN;
                cardStatus.applications[0].pin2 = RIL_PINSTATE_UNKNOWN;
#if defined(M2_PIN_RETRIES_FEATURE_ENABLED)
                cardStatus.applications[0].pin1_num_retries = -1;
                cardStatus.applications[0].puk1_num_retries = -1;
                cardStatus.applications[0].pin2_num_retries = -1;
                cardStatus.applications[0].puk2_num_retries = -1;
#endif // M2_PIN_RETRIES_FEATURE_ENABLED
            }
            break;

            case CME_ERROR_SIM_WRONG:
            {
                RIL_LOG_INFO("CTE::PostGetSimStatusCmdHandler() : WRONG SIM Card!\r\n");

                cardStatus.gsm_umts_subscription_app_index = 0;
                cardStatus.cdma_subscription_app_index = -1;
                cardStatus.ims_subscription_app_index = -1;
                cardStatus.universal_pin_state =
                                            RIL_PINSTATE_ENABLED_PERM_BLOCKED;
                cardStatus.num_applications = 0;
            }
            break;

            default:
            break;
        }
    }
    else
    {
        // Upon success, check for silent PIN entry case
        if (NULL != rData.pData && sizeof(RIL_CardStatus_v6) == rData.uiDataSize)
        {
            RIL_CardStatus_v6* pCardStatus = (RIL_CardStatus_v6*) rData.pData;
            if (m_pTEBaseInstance->IsPinEnabled(pCardStatus)
                    && m_pTEBaseInstance->GetPinRetryCount() > 2)
            {
                BOOL bRet = m_pTEBaseInstance->HandleSilentPINEntry(rData.pRilToken, NULL, 0);
                if (bRet)
                {
                    /*
                     * Incase of silent pin entry success/failure,
                     * RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED will be completed in
                     * PostSilentPinRetryCmdHandler. This will force the framework to query
                     * sim status again. So, complete SIM app status as RIL_APPSTATE_DETECTED
                     * in silent pin entry case.
                     */
                    pCardStatus->applications[0].app_state = RIL_APPSTATE_DETECTED;
                    pCardStatus->applications[0].perso_substate = RIL_PERSOSUBSTATE_UNKNOWN;
                    pCardStatus->applications[0].pin1 = RIL_PINSTATE_UNKNOWN;

                    RIL_LOG_INFO("CTE::PostGetSimStatusCmdHandler() -"
                            " HandleSilentPINEntry case\r\n");
                }
            }
            // On Success and response valid, copy the response to cardStatus.
            memcpy(&cardStatus, pCardStatus, sizeof(RIL_CardStatus_v6));
        }
    }

    RIL_onRequestComplete(rData.pRilToken, RIL_E_SUCCESS,
            (void*) &cardStatus, sizeof(RIL_CardStatus_v6));

    RIL_LOG_VERBOSE("CTE::PostGetSimStatusCmdHandler() Exit\r\n");
}

void CTE::PostSimPinCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostSimPinCmdHandler() Enter\r\n");

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostSimPinCmdHandler() rData.pRilToken NULL!\r\n");
        return;
    }

    if (RIL_E_SUCCESS != rData.uiResultCode)
    {
        switch (rData.uiErrorCode)
        {
            case CME_ERROR_INCORRECT_PASSWORD:
                RIL_LOG_INFO("CTE::PostSimPinCmdHandler() - Incorrect password");
                rData.uiResultCode = RIL_E_PASSWORD_INCORRECT;
                break;

            case CME_ERROR_SIM_PUK_REQUIRED:
                RIL_LOG_INFO("CTE::PostSimPinCmdHandler() - SIM PUK required");
                rData.uiResultCode = RIL_E_PASSWORD_INCORRECT;
                SetSIMState(RRIL_SIM_STATE_NOT_READY);
                break;

            case CME_ERROR_SIM_PIN2_REQUIRED:
                RIL_LOG_INFO("CTE::PostSimPinCmdHandler() - SIM PIN2 required");
                rData.uiResultCode = RIL_E_SIM_PIN2;
                m_pTEBaseInstance->SetPin2State(RIL_PINSTATE_ENABLED_NOT_VERIFIED);
                break;

            case CME_ERROR_SIM_PUK2_REQUIRED:
                RIL_LOG_INFO("CTE::PostSimPinCmdHandler() - SIM PUK2 required");
                rData.uiResultCode = RIL_E_SIM_PUK2;
                m_pTEBaseInstance->SetPin2State(RIL_PINSTATE_ENABLED_BLOCKED);
                break;

            default:
                RIL_LOG_INFO("CTE::PostSimPinCmdHandler() - Unknown error [%u]",
                                                            rData.uiErrorCode);
                rData.uiResultCode = RIL_E_GENERIC_FAILURE;
                break;
        }
    }

    /*
     * Currently, ril documentation is not clear on whether the valid number of retries
     * should be sent on success or failure or on both. Following code requests PIN retry
     * count on both success and failure. If there is some issue in adding the PIN retry
     * request to queue, then the actual ril request will be completed with noOfRetries
     * set to -1(means unknown value).
     */
    int noOfRetries = -1; // -1 means unknown value
    UINT32* pResultCode = (UINT32*)malloc(sizeof(UINT32));
    if (NULL == pResultCode)
    {
        RIL_LOG_CRITICAL("CTE::PostSimPinCmdHandler() -"
                " Could not allocate memory for pResultCode\r\n");
    }
    else
    {
        /*
         * Pass the result code as context data to Retry count request.
         * Incase of no error in adding retry count request to command queue, pResultCode
         * will be/has to be deleted in the PostSimPinRetryCount function.
         */
        *pResultCode = rData.uiResultCode;
        RIL_RESULT_CODE res = RequestSimPinRetryCount(rData.pRilToken,
                                                    (void*) pResultCode,
                                                    sizeof(UINT32),
                                                    rData.requestId,
                                                    &CTE::PostSimPinRetryCount);
        if (RRIL_RESULT_OK == res)
        {
            RIL_LOG_INFO("CTE::PostSimPinCmdHandler() - PinRetryCount case\r\n");
            return;
        }
    }

Error:
    free(pResultCode);

    RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                                (void*) &noOfRetries, sizeof(noOfRetries));

    RIL_LOG_VERBOSE("CTE::PostSimPinCmdHandler() Exit\r\n");
}

void CTE::PostSimPin2CmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostSimPin2CmdHandler() Enter\r\n");

    if (RIL_E_SUCCESS == rData.uiResultCode)
    {
        m_pTEBaseInstance->SetPin2State(RIL_PINSTATE_ENABLED_VERIFIED);
    }

    PostSimPinCmdHandler(rData);

    RIL_LOG_VERBOSE("CTE::PostSimPin2CmdHandler() Exit\r\n");
}

void CTE::PostNtwkPersonalizationCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostNtwkPersonalizationCmdHandler() Enter\r\n");

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostNtwkPersonalizationCmdHandler() rData.pRilToken NULL!\r\n");
        return;
    }

    if (RIL_E_SUCCESS != rData.uiResultCode)
    {
        int noOfRetries = -1; // -1 means unknown value

        switch (rData.uiErrorCode)
        {
            case CME_ERROR_INCORRECT_PASSWORD:
                RIL_LOG_INFO("CTE::PostNtwkPersonalizationCmdHandler() - Incorrect password");
                rData.uiResultCode = RIL_E_PASSWORD_INCORRECT;
                break;

            case CME_ERROR_NETWORK_PUK_REQUIRED:
                RIL_LOG_INFO("CTE::PostNtwkPersonalizationCmdHandler() - NETWORK PUK required");
                rData.uiResultCode = RIL_E_NETWORK_PUK_REQUIRED;
                SetSIMState(RRIL_SIM_STATE_NOT_READY);
                break;

            default:
                RIL_LOG_INFO("CTE::PostNtwkPersonalizationCmdHandler() - Unknown error [%u]",
                                                            rData.uiErrorCode);
                rData.uiResultCode = RIL_E_GENERIC_FAILURE;
                break;
        }

        // Number of retry count not available for Network personalization locks
        RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                                    (void*) &noOfRetries, sizeof(noOfRetries));
    }
    else
    {
        /// @TODO: Check this out
        SetSIMState(RRIL_SIM_STATE_READY);
        CSystemManager::GetInstance().TriggerSimUnlockedEvent();

        RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                                                rData.pData, rData.uiDataSize);
    }

    RIL_LOG_VERBOSE("CTE::PostNtwkPersonalizationCmdHandler() Exit\r\n");
}

void CTE::PostGetCurrentCallsCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostGetCurrentCallsCmdHandler() Enter\r\n");

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostGetCurrentCallsCmdHandler() rData.pRilToken NULL!\r\n");
        return;
    }

    if (RIL_E_GENERIC_FAILURE == rData.uiResultCode &&
                        RRIL_SIM_STATE_NOT_READY == GetSIMState())
        rData.uiResultCode = RIL_E_SUCCESS;

    RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                                                rData.pData, rData.uiDataSize);

    RIL_LOG_VERBOSE("CTE::PostGetCurrentCallsCmdHandler() Exit\r\n");
}

void CTE::PostDialCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostDialCmdHandler() Enter\r\n");

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostDialCmdHandler() rData.pRilToken NULL!\r\n");
        return;
    }

    RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                                                rData.pData, rData.uiDataSize);

    RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0);

    RIL_LOG_VERBOSE("CTE::PostDialCmdHandler() Exit\r\n");
}

void CTE::PostHangupCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostHangupCmdHandler() Enter\r\n");

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostHangupCmdHandler() rData.pRilToken NULL!\r\n");
        return;
    }

    RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                                                rData.pData, rData.uiDataSize);

    RIL_LOG_VERBOSE("CTE::PostHangupCmdHandler() Exit\r\n");
}

void CTE::PostSwitchHoldingAndActiveCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostSwitchHoldingAndActiveCmdHandler() Enter\r\n");

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostSwitchHoldingAndActiveCmdHandler() rData.pRilToken NULL!\r\n");
        return;
    }

    RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                                                rData.pData, rData.uiDataSize);

    if (IsClearPendingCHLD() || RRIL_RESULT_OK != rData.uiResultCode)
    {
        RIL_LOG_VERBOSE("CTE::PostSwitchHoldingAndActiveCmdHandler()"
                " clearing all RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE\r\n");
        SetClearPendingCHLDs(FALSE);

        CompleteIdenticalRequests(rData.uiChannel,
                RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE, RIL_E_GENERIC_FAILURE, NULL, 0);
    }

    RIL_LOG_VERBOSE("CTE::PostSwitchHoldingAndActiveCmdHandler() Exit\r\n");
}

void CTE::PostConferenceCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostConferenceCmdHandler() Enter\r\n");

    PostHangupCmdHandler(rData);

    RIL_LOG_VERBOSE("CTE::PostConferenceCmdHandler() Exit\r\n");
}

void CTE::PostNetworkInfoCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostNetworkInfoCmdHandler() Enter\r\n");
    CChannel* pChannel = NULL;

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostNetworkInfoCmdHandler() rData.pRilToken NULL!\r\n");
        return;
    }

    RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                                                rData.pData, rData.uiDataSize);

    for (UINT32 i = 0; i < g_uiRilChannelCurMax; i++)
    {
        pChannel = g_pRilChannel[i];
        if (NULL == pChannel) // could be NULL if reserved channel
            continue;

        if (pChannel->GetRilChannel() == rData.uiChannel)
            break;
    }

    if (NULL == pChannel)
    {
        RIL_LOG_INFO("CTE::PostNetworkInfoCmdHandler() pChannel NULL!\r\n");
        return;
    }

    CompleteIdenticalRequests(rData.uiChannel, rData.requestId,
            rData.uiResultCode, (void*)rData.pData, rData.uiDataSize);

    RIL_LOG_VERBOSE("CTE::PostNetworkInfoCmdHandler() Exit\r\n");
}

void CTE::PostOperator(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostOperator() Enter\r\n");

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostOperator() rData.pRilToken NULL!\r\n");
        return;
    }

    if (RIL_E_SUCCESS != rData.uiResultCode)
    {
        if (CME_ERROR_NO_NETWORK_SERVICE == rData.uiErrorCode)
            rData.uiResultCode = RIL_E_OP_NOT_ALLOWED_BEFORE_REG_TO_NW;
        else
            rData.uiResultCode = RIL_E_GENERIC_FAILURE;
    }

    PostNetworkInfoCmdHandler(rData);

    RIL_LOG_VERBOSE("CTE::PostOperator() Exit\r\n");
}

void CTE::PostRadioPower(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostRadioPower() Enter\r\n");

    ResetInternalStates();

    triggerDataResumedInd(NULL);

    /*
     * Rapid RIL remains active even when the system services are
     * killed due to FATAL exception. So, when the system servcies
     * are started again, android telephony framework will turn off
     * the RADIO which is as per its state machine. This will result
     * in cleaning up the data connections on modem side. So, framework
     * and modem will have the right data state but not the rapid ril.
     * Cleanup data connections internally when there is a change in
     * radio state(on/off).
     */
    CleanupAllDataConnections();

    if (RADIO_POWER_ON == m_RequestedRadioPower)
    {
        // Send request to SetPreferredNetworkType if previously received before radio power on
        if (NULL != m_pPrefNetTypeReqInfo)
        {
            SendSetPrefNetTypeRequest();
        }

        if (SCREEN_STATE_UNKNOWN != m_ScreenState)
        {
            m_pTEBaseInstance->HandleScreenStateReq(m_ScreenState);
        }

        //  Turning on phone
        SetRadioStateAndNotify(RRIL_RADIO_STATE_ON);
        CSystemManager::GetInstance().TriggerRadioPoweredOnEvent();
    }
    else
    {
        if (E_RADIO_OFF_REASON_SHUTDOWN == m_RadioOffReason)
        {
            //  Send shutdown request to MMgr
            if (!CSystemManager::GetInstance().SendRequestModemShutdown())
            {
                RIL_LOG_CRITICAL("CTE::PostRadioPower() - CANNOT SEND MODEM SHUTDOWN REQUEST\r\n");

                /*
                 * Even if modem power off request fails, close the channel ports
                 * and complete the modem power off ril request
                 */
                CSystemManager::GetInstance().CloseChannelPorts();

                CSystemManager::GetInstance().TriggerModemPoweredOffEvent();
                SetRadioStateAndNotify(RRIL_RADIO_STATE_OFF);
            }
        }
        else
        {
            SetRadioStateAndNotify(RRIL_RADIO_STATE_OFF);

            if (GetModemOffInFlightModeState()
                    && E_RADIO_OFF_REASON_AIRPLANE_MODE == m_RadioOffReason)
            {
                CSystemManager::GetInstance().ReleaseModem();
            }
        }
    }

    if (NULL != m_pRadioStateChangedEvent)
    {
        CEvent::Signal(m_pRadioStateChangedEvent);
    }

    RIL_LOG_VERBOSE("CTE::PostRadioPower() Exit\r\n");
}

void CTE::PostSendSmsCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostSendSmsCmdHandler() Enter\r\n");

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostSendSmsCmdHandler() rData.pRilToken NULL!\r\n");
        return;
    }

    if (RIL_E_SUCCESS != rData.uiResultCode)
    {
        switch (rData.uiErrorCode)
        {
            case CMS_ERROR_FDN_CHECK_FAILED:
            case CMS_ERROR_SCA_FDN_FAILED:
            case CMS_ERROR_DA_FDN_FAILED:
                rData.uiResultCode = RIL_E_FDN_CHECK_FAILURE;
                break;
            default:
                if (isRetryPossible(rData.uiErrorCode))
                    rData.uiResultCode = RIL_E_SMS_SEND_FAIL_RETRY;
                else
                    rData.uiResultCode = RIL_E_GENERIC_FAILURE;
                break;
        }
    }

    RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                                                rData.pData, rData.uiDataSize);

    RIL_LOG_VERBOSE("CTE::PostSendSmsCmdHandler() Exit\r\n");
}

void CTE::PostSetupDataCallCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostSetupDataCallCmdHandler - Enter\r\n");

    m_pTEBaseInstance->PostSetupDataCallCmdHandler(rData);

    RIL_LOG_VERBOSE("CTE::PostSetupDataCallCmdHandler() Exit\r\n");
}

void CTE::PostPdpContextActivateCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostPdpContextActivateCmdHandler - Enter\r\n");

    m_pTEBaseInstance->PostPdpContextActivateCmdHandler(rData);

    RIL_LOG_VERBOSE("CTE::PostActivateDataCallCmdHandler() Exit\r\n");
}

void CTE::PostQueryIpAndDnsCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostQueryIpAndDnsCmdHandler - Enter\r\n");

    m_pTEBaseInstance->PostQueryIpAndDnsCmdHandler(rData);

    RIL_LOG_VERBOSE("CTE::PostQueryIpAndDnsCmdHandler() Exit\r\n");
}

void CTE::PostEnterDataStateCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostEnterDataStateCmdHandler - Enter\r\n");

    m_pTEBaseInstance->PostEnterDataStateCmdHandler(rData);

    RIL_LOG_VERBOSE("CTE::PostEnterDataStateCmdHandler() Exit\r\n");
}

void CTE::PostSimIOCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostSimIOCmdHandler() Enter\r\n");

    if (RIL_E_SUCCESS != rData.uiResultCode)
    {
        switch (rData.uiErrorCode)
        {
            case CME_ERROR_SIM_PIN2_REQUIRED:
                RIL_LOG_INFO("CTE::PostSimIOCmdHandler() - SIM PIN2 required");
                rData.uiResultCode = RIL_E_SIM_PIN2;
                m_pTEBaseInstance->SetPin2State(RIL_PINSTATE_ENABLED_NOT_VERIFIED);
                break;

            case CME_ERROR_INCORRECT_PASSWORD:
                RIL_LOG_INFO("CTE::PostSimIOCmdHandler() - Incorrect Password");
                rData.uiResultCode = RIL_E_PASSWORD_INCORRECT;
                break;

            case CME_ERROR_SIM_PUK2_REQUIRED:
                RIL_LOG_INFO("CTE::PostSimIOCmdHandler() - SIM PUK2 required");
                rData.uiResultCode = RIL_E_SIM_PUK2;
                m_pTEBaseInstance->SetPin2State(RIL_PINSTATE_ENABLED_BLOCKED);
                break;

            default:
                RIL_LOG_INFO("CTE::PostSimIOCmdHandler() - Unknown error [%u]",
                                                            rData.uiErrorCode);
                rData.uiResultCode = RIL_E_GENERIC_FAILURE;
                break;
        }
    }
    else // Success case
    {
        if (NULL != rData.pContextData &&
                    rData.uiContextDataSize == sizeof(S_SIM_IO_CONTEXT_DATA))
        {
            S_SIM_IO_CONTEXT_DATA* pContextData =
                        (S_SIM_IO_CONTEXT_DATA*) rData.pContextData;
            if (isFDNRequest(pContextData->fileId) &&
                        SIM_COMMAND_UPDATE_RECORD == pContextData->command)
            {
                m_pTEBaseInstance->SetPin2State(RIL_PINSTATE_ENABLED_VERIFIED);
            }
        }
    }

    free(rData.pContextData);
    rData.pContextData = NULL;

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostSimIOCmdHandler() rData.pRilToken NULL!!!\r\n");
    }
    else
    {
        RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                                                rData.pData, rData.uiDataSize);
    }

    RIL_LOG_VERBOSE("CTE::PostSimIOCmdHandler() Exit\r\n");
}

void CTE::PostDeactivateDataCallCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostDeactivateDataCallCmdHandler - Enter\r\n");

    m_pTEBaseInstance->PostDeactivateDataCallCmdHandler(rData);

    RIL_LOG_VERBOSE("CTE::PostDeactivateDataCallCmdHandler() Exit\r\n");
}

void CTE::PostSetFacilityLockCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostSetFacilityLockCmdHandler() Enter\r\n");

    if (RIL_E_SUCCESS != rData.uiResultCode)
    {
        switch (rData.uiErrorCode)
        {
            case CME_ERROR_INCORRECT_PASSWORD:
                RIL_LOG_INFO("CTE::PostSetFacilityLockCmdHandler() - Incorrect password");
                rData.uiResultCode = RIL_E_PASSWORD_INCORRECT;
                break;

            case CME_ERROR_SIM_PUK_REQUIRED:
                RIL_LOG_INFO("CTE::PostSetFacilityLockCmdHandler() - SIM PUK required");
                rData.uiResultCode = RIL_E_PASSWORD_INCORRECT;
                SetSIMState(RRIL_SIM_STATE_NOT_READY);
                break;

            case CME_ERROR_SIM_PUK2_REQUIRED:
                RIL_LOG_INFO("CTE::PostSetFacilityLockCmdHandler() - SIM PUK2 required");
                rData.uiResultCode = RIL_E_SIM_PUK2;
                m_pTEBaseInstance->SetPin2State(RIL_PINSTATE_ENABLED_BLOCKED);
                break;

            default:
                RIL_LOG_INFO("CTE::PostSetFacilityLockCmdHandler() - Unknown error [%d]",
                        rData.uiErrorCode);
                rData.uiResultCode = RIL_E_GENERIC_FAILURE;
                break;
        }
    }

    int noOfRetries = -1; // -1 means unknown value
    if (NULL == rData.pContextData ||
                    rData.uiContextDataSize != sizeof(S_SET_FACILITY_LOCK_CONTEXT_DATA))
    {
        RIL_LOG_INFO("CTE::PostSetFacilityLockCmdHandler() -"
                " pin retry count not available case\r\n");
    }
    else
    {
        RIL_LOG_INFO("CTE::PostSetFacilityLockCmdHandler() - Fetch pin retry count\r\n");

        /*
         * Context Data will be set only for SC(SIM CARD) and FD(Fixed Dialing) locks.
         * This is because modem only supports retry count information for SC and FD
         * locks via XPINCNT.
         *
         * Note: No point in calling this on success but ril documentation not clear
         */
        S_SET_FACILITY_LOCK_CONTEXT_DATA* pContextData =
                                (S_SET_FACILITY_LOCK_CONTEXT_DATA*) rData.pContextData;

        if (RIL_E_SUCCESS == rData.uiResultCode
                && (0 == strncmp(pContextData->szFacilityLock, "FD", 2)))
        {
            m_pTEBaseInstance->SetPin2State(RIL_PINSTATE_ENABLED_VERIFIED);
        }

        pContextData->uiResultCode = rData.uiResultCode;

        RIL_RESULT_CODE res = RequestSimPinRetryCount(rData.pRilToken, pContextData,
                                                sizeof(S_SET_FACILITY_LOCK_CONTEXT_DATA),
                                                rData.requestId,
                                                &CTE::PostFacilityLockRetryCount);
        if (RRIL_RESULT_OK == res)
        {
            RIL_LOG_CRITICAL("CTE::PostSetFacilityLockCmdHandler - PinRetryCount case\r\n");
            return;
        }
    }

    /*
     * Incase of SIM Card and FD Lock, context data will be a pointer to
     * S_SET_FACILITY_LOCK_CONTEXT_DATA. Free it before completing the request.
     * For other fac's, this won't create any issues as free called with NULL
     * is safe.
     */
    free(rData.pContextData);
    rData.pContextData = NULL;

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostSetFacilityLockCmdHandler() rData.pRilToken NULL!\r\n");
    }
    else
    {
        RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                                    (void*) &noOfRetries, sizeof(noOfRetries));
    }

    RIL_LOG_VERBOSE("CTE::PostSetFacilityLockCmdHandler() Exit\r\n");
}

void CTE::PostQueryAvailableNetworksCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostQueryAvailableNetworksCmdHandler() Enter\r\n");

    char szConformanceProperty[PROPERTY_VALUE_MAX] = {'\0'};

    property_get("persist.conformance", szConformanceProperty, NULL);
    if (0 != strncmp(szConformanceProperty, "true", PROPERTY_VALUE_MAX))
    {
        m_pTEBaseInstance->PSAttach();
    }

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostQueryAvailableNetworksCmdHandler() rData.pRilToken NULL!\r\n");
        return;
    }

    if (RIL_E_SUCCESS != rData.uiResultCode)
    {
        switch (rData.uiErrorCode)
        {
            case CME_ERROR_PLMN_NOT_ALLOWED:
            case CME_ERROR_LOCATION_NOT_ALLOWED:
            case CME_ERROR_ROAMING_NOT_ALLOWED:
                rData.uiResultCode = RIL_E_ILLEGAL_SIM_OR_ME;
                break;
            default:
                break;
        }
    }

    SetManualNetworkSearchOn(FALSE);

    RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                                                rData.pData, rData.uiDataSize);

    RIL_LOG_VERBOSE("CTE::PostQueryAvailableNetworksCmdHandler() Exit\r\n");
}

void CTE::PostDtmfStart(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostDtmfStart() Enter\r\n");

    if (NULL != rData.pRilToken)
    {
        RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                rData.pData, rData.uiDataSize);
    }

    RIL_LOG_VERBOSE("CTE::PostDtmfStart() Exit\r\n");
}

void CTE::PostDtmfStop(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostDtmfStop() Enter\r\n");

    if (NULL != rData.pRilToken)
    {
        RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                rData.pData, rData.uiDataSize);
    }

    RIL_LOG_VERBOSE("CTE::PostDtmfStop() Exit\r\n");
}

void CTE::PostHookStrings(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostHookStrings() Enter\r\n");

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostHookStrings() rData.pRilToken NULL!\r\n");
        return;
    }

    UINT32 uiCommand = (UINT32)rData.pContextData;
    if (RIL_OEM_HOOK_STRING_POWEROFF_MODEM == (int) uiCommand)
    {
        // Send shutdown request to MMgr
        if (!CSystemManager::GetInstance().SendRequestModemShutdown())
        {
            RIL_LOG_CRITICAL("CTE::PostHookStrings() - CANNOT SEND MODEM SHUTDOWN REQUEST\r\n");

            /*
             * Even if modem power off request fails, close the channel ports
             * and complete the modem power off ril request
             */
            CSystemManager::GetInstance().CloseChannelPorts();

            SetRadioStateAndNotify(RRIL_RADIO_STATE_OFF);
            CSystemManager::GetInstance().TriggerModemPoweredOffEvent();
        }

        // Incase of modem off, request will be completed in the CoreHookStrings function
        // on modem powered off event or on request handling error.
    }
    else
    {
        RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                rData.pData, rData.uiDataSize);
    }

    RIL_LOG_VERBOSE("CTE::PostHookStrings() Exit\r\n");
}

void CTE::PostWriteSmsToSimCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostWriteSmsToSimCmdHandler() Enter\r\n");

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostWriteSmsToSimCmdHandler() rData.pRilToken NULL!\r\n");
        return;
    }

    RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                                                rData.pData, rData.uiDataSize);

    if (RIL_E_SUCCESS != rData.uiResultCode &&
            CMS_ERROR_MEMORY_FULL == rData.uiErrorCode)
    {
        RIL_onUnsolicitedResponse(RIL_UNSOL_SIM_SMS_STORAGE_FULL, NULL, 0);
    }

    RIL_LOG_VERBOSE("CTE::PostWriteSmsToSimCmdHandler() Exit\r\n");
}

void CTE::PostGetNeighboringCellIDs(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostGetNeighboringCellIDs() Enter\r\n");

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostGetNeighboringCellIDs() rData.pRilToken NULL!\r\n");
        return;
    }

    RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                                    (void*)rData.pData, rData.uiDataSize);

    CompleteIdenticalRequests(rData.uiChannel,
            rData.requestId, rData.uiResultCode, (void*)rData.pData, rData.uiDataSize);

    RIL_LOG_VERBOSE("CTE::PostGetNeighboringCellIDs() Exit\r\n");
}

void CTE::PostGetCellInfoList(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostGetCellInfoList() Enter\r\n");

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostGetCellInfoList() rData.pRilToken NULL!\r\n");
        return;
    }

    RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                                    (void*)rData.pData, rData.uiDataSize);

    CompleteIdenticalRequests(rData.uiChannel,
            rData.requestId, rData.uiResultCode, (void*)rData.pData, rData.uiDataSize);

    RIL_LOG_VERBOSE("CTE::PostGetCellInfoList() Exit\r\n");
}

void CTE::PostSetLocationUpdates(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostSetLocationUpdates() Enter\r\n");

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostSetLocationUpdates() rData.pRilToken NULL!\r\n");
        return;
    }

    RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                                                NULL, 0);

    if (RIL_E_SUCCESS != rData.uiResultCode)
    {
        m_enableLocationUpdates =
                (m_enableLocationUpdates > 0) ? m_enableLocationUpdates : 0;
    }

    RIL_LOG_VERBOSE("CTE::PostSetLocationUpdates() Exit\r\n");
}

void CTE::PostSilentPinRetryCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostSilentPinRetryCmdHandler() Enter\r\n");

    /* Clear PIN code caching on error */
    if (RRIL_RESULT_OK != rData.uiResultCode)
    {
        PCache_Clear();
    }

    // This will make the framework to trigger GET_SIM_STATUS and QUERY_FACILITY_LOCK requests
    RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, NULL, 0);

    RIL_LOG_VERBOSE("CTE::PostSilentPinRetryCmdHandler() Exit\r\n");
}

RIL_RESULT_CODE CTE::RequestSimPinRetryCount(RIL_Token rilToken, void* pData, size_t datalen,
                                    int reqId, PFN_TE_POSTCMDHANDLER pPostCmdHandlerFcn)
{
    RIL_LOG_VERBOSE("CTE::RequestSimPinRetryCount() - Enter\r\n");

    REQUEST_DATA reqData;
    RIL_RESULT_CODE res = RIL_E_GENERIC_FAILURE;

    if (0 == reqId)
    {
        RIL_LOG_CRITICAL("CTE::RequestSimPinRetryCount() - reqId is 0\r\n");
        return res;
    }

    memset(&reqData, 0, sizeof(REQUEST_DATA));
    res = m_pTEBaseInstance->QueryPinRetryCount(reqData, pData, datalen);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE::RequestSimPinRetryCount() - Unable to create AT command data\r\n");
    }
    else
    {
        CCommand* pCmd = new CCommand(g_pReqInfo[reqId].uiChannel, rilToken, reqId,
                                        reqData, &CTE::ParseSimPinRetryCount, pPostCmdHandlerFcn);

        if (NULL != pCmd)
        {
            pCmd->SetHighPriority();
            pCmd->SetContextData(pData);
            pCmd->SetContextDataSize(datalen);
            if (!CCommand::AddCmdToQueue(pCmd, TRUE))
            {
                RIL_LOG_CRITICAL("CTE::RequestSimPinRetryCount() -"
                        " Unable to add command to queue\r\n");
                res = RIL_E_GENERIC_FAILURE;
                delete pCmd;
                pCmd = NULL;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE::RequestSimPinRetryCount() -"
                    " Unable to allocate memory for command\r\n");
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    RIL_LOG_VERBOSE("CTE::RequestSimPinRetryCount() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE::ParseSimPinRetryCount(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSimPinRetryCount() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSimPinRetryCount(rRspData);
}

void CTE::PostSimPinRetryCount(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostSimPinRetryCount() Enter\r\n");

    int noOfRetries = -1; // -1 means unknown value
    UINT32 uiResultCode;

    if (NULL == rData.pContextData || sizeof(UINT32) < rData.uiContextDataSize)
    {
        RIL_LOG_INFO("CTE::PostSimPinRetryCount() - No Context data\r\n");
        uiResultCode = RIL_E_GENERIC_FAILURE;
    }
    else
    {
        uiResultCode = *((UINT32*) rData.pContextData);
    }

    if (RIL_E_SUCCESS == rData.uiResultCode)
    {
        switch (rData.requestId)
        {
            case RIL_REQUEST_ENTER_SIM_PIN:
            case RIL_REQUEST_CHANGE_SIM_PIN:
                noOfRetries = m_pTEBaseInstance->GetPinRetryCount();
                break;
            case RIL_REQUEST_ENTER_SIM_PIN2:
            case RIL_REQUEST_CHANGE_SIM_PIN2:
                noOfRetries = m_pTEBaseInstance->GetPin2RetryCount();
                break;
            case RIL_REQUEST_ENTER_SIM_PUK:
                noOfRetries = m_pTEBaseInstance->GetPukRetryCount();
                break;
            case RIL_REQUEST_ENTER_SIM_PUK2:
                noOfRetries = m_pTEBaseInstance->GetPuk2RetryCount();
                break;
            default:
                noOfRetries = -1; // -1 means unknown value
                break;
        }
    }

    /*
     * In case of retry count request, actual pin/puk/pin2/puk2 request's result code is
     * passed as contextData.
     */
    free(rData.pContextData);
    rData.pContextData = NULL;

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostSimPinRetryCount() rData.pRilToken NULL!\r\n");
    }
    else
    {
        RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) uiResultCode,
                                (void*) &noOfRetries, sizeof(noOfRetries));
    }

    RIL_LOG_VERBOSE("CTE::PostSimPinRetryCount() Exit\r\n");
}

void CTE::PostFacilityLockRetryCount(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostFacilityLockRetryCount() Enter\r\n");

    int noOfRetries = -1; // -1 means unknown value
    UINT32 uiResultCode = RIL_E_GENERIC_FAILURE;

    if (NULL != rData.pContextData &&
                    rData.uiContextDataSize == sizeof(S_SET_FACILITY_LOCK_CONTEXT_DATA))
    {
        RIL_LOG_INFO("CTE::PostFacilityLockRetryCount() Valid context data\r\n");

        S_SET_FACILITY_LOCK_CONTEXT_DATA* pContextData =
                                (S_SET_FACILITY_LOCK_CONTEXT_DATA*) rData.pContextData;
        uiResultCode = pContextData->uiResultCode;

        if (RIL_E_SUCCESS == rData.uiResultCode)
        {
            if (0 == strncmp(pContextData->szFacilityLock, "SC", 2))
            {
                noOfRetries = m_pTEBaseInstance->GetPinRetryCount();
            }
            else if (0 == strncmp(pContextData->szFacilityLock, "FD", 2))
            {
                noOfRetries = m_pTEBaseInstance->GetPin2RetryCount();
            }
        }
    }

    /*
     * In case of facility lock retry count, actual pin/puk/pin2/puk2 request's result code is
     * passed as contextData.
     */
    free(rData.pContextData);
    rData.pContextData = NULL;

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostFacilityLockRetryCount() rData.pRilToken NULL!\r\n");
    }
    else
    {
        RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) uiResultCode,
                                (void*) &noOfRetries, sizeof(noOfRetries));
    }
    RIL_LOG_VERBOSE("CTE::PostFacilityLockRetryCount() Exit\r\n");
}

void CTE::PostSetNetworkSelectionCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostSetNetworkSelectionCmdHandler() Enter\r\n");

    if (NULL == rData.pRilToken)
    {
        RIL_LOG_CRITICAL("CTE::PostSetNetworkSelectionCmdHandler() rData.pRilToken NULL!\r\n");
        return;
    }

    if (RIL_E_SUCCESS == rData.uiResultCode)
    {
        if (IsNwInitiatedContextActSupported())
        {
            m_pTEBaseInstance->SetAutomaticResponseforNwInitiatedContext(rData);
        }
    }

    // No need to handle ILLEGAL_SIM_OR_ME here since it is already handled in
    // CResponse::RetrieveErrorCode
    if (RIL_E_SUCCESS != rData.uiResultCode &&
            RIL_E_ILLEGAL_SIM_OR_ME != rData.uiResultCode)
    {
        switch (rData.uiErrorCode)
        {
            case CME_ERROR_UKNOWN_ERROR: // 100
            case CME_ERROR_UNSPECIFIED_GPRS_ERROR: // 148

                // send +NEER command to get more info
                if (!RequestQueryNEER(rData.uiChannel, rData.pRilToken, rData.requestId))
                {
                    RIL_LOG_CRITICAL("CTE::PostSetNetworkSelectionCmdHandler() -"
                            " RequestQueryNEER failed\r\n");
                    return;
                }
                break;
        }
    }

    RIL_onRequestComplete(rData.pRilToken, (RIL_Errno) rData.uiResultCode,
                                                rData.pData, rData.uiDataSize);

    RIL_LOG_VERBOSE("CTE::PostSetNetworkSelectionCmdHandler() Exit\r\n");
}

int CTE::GetActiveDataCallInfoList(P_ND_PDP_CONTEXT_DATA pPDPListData)
{
    CChannel_Data* pChannelData = NULL;
    S_DATA_CALL_INFO sDataCallInfo;
    int noOfActivePDP = 0;

    for (UINT32 i = RIL_CHANNEL_DATA1; i < g_uiRilChannelCurMax; i++)
    {
        if (NULL == g_pRilChannel[i]) // could be NULL if reserved channel
            continue;

        pChannelData = static_cast<CChannel_Data*>(g_pRilChannel[i]);

        //  We are taking down all data connections here, so we are looping over each data channel.
        //  Don't call DataConfigDown with invalid CID.
        if (NULL != pChannelData &&
                        E_DATA_STATE_ACTIVE == pChannelData->GetDataState())
        {
            memset(&sDataCallInfo, 0, sizeof(sDataCallInfo));
            pChannelData->GetDataCallInfo(sDataCallInfo);

            PrintStringNullTerminate(pPDPListData->pDnsesBuffers[noOfActivePDP],
                                             MAX_BUFFER_SIZE,
                                             "%s %s %s %s",
                                             sDataCallInfo.szDNS1,
                                             sDataCallInfo.szDNS2,
                                             sDataCallInfo.szIpV6DNS1,
                                             sDataCallInfo.szIpV6DNS2);

            PrintStringNullTerminate(pPDPListData->pAddressBuffers[noOfActivePDP],
                                             MAX_BUFFER_SIZE,
                                             "%s %s",
                                             sDataCallInfo.szIpAddr1,
                                             sDataCallInfo.szIpAddr2);

            CopyStringNullTerminate(pPDPListData->pGatewaysBuffers[noOfActivePDP],
                                            sDataCallInfo.szGateways,
                                            MAX_BUFFER_SIZE);

            CopyStringNullTerminate(pPDPListData->pTypeBuffers[noOfActivePDP],
                                            sDataCallInfo.szPdpType,
                                            MAX_BUFFER_SIZE);

            CopyStringNullTerminate(pPDPListData->pIfnameBuffers[noOfActivePDP],
                                            sDataCallInfo.szInterfaceName,
                                            MAX_BUFFER_SIZE);

            pPDPListData->pPDPData[noOfActivePDP].status = sDataCallInfo.failCause;
            pPDPListData->pPDPData[noOfActivePDP].suggestedRetryTime = -1;
            pPDPListData->pPDPData[noOfActivePDP].cid = sDataCallInfo.uiCID;
            pPDPListData->pPDPData[noOfActivePDP].active = 2;
            pPDPListData->pPDPData[noOfActivePDP].type =
                    pPDPListData->pTypeBuffers[noOfActivePDP];
            pPDPListData->pPDPData[noOfActivePDP].addresses =
                    pPDPListData->pAddressBuffers[noOfActivePDP];
            pPDPListData->pPDPData[noOfActivePDP].dnses =
                    pPDPListData->pDnsesBuffers[noOfActivePDP];
            pPDPListData->pPDPData[noOfActivePDP].gateways =
                    pPDPListData->pGatewaysBuffers[noOfActivePDP];
            pPDPListData->pPDPData[noOfActivePDP].ifname =
                    pPDPListData->pIfnameBuffers[noOfActivePDP];

            ++noOfActivePDP;
        }
    }

    return noOfActivePDP;
}

void CTE::CompleteDataCallListChanged()
{
    RIL_LOG_VERBOSE("CTE::CompleteDataCallListChanged() - Enter\r\n");

    int noOfActivePDP = 0;

    P_ND_PDP_CONTEXT_DATA pPDPListData =
                (P_ND_PDP_CONTEXT_DATA)malloc(sizeof(S_ND_PDP_CONTEXT_DATA));
    if (NULL == pPDPListData)
    {
        RIL_LOG_CRITICAL("CTE::CompleteDataCallListChanged() -"
                " Could not allocate memory for a P_ND_PDP_CONTEXT_DATA struct.\r\n");
        return;
    }
    memset(pPDPListData, 0, sizeof(S_ND_PDP_CONTEXT_DATA));

    noOfActivePDP = GetActiveDataCallInfoList(pPDPListData);
    if (noOfActivePDP > 0)
    {
        RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED,
                                                        (void*) pPDPListData,
                            noOfActivePDP * sizeof(RIL_Data_Call_Response_v6));
    }
    else
    {
        RIL_onUnsolicitedResponse (RIL_UNSOL_DATA_CALL_LIST_CHANGED, NULL, 0);
    }

    free(pPDPListData);
    pPDPListData = NULL;

    RIL_LOG_VERBOSE("CTE::CompleteDataCallListChanged() - Exit\r\n");
}

BOOL CTE::DataConfigDown(UINT32 uiCID, BOOL bForceCleanup)
{
    RIL_LOG_VERBOSE("CTE::DataConfigDown() - Enter / Exit\r\n");
    return m_pTEBaseInstance->DataConfigDown(uiCID, bForceCleanup);
}

void CTE::CleanupAllDataConnections()
{
    RIL_LOG_VERBOSE("CTE::CleanupAllDataConnections() - Enter / Exit\r\n");

    if (!TestAndSetDataCleanupStatus(TRUE))
    {
        m_pTEBaseInstance->CleanupAllDataConnections();
        TestAndSetDataCleanupStatus(FALSE);
    }
}

BOOL CTE::IsPlatformShutDownRequested()
{
    RIL_LOG_VERBOSE("CTE::IsPlatformShutDownRequested() - Enter\r\n");

    static BOOL s_bIsShutDownRequested = FALSE;

    if (!s_bIsShutDownRequested)
    {
        char szShutdownActionProperty[PROPERTY_VALUE_MAX] = {'\0'};

        // Retrieve the shutdown property
        if (property_get("sys.shutdown.requested", szShutdownActionProperty, NULL)
                && (('0' == szShutdownActionProperty[0]) || ('1' == szShutdownActionProperty[0])))
        {
            s_bIsShutDownRequested = TRUE;
        }
    }

    return s_bIsShutDownRequested;
}

void CTE::CompleteIdenticalRequests(UINT32 uiChannelId, int reqID,
                                        UINT32 uiResultCode,
                                        void* pResponse,
                                        size_t responseLen,
                                        int callId)
{
    RIL_LOG_VERBOSE("CTE::CompleteIdenticalRequests() - Enter\r\n");

    if (uiChannelId < RIL_CHANNEL_MAX)
    {
        CChannel* pChannel = g_pRilChannel[uiChannelId];
        if (NULL != pChannel)
        {
            pChannel->FindIdenticalRequestsAndSendResponses(reqID, uiResultCode,
                    pResponse, responseLen, callId);
        }
    }
    RIL_LOG_VERBOSE("CTE::CompleteIdenticalRequests() - Exit\r\n");
}

void CTE::SaveCEER(const char* pszData)
{
    CopyStringNullTerminate(m_szLastCEER, pszData, 255);
}

void CTE::SaveNetworkData(LAST_NETWORK_DATA_ID id, const char* pszData)
{
    if (id >= LAST_NETWORK_DATA_COUNT)
    {
        return;
    }

    CopyStringNullTerminate(m_szLastNetworkData[id], pszData,
            MAX_NETWORK_DATA_SIZE);
}

RIL_RESULT_CODE CTE::ParseDeactivateAllDataCalls(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseDeactivateAllDataCalls() - Enter / Exit\r\n");
    return m_pTEBaseInstance->ParseDeactivateAllDataCalls(rRspData);
}

RIL_RESULT_CODE CTE::CreateIMSRegistrationReq(REQUEST_DATA& rReqData,
        const char** ppszRequest,
        const UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE::CreateIMSRegistrationReq() - Enter/Exit\r\n");
    return m_pTEBaseInstance->CreateIMSRegistrationReq(rReqData,
                    ppszRequest, uiDataSize);
}

RIL_RESULT_CODE CTE::CreateIMSConfigReq(REQUEST_DATA& rReqData,
        const char** ppszRequest,
        const int nNumStrings)
{
    RIL_LOG_VERBOSE("CTE::CreateIMSConfigReq() - Enter/Exit\r\n");
    return m_pTEBaseInstance->CreateIMSConfigReq(rReqData,
                    ppszRequest, nNumStrings);
}

void CTE::PostReadDefaultPDNContextParams(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostReadDefaultPDNContextParams - Enter/Exit \r\n");

    if (RIL_E_SUCCESS == rData.uiResultCode)
    {
        CChannel_Data* pChannelData =
                CChannel_Data::GetChnlFromContextID(m_uiDefaultPDNCid);
        if (NULL != pChannelData)
        {
            pChannelData->SetDataState(E_DATA_STATE_ACTIVATING);
        }

        /*
         * In case of LTE, actual data registration state is signalled only after default PDN
         * context parameters are read. So, in order to force the framework to query the data
         * registration state, RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED needs to be
         * completed.
         */
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED, NULL, 0);
    }
}

RIL_RESULT_CODE CTE::ParseSetupDefaultPDN(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSetupDefaultPDN() - Enter / Exit\r\n");

    return m_pTEBaseInstance->ParseSetupDefaultPDN(rRspData);
}

void CTE::PostSetupDefaultPDN(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostSetupDefaultPDN - Enter/Exit \r\n");
    m_pTEBaseInstance->PostSetupDefaultPDN(rData);
}

void CTE::HandleChannelsBasicInitComplete()
{
    RIL_LOG_VERBOSE("CTE::HandleChannelsBasicInitComplete() - Enter/Exit\r\n");
    m_pTEBaseInstance->HandleChannelsBasicInitComplete();
}

RIL_RESULT_CODE CTE::ParseSimStateQuery(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseSimStateQuery() - Enter/Exit\r\n");
    return m_pTEBaseInstance->ParseSimStateQuery(rRspData);
}

void CTE::HandleChannelsUnlockInitComplete()
{
    RIL_LOG_VERBOSE("CTE::HandleChannelsUnlockInitComplete() - Enter\r\n");
    m_pTEBaseInstance->HandleChannelsUnlockInitComplete();
    RIL_LOG_VERBOSE("CTE::HandleChannelsUnlockInitComplete() - Exit\r\n");
}

void CTE::TriggerQuerySimSmsStoreStatus()
{
    RIL_LOG_VERBOSE("CTE::TriggerQuerySimSmsStoreStatus() - Enter\r\n");
    m_pTEBaseInstance->QuerySimSmsStoreStatus();
    RIL_LOG_VERBOSE("CTE::TriggerQuerySimSmsStoreStatus() - Exit\r\n");
}

void CTE::CompleteGetSimStatusRequest(RIL_Token hRilToken)
{
    RIL_LOG_VERBOSE("CTE::CompleteGetSimStatusRequest() - Enter\r\n");

    RIL_CardStatus_v6 cardStatus;
    int sim_state = m_pTEBaseInstance->GetSIMState();

    // Fill in the default values
    cardStatus.gsm_umts_subscription_app_index = -1;
    cardStatus.cdma_subscription_app_index = -1;
    cardStatus.ims_subscription_app_index = -1;
    cardStatus.universal_pin_state = RIL_PINSTATE_UNKNOWN;

    if (RRIL_SIM_STATE_ABSENT == sim_state)
    {
        cardStatus.card_state = RIL_CARDSTATE_ABSENT;
        cardStatus.num_applications = 0;
    }
    else if (RRIL_SIM_STATE_READY == sim_state
            || RRIL_SIM_STATE_NOT_READY == sim_state)
    {
        cardStatus.card_state = RIL_CARDSTATE_PRESENT;
        cardStatus.num_applications = 1;
        cardStatus.gsm_umts_subscription_app_index = 0;
        cardStatus.applications[0].app_type = RIL_APPTYPE_UNKNOWN;
        cardStatus.applications[0].app_state = RIL_APPSTATE_UNKNOWN;
        cardStatus.applications[0].perso_substate = RIL_PERSOSUBSTATE_UNKNOWN;
        cardStatus.applications[0].aid_ptr = NULL;
        cardStatus.applications[0].app_label_ptr = NULL;
        cardStatus.applications[0].pin1_replaced = 0;
        cardStatus.applications[0].pin1 = RIL_PINSTATE_UNKNOWN;
        cardStatus.applications[0].pin2 = RIL_PINSTATE_UNKNOWN;
#if defined(M2_PIN_RETRIES_FEATURE_ENABLED)
        cardStatus.applications[0].pin1_num_retries = -1;
        cardStatus.applications[0].puk1_num_retries = -1;
        cardStatus.applications[0].pin2_num_retries = -1;
        cardStatus.applications[0].puk2_num_retries = -1;
#endif // M2_PIN_RETRIES_FEATURE_ENABLE
    }
    else
    {
        cardStatus.card_state = RIL_CARDSTATE_ERROR;
        cardStatus.num_applications = 0;
    }

    RIL_onRequestComplete(hRilToken, RIL_E_SUCCESS, &cardStatus,
            sizeof(RIL_CardStatus_v6));

    RIL_LOG_VERBOSE("CTE::CompleteGetSimStatusRequest() - Exit\r\n");
}

// RIL_REQUEST_SET_UNSOL_CELL_INFO_LIST_RATE 110
RIL_RESULT_CODE CTE::ParseUnsolCellInfoListRate(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE::ParseGetCellInfoListRate() - Enter / Exit\r\n");
    return m_pTEBaseInstance->ParseUnsolCellInfoListRate(rRspData);
}

void CTE::PostUnsolCellInfoListRate(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE::PostUnsolCellInfoListRate() - Enter / Exit\r\n");
    // restart the timer in case of error.
    if (rData.uiResultCode == RRIL_RESULT_ERROR)
    {
        m_pTEBaseInstance->RestartUnsolCellInfoListTimer(m_nCellInfoListRate);
    }
}
