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
#include "../../util.h"
#include "extract.h"
#include "rillog.h"
#include "te.h"
#include "te_base.h"
#include "../../globals.h"
#include "sync_ops.h"
#include "command.h"
#include "te_inf_6260.h"
#include "rildmain.h"
#include "callbacks.h"
#include "oemhookids.h"
#include "repository.h"
#include "../../../../linux-2.6/include/linux/gsmmux.h"
#include <cutils/properties.h>
#include <sys/system_properties.h>

//  This is for close().
#include <unistd.h>

//  This is for socket-related calls.
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>

#include <errno.h>

CTE_INF_6260::CTE_INF_6260()
: m_nCurrentNetworkType(0),
m_pQueryPIN2Event(NULL)
{
    strcpy(m_szNetworkInterfaceNamePrefix, "");
    strcpy(m_szCPIN2Result, "");

    CRepository repository;

    //  Grab the network interface name
    if (!repository.Read(g_szGroupModem, g_szNetworkInterfaceNamePrefix, m_szNetworkInterfaceNamePrefix, MAX_BUFFER_SIZE))
    {
        RIL_LOG_CRITICAL("CCTE_INF_6260::CTE_INF_6260() - Could not read network interface name prefix from repository\r\n");
        strcpy(m_szNetworkInterfaceNamePrefix, "");
    }
    else
    {
        RIL_LOG_INFO("CTE_INF_6260::CTE_INF_6260() - m_szNetworkInterfaceNamePrefix=[%s]\r\n", m_szNetworkInterfaceNamePrefix);
    }

    //  Create PIN2 query event
    m_pQueryPIN2Event = new CEvent();
    if (!m_pQueryPIN2Event)
    {
        RIL_LOG_CRITICAL("TE_INF_6260::CTE_INF_6260() - ERROR: Could not create new QueryPIN2Event!\r\n");
    }

}

CTE_INF_6260::~CTE_INF_6260()
{
    delete m_pQueryPIN2Event;
    m_pQueryPIN2Event = NULL;
}


//
// RIL_REQUEST_GET_SIM_STATUS 1
//
RIL_RESULT_CODE CTE_INF_6260::CoreGetSimStatus(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreGetSimStatus() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

#if defined(M2_PIN_RETRIES_FEATURE_ENABLED)
    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CPIN?;+XUICC?;+XPINCNT\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }
#else  // M2_PIN_RETRIES_FEATURE_ENABLED
    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CPIN?;+XUICC?\r", sizeof(rReqData.szCmd1)))
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

    const char * pszRsp = rRspData.szResponse;
    RIL_CardStatus_v6* pCardStatus = NULL;
    RIL_RESULT_CODE res = CTEBase::ParseSimPin(pszRsp, pCardStatus);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetSimStatus() - ERROR: Could not parse Sim Pin.\r\n");
        goto Error;
    }

    if (pCardStatus->card_state != RIL_CARDSTATE_ABSENT)
    {
        // Parse "<prefix>+XUICC: <state><postfix>"
        SkipRspStart(pszRsp, g_szNewLine, pszRsp);

        if (SkipString(pszRsp, "+XUICC: ", pszRsp))
        {
            if (!ExtractUpperBoundedUInt32(pszRsp, 2, nValue, pszRsp))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetSimStatus() - ERROR: Invalid SIM type.\r\n");
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

            SkipRspEnd(pszRsp, g_szNewLine, pszRsp);
        }

#if defined(M2_PIN_RETRIES_FEATURE_ENABLED)
        // Parse "<prefix>+XPINCNT: <PIN attempts>, <PIN2 attempts>, <PUK attempts>, <PUK2 attempts><postfix>"
        SkipRspStart(pszRsp, g_szNewLine, pszRsp);

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
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetSimStatus() - ERROR: Cannot parse XPINCNT\r\n");
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
            SkipRspEnd(pszRsp, g_szNewLine, pszRsp);
        }

#endif // M2_PIN_RETRIES_FEATURE_ENABLED
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


// RIL_REQUEST_SETUP_DATA_CALL 27
RIL_RESULT_CODE CTE_INF_6260::CoreSetupDataCall(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize, UINT32 uiCID)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSetupDataCall() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    PdpData stPdpData;
    memset(&stPdpData, 0, sizeof(PdpData));

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetupDataCall() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize < (6 * sizeof(char*)))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetupDataCall() - ERROR: Invalid data size. Was given %d bytes\r\n", uiDataSize);
        goto Error;
    }

    RIL_LOG_INFO("CTE_INF_6260::CoreSetupDataCall() - uiDataSize=[%d]\r\n", uiDataSize);


    // extract data
    stPdpData.szRadioTechnology = ((char **)pData)[0];  // not used
    stPdpData.szRILDataProfile  = ((char **)pData)[1];  // not used
    stPdpData.szApn             = ((char **)pData)[2];
    stPdpData.szUserName        = ((char **)pData)[3];  // not used
    stPdpData.szPassword        = ((char **)pData)[4];  // not used
    stPdpData.szPAPCHAP         = ((char **)pData)[5];  // not used

    RIL_LOG_INFO("CTE_INF_6260::CoreSetupDataCall() - stPdpData.szRadioTechnology=[%s]\r\n", stPdpData.szRadioTechnology);
    RIL_LOG_INFO("CTE_INF_6260::CoreSetupDataCall() - stPdpData.szRILDataProfile=[%s]\r\n", stPdpData.szRILDataProfile);
    RIL_LOG_INFO("CTE_INF_6260::CoreSetupDataCall() - stPdpData.szApn=[%s]\r\n", stPdpData.szApn);
    RIL_LOG_INFO("CTE_INF_6260::CoreSetupDataCall() - stPdpData.szUserName=[%s]\r\n", stPdpData.szUserName);
    RIL_LOG_INFO("CTE_INF_6260::CoreSetupDataCall() - stPdpData.szPassword=[%s]\r\n", stPdpData.szPassword);
    RIL_LOG_INFO("CTE_INF_6260::CoreSetupDataCall() - stPdpData.szPAPCHAP=[%s]\r\n", stPdpData.szPAPCHAP);


    if (RIL_VERSION >= 4 && (uiDataSize >= (7 * sizeof(char*))))
    {
        stPdpData.szPDPType         = ((char **)pData)[6];  // new in Android 2.3.4.
        RIL_LOG_INFO("CTE_INF_6260::CoreSetupDataCall() - stPdpData.szPDPType=[%s]\r\n", stPdpData.szPDPType);
    }

    // For setting up data call we need to send 2 sets of chained commands: AT+CGDCONT to define PDP Context, then
    // if RAW IP is used send AT+CGDATA to enable Raw IP on data channel (which will then switch the channel to data mode).
    //
#if defined(M2_IPV6_FEATURE_ENABLED)
    //  IP type is passed in dynamically.
    if (NULL == stPdpData.szPDPType)
    {
        //  hard-code "IPV4V6" (this is the default)
        strncpy(stPdpData.szPDPType, "IPV4V6",sizeof("IPV4V6"));
    }

    //  dynamic PDP type, need to set XDNS parameter depending on szPDPType.
    //  If not recognized, just use IPV4V6 as default.
    if (0 == strcmp(stPdpData.szPDPType, "IP"))
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1,
            sizeof(rReqData.szCmd1),
            "AT+CGDCONT=%d,\"%s\",\"%s\",,0,0;+XDNS=%d,1\r", uiCID, stPdpData.szPDPType,
            stPdpData.szApn, uiCID))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetupDataCall() - ERROR: cannot create CGDCONT command, stPdpData.szPDPType\r\n");
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
            RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetupDataCall() - ERROR: cannot create CGDCONT command, stPdpData.szPDPType\r\n");
            goto Error;
        }
    }
    else if (0 == strcmp(stPdpData.szPDPType, "IPV4V6"))
    {
        //  XDNS=3 is not supported by the modem so two commands +XDNS=1 and +XDNS=2 should be sent.
        if (!PrintStringNullTerminate(rReqData.szCmd1,
            sizeof(rReqData.szCmd1),
            "AT+CGDCONT=%d,\"IPV4V6\",\"%s\",,0,0;+XDNS=%d,1;+XDNS=%d,2\r", uiCID,
            stPdpData.szApn, uiCID, uiCID))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetupDataCall() - ERROR: cannot create CGDCONT command, stPdpData.szPDPType\r\n");
            goto Error;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetupDataCall() - ERROR: Wrong PDP type\r\n");
        goto Error;
    }

#else // M2_IPV6_FEATURE_ENABLED
    //  just hard-code "IP" as we do not support IPv6
    if (!PrintStringNullTerminate(rReqData.szCmd1,
        sizeof(rReqData.szCmd1),
        "AT+CGDCONT=%d,\"IP\",\"%s\",,0,0;+XDNS=%d,1\r", uiCID,
        stPdpData.szApn, uiCID))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetupDataCall() - ERROR: cannot create CGDCONT command, IP hard-coded\r\n");
        goto Error;
    }
#endif // M2_IPV6_FEATURE_ENABLED


    if (!PrintStringNullTerminate(rReqData.szCmd2, sizeof(rReqData.szCmd2), "AT+CGACT=1,%d;+CGDATA=\"M-RAW_IP\",%d\r", uiCID, uiCID))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetupDataCall() - ERROR: cannot create CGDATA command\r\n");
        goto Error;
    }

    //  Store the potential uiCID in the pContext
    rReqData.pContextData = (void*)uiCID;

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSetupDataCall() - Exit\r\n");
    return res;
}


RIL_RESULT_CODE CTE_INF_6260::ParseSetupDataCall(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_INF_6260::ParseSetupDataCall() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char szIP[PROPERTY_VALUE_MAX] = {0};
    P_ND_SETUP_DATA_CALL pDataCallRsp = NULL;



    // For RAW IP, when we get the CONNECT response to AT+CGDATA, we then need to send AT+CGPADDR (or AT+CGDCONT?)
    // to get the IP address (which is returned in the response to the RIL_REQUEST_SETUP_DATA_CALL) and finally
    // AT+XDNS to query for the DNS address of the activated context. We wait until we get DNS response before
    // sending the reply to RIL_REQUEST_SETUP_DATA_CALL.

// N_GSM related code
    struct gsm_config cfg;
    struct gsm_netconfig netconfig;

    CCommand * pCmd1 = NULL;
    CCommand * pCmd2 = NULL;
    int nIPAddrTimeout = 5000, nDNSTimeout = 5000, nTotalTimeout = (nIPAddrTimeout + nDNSTimeout);
    BOOL bRet1 = FALSE, bRet2 = FALSE;
    UINT32 uiWaitRes;
    char szCmd[MAX_BUFFER_SIZE];
    CChannel_Data* pChannelData = NULL;
    UINT32 nCID = 0;
    int fd = -1;
    int ret = 0;
    CRepository repository;
    PDP_TYPE eDataConnectionType = PDP_TYPE_IPV4;  //  dummy for now, set to IPv4.

    // 1st confirm we got "CONNECT"
    const char* szRsp = rRspData.szResponse;

    if (!FindAndSkipString(szRsp, "CONNECT", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: Did not get \"CONNECT\" response.\r\n");
        goto Error;
    }

    pChannelData = CChannel_Data::GetChnlFromRilChannelNumber(rRspData.uiChannel);
    if (!pChannelData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: Could not get Data Channel for RIL channel number %d.\r\n", rRspData.uiChannel);
        goto Error;
    }

    //  Set CID
    nCID = (UINT32)rRspData.pContextData;
    if (nCID == 0)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - CID must be >= 1!! CID=[%d]\r\n", nCID);
        goto Error;
    }

// Following code-block is moved up here from the end of this function to get if_name needed for netconfig (N_GSM)
// But the IP address is filled in end of function.
    pDataCallRsp = (P_ND_SETUP_DATA_CALL)malloc(sizeof(S_ND_SETUP_DATA_CALL));
    if (NULL == pDataCallRsp)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: Cannot allocate pDataCallRsp  size=[%d]\r\n", sizeof(S_ND_SETUP_DATA_CALL));
        goto Error;
    }
    memset(pDataCallRsp, 0, sizeof(S_ND_SETUP_DATA_CALL));

    //  Populate pDataCallRsp
    sprintf(pDataCallRsp->szCID, "%d", nCID);
    if (!PrintStringNullTerminate(pDataCallRsp->szNetworkInterfaceName, MAX_BUFFER_SIZE, "%s%d", m_szNetworkInterfaceNamePrefix, nCID-1))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: Cannot set network interface name\r\n");
        goto Error;
    }
    else
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseSetupDataCall() - szNetworkInterfaceName=[%s], CID=[%d]\r\n", pDataCallRsp->szNetworkInterfaceName, nCID);
    }

    pDataCallRsp->sSetupDataCallPointers.pszCID = pDataCallRsp->szCID;
    pDataCallRsp->sSetupDataCallPointers.pszNetworkInterfaceName = pDataCallRsp->szNetworkInterfaceName;


// N_GSM related code
    netconfig.adaption = 3;
    netconfig.protocol = htons(ETH_P_IP);
    strncpy(netconfig.if_name, pDataCallRsp->szNetworkInterfaceName, IFNAMSIZ);
// Add IF NAME
    fd = pChannelData->GetFD();
    if (fd >= 0)
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseSetupDataCall() - ***** PUTTING channel=[%d] in DATA MODE *****\r\n", rRspData.uiChannel);
        ret = ioctl( fd, GSMIOC_ENABLE_NET, &netconfig );       // Enable data channel
    }
    else
    {
        //  No FD.
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: Could not get Data Channel chnl=[%d] fd=[%d].\r\n", rRspData.uiChannel, fd);
        goto Error;
    }


    // Send AT+CGPADDR and AT+XDNS? commands to query for assigned IP Address and DNS and wait for responses

    CEvent::Reset(pChannelData->m_pSetupDoneEvent);

    if (!PrintStringNullTerminate(szCmd, MAX_BUFFER_SIZE, "AT+CGPADDR=%d\r", nCID))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: cannot create CGPADDR command\r\n");
        goto Error;
    }

    pCmd1 = new CCommand(g_arChannelMapping[ND_REQ_ID_GETIPADDRESS], NULL, ND_REQ_ID_GETIPADDRESS, szCmd, &CTE::ParseIpAddress);
    if (pCmd1)
    {
        if (!CCommand::AddCmdToQueue(pCmd1))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: Unable to queue AT+CGPADDR command!\r\n");
            delete pCmd1;
            pCmd1 = NULL;
            goto Error;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: Unable to allocate memory for new AT+CGPADDR command!\r\n");
        goto Error;
    }

    pCmd2 = new CCommand(g_arChannelMapping[ND_REQ_ID_GETDNS], NULL, ND_REQ_ID_GETDNS, "AT+XDNS?\r", &CTE::ParseDns);
    if (pCmd2)
    {
        //  Need to send context ID to parse function so set contextData for this command
        pCmd2->SetContextData(rRspData.pContextData);
        if (!CCommand::AddCmdToQueue(pCmd2))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: Unable to queue AT+XDNS? command!\r\n");
            delete pCmd2;
            pCmd2 = NULL;
            goto Error;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: Unable to allocate memory for new AT+XDNS? command!\r\n");
        goto Error;
    }

    //  Read the 2 AT command timeouts from repository.  Sum them.
    bRet1 = repository.Read(g_szGroupRequestTimeouts, g_szRequestNames[ND_REQ_ID_GETIPADDRESS], nIPAddrTimeout);
    bRet2 = repository.Read(g_szGroupRequestTimeouts, g_szRequestNames[ND_REQ_ID_GETDNS], nDNSTimeout);
    if (bRet1 && bRet2)
    {
        nTotalTimeout = nIPAddrTimeout + nDNSTimeout;
        RIL_LOG_INFO("CTE_INF_6260::ParseSetupDataCall() - nIPAddrTimeout=[%d] + nDNSTimeout=[%d] = nTotalTimeout=[%d]\r\n",
                        nIPAddrTimeout, nDNSTimeout, nTotalTimeout);
    }
    else
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: bRet1=[%d] bRet2=[%d]  nTotalTimeout=[%d]\r\n",
                        bRet1, bRet2, nTotalTimeout);
    }


    uiWaitRes = CEvent::Wait(pChannelData->m_pSetupDoneEvent, nTotalTimeout);
    switch (uiWaitRes)
    {
        case WAIT_EVENT_0_SIGNALED:
            RIL_LOG_INFO("CTE_INF_6260::ParseSetupDataCall() : SetupData event signalled\r\n");
#if defined(M2_IPV6_FEATURE_ENABLED)
            // pChannelData->m_szIpAddr and pChannelData->m_szIpAddr2 can not be null because if initialization fails
            // the connection manager do not call this function but they can be empty
            if (0 == pChannelData->m_szIpAddr[0] && 0 == pChannelData->m_szIpAddr2[0])
            {
                 RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: IP addresses are null\r\n");
                 goto Error;
            }
#else
            if (NULL == pChannelData->m_szIpAddr)
            {
                 RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: IP address is null\r\n");
                 goto Error;
            }
#endif

            if (NULL == pChannelData->m_szDNS1)
            {
                 RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: DNS1 is null\r\n");
                 goto Error;
            }
            if (NULL == pChannelData->m_szDNS2)
            {
                 RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: DNS2 is null\r\n");
                 goto Error;
            }
#if defined(M2_IPV6_FEATURE_ENABLED)
            if (0 == pChannelData->m_szIpAddr[0])
            {
                strcpy(szIP, pChannelData->m_szIpAddr2);
            }
            else
            {
                strcpy(szIP, pChannelData->m_szIpAddr);
            }
#else
            strcpy(szIP, pChannelData->m_szIpAddr);
#endif
            break;

        case WAIT_TIMEDOUT:
             RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - Timed out waiting for IP Address and DNS\r\n");
             goto Error;

        default:
             RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - Unexpected event result on Wait for IP Address and DNS, res: %d\r\n", uiWaitRes);
             goto Error;
    }


    // invoke netcfg commands
    //  At this point we should have received a +CGEV: notification with the type
    //        of data connection made (IPv4, IPv6, IPv4v6).  Configure the data connection
    //        based on the type.  Types are defined in CORE/ND/nd_structs.h

#if defined(M2_IPV6_FEATURE_ENABLED)
    if(pChannelData->m_szIpAddr2[0] == 0 )
    {
        eDataConnectionType =PDP_TYPE_IPV4;
    }
    else if (pChannelData->m_szIpAddr[0] == 0 )
    {
        eDataConnectionType =PDP_TYPE_IPV6;
    }
    else
    {
        eDataConnectionType =PDP_TYPE_IPV4V6;
    }
#else
    eDataConnectionType = PDP_TYPE_IPV4;
#endif // M2_IPV6_FEATURE_ENABLED

    if (!DataConfigUp(pDataCallRsp->szNetworkInterfaceName, pChannelData, eDataConnectionType))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: Unable to set ifconfig\r\n");
        goto Error;
    }

    strcpy(pDataCallRsp->szIPAddress, szIP);
    pDataCallRsp->sSetupDataCallPointers.pszIPAddress = pDataCallRsp->szIPAddress;

    //  New for Android 2.3.4 - TODO: Implement for Android 3.0.
    pDataCallRsp->sSetupDataCallPointers.pszDNS = NULL;
    pDataCallRsp->sSetupDataCallPointers.pszGateway = NULL;


    rRspData.pData = (void*)pDataCallRsp;

    if (RIL_VERSION >= 4)
    {
        rRspData.uiDataSize = sizeof(S_ND_SETUP_DATA_CALL_POINTERS);
    }
    else
    {
        rRspData.uiDataSize = 3 * sizeof(char *);
    }
    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseSetupDataCall() - Error cleanup\r\n");
        if (pChannelData)
        {
            RIL_LOG_INFO("CTE_INF_6260::ParseSetupDataCall() - Calling DataConfigDown  chnl=[%d], cid=[%d]\r\n", rRspData.uiChannel, nCID);

            //  Explicitly deactivate context ID = nCID
            //  This will call DataConfigDown(nCID) and convert channel to AT mode.
            //  Otherwise the data channel hangs and is unresponsive.
            RIL_requestTimedCallback(triggerDeactivateDataCall, (void*)nCID, 0, 0);
        }

        free(pDataCallRsp);
        pDataCallRsp = NULL;
    }
    RIL_LOG_INFO("CTE_INF_6260::ParseSetupDataCall() - Exit\r\n");
    return res;
}

void init_sockaddr_in(struct sockaddr_in *sin, const char *addr)
{
    sin->sin_family = AF_INET;
    sin->sin_port = 0;
    sin->sin_addr.s_addr = inet_addr(addr);
}

BOOL setaddr6(int sockfd6, struct ifreq *ifr, const char *addr)
{
    int ret;

    struct in6_ifreq ifr6;
    inet_pton(AF_INET6, addr, &ifr6.ifr6_addr);

    ioctl(sockfd6, SIOGIFINDEX, ifr);
    ifr6.ifr6_ifindex = ifr->ifr_ifindex;
    ifr6.ifr6_prefixlen = 64; //prefix_len;

    RIL_LOG_INFO("setaddr6() - calling SIOCSIFADDR ADDR:%s INET:%s\r\n",addr,ifr->ifr_name);
    errno = 0;
    ret = ioctl(sockfd6, SIOCSIFADDR, &ifr6);
    if (ret < 0)
    {
        RIL_LOG_CRITICAL("setaddr6() : ERROR: SIOCSIFADDR : %s\r\n",
                strerror(errno));
        return FALSE;
    }

    return TRUE;
}

BOOL setaddr(int s, struct ifreq *ifr, const char *addr)
{
    int ret;
    init_sockaddr_in((struct sockaddr_in *) &ifr->ifr_addr, addr);
    RIL_LOG_INFO("setaddr - calling SIOCSIFADDR\r\n");
    errno = 0; // NOERROR
    ret = ioctl(s, SIOCSIFADDR, ifr);
    if (ret < 0)
    {
        RIL_LOG_CRITICAL("setaddr() : ERROR: SIOCSIFADDR : %s\r\n",
            strerror(errno));
        return FALSE;
    }
    return TRUE;
}

BOOL setflags(int s, struct ifreq *ifr, int set, int clr)
{
    int ret;
    RIL_LOG_INFO("setflags - calling SIOCGIFFLAGS\r\n");
    ret = ioctl(s, SIOCGIFFLAGS, ifr);
    if (ret < 0)
    {
        RIL_LOG_CRITICAL("setflags : ERROR: SIOCGIFFLAGS : %d\r\n", ret);
        return FALSE;
    }

    ifr->ifr_flags = (ifr->ifr_flags & (~clr)) | set;
    RIL_LOG_INFO("setflags - calling SIOCGIFFLAGS 2\r\n");
    ret = ioctl(s, SIOCSIFFLAGS, ifr);
    if (ret < 0)
    {
        RIL_LOG_CRITICAL("setflags: ERROR: SIOCSIFFLAGS 2 : %d\r\n", ret);
        return FALSE;
    }
    return TRUE;
}

//
//  Call this function whenever data is activated
//
BOOL DataConfigUp(char *szNetworkInterfaceName, CChannel_Data* pChannelData, PDP_TYPE eDataConnectionType)
{
    BOOL bRet = FALSE;
    RIL_LOG_INFO("DataConfigUp() ENTER\r\n");

    switch(eDataConnectionType)
    {
        case PDP_TYPE_IPV4:
            RIL_LOG_INFO("DataConfigUp() - IPV4 - Calling DataConfigUpIpV4()\r\n");
            bRet = DataConfigUpIpV4(szNetworkInterfaceName, pChannelData);
            break;

#if defined(M2_IPV6_FEATURE_ENABLED)
        case PDP_TYPE_IPV6:
            RIL_LOG_INFO("DataConfigUp() - IPV6 - Calling DataConfigUpIpV6()\r\n");
            bRet = DataConfigUpIpV6(szNetworkInterfaceName, pChannelData);
            break;

        case PDP_TYPE_IPV4V6:
            RIL_LOG_INFO("DataConfigUp() - IPV4V6 - Calling DataConfigUpIpV4V6()\r\n");
            bRet = DataConfigUpIpV4V6(szNetworkInterfaceName, pChannelData);
            break;
#endif // M2_IPV6_FEATURE_ENABLED

        default:
            RIL_LOG_CRITICAL("DataConfigUp() - Unknown PDP_TYPE!  eDataConnectionType=[%d]\r\n", eDataConnectionType);
            bRet = FALSE;
            break;
    }

    RIL_LOG_INFO("DataConfigUp() EXIT=%d\r\n", bRet);
    return bRet;
}


BOOL DataConfigUpIpV4(char *szNetworkInterfaceName, CChannel_Data* pChannelData)
{
    BOOL bRet = FALSE;
    int s = -1;
    char szPropName[MAX_BUFFER_SIZE] = {0};
    char *defaultGatewayStr = NULL;

    char *szIpAddr = pChannelData->m_szIpAddr;
    char *szDNS1 = pChannelData->m_szDNS1;
    char *szDNS2 = pChannelData->m_szDNS2;


    RIL_LOG_INFO("DataConfigUpIpV4() ENTER  szNetworkInterfaceName=[%s]  szIpAddr=[%s]\r\n", szNetworkInterfaceName, szIpAddr);
    RIL_LOG_INFO("DataConfigUpIpV4() ENTER  szDNS1=[%s]  szDNS2=[%s]\r\n", szDNS1, szDNS2);



    // set net. properties
    // TODO For multiple PDP context support this will have to be updated to be consistent with whatever changes to
    // the Android framework's use of these properties is made. (Currently there is only one property for the
    // IP address and there needs to be one for each context.


    //  Open socket for ifconfig command
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        RIL_LOG_CRITICAL("DataConfigUpIpV4() : cannot open control socket\n");
        goto Error;
    }

    //  Code in this function is from system/core/toolbox/ifconfig.c
    //  also from system/core/toolbox/route.c


    //  ifconfig ifx02 <ip address> netmask 255.255.255.0
    {
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(struct ifreq));
        strncpy(ifr.ifr_name, szNetworkInterfaceName, IFNAMSIZ);
        ifr.ifr_name[IFNAMSIZ-1] = 0;

        RIL_LOG_INFO("DataConfigUpIpV4() : Setting addr\r\n");
        if (!setaddr(s, &ifr, szIpAddr)) // ipaddr
        {
            //goto Error;
            RIL_LOG_CRITICAL("DataConfigUpIpV4() : Error setting add\r\n");
        }

        RIL_LOG_INFO("DataConfigUpIpV4() : Setting flags\r\n");
        if (!setflags(s, &ifr, IFF_UP, 0))
        {
            //goto Error;
            RIL_LOG_CRITICAL("DataConfigUpIpV4() : Error setting flags\r\n");
        }
    }

    // TODO retrieve gateway from the modem with XDNS?
    if (defaultGatewayStr == NULL)
    {
        in_addr_t gw;
        struct in_addr gwaddr;
        in_addr_t addr;

        RIL_LOG_INFO("DataConfigUpIpV4() : set default gateway to fake value");
        if (inet_pton(AF_INET, szIpAddr, &addr) <= 0)
        {
            RIL_LOG_INFO("DataConfigUpIpV4() : inet_pton() failed for %s!", szIpAddr);
            goto Error;
        }
        gw = ntohl(addr) & 0xFFFFFF00;
        gw |= 1;
        gwaddr.s_addr = htonl(gw);

        defaultGatewayStr = strdup(inet_ntoa(gwaddr));
    }
    if (defaultGatewayStr != NULL)
    {
        //  Set gateway system property
        if (!PrintStringNullTerminate(szPropName, MAX_BUFFER_SIZE, "net.%s.gw", szNetworkInterfaceName))
        {
            RIL_LOG_CRITICAL("DataConfigUpIpV4() :cannot create szPropName net.X.gw\r\n");
        }
        else
        {
            RIL_LOG_INFO("DataConfigUpIpV4() - setting '%s' to '%s'\r\n", szPropName, defaultGatewayStr);
            property_set(szPropName, defaultGatewayStr);
        }
    }

    //  Set DNS1
    if (szDNS1)
    {
        if (!PrintStringNullTerminate(szPropName, MAX_BUFFER_SIZE, "net.%s.dns1", szNetworkInterfaceName))
        {
            RIL_LOG_CRITICAL("DataConfigUpIpV4() :cannot create szPropName net.X.dns1\r\n");
        }
        else
        {
            RIL_LOG_INFO("DataConfigUpIpV4() - setting '%s' to '%s'\r\n", szPropName, szDNS1);
            property_set(szPropName, szDNS1);
        }
    }

    //  Set DNS2
    if (szDNS2)
    {
        if (!PrintStringNullTerminate(szPropName, MAX_BUFFER_SIZE, "net.%s.dns2", szNetworkInterfaceName))
        {
            RIL_LOG_CRITICAL("DataConfigUpIpV4() :cannot create szPropName net.X.dns2\r\n");
        }
        else
        {
            RIL_LOG_INFO("DataConfigUpIpV4() - setting '%s' to '%s'\r\n", szPropName, szDNS2);
            property_set(szPropName, szDNS2);
        }
    }

    bRet = TRUE;

Error:
    if (defaultGatewayStr != NULL)
        free(defaultGatewayStr);

    if (s >= 0)
    {
        close(s);
    }

    RIL_LOG_INFO("DataConfigUpIpV4() EXIT  bRet=[%d]\r\n", bRet);

    return bRet;
}

#if defined(M2_IPV6_FEATURE_ENABLED)

BOOL DataConfigUpIpV6(char *szNetworkInterfaceName, CChannel_Data* pChannelData)
{
    BOOL bRet = FALSE;
    int s = -1;
    char szPropName[MAX_BUFFER_SIZE] = {0};
    char *szIpAddr = pChannelData->m_szIpAddr2;
    char *szDNS1 = pChannelData->m_szIpV6DNS1;
    char *szDNS2 = pChannelData->m_szIpV6DNS2;

    char szIpAddrOut[50];
    struct in6_addr ifIdAddr;
    struct in6_addr ifPrefixAddr;
    struct in6_addr ifOutAddr;


    RIL_LOG_INFO("DataConfigUpIpV6() ENTER  szNetworkInterfaceName=[%s]  szIpAddr=[%s]\r\n", szNetworkInterfaceName, szIpAddr);
    RIL_LOG_INFO("DataConfigUpIpV6() ENTER  szDNS1=[%s]  szDNS2=[%s]\r\n", szDNS1, szDNS2);


    // set net. properties
    // TODO For multiple PDP context support this will have to be updated to be consistent with whatever changes to
    // the Android framework's use of these properties is made. (Currently there is only one property for the
    // IP address and there needs to be one for each context.


    //  Open socket for ifconfig command (note this is ipv6 socket)
    s = socket(AF_INET6, SOCK_DGRAM, 0);
    if (s < 0)
    {
        RIL_LOG_CRITICAL("DataConfigUpIpV6() : cannot open control socket\n");
        goto Error;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, szNetworkInterfaceName, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ-1] = 0;

    inet_pton(AF_INET6, szIpAddr, &ifIdAddr);
    inet_pton(AF_INET6, "FE80::", &ifPrefixAddr);


    // Set local prefix from FE80::
    memcpy(ifOutAddr.s6_addr,ifPrefixAddr.s6_addr,8);
    // Set interface identifier from address given by network
    memcpy((ifOutAddr.s6_addr)+8,(ifIdAddr.s6_addr)+8,8);

    inet_ntop(AF_INET6, &ifOutAddr, szIpAddrOut, sizeof(szIpAddrOut));
    strncpy(szIpAddr,szIpAddrOut,sizeof(szIpAddrOut));
    RIL_LOG_INFO("DataConfigUpIpV6() : Setting addr :%s\r\n",szIpAddr);

    if (!setaddr6(s, &ifr, szIpAddr))
    {
        //goto Error;
        RIL_LOG_CRITICAL("DataConfigUpIpV6() : Error setting addr %s\r\n", szIpAddr);
    }

    //  Before setting interface UP, need to deactivate DAD on interface.
    char file_to_open[100];
    file_to_open[0]='\0';
    FILE * fp;

    //  Open dad_transmits file, write 0<lf>.
    snprintf(file_to_open, 99, "/proc/sys/net/ipv6/conf/%s/dad_transmits", szNetworkInterfaceName);

    fp = fopen(file_to_open, "w");
    if (fp)
    {
        char szData[] = "0\n";

        RIL_LOG_INFO("DataConfigUpIpV6() : Opened file=[%s]\r\n", file_to_open);
        if (EOF == fputs(szData, fp))
        {
            RIL_LOG_CRITICAL("DataConfigUpIpV6() : ERROR: file=[%s] cannot write value [%s]\r\n", file_to_open, szData);
        }

        else
        {
            RIL_LOG_INFO("DataConfigUpIpV6() : Wrote [%s] to file=[%s]\r\n", CRLFExpandedString(szData, strlen(szData)).GetString(), file_to_open);
        }

        //  Close file handle
        fclose(fp);
    }
    else
    {
        RIL_LOG_CRITICAL("DataConfigUpIpV6() : ERROR: Cannot open [%s]\r\n", file_to_open);
    }
    //  Open accept_dad file, write 0<lf>.
    snprintf(file_to_open, 99, "/proc/sys/net/ipv6/conf/%s/accept_dad", szNetworkInterfaceName);

    fp = fopen(file_to_open, "w");
    if (fp)
    {
        char szData[] = "0\n";

        RIL_LOG_INFO("DataConfigUpIpV6() : Opened file=[%s]\r\n", file_to_open);
        if (EOF == fputs(szData, fp))
        {
            RIL_LOG_CRITICAL("DataConfigUpIpV6() : ERROR: file=[%s] cannot write value [%s]\r\n", file_to_open, szData);
        }
        else
        {
            RIL_LOG_INFO("DataConfigUpIpV6() : Wrote [%s] to file=[%s]\r\n", CRLFExpandedString(szData, strlen(szData)).GetString(), file_to_open);
        }

        //  Close file handle.
        fclose(fp);
    }
    else
    {
        RIL_LOG_CRITICAL("DataConfigUpIpV6() : ERROR: Cannot open [%s]\r\n", file_to_open);
    }
    RIL_LOG_INFO("DataConfigUpIpV6() : Setting flags\r\n");
    if (!setflags(s, &ifr, IFF_UP, 0))
    {
        //goto Error;
        RIL_LOG_CRITICAL("DataConfigUpIpV6() : Error setting flags\r\n");
    }

    //  Set DNS1
    if (szDNS1)
    {
        if (!PrintStringNullTerminate(szPropName, MAX_BUFFER_SIZE, "net.%s.dns1", szNetworkInterfaceName))
        {
            RIL_LOG_CRITICAL("DataConfigUpIpV6() :cannot create szPropName net.X.dns1\r\n");
        }
        else
        {
            RIL_LOG_INFO("DataConfigUpIpV6() - setting '%s' to '%s'\r\n", szPropName, szDNS1);
            property_set(szPropName, szDNS1);
        }
    }

    //  Set DNS2
    if (szDNS2)
    {
        if (!PrintStringNullTerminate(szPropName, MAX_BUFFER_SIZE, "net.%s.dns2", szNetworkInterfaceName))
        {
            RIL_LOG_CRITICAL("DataConfigUpIpV6() :cannot create szPropName net.X.dns2\r\n");
        }
        else
        {
            RIL_LOG_INFO("DataConfigUpIpV6() - setting '%s' to '%s'\r\n", szPropName, szDNS2);
            property_set(szPropName, szDNS2);
        }
    }
    bRet = TRUE;

Error:
    if (s >= 0)
    {
        close(s);
    }

    RIL_LOG_INFO("DataConfigUpIpV6() EXIT  bRet=[%d]\r\n", bRet);
    return bRet;
}

BOOL DataConfigUpIpV4V6(char *szNetworkInterfaceName,CChannel_Data* pChannelData )
{
    CRepository repository;
    BOOL bRet = FALSE;
    int s = -1;
    int s6 =-1;
    char szPropName[MAX_BUFFER_SIZE] = {0};
    char *defaultGatewayStr = NULL;

    char szIpAddrOut[50];
    struct in6_addr ifIdAddr;
    struct in6_addr ifPrefixAddr;
    struct in6_addr ifOutAddr;

    char *szIpAddr = pChannelData->m_szIpAddr;
    char *szDNS1 = pChannelData->m_szDNS1;
    char *szDNS2 = pChannelData->m_szDNS2;
    char *szIpAddr2 = pChannelData->m_szIpAddr2;
    char *szIpV6DNS1 = pChannelData->m_szIpV6DNS1;
    char *szIpV6DNS2 = pChannelData->m_szIpV6DNS2;

    RIL_LOG_INFO("DataConfigUpIpV4V6() ENTER  szNetworkInterfaceName=[%s]  szIpAddr=[%s] szIpAddr2=[%s]\r\n", szNetworkInterfaceName, szIpAddr, szIpAddr2);
    RIL_LOG_INFO("DataConfigUpIpV4V6() ENTER  szDNS1=[%s]  szDNS2=[%s] szIpV6DNS1=[%s]  szIpV6DNS2=[%s]\r\n", szDNS1, szDNS2, szIpV6DNS1, szIpV6DNS2);



    // set net. properties
    // TODO For multiple PDP context support this will have to be updated to be consistent with whatever changes to
    // the Android framework's use of these properties is made. (Currently there is only one property for the
    // IP address and there needs to be one for each context.


    //  Open socket for ifconfig and setFlags commands
    s = socket(AF_INET, SOCK_DGRAM, 0);
    s6 = socket(AF_INET6, SOCK_DGRAM, 0);
    if (s < 0 || s6 < 0)
    {
        RIL_LOG_CRITICAL("DataConfigUpIpV4V6() : cannot open control socket\n");
        goto Error;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, szNetworkInterfaceName, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ-1] = 0;

    RIL_LOG_INFO("DataConfigUpIpV4V6() : Setting addr\r\n");
    if (!setaddr(s, &ifr, szIpAddr)) // ipaddr
    {
        //goto Error;
        RIL_LOG_CRITICAL("DataConfigUpIpV4V6() : Error setting add\r\n");
    }

    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, szNetworkInterfaceName, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ-1] = 0;

    // Set link local address to start the SLAAC process
    inet_pton(AF_INET6, szIpAddr2, &ifIdAddr);
    inet_pton(AF_INET6, "FE80::", &ifPrefixAddr);

    // Set local prefix from FE80::
    memcpy(ifOutAddr.s6_addr,ifPrefixAddr.s6_addr,8);
    // Set interface identifier from address given by network
    memcpy((ifOutAddr.s6_addr)+8,(ifIdAddr.s6_addr)+8,8);

    inet_ntop(AF_INET6, &ifOutAddr, szIpAddrOut, sizeof(szIpAddrOut));
    strncpy(szIpAddr2,szIpAddrOut,sizeof(szIpAddrOut));


    if (!setaddr6(s6, &ifr, szIpAddr2)) // ipaddr
    {
        //goto Error;
        RIL_LOG_CRITICAL("DataConfigUpIpV4V6() : Error setting add\r\n");
    }

    //  Before setting interface UP, need to deactivate DAD on interface.
    char file_to_open[100];
    file_to_open[0]='\0';
    FILE * fp;

    //  Open dad_transmits file, write 0<lf>.
    snprintf(file_to_open, 99, "/proc/sys/net/ipv6/conf/%s/dad_transmits", szNetworkInterfaceName);

    fp = fopen(file_to_open, "w");
    if (fp)
    {
        char szData[] = "0\n";

        RIL_LOG_INFO("DataConfigUpIpV4V6() : Opened file=[%s]\r\n", file_to_open);
        if (EOF == fputs(szData, fp))
        {
            RIL_LOG_CRITICAL("DataConfigUpIpV4V6() : ERROR: file=[%s] cannot write value [%s]\r\n", file_to_open, szData);
        }
        else
        {
            RIL_LOG_INFO("DataConfigUpIpV4V6() : Wrote [%s] to file=[%s]\r\n", CRLFExpandedString(szData, strlen(szData)).GetString(), file_to_open);
        }

        //  Close file handle
        fclose(fp);
    }
    else
    {
        RIL_LOG_CRITICAL("DataConfigUpIpV4V6() : ERROR: Cannot open [%s]\r\n", file_to_open);
    }

    //  Open accept_dad file, write 0<lf>.
    snprintf(file_to_open, 99, "/proc/sys/net/ipv6/conf/%s/accept_dad", szNetworkInterfaceName);

    fp = fopen(file_to_open, "w");
    if (fp)
    {
        char szData[] = "0\n";

        RIL_LOG_INFO("DataConfigUpIpV4V6() : Opened file=[%s]\r\n", file_to_open);
        if (EOF == fputs(szData, fp))
        {
            RIL_LOG_CRITICAL("DataConfigUpIpV4V6() : ERROR: file=[%s] cannot write value [%s]\r\n", file_to_open, szData);

        }
        else
        {
            RIL_LOG_INFO("DataConfigUpIpV4V6() : Wrote [%s] to file=[%s]\r\n", CRLFExpandedString(szData, strlen(szData)).GetString(), file_to_open);
        }

        //  Close file handle.
        fclose(fp);
    }
    else
    {
        RIL_LOG_CRITICAL("DataConfigUpIpV4V6() : ERROR: Cannot open [%s]\r\n", file_to_open);
    }

    RIL_LOG_INFO("DataConfigUpIpV4V6() : Setting flags\r\n");
    if (!setflags(s, &ifr, IFF_UP, 0))
    {
        //goto Error;
        RIL_LOG_CRITICAL("DataConfigUpIpV4V6() : Error setting flags\r\n");
    }


    if (defaultGatewayStr == NULL)
    {
        in_addr_t gw;
        struct in_addr gwaddr;
        in_addr_t addr;

        RIL_LOG_INFO("DataConfigUpIpV4V6() : set default gateway to fake value");
        if (inet_pton(AF_INET, szIpAddr, &addr) <= 0)
        {
            RIL_LOG_INFO("DataConfigUpIpV4V6() : inet_pton() failed for %s!", szIpAddr);
            goto Error;
        }

        gw = ntohl(addr) & 0xFFFFFF00;
        gw |= 1;

        gwaddr.s_addr = htonl(gw);

        defaultGatewayStr = strdup(inet_ntoa(gwaddr));
    }

    if (defaultGatewayStr != NULL)
    {
        //  Set gateway system property
        if (!PrintStringNullTerminate(szPropName, MAX_BUFFER_SIZE, "net.%s.gw", szNetworkInterfaceName))
        {
            RIL_LOG_CRITICAL("DataConfigUpIpV4V6() :cannot create szPropName net.X.gw\r\n");
        }
        else
        {
            RIL_LOG_INFO("DataConfigUpIpV4V6() - setting '%s' to '%s'\r\n", szPropName, defaultGatewayStr);
            property_set(szPropName, defaultGatewayStr);
        }
    }

    //  Set DNS1
    if (szDNS1)
    {
        if (!PrintStringNullTerminate(szPropName, MAX_BUFFER_SIZE, "net.%s.dns1", szNetworkInterfaceName))
        {
            RIL_LOG_CRITICAL("DataConfigUpIpV4V6() :cannot create szPropName net.X.dns1\r\n");
        }
        else
        {
            RIL_LOG_INFO("DataConfigUpIpV4V6() - setting '%s' to '%s'\r\n", szPropName, szDNS1);
            property_set(szPropName, szDNS1);
        }
    }

    //  Set DNS2
    if (szDNS2)
    {
        if (!PrintStringNullTerminate(szPropName, MAX_BUFFER_SIZE, "net.%s.dns2", szNetworkInterfaceName))
        {
            RIL_LOG_CRITICAL("DataConfigUpIpV4V6() :cannot create szPropName net.X.dns2\r\n");
        }
        else
        {
            RIL_LOG_INFO("DataConfigUpIpV4V6() - setting '%s' to '%s'\r\n", szPropName, szDNS2);
            property_set(szPropName, szDNS2);
        }
    }

    //  Set DNS3
    if (szIpV6DNS1)
    {
        if (!PrintStringNullTerminate(szPropName, MAX_BUFFER_SIZE, "net.%s.dns3", szNetworkInterfaceName))
        {
            RIL_LOG_CRITICAL("DataConfigUpIpV4V6() :cannot create szPropName net.X.dns3\r\n");
        }
        else
        {
            RIL_LOG_INFO("DataConfigUpIpV4V6() - setting '%s' to '%s'\r\n", szPropName, szIpV6DNS1);
            property_set(szPropName, szDNS1);
        }
    }

    //  Set DNS4
    if (szIpV6DNS2)
    {
        if (!PrintStringNullTerminate(szPropName, MAX_BUFFER_SIZE, "net.%s.dns4", szNetworkInterfaceName))
        {
            RIL_LOG_CRITICAL("DataConfigUpIpV4V6() :cannot create szPropName net.X.dns4\r\n");
        }
        else
        {
            RIL_LOG_INFO("DataConfigUpIpV4V6() - setting '%s' to '%s'\r\n", szPropName, szIpV6DNS2);
            property_set(szPropName, szDNS2);
        }
    }

    bRet = TRUE;

Error:
    if (defaultGatewayStr != NULL)
        free(defaultGatewayStr);

    if (s >= 0)
    {
        close(s);
    }

    if (s6 >= 0)
    {
        close(s6);
    }

    RIL_LOG_INFO("DataConfigUp() EXIT  bRet=[%d]\r\n", bRet);

    return bRet;
}

#endif // M2_IPV6_FEATURE_ENABLED

//
//  Call this whenever data is disconnected
//
BOOL DataConfigDown(int nCID)
{
    //  First check to see if nCID is valid
    //  This could happen if there is an existing data channel and no PDP active
    //  for that channel.
    if (nCID <= 0)
    {
        RIL_LOG_CRITICAL("DataConfigDown() - Invalid nCID = [%d]\r\n", nCID);
        return FALSE;
    }

    CRepository repository;
    BOOL bRet = FALSE;
    int s = -1;
    char szNetworkInterfaceName[MAX_BUFFER_SIZE] = {0};
    char szPropName[MAX_BUFFER_SIZE] = {0};
    CChannel_Data* pChannelData = NULL;
    struct gsm_netconfig netconfig;
    int fd=-1;
    int ret =-1;

    //  See if CID passed in is valid
    pChannelData = CChannel_Data::GetChnlFromContextID(nCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("DataConfigDown() - Invalid CID=[%d], no data channel found!\r\n", nCID);
        return FALSE;
    }

    //  Grab the network interface name
    if (!repository.Read(g_szGroupModem, g_szNetworkInterfaceNamePrefix, szNetworkInterfaceName, MAX_BUFFER_SIZE))
    {
        RIL_LOG_CRITICAL("DataConfigDown() - Could not read network interface name prefix from repository\r\n");
        strcpy(szNetworkInterfaceName, "");
        goto Error;
    }
    RIL_LOG_INFO("DataConfigDown() - ENTER  szNetworkInterfaceName prefix=[%s]  CID=[%d]\r\n", szNetworkInterfaceName, nCID);
    //  Don't forget to append the Context ID!
    if (!PrintStringNullTerminate(szNetworkInterfaceName, MAX_BUFFER_SIZE, "%s%d", szNetworkInterfaceName, nCID-1))
    {
        RIL_LOG_CRITICAL("DataConfigDown() - Could not create network interface name\r\n");
        strcpy(szNetworkInterfaceName, "");
        goto Error;
    }
    else
    {
        RIL_LOG_INFO("DataConfigDown() - ENTER  szNetworkInterfaceName=[%s]  CID=[%d]\r\n", szNetworkInterfaceName, nCID);
    }


    // Reset ContextID to 0, to free up the channel for future use
    RIL_LOG_INFO("DataConfigDown() - ****** Setting chnl=[%d] to CID=[0] ******\r\n", pChannelData->GetRilChannel());
    pChannelData->SetContextID(0);
    fd = pChannelData->GetFD();

    //  Put the channel back into AT command mode
    netconfig.adaption = 3;
    netconfig.protocol = htons(ETH_P_IP);

    if (fd >= 0)
    {
        RIL_LOG_INFO("DataConfigDown() - ***** PUTTING channel=[%d] in AT COMMAND MODE *****\r\n", pChannelData->GetRilChannel());
        ret = ioctl( fd, GSMIOC_DISABLE_NET, &netconfig );
    }




    // unset net. properties
    // TODO For multiple PDP context support this will have to be updated to be consistent with whatever changes to
    // the Android framework's use of these properties is made.

    //  Unset DNS1
    if (!PrintStringNullTerminate(szPropName, MAX_BUFFER_SIZE, "net.%s.dns1", szNetworkInterfaceName))
    {
        RIL_LOG_CRITICAL("DataConfigDown() :cannot create szPropName net.X.dns1\r\n");
    }
    else
    {
        RIL_LOG_INFO("DataConfigDown() - setting '%s' to ''\r\n", szPropName);
        property_set(szPropName, "");
    }

    //  Unset DNS2
    if (!PrintStringNullTerminate(szPropName, MAX_BUFFER_SIZE, "net.%s.dns2", szNetworkInterfaceName))
    {
        RIL_LOG_CRITICAL("DataConfigDown() :cannot create szPropName net.X.dns2\r\n");
    }
    else
    {
        RIL_LOG_INFO("DataConfigDown() - setting '%s' to ''\r\n", szPropName);
        property_set(szPropName, "");
    }

    //  Unset net.X.gw
    if (!PrintStringNullTerminate(szPropName, MAX_BUFFER_SIZE, "net.%s.gw", szNetworkInterfaceName))
    {
        RIL_LOG_CRITICAL("DataConfigDown() :cannot create szPropName net.X.gw\r\n");
    }
    else
    {
        RIL_LOG_INFO("DataConfigDown() - setting '%s' to ''\r\n", szPropName);
        property_set(szPropName, "");
    }

    bRet = TRUE;

Error:
    if (s >= 0)
    {
        close(s);
    }

    RIL_LOG_INFO("DataConfigDown() EXIT  bRet=[%d]\r\n", bRet);
    return bRet;
}

#if defined(M2_IPV6_FEATURE_ENABLED)
// Helper function to convert IP addresses to Android-readable format.
// szIpIn [IN] - The IP string to be converted
// szIpOut [OUT] - The converted IPv4 address in Android-readable format if there is an IPv4 address.
// uiIpOutSize [IN] - The size of the szIpOut buffer
// szIpOut2 [OUT] - The converted IPv6 address in Android-readable format if there is an IPv6 address.
// uiIpOutSize [IN] - The size of szIpOut2 buffer
//
// If IPv4 format a1.a2.a3.a4, then szIpIn is copied to szIpOut.
// If Ipv6 format:
// Convert a1.a2.a3.a4.a5. ... .a15.a16 to XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX IPv6 format
// output is copied to szIpOut2
// If Ipv4v6 format:
// Convert a1.a2.a3.a4.a5. ... .a19.a20 to
// a1.a2.a3.a4 to szIpOut
// XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX (a5-a20) to szIpOut2
// If szIpOut2 is NULL, then this parameter is ignored
BOOL ConvertIPAddressToAndroidReadable(char *szIpIn, char *szIpOut, UINT32 uiIpOutSize, char *szIpOut2, UINT32 uiIpOutSize2)
{
    RIL_LOG_VERBOSE("ConvertIPAddressToAndroidReadable() - Enter\r\n");
    BOOL bRet = FALSE;
    const int MAX_AIPV4_INDEX = 4;      // Number of 'a' values read from modem for IPv4 case
    const int MAX_AIPV6_INDEX = 16;     // Number of 'a' values read from modem for IPv6 case
    const int MAX_AIPV4V6_INDEX = 20;   // Number of 'a' values read from modem for IPv4v6 case

    struct in6_addr ipv6Addr;

    //  Sanity checks
    if ( (NULL == szIpIn) || (NULL == szIpOut) || (0 == uiIpOutSize))
    {
        RIL_LOG_CRITICAL("ConvertIPAddressToAndroidReadable() : ERROR: Invalid inputs!\r\n");
        return FALSE;
    }

    szIpOut[0]='\0';
    if (NULL != szIpOut2)
    {
       szIpOut2[0]='\0';
    }


    //  Count number of '.'
    int nDotCount = 0;
    for (unsigned int i=0; szIpIn[i] != '\0'; i++)
    {
        if ('.' == szIpIn[i])
        {
            //  Found a '.'
            nDotCount++;
        }
    }

    //  If 3 dots, IPv4.  If 15 dots, IPv6.  If 19 dots, then IPv4v6.
    switch(nDotCount)
    {
        case 3:
        {
            //  IPv4 format.  Just copy to szIpOut.
            strncpy(szIpOut, szIpIn, uiIpOutSize);
        }
        break;

        case 15:
        {
            //  IPv6 format.  Need to re-format this to Android IPv6 format.

            //  Extract a1...a16 into aIP.
            //  Then convert aAddress to szIpOut.
            unsigned int aIP[MAX_AIPV6_INDEX] = {0};
            unsigned char acIP[MAX_AIPV6_INDEX] = {0};
            if (EOF == sscanf(szIpIn, "%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u",
                            &aIP[0], &aIP[1], &aIP[2], &aIP[3], &aIP[4], &aIP[5], &aIP[6], &aIP[7],
                            &aIP[8], &aIP[9], &aIP[10], &aIP[11], &aIP[12], &aIP[13], &aIP[14], &aIP[15]))
            {
                RIL_LOG_CRITICAL("ConvertIPAddressToAndroidReadable() - ERROR: cannot sscanf into aIP[]! ipv6\r\n");
                goto Error;
            }

            //  Loop through array, check values from modem is from 0-255.
            for (int i=0; i<MAX_AIPV6_INDEX; i++)
            {
                if (aIP[i] > 255)
                {
                    //  Value is not between 0-255.
                    RIL_LOG_CRITICAL("ConvertIPAddressToAndroidReadable() - ERROR: ipv6 aIP[%d] not in range 0-255. val=%u\r\n", i, aIP[i]);
                    goto Error;
                }
            }

            //  Convert unsigned int to unsigned char (for inet_ntop)
            //  The value read in should be in range 0-255.
            for (int i=0; i<MAX_AIPV6_INDEX; i++)
            {
                acIP[i] = (unsigned char)(aIP[i]);
            }

            if (NULL != szIpOut2 && 0 != uiIpOutSize2)
            {

                if (inet_ntop(AF_INET6, (void *)acIP, szIpOut2, uiIpOutSize2) <= 0)
                {
                    RIL_LOG_CRITICAL("ConvertIPAddressToAndroidReadable() - ERROR: cannot inet_ntop ipv6\r\n");
                    goto Error;
                }
            }

        }
        break;

        case 19:
        {
            //  IPv4v6 format.  Grab a1.a2.a3.a4 and that's the IPv4 part.  The rest is IPv6.
            //  Extract a1...a20 into aIP.
            //  Then IPv4 part is extracted into szIpOut.
            //  IPV6 part is extracted into szIpOut2.
            unsigned int aIP[MAX_AIPV4V6_INDEX] = {0};
            if (EOF == sscanf(szIpIn, "%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u",
                            &aIP[0], &aIP[1], &aIP[2], &aIP[3],
                            &aIP[4], &aIP[5], &aIP[6], &aIP[7], &aIP[8], &aIP[9], &aIP[10], &aIP[11],
                            &aIP[12], &aIP[13], &aIP[14], &aIP[15], &aIP[16], &aIP[17], &aIP[18], &aIP[19]
                            ))
            {
                RIL_LOG_CRITICAL("ConvertIPAddressToAndroidReadable() - ERROR: cannot sscanf into aIP[]! ipv4v6\r\n");
                goto Error;
            }

            //  Loop through array, check values from modem is from 0-255.
            for (int i=0; i<MAX_AIPV4V6_INDEX; i++)
            {
                if (aIP[i] > 255)
                {
                    //  Value is not between 0-255.
                    RIL_LOG_CRITICAL("ConvertIPAddressToAndroidReadable() - ERROR: ipv4v6 aIP[%d] not in range 0-255. val=%u\r\n", i, aIP[i]);
                    goto Error;
                }
            }

            if (snprintf(szIpOut, uiIpOutSize,
                    "%u.%u.%u.%u",
                    aIP[0], aIP[1], aIP[2], aIP[3]) <= 0)
            {
                RIL_LOG_CRITICAL("ConvertIPAddressToAndroidReadable() - ERROR: error with snprintf()! ipv4v6 v4 part\r\n");
                goto Error;
            }

            if (NULL != szIpOut2 && 0 != uiIpOutSize2)
            {
                unsigned char acIP[MAX_AIPV6_INDEX] = {0};

                //  Convert unsigned int to unsigned char (for inet_ntop)
                //  The value read in should be in range 0-255, from check done above.
                for (int i=0; i<MAX_AIPV6_INDEX; i++)
                {
                    acIP[i] = (unsigned char)(aIP[i+4]);
                }

                if (inet_ntop(AF_INET6, (void *)acIP, szIpOut2, uiIpOutSize2) <= 0)
                {
                    RIL_LOG_CRITICAL("ConvertIPAddressToAndroidReadable() - ERROR: cannot inet_ntop ipv4v6\r\n");
                    goto Error;
                }
            }
        }
        break;

        default:
            RIL_LOG_CRITICAL("ConvertIPAddressToAndroidReadable() - ERROR: Unknown address format nDotCount=[%d]\r\n", nDotCount);
            goto Error;
    }

    bRet = TRUE;

Error:
    RIL_LOG_VERBOSE("ConvertIPAddressToAndroidReadable() - Exit\r\n");
    return bRet;
}
#endif // M2_IPV6_FEATURE_ENABLED

//
// Response to AT+CGPADDR=<CID>
//
RIL_RESULT_CODE CTE_INF_6260::ParseIpAddress(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseIpAddress() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    const char* szRsp = rRspData.szResponse;
    UINT32 nCid;
    UINT32  cbIpAddr = 0;
    CChannel_Data* pChannelData = NULL;

    // Parse prefix
    if (!FindAndSkipString(szRsp, "+CGPADDR: ", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - ERROR: Unable to parse \"+CGPADDR\" prefix.!\r\n");
        goto Error;
    }

    // Parse <cid>
    if (!ExtractUInt32(szRsp, nCid, szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - ERROR: Unable to parse <cid>!\r\n");
        goto Error;
    }

    if (nCid > 0)
    {
        pChannelData = CChannel_Data::GetChnlFromContextID(nCid);
        if (!pChannelData)
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - ERROR: Could not get Data Channel for Context ID %d.\r\n", nCid);
            goto Error;
        }


#if defined(M2_IPV6_FEATURE_ENABLED)
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
            //  Then converted address is in pChannelData->m_szIpAddr.
            const int MAX_IPADDR_SIZE = 100;
            char szPdpAddr[MAX_IPADDR_SIZE] = {0};

            //  Extract ,<Pdp_Addr1>
            if (!SkipString(szRsp, ",", szRsp) ||
                !ExtractQuotedString(szRsp, szPdpAddr, MAX_IPADDR_SIZE, szRsp))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - ERROR: Unable to parse <PDP_addr1>!\r\n");
                goto Error;
            }

            if (pChannelData->m_szIpAddr == NULL)
            {
                pChannelData->m_szIpAddr = new char[MAX_IPADDR_SIZE];
            }

            if (pChannelData->m_szIpAddr2 == NULL)
            {
                pChannelData->m_szIpAddr2 = new char[MAX_IPADDR_SIZE];
            }
            //initialisation is made by ConvertIPAddressToAndroidReadable


            //  The AT+CGPADDR command doesn't return IPV4V6 format
            if (!ConvertIPAddressToAndroidReadable(szPdpAddr, pChannelData->m_szIpAddr, MAX_IPADDR_SIZE, pChannelData->m_szIpAddr2, MAX_IPADDR_SIZE))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - ERROR: ConvertIPAddressToAndroidReadable failed!\r\n");
                goto Error;
            }

            //  Extract ,<PDP_Addr2>
            //  Converted address is in pChannelData->m_szIpAddr2.
            if (SkipString(szRsp, ",", szRsp))
            {
                if (!ExtractQuotedString(szRsp, szPdpAddr, MAX_IPADDR_SIZE, szRsp))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - ERROR: Unable to parse <PDP_addr2>!\r\n");
                    goto Error;
                }


                //  The AT+CGPADDR command doesn't return IPV4V6 format.
                if (!ConvertIPAddressToAndroidReadable(szPdpAddr, pChannelData->m_szIpAddr, MAX_IPADDR_SIZE, pChannelData->m_szIpAddr2, MAX_IPADDR_SIZE))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - ERROR: ConvertIPAddressToAndroidReadable failed! m_szIpAddr2\r\n");
                    goto Error;
                }

            RIL_LOG_INFO("CTE_INF_6260::ParseIpAddress() - IPV4 address: %s\r\n", pChannelData->m_szIpAddr);
            RIL_LOG_INFO("CTE_INF_6260::ParseIpAddress() - IPV6 address: %s\r\n", pChannelData->m_szIpAddr2);
            }
        }
#else // M2_IPV6_FEATURE_ENABLED
        //  Remove previous IP addr (if it existed) and set it to NULL to confirm the memory unset memory will be allocated by ExtractQuotedStringWithAllocatedMemory
        delete[] pChannelData->m_szIpAddr;
        pChannelData->m_szIpAddr = NULL;

        // Take original IPV4 string and copy it into data channel's m_szIpAddr.
        // Parse <PDP_addr>
        if (!SkipString(szRsp, ",", szRsp) ||
            !ExtractQuotedStringWithAllocatedMemory(szRsp, pChannelData->m_szIpAddr, cbIpAddr, szRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - ERROR: Unable to parse <PDP_addr>!\r\n");
            goto Error;
        }

        RIL_LOG_INFO("CTE_INF_6260::ParseIpAddress() - IP address: %s\r\n", pChannelData->m_szIpAddr);
#endif // M2_IPV6_FEATURE_ENABLED

        res = RRIL_RESULT_OK;
    }
    else
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - nCid=[%d] not valid!\r\n", nCid);
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
    UINT32 nCid = 0, nXDNSCid = 0;
    UINT32  cbDns1 = 0;
    UINT32  cbDns2 = 0;
    CChannel_Data* pChannelData = NULL;

    //  Get Context ID from context data (passed in from ParseSetupDataCall)
    nCid = (UINT32)rRspData.pContextData;
    RIL_LOG_INFO("CTE_INF_6260::ParseDns() - looking for cid=[%d]\r\n", nCid);

    // Parse "+XDNS: "
    while (FindAndSkipString(szRsp, "+XDNS: ", szRsp))
    {
        // Parse <cid>
        if (!ExtractUInt32(szRsp, nXDNSCid, szRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - ERROR: Unable to parse <cid>!\r\n");
            goto Error;
        }

        if (nXDNSCid == nCid)
        {
            RIL_LOG_INFO("CTE_INF_6260::ParseDns() - Found match! nCid=[%d]\r\n", nXDNSCid);
            pChannelData = CChannel_Data::GetChnlFromContextID(nCid);
            if (!pChannelData)
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - ERROR: Could not get Data Channel for Context ID %d.\r\n", nXDNSCid);
                goto Error;
            }

            //  Remove previous DNS1 and DNS2 (if it existed)
            delete[] pChannelData->m_szDNS1;
            pChannelData->m_szDNS1 = NULL;
            delete[] pChannelData->m_szDNS2;
            pChannelData->m_szDNS2 = NULL;

#if defined(M2_IPV6_FEATURE_ENABLED)
            {
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
                //  Then converted address is in pChannelData->m_szDNS1, pChannelData->m_szIpV6DNS1 (if IPv4v6).
                const int MAX_IPADDR_SIZE = 100;
                char szDNS[MAX_IPADDR_SIZE] = {0};

                // Parse <primary DNS>
                // Converted address is in m_szDNS1, m_szIpV6DNS1 (if necessary)
                if (SkipString(szRsp, ",", szRsp))
                {
                    if (!ExtractQuotedString(szRsp, szDNS, MAX_IPADDR_SIZE, szRsp))
                    {
                        RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - ERROR: Unable to extact szDNS 1!\r\n");
                        goto Error;
                    }

                    if (pChannelData->m_szDNS1== NULL)
                    {
                        pChannelData->m_szDNS1 = new char[MAX_IPADDR_SIZE];
                    }

                    //initialisation is made by ConvertIPAddressToAndroidReadable
                    if (NULL == pChannelData->m_szDNS1)
                    {
                        RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - ERROR: cannot allocate m_szDNS1!\r\n");
                        goto Error;
                    }
                    if (pChannelData->m_szIpV6DNS1== NULL)
                    {
                        pChannelData->m_szIpV6DNS1 = new char[MAX_IPADDR_SIZE];
                    }
                    //initialisation is made by ConvertIPAddressToAndroidReadable
                    if (NULL == pChannelData->m_szIpV6DNS1)
                    {
                        RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - ERROR: cannot allocate m_szIpV6DNS1!\r\n");
                        goto Error;
                    }


                    //  Now convert to Android-readable format (and split IPv4v6 parts (if applicable)
                    if (!ConvertIPAddressToAndroidReadable(szDNS, pChannelData->m_szDNS1, MAX_IPADDR_SIZE,
                            pChannelData->m_szIpV6DNS1, MAX_IPADDR_SIZE))
                    {
                        RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - ERROR: ConvertIPAddressToAndroidReadable failed! m_szDNS1\r\n");
                        goto Error;
                    }
                    RIL_LOG_INFO("CTE_INF_6260::ParseDns() - DNS1: %s\r\n", pChannelData->m_szDNS1);

                    if (strlen(pChannelData->m_szIpV6DNS1) > 0)
                    {
                        RIL_LOG_INFO("CTE_INF_6260::ParseDns() - DNS1_2: %s\r\n", pChannelData->m_szIpV6DNS1);
                    }
                    else
                    {
                        RIL_LOG_INFO("CTE_INF_6260::ParseDns() - m_szIpV6DNS1: <NONE>\r\n");
                    }
                }
                //initialisation is made by ConvertIPAddressToAndroidReadable
                if (pChannelData->m_szIpV6DNS2== NULL)
                {
                    pChannelData->m_szIpV6DNS2 = new char[MAX_IPADDR_SIZE];
                }
                if (pChannelData->m_szDNS2== NULL)
                {
                    pChannelData->m_szDNS2 = new char[MAX_IPADDR_SIZE];
                }

                // Parse <secondary DNS>
                // Converted address is in m_szDNS2, m_szIpV6DNS2 (if necessary)
                if (SkipString(szRsp, ",", szRsp))
                {
                    if (!ExtractQuotedString(szRsp, szDNS, MAX_IPADDR_SIZE, szRsp))
                    {
                        RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - ERROR: Unable to extact szDNS 2!\r\n");
                        goto Error;
                    }


                    if (NULL == pChannelData->m_szDNS2)
                    {
                        RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - ERROR: cannot allocate m_szDNS2!\r\n");
                        goto Error;
                    }

                    if (NULL == pChannelData->m_szIpV6DNS2)
                    {
                        RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - ERROR: cannot allocate m_szIpV6DNS2!\r\n");
                        goto Error;
                    }


                    //  Now convert to Android-readable format (and split IPv4v6 parts (if applicable)
                    if (!ConvertIPAddressToAndroidReadable(szDNS, pChannelData->m_szDNS2, MAX_IPADDR_SIZE,
                            pChannelData->m_szIpV6DNS2, MAX_IPADDR_SIZE))
                    {
                        RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - ERROR: ConvertIPAddressToAndroidReadable failed! m_szDNS2\r\n");
                        goto Error;
                    }
                    RIL_LOG_INFO("CTE_INF_6260::ParseDns() - DNS2: %s\r\n", pChannelData->m_szDNS2);

                    if (strlen(pChannelData->m_szIpV6DNS2) > 0)
                    {
                        RIL_LOG_INFO("CTE_INF_6260::ParseDns() - DNS2_2: %s\r\n", pChannelData->m_szIpV6DNS2);
                    }
                    else
                    {
                        RIL_LOG_INFO("CTE_INF_6260::ParseDns() - szIpV6DNS2: <NONE>\r\n");
                    }
                }
            }
#else // M2_IPV6_FEATURE_ENABLED
            // Parse <primary DNS>
            if (SkipString(szRsp, ",", szRsp))
            {
                if (!ExtractQuotedStringWithAllocatedMemory(szRsp, pChannelData->m_szDNS1, cbDns1, szRsp))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - ERROR: Unable to parse <primary DNS>! szDns1\r\n");
                    goto Error;
                }
                RIL_LOG_INFO("CTE_INF_6260::ParseDns() - DNS1: %s\r\n", pChannelData->m_szDNS1);
            }


            // Parse <secondary DNS>
            if (SkipString(szRsp, ",", szRsp))
            {
                if (!ExtractQuotedStringWithAllocatedMemory(szRsp, pChannelData->m_szDNS2, cbDns2, szRsp))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - ERROR: Unable to parse <secondary DNS>! szDns2\r\n");
                    goto Error;
                }
                RIL_LOG_INFO("CTE_INF_6260::ParseDns() - DNS2: %s\r\n", pChannelData->m_szDNS2);
            }
#endif // M2_IPV6_FEATURE_ENABLED

            res = RRIL_RESULT_OK;
        }
    }

Error:
    if (RRIL_RESULT_ERROR == res)
    {
        //  We didn't parse correctly or didn't find context ID in this response
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - ERROR: Didn't find context ID=[%d] in response\r\n", nCid);
    }

    // Signal completion of setting up data
    if (pChannelData)
        CEvent::Signal(pChannelData->m_pSetupDoneEvent);

    RIL_LOG_VERBOSE("CTE_INF_6260::ParseDns() - Exit\r\n");
    return res;
}

//
//  Response to AT+CPIN2?
//
RIL_RESULT_CODE CTE_INF_6260::ParseQueryPIN2(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseQueryPIN2() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* szRsp = rRspData.szResponse;


    // Parse "+CPIN2: "
    if (!FindAndSkipString(szRsp, "+CPIN2: ", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseQueryPIN2() - ERROR: Unable to parse \"+CPIN2\" prefix.!\r\n");
        goto Error;
    }

    //  Extract <code>
    if (!ExtractUnquotedString(szRsp, g_cTerminator, m_szCPIN2Result, MAX_BUFFER_SIZE, szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseQueryPIN2() - ERROR: Unable to parse \"+CPIN2\" prefix.!\r\n");
        goto Error;
    }


    res = RRIL_RESULT_OK;
Error:
    if (m_pQueryPIN2Event)
    {
        CEvent::Signal(m_pQueryPIN2Event);
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::ParseQueryPIN2() - Exit\r\n");
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
        RIL_LOG_CRITICAL("ParseUSIMRecordStatus - Error: Input parameters\r\n");
        goto Error;
    }

    // Need at least 2 bytes for response data FCP (file control parameters)
    if (2 > cbResponse)
    {
        RIL_LOG_CRITICAL("ParseUSIMRecordStatus - Error: Need at least 2 bytes for response data\r\n");
        goto Error;
    }

    // Validate this response is a 3GPP 102 221 SELECT response.
    tlvFcp.Parse(sResponse, cbResponse);
    if (FCP_TAG != tlvFcp.GetTag())
    {
        RIL_LOG_CRITICAL("ParseUSIMRecordStatus - Error: First tag is not FCP.  Tag=[0x%02X]\r\n", tlvFcp.GetTag());
        goto Error;
    }

    if (cbResponse != tlvFcp.GetTotalSize())
    {
        RIL_LOG_CRITICAL("ParseUSIMRecordStatus - Error: cbResponse=[%d] not equal to total size=[%d]\r\n", cbResponse, tlvFcp.GetTotalSize());
        goto Error;
    }
    pbFcpData = tlvFcp.GetValue();
    cbFcpDataSize = tlvFcp.GetLength();

    // Retrieve the File Descriptor data object
    tlvFileDescriptor.Parse(pbFcpData, cbFcpDataSize);
    if (FILE_DESCRIPTOR_TAG != tlvFileDescriptor.GetTag())
    {
        RIL_LOG_CRITICAL("ParseUSIMRecordStatus - Error: File descriptor tag is not FCP.  Tag=[0x%02X]\r\n", tlvFileDescriptor.GetTag());
        goto Error;
    }

    cbDataUsed = tlvFileDescriptor.GetTotalSize();
    if (cbDataUsed > cbFcpDataSize)
    {
        RIL_LOG_CRITICAL("ParseUSIMRecordStatus - Error: cbDataUsed=[%d] is greater than cbFcpDataSize=[%d]\r\n", cbDataUsed, cbFcpDataSize);
        goto Error;
    }

    pbFileDescData = tlvFileDescriptor.GetValue();
    cbFileDescDataSize = tlvFileDescriptor.GetLength();
    // File descriptors should only be 2 or 5 bytes long.
    if((2 != cbFileDescDataSize) && (5 != cbFileDescDataSize))
    {
        RIL_LOG_CRITICAL("ParseUSIMRecordStatus - Error: File descriptor can only be 2 or 5 bytes.  cbFileDescDataSize=[%d]\r\n", cbFileDescDataSize);
        goto Error;

    }
    if (2 > cbFileDescDataSize)
    {
        RIL_LOG_CRITICAL("ParseUSIMRecordStatus - Error: File descriptor less than 2 bytes.  cbFileDescDataSize=[%d]\r\n", cbFileDescDataSize);
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
                RIL_LOG_CRITICAL("ParseUSIMRecordStatus - Error: File descriptor less than 5 bytes.  cbFileDescDataSize=[%d]\r\n", cbFileDescDataSize);
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
                    RIL_LOG_CRITICAL("ParseUSIMRecordStatus - Error: Couldn't parse TRANSPARENT\r\n");
                    goto Error;
                }

                cbDataUsed += tlvCurrent.GetTotalSize();

                if (FILE_SIZE_TAG == tlvCurrent.GetTag())
                {
                    const UINT8* pbFileSize = NULL;

                    if (2 > tlvCurrent.GetLength())
                    {
                        RIL_LOG_CRITICAL("ParseUSIMRecordStatus - Error: TRANSPARENT length must be at least 2\r\n");
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
            RIL_LOG_CRITICAL("ParseUSIMRecordStatus - Error: UNKNOWN tag not equal to FILE_ID_TAG\r\n");
            goto Error;
        }

        if (2 != tlvFileId.GetLength())
        {
            RIL_LOG_CRITICAL("ParseUSIMRecordStatus - Error: UNKNOWN length not equal to 2\r\n");
            goto Error;
        }

        pbFileId = tlvFileId.GetValue();
        uFileId = (pbFileId[0] << 4) + pbFileId[1];

        if (MASTER_FILE_ID != uFileId)
        {
            RIL_LOG_CRITICAL("ParseUSIMRecordStatus - Error: UNKNOWN file ID not equal to MASTER_FILE_ID\r\n");
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


//
// RIL_REQUEST_SIM_IO 28
//
RIL_RESULT_CODE CTE_INF_6260::CoreSimIo(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSimIo() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_SIM_IO_v6 *   pSimIOArgs = NULL;
    char szGraphicsPath[] = "3F007F105F50";  // substitute actual path instead of string "img"
    char szImg[] = "img";
    char *pszPath = NULL;

    int Efsms = 28476;
    int nReadCmd = 178;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (sizeof(RIL_SIM_IO_v6) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: Invalid data size. Given %d bytes\r\n", uiDataSize);
        goto Error;
    }

    // extract data
    pSimIOArgs = (RIL_SIM_IO_v6 *)pData;

    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSimIo() - command=[0x%08x]  [%d]\r\n", pSimIOArgs->command, pSimIOArgs->command);
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSimIo() - fileid=[0x%08x]  [%d]\r\n", pSimIOArgs->fileid, pSimIOArgs->fileid);
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSimIo() - path=[%s]\r\n", (pSimIOArgs->path ? pSimIOArgs->path : "NULL") );
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSimIo() - p1=[0x%08x]  [%d]\r\n", pSimIOArgs->p1, pSimIOArgs->p1);
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSimIo() - p2=[0x%08x]  [%d]\r\n", pSimIOArgs->p2, pSimIOArgs->p2);
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSimIo() - p3=[0x%08x]  [%d]\r\n", pSimIOArgs->p3, pSimIOArgs->p3);
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSimIo() - data=[%s]\r\n", (pSimIOArgs->data ? pSimIOArgs->data : "NULL") );


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

    //  If PIN2 is required, send out AT+CPIN2 request
    if (pSimIOArgs->pin2)
    {
        RIL_LOG_INFO("CTE_INF_6260::CoreSimIo() - PIN2 required\r\n");

        CCommand *pCmd1 = NULL;
        char szCmd1[MAX_BUFFER_SIZE] = {0};
        UINT32 uiWaitRes = 0;
        BOOL bEnterPIN2 = FALSE;
        BOOL bPIN2Ready = FALSE;

        CEvent::Reset(m_pQueryPIN2Event);

        if (!CopyStringNullTerminate(szCmd1,
                     "AT+CPIN2?\r",
                     sizeof(szCmd1)))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: Cannot CopyStringNullTerminate CPIN2?\r\n");
            goto Error;
        }

        pCmd1 = new CCommand(g_arChannelMapping[ND_REQ_ID_QUERYPIN2], NULL, ND_REQ_ID_QUERYPIN2, szCmd1, &CTE::ParseQueryPIN2);
        if (pCmd1)
        {
            if (!CCommand::AddCmdToQueue(pCmd1))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: Unable to queue AT+CPIN2 command!\r\n");
                delete pCmd1;
                pCmd1 = NULL;
                goto Error;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: Unable to allocate memory for new AT+CPIN2 command!\r\n");
            goto Error;
        }


        //  Wait here for response
        uiWaitRes = CEvent::Wait(m_pQueryPIN2Event, g_TimeoutAPIDefault);
        switch (uiWaitRes)
        {
            case WAIT_EVENT_0_SIGNALED:
                RIL_LOG_INFO("CTE_INF_6260::CoreSimIo() : CPIN2 event signalled\r\n");
                if (0 == strcmp("READY", m_szCPIN2Result))
                {
                    //  Found READY, send AT+CPWD="P2","<PIN2>","<PIN2>" to validate PIN2
                    bEnterPIN2 = TRUE;
                    bPIN2Ready = TRUE;
                }
                else if (0 == strcmp("SIM PIN2", m_szCPIN2Result))
                {
                    //  Found SIM PIN2, enter CPIN2
                    bEnterPIN2 = TRUE;
                }
                else if (0 == strcmp("SIM PUK2", m_szCPIN2Result))
                {
                    //  Found SIM PUK2, return PUK2 error
                    res = RIL_E_SIM_PUK2;
                    goto Error;
                }
                else
                {
                    //  Found something unrecognized
                    goto Error;
                }
                break;

            case WAIT_TIMEDOUT:
                 RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - Timed out waiting for CPIN2 result!! \r\n");
                 goto Error;

            default:
                 RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - Unexpected event result on Wait for CPIN2, res: %d\r\n", uiWaitRes);
                 goto Error;
        }

        if (bEnterPIN2)
        {
            if (!bPIN2Ready)
            {
                if (!PrintStringNullTerminate(rReqData.szCmd1,
                    sizeof(rReqData.szCmd1),
                    "AT+CPIN2=\"%s\"\r",
                    pSimIOArgs->pin2))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: cannot create CPIN2 command\r\n");
                    goto Error;
                }
            }
            else
            {
                if (!PrintStringNullTerminate(rReqData.szCmd1,
                    sizeof(rReqData.szCmd1),
                    "AT+CPWD=\"P2\",\"%s\",\"%s\"\r",
                    pSimIOArgs->pin2,
                    pSimIOArgs->pin2))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: cannot create CPWD command\r\n");
                    goto Error;
                }
            }

            if (NULL == pSimIOArgs->data)
            {
                if(NULL == pSimIOArgs->path)
                {
                    if (!PrintStringNullTerminate(rReqData.szCmd2,
                        sizeof(rReqData.szCmd2),
                        "AT+CRSM=%d,%d,%d,%d,%d\r",
                        pSimIOArgs->command,
                        pSimIOArgs->fileid,
                        pSimIOArgs->p1,
                        pSimIOArgs->p2,
                        pSimIOArgs->p3))
                    {
                        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: cannot create CRSM command 1\r\n");
                        goto Error;
                    }
                }
                else
                {
                    if (!PrintStringNullTerminate(rReqData.szCmd2,
                                  sizeof(rReqData.szCmd2),
                                  "AT+CRSM=%d,%d,%d,%d,%d,,\"%s\"\r",
                                  pSimIOArgs->command,
                                  pSimIOArgs->fileid,
                                  pSimIOArgs->p1,
                                  pSimIOArgs->p2,
                                  pSimIOArgs->p3,
                                  pszPath))
                    {
                        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: cannot create CRSM command 2\r\n");
                        goto Error;
                    }
                }
            }
            else
            {
                if(NULL == pSimIOArgs->path)
                {
                    if (!PrintStringNullTerminate(rReqData.szCmd2,
                        sizeof(rReqData.szCmd2),
                        "AT+CRSM=%d,%d,%d,%d,%d,\"%s\"\r",
                        pSimIOArgs->command,
                        pSimIOArgs->fileid,
                        pSimIOArgs->p1,
                        pSimIOArgs->p2,
                        pSimIOArgs->p3,
                        pSimIOArgs->data))
                    {
                        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: cannot create CRSM command 3\r\n");
                        goto Error;
                    }
                }
                else
                {
                    if (!PrintStringNullTerminate(rReqData.szCmd2,
                        sizeof(rReqData.szCmd2),
                        "AT+CRSM=%d,%d,%d,%d,%d,\"%s\",\"%s\"\r",
                        pSimIOArgs->command,
                        pSimIOArgs->fileid,
                        pSimIOArgs->p1,
                        pSimIOArgs->p2,
                        pSimIOArgs->p3,
                        pSimIOArgs->data,
                        pszPath))
                    {
                        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: cannot create CRSM command 4\r\n");
                        goto Error;
                    }
                }
            }

        }
        else
        {
            //  Didn't have to enter PIN2
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
                        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: cannot create CRSM command 5\r\n");
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
                        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: cannot create CRSM command 6\r\n");
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
                        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: cannot create CRSM command 7\r\n");
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
                        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: cannot create CRSM command 8\r\n");
                        goto Error;
                    }
                }
            }

        }

    }
    else
    {
        //  No PIN2

        if (NULL == pSimIOArgs->data)
        {

            if(NULL == pSimIOArgs->path)
            {
                if(pSimIOArgs->command == nReadCmd && pSimIOArgs->fileid == Efsms)
                {
                     RIL_LOG_VERBOSE("CTEBase::CoreSimIo() - Create CRSM command 1 for reading sms\r\n");

                     if (!PrintStringNullTerminate(rReqData.szCmd1,
                         sizeof(rReqData.szCmd1),
                         "AT+CRSM=%d,%d,%d,%d,%d;+CMGR=%d\r",
                         pSimIOArgs->command,
                         pSimIOArgs->fileid,
                         pSimIOArgs->p1,
                         pSimIOArgs->p2,
                         pSimIOArgs->p3,
                         pSimIOArgs->p1))
                     {
                         RIL_LOG_CRITICAL("CTEBase::CoreSimIo() - ERROR: cannot create CRSM command 2 for reading sms\r\n");
                         goto Error;
                     }

                 }
                 else
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
                         RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: cannot create CRSM command 9\r\n");
                         goto Error;
                     }
                 }
            }
            else
            {
                 if(pSimIOArgs->command == nReadCmd && pSimIOArgs->fileid == Efsms)
                 {
                     RIL_LOG_VERBOSE("CTEBase::CoreSimIo() - Create CRSM command 1 for reading sms\r\n");

                     if (!PrintStringNullTerminate(rReqData.szCmd1,
                         sizeof(rReqData.szCmd1),
                         "AT+CRSM=%d,%d,%d,%d,%d,,\"%s\";+CMGR=%d\r",
                         pSimIOArgs->command,
                         pSimIOArgs->fileid,
                         pSimIOArgs->p1,
                         pSimIOArgs->p2,
                         pSimIOArgs->p3,
                         pszPath,
                         pSimIOArgs->p1))
                     {
                         RIL_LOG_CRITICAL("CTEBase::CoreSimIo() - ERROR: cannot create CRSM command 2 for reading sms\r\n");
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
                         RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: cannot create CRSM command 10\r\n");
                         goto Error;
                    }

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
                    RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: cannot create CRSM command 11\r\n");
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
                    RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: cannot create CRSM command 12\r\n");
                    goto Error;
                }
            }
        }
    }

    //  Set the context of this command to the SIM_IO command-type.
    rReqData.pContextData = (void*)pSimIOArgs->command;


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
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - ERROR: Response String pointer is NULL.\r\n");
        goto Error;
    }

    // Parse "<prefix>+CRSM: <sw1>,<sw2>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - ERROR: Could not skip over response prefix.\r\n");
        goto Error;
    }

    if (!SkipString(pszRsp, "+CRSM: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - ERROR: Could not skip over \"+CRSM: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt32(pszRsp, uiSW1, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - ERROR: Could not extract SW1 value.\r\n");
        goto Error;
    }

    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt32(pszRsp, uiSW2, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - ERROR: Could not extract SW2 value.\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_INF_6260::ParseSimIo() - Extracted SW1 = %u and SW2 = %u\r\n", uiSW1, uiSW2);

    // Parse ","
    if (SkipString(pszRsp, ",", pszRsp))
    {
        // Parse <response>
        // NOTE: we take ownership of allocated szResponseString
        if (!ExtractQuotedStringWithAllocatedMemory(pszRsp, szResponseString, cbResponseString, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - ERROR: Could not extract data string.\r\n");
            goto Error;
        }
        else
        {
            RIL_LOG_INFO("CTE_INF_6260::ParseSimIo() - Extracted data string: \"%s\" (%u chars including NULL)\r\n", szResponseString, cbResponseString);
        }

        if (0 != (cbResponseString - 1) % 2)
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() : ERROR : String was not a multiple of 2.\r\n");
            goto Error;
        }

        // Check for USIM GET_RESPONSE response
        int nCRSMCommand = (int)rRspData.pContextData;
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
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - ERROR: Cannot create new string!\r\n");
                goto Error;
            }
            memset(sNewString, 0, cbNewString);

            UINT32 cbUsed = 0;
            if (!GSMHexToGSM(szResponseString, cbResponseString, sNewString, cbNewString, cbUsed))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - ERROR: Cannot cconvert szResponseString to GSMHex.\r\n");
                delete[] sNewString;
                sNewString = NULL;
                cbNewString = 0;
                goto Error;
            }

            //  OK, now parse!
            if (!ParseUSIMRecordStatus((UINT8*)sNewString, cbNewString, &sUSIM))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - ERROR: Cannot parse USIM record status\r\n");
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
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - ERROR: Cannot create new szResponsestring!\r\n");
                delete[] sNewString;
                sNewString = NULL;
                goto Error;
            }
            if (!CopyStringNullTerminate(szResponseString, "000000000000000000000000000000", cbResponseString))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - ERROR: Cannot CopyStringNullTerminate szResponsestring!\r\n");
                delete[] sNewString;
                sNewString = NULL;
                goto Error;
            }

            //  Extract info, put into new response string
            if (!PrintStringNullTerminate(szTemp, 5, "%04X", sUSIM.dwTotalSize))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - ERROR: Cannot PrintStringNullTerminate sUSIM.dwTotalSize!\r\n");
                delete[] sNewString;
                sNewString = NULL;
                goto Error;
            }
            memcpy(&szResponseString[4], szTemp, 4);

            if (!PrintStringNullTerminate(szTemp, 3, "%02X", sUSIM.dwRecordSize))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - ERROR: Cannot PrintStringNullTerminate sUSIM.dwRecordSize!\r\n");
                delete[] sNewString;
                sNewString = NULL;
                goto Error;
            }
            memcpy(&szResponseString[28], szTemp, 2);

            if (RIL_SIMRECORDTYPE_UNKNOWN == sUSIM.dwRecordType)
            {
                //  bad parse.
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - ERROR: sUSIM.dwRecordType is unknown!\r\n");
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
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - ERROR: Could not allocate memory for a RIL_SIM_IO_Response struct.\r\n");
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
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - ERROR: Cannot CopyStringNullTerminate szResponseString\r\n");
            goto Error;
        }

        // Ensure NULL termination!
        pResponse->simResponse[cbResponseString] = '\0';
    }

    // Parse "<postfix>"
    if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
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
    int nCid = 0;

    if (uiDataSize < (1 * sizeof(char *)))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreDeactivateDataCall() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreDeactivateDataCall() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_INF_6260::CoreDeactivateDataCall() - uiDataSize=[%d]\r\n", uiDataSize);

    pszCid = ((char**)pData)[0];
    if (pszCid == NULL || '\0' == pszCid[0])
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreDeactivateDataCall() - ERROR: pszCid was NULL\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_INF_6260::CoreDeactivateDataCall() - pszCid=[%s]\r\n", pszCid);

    if ( (RIL_VERSION >= 4) && (uiDataSize >= (2 * sizeof(char *))) )
    {
        pszReason = ((char**)pData)[1];
        if (pszReason == NULL || '\0' == pszReason[0])
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::CoreDeactivateDataCall() - ERROR: pszReason was NULL\r\n");
            goto Error;
        }
        RIL_LOG_INFO("CTE_INF_6260::CoreDeactivateDataCall() - pszReason=[%s]\r\n", pszReason);
    }


    //  May 18,2011 - Don't call AT+CGACT=0,X if SIM was removed since context is already deactivated.
    if (RADIO_STATE_SIM_LOCKED_OR_ABSENT == g_RadioState.GetRadioState())
    {
        RIL_LOG_INFO("CTE_INF_6260::CoreDeactivateDataCall() - SIM LOCKED OR ABSENT!! no-op this command\r\n");
        rReqData.szCmd1[0] = '\0';
        res = RRIL_RESULT_OK;
        rReqData.pContextData = (void*)((int)0);
    }
    else
    {
        //  Get CID as int.
        if (sscanf(pszCid, "%d", &nCid) == EOF)
        {
            // Error
            RIL_LOG_CRITICAL("CTE_INF_6260::CoreDeactivateDataCall() - ERROR: cannot convert %s to int\r\n", pszCid);
            goto Error;
        }

        if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CGACT=0,%s\r", pszCid))
        {
            res = RRIL_RESULT_OK;
        }

        //  Set the context of this command to the CID (for multiple context support).
        rReqData.pContextData = (void*)nCid;  // Store this as an int.
    }

Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreDeactivateDataCall() - Exit\r\n");
    return res;
}


RIL_RESULT_CODE CTE_INF_6260::ParseDeactivateDataCall(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_INF_6260::ParseDeactivateDataCall() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    //  Set CID to 0 for this data channel
    UINT32 nCID = 0;
    nCID = (UINT32)rRspData.pContextData;

    if (nCID > 0)
    {
        CChannel_Data* pChannelData = NULL;
        pChannelData = CChannel_Data::GetChnlFromContextID(nCID);
        if (pChannelData)
        {
            RIL_LOG_INFO("CTE_INF_6260::ParseDeactivateDataCall() - Calling DataConfigDown  chnl=[%d], cid=[%d]\r\n", pChannelData->GetRilChannel(), nCID);
        }
        if (!DataConfigDown(nCID))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseDeactivateDataCall() - ERROR : Couldn't DataConfigDown chnl=[%d] nCID=[%d]\r\n", rRspData.uiChannel, nCID);
        }
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_INFO("CTE_INF_6260::ParseDeactivateDataCall() - Exit\r\n");
    return res;
}


//
// RIL_REQUEST_OEM_HOOK_RAW 59
//
RIL_RESULT_CODE CTE_INF_6260::CoreHookRaw(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
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

    BYTE bCommand = 0;

    //  uiDataSize MUST be 1 or greater.
    if (0 == uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreHookRaw() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreHookRaw() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }


    bCommand = (BYTE)pDataBytes[0];  //  This is the command.
    rReqData.pContextData = (void*)(UINT32)bCommand;

    switch(bCommand)
    {
        case RIL_OEM_HOOK_RAW_TRIGGER_FAST_DORMANCY:
        {
            RIL_LOG_INFO("TE_INF_6260::CoreHookRaw() - RIL_OEM_HOOK_RAW_TRIGGER_FAST_DORMANCY Command=[0x%02X] received OK\r\n", (unsigned char)bCommand);

            //  Shouldn't be any data following command
            if (sizeof(sOEM_HOOK_RAW_TRIGGER_FAST_DORMANCY) == uiDataSize)
            {
                if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XFDOR=1\r", sizeof(rReqData.szCmd1)))
                {
                    RIL_LOG_CRITICAL("TE_INF_6260::CoreHookRaw() - ERROR: RIL_OEM_HOOK_RAW_TRIGGER_FAST_DORMANCY - Can't construct szCmd1.\r\n");
                    goto Error;
                }
            }
            else
            {
                RIL_LOG_CRITICAL("TE_INF_6260::CoreHookRaw() : ERROR : uiDataSize=%d not sOEM_HOOK_RAW_TRIGGER_FAST_DORMANCY=%d\r\n", uiDataSize, sizeof(sOEM_HOOK_RAW_TRIGGER_FAST_DORMANCY));
                goto Error;
            }

        }
        break;

        case RIL_OEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER:
        {
            RIL_LOG_INFO("TE_INF_6260::CoreHookRaw() - RIL_OEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER Command=[0x%02X] received OK\r\n", (unsigned char)bCommand);

            //  Cast our data into our structure
            if (sizeof(sOEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER) == uiDataSize)
            {
                sOEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER sFDT;
                memset(&sFDT, 0, sizeof(sOEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER));
                memcpy(&sFDT, pDataBytes, sizeof(sOEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER));

                RIL_LOG_INFO("TE_INF_6260::CoreHookRaw() - bCommand=[0x%02X]\r\n", (unsigned char)sFDT.bCommand);
                RIL_LOG_INFO("TE_INF_6260::CoreHookRaw() - nTimerValue=[%d]\r\n", (int)sFDT.nTimerValue);


                if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XFDORT=%u\r", sFDT.nTimerValue))
                {
                    RIL_LOG_CRITICAL("TE_INF_6260::CoreHookRaw() - ERROR: RIL_OEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER - Can't construct szCmd1.\r\n");
                    goto Error;
                }

            }
            else
            {
                RIL_LOG_CRITICAL("TE_INF_6260::CoreHookRaw() : ERROR : uiDataSize=%d not sOEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER=%d\r\n", uiDataSize, sizeof(sOEM_HOOK_RAW_SET_FAST_DORMANCY_TIMER));
                goto Error;
            }

        }
        break;


        default:
            RIL_LOG_CRITICAL("TE_INF_6260::CoreHookRaw() - ERROR: Received unknown command=[0x%02X]\r\n", bCommand);
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

    UINT32 uCommand = (UINT32)rRspData.pContextData;

    switch(uCommand)
    {
        default:
            break;
    }

    RIL_LOG_INFO("CTE_INF_6260::ParseHookRaw() - Exit\r\n");
    return RRIL_RESULT_OK;
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
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetBandMode() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetBandMode() - ERROR: Passed data pointer was NULL\r\n");
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
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetBandMode() - ERROR: Japan region is not supported!\r\n");
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
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetBandMode() - ERROR: Undefined region code: %d\r\n", *pnBandMode);
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
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseQueryAvailableBandMode() - ERROR: Could not skip \"+XBANDSEL: \".\r\n");
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
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseQueryAvailableBandMode() - ERROR: Cannot have a count of zero.\r\n");
        goto Error;
    }

    //  Alloc array and don't forget about the first entry for size.
    pModes = (int*)malloc( (1 + count) * sizeof(int));
    if (NULL == pModes)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseQueryAvailableBandMode() - ERROR: Could not allocate memory for response.\r\n");
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
        RIL_LOG_INFO("CTE_INF_6260::ParseStkGetProfile() - ERROR: Response string is NULL!\r\n");
        goto Error;
    }

    pszTermProfile = (char*)malloc(MAX_BUFFER_SIZE);
    if (NULL == pszTermProfile)
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseStkGetProfile() - ERROR: Could not allocate memory for a %u-char string.\r\n", MAX_BUFFER_SIZE);
        goto Error;
    }

    memset(pszTermProfile, 0x00, MAX_BUFFER_SIZE);

    // Parse "<prefix>+STKPROF: <length>,<data><postfix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseStkGetProfile() - ERROR: Could not skip response prefix.\r\n");
        goto Error;
    }

    if (!SkipString(pszRsp, "+STKPROF: ", pszRsp))
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseStkGetProfile() - ERROR: Could not skip \"+STKPROF: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt32(pszRsp, uiLength, pszRsp))
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseStkGetProfile() - ERROR: Could not extract length value.\r\n");
        goto Error;
    }

    if (SkipString(pszRsp, ",", pszRsp))
    {
        if (!ExtractQuotedString(pszRsp, pszTermProfile, MAX_BUFFER_SIZE, pszRsp))
        {
            RIL_LOG_INFO("CTE_INF_6260::ParseStkGetProfile() - ERROR: Could not extract the terminal profile.\r\n");
            goto Error;
        }
    }

    if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseStkGetProfile() - ERROR: Could not extract the response end.\r\n");
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
        RIL_LOG_INFO("CTE_INF_6260::CoreStkSetProfile() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_INFO("CTE_INF_6260::CoreStkSetProfile() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    // extract data
    pszTermProfile = (char*)pData;

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+STKPROF=%u,\"%s\"\r", uiDataSize, pszTermProfile))
    {
        RIL_LOG_INFO("CTE_INF_6260::CoreStkSetProfile() - ERROR: Could not form string.\r\n");
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
        RIL_LOG_INFO("CTE_INF_6260::CoreStkSendEnvelopeCommand() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_INFO("CTE_INF_6260::CoreStkSendEnvelopeCommand() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    // extract data
    pszEnvCommand = (char*)pData;

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+SATE=\"%s\"\r", pszEnvCommand))
    {
        RIL_LOG_INFO("CTE_INF_6260::CoreStkSendEnvelopeCommand() - ERROR: Could not form string.\r\n");
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
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - ERROR: Response string is NULL!\r\n");
        goto Error;
    }

    // Parse "<prefix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - ERROR: Could not skip over response prefix.\r\n");
        goto Error;
    }

    // Parse "+SATE: "
    if (!SkipString(pszRsp, "+SATE: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - ERROR: Could not skip over \"+SATE: \".\r\n");
        goto Error;
    }

    // Parse "<sw1>"
    if (!ExtractUInt32(pszRsp, uiSw1, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - ERROR: Could not extract sw1.\r\n");
        goto Error;
    }

    // Parse ",<sw2>"
    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt32(pszRsp, uiSw2, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - ERROR: Could not extract sw2.\r\n");
        goto Error;
    }

    // Parse ",<event_type>"
    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt32(pszRsp, uiEventType, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - ERROR: Could not extract event type.\r\n");
        goto Error;
    }

    // Parse ",<envelope_type>"
    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt32(pszRsp, uiEnvelopeType, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - ERROR: Could not extract envelope type.\r\n");
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
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - ERROR: Could not alloc mem for command.\r\n");
            goto Error;
        }

        memset(pszRespData, 0x00, MAX_BUFFER_SIZE);

        // Parse ",<response_data>"
        if (!ExtractUnquotedString(pszRsp, g_cTerminator, pszRespData, MAX_BUFFER_SIZE, pszRsp))
        {
            RIL_LOG_INFO("CTE_INF_6260::ParseStkSendEnvelopeCommand() - ERROR: Could not parse response data.\r\n");
            goto Error;
        }

        RIL_LOG_INFO("CTE_INF_6260::ParseStkSendEnvelopeCommand() - response data: \"%s\".\r\n", pszRespData);
    }
    else
    {
       pszRespData = NULL;
    }

    // Parse "<postfix>"
    if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - ERROR: Could not extract the response end.\r\n");
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
        RIL_LOG_INFO("CTE_INF_6260::CoreStkSendTerminalResponse() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != sizeof(char *))
    {
        RIL_LOG_INFO("CTE_INF_6260::CoreStkSendTerminalResponse() - ERROR: Invalid data size.\r\n");
        goto Error;
    }

    RIL_LOG_INFO(" uiDataSize= %d\r\n", uiDataSize);

    pszTermResponse = (char *)pData;

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+SATR=\"%s\"\r", pszTermResponse))
    {
        RIL_LOG_INFO("CTE_INF_6260::CoreStkSendTerminalResponse() - ERROR: Could not form string.\r\n");
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
        RIL_LOG_INFO("CTE_INF_6260::CoreStkHandleCallSetupRequestedFromSim() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != sizeof(int *))
    {
        RIL_LOG_INFO("CTE_INF_6260::CoreStkHandleCallSetupRequestedFromSim() - ERROR: Invalid data size.\r\n");
        goto Error;
    }

    nConfirmation = ((int *)pData)[0];
    if (0 == nConfirmation)
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+SATD=0\r"))
        {
            RIL_LOG_INFO("CTE_INF_6260::CoreStkHandleCallSetupRequestedFromSim() - ERROR: Could not form string.\r\n");
            goto Error;
        }
    }
    else
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+SATD=1\r"))
        {
            RIL_LOG_INFO("CTE_INF_6260::CoreStkHandleCallSetupRequestedFromSim() - ERROR: Could not form string.\r\n");
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
    int nNetworkType = 0;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetPreferredNetworkType() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != sizeof(int *))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetPreferredNetworkType() - ERROR: Invalid data size.\r\n");
        goto Error;
    }

    nNetworkType = ((int *)pData)[0];

    // if network type already set, NO-OP this command
    if (m_nCurrentNetworkType == nNetworkType)
    {
        rReqData.szCmd1[0] = '\0';
        res = RRIL_RESULT_OK;
        RIL_LOG_INFO("CTE_INF_6260::CoreSetPreferredNetworkType() - Network type {%d} already set.\r\n", nNetworkType);
        goto Error;
    }

    switch (nNetworkType)
    {
        case PREF_NET_TYPE_GSM_WCDMA: // WCDMA Preferred

            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XRAT=1,2\r", sizeof(rReqData.szCmd1) ))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetPreferredNetworkType() - ERROR: Can't construct szCmd1 nNetworkType=%d\r\n", nNetworkType);
                goto Error;
            }

           break;

        case PREF_NET_TYPE_GSM_ONLY: // GSM Only

            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XRAT=0\r", sizeof(rReqData.szCmd1) ))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetPreferredNetworkType() - ERROR: Can't construct szCmd1 nNetworkType=%d\r\n", nNetworkType);
                goto Error;
            }

            break;

        case PREF_NET_TYPE_WCDMA: // WCDMA Only

            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XRAT=2\r", sizeof(rReqData.szCmd1) ))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetPreferredNetworkType() - ERROR: Can't construct szCmd1 nNetworkType=%d\r\n", nNetworkType);
                goto Error;
            }

            break;

        default:
            RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetPreferredNetworkType() - ERROR: Undefined rat code: %d\r\n", nNetworkType);
            res = RIL_E_GENERIC_FAILURE;
            goto Error;
            break;
    }

    //  Set the context of this command to the network type we're attempting to set
    rReqData.pContextData = (void*)nNetworkType;  // Store this as an int.


    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSetPreferredNetworkType() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseSetPreferredNetworkType(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseSetPreferredNetworkType() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    int nNetworkType = (int)rRspData.pContextData;
    m_nCurrentNetworkType = nNetworkType;

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
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetPreferredNetworkType() - ERROR: Could not allocate memory for response.\r\n");
        goto Error;
    }

    // Skip "<prefix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetPreferredNetworkType() - ERROR: Could not skip response prefix.\r\n");
        goto Error;
    }

    // Skip "+XRAT: "
    if (!SkipString(pszRsp, "+XRAT: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetPreferredNetworkType() - ERROR: Could not skip \"+XRAT: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt32(pszRsp, rat, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetPreferredNetworkType() - ERROR: Could not extract rat value.\r\n");
        goto Error;
    }

    if (FindAndSkipString(pszRsp, ",", pszRsp))
    {
        if (!ExtractUInt32(pszRsp, pref, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetPreferredNetworkType() - ERROR: Could not find and skip pref value even though it was expected.\r\n");
            goto Error;
        }
    }

    switch(rat)
    {
        case 0:     // GSM Only
        {
            pRat[0] = PREF_NET_TYPE_GSM_ONLY;
            m_nCurrentNetworkType = PREF_NET_TYPE_GSM_ONLY;
            break;
        }

        case 1:     // WCDMA Preferred
        {
            pRat[0] = PREF_NET_TYPE_GSM_WCDMA;
            m_nCurrentNetworkType = PREF_NET_TYPE_GSM_WCDMA;
            break;
        }

        case 2:     // WCDMA only
        {
            pRat[0] = PREF_NET_TYPE_WCDMA;
            m_nCurrentNetworkType = PREF_NET_TYPE_WCDMA;
            break;
        }

        default:
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetPreferredNetworkType() - ERROR: Unexpected rat found: %d. Failing out.\r\n", rat);
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
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: Could not allocate memory for a S_ND_N_CELL_DATA struct.\r\n");
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
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: Exceeded max count = %d\r\n", RRIL_MAX_CELL_ID_COUNT);
            goto Error;
        }

        //  Get <mode>
        if (!ExtractUInt32(pszRsp, nMode, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: cannot extract <mode>\r\n");
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
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: mode 0, cannot skip to LAC and CI\r\n");
                    goto Error;
                }

                //  Read <LAC> and <CI>
                if (!ExtractUInt32(pszRsp, nLAC, pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: mode 0, could not extract LAC\r\n");
                    goto Error;
                }
                //  Read <CI>
                if ((!SkipString(pszRsp, ",", pszRsp)) ||
                    (!ExtractUInt32(pszRsp, nCI, pszRsp)))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: mode 0, could not extract CI value\r\n");
                    goto Error;
                }
                //  Read <RxLev>
                if ((!SkipString(pszRsp, ",", pszRsp)) ||
                    (!ExtractUInt32(pszRsp, nRSSI, pszRsp)))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: mode 0, could not extract RSSI value\r\n");
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
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: mode 1, could not extract LAC\r\n");
                    goto Error;
                }
                //  Read <CI>
                if ((!SkipString(pszRsp, ",", pszRsp)) ||
                    (!ExtractUInt32(pszRsp, nCI, pszRsp)))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: mode 1, could not extract CI value\r\n");
                    goto Error;
                }
                //  Read <RxLev>
                if ((!SkipString(pszRsp, ",", pszRsp)) ||
                    (!ExtractUInt32(pszRsp, nRSSI, pszRsp)))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: mode 1, could not extract RSSI value\r\n");
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
                if (!ExtractUnquotedString(pszRsp, g_cTerminator, szBuf, MAX_BUFFER_SIZE, szDummy))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: mode 2, could not extract temp buf\r\n");
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
                        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: mode 2, could not skip to scrambling code\r\n");
                        goto Error;
                    }
                    if (!ExtractUInt32(pszRsp, nScramblingCode, pszRsp))
                    {
                        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: mode 2, could not extract scrambling code\r\n");
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
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: mode %d, could not extract scrambling code\r\n", nMode);
                    goto Error;
                }

                //  <rscp> is parameter 5
                if (!FindAndSkipString(pszRsp, ",", pszRsp) ||
                    !FindAndSkipString(pszRsp, ",", pszRsp) ||
                    !FindAndSkipString(pszRsp, ",", pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: mode %d, could not skip to rscp\r\n", nMode);
                    goto Error;
                }
                //  read <rscp>
                if (!ExtractUInt32(pszRsp, nRSSI, pszRsp))
                {
                    RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: mode %d, could not extract rscp\r\n", nMode);
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
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: Invalid nMode=[%d]\r\n", nMode);
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
        RIL_LOG_INFO("CTE_INF_6260::CoreSetTtyMode() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_INFO("CTE_INF_6260::CoreSetTtyMode() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }

    // extract the data
    nTtyMode = ((int*)pData)[0];

    RIL_LOG_INFO(" Set TTY mode: %d\r\n", nTtyMode);

    // check for invalid value
    if ((nTtyMode < 0) || (nTtyMode > 3))
    {
        RIL_LOG_INFO("CTE_INF_6260::CoreSetTtyMode() - ERROR: Undefined CTM/TTY mode: %d\r\n", nTtyMode);
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
        RIL_LOG_INFO("CTE_INF_6260::CoreSetTtyMode() - ERROR: Could not form string.\r\n");
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
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseQueryTtyMode() - ERROR: Could not allocate memory for response.\r\n");
        goto Error;
    }

    // Parse prefix
    if (!FindAndSkipString(szRsp, "+XCTMS: ", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseQueryTtyMode() - ERROR: Unable to parse \"CTM/TTY mode: \" prefix.!\r\n");
        goto Error;
    }

    // Parse <mode>
    if (!ExtractUInt32(szRsp, uiTtyMode, szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseQueryTtyMode() - ERROR: Unable to parse <mode>!\r\n");
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
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreReportSmsMemoryStatus() - ERROR: Data pointer is NULL\r\n");
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


