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


CTE_XMM7160::CTE_XMM7160(CTE& cte)
: CTE_XMM6360(cte)
{
}

CTE_XMM7160::~CTE_XMM7160()
{
}

char* CTE_XMM7160::GetBasicInitCommands(UINT32 uiChannelType)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::GetBasicInitCommands() - Enter\r\n");

    char szInitCmd[MAX_BUFFER_SIZE] = {'\0'};
    char *pInitCmd = NULL;

    pInitCmd = CTE_XMM6260::GetBasicInitCommands(uiChannelType);

    if (RIL_CHANNEL_URC != uiChannelType)
    {
        RIL_LOG_VERBOSE("CTE_XMM7160::GetBasicInitCommands() - Exit.\r\n");
        return pInitCmd;
    }

    if (pInitCmd != NULL)
    {
        strncpy(szInitCmd,pInitCmd,MAX_BUFFER_SIZE);

        if (m_cte.IsIMSCapable())
        {
            char szEnableIMS[MAX_BUFFER_SIZE] = {'\0'};
            PrintStringNullTerminate(szEnableIMS,
                    MAX_BUFFER_SIZE,
                    "|+CISRVCC=1|+CIREP=1|+CIREG=1|+XISMSCFG=%d",
                    m_cte.IsSMSOverIPCapable() ? 1 : 0);
            ConcatenateStringNullTerminate(szInitCmd, MAX_BUFFER_SIZE - strlen(szInitCmd),
                    szEnableIMS);
        }

        // add to init string. +CEREG=2
        ConcatenateStringNullTerminate(szInitCmd, MAX_BUFFER_SIZE - strlen(szInitCmd),
                "|+CEREG=2");
        free (pInitCmd);
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::GetBasicInitCommands() - Exit\r\n");
    return strndup(szInitCmd, strlen(szInitCmd));
}

// RIL_REQUEST_SETUP_DATA_CALL 27
RIL_RESULT_CODE CTE_XMM7160::CoreSetupDataCall(REQUEST_DATA& rReqData,
                                                           void* pData,
                                                           UINT32 uiDataSize,
                                                           UINT32 uiCID)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::CoreSetupDataCall() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char szIPV4V6[] = "IPV4V6";
    int nPapChap = 0; // no auth
    PdpData stPdpData;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;
    CChannel_Data* pChannelData = NULL;
    BOOL bContextActivated = FALSE;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CoreSetupDataCall() - Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize < (6 * sizeof(char*)))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CoreSetupDataCall() -"
                " Invalid data size. Was given %d bytes\r\n", uiDataSize);
        goto Error;
    }

    RIL_LOG_INFO("CTE_XMM7160::CoreSetupDataCall() - uiDataSize=[%d],uiCID=[%d]\r\n",
                 uiDataSize,uiCID);

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
        RIL_LOG_CRITICAL("CTE_XMM7160::CoreSetupDataCall() - No Channel with context Id: %u\r\n",
                uiCID);
        goto Error;
    }

    memset(&stPdpData, 0, sizeof(PdpData));
    pDataCallContextData->uiCID = uiCID;

    // extract data
    stPdpData.szRadioTechnology = ((char**)pData)[0];  // not used
    stPdpData.szRILDataProfile  = ((char**)pData)[1];
    stPdpData.szApn             = ((char**)pData)[2];
    stPdpData.szUserName        = ((char**)pData)[3];
    stPdpData.szPassword        = ((char**)pData)[4];
    stPdpData.szPAPCHAP         = ((char**)pData)[5];

    RIL_LOG_INFO("CTE_XMM7160::CoreSetupDataCall() - stPdpData.szRadioTechnology=[%s]\r\n",
            stPdpData.szRadioTechnology);
    RIL_LOG_INFO("CTE_XMM7160::CoreSetupDataCall() - stPdpData.szRILDataProfile=[%s]\r\n",
            stPdpData.szRILDataProfile);
    RIL_LOG_INFO("CTE_XMM7160::CoreSetupDataCall() - stPdpData.szApn=[%s]\r\n", stPdpData.szApn);
    RIL_LOG_INFO("CTE_XMM7160::CoreSetupDataCall() - stPdpData.szUserName=[%s]\r\n",
            stPdpData.szUserName);
    RIL_LOG_INFO("CTE_XMM7160::CoreSetupDataCall() - stPdpData.szPassword=[%s]\r\n",
            stPdpData.szPassword);
    RIL_LOG_INFO("CTE_XMM7160::CoreSetupDataCall() - stPdpData.szPAPCHAP=[%s]\r\n",
            stPdpData.szPAPCHAP);

    bContextActivated = CTE::GetTE().ContextActivated(stPdpData.szApn);
    if(bContextActivated)
    {
        RIL_LOG_INFO("CTE_INF_7160::CoreSetupDataCall() - context (apn=%s) "
                     "has been activated by network!\r\n", stPdpData.szApn);
    }

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

            RIL_LOG_INFO("CTE_XMM7160::CoreSetupDataCall() - New PAP/CHAP=[%d]\r\n", nPapChap);
        }
    }

    if (RIL_VERSION >= 4 && (uiDataSize >= (7 * sizeof(char*))))
    {
        stPdpData.szPDPType         = ((char**)pData)[6];  // new in Android 2.3.4.
        RIL_LOG_INFO("CTE_XMM7160::CoreSetupDataCall() - stPdpData.szPDPType=[%s]\r\n",
                stPdpData.szPDPType);
    }

    //
    //  IP type is passed in dynamically.
    if (NULL == stPdpData.szPDPType)
    {
        //  hard-code "IPV4V6" (this is the default)
        stPdpData.szPDPType = szIPV4V6;
    }


    if(!bContextActivated)
    {
        //  dynamic PDP type, need to set XDNS parameter depending on szPDPType.
        //  If not recognized, just use IPV4V6 as default.
        if (0 == strcmp(stPdpData.szPDPType, "IP"))
        {
            if (!PrintStringNullTerminate(rReqData.szCmd1,
                sizeof(rReqData.szCmd1),
                "AT+CGDCONT=%d,\"%s\",\"%s\",,0,0;+XDNS=%d,1\r", uiCID, stPdpData.szPDPType,
                stPdpData.szApn, uiCID))
            {
                RIL_LOG_CRITICAL("CTE_INF_7160::CoreSetupDataCall() - cannot create CGDCONT command"
                                 ", stPdpData.szPDPType\r\n");
                goto Error;
            }
         }
        else if (0 == strcmp(stPdpData.szPDPType, "IPV6"))
         {
            if (!PrintStringNullTerminate(rReqData.szCmd1,
                sizeof(rReqData.szCmd1),
                "AT+CGDCONT=%d,\"%s\",\"%s\",,0,0;+XDNS=%d,2\r", uiCID, stPdpData.szPDPType,
                stPdpData.szApn, uiCID))
            {
                RIL_LOG_CRITICAL("CTE_INF_7160::CoreSetupDataCall() - cannot create CGDCONT command"
                                 ", stPdpData.szPDPType\r\n");
                goto Error;
            }
        }
        else if (0 == strcmp(stPdpData.szPDPType, "IPV4V6"))
        {
            //  XDNS=3 is not supported by the modem so two commands +XDNS=1 and
            // +XDNS=2 should be sent.
            if (!PrintStringNullTerminate(rReqData.szCmd1,
                sizeof(rReqData.szCmd1),
                "AT+CGDCONT=%d,\"IPV4V6\",\"%s\",,0,0;+XDNS=%d,1;+XDNS=%d,2\r", uiCID,
                stPdpData.szApn, uiCID, uiCID))
            {
                RIL_LOG_CRITICAL("CTE_INF_7160::CoreSetupDataCall() - cannot create CGDCONT command"
                                 ", stPdpData.szPDPType\r\n");
                goto Error;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE_INF_7160::CoreSetupDataCall() - Wrong PDP type\r\n");
            goto Error;
        }
    }
    else
    {
        char* szModemResourceName = {'\0'};
        int muxControlChannel = -1;
        int hsiChannel = pChannelData->GetHSIChannel();
        int ipcDataChannelMin = 0;
        UINT32 uiRilChannel = pChannelData->GetRilChannel();

        // Get the mux  channel id corresponding to the control of the data channel
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
                RIL_LOG_CRITICAL("CTE_INF_7160::CoreSetupDataCall() - Unknown mux channel"
                    "for RIL Channel [%u] \r\n", uiRilChannel);
                goto Error;
        }

        szModemResourceName = CSystemManager::GetInstance().GetModemResourceName();
        ipcDataChannelMin = CSystemManager::GetInstance().GetIpcDataChannelMin();

        if (ipcDataChannelMin > hsiChannel || RIL_MAX_NUM_IPC_CHANNEL <= hsiChannel )
        {
           RIL_LOG_CRITICAL("CTE_INF_7160::CoreSetupDataCall() - Unknown HSI Channel [%d] \r\n",
                            hsiChannel);
           goto Error;
        }

        // NOTE: for LTE cmd1 for the already activated PDP shall be AT+XDATACHANNEL=...;AT+CGDATA.
        if (!PrintStringNullTerminate(rReqData.szCmd1,
            sizeof(rReqData.szCmd1),
            "AT+XDATACHANNEL=1,1,\"/mux/%d\",\"/%s/%d\",0,%d;+CGDATA=\"M-RAW_IP\",%d\r",
            muxControlChannel,szModemResourceName,hsiChannel,uiCID,uiCID))
        {
            RIL_LOG_CRITICAL("CTE_INF_7160::CoreSetupDataCall() - cannot create XDATACHANNEL"
                             "command\r\n");
            goto Error;
        }
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

    RIL_LOG_VERBOSE("CTE_XMM7160::CoreSetupDataCall() - Exit\r\n");
    return res;
}


RIL_RESULT_CODE CTE_XMM7160::ParseSetupDataCall(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::ParseSetupDataCall() - Enter\r\n");

    RIL_LOG_VERBOSE("CTE_XMM7160::ParseSetupDataCall() - Exit\r\n");
    return RRIL_RESULT_OK;
}

void CTE_XMM7160::PostSetupDataCallCmdHandler(POST_CMD_HANDLER_DATA& rData)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::PostSetupDataCallCmdHandler() Enter\r\n");

    BOOL bSuccess = FALSE;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;
    UINT32 uiCID = 0;
    CChannel_Data* pChannelData = NULL;

    if (NULL == rData.pContextData ||
            sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA) != rData.uiContextDataSize)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::PostSetupDataCallCmdHandler() - Invalid context data\r\n");
        goto Error;
    }

    pDataCallContextData = (S_SETUP_DATA_CALL_CONTEXT_DATA*)rData.pContextData;
    uiCID = pDataCallContextData->uiCID;

    if (RIL_E_SUCCESS != rData.uiResultCode)
    {
        RIL_LOG_INFO("CTE_XMM7160::PostSetupDataCallCmdHandler() - Failure\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_XMM7160::PostSetupDataCallCmdHandler() - CID=%d\r\n", uiCID);

    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_INFO("CTE_XMM7160::PostSetupDataCallCmdHandler() -"
                " No Data Channel for CID %u.\r\n", uiCID);
        goto Error;
    }


    RIL_LOG_VERBOSE("CTE_XMM7160::PostSetupDataCallCmdHandler() uiAct=%d\r\n",
                    (CTE::GetTE().GetUiAct()));
    if ( (CTE::GetTE().GetUiAct() == RADIO_TECH_LTE) &&  uiCID == 1 )
    {
        RIL_LOG_VERBOSE("CTE_XMM7160::PostSetupDataCallCmdHandler() set channel data\r\n");
        char tmpbuf[MAX_IPADDR_SIZE+1] = {0};
        char tmpbuf1[MAX_IPADDR_SIZE+1] = {0};
        char tmpbuf2[MAX_IPADDR_SIZE+1] = {0};
        char tmpbuf3[MAX_IPADDR_SIZE+1] = {0};

        pChannelData->SetDataState(E_DATA_STATE_ACTIVE);
        pChannelData->SetIpAddress(GetPDNFirstIpV4(uiCID, tmpbuf),
                                   GetPDNSecIpV4(uiCID, tmpbuf1));
        pChannelData->SetDNS(GetPDNFirstIpV4Dns(uiCID, tmpbuf),
                             GetPDNSecIpV4Dns(uiCID, tmpbuf1),
                             GetPDNFirstIpV6Dns(uiCID, tmpbuf2),
                             GetPDNSecIpV6Dns(uiCID, tmpbuf3));
        pChannelData->SetGateway(GetPDNGwIpV4(uiCID, tmpbuf1));
        CTE_XMM6360::SetupInterface(uiCID);
        HandleSetupDataCallSuccess(uiCID, rData.pRilToken);
    }
    else
    {
        pChannelData->SetDataState(E_DATA_STATE_ACTIVATING);

        if (!CreatePdpContextActivateReq(rData.uiChannel, rData.pRilToken,
                                            rData.uiRequestId, rData.pContextData,
                                            rData.uiContextDataSize,
                                            &CTE::ParsePdpContextActivate,
                                            &CTE::PostPdpContextActivateCmdHandler))
        {
                RIL_LOG_INFO("CTE_XMM7160::PostSetupDataCallCmdHandler() -"
                        " CreatePdpContextActivateReq failed\r\n");
                goto Error;
        }
    }

    bSuccess = TRUE;
Error:
    if (!bSuccess)
    {
        free(rData.pContextData);
        rData.pContextData = NULL;

        HandleSetupDataCallFailure(uiCID, rData.pRilToken, rData.uiResultCode);
    }

    RIL_LOG_VERBOSE("CTE_XMM7160::PostSetupDataCallCmdHandler() Exit\r\n");
}


void CTE_XMM7160::HandleSetupDataCallSuccess(UINT32 uiCID, void* pRilToken)
{
    RIL_LOG_VERBOSE("CTE_XMM7160::HandleSetupDataCallSuccess() - Enter\r\n");

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

    m_cte.SetupDataCallOngoing(FALSE);

    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::HandleSetupDataCallSuccess() -"
                " No Data Channel for CID %u.\r\n", uiCID);
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
        RIL_LOG_INFO("status=%d suggRetryTime=%d cid=%d active=%d type=\"%s\" ifname=\"%s\""
                " addresses=\"%s\" dnses=\"%s\" gateways=\"%s\"\r\n",
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

    RIL_LOG_VERBOSE("CTE_XMM7160::HandleSetupDataCallSuccess() - Exit\r\n");
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

        case 3:     // WCDMA prefered
        {
            pRat[0] = PREF_NET_TYPE_GSM_WCDMA;
            m_currentNetworkType = PREF_NET_TYPE_GSM_WCDMA;
            break;
        }

        case 6:     // LTE prefered
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
    S_IMS_APN_INFO sImsApnInfo;

    if (pszRequest == NULL)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() - pszRequest was empty\r\n");
        return res;
    }

    // There should be 9 parameters and the request ID
    if (nNumStrings < 10)
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() :"
                " received_size < required_size\r\n");
        return res;
    }

    memset(&sImsApnInfo, 0, sizeof(S_IMS_APN_INFO));
    strncpy(sImsApnInfo.szIMSApn, pszRequest[1], MAX_PARAM_LENGTH - 1);
    strncpy(sImsApnInfo.szOutboundProxyName, pszRequest[2], MAX_PARAM_LENGTH - 1);
    strncpy(sImsApnInfo.szOutboundProxyPort, pszRequest[3], MAX_PARAM_LENGTH - 1);
    strncpy(sImsApnInfo.szPrivateUserId,pszRequest[4], MAX_PARAM_LENGTH - 1);
    strncpy(sImsApnInfo.szHomeNWDomainName, pszRequest[5], MAX_PARAM_LENGTH - 1);
    strncpy(sImsApnInfo.szAuthName, pszRequest[6], MAX_PARAM_LENGTH - 1);
    strncpy(sImsApnInfo.szAuthPassword, pszRequest[7], MAX_PARAM_LENGTH - 1);
    strncpy(sImsApnInfo.szAuthType, pszRequest[8], MAX_PARAM_LENGTH - 1);
    strncpy(sImsApnInfo.szLoggerLevel, pszRequest[9], MAX_PARAM_LENGTH - 1);

    sImsApnInfo.szIMSApn[MAX_PARAM_LENGTH - 1] = '\0';
    sImsApnInfo.szOutboundProxyName[MAX_PARAM_LENGTH - 1] = '\0';
    sImsApnInfo.szOutboundProxyPort[MAX_PARAM_LENGTH - 1] = '\0';
    sImsApnInfo.szPrivateUserId[MAX_PARAM_LENGTH - 1] = '\0';
    sImsApnInfo.szHomeNWDomainName[MAX_PARAM_LENGTH - 1] = '\0';
    sImsApnInfo.szAuthName[MAX_PARAM_LENGTH - 1] = '\0';
    sImsApnInfo.szAuthPassword[MAX_PARAM_LENGTH - 1] = '\0';
    sImsApnInfo.szAuthType[MAX_PARAM_LENGTH - 1] = '\0';
    sImsApnInfo.szLoggerLevel[MAX_PARAM_LENGTH - 1] = '\0';

    if (0 == strncmp(sImsApnInfo.szIMSApn, "void", 4)) {
        sImsApnInfo.szIMSApn[0] = '\0';
    }

    if (0 == strncmp(sImsApnInfo.szAuthPassword, "void", 4)) {
        sImsApnInfo.szAuthPassword[0] = '\0';
    }

    RIL_LOG_INFO("CTE_XMM7160::CreateIMSConfigReq() - IMS_APN=[%s %s %s %s %s %s %s %s %s]\r\n",
            sImsApnInfo.szIMSApn,
            sImsApnInfo.szOutboundProxyName,
            sImsApnInfo.szOutboundProxyPort,
            sImsApnInfo.szPrivateUserId,
            sImsApnInfo.szHomeNWDomainName,
            sImsApnInfo.szAuthName,
            sImsApnInfo.szAuthPassword,
            sImsApnInfo.szAuthType,
            sImsApnInfo.szLoggerLevel);

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
            "AT+XICFG=\"%s\",\"%s\",%s,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",%s\r",
            sImsApnInfo.szIMSApn,
            sImsApnInfo.szOutboundProxyName,
            sImsApnInfo.szOutboundProxyPort,
            sImsApnInfo.szPrivateUserId,
            sImsApnInfo.szHomeNWDomainName,
            sImsApnInfo.szAuthName,
            sImsApnInfo.szAuthPassword,
            sImsApnInfo.szAuthType,
            sImsApnInfo.szLoggerLevel))
    {
        RIL_LOG_CRITICAL("CTE_XMM7160::CreateIMSConfigReq() - Can't construct szCmd1.\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;
Error:
    RIL_LOG_VERBOSE("CTE_XMM7160::CreateIMSConfigReq() - Exit\r\n");
    return res;
}

