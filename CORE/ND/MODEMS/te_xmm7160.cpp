////////////////////////////////////////////////////////////////////////////
// te_xmm7160.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Overlay for the IMC 7160 modem
//
/////////////////////////////////////////////////////////////////////////////

#include <wchar.h>
#include <cutils/properties.h>
#include <sys/system_properties.h>

//  This is for socket-related calls.
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/gsmmux.h>

#include "types.h"
#include "nd_structs.h"
#include "util.h"
#include "extract.h"
#include "rillog.h"
#include "te.h"
#include "sync_ops.h"
#include "command.h"
#include "te_xmm7160.h"
#include "rildmain.h"
#include "callbacks.h"
#include "oemhookids.h"
#include "repository.h"
#include "reset.h"
#include "data_util.h"
#include "init7160.h"

CTE_XMM7160::CTE_XMM7160(CTE& cte)
: CTE_XMM6360(cte)
{
    m_cte.SetDefaultPDNCid(DEFAULT_PDN_CID);
}

CTE_XMM7160::~CTE_XMM7160()
{
}

CInitializer* CTE_XMM7160::GetInitializer()
{
    RIL_LOG_VERBOSE("CTE_XMM7160::GetInitializer() - Enter\r\n");
    CInitializer* pRet = NULL;

    RIL_LOG_INFO("CTE_XMM7160::GetInitializer() - Creating CInit7160 initializer\r\n");
    m_pInitializer = new CInit7160();
    if (NULL == m_pInitializer)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::GetInitializer() - Failed to create a CInit7160 "
                "initializer!\r\n");
        goto Error;
    }

    pRet = m_pInitializer;

Error:
    RIL_LOG_VERBOSE("CTE_XMM7160::GetInitializer() - Exit\r\n");
    return pRet;
}

const char* CTE_XMM7160::GetRegistrationInitString()
{
    return "+CREG=3|+XREG=3|+CEREG=3";
}

const char* CTE_XMM7160::GetPsRegistrationReadString()
{
    if (m_cte.IsEPSRegistered())
    {
        return "AT+CEREG=3;+CEREG?;+CEREG=0\r";
    }
    else
    {
        return "AT+XREG=3;+XREG?;+XREG=0\r";
    }
}

const char* CTE_XMM7160::GetScreenOnString()
{
    return "AT+CREG=3;+CGREG=0;+XREG=3;+CEREG=3;+XCSQ=1\r";
}

const char* CTE_XMM7160::GetScreenOffString()
{
    if (m_cte.IsLocationUpdatesEnabled())
    {
        return "AT+CGREG=1;+CEREG=1;+XREG=0;+XCSQ=0\r";
    }
    else
    {
        return "AT+CREG=1;+CGREG=1;+CEREG=1;+XREG=0;+XCSQ=0\r";
    }
}

//
// RIL_REQUEST_DATA_REGISTRATION_STATE 21
//
RIL_RESULT_CODE CTE_XMM7160::CoreGPRSRegistrationState(REQUEST_DATA& rReqData,
        void* pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CoreGPRSRegistrationState() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, GetPsRegistrationReadString(),
            sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::CoreGPRSRegistrationState() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7160::ParseGPRSRegistrationState(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseGPRSRegistrationState() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* pszRsp = rRspData.szResponse;
    const char* pszDummy;

    S_ND_GPRS_REG_STATUS psRegStatus;
    P_ND_GPRS_REG_STATUS pGPRSRegStatus = NULL;

    pGPRSRegStatus = (P_ND_GPRS_REG_STATUS)malloc(sizeof(S_ND_GPRS_REG_STATUS));
    if (NULL == pGPRSRegStatus)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseGPRSRegistrationState() -"
                " Could not allocate memory for a S_ND_GPRS_REG_STATUS struct.\r\n");
        goto Error;
    }
    memset(pGPRSRegStatus, 0, sizeof(S_ND_GPRS_REG_STATUS));

    if (FindAndSkipString(pszRsp, "+XREG: ", pszDummy))
    {
        if (!m_cte.ParseXREG(pszRsp, FALSE, psRegStatus))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::ParseGPRSRegistrationState() - "
                    "ERROR in parsing XREG response.\r\n");
            goto Error;
        }

        m_cte.StoreRegistrationInfo(&psRegStatus, E_REGISTRATION_TYPE_XREG);
    }
    else if (FindAndSkipString(pszRsp, "+CEREG: ", pszDummy))
    {
        if (!m_cte.ParseCEREG(pszRsp, FALSE, psRegStatus))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::ParseGPRSRegistrationState() - "
                    "ERROR in parsing CEREG response.\r\n");
            goto Error;
        }

        m_cte.StoreRegistrationInfo(&psRegStatus, E_REGISTRATION_TYPE_CEREG);
    }

    m_cte.CopyCachedRegistrationInfo(pGPRSRegStatus, TRUE);

    rRspData.pData  = (void*)pGPRSRegStatus;
    rRspData.uiDataSize = sizeof(S_ND_GPRS_REG_STATUS_POINTERS);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pGPRSRegStatus);
        pGPRSRegStatus = NULL;
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::ParseGPRSRegistrationState() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DEACTIVATE_DATA_CALL 41
//
RIL_RESULT_CODE CTE_XMM7160::CoreDeactivateDataCall(REQUEST_DATA& rReqData,
                                                                void* pData,
                                                                UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CoreDeactivateDataCall() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char* pszCid = NULL;
    UINT32 uiCID = 0;
    const LONG REASON_RADIO_OFF = 1;
    LONG reason = 0;

    if (uiDataSize < (1 * sizeof(char *)))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CoreDeactivateDataCall() -"
                " Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CoreDeactivateDataCall() -"
                " Passed data pointer was NULL\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_XMM7160::CoreDeactivateDataCall() - uiDataSize=[%d]\r\n", uiDataSize);

    pszCid = ((char**)pData)[0];
    if (pszCid == NULL || '\0' == pszCid[0])
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CoreDeactivateDataCall() - pszCid was NULL\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_XMM7160::CoreDeactivateDataCall() - pszCid=[%s]\r\n", pszCid);

    //  Get CID as UINT32.
    if (sscanf(pszCid, "%u", &uiCID) == EOF)
    {
        // Error
        RIL_LOG_CRITICAL("CTE_XMM7160::CoreDeactivateDataCall() -  cannot convert %s to int\r\n",
                pszCid);
        goto Error;
    }

    if ((RIL_VERSION >= 4) && (uiDataSize >= (2 * sizeof(char *))))
    {
        reason == GetDataDeactivateReason(((char**)pData)[1]);
        RIL_LOG_INFO("CTE_XMM7160::CoreDeactivateDataCall() - reason=[%ld]\r\n", reason);
    }

    if (reason == REASON_RADIO_OFF || RRIL_SIM_STATE_ABSENT == GetSIMState())
    {
        // complete the request without sending the AT command to modem.
        res = RRIL_RESULT_OK_IMMEDIATE;
        DataConfigDown(uiCID, TRUE);
        goto Error;
    }

    if (m_cte.IsEPSRegistered() && uiCID == DEFAULT_PDN_CID)
    {
        char* szModemResourceName = {'\0'};
        int muxControlChannel = -1;
        int hsiChannel = -1;
        int ipcDataChannelMin = 0;
        UINT32 uiRilChannel = 0;
        CCommand* pCmd = NULL;
        CChannel_Data* pChannelData = NULL;
        UINT32* pCID = NULL;

        pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
        if (NULL == pChannelData)
        {
            RIL_LOG_INFO("CTE_XMM7160::CoreDeactivateDataCall() -"
                    " No Data Channel for CID %u.\r\n", uiCID);
            goto Error;
        }

        uiRilChannel = pChannelData->GetRilChannel();
        hsiChannel = pChannelData->GetHSIChannel();

        muxControlChannel = pChannelData->GetMuxControlChannel();
        if (-1 == muxControlChannel)
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::CoreDeactivateDataCall() - Unknown mux channel\r\n");
            goto Error;
        }

        szModemResourceName = pChannelData->GetModemResourceName();
        ipcDataChannelMin = pChannelData->GetIpcDataChannelMin();

        if (ipcDataChannelMin > hsiChannel || RIL_MAX_NUM_IPC_CHANNEL <= hsiChannel )
        {
           RIL_LOG_CRITICAL("CTE_XMM7160::CoreDeactivateDataCall() - Unknown HSI Channel [%d] \r\n",
                    hsiChannel);
           goto Error;
        }

        memset(&rReqData, 0, sizeof(REQUEST_DATA));

        // When context is deactivated, disable the routing to avoid modem waking up AP
        if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
                "AT+XDATACHANNEL=0,1,\"/mux/%d\",\"/%s/%d\",0,%d\r",
                muxControlChannel, szModemResourceName, hsiChannel, uiCID))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::CoreDeactivateDataCall() - cannot create XDATACHANNEL"
                    "command\r\n");
            goto Error;
        }

        pCID = (UINT32*)malloc(sizeof(UINT32));
        if (NULL == pCID)
        {
            goto Error;
        }

        *pCID = uiCID;

        rReqData.pContextData = (void*)pCID;
        rReqData.cbContextData = sizeof(UINT32);
        res = RRIL_RESULT_OK;
    }
    else
    {
        res = CTE_XMM6260::CoreDeactivateDataCall(rReqData, pData, uiDataSize);
    }

Error:
    RIL_LOG_VERBOSE("CTE_XMM7160::CoreDeactivateDataCall() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
//
RIL_RESULT_CODE CTE_XMM7160::CoreSetPreferredNetworkType(REQUEST_DATA& rReqData,
        void* pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CoreSetPreferredNetworkType() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_PreferredNetworkType networkType = PREF_NET_TYPE_LTE_GSM_WCDMA; //9

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CoreSetPreferredNetworkType() - Data "
                "pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != sizeof(RIL_PreferredNetworkType*))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CoreSetPreferredNetworkType() - "
                "Invalid data size.\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_XMM7160::CoreSetPreferredNetworkType() - "
               "Network type {%d} from framework.\r\n", ((RIL_PreferredNetworkType*)pData)[0]);

    networkType = ((RIL_PreferredNetworkType*)pData)[0];

    // if network type already set, NO-OP this command
    if (m_currentNetworkType == networkType)
    {
        rReqData.szCmd1[0] = '\0';
        res = RRIL_RESULT_OK;
        RIL_LOG_INFO("CTE_XMM7160::CoreSetPreferredNetworkType() - "
                "Network type {%d} already set.\r\n", networkType);
        goto Error;
    }

    switch (networkType)
    {
        case PREF_NET_TYPE_GSM_WCDMA: // WCDMA Preferred
            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT=3,1\r",
                    sizeof(rReqData.szCmd1) ))
            {
                RIL_LOG_CRITICAL("CTE_XMM7160::HandleNetworkType() - Can't "
                        "construct szCmd1 networkType=%d\r\n", networkType);
                break;
            }
            res = RRIL_RESULT_OK;
            break;

        case PREF_NET_TYPE_GSM_ONLY: // GSM Only
            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT=0\r",
                    sizeof(rReqData.szCmd1) ))
            {
                RIL_LOG_CRITICAL("CTE_XMM7160::HandleNetworkType() - Can't "
                        "construct szCmd1 networkType=%d\r\n", networkType);
                break;
            }
            res = RRIL_RESULT_OK;
            break;

        case PREF_NET_TYPE_WCDMA: // WCDMA Only
            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT=1\r",
                    sizeof(rReqData.szCmd1)))
            {
                RIL_LOG_CRITICAL("CTE_XMM7160::HandleNetworkType() - Can't "
                        "construct szCmd1 networkType=%d\r\n", networkType);
                break;
            }
            res = RRIL_RESULT_OK;
            break;

        case PREF_NET_TYPE_LTE_ONLY: // LTE Only
            RIL_LOG_VERBOSE("CTE_XMM7160::CoreSetPreferredNetworkType(XACT=2) - Enter\r\n");
            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT=2\r",
                    sizeof(rReqData.szCmd1)))
            {
                RIL_LOG_CRITICAL("CTE_XMM7160::CoreSetPreferredNetworkType() - "
                        "Can't construct szCmd1 networkType=%d\r\n", networkType);
                goto Error;
            }
            break;

        /*
         * PREF_NET_TYPE_GSM_WCDMA_CDMA_EVDO_AUTO value is received as a result
         * of the recovery mechanism in the framework.
         */
        case PREF_NET_TYPE_LTE_GSM_WCDMA: // LTE Preferred
        case PREF_NET_TYPE_GSM_WCDMA_CDMA_EVDO_AUTO:
            RIL_LOG_VERBOSE("CTE_XMM7160::CoreSetPreferredNetworkType(XACT=6,2,1) - Enter\r\n");
            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT=6,2,1\r",
                    sizeof(rReqData.szCmd1)))
            {
                RIL_LOG_CRITICAL("CTE_XMM7160::CoreSetPreferredNetworkType() - "
                    "Can't construct szCmd1 networkType=%d\r\n", networkType);
                goto Error;
            }
            break;

        case PREF_NET_TYPE_LTE_WCDMA: // LTE Preferred
            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT=4,2\r",
                    sizeof(rReqData.szCmd1)))
            {
                RIL_LOG_CRITICAL("CTE_XMM7160::HandleNetworkType() - Can't "
                        "construct szCmd1 networkType=%d\r\n", networkType);
                goto Error;
            }
            break;

        default:
            RIL_LOG_CRITICAL("CTE_XMM7160::CoreSetPreferredNetworkType() - "
                    "Undefined rat code: %d\r\n", networkType);
            res = RIL_E_MODE_NOT_SUPPORTED;
            goto Error;
    }

    //  Set the context of this command to the network type we're attempting to set
    rReqData.pContextData = (void*)networkType;  // Store this as an int.

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTE_XMM7160::CoreSetPreferredNetworkType() - Exit\r\n");
    return res;
}

// RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
//
RIL_RESULT_CODE CTE_XMM7160::CoreGetPreferredNetworkType(REQUEST_DATA& rReqData,
        void* pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CoreGetPreferredNetworkType() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT?\r",
            sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::CoreGetPreferredNetworkType() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7160::ParseGetPreferredNetworkType(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseGetPreferredNetworkType() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* pszRsp = rRspData.szResponse;

    UINT32 rat = 0;
    UINT32 pref = 0;

    int* pRat = (int*)malloc(sizeof(int));
    if (NULL == pRat)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseGetPreferredNetworkType() - Could "
                "not allocate memory for response.\r\n");
        goto Error;
    }

    // Skip "<prefix>"
    if (!SkipRspStart(pszRsp, m_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseGetPreferredNetworkType() - Could "
                "not skip response prefix.\r\n");
        goto Error;
    }

    // Skip "+XACT: "
    if (!SkipString(pszRsp, "+XACT: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseGetPreferredNetworkType() - Could "
                "not skip \"+XACT: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt32(pszRsp, rat, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseGetPreferredNetworkType() - Could "
                "not extract rat value.\r\n");
        goto Error;
    }

    if (FindAndSkipString(pszRsp, ",", pszRsp))
    {
        if (!ExtractUInt32(pszRsp, pref, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::ParseGetPreferredNetworkType() - "
                    "Could not find and skip pref value even though it was expected.\r\n");
            goto Error;
        }
    }

    switch (rat)
    {
        case 0:     // GSM Only
        {
            pRat[0] = PREF_NET_TYPE_GSM_ONLY;
            m_currentNetworkType = PREF_NET_TYPE_GSM_ONLY;
            break;
        }

        case 1:     // WCDMA Only
        {
            pRat[0] = PREF_NET_TYPE_WCDMA;
            m_currentNetworkType = PREF_NET_TYPE_WCDMA;
            break;
        }

        case 2:     // LTE only
        {
            pRat[0] = PREF_NET_TYPE_LTE_ONLY;
            m_currentNetworkType = PREF_NET_TYPE_LTE_ONLY;
            break;
        }

        case 3:     // WCDMA preferred
        {
            pRat[0] = PREF_NET_TYPE_GSM_WCDMA;
            m_currentNetworkType = PREF_NET_TYPE_GSM_WCDMA;
            break;
        }

        case 4:     // LTE/WCDMA, LTE preferred
        {
            pRat[0] = PREF_NET_TYPE_LTE_WCDMA;
            m_currentNetworkType = PREF_NET_TYPE_LTE_WCDMA;
            break;
        }

        case 6:     // LTE/WCDMA/GSM, LTE preferred
        {
            pRat[0] = PREF_NET_TYPE_LTE_GSM_WCDMA;
            m_currentNetworkType = PREF_NET_TYPE_LTE_GSM_WCDMA;
            break;
        }

        default:
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::ParseGetPreferredNetworkType() - "
                    "Unexpected rat found: %d. Failing out.\r\n", rat);
            goto Error;
        }
    }

    rRspData.pData  = (void*)pRat;
    rRspData.uiDataSize = sizeof(int*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pRat);
        pRat = NULL;
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::ParseGetPreferredNetworkType() - Exit\r\n");
    return res;
}

BOOL CTE_XMM7160::IMSRegister(REQUEST_DATA& rReqData, void* pData,
        UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::IMSRegister() - Enter\r\n");

    BOOL bRet = FALSE;

    if ((NULL == pData) || (sizeof(int) != uiDataSize))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::IMSRegister() - Invalid input data\r\n");
        return bRet;
    }

    int* pService = (int*)pData;

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
                "AT+XIREG=%d\r", *pService))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::IMSRegister() - Can't construct szCmd1.\r\n");
        return bRet;
    }

    int temp = 0;
    const int DEFAULT_XIREG_TIMEOUT = 180000;
    CRepository repository;

    if (repository.Read(g_szGroupOtherTimeouts, g_szTimeoutWaitForXIREG, temp))
    {
        rReqData.uiTimeout = temp;
    }
    else
    {
        rReqData.uiTimeout = DEFAULT_XIREG_TIMEOUT;
    }

    bRet = TRUE;

    return bRet;
}

RIL_RESULT_CODE CTE_XMM7160::ParseIMSRegister(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseIMSRegister() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_OK;
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseIMSRegister() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7160::CreateIMSRegistrationReq(REQUEST_DATA& rReqData,
        const char** pszRequest,
        const UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CreateIMSRegistrationReq() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (pszRequest == NULL || '\0' == pszRequest[1])
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSRegistrationReq() - pszRequest was empty\r\n");
        goto Error;
    }

    if (uiDataSize < (2 * sizeof(char*)))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSRegistrationReq() :"
                " received_size < required_size\r\n");
        goto Error;
    }

    int service;
    if (sscanf(pszRequest[1], "%d", &service) == EOF)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSRegistrationReq() -"
                " cannot convert %s to int\r\n", pszRequest);
        goto Error;
    }

    if ((service < 0) || (service > 1))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSRegistrationReq() -"
                " service %s out of boundaries\r\n", service);
        goto Error;
    }

    RIL_LOG_INFO("CTE_XMM7160::CreateIMSRegistrationReq() - service=[%d]\r\n", service);

    if (!IMSRegister(rReqData, &service, sizeof(int)))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSRegistrationReq() - Can't construct szCmd1.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;
Error:
    RIL_LOG_VERBOSE("CTE_XMM7160::CreateIMSRegistrationReq() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7160::CreateIMSConfigReq(REQUEST_DATA& rReqData,
        const char** pszRequest,
        const int nNumStrings)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CreateIMSConfigReq() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char szXicfgCmd[MAX_BUFFER_SIZE] = {'\0'};
    int xicfgParams[XICFG_N_PARAMS] = {0};

    if (pszRequest == NULL)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() - pszRequest was empty\r\n");
        return res;
    }

    // There should be XICFG_N_PARAMS parameters and the request ID
    if (nNumStrings < (XICFG_N_PARAMS + 1))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() :"
                " received_size < required_size\r\n");
        return res;
    }

    xicfgParams[0] = E_XICFG_IMS_APN;
    xicfgParams[1] = E_XICFG_PCSCF_ADDRESS;
    xicfgParams[2] = E_XICFG_PCSCF_PORT;
    xicfgParams[3] = E_XICFG_IMS_AUTH_MODE;
    xicfgParams[4] = E_XICFG_PHONE_CONTEXT;
    xicfgParams[5] = E_XICFG_LOCAL_BREAKOUT;
    xicfgParams[6] = E_XICFG_XCAP_APN;
    xicfgParams[7] = E_XICFG_XCAP_ROOT_URI;
    xicfgParams[8] = E_XICFG_XCAP_USER_NAME;
    xicfgParams[9] = E_XICFG_XCAP_USER_PASSWORD;

    char szTemp1Xicfg[MAX_BUFFER_SIZE] = {'\0'};
    char szTemp2Xicfg[MAX_BUFFER_SIZE] = {'\0'};
    int nParams = 0;

    for (int i = 1; i <= XICFG_N_PARAMS; i++)
    {
        if ((pszRequest[i] != NULL) && (0 != strncmp(pszRequest[i], "default", 7)))
        {   // The XICFG parameter is a numeric hence "" not required.
            if (xicfgParams[i - 1] == E_XICFG_PCSCF_PORT ||
                xicfgParams[i - 1] == E_XICFG_IMS_AUTH_MODE ||
                xicfgParams[i - 1] == E_XICFG_LOCAL_BREAKOUT)
            {
                if (!PrintStringNullTerminate(szTemp1Xicfg,
                                             MAX_BUFFER_SIZE,",%d,%s",
                                             xicfgParams[i - 1], pszRequest[i]))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() - Can't add %s.\r\n",
                                     pszRequest[i]);
                    goto Error;
                }
            }
            else
            {   // The XICFG parameter is a string hence "" required.
                if (!PrintStringNullTerminate(szTemp1Xicfg,
                                              MAX_BUFFER_SIZE,",%d,\"%s\"",
                                              xicfgParams[i - 1], pszRequest[i]))
                {
                    RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() - Can't add %s.\r\n",
                                     pszRequest[i]);
                    goto Error;
                }
            }
            if (!ConcatenateStringNullTerminate(szTemp2Xicfg,
                                                MAX_BUFFER_SIZE - strlen(szTemp2Xicfg),
                                                szTemp1Xicfg))
            {
                RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() - Can't add %s.\r\n",
                                 szTemp1Xicfg);
                goto Error;
            }
            nParams++;
        }
    }

    if (nParams == 0)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() - nParams=0\r\n");
        goto Error;
    }

    if (!ConcatenateStringNullTerminate(szTemp2Xicfg, MAX_BUFFER_SIZE - strlen(szTemp2Xicfg),
                                        "\r"))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() - Can't add %s.\r\n",
                         "\r");
        goto Error;
    }

    if (!PrintStringNullTerminate(szXicfgCmd,
            MAX_BUFFER_SIZE,"AT+XICFG=%d,%d", XICFG_SET, nParams))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() - Can't construct szCmd1.\r\n");
        goto Error;
    }

    if (!ConcatenateStringNullTerminate(szXicfgCmd, MAX_BUFFER_SIZE - strlen(szXicfgCmd),
                                        szTemp2Xicfg))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() - Can't construct szCmd1.\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_XMM7160::CreateIMSConfigReq() - IMS_APN=[%s]\r\n",
                 szXicfgCmd);

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), szXicfgCmd))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() - Can't construct szCmd1.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;
Error:
    RIL_LOG_VERBOSE("CTE_XMM7160::CreateIMSConfigReq() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7160::HandleSetupDefaultPDN(RIL_Token rilToken,
        CChannel_Data* pChannelData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::HandleSetupDefaultPDN() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char* szModemResourceName = {'\0'};
    int muxControlChannel = -1;
    int hsiChannel = pChannelData->GetHSIChannel();
    int ipcDataChannelMin = 0;
    UINT32 uiRilChannel = pChannelData->GetRilChannel();
    REQUEST_DATA reqData;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;
    CCommand* pCmd = NULL;
    int dataState = pChannelData->GetDataState();

    pDataCallContextData =
            (S_SETUP_DATA_CALL_CONTEXT_DATA*)malloc(sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA));
    if (NULL == pDataCallContextData)
    {
        goto Error;
    }

    pDataCallContextData->uiCID = pChannelData->GetContextID();

    // Get the mux channel id corresponding to the control of the data channel
    switch (uiRilChannel)
    {
        case RIL_CHANNEL_DATA1:
            sscanf(g_szDataPort1, "/dev/gsmtty%d", &muxControlChannel);
            break;
        case RIL_CHANNEL_DATA2:
            sscanf(g_szDataPort2, "/dev/gsmtty%d", &muxControlChannel);
            break;
        case RIL_CHANNEL_DATA3:
            sscanf(g_szDataPort3, "/dev/gsmtty%d", &muxControlChannel);
            break;
        case RIL_CHANNEL_DATA4:
            sscanf(g_szDataPort4, "/dev/gsmtty%d", &muxControlChannel);
            break;
        case RIL_CHANNEL_DATA5:
            sscanf(g_szDataPort5, "/dev/gsmtty%d", &muxControlChannel);
            break;
        default:
            RIL_LOG_CRITICAL("CTE_XMM7160::HandleSetupDefaultPDN() - Unknown mux channel"
                    "for RIL Channel [%u] \r\n", uiRilChannel);
            goto Error;
    }

    szModemResourceName = pChannelData->GetModemResourceName();
    ipcDataChannelMin = pChannelData->GetIpcDataChannelMin();

    if (ipcDataChannelMin > hsiChannel || RIL_MAX_NUM_IPC_CHANNEL <= hsiChannel )
    {
       RIL_LOG_CRITICAL("CTE_XMM7160::HandleSetupDefaultPDN() - Unknown HSI Channel [%d] \r\n",
                hsiChannel);
       goto Error;
    }

    memset(&reqData, 0, sizeof(REQUEST_DATA));

    if (E_DATA_STATE_ACTIVATING == dataState)
    {
        /*
         * ACTIVATING means that context is up but the routing is not enabled and also channel
         * is not configured for Data.
         */
        if (!PrintStringNullTerminate(reqData.szCmd1, sizeof(reqData.szCmd1),
                "AT+XDATACHANNEL=1,1,\"/mux/%d\",\"/%s/%d\",0,%d;+CGDATA=\"M-RAW_IP\",%d\r",
                muxControlChannel, szModemResourceName, hsiChannel,
                pDataCallContextData->uiCID, pDataCallContextData->uiCID))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::HandleSetupDefaultPDN() - cannot create XDATACHANNEL"
                    "command\r\n");
            goto Error;
        }
    }
    else if (E_DATA_STATE_ACTIVE == dataState)
    {
        /*
         * ACTIVATE means that context is up and the channel is configured for data but the
         * routing is disabled.
         */
        if (!PrintStringNullTerminate(reqData.szCmd1, sizeof(reqData.szCmd1),
                "AT+XDATACHANNEL=1,1,\"/mux/%d\",\"/%s/%d\",0,%d\r",
                muxControlChannel, szModemResourceName, hsiChannel, pDataCallContextData->uiCID))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::HandleSetupDefaultPDN() - cannot create XDATACHANNEL"
                    "command\r\n");
            goto Error;
        }
    }

    pCmd = new CCommand(uiRilChannel, rilToken, ND_REQ_ID_NONE, reqData,
            &CTE::ParseSetupDefaultPDN, &CTE::PostSetupDefaultPDN);

    if (pCmd)
    {
        pCmd->SetContextData(pDataCallContextData);
        pCmd->SetContextDataSize(sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA));

        if (!CCommand::AddCmdToQueue(pCmd))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::HandleSetupDefaultPDN() - "
                    "Unable to add command to queue\r\n");
            delete pCmd;
            pCmd = NULL;
            goto Error;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::HandleSetupDefaultPDN() -"
                " Unable to allocate memory for command\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pDataCallContextData);
        pDataCallContextData = NULL;
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::HandleSetupDefaultPDN() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7160::ParseSetupDefaultPDN(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseSetupDefaultPDN() - Enter\r\n");

    RIL_LOG_VERBOSE("CTE_XMM7160::ParseSetupDefaultPDN() - Exit\r\n");
    return RRIL_RESULT_OK;
}

void CTE_XMM7160::PostSetupDefaultPDN(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::PostSetupDefaultPDN() Enter\r\n");

    BOOL bSuccess = FALSE;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;
    UINT32 uiCID = 0;
    CChannel_Data* pChannelData = NULL;

    if (NULL == rData.pContextData ||
            sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA) != rData.uiContextDataSize)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::PostSetupDefaultPDN() - Invalid context data\r\n");
        goto Error;
    }

    pDataCallContextData = (S_SETUP_DATA_CALL_CONTEXT_DATA*)rData.pContextData;
    uiCID = pDataCallContextData->uiCID;

    if (RIL_E_SUCCESS != rData.uiResultCode)
    {
        RIL_LOG_INFO("CTE_XMM7160::PostSetupDefaultPDN() - Failure\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_XMM7160::PostSetupDefaultPDN() - CID=%d\r\n", uiCID);

    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_INFO("CTE_XMM7160::PostSetupDefaultPDN() -"
                " No Data Channel for CID %u.\r\n", uiCID);
        goto Error;
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::PostSetupDefaultPDN() set channel data\r\n");

    pChannelData->SetDataState(E_DATA_STATE_ACTIVE);

    if (!SetupInterface(uiCID))
    {
        RIL_LOG_INFO("CTE_XMM7160::PostSetupDefaultPDN() - SetupInterface failed\r\n");
        goto Error;
    }

    bSuccess = TRUE;

Error:
    free(rData.pContextData);
    rData.pContextData = NULL;

    if (!bSuccess)
    {
        HandleSetupDataCallFailure(uiCID, rData.pRilToken, rData.uiResultCode);
    }
    else
    {
        HandleSetupDataCallSuccess(uiCID, rData.pRilToken);
    }
}

//
//  Call this whenever data is disconnected
//
BOOL CTE_XMM7160::DataConfigDown(UINT32 uiCID, BOOL bForceCleanup)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::DataConfigDown() - Enter\r\n");

    //  First check to see if uiCID is valid
    if (uiCID > MAX_PDP_CONTEXTS || uiCID == 0)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::DataConfigDown() - Invalid CID = [%u]\r\n", uiCID);
        return FALSE;
    }

    CChannel_Data* pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::DataConfigDown() -"
                " Invalid CID=[%u], no data channel found!\r\n", uiCID);
        return FALSE;
    }

    pChannelData->RemoveInterface();

    if (!m_cte.IsEPSRegistered() || uiCID != DEFAULT_PDN_CID || bForceCleanup)
    {
        pChannelData->ResetDataCallInfo();
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::DataConfigDown() EXIT\r\n");
    return TRUE;
}

//
// RIL_REQUEST_SET_BAND_MODE 65
//
RIL_RESULT_CODE CTE_XMM7160::CoreSetBandMode(REQUEST_DATA& rReqData,
                                                         void* pData,
                                                         UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CoreSetBandMode() - Enter\r\n");
    // TODO: Change to +XACT usage when the modem is ready
    return CTE_XMM6260::CoreSetBandMode(rReqData, pData, uiDataSize);
}

RIL_RESULT_CODE CTE_XMM7160::ParseSetBandMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseSetBandMode() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_OK;
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseSetBandMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7160::CreateSetDefaultApnReq(REQUEST_DATA& rReqData,
        const char** pszRequest, const int numStrings)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CreateSetDefaultApnReq() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (numStrings != 3)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateSetDefaultApnReq() :"
                " received_size != required_size\r\n");
        return res;
    }

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
            "AT+CGDCONT=1,\"%s\",\"%s\"\r", pszRequest[2], pszRequest[1]))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateSetDefaultApnReq() - "
                "Can't construct szCmd1.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;
Error:
    RIL_LOG_VERBOSE("CTE_XMM7160::CreateSetDefaultApnReq() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7160::ParseGetNeighboringCellIDs(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseGetNeighboringCellIDs() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* pszRsp = rRspData.szResponse;
    UINT32 uiIndex = 0, uiMode = 0;

    P_ND_N_CELL_DATA pCellData = NULL;
    pCellData = (P_ND_N_CELL_DATA)malloc(sizeof(S_ND_N_CELL_DATA));
    if (NULL == pCellData)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::ParseGetNeighboringCellIDs() -"
                " Could not allocate memory for a S_ND_N_CELL_DATA struct.\r\n");
        goto Error;
    }

    memset(pCellData, 0, sizeof(S_ND_N_CELL_DATA));
    // Loop on +XCELLINFO until no more entries are found
    while (FindAndSkipString(pszRsp, "+XCELLINFO: ", pszRsp))
    {
        if (RRIL_MAX_CELL_ID_COUNT == uiIndex)
        {
            //  We're full.
            RIL_LOG_CRITICAL("CTE_XMM7160::ParseGetNeighboringCellIDs() -"
                    " Exceeded max count = %d\r\n", RRIL_MAX_CELL_ID_COUNT);
            break;
        }

        // Get <mode>
        if (!ExtractUInt32(pszRsp, uiMode, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_XMM7160::ParseGetNeighboringCellIDs() -"
                    " cannot extract <mode>\r\n");
            goto Error;
        }

        RIL_LOG_INFO("CTE_XMM7160::ParseGetNeighboringCellIDs() - found mode=%d\r\n",
                uiMode);
        RIL_RESULT_CODE result = RRIL_RESULT_ERROR;
        switch (uiMode)
        {
            // GSM/UMTS cases
            case 0:
            case 1:
            case 2:
            case 3:

                result = m_cte.ParseGsmUmtsNeighboringCellInfo(pCellData,
                        pszRsp, uiIndex, uiMode);
                if (result == RRIL_RESULT_OK)
                {
                    // Connect the pointer
                    pCellData->pnCellPointers[uiIndex] = &(pCellData->pnCellData[uiIndex]);
                    uiIndex++;
                    RIL_LOG_INFO("CTE_XMM7160::ParseGetNeighboringCellIDs() - Index=%d\r\n",
                            uiIndex);
                }
            break;
            // LTE Cases
            case 5:
            case 6:

                result = m_cte.ParseLteNeighboringCellInfo(pCellData,
                        pszRsp, uiIndex, uiMode);
                if (result == RRIL_RESULT_OK)
                {
                    // Connect the pointer
                    pCellData->pnCellPointers[uiIndex] = &(pCellData->pnCellData[uiIndex]);
                    uiIndex++;
                    RIL_LOG_INFO("CTE_XMM7160::ParseGetNeighboringCellIDs() - Index=%d\r\n",
                            uiIndex);
                }
            break;
            default:
            break;
        }
    }

    if (uiIndex > 0)
    {
        rRspData.pData  = (void*)pCellData;
        rRspData.uiDataSize = uiIndex * sizeof(RIL_NeighboringCell*);
    }
    else
    {
        rRspData.pData  = NULL;
        rRspData.uiDataSize = 0;
        free(pCellData);
        pCellData = NULL;
    }

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pCellData);
        pCellData = NULL;
    }


    RIL_LOG_VERBOSE("CTE_XMM7160::ParseGetNeighboringCellIDs() - Exit\r\n");
    return res;
}

