////////////////////////////////////////////////////////////////////////////
// te_inf_6260.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Overlay for the Infineon 6260 modem
//
/////////////////////////////////////////////////////////////////////////////

#include <wchar.h>

#include "types.h"
#include "nd_structs.h"
#include "util.h"
#include "extract.h"
#include "rillog.h"
#include "te.h"
#include "te_base.h"
#include "sync_ops.h"
#include "command.h"
#include "te_inf_6260.h"
#include "rildmain.h"
#include "callbacks.h"
#include "oemhookids.h"
#include "repository.h"
#include "reset.h"
#include <cutils/properties.h>
#include <sys/system_properties.h>
#include "data_util.h"
#include "rril.h"
#include "callbacks.h"

//  This is for socket-related calls.
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/gsmmux.h>


CTE_INF_6260::CTE_INF_6260()
: m_currentNetworkType(-1)
{
    m_szUICCID[0] = '\0';
}

CTE_INF_6260::~CTE_INF_6260()
{
}

//
// RIL_REQUEST_GET_SIM_STATUS 1
//
RIL_RESULT_CODE CTE_INF_6260::CoreGetSimStatus(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreGetSimStatus() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

#if defined(M2_PIN_RETRIES_FEATURE_ENABLED)
    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CPIN?;+XUICC?;+XPINCNT;+CCID\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }
#else  // M2_PIN_RETRIES_FEATURE_ENABLED
    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CPIN?;+XUICC?;+CCID\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }
#endif // M2_PIN_RETRIES_FEATURE_ENABLED

    RIL_LOG_VERBOSE("CTE_INF_6260::CoreGetSimStatus() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseGetSimStatus(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseGetSimStatus() - Enter\r\n");

    UINT32 nValue;

    RIL_CardStatus_v6* pCardStatus = NULL;
    const char* pszRsp = rRspData.szResponse;

    RIL_RESULT_CODE res = ParseSimPin(pszRsp, pCardStatus);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetSimStatus() - Could not parse Sim Pin.\r\n");
        goto Error;
    }

    if (pCardStatus->card_state != RIL_CARDSTATE_ABSENT)
    {
        // Parse "<prefix>+XUICC: <state><postfix>"
        SkipRspStart(pszRsp, m_szNewLine, pszRsp);

        if (SkipString(pszRsp, "+XUICC: ", pszRsp))
        {
            if (!ExtractUpperBoundedUInt32(pszRsp, 2, nValue, pszRsp))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetSimStatus() - Invalid SIM type.\r\n");
                pCardStatus->applications[0].app_type = RIL_APPTYPE_UNKNOWN;
            }
            else
            {
                if (1 == nValue)
                {
                    RIL_LOG_INFO("CTE_INF_6260::ParseGetSimStatus() - SIM type = %d  detected USIM\r\n", nValue);

                    //  Set to USIM
                    pCardStatus->applications[0].app_type = RIL_APPTYPE_USIM;
                }
                else if (0 == nValue)
                {
                    RIL_LOG_INFO("CTE_INF_6260::ParseGetSimStatus() - SIM type = %d  detected normal SIM\r\n", nValue);

                    //  Set to SIM
                    pCardStatus->applications[0].app_type = RIL_APPTYPE_SIM;
                }
            }

            m_nSimAppType = pCardStatus->applications[0].app_type;

            SkipRspEnd(pszRsp, m_szNewLine, pszRsp);
        }

#if defined(M2_PIN_RETRIES_FEATURE_ENABLED)
        // Parse "<prefix>+XPINCNT: <PIN attempts>, <PIN2 attempts>, <PUK attempts>, <PUK2 attempts><postfix>"
        SkipRspStart(pszRsp, m_szNewLine, pszRsp);

        if (SkipString(pszRsp, "+XPINCNT: ", pszRsp))
        {
            UINT32 uiPin1 = 0, uiPin2 = 0, uiPuk1 = 0, uiPuk2 = 0;

            if (!ExtractUInt32(pszRsp, uiPin1, pszRsp) ||
                !SkipString(pszRsp, ",", pszRsp) ||
                !ExtractUInt32(pszRsp, uiPin2, pszRsp) ||
                !SkipString(pszRsp, ",", pszRsp) ||
                !ExtractUInt32(pszRsp, uiPuk1, pszRsp) ||
                !SkipString(pszRsp, ",", pszRsp) ||
                !ExtractUInt32(pszRsp, uiPuk2, pszRsp))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetSimStatus() - Cannot parse XPINCNT\r\n");
                //  Set pin retries to -1 (unknown)
                pCardStatus->applications[0].pin1_num_retries = -1;
                pCardStatus->applications[0].puk1_num_retries = -1;
                pCardStatus->applications[0].pin2_num_retries = -1;
                pCardStatus->applications[0].puk2_num_retries = -1;
            }
            else
            {
                RIL_LOG_INFO("CTE_INF_6260::ParseGetSimStatus() - retries pin1:%d pin2:%d puk1:%d puk2:%d\r\n",
                    uiPin1, uiPin2, uiPuk1, uiPuk2);

                pCardStatus->applications[0].pin1_num_retries = uiPin1;
                pCardStatus->applications[0].puk1_num_retries = uiPuk1;
                pCardStatus->applications[0].pin2_num_retries = uiPin2;
                pCardStatus->applications[0].puk2_num_retries = uiPuk2;
            }
            SkipRspEnd(pszRsp, m_szNewLine, pszRsp);
        }

#endif // M2_PIN_RETRIES_FEATURE_ENABLED

        // Parse "<prefix>+CCID: <ICCID><postfix>"
        SkipRspStart(pszRsp, m_szNewLine, pszRsp);

        if (SkipString(pszRsp, "+CCID: ", pszRsp))
        {
            if (!ExtractUnquotedString(pszRsp, m_cTerminator, m_szUICCID, MAX_PROP_VALUE, pszRsp))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetSimStatus() - Cannot parse UICC ID\r\n");
                m_szUICCID[0] = '\0';
            }

            SkipRspEnd(pszRsp, m_szNewLine, pszRsp);
        }
    }

    res = RRIL_RESULT_OK;

    rRspData.pData   = (void*)pCardStatus;
    rRspData.uiDataSize  = sizeof(RIL_CardStatus_v6);

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pCardStatus);
        pCardStatus = NULL;
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::ParseGetSimStatus() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseEnterSimPin(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseEnterSimPin() - Enter\r\n");

    return CTEBase::ParseEnterSimPin(rRspData);
}


// RIL_REQUEST_SETUP_DATA_CALL 27
RIL_RESULT_CODE CTE_INF_6260::CoreSetupDataCall(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize, UINT32 uiCID)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSetupDataCall() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char szIPV4V6[] = "IPV4V6";
    int nPapChap = 0; // no auth
    PdpData stPdpData;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;
    CChannel_Data* pChannelData = NULL;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetupDataCall() - Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize < (6 * sizeof(char*)))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetupDataCall() - Invalid data size. Was given %d bytes\r\n", uiDataSize);
        goto Error;
    }

    RIL_LOG_INFO("CTE_INF_6260::CoreSetupDataCall() - uiDataSize=[%d]\r\n", uiDataSize);

    pDataCallContextData =
            (S_SETUP_DATA_CALL_CONTEXT_DATA*)malloc(sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA));
    if (NULL == pDataCallContextData)
    {
        goto Error;
    }

    // Get Channel Data according to CID
    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetupDataCall() - No Channel with context Id: %u\r\n", uiCID);
        goto Error;
    }

    memset(&stPdpData, 0, sizeof(PdpData));
    pDataCallContextData->uiCID = uiCID;

    // extract data
    stPdpData.szRadioTechnology = ((char **)pData)[0];  // not used
    stPdpData.szRILDataProfile  = ((char **)pData)[1];
    stPdpData.szApn             = ((char **)pData)[2];
    stPdpData.szUserName        = ((char **)pData)[3];
    stPdpData.szPassword        = ((char **)pData)[4];
    stPdpData.szPAPCHAP         = ((char **)pData)[5];

    RIL_LOG_INFO("CTE_INF_6260::CoreSetupDataCall() - stPdpData.szRadioTechnology=[%s]\r\n", stPdpData.szRadioTechnology);
    RIL_LOG_INFO("CTE_INF_6260::CoreSetupDataCall() - stPdpData.szRILDataProfile=[%s]\r\n", stPdpData.szRILDataProfile);
    RIL_LOG_INFO("CTE_INF_6260::CoreSetupDataCall() - stPdpData.szApn=[%s]\r\n", stPdpData.szApn);
    RIL_LOG_INFO("CTE_INF_6260::CoreSetupDataCall() - stPdpData.szUserName=[%s]\r\n", stPdpData.szUserName);
    RIL_LOG_INFO("CTE_INF_6260::CoreSetupDataCall() - stPdpData.szPassword=[%s]\r\n", stPdpData.szPassword);
    RIL_LOG_INFO("CTE_INF_6260::CoreSetupDataCall() - stPdpData.szPAPCHAP=[%s]\r\n", stPdpData.szPAPCHAP);

    // if user name is empty, always use no authentication
    if (stPdpData.szUserName == NULL || strlen(stPdpData.szUserName) == 0)
    {
        nPapChap = 0;    // No authentication
    }
    else
    {
        // PAP/CHAP auth type 3 (PAP or CHAP) is not supported. In this case if a
        // a username is defined we will use PAP for authentication.
        // Note: due to an issue in the Android/Fw (missing check of the username
        // length), if the authentication is not defined, it's the value 3 (PAP or
        // CHAP) which is sent to RRIL by default.
        nPapChap = atoi(stPdpData.szPAPCHAP);
        if (nPapChap == 3)
        {
            nPapChap = 1;    // PAP authentication

            RIL_LOG_INFO("CTE_INF_6260::CoreSetupDataCall() - New PAP/CHAP=[%d]\r\n", nPapChap);
        }
    }

    if (RIL_VERSION >= 4 && (uiDataSize >= (7 * sizeof(char*))))
    {
        stPdpData.szPDPType         = ((char **)pData)[6];  // new in Android 2.3.4.
        RIL_LOG_INFO("CTE_INF_6260::CoreSetupDataCall() - stPdpData.szPDPType=[%s]\r\n", stPdpData.szPDPType);
    }

    //
    //  IP type is passed in dynamically.
    if (NULL == stPdpData.szPDPType)
    {
        //  hard-code "IPV4V6" (this is the default)
        stPdpData.szPDPType = szIPV4V6;
    }

    //  dynamic PDP type, need to set XDNS parameter depending on szPDPType.
    //  If not recognized, just use IPV4V6 as default.
    if (0 == strcmp(stPdpData.szPDPType, "IP"))
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1,
            sizeof(rReqData.szCmd1),
            "AT+CGDCONT=%d,\"%s\",\"%s\",,0,0;+XGAUTH=%d,%u,\"%s\",\"%s\";+XDNS=%d,1\r", uiCID, stPdpData.szPDPType,
            stPdpData.szApn, uiCID, nPapChap, stPdpData.szUserName, stPdpData.szPassword, uiCID))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetupDataCall() - cannot create CGDCONT command, stPdpData.szPDPType\r\n");
            goto Error;
        }
    }
    else if (0 == strcmp(stPdpData.szPDPType, "IPV6"))
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1,
            sizeof(rReqData.szCmd1),
            "AT+CGDCONT=%d,\"%s\",\"%s\",,0,0;+XGAUTH=%d,%u,\"%s\",\"%s\";+XDNS=%d,2\r", uiCID, stPdpData.szPDPType,
            stPdpData.szApn, uiCID, nPapChap, stPdpData.szUserName, stPdpData.szPassword, uiCID))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetupDataCall() - cannot create CGDCONT command, stPdpData.szPDPType\r\n");
            goto Error;
        }
    }
    else if (0 == strcmp(stPdpData.szPDPType, "IPV4V6"))
    {
        //  XDNS=3 is not supported by the modem so two commands +XDNS=1 and +XDNS=2 should be sent.
        if (!PrintStringNullTerminate(rReqData.szCmd1,
            sizeof(rReqData.szCmd1),
            "AT+CGDCONT=%d,\"IPV4V6\",\"%s\",,0,0;+XGAUTH=%u,%d,\"%s\",\"%s\";+XDNS=%d,1;+XDNS=%d,2\r", uiCID,
            stPdpData.szApn, uiCID, nPapChap, stPdpData.szUserName, stPdpData.szPassword, uiCID, uiCID))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetupDataCall() - cannot create CGDCONT command, stPdpData.szPDPType\r\n");
            goto Error;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetupDataCall() - Wrong PDP type\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pDataCallContextData);
    }
    else
    {
        pChannelData->SetDataState(E_DATA_STATE_INITING);

        rReqData.pContextData = (void*)pDataCallContextData;
        rReqData.cbContextData = sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA);
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSetupDataCall() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseSetupDataCall(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseSetupDataCall() - Enter\r\n");

    RIL_LOG_VERBOSE("CTE_INF_6260::ParseSetupDataCall() - Exit\r\n");
    return RRIL_RESULT_OK;
}

BOOL CTE_INF_6260::PdpContextActivate(REQUEST_DATA& rReqData, void* pData,
                                                            UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::PdpContextActivate() - Enter\r\n");

    BOOL bRet = FALSE;
    UINT32 uiCID = 0;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;

    if (NULL == pData ||
                    sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::PdpContextActivate() - Invalid input data\r\n");
        goto Error;
    }

    pDataCallContextData = (S_SETUP_DATA_CALL_CONTEXT_DATA*)pData;
    uiCID = pDataCallContextData->uiCID;

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
                                                    "AT+CGACT=1,%d\r", uiCID))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::PdpContextActivate() -  cannot create CGDATA command\r\n");
        goto Error;
    }

    if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+CEER\r", sizeof(rReqData.szCmd2)))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::PdpContextActivate() - Cannot create CEER command\r\n");
    }

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::PdpContextActivate() - Exit\r\n");
    return bRet;
}

RIL_RESULT_CODE CTE_INF_6260::ParsePdpContextActivate(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParsePdpContextActivate() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* szRsp = rRspData.szResponse;
    UINT32 uiCause;

    //  Could have +CEER response here, if AT command returned CME error.
    if (ParseCEER(rRspData, uiCause))
    {
        RIL_LOG_INFO("CTE_INF_6260::ParsePdpContextActivate() - uiCause: %u\r\n", uiCause);

        S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;
        UINT32 uiCID = 0;
        CChannel_Data* pChannelData = NULL;
        int failCause = PDP_FAIL_ERROR_UNSPECIFIED;

        if (NULL == rRspData.pContextData ||
                sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA) != rRspData.cbContextData)
        {
            RIL_LOG_INFO("CTE_INF_6260::ParsePdpContextActivate() - Invalid context data\r\n");
            goto Error;
        }

        pDataCallContextData =
                        (S_SETUP_DATA_CALL_CONTEXT_DATA*)rRspData.pContextData;
        uiCID = pDataCallContextData->uiCID;

        pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
        if (NULL == pChannelData)
        {
            RIL_LOG_INFO("CTE_INF_6260::ParsePdpContextActivate() - No Data Channel for CID %u.\r\n",
                                                                        uiCID);
            goto Error;
        }

        failCause = MapErrorCodeToRilDataFailCause(uiCause);
        pChannelData->SetDataFailCause(failCause);
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::ParsePdpContextActivate() - Exit\r\n");
    return res;
}

BOOL CTE_INF_6260::QueryIpAndDns(REQUEST_DATA& rReqData, UINT32 uiCID)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::QueryIpAndDns() - Enter\r\n");
    BOOL bRet = FALSE;

    if (uiCID != 0)
    {
        if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
                                            "AT+CGPADDR=%u;+XDNS?\r", uiCID))
        {
            bRet = TRUE;
        }
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::QueryIpAndDns() - Exit\r\n");
    return bRet;
}

RIL_RESULT_CODE CTE_INF_6260::ParseQueryIpAndDns(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseQueryIpAndDns() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    const char* pszRsp = rRspData.szResponse;
    const char* szStrExtract = NULL;

    // Parse prefix
    if (FindAndSkipString(pszRsp, "+CGPADDR: ", pszRsp))
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseQueryIpAndDns() - parse \"+CGPADDR\" \r\n");
        res = ParseIpAddress(rRspData);
    }

    if (FindAndSkipString(pszRsp, "+XDNS: ", pszRsp))
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseQueryIpAndDns() - parse \"+XDNS\" \r\n");
        res = ParseDns(rRspData);
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::ParseQueryIpAndDns() - Exit\r\n");
    return res;
}

//
// Response to AT+CGPADDR=<CID>
//
RIL_RESULT_CODE CTE_INF_6260::ParseIpAddress(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseIpAddress() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* szRsp = rRspData.szResponse;
    UINT32 uiCID;

    // Parse prefix
    if (!FindAndSkipString(szRsp, "+CGPADDR: ", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - Unable to parse \"+CGPADDR\" prefix.!\r\n");
        goto Error;
    }

    // Parse <cid>
    if (!ExtractUInt32(szRsp, uiCID, szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - Unable to parse <cid>!\r\n");
        goto Error;
    }

    if (uiCID > 0)
    {
        //  The response could come back as:
        //  +CGPADDR: <cid>,<PDP_Addr1>,<PDP_Addr2>
        //  PDP_Addr1 could be in IPv4, or IPv6.  PDP_Addr2 is present only for IPv4v6
        //  in which case PDP_Addr1 is IPv4 and PDP_Addr2 is IPv6.
        //  a1.a2.a3.a4 (for IPv4)
        //  a1.a2.a3.a4.a5.a6.a7.a8.a9.a10.a11.a12.a13.a14.a15.a16 (for IPv6)

        //  The IPv6 format above is not IPv6 standard address string notation, as
        //  required by Android, so we need to convert it.

        //  Extract original string into szPdpAddr.
        //  Then converted address is in szIpAddr1.
        char szPdpAddr[MAX_IPADDR_SIZE] = {'\0'};
        char szIpAddr1[MAX_IPADDR_SIZE] = {'\0'};
        char szIpAddr2[MAX_IPADDR_SIZE] = {'\0'};
        int state;

        CChannel_Data* pChannelData =
                                    CChannel_Data::GetChnlFromContextID(uiCID);
        if (NULL == pChannelData)
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - No Data Channel for CID %u.\r\n",
                                                                                        uiCID);
            goto Error;
        }

        state = pChannelData->GetDataState();
        if (E_DATA_STATE_ACTIVE != state)
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - Wrong data state: %d\r\n",
                                                                                state);
            goto Error;
        }

        //  Extract ,<Pdp_Addr1>
        if (!SkipString(szRsp, ",", szRsp) ||
            !ExtractQuotedString(szRsp, szPdpAddr, MAX_IPADDR_SIZE, szRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - Unable to parse <PDP_addr1>!\r\n");
            goto Error;
        }

        //  The AT+CGPADDR command doesn't return IPV4V6 format
        if (!ConvertIPAddressToAndroidReadable(szPdpAddr,
                                                szIpAddr1,
                                                MAX_IPADDR_SIZE,
                                                szIpAddr2,
                                                MAX_IPADDR_SIZE))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - ConvertIPAddressToAndroidReadable failed!\r\n");
            goto Error;
        }

        //  Extract ,<PDP_Addr2>
        //  Converted address is in szIpAddr2.
        if (SkipString(szRsp, ",", szRsp))
        {
            if (!ExtractQuotedString(szRsp, szPdpAddr, MAX_IPADDR_SIZE, szRsp))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - Unable to parse <PDP_addr2>!\r\n");
                goto Error;
            }

            //  The AT+CGPADDR command doesn't return IPV4V6 format.
            if (!ConvertIPAddressToAndroidReadable(szPdpAddr,
                                                    szIpAddr1,
                                                    MAX_IPADDR_SIZE,
                                                    szIpAddr2,
                                                    MAX_IPADDR_SIZE))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - ConvertIPAddressToAndroidReadable failed! m_szIpAddr2\r\n");
                goto Error;
            }

            //  Extract ,<PDP_Addr2>
            //  Converted address is in pChannelData->m_szIpAddr2.
            if (SkipString(szRsp, ",", szRsp))
            {
                if (!ExtractQuotedString(szRsp, szPdpAddr, MAX_IPADDR_SIZE, szRsp))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - Unable to parse <PDP_addr2>!\r\n");
                    goto Error;
                }

                //  The AT+CGPADDR command doesn't return IPV4V6 format
                //  Use a dummy IPV4 address as only an IPV6 address is expected here
                char szDummyIpAddr[MAX_IPADDR_SIZE];
                szDummyIpAddr[0] = '\0';

                if (!ConvertIPAddressToAndroidReadable(szPdpAddr, szDummyIpAddr, MAX_IPADDR_SIZE, szIpAddr2, MAX_IPADDR_SIZE))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - ConvertIPAddressToAndroidReadable failed! m_szIpAddr2\r\n");
                    goto Error;
                }

            RIL_LOG_INFO("CTE_INF_6260::ParseIpAddress() - IPV4 address: %s\r\n", szIpAddr1);
            RIL_LOG_INFO("CTE_INF_6260::ParseIpAddress() - IPV6 address: %s\r\n", szIpAddr2);
            }
        }

        pChannelData->SetIpAddress(szIpAddr1, szIpAddr2);
        res = RRIL_RESULT_OK;
    }
    else
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - uiCID=[%u] not valid!\r\n", uiCID);
    }

Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseIpAddress() - Exit\r\n");
    return res;
}

//
// Response to AT+XDNS?
//
RIL_RESULT_CODE CTE_INF_6260::ParseDns(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseDns() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* szRsp = rRspData.szResponse;
    UINT32 uiCID = 0;
    char szDNS[MAX_IPADDR_SIZE] = {'\0'};
    char szIpDNS1[MAX_IPADDR_SIZE] = {'\0'};
    char szIpDNS2[MAX_IPADDR_SIZE] = {'\0'};
    char szIpV6DNS1[MAX_IPADDR_SIZE] = {'\0'};
    char szIpV6DNS2[MAX_IPADDR_SIZE] = {'\0'};
    CChannel_Data* pChannelData = NULL;
    int state;

    // Parse "+XDNS: "
    while (FindAndSkipString(szRsp, "+XDNS: ", szRsp))
    {
        // Parse <cid>
        if (!ExtractUInt32(szRsp, uiCID, szRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - Unable to parse <cid>!\r\n");
            continue;
        }

        pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
        if (NULL == pChannelData)
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - No Data Channel for CID %u.\r\n",
                                                                                        uiCID);
            continue;
        }

        state = pChannelData->GetDataState();
        if (E_DATA_STATE_ACTIVE != state)
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - Wrong data state: %d\r\n",
                                                                                state);
            continue;
        }

        //  The response could come back as:
        //  +XDNS: <cid>,<Primary_DNS>,<Secondary_DNS>
        //  Also, Primary_DNS and Secondary_DNS could be in ipv4, ipv6, or ipv4v6 format.
        //  String is in dot-separated numeric (0-255) of the form:
        //  a1.a2.a3.a4 (for IPv4)
        //  a1.a2.a3.a4.a5.a6.a7.a8.a9.a10.a11.a12.a13.a14.a15.a16 (for IPv6)
        //  a1.a2.a3.a4.a5.a6.a7.a8.a9.a10.a11.a12.a13.a14.a15.a16.a17.a18.a19.a20 (for IPv4v6)

        //  The IPV6, and IPV4V6 format above is incompatible with Android, so we need to convert
        //  to an Android-readable IPV6, IPV4 address format.

        //  Extract original string into
        //  Then converted address is in szIpDNS1, szIpV6DNS1 (if IPv4v6).

        // Parse <primary DNS>
        // Converted address is in szIpDNS1, szIpV6DNS1 (if necessary)
        if (SkipString(szRsp, ",", szRsp))
        {
            if (!ExtractQuotedString(szRsp, szDNS, MAX_IPADDR_SIZE, szRsp))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - Unable to extact szDNS 1!\r\n");
                continue;
            }

            //  Now convert to Android-readable format (and split IPv4v6 parts (if applicable)
            if (!ConvertIPAddressToAndroidReadable(szDNS,
                                                    szIpDNS1,
                                                    MAX_IPADDR_SIZE,
                                                    szIpV6DNS1,
                                                    MAX_IPADDR_SIZE))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - ConvertIPAddressToAndroidReadable failed! m_szDNS1\r\n");
                continue;
            }

            RIL_LOG_INFO("CTE_INF_6260::ParseDns() - szIpDNS1: %s\r\n", szIpDNS1);

            if (strlen(szIpV6DNS1) > 0)
            {
                RIL_LOG_INFO("CTE_INF_6260::ParseDns() - szIpV6DNS1: %s\r\n", szIpV6DNS1);
            }
            else
            {
                RIL_LOG_INFO("CTE_INF_6260::ParseDns() - szIpV6DNS1: <NONE>\r\n");
            }
        }

        // Parse <secondary DNS>
        // Converted address is in szIpDNS2, szIpV6DNS2 (if necessary)
        if (SkipString(szRsp, ",", szRsp))
        {
            if (!ExtractQuotedString(szRsp, szDNS, MAX_IPADDR_SIZE, szRsp))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - Unable to extact szDNS 2!\r\n");
                continue;
            }

            //  Now convert to Android-readable format (and split IPv4v6 parts (if applicable)
            if (!ConvertIPAddressToAndroidReadable(szDNS,
                                                    szIpDNS2,
                                                    MAX_IPADDR_SIZE,
                                                    szIpV6DNS2,
                                                    MAX_IPADDR_SIZE))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - ConvertIPAddressToAndroidReadable failed! szIpDNS2\r\n");
                continue;
            }

            RIL_LOG_INFO("CTE_INF_6260::ParseDns() - szIpDNS2: %s\r\n", szIpDNS2);

            if (strlen(szIpV6DNS2) > 0)
            {
                RIL_LOG_INFO("CTE_INF_6260::ParseDns() - szIpV6DNS2: %s\r\n", szIpV6DNS2);
            }
            else
            {
                RIL_LOG_INFO("CTE_INF_6260::ParseDns() - szIpV6DNS2: <NONE>\r\n");
            }
        }

        pChannelData->SetDNS(szIpDNS1, szIpV6DNS1, szIpDNS2, szIpV6DNS2);

        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseDns() - Exit\r\n");
    return res;
}

BOOL CTE_INF_6260::EnterDataState(REQUEST_DATA& rReqData, UINT32 uiCID)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::EnterDataState() - Enter\r\n");

    BOOL bRet = FALSE;

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
                                        "AT+CGDATA=\"M-RAW_IP\",%d\r", uiCID))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::EnterDataState() -  cannot create CGDATA command\r\n");
        goto Error;
    }

    if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+CEER\r", sizeof(rReqData.szCmd2)))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::EnterDataState() - Cannot create CEER command\r\n");
    }

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::EnterDataState() - Exit\r\n");
    return bRet;
}

RIL_RESULT_CODE CTE_INF_6260::ParseEnterDataState(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseEnterDataState() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* pszRsp = rRspData.szResponse;
    CChannel_Data* pChannelData = NULL;
    UINT32 uiCause;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;
    UINT32 uiCID = 0;

    if (NULL == rRspData.pContextData ||
            sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA) != rRspData.cbContextData)
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseEnterDataState() - Invalid context data\r\n");
        goto Error;
    }

    if (ParseCEER(rRspData, uiCause))
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseEnterDataState() - uiCause: %u\r\n",
                                                                    uiCause);
        int failCause = PDP_FAIL_ERROR_UNSPECIFIED;

        pDataCallContextData =
                        (S_SETUP_DATA_CALL_CONTEXT_DATA*)rRspData.pContextData;
        uiCID = pDataCallContextData->uiCID;

        pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
        if (NULL == pChannelData)
        {
            RIL_LOG_INFO("CTE_INF_6260::ParseEnterDataState() - No Data Channel for CID %u.\r\n",
                                                                        uiCID);
            goto Error;
        }

        failCause = MapErrorCodeToRilDataFailCause(uiCause);
        pChannelData->SetDataFailCause(failCause);
        goto Error;
    }

    if (!FindAndSkipString(pszRsp, "CONNECT", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseEnterDataState() -  Did not get \"CONNECT\" response.\r\n");
        goto Error;
    }

    pDataCallContextData =
                    (S_SETUP_DATA_CALL_CONTEXT_DATA*)rRspData.pContextData;
    uiCID = pDataCallContextData->uiCID;

    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseEnterDataState() - No Data Channel for CID %u.\r\n",
                                                                    uiCID);
        goto Error;
    }
    else
    {
        // Block the read thread and then flush the tty and the channel
        // From now, any failure will lead to DataConfigDown
        pChannelData->BlockAndFlushChannel(BLOCK_CHANNEL_BLOCK_ALL, FLUSH_CHANNEL_NO_FLUSH);
        pChannelData->FlushAndUnblockChannel(UNBLOCK_CHANNEL_UNBLOCK_TTY, FLUSH_CHANNEL_FLUSH_ALL);
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseEnterDataState() - Exit\r\n");
    return res;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
class BerTlv
{
public:
    BerTlv() : m_bTag(0), m_uLen(0), m_pbValue(NULL), m_uTotalSize(0) {};
    ~BerTlv() {};

    UINT8 GetTag() {return m_bTag;};
    UINT32 GetLength() {return m_uLen;};
    const UINT8* GetValue() {return m_pbValue;};
    UINT32 GetTotalSize() {return m_uTotalSize;};

    BOOL Parse(const UINT8* pRawData, UINT32 cbSize);


private:
    UINT8 m_bTag;
    UINT32 m_uLen;
    const UINT8* m_pbValue;
    UINT32 m_uTotalSize;
};



BOOL BerTlv::Parse(const UINT8* pRawData, UINT32 cbSize)
{
    if (2 > cbSize) {
        // Not enough data for a TLV.
        return FALSE;
    }

    // Tag at index 0.
    UINT8 bTag = pRawData[0];
    if (0x00 == bTag ||
        0xFF == bTag)
    {
        // Invalid Tag
        return FALSE;
    }

    // Encoded length starts at index 1
    UINT8 bValuePos = 0;
    UINT32 uLen = 0;

    if (0x80 == (0x80 & pRawData[1])) {
        UINT8 bLenBytes = 0x7F & pRawData[1];

        if (1 < bLenBytes ||
            3 > cbSize) {
            // Currently only support 1 extra length byte
            return FALSE;
        }

        uLen = pRawData[2];
        bValuePos = 3;
    }
     else {
        uLen = pRawData[1];
        bValuePos = 2;
    }

    // Verify there is enough data available for the value
    if (uLen + bValuePos > cbSize) {
        return FALSE;
    }

    // Verify length and value size are consistent.
    if (cbSize - bValuePos < uLen) {
        // Try and recover using the minimum value.
        uLen = cbSize - bValuePos;
    }

    m_bTag = bTag;
    m_uLen = uLen;
    m_pbValue = pRawData + bValuePos;
    m_uTotalSize = uLen + bValuePos;

    return TRUE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////



//  Extract USIM GET_RESPONSE info.
struct sUSIMGetResponse
{
    UINT32 dwRecordType;
    //UINT32 dwItemCount;
    UINT32 dwTotalSize;
    UINT32 dwRecordSize;
};

#define RIL_SIMRECORDTYPE_UNKNOWN       (0x00000000)
#define RIL_SIMRECORDTYPE_TRANSPARENT   (0x00000001)
#define RIL_SIMRECORDTYPE_CYCLIC        (0x00000002)
#define RIL_SIMRECORDTYPE_LINEAR        (0x00000003)
#define RIL_SIMRECORDTYPE_MASTER        (0x00000004)
#define RIL_SIMRECORDTYPE_DEDICATED     (0x00000005)


//
//  This function converts a USIM GET_RESPONSE response into a SIM GET_RESPONSE that Android
//  can understand.  We extract the record type, total size, and record size.
//
//  sResponse = [in] binary buffer of CRSM response
//  cbResponse = [in] length of binary buffer
//  sUSIM = [in,out] where the extracted data is stored
BOOL ParseUSIMRecordStatus(UINT8* sResponse, UINT32 cbResponse, struct sUSIMGetResponse *sUSIM)
{
    RIL_LOG_VERBOSE("ParseUSIMRecordStatus - ENTER\r\n");

    BOOL bRet = FALSE;

    const UINT8 FCP_TAG = 0x62;
    const UINT8 FILE_SIZE_TAG = 0x80;
    const UINT8 FILE_DESCRIPTOR_TAG = 0x82;
    const UINT8 FILE_ID_TAG = 0x83;
    const UINT32 MASTER_FILE_ID = 0x3F00;
    UINT32 dwRecordType = 0;
    UINT32 dwItemCount = 0;
    UINT32 dwRecordSize = 0;
    UINT32 dwTotalSize = 0;
    BerTlv tlvFileDescriptor;
    BerTlv tlvFcp;

    const UINT8* pbFcpData = NULL;
    UINT32 cbFcpDataSize = 0;
    UINT32 cbDataUsed = 0;
    const UINT8* pbFileDescData = NULL;
    UINT32 cbFileDescDataSize = 0;

    BOOL fIsDf = FALSE;
    UINT8 bEfStructure = 0;

    if (NULL == sResponse ||
        0 == cbResponse ||
        NULL == sUSIM)
    {
        RIL_LOG_CRITICAL("ParseUSIMRecordStatus - Input parameters\r\n");
        goto Error;
    }

    // Need at least 2 bytes for response data FCP (file control parameters)
    if (2 > cbResponse)
    {
        RIL_LOG_CRITICAL("ParseUSIMRecordStatus - Need at least 2 bytes for response data\r\n");
        goto Error;
    }

    // Validate this response is a 3GPP 102 221 SELECT response.
    tlvFcp.Parse(sResponse, cbResponse);
    if (FCP_TAG != tlvFcp.GetTag())
    {
        RIL_LOG_CRITICAL("ParseUSIMRecordStatus - First tag is not FCP.  Tag=[0x%02X]\r\n", tlvFcp.GetTag());
        goto Error;
    }

    if (cbResponse != tlvFcp.GetTotalSize())
    {
        RIL_LOG_CRITICAL("ParseUSIMRecordStatus - cbResponse=[%d] not equal to total size=[%d]\r\n", cbResponse, tlvFcp.GetTotalSize());
        goto Error;
    }
    pbFcpData = tlvFcp.GetValue();
    cbFcpDataSize = tlvFcp.GetLength();

    // Retrieve the File Descriptor data object
    tlvFileDescriptor.Parse(pbFcpData, cbFcpDataSize);
    if (FILE_DESCRIPTOR_TAG != tlvFileDescriptor.GetTag())
    {
        RIL_LOG_CRITICAL("ParseUSIMRecordStatus - File descriptor tag is not FCP.  Tag=[0x%02X]\r\n", tlvFileDescriptor.GetTag());
        goto Error;
    }

    cbDataUsed = tlvFileDescriptor.GetTotalSize();
    if (cbDataUsed > cbFcpDataSize)
    {
        RIL_LOG_CRITICAL("ParseUSIMRecordStatus - cbDataUsed=[%d] is greater than cbFcpDataSize=[%d]\r\n", cbDataUsed, cbFcpDataSize);
        goto Error;
    }

    pbFileDescData = tlvFileDescriptor.GetValue();
    cbFileDescDataSize = tlvFileDescriptor.GetLength();
    // File descriptors should only be 2 or 5 bytes long.
    if((2 != cbFileDescDataSize) && (5 != cbFileDescDataSize))
    {
        RIL_LOG_CRITICAL("ParseUSIMRecordStatus - File descriptor can only be 2 or 5 bytes.  cbFileDescDataSize=[%d]\r\n", cbFileDescDataSize);
        goto Error;

    }
    if (2 > cbFileDescDataSize)
    {
        RIL_LOG_CRITICAL("ParseUSIMRecordStatus - File descriptor less than 2 bytes.  cbFileDescDataSize=[%d]\r\n", cbFileDescDataSize);
        goto Error;
    }

    fIsDf = (0x38 == (0x38 & pbFileDescData[0]));
    bEfStructure = (0x07 & pbFileDescData[0]);

    if (fIsDf)
    {
        dwRecordType = RIL_SIMRECORDTYPE_DEDICATED;
    }
    // or it is an EF or MF.
    else
    {
        switch (bEfStructure)
        {
            // Transparent
            case 0x01:
                dwRecordType = RIL_SIMRECORDTYPE_TRANSPARENT;
                break;

            // Linear Fixed
            case 0x02:
                dwRecordType = RIL_SIMRECORDTYPE_LINEAR;
                break;

            // Cyclic
            case 0x06:
                dwRecordType = RIL_SIMRECORDTYPE_CYCLIC;
                break;

           default:
                break;
        }

        if (RIL_SIMRECORDTYPE_LINEAR == dwRecordType ||
            RIL_SIMRECORDTYPE_CYCLIC == dwRecordType)
        {
            // Need at least 5 bytes
            if (5 > cbFileDescDataSize)
            {
                RIL_LOG_CRITICAL("ParseUSIMRecordStatus - File descriptor less than 5 bytes.  cbFileDescDataSize=[%d]\r\n", cbFileDescDataSize);
                goto Error;
            }

            dwItemCount = pbFileDescData[4];
            dwRecordSize = (pbFileDescData[2] << 4) + (pbFileDescData[3]);

            // Skip checking of consistency with the File Size data object to
            // save time.
            dwTotalSize = dwItemCount * dwRecordSize;
        }
        else if(RIL_SIMRECORDTYPE_TRANSPARENT == dwRecordType)
        {
            // Retrieve the file size.
            BerTlv tlvCurrent;

            while (cbFcpDataSize > cbDataUsed)
            {
                if (!tlvCurrent.Parse(
                    pbFcpData + cbDataUsed,
                    cbFcpDataSize - cbDataUsed))
                {
                    RIL_LOG_CRITICAL("ParseUSIMRecordStatus - Couldn't parse TRANSPARENT\r\n");
                    goto Error;
                }

                cbDataUsed += tlvCurrent.GetTotalSize();

                if (FILE_SIZE_TAG == tlvCurrent.GetTag())
                {
                    const UINT8* pbFileSize = NULL;

                    if (2 > tlvCurrent.GetLength())
                    {
                        RIL_LOG_CRITICAL("ParseUSIMRecordStatus - TRANSPARENT length must be at least 2\r\n");
                        goto Error;
                    }

                    pbFileSize = tlvCurrent.GetValue();

                    dwTotalSize = (pbFileSize[0] << 4) + pbFileSize[1];

                    // Size found. Leave loop
                    break;
                }
            }
        }
    }

    // if record type has not been resolved, check for Master file.
    if (RIL_SIMRECORDTYPE_UNKNOWN == dwRecordType)
    {
        const UINT8* pbFileId = NULL;
        UINT32 uFileId = 0;

        // Next data object should be File ID.
        BerTlv tlvFileId;
        tlvFileId.Parse(
            pbFcpData + cbFcpDataSize,
            cbFcpDataSize - cbDataUsed);

        if (FILE_ID_TAG != tlvFileId.GetTag())
        {
            RIL_LOG_CRITICAL("ParseUSIMRecordStatus - UNKNOWN tag not equal to FILE_ID_TAG\r\n");
            goto Error;
        }

        if (2 != tlvFileId.GetLength())
        {
            RIL_LOG_CRITICAL("ParseUSIMRecordStatus - UNKNOWN length not equal to 2\r\n");
            goto Error;
        }

        pbFileId = tlvFileId.GetValue();
        uFileId = (pbFileId[0] << 4) + pbFileId[1];

        if (MASTER_FILE_ID != uFileId)
        {
            RIL_LOG_CRITICAL("ParseUSIMRecordStatus - UNKNOWN file ID not equal to MASTER_FILE_ID\r\n");
            goto Error;
        }

        dwRecordType = RIL_SIMRECORDTYPE_MASTER;
    }

    sUSIM->dwRecordType = dwRecordType;
    sUSIM->dwTotalSize = dwTotalSize;
    sUSIM->dwRecordSize = dwRecordSize;

    bRet = TRUE;
Error:

    RIL_LOG_VERBOSE("ParseUSIMRecordStatus - EXIT = [%d]\r\n", bRet);
    return bRet;
}

RIL_RESULT_CODE CTE_INF_6260::HandlePin2RelatedSIMIO(RIL_SIM_IO_v6* pSimIOArgs, REQUEST_DATA& rReqData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::HandlePin2RelatedSIMIO() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char szSimIoCmd[MAX_BUFFER_SIZE] = {0};

    //  If PIN2 is required, send out AT+CPIN2 request
    if (NULL == pSimIOArgs->pin2 && SIM_COMMAND_UPDATE_RECORD == pSimIOArgs->command)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::HandlePin2RelatedSIMIO() - PIN 2 required but not provided!\r\n");
        res = RIL_E_SIM_PIN2;
        goto Error;
    }

    if (RIL_PINSTATE_ENABLED_BLOCKED == m_ePin2State)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::HandlePin2RelatedSIMIO() - RIL_PINSTATE_ENABLED_BLOCKED\r\n");
        res = RIL_E_SIM_PUK2;
        goto Error;
    }
    else if (RIL_PINSTATE_ENABLED_PERM_BLOCKED == m_ePin2State)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::HandlePin2RelatedSIMIO() - RIL_PINSTATE_ENABLED_PERM_BLOCKED\r\n");
        res = RIL_E_GENERIC_FAILURE;
        goto Error;
    }

    if (NULL == pSimIOArgs->data)
    {
        if(NULL == pSimIOArgs->path)
        {
            if (!PrintStringNullTerminate(szSimIoCmd, sizeof(szSimIoCmd),
                "+CRSM=%d,%d,%d,%d,%d\r",
                pSimIOArgs->command,
                pSimIOArgs->fileid,
                pSimIOArgs->p1,
                pSimIOArgs->p2,
                pSimIOArgs->p3))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::HandlePin2RelatedSIMIO() - cannot create CRSM command!\r\n");
                goto Error;
            }
        }
        else // (NULL != pSimIOArgs->path)
        {
            if (!PrintStringNullTerminate(szSimIoCmd, sizeof(szSimIoCmd),
                          "+CRSM=%d,%d,%d,%d,%d,,\"%s\"\r",
                          pSimIOArgs->command,
                          pSimIOArgs->fileid,
                          pSimIOArgs->p1,
                          pSimIOArgs->p2,
                          pSimIOArgs->p3,
                          pSimIOArgs->path))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::HandlePin2RelatedSIMIO() - cannot create CRSM command!\r\n");
                goto Error;
            }
        }
    }
    else // (NULL != pSimIOArgs->data)
    {
        if(NULL == pSimIOArgs->path)
        {
            if (!PrintStringNullTerminate(szSimIoCmd, sizeof(szSimIoCmd),
                "+CRSM=%d,%d,%d,%d,%d,\"%s\"\r",
                pSimIOArgs->command,
                pSimIOArgs->fileid,
                pSimIOArgs->p1,
                pSimIOArgs->p2,
                pSimIOArgs->p3,
                pSimIOArgs->data))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::HandlePin2RelatedSIMIO() - cannot create CRSM command!\r\n");
                goto Error;
            }
        }
        else // (NULL != pSimIOArgs->path)
        {
            if (!PrintStringNullTerminate(szSimIoCmd, sizeof(szSimIoCmd),
                "+CRSM=%d,%d,%d,%d,%d,\"%s\",\"%s\"\r",
                pSimIOArgs->command,
                pSimIOArgs->fileid,
                pSimIOArgs->p1,
                pSimIOArgs->p2,
                pSimIOArgs->p3,
                pSimIOArgs->data,
                pSimIOArgs->path))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::HandlePin2RelatedSIMIO() - cannot create CRSM command!\r\n");
                goto Error;
            }
        }
    }

    if (RIL_PINSTATE_ENABLED_NOT_VERIFIED == m_ePin2State ||
                    RIL_PINSTATE_UNKNOWN == m_ePin2State)
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1,
            sizeof(rReqData.szCmd1),
            "AT+CPIN2=\"%s\";%s\r",
            pSimIOArgs->pin2, szSimIoCmd))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::HandlePin2RelatedSIMIO() - cannot create CPIN2 command!\r\n");
            goto Error;
        }
    }
    else if (RIL_PINSTATE_ENABLED_VERIFIED == m_ePin2State)
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1,
            sizeof(rReqData.szCmd1),
            "AT+CPWD=\"P2\",\"%s\",\"%s\";%s\r",
            pSimIOArgs->pin2,
            pSimIOArgs->pin2, szSimIoCmd))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::HandlePin2RelatedSIMIO() - cannot create CPWD command!\r\n");
            goto Error;
        }
    }

    res = RRIL_RESULT_OK;
Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::HandlePin2RelatedSIMIO() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SIM_IO 28
//
RIL_RESULT_CODE CTE_INF_6260::CoreSimIo(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSimIo() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_SIM_IO_v6 *   pSimIOArgs = NULL;
    // substitute actual path instead of string "img"
    char szGraphicsPath[] = "3F007F105F50";
    char szImg[] = "img";
    char *pszPath = NULL;
    S_SIM_IO_CONTEXT_DATA* pContextData = NULL;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - Data pointer is NULL.\r\n");
        goto Error;
    }

    if (sizeof(RIL_SIM_IO_v6) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - Invalid data size. Given %d bytes\r\n", uiDataSize);
        goto Error;
    }

    // extract data
    pSimIOArgs = (RIL_SIM_IO_v6 *)pData;

#if defined(DEBUG)
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSimIo() - command=%d fileid=%04X path=\"%s\" p1=%d p2=%d p3=%d data=\"%s\" pin2=\"%s\" aidPtr=\"%s\"\r\n",
        pSimIOArgs->command, pSimIOArgs->fileid, pSimIOArgs->path,
        pSimIOArgs->p1, pSimIOArgs->p2, pSimIOArgs->p3,
        pSimIOArgs->data, pSimIOArgs->pin2, pSimIOArgs->aidPtr);
#else
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSimIo() - command=%d fileid=%04X path=\"%s\" p1=%d p2=%d p3=%d data=\"%s\" aidPtr=\"%s\"\r\n",
        pSimIOArgs->command, pSimIOArgs->fileid, pSimIOArgs->path,
        pSimIOArgs->p1, pSimIOArgs->p2, pSimIOArgs->p3,
        pSimIOArgs->data, pSimIOArgs->aidPtr);
#endif

    // Currently, in JB, Android implements functions that use the aidPtr parameter
    // but for the moment only an empty string is used. The use of aidPtr is not
    // supported here. This function needs to be modified to use +CRLA, CCHO and
    // CCHC commands.
    if (NULL != pSimIOArgs->aidPtr && 0 != strcmp(pSimIOArgs->aidPtr, ""))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - use of aidPtr NOT SUPPORTED, must call +CRLA, CCHO and CCHC commands\r\n");
        res = RIL_E_REQUEST_NOT_SUPPORTED;
        goto Error;
    }

    rReqData.pContextData = NULL;

    pContextData = (S_SIM_IO_CONTEXT_DATA*) malloc(sizeof(S_SIM_IO_CONTEXT_DATA));
    if (NULL != pContextData)
    {
        pContextData->fileId = pSimIOArgs->fileid;
        pContextData->command = pSimIOArgs->command;
        rReqData.pContextData = pContextData;
        rReqData.cbContextData = sizeof(S_SIM_IO_CONTEXT_DATA);
    }

    switch (pSimIOArgs->fileid)
    {
        case EF_FDN:
        case EF_EXT2:
            if (EF_FDN == pSimIOArgs->fileid &&
                        SIM_COMMAND_UPDATE_RECORD == pSimIOArgs->command)
            {
                return HandlePin2RelatedSIMIO(pSimIOArgs, rReqData);
            }
        break;

        default:
        break;
    }

    //  Replace path of "img" with "3F007F105F50"
    if (pSimIOArgs->path)
    {
        if (0 == strcmp(pSimIOArgs->path, szImg))
        {
            //  We have a match for "img"
            RIL_LOG_INFO("CTE_INF_6260::CoreSimIo() - Found match for path='img'.  Use GRAPHICS path\r\n");
            pszPath = szGraphicsPath;
        }
        else
        {
            //  Original path
            pszPath = pSimIOArgs->path;
        }
    }

    if (NULL == pSimIOArgs->data)
    {
        if(NULL == pSimIOArgs->path)
        {
            if (!PrintStringNullTerminate(rReqData.szCmd1,
                sizeof(rReqData.szCmd1),
                "AT+CRSM=%d,%d,%d,%d,%d\r",
                pSimIOArgs->command,
                pSimIOArgs->fileid,
                pSimIOArgs->p1,
                pSimIOArgs->p2,
                pSimIOArgs->p3))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - cannot create CRSM command\r\n");
                goto Error;
            }
        }
        else
        {
            if (!PrintStringNullTerminate(rReqData.szCmd1,
                sizeof(rReqData.szCmd1),
                "AT+CRSM=%d,%d,%d,%d,%d,,\"%s\"\r",
                pSimIOArgs->command,
                pSimIOArgs->fileid,
                pSimIOArgs->p1,
                pSimIOArgs->p2,
                pSimIOArgs->p3,
                pszPath))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - cannot create CRSM command\r\n");
                goto Error;
            }
        }
    }
    else
    {
        if(NULL == pSimIOArgs->path)
        {
            if (!PrintStringNullTerminate(rReqData.szCmd1,
                sizeof(rReqData.szCmd1),
                "AT+CRSM=%d,%d,%d,%d,%d,\"%s\"\r",
                pSimIOArgs->command,
                pSimIOArgs->fileid,
                pSimIOArgs->p1,
                pSimIOArgs->p2,
                pSimIOArgs->p3,
                pSimIOArgs->data))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - cannot create CRSM command\r\n");
                goto Error;
            }
        }
        else
        {
            if (!PrintStringNullTerminate(rReqData.szCmd1,
                sizeof(rReqData.szCmd1),
                "AT+CRSM=%d,%d,%d,%d,%d,\"%s\",\"%s\"\r",
                pSimIOArgs->command,
                pSimIOArgs->fileid,
                pSimIOArgs->p1,
                pSimIOArgs->p2,
                pSimIOArgs->p3,
                pSimIOArgs->data,
                pszPath))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - cannot create CRSM command\r\n");
                goto Error;
            }
        }
    }

    res = RRIL_RESULT_OK;
Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSimIo() - Exit\r\n");
    return res;
}



RIL_RESULT_CODE CTE_INF_6260::ParseSimIo(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseSimIo() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    UINT32  uiSW1 = 0;
    UINT32  uiSW2 = 0;
    char* szResponseString = NULL;
    UINT32  cbResponseString = 0;
    RIL_SIM_IO_Response* pResponse = NULL;

    if (NULL == rRspData.szResponse)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - Response String pointer is NULL.\r\n");
        goto Error;
    }

    // Parse "<prefix>+CRSM: <sw1>,<sw2>"
    if (!SkipRspStart(pszRsp, m_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - Could not skip over response prefix.\r\n");
        goto Error;
    }

    if (!SkipString(pszRsp, "+CRSM: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - Could not skip over \"+CRSM: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt32(pszRsp, uiSW1, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - Could not extract SW1 value.\r\n");
        goto Error;
    }

    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt32(pszRsp, uiSW2, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - Could not extract SW2 value.\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_INF_6260::ParseSimIo() - Extracted SW1 = %u and SW2 = %u\r\n", uiSW1, uiSW2);

    // Parse ","
    if (SkipString(pszRsp, ",", pszRsp))
    {
        int nCRSMCommand = 0;
        S_SIM_IO_CONTEXT_DATA* pContextData = NULL;

        // Parse <response>
        // NOTE: we take ownership of allocated szResponseString
        if (!ExtractQuotedStringWithAllocatedMemory(pszRsp, szResponseString, cbResponseString, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - Could not extract data string.\r\n");
            goto Error;
        }
        else
        {
            RIL_LOG_INFO("CTE_INF_6260::ParseSimIo() - Extracted data string: \"%s\" (%u chars including NULL)\r\n", szResponseString, cbResponseString);
        }

        if (0 != (cbResponseString - 1) % 2)
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() : String was not a multiple of 2.\r\n");
            goto Error;
        }

        // Check for USIM GET_RESPONSE response
        if (NULL == rRspData.pContextData ||
                    rRspData.cbContextData != sizeof(S_SIM_IO_CONTEXT_DATA))
        {
            goto Error;
        }

        pContextData = (S_SIM_IO_CONTEXT_DATA*) rRspData.pContextData;
        nCRSMCommand = pContextData->command;

        if ((192 == nCRSMCommand) && ('6' == szResponseString[0]) && ('2' == szResponseString[1]))
        {
            //  USIM GET_RESPONSE response
            RIL_LOG_INFO("CTE_INF_6260::ParseSimIo() - USIM GET_RESPONSE\r\n");

            char szTemp[5] = {0};

            struct sUSIMGetResponse sUSIM;
            memset(&sUSIM, 0, sizeof(struct sUSIMGetResponse));

            //  Need to convert the response string from ascii to binary.
            BYTE *sNewString = NULL;
            UINT32 cbNewString = (cbResponseString-1)/2;
            sNewString = new BYTE[cbNewString];
            if (NULL == sNewString)
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - Cannot create new string!\r\n");
                goto Error;
            }
            memset(sNewString, 0, cbNewString);

            UINT32 cbUsed = 0;
            if (!GSMHexToGSM(szResponseString, cbResponseString, sNewString, cbNewString, cbUsed))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - Cannot cconvert szResponseString to GSMHex.\r\n");
                delete[] sNewString;
                sNewString = NULL;
                cbNewString = 0;
                goto Error;
            }

            //  OK, now parse!
            if (!ParseUSIMRecordStatus((UINT8*)sNewString, cbNewString, &sUSIM))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - Cannot parse USIM record status\r\n");
                delete[] sNewString;
                sNewString = NULL;
                cbNewString = 0;
                goto Error;
            }


            delete[] sNewString;
            sNewString = NULL;
            cbNewString = 0;

            RIL_LOG_VERBOSE("CTE_INF_6260::ParseSimIo() - sUSIM.dwRecordType=[0x%04X]\r\n", sUSIM.dwRecordType);
            RIL_LOG_VERBOSE("CTE_INF_6260::ParseSimIo() - sUSIM.dwTotalSize=[0x%04X]\r\n", sUSIM.dwTotalSize);
            RIL_LOG_VERBOSE("CTE_INF_6260::ParseSimIo() - sUSIM.dwRecordSize=[0x%04X]\r\n", sUSIM.dwRecordSize);


            //  Delete old original response.  Create new "fake" response.
            delete []szResponseString;
            szResponseString = NULL;
            cbResponseString = 0;

            //  Java layers as of Froyo (Nov 1/2010) only use:
            //  Total size (0-based bytes 2 and 3), File type (0-based byte 6),
            //  Data structure (0-based byte 13), Data record length (0-based byte 14)
            cbResponseString = 31;  //  15 bytes + NULL
            szResponseString = new char[cbResponseString];
            if (NULL == szResponseString)
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - Cannot create new szResponsestring!\r\n");
                delete[] sNewString;
                sNewString = NULL;
                goto Error;
            }
            if (!CopyStringNullTerminate(szResponseString, "000000000000000000000000000000", cbResponseString))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - Cannot CopyStringNullTerminate szResponsestring!\r\n");
                delete[] sNewString;
                sNewString = NULL;
                goto Error;
            }

            //  Extract info, put into new response string
            if (!PrintStringNullTerminate(szTemp, 5, "%04X", sUSIM.dwTotalSize))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - Cannot PrintStringNullTerminate sUSIM.dwTotalSize!\r\n");
                delete[] sNewString;
                sNewString = NULL;
                goto Error;
            }
            memcpy(&szResponseString[4], szTemp, 4);

            if (!PrintStringNullTerminate(szTemp, 3, "%02X", sUSIM.dwRecordSize))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - Cannot PrintStringNullTerminate sUSIM.dwRecordSize!\r\n");
                delete[] sNewString;
                sNewString = NULL;
                goto Error;
            }
            memcpy(&szResponseString[28], szTemp, 2);

            if (RIL_SIMRECORDTYPE_UNKNOWN == sUSIM.dwRecordType)
            {
                //  bad parse.
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - sUSIM.dwRecordType is unknown!\r\n");
                delete[] sNewString;
                sNewString = NULL;
                goto Error;
            }


            if (RIL_SIMRECORDTYPE_MASTER == sUSIM.dwRecordType)
            {
                szResponseString[13] = '1';
            }
            else if (RIL_SIMRECORDTYPE_DEDICATED == sUSIM.dwRecordType)
            {
                szResponseString[13] = '2';
            }
            else
            {
                //  elementary file
                szResponseString[13] = '4';

                if (RIL_SIMRECORDTYPE_TRANSPARENT == sUSIM.dwRecordType)
                {
                    szResponseString[27] = '0';
                }
                else if (RIL_SIMRECORDTYPE_LINEAR == sUSIM.dwRecordType)
                {
                    szResponseString[27] = '1';
                }
                else if (RIL_SIMRECORDTYPE_CYCLIC == sUSIM.dwRecordType)
                {
                    szResponseString[27] = '3';
                }
            }

            //  ok we're done.  print.
            RIL_LOG_INFO("CTE_INF_6260::ParseSimIo() - new USIM response=[%s]\r\n", szResponseString);

        }

    }

    // Allocate memory for the response struct PLUS a buffer for the response string
    // The char* in the RIL_SIM_IO_Response will point to the buffer allocated directly after the RIL_SIM_IO_Response
    // When the RIL_SIM_IO_Response is deleted, the corresponding response string will be freed as well.
    pResponse = (RIL_SIM_IO_Response*)malloc(sizeof(RIL_SIM_IO_Response) + cbResponseString + 1);
    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - Could not allocate memory for a RIL_SIM_IO_Response struct.\r\n");
        goto Error;
    }
    memset(pResponse, 0, sizeof(RIL_SIM_IO_Response) + cbResponseString + 1);

    pResponse->sw1 = uiSW1;
    pResponse->sw2 = uiSW2;

    if (NULL == szResponseString)
    {
        pResponse->simResponse = NULL;
    }
    else
    {
        pResponse->simResponse = (char*)(((char*)pResponse) + sizeof(RIL_SIM_IO_Response));
        if (!CopyStringNullTerminate(pResponse->simResponse, szResponseString, cbResponseString))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - Cannot CopyStringNullTerminate szResponseString\r\n");
            goto Error;
        }

        // Ensure NULL termination!
        pResponse->simResponse[cbResponseString] = '\0';
    }

    // Parse "<postfix>"
    if (!SkipRspEnd(pszRsp, m_szNewLine, pszRsp))
    {
        goto Error;
    }

    rRspData.pData   = (void*)pResponse;
    rRspData.uiDataSize  = sizeof(RIL_SIM_IO_Response);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pResponse);
        pResponse = NULL;
    }

    delete[] szResponseString;
    szResponseString = NULL;

    RIL_LOG_VERBOSE("CTE_INF_6260::ParseSimIo() - Exit\r\n");
    return res;
}



//
// RIL_REQUEST_DEACTIVATE_DATA_CALL 41
//
RIL_RESULT_CODE CTE_INF_6260::CoreDeactivateDataCall(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreDeactivateDataCall() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char * pszCid = NULL;
    char * pszReason = NULL;
    UINT32 uiCID = 0;

    if (uiDataSize < (1 * sizeof(char *)))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreDeactivateDataCall() - Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreDeactivateDataCall() - Passed data pointer was NULL\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_INF_6260::CoreDeactivateDataCall() - uiDataSize=[%d]\r\n", uiDataSize);

    pszCid = ((char**)pData)[0];
    if (pszCid == NULL || '\0' == pszCid[0])
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreDeactivateDataCall() - pszCid was NULL\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_INF_6260::CoreDeactivateDataCall() - pszCid=[%s]\r\n", pszCid);

    if ( (RIL_VERSION >= 4) && (uiDataSize >= (2 * sizeof(char *))) )
    {
        pszReason = ((char**)pData)[1];
        if (pszReason == NULL || '\0' == pszReason[0])
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::CoreDeactivateDataCall() - pszReason was NULL\r\n");
            goto Error;
        }
        RIL_LOG_INFO("CTE_INF_6260::CoreDeactivateDataCall() - pszReason=[%s]\r\n", pszReason);
    }

    //  Get CID as UINT32.
    if (sscanf(pszCid, "%u", &uiCID) == EOF)
    {
        // Error
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreDeactivateDataCall() -  cannot convert %s to int\r\n", pszCid);
        goto Error;
    }

    //  May 18,2011 - Don't call AT+CGACT=0,X if SIM was removed since context is already deactivated.
    if (RRIL_SIM_STATE_NOT_READY == GetSIMState())
    {
        RIL_LOG_INFO("CTE_INF_6260::CoreDeactivateDataCall() - SIM LOCKED OR ABSENT!! no-op this command\r\n");
        rReqData.szCmd1[0] = '\0';
        res = RRIL_RESULT_OK;
        rReqData.pContextData = (void*)((UINT32)0);

        if (uiCID > 0 && uiCID <= MAX_PDP_CONTEXTS)
        {
            DataConfigDown(uiCID);
        }
    }
    else
    {
        UINT32* pCID = (UINT32*)malloc(sizeof(UINT32));
        if (NULL == pCID)
        {
            goto Error;
        }

        *pCID = uiCID;

        if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CGACT=0,%s\r", pszCid))
        {
            res = RRIL_RESULT_OK;
        }

        //  Set the context of this command to the CID (for multiple context support).
        rReqData.pContextData = (void*)pCID;
        rReqData.cbContextData = sizeof(UINT32);
    }

Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreDeactivateDataCall() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseDeactivateDataCall(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseDeactivateDataCall() - Enter\r\n");

    RIL_LOG_VERBOSE("CTE_INF_6260::ParseDeactivateDataCall() - Exit\r\n");
    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_OEM_HOOK_RAW 59
//
// [out] UINT32 uiRilChannel - Set this value to the RIL channel that the command will be sent on
//                             e.g. RIL_CHANNEL_DLC2  (as defined in rilchannels.h)
//                             Default value is RIL_CHANNEL_ATCMD
//
// Note: Use REQUEST_DATA's pContextData2 to pass custom data to the parse function.
//       RIL Framework uses pContextData, and is reserved in this function.
RIL_RESULT_CODE CTE_INF_6260::CoreHookRaw(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize, UINT32 & uiRilChannel)
{
    RIL_LOG_INFO("CTE_INF_6260::CoreHookRaw() - Enter\r\n");

    BYTE *pDataBytes = (BYTE*)pData;

    RIL_LOG_INFO("CTE_INF_6260::CoreHookRaw() - uiDataSize=[%d]\r\n", uiDataSize);
    for (int i = 0; i < (int)uiDataSize; i++)
    {
        BYTE b = pDataBytes[i];
        RIL_LOG_INFO("CTE_INF_6260::CoreHookRaw() - pData[%d]=[0x%02X]\r\n", i, (unsigned char)b);
    }

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    int nCommand = 0;

    //  uiDataSize MUST be 4 or greater.
    if (4 > uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreHookRaw() - Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreHookRaw() - Passed data pointer was NULL\r\n");
        goto Error;
    }

    memcpy(&nCommand, &(pDataBytes[0]), sizeof(int));
    nCommand = ntohl(nCommand);  //  This is the command.
    rReqData.pContextData = (void*)nCommand;

    switch(nCommand)
    {
        default:
            RIL_LOG_CRITICAL("CTE_INF_6260::CoreHookRaw() - Received unknown command=[0x%08X]\r\n", nCommand);
            goto Error;
            break;
    }

    res = RRIL_RESULT_OK;
Error:
    RIL_LOG_INFO("CTE_INF_6260::CoreHookRaw() - Exit\r\n");
    return res;
}


RIL_RESULT_CODE CTE_INF_6260::ParseHookRaw(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_INF_6260::ParseHookRaw() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* szRsp = rRspData.szResponse;
    int nCommand = (int)rRspData.pContextData;

    //  Populate these below (if applicable)
    void* pData = NULL;
    UINT32 uiDataSize = 0;

    res = RRIL_RESULT_OK;

    rRspData.pData   = pData;
    rRspData.uiDataSize  = uiDataSize;

    RIL_LOG_INFO("CTE_INF_6260::ParseHookRaw() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_OEM_HOOK_STRINGS 60
//
// [out] UINT32 uiRilChannel - Set this value to the RIL channel that the command will be sent on
//                             e.g. RIL_CHANNEL_DLC2  (as defined in rilchannels.h)
//                             Default value is RIL_CHANNEL_ATCMD
//
// Note: Use REQUEST_DATA's pContextData2 to pass custom data to the parse function.
//       RIL Framework uses pContextData, and is reserved in this function.
RIL_RESULT_CODE CTE_INF_6260::CoreHookStrings(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize, UINT32 & uiRilChannel)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreHookStrings() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char** pszRequest = NULL;
    UINT32 uiCommand = 0;
    int nNumStrings = 0;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreHookStrings() - Data pointer is NULL.\r\n");
        goto Error;
    }

    pszRequest = ((char**)pData);
    if (pszRequest == NULL || '\0' == pszRequest[0])
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreHookStrings() - pszRequest was NULL\r\n");
        goto Error;
    }

    if ( (uiDataSize < (1 * sizeof(char *))) || (0 != (uiDataSize % sizeof(char*))) )
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreHookStrings() - Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    //  Loop through input strings and print them
    nNumStrings = uiDataSize / sizeof(char*);
    RIL_LOG_INFO("CTE_INF_6260::CoreHookStrings() - uiDataSize=[%d], numStrings=[%d]\r\n", uiDataSize, nNumStrings);
    for (int i=0; i<nNumStrings; i++)
    {
        RIL_LOG_INFO("CTE_INF_6260::CoreHookStrings() - pszRequest[%d]=[%s]\r\n", i, pszRequest[i]);
    }

    //  Get command as int.
    if (sscanf(pszRequest[0], "%u", &uiCommand) == EOF)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreHookStrings() - cannot convert %s to int\r\n", pszRequest);
        goto Error;
    }

    RIL_LOG_INFO("CTE_INF_6260::CoreHookStrings(), uiCommand: %u", uiCommand);

    switch(uiCommand)
    {
        case RIL_OEM_HOOK_STRING_THERMAL_GET_SENSOR:
            RIL_LOG_INFO("Received Commmand: RIL_OEM_HOOK_STRING_THERMAL_GET_SENSOR");
            res = CreateGetThermalSensorValuesReq(rReqData, (const char**) pszRequest, uiDataSize);
            break;

        case RIL_OEM_HOOK_STRING_THERMAL_SET_THRESHOLD:
            RIL_LOG_INFO("Received Commmand: RIL_OEM_HOOK_STRING_THERMAL_SET_THRESHOLD\r\n");
            res = CreateActivateThermalSensorInd(rReqData, (const char**) pszRequest, uiDataSize);
            break;

        case RIL_OEM_HOOK_STRING_GET_ATR:
            if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XGATR\r"))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::CoreHookStrings() - ERROR: RIL_OEM_HOOK_STRINGS_GET_ATR - Can't construct szCmd1.\r\n");
                goto Error;
            }
            res = RRIL_RESULT_OK;
            break;

        case RIL_OEM_HOOK_STRING_SET_MODEM_AUTO_FAST_DORMANCY:
            RIL_LOG_INFO("Received Commmand: RIL_OEM_HOOK_STRING_SET_MODEM_AUTO_FAST_DORMANCY");
            // Check if Fast Dormancy mode allows OEM Hook
            if (CTE::GetTE().GetFastDormancyMode() == E_FD_MODE_OEM_MANAGED)
            {
                res = CreateAutonomousFDReq(rReqData, (const char**) pszRequest, uiDataSize);
            }
            else
            {
                RIL_LOG_INFO("RIL_OEM_HOOK_STRING_SET_MODEM_AUTO_FAST_DORMANCY - FD Mode is not OEM Managed.\r\n");
                goto Error;
            }
            break;

        case RIL_OEM_HOOK_STRING_GET_GPRS_CELL_ENV:
            RIL_LOG_INFO("Received Commmand: RIL_OEM_HOOK_STRING_GET_GPRS_CELL_ENV");
            if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CGED=0\r"))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::CoreHookStrings() - RIL_OEM_HOOK_STRING_GET_GPRS_CELL_ENV - Can't construct szCmd1.\r\n");
                goto Error;
            }
            //  Send this command on OEM channel.
            uiRilChannel = RIL_CHANNEL_OEM;
            res = RRIL_RESULT_OK;
            break;

        case RIL_OEM_HOOK_STRING_DEBUG_SCREEN_COMMAND:
            RIL_LOG_INFO("Received Commmand: RIL_OEM_HOOK_STRING_DEBUG_SCREEN_COMMAND");
            res = CreateDebugScreenReq(rReqData, (const char**) pszRequest, uiDataSize);
            //  Send this command on OEM channel.
            uiRilChannel = RIL_CHANNEL_OEM;
            break;

        case RIL_OEM_HOOK_STRING_RELEASE_ALL_CALLS:
            RIL_LOG_INFO("Received Commmand: RIL_OEM_HOOK_STRING_RELEASE_ALL_CALLS");
            if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CHLD=8\r"))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::CoreHookStrings() - RIL_OEM_HOOK_STRING_RELEASE_ALL_CALLS - Can't construct szCmd1.\r\n");
                goto Error;
            }
            res = RRIL_RESULT_OK;
            break;

        case RIL_OEM_HOOK_STRING_GET_SMS_TRANSPORT_MODE:
            RIL_LOG_INFO("Received Commmand: RIL_OEM_HOOK_STRING_GET_SMS_TRANSPORT_MODE");
            if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CGSMS?\r"))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::CoreHookStrings() - RIL_OEM_HOOK_STRING_GET_SMS_TRANSPORT_MODE - Can't construct szCmd1.\r\n");
                goto Error;
            }
            //  Send this command on SMS channel.
            uiRilChannel = RIL_CHANNEL_DLC6;
            res = RRIL_RESULT_OK;
            break;

        case RIL_OEM_HOOK_STRING_SET_SMS_TRANSPORT_MODE:
            RIL_LOG_INFO("Received Commmand: RIL_OEM_HOOK_STRING_SET_SMS_TRANSPORT_MODE");
            //  Send this command on SMS channel.
            uiRilChannel = RIL_CHANNEL_DLC6;
            res = CreateSetSMSTransportModeReq(rReqData, (const char**) pszRequest, uiDataSize);
            break;

#if defined(M2_DUALSIM_FEATURE_ENABLED)
        case RIL_OEM_HOOK_STRING_SWAP_PS:
            {
                int param;
                RIL_LOG_INFO("Received Command: RIL_OEM_HOOK_STRING_SWAP_PS");
                if (sscanf(pszRequest[1], "%u", &param) == EOF)
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::CoreHookStrings() - cannot convert %s to int\r\n", pszRequest);
                    goto Error;
                }
                rReqData.pContextData = (void *) param;
                if (param & 1)
                {
                    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XRAT=8\r"))
                    {
                        RIL_LOG_CRITICAL("CTE_INF_6260::CoreHookStrings() - RIL_OEM_HOOK_STRING_SWAP_PS - Can't construct szCmd1.\r\n");
                        goto Error;
                    }
                    rReqData.uiTimeout = 60000;
                }
                else
                {
                    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT\r"))
                    {
                        RIL_LOG_CRITICAL("CTE_INF_6260::CoreHookStrings() - RIL_OEM_HOOK_STRING_SWAP_PS - Can't construct szCmd1.\r\n");
                        goto Error;
                    }
                }
                res = RRIL_RESULT_OK;
                break;
            }
#endif // M2_DUALSIM_FEATURE_ENABLED

        default:
            RIL_LOG_CRITICAL("CTE_INF_6260::CoreHookStrings() - ERROR: Received unknown uiCommand=[0x%X]\r\n", uiCommand);
            goto Error;
    }

    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreHookStrings() : Can't create OEM HOOK String request uiCommand: 0x%X", uiCommand);
        goto Error;
    }

    rReqData.pContextData = (void*)uiCommand;
Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreHookStrings() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseHookStrings(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseHookStrings() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* pszRsp = rRspData.szResponse;
    UINT32 uiCommand = (UINT32)rRspData.pContextData;

    if (NULL == pszRsp)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseHookStrings() - Response string is NULL!\r\n");
        goto Error;
    }

    // Skip "<prefix>"
    if (!SkipRspStart(pszRsp, m_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseHookStrings() - Could not skip response prefix.\r\n");
        goto Error;
    }

    switch(uiCommand)
    {
        case RIL_OEM_HOOK_STRING_GET_ATR:
            res = ParseXGATR(pszRsp, rRspData);
            break;

        case RIL_OEM_HOOK_STRING_THERMAL_GET_SENSOR:
        case RIL_OEM_HOOK_STRING_THERMAL_SET_THRESHOLD:
            res = ParseXDRV(pszRsp, rRspData);
            break;

        case RIL_OEM_HOOK_STRING_GET_GPRS_CELL_ENV:
            res = ParseCGED(pszRsp, rRspData);
            break;

        case RIL_OEM_HOOK_STRING_DEBUG_SCREEN_COMMAND:
            res = ParseXCGEDPAGE(pszRsp, rRspData);
            break;

        case RIL_OEM_HOOK_STRING_GET_SMS_TRANSPORT_MODE:
            res = ParseCGSMS(pszRsp, rRspData);
            break;

        case RIL_OEM_HOOK_STRING_SET_MODEM_AUTO_FAST_DORMANCY:
        case RIL_OEM_HOOK_STRING_RELEASE_ALL_CALLS:
        case RIL_OEM_HOOK_STRING_SET_SMS_TRANSPORT_MODE:
            // no need for a parse function as this AT command only returns "OK"
            res = RRIL_RESULT_OK;
            break;

#if defined(M2_DUALSIM_FEATURE_ENABLED)
        case RIL_OEM_HOOK_STRING_SWAP_PS:
            res = ParseSwapPS(pszRsp, rRspData);
            break;
#endif // M2_DUALSIM_FEATURE_ENABLED

        default:
            RIL_LOG_INFO("CTE_INF_6260::ParseHookStrings() - Parsing not implemented for uiCommand: %u\r\n",
                         uiCommand);
            break;
    }

Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseHookStrings() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_BAND_MODE 65
//
RIL_RESULT_CODE CTE_INF_6260::CoreSetBandMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSetBandMode() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    UINT32* pnBandMode;

    if (sizeof(int*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetBandMode() - Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetBandMode() - Passed data pointer was NULL\r\n");
        goto Error;
    }

    pnBandMode = (UINT32*)pData;

    switch(*pnBandMode)
    {
    case 0:
        if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XBANDSEL=0\r"))
        {
            res = RRIL_RESULT_OK;
        }
        break;

    case 1:
        if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XBANDSEL=900\r"))
        {
            res = RRIL_RESULT_OK;
        }
        break;

    case 2:
        if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XBANDSEL=850,1900\r"))
        {
            res = RRIL_RESULT_OK;
        }
        break;

    case 3:
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetBandMode() - Japan region is not supported!\r\n");
        res = RIL_E_GENERIC_FAILURE;
        break;

    case 4:
    case 5:
        if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XBANDSEL=850,900,1800\r"))
        {
            res = RRIL_RESULT_OK;
        }
        break;

    default:
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetBandMode() - Undefined region code: %d\r\n", *pnBandMode);
        res = RIL_E_GENERIC_FAILURE;
        break;
    }

Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSetBandMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseSetBandMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseSetBandMode() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_OK;
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseSetBandMode() - Exit\r\n");
    return res;
}


//
// RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE 66
//
RIL_RESULT_CODE CTE_INF_6260::CoreQueryAvailableBandMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreQueryAvailableBandMode() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XBANDSEL=?\r"))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreQueryAvailableBandMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseQueryAvailableBandMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseQueryAvailableBandMode() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    const char* szRsp = rRspData.szResponse;
    const char* szDummy = NULL;

    BOOL automatic = FALSE;
    BOOL euro = FALSE;
    BOOL usa = FALSE;
    BOOL japan = FALSE;
    BOOL aus = FALSE;
    BOOL aus2 = FALSE;
    UINT32 count = 0, count2 = 0;

    int * pModes = NULL;

    // Skip "+XBANDSEL: "
    if (!FindAndSkipString(szRsp, "+XBANDSEL: ", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseQueryAvailableBandMode() - Could not skip \"+XBANDSEL: \".\r\n");
        goto Error;
    }

    // Check for automatic
    if (FindAndSkipString(szRsp, "(0", szDummy) ||
        FindAndSkipString(szRsp, ",0,", szDummy) ||
        FindAndSkipString(szRsp, ",0)", szDummy))
    {
        automatic = TRUE;
    }

    // Check for 800 Mhz
    if (FindAndSkipString(szRsp, "(800", szDummy) ||
        FindAndSkipString(szRsp, ",800,", szDummy) ||
        FindAndSkipString(szRsp, ",800)", szDummy))
    {
        japan = TRUE;
    }

    // Check for 850 Mhz
    if (FindAndSkipString(szRsp, "(850", szDummy) ||
        FindAndSkipString(szRsp, ",850,", szDummy) ||
        FindAndSkipString(szRsp, ",850)", szDummy))
    {
        usa = TRUE;
        aus = TRUE;
        aus2 = TRUE;
    }

    // Check for 900 Mhz
    if (FindAndSkipString(szRsp, "(900", szDummy) ||
        FindAndSkipString(szRsp, ",900,", szDummy) ||
        FindAndSkipString(szRsp, ",900)", szDummy))
    {
        euro = TRUE;
        aus = TRUE;
        aus2 = TRUE;
    }

    // Check for 1800 Mhz
    if (FindAndSkipString(szRsp, "(1800", szDummy) ||
        FindAndSkipString(szRsp, ",1800,", szDummy) ||
        FindAndSkipString(szRsp, ",1800)", szDummy))
    {
        euro = TRUE;
        aus = TRUE;
        aus2 = TRUE;
    }

    // Check for 1900 Mhz
    if (FindAndSkipString(szRsp, "(1900", szDummy) ||
        FindAndSkipString(szRsp, ",1900,", szDummy) ||
        FindAndSkipString(szRsp, ",1900)", szDummy))
    {
        usa = TRUE;
    }

    // Check for 2000 Mhz
    if (FindAndSkipString(szRsp, "(2000", szDummy) ||
        FindAndSkipString(szRsp, ",2000,", szDummy) ||
        FindAndSkipString(szRsp, ",2000)", szDummy))
    {
        japan = TRUE;
        euro = TRUE;
        aus = TRUE;
    }

    // pModes is an array of ints where the first is the number of ints to follow
    // then an int representing each available band mode where:
    /* 0 for "unspecified" (selected by baseband automatically)
    *  1 for "EURO band" (GSM-900 / DCS-1800 / WCDMA-IMT-2000)
    *  2 for "US band" (GSM-850 / PCS-1900 / WCDMA-850 / WCDMA-PCS-1900)
    *  3 for "JPN band" (WCDMA-800 / WCDMA-IMT-2000)
    *  4 for "AUS band" (GSM-900 / DCS-1800 / WCDMA-850 / WCDMA-IMT-2000)
    *  5 for "AUS band 2" (GSM-900 / DCS-1800 / WCDMA-850)
    */

    //  Get total count
    if (automatic)
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseQueryAvailableBandMode() - automatic\r\n");
        count++;
    }

    if (euro)
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseQueryAvailableBandMode() - euro\r\n");
        count++;
    }

    if (usa)
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseQueryAvailableBandMode() - usa\r\n");
        count++;
    }

    if (japan)
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseQueryAvailableBandMode() - japan\r\n");
        count++;
    }

    if (aus)
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseQueryAvailableBandMode() - aus\r\n");
        count++;
    }

    if (aus2)
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseQueryAvailableBandMode() - aus2\r\n");
        count++;
    }

    RIL_LOG_INFO("CTE_INF_6260::ParseQueryAvailableBandMode() - count=%d\r\n", count);

    if (0 == count)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseQueryAvailableBandMode() - Cannot have a count of zero.\r\n");
        goto Error;
    }

    //  Alloc array and don't forget about the first entry for size.
    pModes = (int*)malloc( (1 + count) * sizeof(int));
    if (NULL == pModes)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseQueryAvailableBandMode() - Could not allocate memory for response.\r\n");
        goto Error;
    }
    memset(pModes, 0, (1 + count) * sizeof(int));

    pModes[0] = count + 1;

    //  Go assign bands in array.
    count2 = 1;
    if (automatic)
    {
        pModes[count2] = 0;
        count2++;
    }

    if (euro)
    {
        pModes[count2] = 1;
        count2++;
    }

    if (usa)
    {
        pModes[count2] = 2;
        count2++;
    }

    if (japan)
    {
        pModes[count2] = 3;
        count2++;
    }

    if (aus)
    {
        pModes[count2] = 4;
        count2++;
    }

    if (aus2)
    {
        pModes[count2] = 5;
        count2++;
    }


    res = RRIL_RESULT_OK;

    rRspData.pData   = (void*)pModes;
    rRspData.uiDataSize  = sizeof(int) * (count + 1);

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pModes);
        pModes = NULL;
    }


    RIL_LOG_VERBOSE("CTE_INF_6260::ParseQueryAvailableBandMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_GET_PROFILE 67
//
RIL_RESULT_CODE CTE_INF_6260::CoreStkGetProfile(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_INFO("CTE_INF_6260::CoreStkGetProfile() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+STKPROF?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_INFO("CTE_INF_6260::CoreStkGetProfile() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseStkGetProfile(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_INF_6260::ParseStkGetProfile() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* pszRsp = rRspData.szResponse;
    char* pszTermProfile = NULL;
    UINT32 uiLength = 0;

    if (NULL == pszRsp)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkGetProfile() - Response string is NULL!\r\n");
        goto Error;
    }

    pszTermProfile = (char*)malloc(MAX_BUFFER_SIZE);
    if (NULL == pszTermProfile)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkGetProfile() - Could not allocate memory for a %u-char string.\r\n", MAX_BUFFER_SIZE);
        goto Error;
    }

    memset(pszTermProfile, 0x00, MAX_BUFFER_SIZE);

    // Parse "<prefix>+STKPROF: <length>,<data><postfix>"
    if (!SkipRspStart(pszRsp, m_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkGetProfile() - Could not skip response prefix.\r\n");
        goto Error;
    }

    if (!SkipString(pszRsp, "+STKPROF: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkGetProfile() - Could not skip \"+STKPROF: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt32(pszRsp, uiLength, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkGetProfile() - Could not extract length value.\r\n");
        goto Error;
    }

    if (SkipString(pszRsp, ",", pszRsp))
    {
        if (!ExtractQuotedString(pszRsp, pszTermProfile, MAX_BUFFER_SIZE, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkGetProfile() - Could not extract the terminal profile.\r\n");
            goto Error;
        }
    }

    if (!SkipRspEnd(pszRsp, m_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkGetProfile() - Could not extract the response end.\r\n");
        goto Error;
    }

    rRspData.pData   = (void*)pszTermProfile;
    rRspData.uiDataSize  = uiLength;

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pszTermProfile);
        pszTermProfile = NULL;
    }

    RIL_LOG_INFO("CTE_INF_6260::ParseStkGetProfile() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_SET_PROFILE 68
//
RIL_RESULT_CODE CTE_INF_6260::CoreStkSetProfile(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_INFO("CTE_INF_6260::CoreStkSetProfile() - Enter\r\n");
    char* pszTermProfile = NULL;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (0 == uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreStkSetProfile() - Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreStkSetProfile() - Passed data pointer was NULL\r\n");
        goto Error;
    }

    // extract data
    pszTermProfile = (char*)pData;

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+STKPROF=%u,\"%s\"\r", uiDataSize, pszTermProfile))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreStkSetProfile() - Could not form string.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_INFO("CTE_INF_6260::CoreStkSetProfile() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseStkSetProfile(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_INF_6260::ParseStkSetProfile() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_INFO("CTE_INF_6260::ParseStkSetProfile() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND 69
//
RIL_RESULT_CODE CTE_INF_6260::CoreStkSendEnvelopeCommand(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_INFO("CTE_INF_6260::CoreStkSendEnvelopeCommand() - Enter\r\n");
    char* pszEnvCommand = NULL;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (sizeof(char*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreStkSendEnvelopeCommand() - Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreStkSendEnvelopeCommand() - Passed data pointer was NULL\r\n");
        goto Error;
    }

    // extract data
    pszEnvCommand = (char*)pData;

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+SATE=\"%s\"\r", pszEnvCommand))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreStkSendEnvelopeCommand() - Could not form string.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_INFO("CTE_INF_6260::CoreStkSendEnvelopeCommand() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseStkSendEnvelopeCommand(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_INF_6260::ParseStkSendEnvelopeCommand() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    UINT32 uiSw1 = 0, uiSw2 = 0;
    UINT32 uiEventType = 0;
    UINT32 uiEnvelopeType = 0;
    char* pszRespData = NULL;

    const char* pszRsp = rRspData.szResponse;
    if (NULL == pszRsp)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - Response string is NULL!\r\n");
        goto Error;
    }

    // Parse "<prefix>"
    if (!SkipRspStart(pszRsp, m_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - Could not skip over response prefix.\r\n");
        goto Error;
    }

    // Parse "+SATE: "
    if (!SkipString(pszRsp, "+SATE: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - Could not skip over \"+SATE: \".\r\n");
        goto Error;
    }

    // Parse "<sw1>"
    if (!ExtractUInt32(pszRsp, uiSw1, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - Could not extract sw1.\r\n");
        goto Error;
    }

    // Parse ",<sw2>"
    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt32(pszRsp, uiSw2, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - Could not extract sw2.\r\n");
        goto Error;
    }

    // Parse ",<event_type>"
    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt32(pszRsp, uiEventType, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - Could not extract event type.\r\n");
        goto Error;
    }

    // Parse ",<envelope_type>"
    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt32(pszRsp, uiEnvelopeType, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - Could not extract envelope type.\r\n");
        goto Error;
    }

    RIL_LOG_INFO(" sw1: %u, sw2: %u\r\n", uiSw1, uiSw2);
    RIL_LOG_INFO(" event type: %u\r\n", uiEventType);
    RIL_LOG_INFO(" envelope type: %u\r\n", uiEnvelopeType);

    // Parse "," if response data exists
    if (SkipString(pszRsp, ",", pszRsp))
    {
        pszRespData = (char*)malloc(MAX_BUFFER_SIZE);
        if (NULL == pszRespData)
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - Could not alloc mem for command.\r\n");
            goto Error;
        }

        memset(pszRespData, 0x00, MAX_BUFFER_SIZE);

        // Parse ",<response_data>"
        if (!ExtractUnquotedString(pszRsp, m_cTerminator, pszRespData, MAX_BUFFER_SIZE, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - Could not parse response data.\r\n");
            goto Error;
        }

        RIL_LOG_INFO("CTE_INF_6260::ParseStkSendEnvelopeCommand() - response data: \"%s\".\r\n", pszRespData);
    }
    else
    {
       pszRespData = NULL;
    }

    // Parse "<postfix>"
    if (!SkipRspEnd(pszRsp, m_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - Could not extract the response end.\r\n");
        goto Error;
    }

    rRspData.pData   = (void*)pszRespData;
    rRspData.uiDataSize  = sizeof(char*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pszRespData);
        pszRespData = NULL;
    }

    RIL_LOG_INFO("CTE_INF_6260::ParseStkSendEnvelopeCommand() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE 70
//
RIL_RESULT_CODE CTE_INF_6260::CoreStkSendTerminalResponse(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_INFO("CTE_INF_6260::CoreStkSendTerminalResponse() - Enter\r\n");
    char* pszTermResponse = NULL;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreStkSendTerminalResponse() - Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != sizeof(char *))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreStkSendTerminalResponse() - Invalid data size.\r\n");
        goto Error;
    }

    RIL_LOG_INFO(" uiDataSize= %d\r\n", uiDataSize);

    pszTermResponse = (char *)pData;

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+SATR=\"%s\"\r", pszTermResponse))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreStkSendTerminalResponse() - Could not form string.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_INFO("CTE_INF_6260::CoreStkSendTerminalResponse() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseStkSendTerminalResponse(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseStkSendTerminalResponse() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTE_INF_6260::ParseStkSendTerminalResponse() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM 71
//
RIL_RESULT_CODE CTE_INF_6260::CoreStkHandleCallSetupRequestedFromSim(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_INFO("CTE_INF_6260::CoreStkHandleCallSetupRequestedFromSim() - Enter\r\n");
    int nConfirmation = 0;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreStkHandleCallSetupRequestedFromSim() - Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != sizeof(int *))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreStkHandleCallSetupRequestedFromSim() - Invalid data size.\r\n");
        goto Error;
    }

    nConfirmation = ((int *)pData)[0];
    if (0 == nConfirmation)
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+SATD=0\r"))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::CoreStkHandleCallSetupRequestedFromSim() - Could not form string.\r\n");
            goto Error;
        }
    }
    else
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+SATD=1\r"))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::CoreStkHandleCallSetupRequestedFromSim() - Could not form string.\r\n");
            goto Error;
        }
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_INFO("CTE_INF_6260::CoreStkHandleCallSetupRequestedFromSim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseStkHandleCallSetupRequestedFromSim(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseStkHandleCallSetupRequestedFromSim() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTE_INF_6260::ParseStkHandleCallSetupRequestedFromSim() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
//
RIL_RESULT_CODE CTE_INF_6260::CoreSetPreferredNetworkType(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSetPreferredNetworkType() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_PreferredNetworkType networkType = PREF_NET_TYPE_GSM_WCDMA; // 0

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetPreferredNetworkType() - Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != sizeof(RIL_PreferredNetworkType *))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetPreferredNetworkType() - Invalid data size.\r\n");
        goto Error;
    }

    networkType = ((RIL_PreferredNetworkType *)pData)[0];

    // if network type already set, NO-OP this command
    if (m_currentNetworkType == networkType)
    {
        rReqData.szCmd1[0] = '\0';
        res = RRIL_RESULT_OK;
        RIL_LOG_INFO("CTE_INF_6260::CoreSetPreferredNetworkType() - Network type {%d} already set.\r\n", networkType);
        goto Error;
    }

    switch (networkType)
    {
        case PREF_NET_TYPE_GSM_WCDMA: // WCDMA Preferred

            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XRAT=1,2\r", sizeof(rReqData.szCmd1) ))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetPreferredNetworkType() - Can't construct szCmd1 networkType=%d\r\n", networkType);
                goto Error;
            }

           break;

        case PREF_NET_TYPE_GSM_ONLY: // GSM Only

            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XRAT=0\r", sizeof(rReqData.szCmd1) ))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetPreferredNetworkType() - Can't construct szCmd1 networkType=%d\r\n", networkType);
                goto Error;
            }

            break;

        case PREF_NET_TYPE_WCDMA: // WCDMA Only

            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XRAT=2\r", sizeof(rReqData.szCmd1) ))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetPreferredNetworkType() - Can't construct szCmd1 networkType=%d\r\n", networkType);
                goto Error;
            }

            break;

        default:
            RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetPreferredNetworkType() - Undefined rat code: %d\r\n", networkType);
            res = RIL_E_MODE_NOT_SUPPORTED;
            goto Error;
            break;
    }

    //  Set the context of this command to the network type we're attempting to set
    rReqData.pContextData = (void*)networkType;  // Store this as an int.

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSetPreferredNetworkType() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseSetPreferredNetworkType(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseSetPreferredNetworkType() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_PreferredNetworkType networkType = (RIL_PreferredNetworkType)((int)rRspData.pContextData);
    m_currentNetworkType = networkType;

    RIL_LOG_VERBOSE("CTE_INF_6260::ParseSetPreferredNetworkType() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
//
RIL_RESULT_CODE CTE_INF_6260::CoreGetPreferredNetworkType(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreGetPreferredNetworkType() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+XRAT?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::CoreGetPreferredNetworkType() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseGetPreferredNetworkType(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseGetPreferredNetworkType() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    UINT32 rat = 0;
    UINT32 pref = 0;

    int * pRat = (int*)malloc(sizeof(int));
    if (NULL == pRat)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetPreferredNetworkType() - Could not allocate memory for response.\r\n");
        goto Error;
    }

    // Skip "<prefix>"
    if (!SkipRspStart(pszRsp, m_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetPreferredNetworkType() - Could not skip response prefix.\r\n");
        goto Error;
    }

    // Skip "+XRAT: "
    if (!SkipString(pszRsp, "+XRAT: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetPreferredNetworkType() - Could not skip \"+XRAT: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt32(pszRsp, rat, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetPreferredNetworkType() - Could not extract rat value.\r\n");
        goto Error;
    }

    if (FindAndSkipString(pszRsp, ",", pszRsp))
    {
        if (!ExtractUInt32(pszRsp, pref, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetPreferredNetworkType() - Could not find and skip pref value even though it was expected.\r\n");
            goto Error;
        }
    }

    switch(rat)
    {
        case 0:     // GSM Only
        {
            pRat[0] = PREF_NET_TYPE_GSM_ONLY;
            m_currentNetworkType = PREF_NET_TYPE_GSM_ONLY;
            break;
        }

        case 1:     // WCDMA Preferred
        {
            pRat[0] = PREF_NET_TYPE_GSM_WCDMA;
            m_currentNetworkType = PREF_NET_TYPE_GSM_WCDMA;
            break;
        }

        case 2:     // WCDMA only
        {
            pRat[0] = PREF_NET_TYPE_WCDMA;
            m_currentNetworkType = PREF_NET_TYPE_WCDMA;
            break;
        }

        default:
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetPreferredNetworkType() - Unexpected rat found: %d. Failing out.\r\n", rat);
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

    RIL_LOG_VERBOSE("CTE_INF_6260::ParseGetPreferredNetworkType() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_NEIGHBORING_CELL_IDS 75
//
RIL_RESULT_CODE CTE_INF_6260::CoreGetNeighboringCellIDs(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreGetNeighboringCellIDs() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+XCELLINFO?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::CoreGetNeighboringCellIDs() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseGetNeighboringCellIDs(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseGetNeighboringCellIDs() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    UINT32 nIndex = 0, nTotal = 0;
    UINT32 nLAC = 0, nCI = 0;
    UINT32 nRSSI = 0;
    UINT32 nScramblingCode = 0;
    UINT32 nMode = 0;

    P_ND_N_CELL_DATA pCellData = NULL;


    //  Data is either (according to C_AT_FS_SUNRISE_Rev6.0.pdf AT spec)
    //  GSM cells:
    //  +XCELLINFO: 0,<MCC>,<MNC>,<LAC>,<CI>,<RxLev>,<BSIC>,<BCCH_Car>,<true_freq>,<t_advance>
    //  +XCELLINFO: 1,<LAC>,<CI>,<RxLev>,<BSIC>,<BCCH_Car>  // one row for each neighboring cell [0..6]
    //  For GSM cells, according to ril.h, must return (LAC/CID , received RSSI)
    //
    //  UMTS FDD cells:
    //  +XCELLINFO: 2,<MCC>,<MNC>,<LAC>,<UCI>,<scrambling_code>,<dl_frequency>,<ul_frequency>
    //  +XCELLINFO: 2,<scrambling_code>,<dl_frequency>,<UTRA_rssi>,<rscp>,<ecn0>,<pathloss>  // If UMTS
    //                          // has any ACTIVE SET neighboring cell
    //  +XCELLINFO: 3,<scrambling_code>,<dl_frequency>,<UTRA_rssi>,<rscp>,<ecn0>,<pathloss>  // One row
    //                          // for each intra-frequency neighboring cell [1..32] for each
    //                          // frequency [0..8] in BA list
    //  For UMTS cells, according to ril.h, must return (Primary scrambling code , received signal code power)
    //  NOTE that for first UMTS format above, there is no <rcsp> parameter.
    //
    //  A <type> of 0 or 1 = GSM.  A <type> of 2,3 = UMTS.


    pCellData = (P_ND_N_CELL_DATA)malloc(sizeof(S_ND_N_CELL_DATA));
    if (NULL == pCellData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - Could not allocate memory for a S_ND_N_CELL_DATA struct.\r\n");
        goto Error;
    }
    memset(pCellData, 0, sizeof(S_ND_N_CELL_DATA));


    // Loop on +XCELLINFO until no more entries are found
    nIndex = 0;
    while (FindAndSkipString(pszRsp, "+XCELLINFO: ", pszRsp))
    {
        nTotal++;
        if (RRIL_MAX_CELL_ID_COUNT == nTotal)
        {
            //  We're full.
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - Exceeded max count = %d\r\n", RRIL_MAX_CELL_ID_COUNT);
            goto Error;
        }

        //  Get <mode>
        if (!ExtractUInt32(pszRsp, nMode, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - cannot extract <mode>\r\n");
            goto Error;
        }

        RIL_LOG_INFO("CTE_INF_6260::ParseGetNeighboringCellIDs() - nTotal=%d, found mode=%d\r\n", nTotal, nMode);

        switch(nMode)
        {
            case 0: // GSM  get (LAC/CI , RxLev)
            {
                //  <LAC> and <CI> are parameters 4 and 5
                if (!FindAndSkipString(pszRsp, ",", pszRsp) ||
                    !FindAndSkipString(pszRsp, ",", pszRsp) ||
                    !FindAndSkipString(pszRsp, ",", pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode 0, cannot skip to LAC and CI\r\n");
                    goto Error;
                }

                //  Read <LAC> and <CI>
                if (!ExtractUInt32(pszRsp, nLAC, pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode 0, could not extract LAC\r\n");
                    goto Error;
                }
                //  Read <CI>
                if ((!SkipString(pszRsp, ",", pszRsp)) ||
                    (!ExtractUInt32(pszRsp, nCI, pszRsp)))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode 0, could not extract CI value\r\n");
                    goto Error;
                }
                //  Read <RxLev>
                if ((!SkipString(pszRsp, ",", pszRsp)) ||
                    (!ExtractUInt32(pszRsp, nRSSI, pszRsp)))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode 0, could not extract RSSI value\r\n");
                    goto Error;
                }

                //  We now have what we want, copy to main structure.
                pCellData->pnCellData[nIndex].cid = pCellData->pnCellCIDBuffers[nIndex];

                //  cid = upper 16 bits (LAC), lower 16 bits (CID)
                snprintf(pCellData->pnCellCIDBuffers[nIndex], CELL_ID_ARRAY_LENGTH, "%04X%04X", nLAC, nCI);

                RIL_LOG_INFO("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode 0 GSM LAC,CID index=[%d]  cid=[%s]\r\n", nIndex, pCellData->pnCellCIDBuffers[nIndex]);

                //  rssi = <RxLev>

                //  Convert RxLev to asu (0 to 31).
                //  For GSM, it is in "asu" ranging from 0 to 31 (dBm = -113 + 2*asu)
                //  0 means "-113 dBm or less" and 31 means "-51 dBm or greater"

                //  Divide nRSSI by 2 since rxLev = [0-63] and assume ril.h wants 0-31 like AT+CSQ response.
                pCellData->pnCellData[nIndex].rssi = (int)(nRSSI / 2);
                RIL_LOG_INFO("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode 0 GSM rxlev index=[%d]  rssi=[%d]\r\n", nIndex, pCellData->pnCellData[nIndex].rssi);
            }
            break;

            case 1: // GSM  get (LAC/CI , RxLev)
            {
                //  <LAC> and <CI> are parameters 2 and 3
                //  Read <LAC> and <CI>
                if (!SkipString(pszRsp, ",", pszRsp) ||
                    !ExtractUInt32(pszRsp, nLAC, pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode 1, could not extract LAC\r\n");
                    goto Error;
                }
                //  Read <CI>
                if ((!SkipString(pszRsp, ",", pszRsp)) ||
                    (!ExtractUInt32(pszRsp, nCI, pszRsp)))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode 1, could not extract CI value\r\n");
                    goto Error;
                }
                //  Read <RxLev>
                if ((!SkipString(pszRsp, ",", pszRsp)) ||
                    (!ExtractUInt32(pszRsp, nRSSI, pszRsp)))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode 1, could not extract RSSI value\r\n");
                    goto Error;
                }

                //  We now have what we want, copy to main structure.
                pCellData->pnCellData[nIndex].cid = pCellData->pnCellCIDBuffers[nIndex];

                //  cid = upper 16 bits (LAC), lower 16 bits (CID)
                snprintf(pCellData->pnCellCIDBuffers[nIndex], CELL_ID_ARRAY_LENGTH, "%04X%04X", nLAC, nCI);

                RIL_LOG_INFO("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode 1 GSM LAC,CID index=[%d]  cid=[%s]\r\n", nIndex, pCellData->pnCellCIDBuffers[nIndex]);

                //  rssi = <RxLev>

                //  May have to convert RxLev to asu (0 to 31).
                //  For GSM, it is in "asu" ranging from 0 to 31 (dBm = -113 + 2*asu)
                //  0 means "-113 dBm or less" and 31 means "-51 dBm or greater"

                //  Divide nRSSI by 2 since rxLev = [0-63] and assume ril.h wants 0-31 like AT+CSQ response.
                pCellData->pnCellData[nIndex].rssi = (int)(nRSSI / 2);
                RIL_LOG_INFO("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode 1 GSM rxlev index=[%d]  rssi=[%d]\r\n", nIndex, pCellData->pnCellData[nIndex].rssi);
            }
            break;

            case 2: // UMTS  get (scrambling_code , rscp)
            {
                //  This can be either first case or second case.
                //  Loop and count number of commas
                char szBuf[MAX_BUFFER_SIZE] = {0};
                const char *szDummy = pszRsp;
                UINT32 nCommaCount = 0;
                if (!ExtractUnquotedString(pszRsp, m_cTerminator, szBuf, MAX_BUFFER_SIZE, szDummy))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode 2, could not extract temp buf\r\n");
                    goto Error;
                }
                for (UINT32 n=0; n<strlen(szBuf); n++)
                {
                    if (szBuf[n] == ',')
                        nCommaCount++;
                }
                RIL_LOG_INFO("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode 2, found %d commas\r\n", nCommaCount);

                if (6 != nCommaCount)
                {
                    //  Handle first case here
                    //  <scrambling_code> is parameter 6
                    if (!FindAndSkipString(pszRsp, ",", pszRsp) ||
                        !FindAndSkipString(pszRsp, ",", pszRsp) ||
                        !FindAndSkipString(pszRsp, ",", pszRsp) ||
                        !FindAndSkipString(pszRsp, ",", pszRsp) ||
                        !FindAndSkipString(pszRsp, ",", pszRsp))
                    {
                        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode 2, could not skip to scrambling code\r\n");
                        goto Error;
                    }
                    if (!ExtractUInt32(pszRsp, nScramblingCode, pszRsp))
                    {
                        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode 2, could not extract scrambling code\r\n");
                        goto Error;
                    }
                    //  Cannot get <rscp> as it does not exist.
                    //  We now have what we want, copy to main structure.
                    //  cid = <scrambling code> as char *
                    pCellData->pnCellData[nIndex].cid = pCellData->pnCellCIDBuffers[nIndex];
                    snprintf(pCellData->pnCellCIDBuffers[nIndex], CELL_ID_ARRAY_LENGTH, "%08x", nScramblingCode);

                    RIL_LOG_INFO("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode 2 UMTS scramblingcode index=[%d]  cid=[%s]\r\n", nIndex, pCellData->pnCellCIDBuffers[nIndex]);

                    //  rssi = <rscp>
                    //  Note that <rscp> value does not exist with this response.
                    //  Set to 0 for now.
                    pCellData->pnCellData[nIndex].rssi = 0;
                    RIL_LOG_INFO("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode 2 UMTS rscp index=[%d]  rssi=[%d]\r\n", nIndex, pCellData->pnCellData[nIndex].rssi);

                    break;
                }
                else
                {
                    //  fall through to case 3 as it is parsed the same.
                    RIL_LOG_INFO("CTE_INF_6260::ParseGetNeighboringCellIDs() - comma count = 6, drop to case 3\r\n");
                }
            }


            case 3: // UMTS  get (scrambling_code , rscp)
            {
                //  scrabling_code is parameter 2
                //  Read <scrambling_code>
                if ((!SkipString(pszRsp, ",", pszRsp)) ||
                    (!ExtractUInt32(pszRsp, nScramblingCode, pszRsp)))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode %d, could not extract scrambling code\r\n", nMode);
                    goto Error;
                }

                //  <rscp> is parameter 5
                if (!FindAndSkipString(pszRsp, ",", pszRsp) ||
                    !FindAndSkipString(pszRsp, ",", pszRsp) ||
                    !FindAndSkipString(pszRsp, ",", pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode %d, could not skip to rscp\r\n", nMode);
                    goto Error;
                }
                //  read <rscp>
                if (!ExtractUInt32(pszRsp, nRSSI, pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode %d, could not extract rscp\r\n", nMode);
                    goto Error;
                }

                //  We now have what we want, copy to main structure.
                //  cid = <scrambling code> as char *
                pCellData->pnCellData[nIndex].cid = pCellData->pnCellCIDBuffers[nIndex];
                snprintf(pCellData->pnCellCIDBuffers[nIndex], CELL_ID_ARRAY_LENGTH, "%08x", nScramblingCode);

                RIL_LOG_INFO("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode %d UMTS scramblingcode index=[%d]  cid=[%s]\r\n", nMode, nIndex, pCellData->pnCellCIDBuffers[nIndex]);

                //  rssi = <rscp>
                //  Assume that rssi value is same as <rscp> value and no conversion needs to be done.
                pCellData->pnCellData[nIndex].rssi = (int)nRSSI;
                RIL_LOG_INFO("CTE_INF_6260::ParseGetNeighboringCellIDs() - mode %d UMTS rscp index=[%d]  rssi=[%d]\r\n", nMode, nIndex, pCellData->pnCellData[nIndex].rssi);
            }
            break;

            default:
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - Invalid nMode=[%d]\r\n", nMode);
                goto Error;
            }
            break;
        }

        //  Connect the pointer
        pCellData->pnCellPointers[nIndex] = &(pCellData->pnCellData[nIndex]);

        nIndex++;
    }


    if (nTotal > 0)
    {
        rRspData.pData  = (void*)pCellData;
        rRspData.uiDataSize = nTotal * sizeof(RIL_NeighboringCell*);
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


    RIL_LOG_VERBOSE("CTE_INF_6260::ParseGetNeighboringCellIDs() - Exit\r\n");
    return res;

}

//
// RIL_REQUEST_SET_TTY_MODE 80
//
RIL_RESULT_CODE CTE_INF_6260::CoreSetTtyMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_INFO("CTE_INF_6260::CoreSetTtyMode() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int nTtyMode = 0;

    if (sizeof(int*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetTtyMode() - Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetTtyMode() - Passed data pointer was NULL\r\n");
        goto Error;
    }

    // extract the data
    nTtyMode = ((int*)pData)[0];

    RIL_LOG_INFO(" Set TTY mode: %d\r\n", nTtyMode);

    // check for invalid value
    if ((nTtyMode < 0) || (nTtyMode > 3))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetTtyMode() - Undefined CTM/TTY mode: %d\r\n", nTtyMode);
        res = RIL_E_GENERIC_FAILURE;
        goto Error;
    }

    //  Need to switch 2 and 3 for this modem.
    if (2 == nTtyMode)
        nTtyMode = 3;
    else if (3 == nTtyMode)
        nTtyMode = 2;

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XCTMS=%d\r", nTtyMode))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetTtyMode() - Could not form string.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;
Error:
    RIL_LOG_INFO("CTE_INF_6260::CoreSetTtyMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseSetTtyMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseSetTtyMode() - Enter\r\n");
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseSetTtyMode() - Exit\r\n");

    return RRIL_RESULT_OK;
}

//
// RIL_REQUEST_QUERY_TTY_MODE 81
//
RIL_RESULT_CODE CTE_INF_6260::CoreQueryTtyMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_INFO("CTE_INF_6260::CoreQueryTtyMode() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XCTMS?\r"))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_INFO("CTE_INF_6260::CoreQueryTtyMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseQueryTtyMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_INF_6260::ParseQueryTtyMode() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* szRsp = rRspData.szResponse;
    UINT32 uiTtyMode = 0;

    int* pnMode = (int*)malloc(sizeof(int));
    if (NULL == pnMode)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseQueryTtyMode() - Could not allocate memory for response.\r\n");
        goto Error;
    }

    // Parse prefix
    if (!FindAndSkipString(szRsp, "+XCTMS: ", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseQueryTtyMode() - Unable to parse \"CTM/TTY mode: \" prefix.!\r\n");
        goto Error;
    }

    // Parse <mode>
    if (!ExtractUInt32(szRsp, uiTtyMode, szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseQueryTtyMode() - Unable to parse <mode>!\r\n");
        goto Error;
    }

    //  Need to switch 2 and 3 for this modem.
    if (2 == uiTtyMode)
        uiTtyMode = 3;
    else if (3 == uiTtyMode)
        uiTtyMode = 2;

    pnMode[0] = (int)uiTtyMode;

    RIL_LOG_INFO(" Current TTY mode: %d\r\n", uiTtyMode);

    rRspData.pData  = (void*)pnMode;
    rRspData.uiDataSize = sizeof(int*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pnMode);
        pnMode = NULL;
    }
    RIL_LOG_INFO("CTE_INF_6260::ParseQueryTtyMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_REPORT_SMS_MEMORY_STATUS 102
//
RIL_RESULT_CODE CTE_INF_6260::CoreReportSmsMemoryStatus(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreReportSmsMemoryStatus - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    int nSmsMemoryStatus = 0;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreReportSmsMemoryStatus() - Data pointer is NULL\r\n");
        goto Error;
    }

    nSmsMemoryStatus = ((int *)pData)[0];

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
            ((nSmsMemoryStatus == 1) ? "AT+XTESM=0\r" : "AT+XTESM=1\r")) )
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreReportSmsMemoryStatus() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseReportSmsMemoryStatus(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseReportSmsMemoryStatus() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_OK;
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseReportSmsMemoryStatus() - Exit\r\n");
    return res;
}


//
// RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING 103
//
RIL_RESULT_CODE CTE_INF_6260::CoreReportStkServiceRunning(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreReportStkServiceRunning - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XSATK=1,1\r"))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::CoreReportStkServiceRunning() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseReportStkServiceRunning(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseReportStkServiceRunning() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_OK;
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseReportStkServiceRunning() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_VOICE_RADIO_TECH 108
//
RIL_RESULT_CODE CTE_INF_6260::CoreVoiceRadioTech(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreVoiceRadioTech() - Enter / Exit\r\n");

    return CoreGetPreferredNetworkType(rReqData, pData, uiDataSize);
}

RIL_RESULT_CODE CTE_INF_6260::ParseVoiceRadioTech(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseVoiceRadioTech() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    UINT32 uiAcT = 0;
    UINT32 uiPreferredAcT = 0;

    int* pCurRadioTech = (int*)malloc(sizeof(int));
    if (NULL == pCurRadioTech)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseVoiceRadioTech() - Could not allocate memory for response.\r\n");
        goto Error;
    }

    // Skip "<prefix>"
    if (!SkipRspStart(pszRsp, m_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseVoiceRadioTech() - Could not skip response prefix.\r\n");
        goto Error;
    }

    // Skip "+XRAT: "
    if (!SkipString(pszRsp, "+XRAT: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseVoiceRadioTech() - Could not skip \"+XRAT: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt32(pszRsp, uiAcT, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseVoiceRadioTech() - Could not extract rat value.\r\n");
        goto Error;
    }

    if (FindAndSkipString(pszRsp, ",", pszRsp))
    {
        if (!ExtractUInt32(pszRsp, uiPreferredAcT, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseVoiceRadioTech() - Could not find and skip pref value even though it was expected.\r\n");
            goto Error;
        }
    }

    switch (uiAcT)
    {
        case 0: // GSM only
            RIL_LOG_INFO("CTE_INF_6260::ParseVoiceRadioTech() - AcT = %d, GSM\r\n", uiAcT);
            pCurRadioTech[0] = RADIO_TECH_GSM;
            break;

        case 1: // GSM / UMTS dual mode
            RIL_LOG_INFO("CTE_INF_6260::ParseVoiceRadioTech() - AcT = %d, GSM/UMTS, preferredAcT = %d\r\n",
                uiAcT, uiPreferredAcT);
            pCurRadioTech[0] = (uiPreferredAcT == 2) ? RADIO_TECH_UMTS : RADIO_TECH_GSM;
            break;

        case 2: // UMTS only
            RIL_LOG_INFO("CTE_INF_6260::ParseVoiceRadioTech() - AcT = %d, UMTS\r\n", uiAcT);
            pCurRadioTech[0] = RADIO_TECH_UMTS;
            break;

        default:
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseVoiceRadioTech() - Unsupported AcT = %d\r\n", uiAcT);
            pCurRadioTech[0] = RADIO_TECH_UNKNOWN;
            goto Error;
    }

    rRspData.pData = (void*)pCurRadioTech;
    rRspData.uiDataSize = sizeof(int*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pCurRadioTech);
        pCurRadioTech = NULL;
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::ParseVoiceRadioTech() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::CreateGetThermalSensorValuesReq(REQUEST_DATA& rReqData,
                                                    const char** pszRequest, const UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CreateGetThermalSensorValuesReq() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int sensorId;

    if (pszRequest == NULL || '\0' == pszRequest[0])
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateGetThermalSensorValuesReq() - pszRequest was NULL\r\n");
        goto Error;
    }

    if (uiDataSize < (2 * sizeof(char *)))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateGetThermalSensorValuesReq() : received_size < required_size\r\n");
        goto Error;
    }

    if (sscanf(pszRequest[1], "%d", &sensorId) == EOF)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateGetThermalSensorReq() - cannot convert %s to int\r\n", pszRequest[1]);
        goto Error;
    }

    RIL_LOG_INFO("CTE_INF_6260::CreateGetThermalSensorValuesReq() - sensorId=[%d]\r\n", sensorId);

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XDRV=5,9,%d\r", sensorId))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateGetThermalSensorValuesReq() - Can't construct szCmd1.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;
Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::CreateGetThermalSensorValuesReq() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::CreateActivateThermalSensorInd(REQUEST_DATA& rReqData,
                                                        const char** pszRequest,
                                                        const UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CreateActivateThermalSensorInd() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int sensorId = 0;
    int minThersholdTemp = 0;
    int maxThersholdTemp = 0;
    char szActivate[10] = {0};
    int noOfVariablesFilled = 0;
    const int MAX_NUM_OF_INPUT_DATA = 4;

    if (pszRequest == NULL || '\0' == pszRequest[0])
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateActivateThermalSensorInd() - pszRequest was NULL\r\n");
        goto Error;
    }

    if (uiDataSize < (2 * sizeof(char *)))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateActivateThermalSensorInd() : received data size is not enough to process the request\r\n");
        goto Error;
    }

    noOfVariablesFilled = sscanf(pszRequest[1], "%s %d %d %d", szActivate, &sensorId, &minThersholdTemp, &maxThersholdTemp);
    if (noOfVariablesFilled < MAX_NUM_OF_INPUT_DATA || noOfVariablesFilled == EOF)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateActivateThermalSensorInd() - Issue with received input data: %d\r\n", noOfVariablesFilled);
        goto Error;
    }

    RIL_LOG_INFO("CTE_INF_6260::CreateActivateThermalSensorInd() - szActivate=[%s],"
                 " sensor Id=[%d], Low Threshold=[%d], Max Threshold=[%d]\r\n",
                 szActivate, sensorId, minThersholdTemp, maxThersholdTemp);

    /*
     * For activating the thermal sensor threshold reached indication, threshold
     * temperatures(minimum,maximum) needs to be sent as part of the set command.
     */
    if (strcmp(szActivate, "true") == 0)
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XDRV=5,14,%d,%d,%d\r",
                                      sensorId, minThersholdTemp, maxThersholdTemp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::CreateActivateThermalSensorInd() - Can't construct szCmd1.\r\n");
            goto Error;
        }
    }
    else
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XDRV=5,14,%d\r", sensorId))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::CreateActivateThermalSensorInd() - Can't construct szCmd1.\r\n");
            goto Error;
        }
    }

    res = RRIL_RESULT_OK;
Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::CreateActivateThermalSensorInd() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::CreateAutonomousFDReq(REQUEST_DATA& rReqData,
                                                        const char** pszRequest,
                                                        const UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CreateAutonomousFDReq() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int noOfVariablesFilled = 0;
    const int MAX_NUM_OF_INPUT_DATA = 3;
    char szFDEnable[10] = {0};
    char szDelayTimer[3] = {0};
    char szSCRITimer[3] = {0};

    if (pszRequest == NULL || '\0' == pszRequest[0])
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateAutonomousFDReq() - pszRequest was NULL\r\n");
        goto Error;
    }

    if (uiDataSize < (2 * sizeof(char *)))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateAutonomousFDReq() : received data size is not enough to process the request\r\n");
        goto Error;
    }

    noOfVariablesFilled = sscanf(pszRequest[1], "%s %s %s", szFDEnable, szDelayTimer, szSCRITimer);
    if (noOfVariablesFilled < MAX_NUM_OF_INPUT_DATA || noOfVariablesFilled == EOF)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateAutonomousFDReq() - Issue with received input data: %d\r\n", noOfVariablesFilled);
        goto Error;
    }

    RIL_LOG_INFO("CTE_INF_6260::CreateAutonomousFDReq() - Activate=[%s], Delay Timer = [%s], SCRI Timer = [%s]\r\n",
                szFDEnable,
                szDelayTimer,
                szSCRITimer);

    if (strcmp(szDelayTimer, "0") == 0) strcpy(szDelayTimer, "");
    if (strcmp(szSCRITimer, "0") == 0) strcpy(szSCRITimer, "");

    if (strcmp(szFDEnable, "true") == 0)
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XFDOR=2,%s,%s\r", szDelayTimer, szSCRITimer))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::CreateAutonomousFDReq - Can't construct szCmd1. 2\r\n");
            goto Error;
        }

    }
    else
    {
        if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XFDOR=3\r", sizeof(rReqData.szCmd1)))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::CreateAutonomousFDReq - Can't construct szCmd1. 3\r\n");
            goto Error;
        }
    }

    res = RRIL_RESULT_OK;
Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::CreateAutonomousFDReq() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::CreateDebugScreenReq(REQUEST_DATA& rReqData,
                                                    const char** pszRequest, const UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CreateDebugScreenReq() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int page_nr;

    if (pszRequest == NULL || '\0' == pszRequest[0])
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateDebugScreenReq() - pszRequest was NULL\r\n");
        goto Error;
    }

    if (uiDataSize < (2 * sizeof(char *)))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateDebugScreenReq() : received_size < required_size\r\n");
        goto Error;
    }

    if (sscanf(pszRequest[1], "%d", &page_nr) == EOF)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateDebugScreenReq() - cannot convert %s to int\r\n", pszRequest[1]);
        goto Error;
    }

    RIL_LOG_INFO("CTE_INF_6260::CreateDebugScreenReq() - page_nr=[%d]\r\n", page_nr);

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XCGEDPAGE=0,%d\r", page_nr))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateDebugScreenReq() - Can't construct szCmd1.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;
Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::CreateDebugScreenReq() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::CreateSetSMSTransportModeReq(REQUEST_DATA& rReqData,
                                                    const char** pszRequest, const UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CreateSetSMSTransportModeReq() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int service;

    if (pszRequest == NULL || '\0' == pszRequest[0])
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateSetSMSTransportModeReq() - pszRequest was NULL\r\n");
        goto Error;
    }

    if (uiDataSize < (2 * sizeof(char *)))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateSetSMSTransportModeReq() : received_size < required_size\r\n");
        goto Error;
    }

    if (sscanf(pszRequest[1], "%d", &service) == EOF)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateSetSMSTransportModeReq() - cannot convert %s to int\r\n", pszRequest);
        goto Error;
    }

    if ((service < 0) || (service > 3))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateSetSMSTransportModeReq() - service %s out of boundaries\r\n", service);
        goto Error;
    }

    RIL_LOG_INFO("CTE_INF_6260::CreateSetSMSTransportModeReq() - service=[%d]\r\n", service);

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CGSMS=%d\r", service))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateSetSMSTransportModeReq() - Can't construct szCmd1.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;
Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::CreateSetSMSTransportModeReq() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseXGATR(const char* pszRsp, RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseXGATR() - Enter\r\n");

    P_ND_GET_ATR pResponse = NULL;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    pResponse = (P_ND_GET_ATR) malloc(sizeof(S_ND_GET_ATR));
    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseXGATR() - Could not allocate memory for response");
        goto Error;
    }
    memset(pResponse, 0, sizeof(S_ND_GET_ATR));

    // Skip "+XGATR: "
    if (!SkipString(pszRsp, "+XGATR: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseXGATR() - Could not skip \"+XGATR: \".\r\n");
        goto Error;
    }

    if (!ExtractQuotedString(pszRsp, pResponse->szATR, sizeof( pResponse->szATR), pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseXGATR() - Could not extract ATR value.\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_INF_6260::ParseXGATR() - szATR: %s\r\n", pResponse->szATR);

    // Parse "<postfix>"
    if (!SkipRspEnd(pszRsp, m_szNewLine, pszRsp))
    {
        goto Error;
    }

    pResponse->sResponsePointer.pszATR = pResponse->szATR;

    rRspData.pData   = (void*)pResponse;
    rRspData.uiDataSize  = sizeof(S_ND_GET_ATR_POINTER);
    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pResponse);
        pResponse = NULL;
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::ParseXGATR() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseXDRV(const char* pszRsp, RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseXDRV() - Enter\r\n");
    UINT32 uiIpcChrGrp;
    UINT32 uiIpcChrTempGet;
    UINT32 uiXdrvResult;
    UINT32 uiTempSensorId;
    UINT32 uiFilteredTemp;
    UINT32 uiRawTemp;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const UINT32 GET_THERMAL_SENSOR = 9;
    P_ND_THERMAL_SENSOR_VALUE pResponse = NULL;

    // Parse prefix
    if (!FindAndSkipString(pszRsp, "+XDRV: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseXDRV() - Unable to parse \"+XDRV\" prefix.!\r\n");
        goto Error;
    }

    // Parse <IPC_CHR_GRP>
    if (!ExtractUInt32(pszRsp, uiIpcChrGrp, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseXDRV() - Unable to parse <IPC_CHR_GRP>!\r\n");
        goto Error;
    }

    // Parse <IPC_CHR_TEMP_GET>
    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt32(pszRsp, uiIpcChrTempGet, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseXDRV() - Unable to parse <IPC_CHR_TEMP_GET>!\r\n");
        goto Error;
    }

    // Parse <xdrv_result>
    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt32(pszRsp, uiXdrvResult, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseXDRV() - Unable to parse <result>!\r\n");
        goto Error;
    }

    // XDRV Result should be XDRV_RESULT_OK (0) otherwise this is an error
    if (uiXdrvResult) goto Error;

    if (GET_THERMAL_SENSOR == uiIpcChrTempGet)
    {
        // Parse <temp_sensor_id>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractUInt32(pszRsp, uiTempSensorId, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseXDRV() - Unable to parse <temp_sensor_id>!\r\n");
            goto Error;
        }

        // Parse <filtered_temp>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractUInt32(pszRsp, uiFilteredTemp, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseXDRV() - Unable to parse <filtered_temp>!\r\n");
            goto Error;
        }

        // Parse <raw_temp>
        if (!SkipString(pszRsp, ",", pszRsp) ||
            !ExtractUInt32(pszRsp, uiRawTemp, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseXDRV() - Unable to parse <raw_temp>!\r\n");
            goto Error;
        }

        pResponse = (P_ND_THERMAL_SENSOR_VALUE) malloc(sizeof(S_ND_THERMAL_SENSOR_VALUE));
        if (NULL == pResponse)
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseXDRV() - Could not allocate memory for response");
            goto Error;
        }
        memset(pResponse, 0, sizeof(S_ND_THERMAL_SENSOR_VALUE));

        RIL_LOG_INFO("IPC_CHR_GRP: %u, IPC_CHR_TEMP_GET: %u, xdrv_result: %u, temp_sensor_id: %u "
                    "filtered_temp: %u, raw_temp: %u\r\n", uiIpcChrGrp, uiIpcChrTempGet,
                    uiXdrvResult, uiTempSensorId, uiFilteredTemp, uiRawTemp);

        snprintf(pResponse->pszTemperature, sizeof(pResponse->pszTemperature) - 1,
                 "%u %u", uiFilteredTemp, uiRawTemp);

        pResponse->sResponsePointer.pszTemperature = pResponse->pszTemperature;

        rRspData.pData   = (void*)pResponse;
        rRspData.uiDataSize  = sizeof(S_ND_THERMAL_SENSOR_VALUE_PTR);
    }

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pResponse);
        pResponse = NULL;
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::ParseXDRV() - Exit\r\n");
    return res;
}


RIL_RESULT_CODE CTE_INF_6260::ParseCGED(const char* pszRsp, RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseCGED() - Enter\r\n");

    P_ND_GPRS_CELL_ENV pResponse = NULL;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    pResponse = (P_ND_GPRS_CELL_ENV) malloc(sizeof(S_ND_GPRS_CELL_ENV));
    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseCGED() - Could not allocate memory for response");
        goto Error;
    }
    memset(pResponse, 0, sizeof(S_ND_GPRS_CELL_ENV));

    //  Copy entire response verbatim to response.
    strncpy(pResponse->pszGprsCellEnv, pszRsp, (MAX_BUFFER_SIZE*2)-1);
    pResponse->pszGprsCellEnv[(MAX_BUFFER_SIZE*2)-1] = '\0';

    pResponse->sResponsePointer.pszGprsCellEnv = pResponse->pszGprsCellEnv;

    rRspData.pData   = (void*)pResponse;
    rRspData.uiDataSize  = sizeof(S_ND_GPRS_CELL_ENV_PTR);
    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pResponse);
        pResponse = NULL;
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::ParseCGED() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseXCGEDPAGE(const char* pszRsp, RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseXCGEDPAGE() - Enter\r\n");

    P_ND_DEBUG_SCREEN pResponse = NULL;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    pResponse = (P_ND_DEBUG_SCREEN) malloc(sizeof(S_ND_DEBUG_SCREEN));
    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseXCGEDPAGE() - Could not allocate memory for response");
        goto Error;
    }
    memset(pResponse, 0, sizeof(S_ND_DEBUG_SCREEN));

    //  Copy entire response verbatim to response.
    strncpy(pResponse->pszDebugScreen, pszRsp, (MAX_BUFFER_SIZE*2)-1);
    pResponse->pszDebugScreen[(MAX_BUFFER_SIZE*2)-1] = '\0';

    pResponse->sResponsePointer.pszDebugScreen = pResponse->pszDebugScreen;

    rRspData.pData   = (void*)pResponse;
    rRspData.uiDataSize  = sizeof(S_ND_DEBUG_SCREEN_PTR);
    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pResponse);
        pResponse = NULL;
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::ParseXCGEDPAGE() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseCGSMS(const char* pszRsp, RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseCGSMS() - Enter\r\n");
    UINT32 uiService;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    P_ND_SMS_TRANSPORT_MODE pResponse = NULL;

    // Parse prefix
    if (!FindAndSkipString(pszRsp, "+CGSMS: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseCGSMS() - Unable to parse \"+CGSMS\" prefix.!\r\n");
        goto Error;
    }

    // Parse <service>
    if (!ExtractUInt32(pszRsp, uiService, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseCGSMS() - Unable to parse <service>!\r\n");
        goto Error;
    }

    // Check the upper boundary of the service
    if (uiService > 3) goto Error;

    pResponse = (P_ND_SMS_TRANSPORT_MODE) malloc(sizeof(S_ND_SMS_TRANSPORT_MODE));
    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseCGSMS() - Could not allocate memory for response");
        goto Error;
    }
    memset(pResponse, 0, sizeof(S_ND_SMS_TRANSPORT_MODE));

    RIL_LOG_INFO("SMS Transport Mode: %u\r\n", uiService);

    snprintf(pResponse->szService, sizeof(pResponse->szService) - 1, "%u", uiService);

    pResponse->sResponsePointer.pszService = pResponse->szService;

    rRspData.pData   = (void*)pResponse;
    rRspData.uiDataSize  = sizeof(S_ND_SMS_TRANSPORT_MODE_PTR);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pResponse);
        pResponse = NULL;
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::ParseCGSMS() - Exit\r\n");
    return res;
}

#if defined(M2_DUALSIM_FEATURE_ENABLED)
RIL_RESULT_CODE CTE_INF_6260::ParseSwapPS(const char* pszRsp, RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseSwapPS() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    if ((int)rRspData.pContextData & 2)
    {
        // Set the radio and SIM state as un available
        SetSIMState(RRIL_SIM_STATE_NOT_AVAILABLE);
        SetRadioState(RRIL_RADIO_STATE_UNAVAILABLE);
    }
    //  Todo: Handle error here.

    RIL_LOG_VERBOSE("CTE_INF_6260::ParseSwapPS() - Exit\r\n");
    return res;
}
#endif // M2_DUALSIM_FEATURE_ENABLED

BOOL CTE_INF_6260::HandleSilentPINEntry(void* pRilToken, void* /*pContextData*/, int /*size*/)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::HandleSilentPINEntry() - Enter\r\n");

    char szPIN[MAX_PIN_SIZE] = {0};
    BOOL bPINCodeIsOk = FALSE;
    BOOL bRet = FALSE;

    ePCache_Code ret = PCache_Get_PIN(m_szUICCID, szPIN);
    if (PIN_OK == ret)
    {
        char szCmd[MAX_BUFFER_SIZE] = {0};
        CCommand* pCmd = NULL;

        bPINCodeIsOk = TRUE;

        //  Queue AT+CPIN=<PIN> command
        if (!PrintStringNullTerminate(szCmd, MAX_BUFFER_SIZE, "AT+CPIN=\"%s\"\r", szPIN))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::HandleSilentPINEntry() - cannot create silent AT+CPIN=<PIN> command\r\n");
            goto Error;
        }

        pCmd = new CCommand(g_arChannelMapping[ND_REQ_ID_SILENT_PIN_ENTRY], pRilToken,
                            ND_REQ_ID_SILENT_PIN_ENTRY, szCmd, &CTE::ParseSilentPinEntry,
                            &CTE::PostSilentPinRetryCmdHandler);
        if (pCmd)
        {
            if (!CCommand::AddCmdToQueue(pCmd, TRUE))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::HandleSilentPINEntry() - Unable to queue AT+CPIN command!\r\n");
                delete pCmd;
                pCmd = NULL;
                goto Error;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::HandleSilentPINEntry() - Unable to allocate memory for new silent AT+CPIN=<PIN> command!\r\n");
            goto Error;
        }

        bRet = TRUE;
    }

Error:
    m_szUICCID[0] = '\0';
    RIL_LOG_VERBOSE("CTE_INF_6260::HandleSilentPINEntry() - Exit\r\n");
    return bRet;
}

//
//  Response to Silent PIN Entry
//
RIL_RESULT_CODE CTE_INF_6260::ParseSilentPinEntry(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseSilentPinEntry() - Enter\r\n");

    RIL_LOG_VERBOSE("CTE_INF_6260::ParseSilentPinEntry() - Exit\r\n");
    return RRIL_RESULT_OK;
}

RIL_RESULT_CODE CTE_INF_6260::QueryPinRetryCount(REQUEST_DATA& rReqData, void* /*pData*/, UINT32 /*uiDataSize*/)
{
    RIL_RESULT_CODE res = RIL_E_GENERIC_FAILURE;

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XPINCNT\r"))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::QueryPinRetryCount() - Can't construct szCmd1.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::QueryPinRetryCount() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseSimPinRetryCount(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseSimPinRetryCount() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* pszRsp = rRspData.szResponse;

    // +XPINCNT: <PIN attempts>, <PIN2 attempts>, <PUK attempts>, <PUK2 attempts>
    if (FindAndSkipString(pszRsp, "+XPINCNT: ", pszRsp))
    {
        UINT32 uiRetryCount[4];

        if (!ExtractUInt32(pszRsp, uiRetryCount[0], pszRsp) ||
            !SkipString(pszRsp, ",", pszRsp) ||
            !ExtractUInt32(pszRsp, uiRetryCount[1], pszRsp) ||
            !SkipString(pszRsp, ",", pszRsp) ||
            !ExtractUInt32(pszRsp, uiRetryCount[2], pszRsp) ||
            !SkipString(pszRsp, ",", pszRsp) ||
            !ExtractUInt32(pszRsp, uiRetryCount[3], pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimPinRetryCount() - Cannot parse XPINCNT\r\n");
        }
        else
        {
            RIL_LOG_INFO("CTE_INF_6260::ParseSimPinRetryCount() - retries pin:%d pin2:%d puk:%d puk2:%d\r\n",
                uiRetryCount[0], uiRetryCount[1], uiRetryCount[2], uiRetryCount[3]);
            m_PinRetryCount.pin = uiRetryCount[0];
            m_PinRetryCount.pin2 = uiRetryCount[1];
            m_PinRetryCount.puk = uiRetryCount[2];
            m_PinRetryCount.puk2 = uiRetryCount[3];
            res = RRIL_RESULT_OK;
        }
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::ParseSimPinRetryCount() - Exit\r\n");

    return res;
}

void CTE_INF_6260::PostSetupDataCallCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::PostSetupDataCallCmdHandler() Enter\r\n");

    BOOL bSuccess = FALSE;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;
    UINT32 uiCID = 0;
    CChannel_Data* pChannelData = NULL;

    if (NULL == rData.pContextData ||
            sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA) != rData.uiContextDataSize)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::PostSetupDataCallCmdHandler() - Invalid context data\r\n");
        goto Error;
    }

    pDataCallContextData = (S_SETUP_DATA_CALL_CONTEXT_DATA*)rData.pContextData;
    uiCID = pDataCallContextData->uiCID;

    if (RIL_E_SUCCESS != rData.uiResultCode)
    {
        RIL_LOG_INFO("CTE_INF_6260::PostSetupDataCallCmdHandler() - Failure\r\n");
        goto Error;
    }

    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_INFO("CTE_INF_6260::PostSetupDataCallCmdHandler() - No Data Channel for CID %u.\r\n",
                                                                    uiCID);
        goto Error;
    }

    pChannelData->SetDataState(E_DATA_STATE_ACTIVATING);

    if (!CreatePdpContextActivateReq(rData.uiChannel, rData.pRilToken,
                                    rData.uiRequestId, rData.pContextData,
                                    rData.uiContextDataSize,
                                    &CTE::ParsePdpContextActivate,
                                    &CTE::PostPdpContextActivateCmdHandler))
    {
        RIL_LOG_INFO("CTE_INF_6260::PostSetupDataCallCmdHandler() - CreatePdpContextActivateReq failed\r\n");
        goto Error;
    }

    bSuccess = TRUE;
Error:
    if (!bSuccess)
    {
        free(rData.pContextData);
        rData.pContextData = NULL;

        HandleSetupDataCallFailure(uiCID, rData.pRilToken, rData.uiResultCode);
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::PostSetupDataCallCmdHandler() Exit\r\n");
}

void CTE_INF_6260::PostPdpContextActivateCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::PostPdpContextActivateCmdHandler() Enter\r\n");

    CChannel_Data* pChannelData = NULL;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;
    UINT32 uiCID = 0;
    int state;
    BOOL bSuccess = FALSE;

    if (NULL == rData.pContextData ||
            sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA) != rData.uiContextDataSize)
    {
        RIL_LOG_INFO("CTE_INF_6260::PostPdpContextActivateCmdHandler() - Invalid context data\r\n");
        goto Error;
    }

    pDataCallContextData = (S_SETUP_DATA_CALL_CONTEXT_DATA*)rData.pContextData;
    uiCID = pDataCallContextData->uiCID;

    if (RIL_E_SUCCESS != rData.uiResultCode)
    {
        RIL_LOG_INFO("CTE_INF_6260::PostPdpContextActivateCmdHandler() - Failure\r\n");
        goto Error;
    }

    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_INFO("CTE_INF_6260::PostPdpContextActivateCmdHandler() - No Data Channel for CID %u.\r\n",
                                                                    uiCID);
        goto Error;
    }

    state = pChannelData->GetDataState();
    if (E_DATA_STATE_ACTIVATING != state)
    {
        RIL_LOG_INFO("CTE_INF_6260::PostPdpContextActivateCmdHandler() - Wrong data state: %d\r\n", state);
        goto Error;
    }

    pChannelData->SetDataState(E_DATA_STATE_ACTIVE);
    if (!CreateQueryIpAndDnsReq(rData.uiChannel, rData.pRilToken,
                                    rData.uiRequestId, rData.pContextData,
                                    rData.uiContextDataSize,
                                    &CTE::ParseQueryIpAndDns,
                                    &CTE::PostQueryIpAndDnsCmdHandler))
    {
        RIL_LOG_INFO("CTE_INF_6260::PostPdpContextActivateCmdHandler() - CreateQueryIpAndDnsReq failed\r\n");
        goto Error;
    }

    bSuccess = TRUE;
Error:
    if (!bSuccess)
    {
        free(rData.pContextData);
        rData.pContextData = NULL;

        HandleSetupDataCallFailure(uiCID, rData.pRilToken, rData.uiResultCode);
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::PostPdpContextActivateCmdHandler() Exit\r\n");
}

void CTE_INF_6260::PostQueryIpAndDnsCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::PostQueryIpAndDnsCmdHandler() Enter\r\n");

    UINT32 uiCID = 0;
    CChannel_Data* pChannelData = NULL;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;;
    int state;
    BOOL bSuccess = FALSE;

    if (NULL == rData.pContextData ||
            sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA) != rData.uiContextDataSize)
    {
        RIL_LOG_INFO("CTE_INF_6260::PostQueryIpAndDnsCmdHandler() - Invalid context data\r\n");
        goto Error;
    }

    pDataCallContextData = (S_SETUP_DATA_CALL_CONTEXT_DATA*)rData.pContextData;
    uiCID = pDataCallContextData->uiCID;

    if (RIL_E_SUCCESS != rData.uiResultCode)
    {
        RIL_LOG_INFO("CTE_INF_6260::PostQueryIpAndDnsCmdHandler() - Failure\r\n");
        goto Error;
    }

    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_INFO("CTE_INF_6260::PostQueryIpAndDnsCmdHandler() - No Data Channel for CID %u.\r\n",
                                                                    uiCID);
        goto Error;
    }

    state = pChannelData->GetDataState();
    if (E_DATA_STATE_ACTIVE != state)
    {
        RIL_LOG_INFO("CTE_INF_6260::PostQueryIpAndDnsCmdHandler() - Wrong data state: %d\r\n",
                                                                        state);
        goto Error;
    }

    if (!CreateEnterDataStateReq(rData.uiChannel, rData.pRilToken,
                                    rData.uiRequestId, rData.pContextData,
                                    rData.uiContextDataSize,
                                    &CTE::ParseEnterDataState,
                                    &CTE::PostEnterDataStateCmdHandler))
    {
        RIL_LOG_INFO("CTE_INF_6260::PostQueryIpAndDnsCmdHandler() - CreateEnterDataStateReq failed\r\n");
        goto Error;
    }

    bSuccess = TRUE;

Error:
    if (!bSuccess)
    {
        free(rData.pContextData);
        rData.pContextData = NULL;

        HandleSetupDataCallFailure(uiCID, rData.pRilToken, rData.uiResultCode);
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::PostQueryIpAndDnsCmdHandler() Exit\r\n");
}

void CTE_INF_6260::PostEnterDataStateCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::PostEnterDataStateCmdHandler() Enter\r\n");

    UINT32 uiCID = 0;
    CChannel_Data* pChannelData = NULL;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;;
    int state;
    BOOL bSuccess = FALSE;

    if (NULL == rData.pContextData ||
            sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA) != rData.uiContextDataSize)
    {
        RIL_LOG_INFO("CTE_INF_6260::PostEnterDataStateCmdHandler() - Invalid context data\r\n");
        goto Error;
    }

    pDataCallContextData = (S_SETUP_DATA_CALL_CONTEXT_DATA*)rData.pContextData;
    uiCID = pDataCallContextData->uiCID;

    if (RIL_E_SUCCESS != rData.uiResultCode)
    {
        RIL_LOG_INFO("CTE_INF_6260::PostEnterDataStateCmdHandler() - Failure\r\n");
        goto Error;
    }

    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_INFO("CTE_INF_6260::PostEnterDataStateCmdHandler() - No Data Channel for CID %u.\r\n",
                                                                    uiCID);
        goto Error;
    }

    state = pChannelData->GetDataState();
    if (E_DATA_STATE_ACTIVE != state)
    {
        RIL_LOG_INFO("CTE_INF_6260::PostEnterDataStateCmdHandler() - Wrong data state: %d\r\n",
                                                                        state);
        goto Error;
    }

    if (!SetupInterface(uiCID))
    {
        RIL_LOG_INFO("CTE_INF_6260::PostEnterDataStateCmdHandler() - SetupInterface failed\r\n");
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

    RIL_LOG_VERBOSE("CTE_INF_6260::PostEnterDataStateCmdHandler() Exit\r\n");
}

void CTE_INF_6260::PostDeactivateDataCallCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::PostDeactivateDataCallCmdHandler() Enter\r\n");

    UINT32 uiCID;

    if (RIL_E_SUCCESS != rData.uiResultCode)
    {
        RIL_LOG_INFO("CTE_INF_6260::PostDeactivateDataCallCmdHandler() request failure\r\n");
        goto Error;
    }

    if (NULL == rData.pContextData ||
            sizeof(UINT32) != rData.uiContextDataSize)
    {
        RIL_LOG_INFO("CTE_INF_6260::PostDeactivateDataCallCmdHandler() - Invalid context data\r\n");
        goto Error;
    }

    uiCID = *((UINT32*)rData.pContextData);

    DataConfigDown(uiCID);

Error:
    free(rData.pContextData);
    rData.pContextData = NULL;

    if (NULL != rData.pRilToken)
    {
        RIL_onRequestComplete(rData.pRilToken, RIL_E_SUCCESS, NULL, 0);
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::PostDeactivateDataCallCmdHandler() Exit\r\n");
}

BOOL CTE_INF_6260::CreatePdpContextActivateReq(UINT32 uiChannel,
                                            RIL_Token rilToken,
                                            UINT32 uiReqId, void* pData,
                                            UINT32 uiDataSize,
                                            PFN_TE_PARSE pParseFcn,
                                            PFN_TE_POSTCMDHANDLER pPostCmdHandlerFcn)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CreatePdpContextActivateReq() - Enter\r\n");

    BOOL bRet = FALSE;
    REQUEST_DATA reqData;

    memset(&reqData, 0, sizeof(REQUEST_DATA));

    if (!PdpContextActivate(reqData, pData, uiDataSize))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreatePdpContextActivateReq() - Unable to create AT command data\r\n");
        goto Error;
    }
    else
    {
        CCommand * pCmd = new CCommand(uiChannel, rilToken, uiReqId, reqData,
                                                pParseFcn, pPostCmdHandlerFcn);
        if (pCmd)
        {
            pCmd->SetContextData(pData);
            pCmd->SetContextDataSize(uiDataSize);
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::CreatePdpContextActivateReq() - Unable to add command to queue\r\n");
                delete pCmd;
                pCmd = NULL;
                goto Error;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::CreatePdpContextActivateReq() - Unable to allocate memory for command\r\n");
            goto Error;
        }
    }

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::CreatePdpContextActivateReq() - Exit\r\n");
    return bRet;
}

BOOL CTE_INF_6260::CreateQueryIpAndDnsReq(UINT32 uiChannel, RIL_Token rilToken,
                                            UINT32 uiReqId, void* pData,
                                            UINT32 uiDataSize,
                                            PFN_TE_PARSE pParseFcn,
                                            PFN_TE_POSTCMDHANDLER pPostCmdHandlerFcn)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CreateQueryIpAndDnsReq() - Enter\r\n");

    BOOL bRet = FALSE;
    REQUEST_DATA reqData;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;

    if (NULL == pData || sizeof(pDataCallContextData) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateQueryIpAndDnsReq() - Invalid context data\r\n");
        goto Error;
    }

    memset(&reqData, 0, sizeof(REQUEST_DATA));
    pDataCallContextData = (S_SETUP_DATA_CALL_CONTEXT_DATA*)pData;

    if (!QueryIpAndDns(reqData, pDataCallContextData->uiCID))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateQueryIpAndDnsReq() - Unable to create AT command data\r\n");
        goto Error;
    }
    else
    {
        CCommand * pCmd = new CCommand(uiChannel, rilToken, uiReqId, reqData,
                                                pParseFcn, pPostCmdHandlerFcn);
        if (pCmd)
        {
            pCmd->SetContextData(pData);
            pCmd->SetContextDataSize(uiDataSize);
            pCmd->SetHighPriority();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::CreateQueryIpAndDnsReq() - Unable to add command to queue\r\n");
                delete pCmd;
                pCmd = NULL;
                goto Error;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::CreateQueryIpAndDnsReq() - Unable to allocate memory for command\r\n");
            goto Error;
        }
    }

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::CreateQueryIpAndDnsReq() - Exit\r\n");
    return bRet;
}

BOOL CTE_INF_6260::CreateEnterDataStateReq(UINT32 uiChannel, RIL_Token rilToken,
                                            UINT32 uiReqId, void* pData,
                                            UINT32 uiDataSize,
                                            PFN_TE_PARSE pParseFcn,
                                            PFN_TE_POSTCMDHANDLER pPostCmdHandlerFcn)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CreateEnterDataStateReq() - Enter\r\n");

    BOOL bRet = FALSE;
    REQUEST_DATA reqData;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;

    if (NULL == pData || sizeof(pDataCallContextData) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateEnterDataStateReq() - Invalid context data\r\n");
        goto Error;
    }

    memset(&reqData, 0, sizeof(REQUEST_DATA));
    pDataCallContextData = (S_SETUP_DATA_CALL_CONTEXT_DATA*)pData;

    if (!EnterDataState(reqData, pDataCallContextData->uiCID))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CreateEnterDataStateReq() - Unable to create AT command data\r\n");
        goto Error;
    }
    else
    {
        CCommand * pCmd = new CCommand(uiChannel, rilToken, uiReqId, reqData,
                                                pParseFcn, pPostCmdHandlerFcn);
        if (pCmd)
        {
            pCmd->SetContextData(pData);
            pCmd->SetContextDataSize(uiDataSize);
            pCmd->SetHighPriority();
            pCmd->SetAlwaysParse();
            if (!CCommand::AddCmdToQueue(pCmd))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::CreateEnterDataStateReq() - Unable to add command to queue\r\n");
                delete pCmd;
                pCmd = NULL;
                goto Error;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::CreateEnterDataStateReq() - Unable to allocate memory for command\r\n");
            goto Error;
        }
    }

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::CreateEnterDataStateReq() - Exit\r\n");
    return bRet;
}

BOOL CTE_INF_6260::SetupInterface(UINT32 uiCID)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::SetupInterface() - Enter\r\n");

    BOOL bRet = FALSE;
    char szNetworkInterfaceName[IFNAMSIZ];
    struct gsm_netconfig netconfig;
    int fd = -1;
    int ret = 0;
    CChannel_Data* pChannelData = NULL;
    PDP_TYPE eDataConnectionType = PDP_TYPE_IPV4;  //  dummy for now, set to IPv4.
    char szPdpType[MAX_BUFFER_SIZE] = {'\0'};
    char szIpAddr[MAX_IPADDR_SIZE] = {'\0'};
    char szIpAddr2[MAX_IPADDR_SIZE] = {'\0'};
    UINT32 uiChannel = 0;
    int state = 0;

    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::SetupInterface() - No Data Channel for CID %u.\r\n",
                                                                    uiCID);
        goto Error;
    }

    state = pChannelData->GetDataState();
    if (E_DATA_STATE_ACTIVE != state)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::SetupInterface() - Invalid data state %d.\r\n",
                                                                    state);
        goto Error;
    }

    if (!PrintStringNullTerminate(szNetworkInterfaceName, IFNAMSIZ,
                            "%s%u", m_szNetworkInterfaceNamePrefix, uiCID-1))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::SetupInterface() - Cannot set network interface name\r\n");
        goto Error;
    }
    else
    {
        RIL_LOG_INFO("CTE_INF_6260::SetupInterface() - szNetworkInterfaceName=[%s], CID=[%u]\r\n",
                                                szNetworkInterfaceName, uiCID);
    }

    pChannelData->SetInterfaceName(szNetworkInterfaceName);
    uiChannel = pChannelData->GetRilChannel();

    // N_GSM related code
    netconfig.adaption = 3; /// @TODO: Use meaningful name
    netconfig.protocol = htons(ETH_P_IP);
    strncpy(netconfig.if_name, szNetworkInterfaceName, IFNAMSIZ - 1);
    netconfig.if_name[IFNAMSIZ - 1] = '\0';

    // Add IF NAME
    fd = pChannelData->GetFD();
    if (fd >= 0)
    {
        RIL_LOG_INFO("CTE_INF_6260::SetupInterface() - ***** PUTTING channel=[%u] in DATA MODE *****\r\n",
                                                                    uiChannel);
        ret = ioctl(fd, GSMIOC_ENABLE_NET, &netconfig); // Enable data channel
        if (ret < 0)
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::SetupInterface() - Unable to create interface %s : %s \r\n",
                                            netconfig.if_name,strerror(errno));
            goto Error;
        }
    }
    else
    {
        //  No FD.
        RIL_LOG_CRITICAL("CTE_INF_6260::SetupInterface() - Could not get Data Channel chnl=[%u] fd=[%d].\r\n",
                                                                uiChannel, fd);
        goto Error;
    }

    pChannelData->GetIpAddress(szIpAddr, sizeof(szIpAddr),
                                    szIpAddr2, sizeof(szIpAddr2));

    if (szIpAddr2[0] == '\0')
    {
        eDataConnectionType = PDP_TYPE_IPV4;
        strcpy(szPdpType, "IPV4");
    }
    else if (szIpAddr[0] == '\0')
    {
        eDataConnectionType = PDP_TYPE_IPV6;
        strcpy(szPdpType, "IPV6");
    }
    else
    {
        eDataConnectionType = PDP_TYPE_IPV4V6;
        strcpy(szPdpType, "IPV4V6");
    }

    pChannelData->SetPdpType(szPdpType);

    // set interface address(es) and bring up interface
    if (!DataConfigUp(szNetworkInterfaceName, pChannelData,
                                                        eDataConnectionType))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::SetupInterface() - Unable to bringup interface ifconfig\r\n");
        goto Error;
    }

    bRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::SetupInterface() Exit\r\n");
    return bRet;
}

void CTE_INF_6260::HandleSetupDataCallSuccess(UINT32 uiCID, void* pRilToken)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::HandleSetupDataCallSuccess() - Enter\r\n");

    RIL_Data_Call_Response_v6 dataCallResp;
    char szPdpType[MAX_PDP_TYPE_SIZE] = {'\0'};
    char szInterfaceName[MAX_INTERFACE_NAME_SIZE] = {'\0'};
    char szIPAddress[MAX_BUFFER_SIZE] = {'\0'};
    char szDNS[MAX_BUFFER_SIZE] = {'\0'};
    char szGateway[MAX_IPADDR_SIZE] = {'\0'};
    S_DATA_CALL_INFO sDataCallInfo;
    CChannel_Data* pChannelData = NULL;

    memset(&dataCallResp, 0, sizeof(RIL_Data_Call_Response_v6));
    dataCallResp.status = PDP_FAIL_ERROR_UNSPECIFIED;
    dataCallResp.suggestedRetryTime = -1;

    CTE::GetTE().SetupDataCallOngoing(FALSE);

    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::HandleSetupDataCallSuccess() - No Data Channel for CID %u.\r\n",
                                                                    uiCID);
        goto Error;
    }

    pChannelData->GetDataCallInfo(sDataCallInfo);

    snprintf(szDNS, MAX_BUFFER_SIZE-1, "%s %s %s %s",
                        sDataCallInfo.szDNS1, sDataCallInfo.szDNS2,
                        sDataCallInfo.szIpV6DNS1, sDataCallInfo.szIpV6DNS2);
    szDNS[MAX_BUFFER_SIZE-1] = '\0';

    snprintf(szIPAddress, MAX_BUFFER_SIZE-1, "%s %s",
                    sDataCallInfo.szIpAddr1, sDataCallInfo.szIpAddr2);
    szIPAddress[MAX_BUFFER_SIZE-1] = '\0';

    strncpy(szGateway, sDataCallInfo.szGateways, MAX_IPADDR_SIZE-1);
    szGateway[MAX_IPADDR_SIZE-1] = '\0';

    strncpy(szPdpType, sDataCallInfo.szPdpType, MAX_PDP_TYPE_SIZE-1);
    szPdpType[MAX_PDP_TYPE_SIZE-1] = '\0';

    strncpy(szInterfaceName, sDataCallInfo.szInterfaceName,
                                            MAX_INTERFACE_NAME_SIZE-1);
    szInterfaceName[MAX_INTERFACE_NAME_SIZE-1] = '\0';

    dataCallResp.status = sDataCallInfo.failCause;
    dataCallResp.suggestedRetryTime = -1;
    dataCallResp.cid = sDataCallInfo.uiCID;
    dataCallResp.active = 2;
    dataCallResp.type = szPdpType;
    dataCallResp.addresses = szIPAddress;
    dataCallResp.dnses = szDNS;
    dataCallResp.gateways = szGateway;
    dataCallResp.ifname = szInterfaceName;

    if (CRilLog::IsFullLogBuild())
    {
        RIL_LOG_INFO("status=%d suggRetryTime=%d cid=%d active=%d type=\"%s\" ifname=\"%s\" addresses=\"%s\" dnses=\"%s\" gateways=\"%s\"\r\n",
                      dataCallResp.status, dataCallResp.suggestedRetryTime,
                      dataCallResp.cid, dataCallResp.active,
                      dataCallResp.type, dataCallResp.ifname,
                      dataCallResp.addresses, dataCallResp.dnses,
                      dataCallResp.gateways);
    }

Error:
    if (NULL != pRilToken)
    {
        RIL_onRequestComplete(pRilToken, RIL_E_SUCCESS, &dataCallResp,
                                    sizeof(RIL_Data_Call_Response_v6));
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::HandleSetupDataCallSuccess() - Exit\r\n");
}

void CTE_INF_6260::HandleSetupDataCallFailure(UINT32 uiCID, void* pRilToken,
                                                        UINT32 uiResultCode)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::HandleSetupDataCallFailure() - Enter\r\n");

    int state;
    int failCause = PDP_FAIL_ERROR_UNSPECIFIED;

    CTE::GetTE().SetupDataCallOngoing(FALSE);

    CChannel_Data* pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_INFO("CTE_INF_6260::HandleSetupDataCallFailure() - No data channel for CID: %u\r\n",
                                                                        uiCID);
        goto Complete;
    }

    state = pChannelData->GetDataState();
    failCause = pChannelData->GetDataFailCause();
    if (PDP_FAIL_NONE == failCause)
    {
        failCause = PDP_FAIL_ERROR_UNSPECIFIED;
    }

    RIL_LOG_INFO("CTE_INF_6260::HandleSetupDataCallFailure() - state: %d\r\n",
                                                                        state);

    switch (state)
    {
        case E_DATA_STATE_ACTIVE:
            /*
             * @TODO: Delay the completion of ril request till the
             * data call is deactivated successfully?
             */
            RIL_requestTimedCallback(triggerDeactivateDataCall,
                                                        (void*)uiCID, 0, 0);
            break;
        default:
            DataConfigDown(uiCID);
            break;
    }

Complete:
    if (NULL != pRilToken)
    {
        RIL_Data_Call_Response_v6 dataCallResp;
        memset(&dataCallResp, 0, sizeof(RIL_Data_Call_Response_v6));
        dataCallResp.status = failCause;
        dataCallResp.suggestedRetryTime = -1;
        RIL_onRequestComplete(pRilToken, RIL_E_SUCCESS, &dataCallResp,
                                    sizeof(RIL_Data_Call_Response_v6));
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::HandleSetupDataCallFailure() - Exit\r\n");
}

//
//  Call this whenever data is disconnected
//
BOOL CTE_INF_6260::DataConfigDown(UINT32 uiCID)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::DataConfigDown() - Enter\r\n");

    //  First check to see if uiCID is valid
    if (uiCID > MAX_PDP_CONTEXTS || uiCID == 0)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::DataConfigDown() - Invalid CID = [%u]\r\n", uiCID);
        return FALSE;
    }

    BOOL bRet = FALSE;
    CChannel_Data* pChannelData = NULL;
    struct gsm_netconfig netconfig;
    int fd = -1;
    int flags;
    int ret = -1;
    UINT32 uiChannel = 0;

    //  See if CID passed in is valid
    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::DataConfigDown() - Invalid CID=[%u], no data channel found!\r\n",
                                                                        uiCID);
        return FALSE;
    }

    uiChannel = pChannelData->GetRilChannel();

    // Reset ContextID to 0, to free up the channel for future use
    RIL_LOG_INFO("CTE_INF_6260::DataConfigDown() - ****** Setting chnl=[%u] to CID=[0] ******\r\n",
                                                                    uiChannel);

    pChannelData->ResetDataCallInfo();

    // Blocking TTY flow. Third security level in order to avoid IP data in response buffer.
    // Not mandatory.
    pChannelData->BlockAndFlushChannel(BLOCK_CHANNEL_BLOCK_TTY, FLUSH_CHANNEL_NO_FLUSH);

    //  Put the channel back into AT command mode
    netconfig.adaption = 3;
    netconfig.protocol = htons(ETH_P_IP);

    fd = pChannelData->GetFD();
    if (fd >= 0)
    {
        RIL_LOG_INFO("CTE_INF_6260::DataConfigDown() - ***** PUTTING channel=[%u] in AT COMMAND MODE *****\r\n",
                            uiChannel);
        ret = ioctl(fd, GSMIOC_DISABLE_NET, &netconfig);
    }
    else
    {
        //  No FD.
        RIL_LOG_CRITICAL("DataConfigDown() - Could not change Channel mode chnl=[%d] fd=[%d].\r\n",
                                pChannelData->GetRilChannel(), fd);
        goto Error;
    }

    bRet = TRUE;

    RIL_LOG_INFO("[RIL STATE] PDP CONTEXT DEACTIVATION chnl=%u\r\n", uiChannel);

Error:
    // Flush buffers and Unblock read thread.
    // Security in order to avoid IP data in response buffer.
    // Will unblock Channel read thread and TTY.
    // Unblock read thread whatever the result is to avoid forever block
    pChannelData->FlushAndUnblockChannel(UNBLOCK_CHANNEL_UNBLOCK_ALL, FLUSH_CHANNEL_FLUSH_ALL);

    RIL_LOG_INFO("CTE_INF_6260::DataConfigDown() EXIT  bRet=[%d]\r\n", bRet);
    return bRet;
}

