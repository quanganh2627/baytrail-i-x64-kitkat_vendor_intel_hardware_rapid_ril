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
// Author:  Francesc J. Vilarino Guell
// Created: 2009-12-11
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date         Who      Ver   Description
//  ----------  -------  ----  -----------------------------------------------
//  Sept 11/12   FV      1.00  Established v1.00 based on current code base.
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
#include "channel_data.h"
#include "atchannel.h"
#include "stk.h"
#include "rildmain.h"
#include "callbacks.h"
#include "oemhookids.h"
#include "repository.h"
#include "../../../../IFX-modem/gsmmux.h"
#include <cutils/properties.h>
#include <sys/system_properties.h>

//  This is for close().
#include <unistd.h>

//  This is for socket-related calls.
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/route.h>
#include <linux/if_ether.h> // Pranav

extern "C" char *hex_to_stk_at(const char *at);

CTE_INF_6260::CTE_INF_6260()
: m_nCurrentNetworkType(0),
m_pQueryPIN2Event(NULL)
{
    strcpy(m_szNetworkInterfaceName, "");
    strcpy(m_szCPIN2Result, "");

    CRepository repository;

    //  Grab the network interface name
    if (!repository.Read(g_szGroupModem, g_szNetworkInterfaceName, m_szNetworkInterfaceName, MAX_BUFFER_SIZE))
    {
        RIL_LOG_CRITICAL("CCTE_INF_6260::CTE_INF_6260() - Could not read network interface name from repository\r\n");
        strcpy(m_szNetworkInterfaceName, "");
    }
    else
    {
        RIL_LOG_INFO("CTE_INF_6260::CTE_INF_6260() - m_szNetworkInterfaceName=[%s]\r\n", m_szNetworkInterfaceName);
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

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CPIN?;+XUICC?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTE_INF_6260::CoreGetSimStatus() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_6260::ParseGetSimStatus(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseGetSimStatus() - Enter\r\n");

    UINT32 nValue;

    const char * pszRsp = rRspData.szResponse;
    RIL_CardStatus* pCardStatus = NULL;
    RIL_RESULT_CODE res = CTEBase::ParseSimPin(pszRsp, pCardStatus);
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetSimStatus() - ERROR: Could not parse Sim Pin.\r\n");
        goto Error;
    }

    if (pCardStatus->card_state != RIL_CARDSTATE_ABSENT)
    {
        // Parse "<prefix>+XUICC: <state><postfix>"
        if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetSimStatus() - ERROR: Could not skip response prefix.\r\n");
            goto Error;
        }

        if (SkipString(pszRsp, "+XUICC: ", pszRsp))
        {
            if (!ExtractUpperBoundedUInt(pszRsp, 2, nValue, pszRsp))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetSimStatus() - ERROR: Invalid SIM type.\r\n");
                goto Error;
            }

            if (1 == nValue)
            {
                RIL_LOG_INFO("CTE_INF_6260::ParseGetSimStatus() - SIM type = %d  detected USIM\r\n", nValue);

                //  Grab setting from repository
                CRepository repository;
                int nUseUSIMAddress = 0;

                //  Grab the network interface name
                if (!repository.Read(g_szGroupRILSettings, g_szUseUSIMAddress, nUseUSIMAddress))
                {
                    RIL_LOG_CRITICAL("CCTE_INF_6260::ParseGetSimStatus() - Could not read UseUSIMAddress from repository\r\n");
                }
                else
                {
                    RIL_LOG_INFO("CTE_INF_6260::ParseGetSimStatus() - nUseUSIMAddress=[%d]\r\n", nUseUSIMAddress);
                }

                if (nUseUSIMAddress >= 1)
                {
                    //  Set to USIM
                    pCardStatus->applications[0].app_type = RIL_APPTYPE_USIM;
                }
            }
            else if (0 == nValue)
            {
                RIL_LOG_INFO("CTE_INF_6260::ParseGetSimStatus() - SIM type = %d  detected normal SIM\r\n", nValue);
            }
        }
    }

    res = RRIL_RESULT_OK;

    rRspData.pData   = (void*)pCardStatus;
    rRspData.uiDataSize  = sizeof(RIL_CardStatus);

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

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetupDataCall() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (7 * sizeof(char*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetupDataCall() - ERROR: Invalid data size. Was given %d bytes\r\n", uiDataSize);
        goto Error;
    }


    // extract data
    stPdpData.szRadioTechnology = ((char **)pData)[0];  // not used
    stPdpData.szRILDataProfile  = ((char **)pData)[1];  // not used
    stPdpData.szApn             = ((char **)pData)[2];
    stPdpData.szUserName        = ((char **)pData)[3];  // not used
    stPdpData.szPassword        = ((char **)pData)[4];  // not used
    stPdpData.szPAPCHAP         = ((char **)pData)[5];  // not used

    // For setting up data call we need to send 2 sets of chained commands: AT+CGDCONT to define PDP Context,
    // with +CGQREQ to specify requested QoS and +CGACT to activate PDP Context; then
    // if RAW IP is used send AT+CGDATA to enable Raw IP on data channel (which will then switch the channel to data mode).
    //
    (void)PrintStringNullTerminate(rReqData.szCmd1,
        sizeof(rReqData.szCmd1),
        "AT+CGDCONT=%d,\"IP\",\"%s\",,0,0;+CGQREQ=%d;+CGQMIN=%d;+XDNS=%d,1;+CGACT=0,%d\r", uiCID,
        stPdpData.szApn, uiCID, uiCID, uiCID, uiCID);

#ifndef RIL_USE_PPP
   (void)PrintStringNullTerminate(rReqData.szCmd2, sizeof(rReqData.szCmd2), "AT+CGDATA=\"M-RAW_IP\",%d\r", uiCID);
#endif //!RIL_USE_PPP

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

#ifdef RIL_USE_PPP
    char szOperstate[PROPERTY_VALUE_MAX];
    BOOL fPPPUp = FALSE;
    const unsigned int nSleep = 2000;
    const int nRetries = 15;
    property_set("ctl.start", "pppd_gprs");

    for (int i = 0; i < nRetries; i++)
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseSetupDataCall() - INFO: Waiting %d seconds for ppp connection. (Attempt #%d/%d)\r\n",
                    nSleep/1000, i + 1, nRetries);
        Sleep(nSleep);

        property_get("net.gprs.operstate", szOperstate, "");
        RIL_LOG_INFO("CTE_INF_6260::ParseSetupDataCall() - INFO: net.grps.operstate=[%s]\r\n", szOperstate);

        if (0 == strcmp(szOperstate, "up"))
        {
            RIL_LOG_INFO("CTE_INF_6260::ParseSetupDataCall() - INFO: ppp connection is up!\r\n");
            fPPPUp = TRUE;
            break;
        }
        else
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: ppp connection is not up\r\n");
        }
    }

    if (fPPPUp)
    {
        Sleep(nSleep);
        property_get("net.grps.local-ip", szIP, "");
        RIL_LOG_INFO("CTE_INF_6260::ParseSetupDataCall() - INFO: net.grps.local-ip=[%s]\r\n", szIP);
    }

#else

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
    BYTE szCmd[MAX_BUFFER_SIZE];
    CChannel_Data* pDataChannel = NULL;
    int nCID = 0;
    int fd = -1;
    int ret = 0;
    CRepository repository;

    // 1st confirm we got "CONNECT"
    const BYTE* szRsp = rRspData.szResponse;

    if (!FindAndSkipString(szRsp, "CONNECT", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: Did not get \"CONNECT\" response.\r\n");
        goto Error;
    }

    pDataChannel = CChannel_Data::GetChnlFromRilChannelNumber(rRspData.uiChannel);
    if (!pDataChannel)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: Could not get Data Channel for RIL channel number %d.\r\n", rRspData.uiChannel);
        goto Error;
    }

    //  Set CID
    nCID = (int)rRspData.pContextData;
    RIL_LOG_INFO("CTE_INF_6260::ParseSetupDataCall() - Setting chnl=[%d] to CID=[%d]\r\n", rRspData.uiChannel, nCID);
    pDataChannel->SetContextID(nCID);

// Following code-block is moved up here from the end of this function to get if_name needed for netconfig (N_GSM)
// But the IP address is filled in end of function.
    pDataCallRsp = (P_ND_SETUP_DATA_CALL)malloc(sizeof(S_ND_SETUP_DATA_CALL));
    sprintf(pDataCallRsp->szCID, "%d", pDataChannel->GetContextID());
    strcpy(pDataCallRsp->szNetworkInterfaceName, m_szNetworkInterfaceName);
    //strcpy(pDataCallRsp->szIPAddress, szIP);
    pDataCallRsp->sSetupDataCallPointers.pszCID = pDataCallRsp->szCID;
    pDataCallRsp->sSetupDataCallPointers.pszNetworkInterfaceName = pDataCallRsp->szNetworkInterfaceName;
    //pDataCallRsp->sSetupDataCallPointers.pszIPAddress = pDataCallRsp->szIPAddress;
    //rRspData.pData = (void*)pDataCallRsp;
    //rRspData.uiDataSize = sizeof(S_ND_SETUP_DATA_CALL_POINTERS);

// N_GSM related code
    netconfig.adaption = 3;
    netconfig.protocol = htons(ETH_P_IP);
    strncpy(netconfig.if_name, pDataCallRsp->szNetworkInterfaceName, IFNAMSIZ);
// Add IF NAME
    fd = pDataChannel->GetFD();
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

    CEvent::Reset(pDataChannel->m_pSetupDoneEvent);

    (void)PrintStringNullTerminate(szCmd, MAX_BUFFER_SIZE, "AT+CGPADDR=%d\r", pDataChannel->GetContextID());

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


    uiWaitRes = CEvent::Wait(pDataChannel->m_pSetupDoneEvent, nTotalTimeout);
    switch (uiWaitRes)
    {
        case WAIT_EVENT_0_SIGNALED:
            RIL_LOG_INFO("CTE_INF_6260::ParseSetupDataCall() : SetupData event signalled\r\n");
            if (NULL == pDataChannel->m_szIpAddr)
            {
                 RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: IP address is null\r\n");
                 goto Error;
            }
            if (NULL == pDataChannel->m_szDNS1)
            {
                 RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: DNS1 is null\r\n");
                 goto Error;
            }
            if (NULL == pDataChannel->m_szDNS2)
            {
                 RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - ERROR: DNS2 is null\r\n");
                 goto Error;
            }
            strcpy(szIP, pDataChannel->m_szIpAddr);
            break;

        case WAIT_TIMEDOUT:
             RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - Timed out waiting for IP Address and DNS\r\n");
             goto Error;

        default:
             RIL_LOG_CRITICAL("CTE_INF_6260::ParseSetupDataCall() - Unexpected event result on Wait for IP Address and DNS, res: %d\r\n", uiWaitRes);
             goto Error;
    }

#endif //RIL_USE_PPP

    // invoke netcfg commands
    if (!DataConfigUp(pDataChannel->m_szIpAddr, pDataChannel->m_szDNS1, pDataChannel->m_szDNS2))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - ERROR: Unable to set ifconfig\r\n");
        goto Error;
    }


//     pDataCallRsp = (P_ND_SETUP_DATA_CALL)malloc(sizeof(S_ND_SETUP_DATA_CALL));
//     sprintf(pDataCallRsp->szCID, "%d", pDataChannel->GetContextID());
//     strcpy(pDataCallRsp->szNetworkInterfaceName, m_szNetworkInterfaceName);
     strcpy(pDataCallRsp->szIPAddress, szIP);
//     pDataCallRsp->sSetupDataCallPointers.pszCID = pDataCallRsp->szCID;
//     pDataCallRsp->sSetupDataCallPointers.pszNetworkInterfaceName = pDataCallRsp->szNetworkInterfaceName;
     pDataCallRsp->sSetupDataCallPointers.pszIPAddress = pDataCallRsp->szIPAddress;
     rRspData.pData = (void*)pDataCallRsp;
     rRspData.uiDataSize = sizeof(S_ND_SETUP_DATA_CALL_POINTERS);
    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseSetupDataCall() - Error cleanup\r\n");
        if (pDataChannel)
        {
            RIL_LOG_INFO("CTE_INF_6260::ParseSetupDataCall() - calling DataConfigDown(%d)\r\n", pDataChannel->GetContextID());

            //  Release network interface
            DataConfigDown(pDataChannel->GetContextID());
        }
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

BOOL setaddr(int s, struct ifreq *ifr, const char *addr)
{
    init_sockaddr_in((struct sockaddr_in *) &ifr->ifr_addr, addr);
    RIL_LOG_INFO("setaddr - calling SIOCSIFADDR\r\n");
    if(ioctl(s, SIOCSIFADDR, ifr) < 0)
    {
        RIL_LOG_CRITICAL("setaddr : ERROR: SIOCSIFADDR\r\n");
        return FALSE;
    }
    return TRUE;
}

BOOL setflags(int s, struct ifreq *ifr, int set, int clr)
{
    RIL_LOG_INFO("setflags - calling SIOCGIFFLAGS\r\n");
    if(ioctl(s, SIOCGIFFLAGS, ifr) < 0)
    {
        RIL_LOG_CRITICAL("setflags : ERROR: SIOCGIFFLAGS\r\n");
        return FALSE;
    }

    ifr->ifr_flags = (ifr->ifr_flags & (~clr)) | set;
    RIL_LOG_INFO("setflags - calling SIOCGIFFLAGS 2\r\n");
    if(ioctl(s, SIOCSIFFLAGS, ifr) < 0)
    {
        RIL_LOG_CRITICAL("setflags: ERROR: SIOCSIFFLAGS 2\r\n");
        return FALSE;
    }
    return TRUE;
}

BOOL setnetmask(int s, struct ifreq *ifr, const char *addr)
{
    init_sockaddr_in((struct sockaddr_in *) &ifr->ifr_netmask, addr);
    RIL_LOG_INFO("setnetmask - calling SIOCSIFNETMASK\r\n");
    if(ioctl(s, SIOCSIFNETMASK, ifr) < 0)
    {
        RIL_LOG_CRITICAL("setnetmask: ERROR: SIOCSIFNETMASK\r\n");
        return FALSE;
    }
    return TRUE;
}


static BOOL setmtu(int s, struct ifreq *ifr, int mtu)
{
    ifr->ifr_mtu = mtu;
    RIL_LOG_INFO("setmtu - calling SIOCSIFMTU\r\n");
    if(ioctl(s, SIOCSIFMTU, ifr) < 0)
    {
        RIL_LOG_CRITICAL("setmtu: ERROR: SIOCSIFMTU\r\n");
        return FALSE;
    }
    return TRUE;
}


BOOL setroute(int s, struct rtentry *rt, const char *addr)
{
    init_sockaddr_in((struct sockaddr_in *) &rt->rt_gateway, addr);
    RIL_LOG_INFO("setroute - calling SIOCADDRT\r\n");
    if(ioctl(s, SIOCADDRT, rt) < 0)
    {
        RIL_LOG_CRITICAL("setroute: ERROR: SIOCADDRT\r\n");
        return FALSE;
    }
    return TRUE;
}

//
//  Call this function whenever data is activated
//
BOOL DataConfigUp(char *szIpAddr, char *szDNS1, char *szDNS2)
{
    CRepository repository;
    BOOL bRet = FALSE;
    int s = -1;
    char szNetworkInterfaceName[MAX_BUFFER_SIZE] = {0};
    char szPropName[MAX_BUFFER_SIZE] = {0};
    char *defaultGatewayStr = NULL;

    //  Grab the network interface name
    if (!repository.Read(g_szGroupModem, g_szNetworkInterfaceName, szNetworkInterfaceName, MAX_BUFFER_SIZE))
    {
        RIL_LOG_CRITICAL("DataConfigUp() - Could not read network interface name from repository\r\n");
        strcpy(szNetworkInterfaceName, "");
        goto Error;
    }

    RIL_LOG_INFO("DataConfigUp() ENTER  szNetworkInterfaceName=[%s]  szIpAddr=[%s]\r\n", szNetworkInterfaceName, szIpAddr);
    RIL_LOG_INFO("DataConfigUp() ENTER  szDNS1=[%s]  szDNS2=[%s]\r\n", szDNS1, szDNS2);



    // set net. properties
    // TODO For multiple PDP context support this will have to be updated to be consistent with whatever changes to
    // the Android framework's use of these properties is made. (Currently there is only one property for the
    // IP address and there needs to be one for each context.
    RIL_LOG_INFO("DataConfigUp() : Setting 'net.interfaces.defaultroute' to 'gprs'\r\n");
    property_set("net.interfaces.defaultroute", "gprs");

    RIL_LOG_INFO("DataConfigUp() : Setting 'net.gprs.local-ip' to '%s'\r\n", szIpAddr);
    property_set("net.gprs.local-ip", szIpAddr);

    RIL_LOG_INFO("DataConfigUp() : Setting 'net.gprs.operstate' to 'up'\r\n");
    property_set("net.gprs.operstate", "up");


    //  Open socket for ifconfig and route commands
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        RIL_LOG_CRITICAL("DataConfigUp() : cannot open control socket\n");
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

        RIL_LOG_INFO("DataConfigUp() : Setting addr\r\n");
        if (!setaddr(s, &ifr, szIpAddr)) // ipaddr
        {
            //goto Error;
            RIL_LOG_CRITICAL("DataConfigUp() : Error setting add\r\n");
        }

        RIL_LOG_INFO("DataConfigUp() : Setting flags\r\n");
        if (!setflags(s, &ifr, IFF_UP, 0))
        {
            //goto Error;
            RIL_LOG_CRITICAL("DataConfigUp() : Error setting flags\r\n");
        }

        RIL_LOG_INFO("DataConfigUp() : Setting netmask\r\n");
        if (!setnetmask(s, &ifr, "255.255.255.0"))  // the netmask
        {
            //goto Error;
            RIL_LOG_CRITICAL("DataConfigUp() : Error setting netmask\r\n");
        }

        RIL_LOG_INFO("DataConfigUp() : Setting mtu\r\n");
        if (!setmtu(s, &ifr, 1460))
        {
            //goto Error;
            RIL_LOG_CRITICAL("DataConfigUp() : Error setting mtu\r\n");
        }
    }



   // TODO retrieve gateway from the modem with XDNS?

    if (defaultGatewayStr == NULL) {
        in_addr_t gw;
        struct in_addr gwaddr;
    in_addr_t addr;

        RIL_LOG_INFO("set default gateway to fake value");
        if (inet_pton(AF_INET, szIpAddr, &addr) <= 0) {
            RIL_LOG_INFO("inet_pton() failed for %s!", szIpAddr);
            goto Error;
        }
        gw = ntohl(addr) & 0xFFFFFF00;
        gw |= 1;

        gwaddr.s_addr = htonl(gw);

        defaultGatewayStr = strdup(inet_ntoa(gwaddr));
}

    if (defaultGatewayStr != NULL) {
        struct rtentry rt;
        memset(&rt, 0, sizeof(struct rtentry));

        rt.rt_dst.sa_family = AF_INET;
        rt.rt_genmask.sa_family = AF_INET;
        rt.rt_gateway.sa_family = AF_INET;


        rt.rt_flags = RTF_UP | RTF_GATEWAY;
        rt.rt_dev = szNetworkInterfaceName;

        if (!setroute(s, &rt, defaultGatewayStr)) // defaultGatewayStr
        {
            //goto Error;
            RIL_LOG_CRITICAL("DataConfigUp() : Error setting gateway\r\n");
        }

        PrintStringNullTerminate(szPropName, MAX_BUFFER_SIZE, "net.%s.gw", szNetworkInterfaceName);
        RIL_LOG_INFO("DataConfigUp() - setting '%s' to '%s'\r\n", szPropName, defaultGatewayStr);
        property_set(szPropName, defaultGatewayStr);
    }

    /*This property does not exist so cannot be modified.
    //  setprop gsm.net.interface ifx02
    RIL_LOG_INFO("DataConfigUp() - setting 'gsm.net.interface' to '%s'\r\n", szNetworkInterfaceName);
    property_set("gsm.net.interface", szNetworkInterfaceName);
    */

    //  Set DNS1
    if (szDNS1)
    {
        PrintStringNullTerminate(szPropName, MAX_BUFFER_SIZE, "net.%s.dns1", szNetworkInterfaceName);
        RIL_LOG_INFO("DataConfigUp() - setting '%s' to '%s'\r\n", szPropName, szDNS1);
        property_set(szPropName, szDNS1);
        /*It's the framwork that have to modify this property
        RIL_LOG_INFO("DataConfigUp() - setting 'net.dns1' to '%s'\r\n", szDNS1);
        property_set("net.dns1", szDNS1);*/
    }

    //  Set DNS2
    if (szDNS2)
    {
        PrintStringNullTerminate(szPropName, MAX_BUFFER_SIZE, "net.%s.dns2", szNetworkInterfaceName);
        RIL_LOG_INFO("DataConfigUp() - setting '%s' to '%s'\r\n", szPropName, szDNS2);
        property_set(szPropName, szDNS2);
        /*It's the framwork that have to modify this property
        RIL_LOG_INFO("DataConfigUp() - setting 'net.dns2' to '%s'\r\n", szDNS2);
        property_set("net.dns2", szDNS2);*/
    }

    bRet = TRUE;

Error:
    if (defaultGatewayStr != NULL)
        free(defaultGatewayStr);

    if (s >= 0)
    {
        close(s);
    }

    RIL_LOG_INFO("DataConfigUp() EXIT  bRet=[%d]\r\n", bRet);

    return bRet;
}


//
//  Call this whenever data is disconnected
//
BOOL DataConfigDown(int nCID)
{
    CRepository repository;
    BOOL bRet = FALSE;
    int s = -1;
    char szNetworkInterfaceName[MAX_BUFFER_SIZE] = {0};
    char szPropName[MAX_BUFFER_SIZE] = {0};
    CChannel_Data* pDataChannel = NULL;
    struct gsm_netconfig netconfig;
    int fd=-1;
    int ret =-1;

    //  Grab the network interface name
    if (!repository.Read(g_szGroupModem, g_szNetworkInterfaceName, szNetworkInterfaceName, MAX_BUFFER_SIZE))
    {
        RIL_LOG_CRITICAL("DataConfigDown() - Could not read network interface name from repository\r\n");
        strcpy(szNetworkInterfaceName, "");
        goto Error;
    }
    RIL_LOG_INFO("DataConfigDown() - ENTER  szNetworkInterfaceName=[%s]  CID=[%d]\r\n", szNetworkInterfaceName, nCID);

    //  Get data channel and set CID to 0.
    if (nCID > 0)
    {
        pDataChannel = CChannel_Data::GetChnlFromContextID(nCID);
        if (pDataChannel)
        {
            // Reset ContextID to 0, to free up the channel for future use
            RIL_LOG_INFO("DataConfigDown() - Setting chnl=[%d] to CID=[0]\r\n", pDataChannel->GetRilChannel());
            pDataChannel->SetContextID(0);
            fd = pDataChannel->GetFD();

            //  Put the channel back into AT command mode
            netconfig.adaption = 3;
            netconfig.protocol = htons(ETH_P_IP);

            if (fd >= 0)
            {
                RIL_LOG_INFO("DataConfigDown() - ***** PUTTING channel=[%d] in AT COMMAND MODE *****\r\n", pDataChannel->GetRilChannel());
                ret = ioctl( fd, GSMIOC_DISABLE_NET, &netconfig );
            }
        }
    }


    // unset net. properties
    // TODO For multiple PDP context support this will have to be updated to be consistent with whatever changes to
    // the Android framework's use of these properties is made.

    RIL_LOG_INFO("DataConfigDown() - setting 'net.interfaces.defaultroute' to ''\r\n");
    property_set("net.interfaces.defaultroute", "");

    RIL_LOG_INFO("DataConfigDown() - setting 'net.gprs.local-ip' to ''\r\n");
    property_set("net.gprs.local-ip", "");

    RIL_LOG_INFO("DataConfigDown() - setting 'net.gprs.operstate' to ''\r\n");
    property_set("net.gprs.operstate", "down");

    RIL_LOG_INFO("DataConfigDown() - setting 'gsm.net.interface' to ''\r\n");
    property_set("gsm.net.interface","");

    PrintStringNullTerminate(szPropName, MAX_BUFFER_SIZE, "net.%s.dns1", szNetworkInterfaceName);
    RIL_LOG_INFO("DataConfigDown() - setting '%s' to ''\r\n", szNetworkInterfaceName);
    property_set(szPropName, "");
    /*
    RIL_LOG_INFO("DataConfigDown() - setting 'net.dns1' to ''\r\n");
    property_set("net.dns1", "");
    */

    PrintStringNullTerminate(szPropName, MAX_BUFFER_SIZE, "net.%s.dns2", szNetworkInterfaceName);
    RIL_LOG_INFO("DataConfigDown() - setting '%s' to ''\r\n", szNetworkInterfaceName);
    property_set(szPropName, "");
    /*
    RIL_LOG_INFO("DataConfigDown() - setting 'net.dns2' to ''\r\n");
    property_set("net.dns2", "");
    */

    //  Open socket for ifconfig and route commands
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        RIL_LOG_CRITICAL("DataConfigDown() : cannot open control socket\n");
        goto Error;
    }


    //  ifconfig ifx02 down
    {
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(struct ifreq));
        strncpy(ifr.ifr_name, szNetworkInterfaceName, IFNAMSIZ);
        ifr.ifr_name[IFNAMSIZ-1] = 0;

        RIL_LOG_CRITICAL("DataConfigDown() : +++++ InterfacName = %s\n", ifr.ifr_name);

        RIL_LOG_INFO("DataConfigDown() : Setting flags\r\n");
        if (!setflags(s, &ifr, 0, IFF_UP))
        {
            RIL_LOG_CRITICAL("DataConfigDown() : ERROR: Setting flags\r\n");
        }
    }

    //  TODO: Implement callback to polling data call list.
    //RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED, NULL, 0);

    //RIL_LOG_INFO("DataConfigDown() - Called timed callback  START\r\n");
    //RIL_requestTimedCallback(triggerDataCallListChanged, NULL, 0, 0);
    //RIL_LOG_INFO("DataConfigDown() - Called timed callback  END\r\n");

    bRet = TRUE;

Error:
    if (s >= 0)
    {
        close(s);
    }

    RIL_LOG_INFO("DataConfigDown() EXIT  bRet=[%d]\r\n", bRet);
    return bRet;
}


//
// Response to AT+CGPADDR=<CID>
//
RIL_RESULT_CODE CTE_INF_6260::ParseIpAddress(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_6260::ParseIpAddress() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    const BYTE* szRsp = rRspData.szResponse;
    UINT32 nCid;
    UINT32  cbIpAddr = 0;
    CChannel_Data* pDataChannel = NULL;

    // Parse prefix
    if (!FindAndSkipString(szRsp, "+CGPADDR: ", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - ERROR: Unable to parse \"+CGPADDR\" prefix.!\r\n");
        goto Error;
    }

    // Parse <cid>
    if (!ExtractUInt(szRsp, nCid, szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - ERROR: Unable to parse <cid>!\r\n");
        goto Error;
    }

    if (nCid > 0)
    {
        pDataChannel = CChannel_Data::GetChnlFromContextID(nCid);
        if (!pDataChannel)
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - ERROR: Could not get Data Channel for Context ID %d.\r\n", nCid);
            goto Error;
        }

        //  Remove previous IP addr (if it existed)
        delete[] pDataChannel->m_szIpAddr;
        pDataChannel->m_szIpAddr = NULL;


        // Parse <PDP_addr>
        if (!SkipString(szRsp, ",", szRsp) ||
            !ExtractQuotedStringWithAllocatedMemory(szRsp, pDataChannel->m_szIpAddr, cbIpAddr, szRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseIpAddress() - ERROR: Unable to parse <PDP_addr>!\r\n");
            goto Error;
        }

        RIL_LOG_INFO("CTE_INF_6260::ParseIpAddress() - IP address: %s\r\n", pDataChannel->m_szIpAddr);

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

    const BYTE* szRsp = rRspData.szResponse;
    UINT32 nCid;
    UINT32  cbDns1 = 0;
    UINT32  cbDns2 = 0;
    CChannel_Data* pDataChannel = NULL;

    // Parse prefix
    if (!FindAndSkipString(szRsp, "+XDNS: ", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - ERROR: Unable to parse \"+XDNS\" prefix.!\r\n");
        goto Error;
    }

    // Parse <cid>
    if (!ExtractUInt(szRsp, nCid, szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - ERROR: Unable to parse <cid>!\r\n");
        goto Error;
    }

    if (nCid > 0)
    {
        pDataChannel = CChannel_Data::GetChnlFromContextID(nCid);
        if (!pDataChannel)
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - ERROR: Could not get Data Channel for Context ID %d.\r\n", nCid);
            goto Error;
        }

        //  Remove previous DNS1 and DNS2 (if it existed)
        delete[] pDataChannel->m_szDNS1;
        pDataChannel->m_szDNS1 = NULL;
        delete[] pDataChannel->m_szDNS2;
        pDataChannel->m_szDNS2 = NULL;


        // Parse <primary DNS>
        if (SkipString(szRsp, ",", szRsp))
        {
            if (!ExtractQuotedStringWithAllocatedMemory(szRsp, pDataChannel->m_szDNS1, cbDns1, szRsp))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - ERROR: Unable to parse <primary DNS>! szDns1\r\n");
                goto Error;
            }
            RIL_LOG_INFO("CTE_INF_6260::ParseDns() - DNS1: %s\r\n", pDataChannel->m_szDNS1);
        }


        // Parse <secondary DNS>
        if (SkipString(szRsp, ",", szRsp))
        {
            if (!ExtractQuotedStringWithAllocatedMemory(szRsp, pDataChannel->m_szDNS2, cbDns2, szRsp))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - ERROR: Unable to parse <secondary DNS>! szDns2\r\n");
                goto Error;
            }
            RIL_LOG_INFO("CTE_INF_6260::ParseDns() - DNS2: %s\r\n", pDataChannel->m_szDNS2);
        }

        res = RRIL_RESULT_OK;
    }
    else
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseDns() - nCid=[%d] not valid!\r\n", nCid);
    }

Error:

    // Signal completion of setting up data
    if (pDataChannel)
        CEvent::Signal(pDataChannel->m_pSetupDoneEvent);

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
    const BYTE* szRsp = rRspData.szResponse;


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
    RIL_SIM_IO *   pSimIOArgs = NULL;

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (sizeof(RIL_SIM_IO) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreSimIo() - ERROR: Invalid data size. Given %d bytes\r\n", uiDataSize);
        goto Error;
    }

    // extract data
    pSimIOArgs = (RIL_SIM_IO *)pData;

    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSimIo() - command=[0x%08x]  [%d]\r\n", pSimIOArgs->command, pSimIOArgs->command);
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSimIo() - fileid=[0x%08x]  [%d]\r\n", pSimIOArgs->fileid, pSimIOArgs->fileid);
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSimIo() - path=[%s]\r\n", (pSimIOArgs->path ? pSimIOArgs->path : "NULL") );
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSimIo() - p1=[0x%08x]  [%d]\r\n", pSimIOArgs->p1, pSimIOArgs->p1);
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSimIo() - p2=[0x%08x]  [%d]\r\n", pSimIOArgs->p2, pSimIOArgs->p2);
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSimIo() - p3=[0x%08x]  [%d]\r\n", pSimIOArgs->p3, pSimIOArgs->p3);
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSimIo() - data=[%s]\r\n", (pSimIOArgs->data ? pSimIOArgs->data : "NULL") );
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreSimIo() - pin2=[%s]\r\n", (pSimIOArgs->pin2 ? pSimIOArgs->pin2 : "NULL") );

    //  If PIN2 is required, send out AT+CPIN2 request
    if (pSimIOArgs->pin2)
    {
        RIL_LOG_INFO("CTE_INF_6260::CoreSimIo() - PIN2 required\r\n");

        CCommand *pCmd1 = NULL;
        BYTE szCmd1[MAX_BUFFER_SIZE] = {0};
        UINT32 uiWaitRes = 0;
        BOOL bEnterPIN2 = FALSE;

        CEvent::Reset(m_pQueryPIN2Event);

        (void)CopyStringNullTerminate(szCmd1,
                     "AT+CPIN2?\r",
                     sizeof(szCmd1));

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
                    //  Found READY, don't enter CPIN2
                    bEnterPIN2 = FALSE;
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
            (void)PrintStringNullTerminate(rReqData.szCmd1,
                sizeof(rReqData.szCmd1),
                "AT+CPIN2=\"%s\"\r",
                pSimIOArgs->pin2);


            if (NULL == pSimIOArgs->data)
            {
                (void)PrintStringNullTerminate(rReqData.szCmd2,
                             sizeof(rReqData.szCmd2),
                             "AT+CRSM=%d,%d,%d,%d,%d\r",
                             pSimIOArgs->command,
                             pSimIOArgs->fileid,
                             pSimIOArgs->p1,
                             pSimIOArgs->p2,
                             pSimIOArgs->p3);
            }
            else
            {
                (void)PrintStringNullTerminate(rReqData.szCmd2,
                             sizeof(rReqData.szCmd2),
                             "AT+CRSM=%d,%d,%d,%d,%d,\"%s\"\r",
                             pSimIOArgs->command,
                             pSimIOArgs->fileid,
                             pSimIOArgs->p1,
                             pSimIOArgs->p2,
                             pSimIOArgs->p3,
                             pSimIOArgs->data);
            }

        }
        else
        {
            //  Didn't have to enter PIN2
            if (NULL == pSimIOArgs->data)
            {
                (void)PrintStringNullTerminate(rReqData.szCmd1,
                             sizeof(rReqData.szCmd1),
                             "AT+CRSM=%d,%d,%d,%d,%d\r",
                             pSimIOArgs->command,
                             pSimIOArgs->fileid,
                             pSimIOArgs->p1,
                             pSimIOArgs->p2,
                             pSimIOArgs->p3);
            }
            else
            {
                (void)PrintStringNullTerminate(rReqData.szCmd1,
                             sizeof(rReqData.szCmd1),
                             "AT+CRSM=%d,%d,%d,%d,%d,\"%s\"\r",
                             pSimIOArgs->command,
                             pSimIOArgs->fileid,
                             pSimIOArgs->p1,
                             pSimIOArgs->p2,
                             pSimIOArgs->p3,
                             pSimIOArgs->data);
            }

        }

    }
    else
    {
        //  No PIN2


        if (NULL == pSimIOArgs->data)
        {
            (void)PrintStringNullTerminate(rReqData.szCmd1,
                         sizeof(rReqData.szCmd1),
                         "AT+CRSM=%d,%d,%d,%d,%d\r",
                         pSimIOArgs->command,
                         pSimIOArgs->fileid,
                         pSimIOArgs->p1,
                         pSimIOArgs->p2,
                         pSimIOArgs->p3);
        }
        else
        {
            (void)PrintStringNullTerminate(rReqData.szCmd1,
                         sizeof(rReqData.szCmd1),
                         "AT+CRSM=%d,%d,%d,%d,%d,\"%s\"\r",
                         pSimIOArgs->command,
                         pSimIOArgs->fileid,
                         pSimIOArgs->p1,
                         pSimIOArgs->p2,
                         pSimIOArgs->p3,
                         pSimIOArgs->data);
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

    uint  uiSW1 = 0;
    uint  uiSW2 = 0;
    BYTE* szResponseString = NULL;
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

    if (!ExtractUInt(pszRsp, uiSW1, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - ERROR: Could not extract SW1 value.\r\n");
        goto Error;
    }

    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt(pszRsp, uiSW2, pszRsp))
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
            szResponseString = new BYTE[cbResponseString];
            if (NULL == szResponseString)
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseSimIo() - ERROR: Cannot create new szResponsestring!\r\n");
                delete[] sNewString;
                sNewString = NULL;
                goto Error;
            }
            CopyStringNullTerminate(szResponseString, "000000000000000000000000000000", cbResponseString);

            //  Extract info, put into new response string
            (void)PrintStringNullTerminate(szTemp, 5, "%04X", sUSIM.dwTotalSize);
            memcpy(&szResponseString[4], szTemp, 4);

            (void)PrintStringNullTerminate(szTemp, 3, "%02X", sUSIM.dwRecordSize);
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

    pResponse->sw1 = uiSW1;
    pResponse->sw2 = uiSW2;

    if (NULL == szResponseString)
    {
        pResponse->simResponse = NULL;
    }
    else
    {
        pResponse->simResponse = (char*)(((char*)pResponse) + sizeof(RIL_SIM_IO_Response));
        (void)CopyStringNullTerminate(pResponse->simResponse, szResponseString, cbResponseString);

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

    char * pszCid = 0;
    char chCid = '1';

    if (sizeof(char **) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreDeactivateDataCall() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::CoreDeactivateDataCall() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
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
        pszCid = ((char**)pData)[0];
        chCid = pszCid[0];

        if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CGACT=0,%c\r", chCid))
        {
            res = RRIL_RESULT_OK;
        }

        //  Set the context of this command to the CID (for multiple context support).
        rReqData.pContextData = (void*)((int)(chCid - '0'));  // Store this as an int.
    }

Error:
    RIL_LOG_VERBOSE("CTE_INF_6260::CoreDeactivateDataCall() - Exit\r\n");
    return res;
}


RIL_RESULT_CODE CTE_INF_6260::ParseDeactivateDataCall(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_INF_6260::ParseDeactivateDataCall() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

#ifdef RIL_USE_PPP
    char szOperstate[PROPERTY_VALUE_MAX];
    char szIP[PROPERTY_VALUE_MAX];
    char* pszDefault = NULL;
    BOOL fPPPDown = FALSE;
    const unsigned int nSleep = 3000;
    const int nRetries = 10;

    property_set("ctl.stop", "pppd_gprs");

    for (int i = 0; i < nRetries; i++)
    {
        RIL_LOG_INFO("CTE_INF_6260::ParseDeactivateDataCall() - INFO: Waiting %d seconds for ppp deactivation. (Attempt #%d/%d)\r\n",
                    nSleep/1000, i + 1, nRetries);
        Sleep(nSleep);

        property_get("net.gprs.operstate", szOperstate, "");
        RIL_LOG_INFO("CTE_INF_6260::ParseDeactivateDataCall() - INFO: net.grps.operstate=[%s]\r\n", szOperstate);

        if (0 == strcmp(szOperstate, "down"))
        {
            RIL_LOG_INFO("CTE_INF_6260::ParseDeactivateDataCall() - INFO: ppp connection is down!\r\n");
            fPPPDown= TRUE;
            break;
        }
        else
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseDeactivateDataCall() - ERROR: ppp connection is not down\r\n");
        }
    }

    if (fPPPDown)
    {
        //  Wait n seconds
        const unsigned int nSec = 10;
        Sleep(nSec * 1000);
        res = RRIL_RESULT_OK;
    }

#else



    //  Set CID to 0 for this data channel
    int nCID = 0;
    nCID = (int)rRspData.pContextData;

    if (!DataConfigDown(nCID))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseDeactivateDataCall() - ERROR : Couldn't DataConfigDown\r\n");
    }

    res = RRIL_RESULT_OK;

#endif //RIL_USE_PPP

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

    switch(bCommand)
    {
        case RIL_OEM_HOOK_RAW_POWEROFF:
            RIL_LOG_INFO("TE_INF_6260::CoreHookRaw() - RIL_OEM_HOOK_RAW_POWEROFF Command=[0x%02X] received OK\r\n", (unsigned char)bCommand);

            //  Shouldn't be any data following command

            //  We have to be unregistered to do the change.
            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+CFUN=0\r", sizeof(rReqData.szCmd1)))
            {
                RIL_LOG_CRITICAL("TE_INF_6260::CoreHookRaw() - ERROR: RIL_OEM_HOOK_RAW_POWEROFF - Can't construct szCmd1.\r\n");
                goto Error;
            }

            //  NOTE: I am assuming this is the last AT command to be sent to the modem.
            //  It turns out that this AT command causes TriggerRadioError() to be called.
            //  The following line will simply return out of the TriggerRadioError() function call.
            g_bIsTriggerRadioError = TRUE;

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

    const BYTE* szRsp = rRspData.szResponse;
    const BYTE* szDummy = NULL;

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
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetRegistrationStatus() - ERROR: Could not skip \"+XBANDSEL: \".\r\n");
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
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetAvailableBandModes() - ERROR: Cannot have a count of zero.\r\n");
        goto Error;
    }

    //  Alloc array and don't forget about the first entry for size.
    pModes = (int*)malloc( (1 + count) * sizeof(int));
    if (NULL == pModes)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetAvailableBandModes() - ERROR: Could not allocate memory for response.\r\n");
        goto Error;
    }

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

    if (!ExtractUInt(pszRsp, uiLength, pszRsp))
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
    if (!ExtractUInt(pszRsp, uiSw1, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - ERROR: Could not extract sw1.\r\n");
        goto Error;
    }

    // Parse ",<sw2>"
    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt(pszRsp, uiSw2, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - ERROR: Could not extract sw2.\r\n");
        goto Error;
    }

    // Parse ",<event_type>"
    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt(pszRsp, uiEventType, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseStkSendEnvelopeCommand() - ERROR: Could not extract event type.\r\n");
        goto Error;
    }

    // Parse ",<envelope_type>"
    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt(pszRsp, uiEnvelopeType, pszRsp))
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
#ifndef USE_STK_RAW_MODE
    char* cmd = NULL;
#endif
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

#if USE_STK_RAW_MODE
    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+SATR=\"%s\"\r", pszTermResponse))
    {
        RIL_LOG_INFO("CTE_INF_6260::CoreStkSendTerminalResponse() - ERROR: Could not form string.\r\n");
        goto Error;
    }
#else
    cmd = hex_to_stk_at(pszTermResponse);

    if (!CopyStringNullTerminate(rReqData.szCmd1, cmd, sizeof(rReqData.szCmd1)))
    {
        RIL_LOG_INFO("CTE_INF_6260::CoreStkSendTerminalResponse() - ERROR: Could not form string.\r\n");
        goto Error;
    }
#endif

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
        case 0: // WCDMA Preferred

            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XRAT=1,2\r", sizeof(rReqData.szCmd1) ))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetPreferredNetworkType() - ERROR: Can't construct szCmd1 nNetworkType=%d\r\n", nNetworkType);
                goto Error;
            }

           break;

        case 1: // GSM Only

            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XRAT=0\r", sizeof(rReqData.szCmd1) ))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::CoreSetPreferredNetworkType() - ERROR: Can't construct szCmd1 nNetworkType=%d\r\n", nNetworkType);
                goto Error;
            }

            break;

        case 2: // WCDMA Only

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

    if (!ExtractUInt(pszRsp, rat, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetPreferredNetworkType() - ERROR: Could not extract rat value.\r\n");
        goto Error;
    }

    if (FindAndSkipString(pszRsp, ",", pszRsp))
    {
        if (!ExtractUInt(pszRsp, pref, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetPreferredNetworkType() - ERROR: Could not find and skip pref value even though it was expected.\r\n");
            goto Error;
        }
    }

    switch(rat)
    {
        case 0:     // GSM Only
        {
            pRat[0] = 1;
            m_nCurrentNetworkType = 1;
            break;
        }

        case 1:     // WCDMA Preferred
        {
            pRat[0] = 0;
            m_nCurrentNetworkType = 0;
            break;
        }

        case 2:     // WCDMA only
        {
            pRat[0] = 2;
            m_nCurrentNetworkType = 2;
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

    const BYTE* szDummy = pszRsp;

    UINT32 nFound = 0, nTotal = 0;
    UINT32 nMode = 0;
    UINT32 nType = 0;
    BYTE szLAC[CELL_ID_ARRAY_LENGTH] = {0};
    BYTE szCI[CELL_ID_ARRAY_LENGTH] = {0};
    UINT32 nRSSI = 0;  //  Use for both GSM and UMTS
    UINT32 nRSCP = 0;  //  UMTS only

    P_ND_N_CELL_DATA pCellData = NULL;



    //  Data is either (according to rev 27 of AT spec)
    //  GSM:
    //  +XCELLINFO: <mode>,<type>,<MCC>,<MNC>,<LAC>,<CI>,<RxLev>[,t_advance]
    //  UMTS:
    //  +XCELLINFO: <mode>,<type>,<MCC>,<MNC>,<LAC>,<CI>,<scrambling_code>,<dl_frequency>,<rscp>,<ecn0>
    //
    //  A <type> of 0 or 1 = GSM.  A <type> of 2,3,4 = UMTS.


    // Loop on +XCELLINFO until no more entries are found
    while (FindAndSkipString(szDummy, "+XCELLINFO: ", szDummy))
    {
        nTotal++;

        if (!ExtractUInt(szDummy, nMode, szDummy) ||
            !SkipString(szDummy, ",", szDummy) ||
            !ExtractUInt(szDummy, nType, szDummy))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: Could not extract nMode or nType 1.\r\n");
            goto Error;
        }

        //  Find next ,
        if (!FindAndSkipString(szDummy, ",", szDummy))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: Could not find comma 1.\r\n");
            goto Error;
        }

        //  Find next ,
        if (!FindAndSkipString(szDummy, ",", szDummy))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: Could not find comma 2.\r\n");
            goto Error;
        }

        //  Find next ,
        if (!FindAndSkipString(szDummy, ",", szDummy))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: Could not find comma 3.\r\n");
            goto Error;
        }

        //  Get <LAC>
        if (!ExtractUnquotedString(szDummy, ',', szLAC, CELL_ID_ARRAY_LENGTH, szDummy))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: Could not extract LAC.\r\n");
            goto Error;
        }

        //  Bogus cell infos have LAC of "ffff" regardless of GSM or UMTS.
        if ((0 == strncmp("ffff", szLAC, 4)) ||
            (0 == strncmp("FFFF", szLAC, 4)))
        {
            //  Found bogus cell!  Skip
            RIL_LOG_VERBOSE("CTE_INF_6260::ParseGetNeighboringCellIDs() - Found bogus cell nFound=[%d] nTotal=[%d]\r\n", nFound, nTotal);
            continue;
        }

        //  Made it this far, add one to count.
        nFound++;
    }

    RIL_LOG_INFO("CTE_INF_6260::ParseGetNeighboringCellIDs() - INFO: Found %d entries out of %d total\r\n", nFound, nTotal);

    if (nFound >= RRIL_MAX_CELL_ID_COUNT)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - Too many neighboring cells\r\n");
        goto Error;
    }


    pCellData = (P_ND_N_CELL_DATA)malloc(sizeof(S_ND_N_CELL_DATA));
    if (NULL == pCellData)
    {
        RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: Could not allocate memory for a S_ND_N_CELL_DATA struct.\r\n");
        goto Error;
    }
    memset(pCellData, 0, sizeof(S_ND_N_CELL_DATA));

    nFound = 0;
    nTotal = 0;

    //  Ok, loop through again.
    while (FindAndSkipString(pszRsp, "+XCELLINFO: ", pszRsp))
    {
        nTotal++;

        //  Get type
        if ((!FindAndSkipString(pszRsp, ",", pszRsp)) ||
            (!ExtractUInt(pszRsp, nType, pszRsp)) )
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: Could not extract type.\r\n");
            goto Error;
        }

        if (nType >= 5)
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: Invalid type=[%d]\r\n", nType);
            goto Error;
        }

        //  Skip to LAC
        if ((!FindAndSkipString(pszRsp, ",", pszRsp)) ||
            (!FindAndSkipString(pszRsp, ",", pszRsp)) ||
            (!FindAndSkipString(pszRsp, ",", pszRsp)) ||
            (!ExtractUnquotedString(pszRsp, ",", szLAC, CELL_ID_ARRAY_LENGTH, pszRsp)))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: Could not extract LAC value. 2\r\n");
            goto Error;
        }

        //  If LAC is "ffff" or "FFFF", skip
        if ((0 == strncmp("ffff", szLAC, 4)) ||
            (0 == strncmp("FFFF", szLAC, 4)))
        {
            //  Found bogus cell!  Skip
            RIL_LOG_VERBOSE("CTE_INF_6260::ParseGetNeighboringCellIDs() - Found bogus cell nFound=[%d] nTotal=[%d] 2\r\n", nFound, nTotal);
            continue;
        }

        //  Read <CI>
        if ((!FindAndSkipString(pszRsp, ",", pszRsp)) ||
            (!ExtractUnquotedString(pszRsp, ",", szCI, CELL_ID_ARRAY_LENGTH, pszRsp)))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: Could not extract CI value. 2\r\n");
            goto Error;
        }

        //  Read <RxLev> or <scrambling_code>
        if ((!FindAndSkipString(pszRsp, ",", pszRsp)) ||
            (!ExtractUInt(pszRsp, nRSSI, pszRsp)))
        {
            RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: Could not extract nRSSI value. 2\r\n");
            goto Error;
        }

        //  If we're type UMTS (2,3,4), read rscp.
        if ( (2 ==nType) || (3 == nType) || (4 == nType) )
        {
            //  Read <rscp>
            if ((!FindAndSkipString(pszRsp, ",", pszRsp)) ||
                (!FindAndSkipString(pszRsp, ",", pszRsp)) ||
                (!ExtractUInt(pszRsp, nRSCP, pszRsp)))
            {
                RIL_LOG_CRITICAL("CTE_INF_6260::ParseGetNeighboringCellIDs() - ERROR: Could not extract nRSCP value. 2\r\n");
                goto Error;
            }
        }

        //  Now populate data we have for this entry.

        //  UMTS
        if ( (2 ==nType) || (3 == nType) || (4 == nType) )
        {
            //  cid = <scrambling code> as char *
            pCellData->pnCellData[nFound].cid = pCellData->pnCellCIDBuffers[nFound];
            snprintf(pCellData->pnCellCIDBuffers[nFound], CELL_ID_ARRAY_LENGTH, "%08x", nRSSI);

            RIL_LOG_INFO("CTE_INF_6260::ParseGetNeighboringCellIDs() - UMTS scramblingcode index=[%d]  cid=[%s]\r\n", nFound, pCellData->pnCellCIDBuffers[nFound]);

            //  rssi = rscp
            pCellData->pnCellData[nFound].rssi = (int)nRSCP;
            RIL_LOG_INFO("CTE_INF_6260::ParseGetNeighboringCellIDs() - UMTS rscp index=[%d]  rssi=[%d]\r\n", nFound, pCellData->pnCellData[nFound].rssi);

        }
        else if ( (0 == nType) || (1 == nType) )
        {
            //  GSM
            pCellData->pnCellData[nFound].cid = pCellData->pnCellCIDBuffers[nFound];

            //  cid = upper 16 bits (LAC), lower 16 bits (CID)
            memcpy(pCellData->pnCellCIDBuffers[nFound], szLAC, 4);
            memcpy(&pCellData->pnCellCIDBuffers[nFound][4], szCI, 4);
            pCellData->pnCellCIDBuffers[nFound][8] = '\0';

            RIL_LOG_INFO("CTE_INF_6260::ParseGetNeighboringCellIDs() - GSM LAC,CID index=[%d]  cid=[%s]\r\n", nFound, pCellData->pnCellCIDBuffers[nFound]);

            //  rssi = <RxLev>

            //  May have to convert RxLev to asu (0 to 31).
            //  For GSM, it is in "asu" ranging from 0 to 31 (dBm = -113 + 2*asu)
            //  0 means "-113 dBm or less" and 31 means "-51 dBm or greater"

            //  But for now, just leave it as is.
            pCellData->pnCellData[nFound].rssi = (int)nRSSI;
            RIL_LOG_INFO("CTE_INF_6260::ParseGetNeighboringCellIDs() - GSM rxlev index=[%d]  rssi=[%d]\r\n", nFound, pCellData->pnCellData[nFound].rssi);

        }

        //  Connect the pointer
        pCellData->pnCellPointers[nFound] = &(pCellData->pnCellData[nFound]);

        nFound++;
    }


    if (nFound > 0)
    {
        rRspData.pData  = (void*)pCellData;
        rRspData.uiDataSize = nFound * sizeof(RIL_NeighboringCell*);
    }
    else
    {
        rRspData.pData  = NULL;
        rRspData.uiDataSize = 0;
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
    const BYTE* szRsp = rRspData.szResponse;
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
    if (!ExtractUInt(szRsp, uiTtyMode, szRsp))
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
