////////////////////////////////////////////////////////////////////////////
// te_inf_n721.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Overlay for the Infineon N721 modem
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
#include "te_inf_n721.h"

#include <cutils/properties.h>
#include <sys/system_properties.h>

//  This is for close().
#include <unistd.h>

//  This is for socket-related calls.
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/route.h>

CTE_INF_N721::CTE_INF_N721()
 : m_szIpAddr(NULL)
{
    // TODO: Should really use an array of events - one for each data channel/CID
    // and corresponding array of IP addresses
    m_pSetupDataEvent = new CEvent(NULL, TRUE);
}

CTE_INF_N721::~CTE_INF_N721()
{
    // destroy event
    if (m_pSetupDataEvent)
    {
        delete m_pSetupDataEvent;
        m_pSetupDataEvent = NULL;
    }

    // delete IP Address
    if (m_szIpAddr)
    {
        delete[] m_szIpAddr;
        m_szIpAddr = NULL;
    }
}

//  It appears signal strength is from 0-31 again.
#if 0
// RIL_REQUEST_SIGNAL_STRENGTH 19
RIL_RESULT_CODE CTE_INF_N721::ParseSignalStrength(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_N721::ParseSignalStrength() - Enter\r\n");
    
    extern ACCESS_TECHNOLOGY g_uiAccessTechnology;

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;
    
    uint                uiRSSI  = 0;
    uint                uiBER   = 0;
    RIL_SignalStrength* pSigStrData;

    pSigStrData = (RIL_SignalStrength*) malloc(sizeof(RIL_SignalStrength));

    if (NULL == pSigStrData)
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseSignalStrength() - ERROR: Could not allocate memory for RIL_SignalStrength.\r\n");
        goto Error;
    }
    memset(pSigStrData, 0x00, sizeof(RIL_SignalStrength));

    // Parse "<prefix>+CSQ: <rssi>,<ber><postfix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp) ||
        !SkipString(pszRsp, "+CSQ: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseSignalStrength() - ERROR: Could not find AT response.\r\n");
        goto Error;            
    }
    
    if (!ExtractUInt(pszRsp, uiRSSI, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseSignalStrength() - ERROR: Could not extract uiRSSI.\r\n");
        goto Error;                        
    }
    
    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt(pszRsp, uiBER, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseSignalStrength() - ERROR: Could not extract uiBER.\r\n");
        goto Error;            
    }
    
    if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseSignalStrength() - ERROR: Could not extract the response end.\r\n");
        goto Error;
    }

    if (ACT_UMTS == g_uiAccessTechnology)
    {
        RIL_LOG_INFO("CTE_INF_N721::ParseSignalStrength() - Compressing values in UMTS\r\n");
        // compress values - this needs to be reviewed as the mapping is not linear,
        // but this will do for now

        // for Infineon modem, the range is 0..91, so compress down to 0..31
        if (99 != uiRSSI)
            uiRSSI /= 3;

        // for Infineon modem, the range is 0..49, so compress down to 0..7
        if (99 != uiBER)
            uiBER /= 7;
    }
    pSigStrData->GW_SignalStrength.signalStrength = (int) uiRSSI;
    pSigStrData->GW_SignalStrength.bitErrorRate   = (int) uiBER;

    rRspData.pData   = (void*)pSigStrData;
    rRspData.uiDataSize  = sizeof(RIL_SignalStrength);
    
    res = RRIL_RESULT_OK;        

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pSigStrData);
        pSigStrData = NULL;
    }

    RIL_LOG_VERBOSE("CTE_INF_N721::ParseSignalStrength() - Exit\r\n");
    return res;    
}
#endif // 0

// RIL_REQUEST_SETUP_DATA_CALL 27
RIL_RESULT_CODE CTE_INF_N721::CoreSetupDataCall(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_N721::CoreSetupDataCall() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    PdpData stPdpData;
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::CoreSetupDataCall() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (6 * sizeof(char*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::CoreSetupDataCall() - ERROR: Invalid data size. Was given %d bytes\r\n", uiDataSize);
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
    // with +CGQREQ to specify requested QoS and +CGACT to activate PDP Context on command channel; then
    // if RAW IP is used send AT+CGDATA to enable Raw IP on data channel (which will then switch the channel to data mode).
    //
    // TODO: CID is currently hardcoded to 1
    //
    // TBD: Do we also need to send AT+XDNS-<CID>,1 before AT+CGDCONT to enable dynamic DNS?
    //
    (void)PrintStringNullTerminate(rReqData.szCmd1,
        sizeof(rReqData.szCmd1),
        "AT+CGDCONT=1,\"IP\",\"%s\",,0,0;+CGQREQ=1;+CGQMIN=1;+CGACT=0,1\r",
        stPdpData.szApn);

#ifndef RIL_USE_PPP       
   (void)CopyStringNullTerminate(rReqData.szCmd2, "AT+CGDATA=\"M-RAW_IP\",1\r", sizeof(rReqData.szCmd2));
#endif //!RIL_USE_PPP
   
    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTE_INF_N721::CoreSetupDataCall() - Exit\r\n");
    return res;
}


RIL_RESULT_CODE CTE_INF_N721::ParseSetupDataCall(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_INF_N721::ParseSetupDataCall() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char szIP[PROPERTY_VALUE_MAX];
    P_ND_SETUP_DATA_CALL pDataCallRsp = NULL;

#ifdef RIL_USE_PPP
    char szOperstate[PROPERTY_VALUE_MAX];
    BOOL fPPPUp = FALSE;
    const unsigned int nSleep = 2000;
    const int nRetries = 15;
    property_set("ctl.start", "pppd_gprs");

    for (int i = 0; i < nRetries; i++)
    {
        RIL_LOG_INFO("CTE_INF_N721::ParseSetupDataCall() - INFO: Waiting %d seconds for ppp connection. (Attempt #%d/%d)\r\n",
                    nSleep/1000, i + 1, nRetries);
        Sleep(nSleep);

        property_get("net.gprs.operstate", szOperstate, "");
        RIL_LOG_INFO("CTE_INF_N721::ParseSetupDataCall() - INFO: net.grps.operstate=[%s]\r\n", szOperstate);

        if (0 == strcmp(szOperstate, "up"))
        {
            RIL_LOG_INFO("CTE_INF_N721::ParseSetupDataCall() - INFO: ppp connection is up!\r\n");
            fPPPUp = TRUE;
            break;
        }
        else
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseSetupDataCall() - ERROR: ppp connection is not up\r\n");
        }
    }

    if (fPPPUp)
    {
        Sleep(nSleep);
        property_get("net.grps.local-ip", szIP, "");
        RIL_LOG_INFO("CTE_INF_N721::ParseSetupDataCall() - INFO: net.grps.local-ip=[%s]\r\n", szIP);
    }

#else

    // For RAW IP, when we get the CONNECT response to AT+CGDATA, we then need to send AT+CGPADDR (or AT+CGDCONT?) 
    // to get the IP address (which is returned in the response to the RIL_REQUEST_SETUP_DATA_CALL) and finally
    // AT+XDNS to query for the DNS address of the activated context. We wait until we get DNS response before
    // sending the reply to RIL_REQUEST_SETUP_DATA_CALL.

    CCommand * pCmd1 = NULL;
    CCommand * pCmd2 = NULL;
    UINT32 uiWaitRes;

    // 1st confirm we got "CONNECT"
    const BYTE* szRsp = rRspData.szResponse;

    if (!FindAndSkipString(szRsp, "CONNECT", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseSetupDataCall() - ERROR: Did not get \"CONNECT\" response.\r\n");
        goto Error;
    }

    // Send AT+CGPADDR and AT+XDNS? commands to query for assigned IP Address and DNS and wait for responses

    CEvent::Reset(m_pSetupDataEvent);


    // TODO: CID is currently hardcoded to 1
    pCmd1 = new CCommand(RIL_CHANNEL_ATCMD, NULL, ND_REQ_ID_GETIPADDRESS, "AT+CGPADDR=1\r", &CTE::ParseIpAddress);
    if (pCmd1)
    {
        if (!CCommand::AddCmdToQueue(pCmd1))
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseSetupDataCall() - ERROR: Unable to queue AT+CGPADDR command!\r\n");
            delete pCmd1;
            pCmd1 = NULL;
            goto Error;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseSetupDataCall() - ERROR: Unable to allocate memory for new AT+CGPADDR command!\r\n");
        goto Error;
    }

    pCmd2 = new CCommand(RIL_CHANNEL_ATCMD, NULL, ND_REQ_ID_GETDNS, "AT+XDNS?\r", &CTE::ParseDns);
    if (pCmd2)
    {
        if (!CCommand::AddCmdToQueue(pCmd2))
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseSetupDataCall() - ERROR: Unable to queue AT+XDNS? command!\r\n");
            delete pCmd2;
            pCmd2 = NULL;
            goto Error;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseSetupDataCall() - ERROR: Unable to allocate memory for new AT+XDNS? command!\r\n");
        goto Error;
    }

    uiWaitRes = CEvent::Wait(m_pSetupDataEvent, 30000); // TODO: put timeout in repository to allow tweaking?
    switch (uiWaitRes) 
    {
        case WAIT_EVENT_0_SIGNALED:
            RIL_LOG_INFO("CTE_INF_N721::ParseSetupDataCall() : SetupData event signalled\r\n");
            if (NULL == m_szIpAddr)
            {
                 RIL_LOG_CRITICAL("CTE_INF_N721::ParseSetupDataCall() - ERROR: IP address is null\r\n");
                 goto Error;
            }
            strcpy(szIP, m_szIpAddr);
            break;

        case WAIT_TIMEDOUT:
             RIL_LOG_CRITICAL("CTE_INF_N721::ParseSetupDataCall() - Timed out waiting for IP Address and DNS\r\n");
             goto Error;

        default:
             RIL_LOG_CRITICAL("CTE_INF_N721::ParseSetupDataCall() - Unexpected event result on Wait for IP Address and DNS, res: %d\r\n", uiWaitRes);
             goto Error;
    }

#endif //RIL_USE_PPP

    pDataCallRsp = (P_ND_SETUP_DATA_CALL)malloc(sizeof(S_ND_SETUP_DATA_CALL));
    strcpy(pDataCallRsp->szCID, "1");     // TODO: CID is currently hardcoded to 1
    strcpy(pDataCallRsp->szNetworkInterfaceName, "gprs");
    strcpy(pDataCallRsp->szIPAddress, szIP);
    pDataCallRsp->sSetupDataCallPointers.pszCID = pDataCallRsp->szCID;
    pDataCallRsp->sSetupDataCallPointers.pszNetworkInterfaceName = pDataCallRsp->szNetworkInterfaceName;
    pDataCallRsp->sSetupDataCallPointers.pszIPAddress = pDataCallRsp->szIPAddress;
    rRspData.pData = (void*)pDataCallRsp;
    rRspData.uiDataSize = sizeof(S_ND_SETUP_DATA_CALL_POINTERS);
    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_INFO("CTE_INF_N721::ParseSetupDataCall() - Exit\r\n");
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

BOOL CallDataConfigCmds(char *szInterfaceName, char *szIpAddr)
{
    RIL_LOG_INFO("CallDataConfigCmds() ENTER  szInterfaceName=[%s]  szIpAddr=[%s]\r\n", szInterfaceName, szIpAddr);
    
    BOOL bRet = FALSE;
    int s;
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        RIL_LOG_CRITICAL("CallDataConfigCmds() : cannot open control socket\n");
        goto Error;
    }

    //  ifconfig ifx02 <ip address> netmask 255.255.255.0
    {
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(struct ifreq));
        strncpy(ifr.ifr_name, szInterfaceName, IFNAMSIZ);
        ifr.ifr_name[IFNAMSIZ-1] = 0;
        
        if (!setaddr(s, &ifr, szIpAddr)) // ipaddr
        {
            goto Error;
        } 
        
        if (!setflags(s, &ifr, IFF_UP, 0))
        {
            goto Error;
        }
        
        if (!setnetmask(s, &ifr, "255.255.255.0"))  // the netmask
        {
            goto Error;
        }
    }


    
    //  route add devault dev ifx02
    {
        struct rtentry rt;
        memset(&rt, 0, sizeof(struct rtentry));
        
        rt.rt_dst.sa_family = AF_INET;
        rt.rt_genmask.sa_family = AF_INET;
        rt.rt_gateway.sa_family = AF_INET;

        
        rt.rt_flags = RTF_UP;
        rt.rt_dev = szInterfaceName;

        RIL_LOG_INFO("CallDataConfigCmds() - calling SIOCADDRT\r\n");
        if (ioctl(s, SIOCADDRT, &rt) < 0)
        {
            RIL_LOG_CRITICAL("CallDataConfigCmds() : ERROR: SIOCADDRT\r\n");
            goto Error;
        }
    }
    
    
    //  setprop gsm.net.interface ifx02
    RIL_LOG_INFO("CallDataConfigCmds() - setting gsm.net.interface=[%s]\r\n", szInterfaceName);
    property_set("gsm.net.interface", szInterfaceName);
    
    bRet = TRUE;
    
Error:
    if (s >= 0)
    {
        close(s);
    }
    
    RIL_LOG_INFO("CallDataConfigCmds() EXIT  bRet=[%d]\r\n", bRet);
    
    return bRet;
}

//
// Response to AT+CGPADDR=<CID>
//
RIL_RESULT_CODE CTE_INF_N721::ParseIpAddress(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_INF_N721::ParseIpAddress() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    const BYTE* szRsp = rRspData.szResponse;
    UINT32 nCid;
    UINT32 nChannel;
    UINT32  cbIpAddr = 0;
    char szInterfaceName[MAX_BUFFER_SIZE];

    if (m_szIpAddr)
    {
        delete[] m_szIpAddr;
        m_szIpAddr = NULL;
    }

    // Parse prefix
    if (!FindAndSkipString(szRsp, "+CGPADDR: ", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseIpAddress() - ERROR: Unable to parse \"+CGPADDR\" prefix.!\r\n");
        goto Error;
    }

    // Parse <cid>
    // TODO: Check for CID match
    if (!ExtractUInt(szRsp, nCid, szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseIpAddress() - ERROR: Unable to parse <cid>!\r\n");
        goto Error;
    }
        
    // Parse <PDP_addr>
    if (!SkipString(szRsp, ",", szRsp) ||
        !ExtractQuotedStringWithAllocatedMemory(szRsp, m_szIpAddr, cbIpAddr, szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseIpAddress() - ERROR: Unable to parse <PDP_addr>!\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_INF_N721::ParseIpAddress() - IP address: %s\r\n", m_szIpAddr);
    // set net. properties
    property_set("net.interfaces.defaultroute", "gprs");
    property_set("net.gprs.local-ip", m_szIpAddr);
    property_set("net.gprs.operstate", "up");

    // invoke netcfg commands
    nChannel = 2; // TODO, ifx data channel is currently hardcoded to 2
    snprintf(szInterfaceName, sizeof(szInterfaceName), "ifx%02d", nChannel);
    
    if (!CallDataConfigCmds(szInterfaceName, m_szIpAddr))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseIpAddress() - ERROR: Unable to set ifconfig\r\n");
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_INFO("CTE_INF_N721::ParseIpAddress() - Exit\r\n");
    return res;
}

//
// Response to AT+XDNS?
//
RIL_RESULT_CODE CTE_INF_N721::ParseDns(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_INF_N721::ParseDns() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    const BYTE* szRsp = rRspData.szResponse;
    UINT32 nCid;
    BYTE* szDns1 = NULL;
    BYTE* szDns2 = NULL;
    UINT32  cbDns1 = 0;
    UINT32  cbDns2 = 0;

    // Parse prefix
    if (!FindAndSkipString(szRsp, "+XDNS: ", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseDns() - ERROR: Unable to parse \"+XDNS\" prefix.!\r\n");
        goto Error;
    }

    // Parse <cid>
    // TODO: Check for CID match
    if (!ExtractUInt(szRsp, nCid, szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseDns() - ERROR: Unable to parse <cid>!\r\n");
        goto Error;
    }

    // Parse <primary DNS>
    if (!SkipString(szRsp, ",", szRsp) ||
        !ExtractQuotedStringWithAllocatedMemory(szRsp, szDns1, cbDns1, szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseDns() - ERROR: Unable to parse <primary DNS>!\r\n");
        goto Error;
    }
    RIL_LOG_INFO("CTE_INF_N721::ParseDns() - setting 'net.gprs.dns1' to: %s\r\n", szDns1);
    property_set("net.gprs.dns1", szDns1);

    // Parse <secondary DNS>
    if (!SkipString(szRsp, ",", szRsp) ||
        !ExtractQuotedStringWithAllocatedMemory(szRsp, szDns2, cbDns2, szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseDns() - ERROR: Unable to parse <secondary DNS>!\r\n");
        goto Error;
    }
    RIL_LOG_INFO("CTE_INF_N721::ParseDns() - setting 'net.gprs.dns2' to: %s\r\n", szDns2);
    property_set("net.gprs.dns2", szDns2);

    res = RRIL_RESULT_OK;

Error:

    // Signal completion of setting up data
    CEvent::Signal(m_pSetupDataEvent);

    if (szDns1) delete[] szDns1;
    if (szDns2) delete[] szDns2;

    RIL_LOG_INFO("CTE_INF_N721::ParseDns() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DEACTIVATE_DATA_CALL 41
//
RIL_RESULT_CODE CTE_INF_N721::CoreDeactivateDataCall(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_N721::CoreDeactivateDataCall() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char * pszCid = 0;
    char chCid = '1';

    if (sizeof(char **) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::CoreDeactivateDataCall() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::CoreDeactivateDataCall() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    pszCid = ((char**)pData)[0];    
    chCid = pszCid[0];
    
    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CGACT=0,%c\r", chCid))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTE_INF_N721::CoreDeactivateDataCall() - Exit\r\n");
    return res;
}


RIL_RESULT_CODE CTE_INF_N721::ParseDeactivateDataCall(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_INF_N721::ParseDeactivateDataCall() - Enter\r\n");
    
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
        RIL_LOG_INFO("CTE_INF_N721::ParseDeactivateDataCall() - INFO: Waiting %d seconds for ppp deactivation. (Attempt #%d/%d)\r\n",
                    nSleep/1000, i + 1, nRetries);
        Sleep(nSleep);

        property_get("net.gprs.operstate", szOperstate, "");
        RIL_LOG_INFO("CTE_INF_N721::ParseDeactivateDataCall() - INFO: net.grps.operstate=[%s]\r\n", szOperstate);

        if (0 == strcmp(szOperstate, "down"))
        {
            RIL_LOG_INFO("CTE_INF_N721::ParseDeactivateDataCall() - INFO: ppp connection is down!\r\n");
            fPPPDown= TRUE;
            break;
        }
        else
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseDeactivateDataCall() - ERROR: ppp connection is not down\r\n");
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

    // unset net. properties
    property_set("net.interfaces.defaultroute", "");
    property_set("net.gprs.dns1", "");
    property_set("net.gprs.dns2", "");
    property_set("net.gprs.local-ip", "");
    property_set("net.gprs.operstate", "down");
    property_set("gsm.net.interface","");

    res = RRIL_RESULT_OK;

#endif //RIL_USE_PPP

    RIL_LOG_INFO("CTE_INF_N721::ParseDeactivateDataCall() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_BAND_MODE 65
//
RIL_RESULT_CODE CTE_INF_N721::CoreSetBandMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_N721::CoreSetBandMode() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    UINT32* pnBandMode;

    if (sizeof(int*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::CoreSetBandMode() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::CoreSetBandMode() - ERROR: Passed data pointer was NULL\r\n");
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
        RIL_LOG_CRITICAL("CTE_INF_N721::CoreSetBandMode() - ERROR: Japan region is not supported!\r\n");
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
        RIL_LOG_CRITICAL("CTE_INF_N721::CoreSetBandMode() - ERROR: Undefined region code: %d\r\n", *pnBandMode);
        res = RIL_E_GENERIC_FAILURE;
        break;
    }

Error:
    RIL_LOG_VERBOSE("CTE_INF_N721::CoreSetBandMode() - Exit\r\n");
    return res;
}


//
// RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE 66
//
RIL_RESULT_CODE CTE_INF_N721::CoreQueryAvailableBandMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_N721::CoreQueryAvailableBandMode() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XBANDSEL=?\r"))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTE_INF_N721::CoreQueryAvailableBandMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_N721::ParseQueryAvailableBandMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_N721::ParseQueryAvailableBandMode() - Enter\r\n");

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
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseGetRegistrationStatus() - ERROR: Could not skip \"+XBANDSEL: \".\r\n");
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
        RIL_LOG_INFO("CTE_INF_N721::ParseQueryAvailableBandMode() - automatic\r\n");
        count++;
    }

    if (euro)
    {
        RIL_LOG_INFO("CTE_INF_N721::ParseQueryAvailableBandMode() - euro\r\n");
        count++;
    }

    if (usa)
    {
        RIL_LOG_INFO("CTE_INF_N721::ParseQueryAvailableBandMode() - usa\r\n");
        count++;
    }

    if (japan)
    {
        RIL_LOG_INFO("CTE_INF_N721::ParseQueryAvailableBandMode() - japan\r\n");
        count++;
    }

    if (aus)
    {
        RIL_LOG_INFO("CTE_INF_N721::ParseQueryAvailableBandMode() - aus\r\n");
        count++;
    }

    if (aus2)
    {
        RIL_LOG_INFO("CTE_INF_N721::ParseQueryAvailableBandMode() - aus2\r\n");
        count++;
    }

    RIL_LOG_INFO("CTE_INF_N721::ParseQueryAvailableBandMode() - count=%d\r\n", count);
    
    if (0 == count)
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseGetAvailableBandModes() - ERROR: Cannot have a count of zero.\r\n");
        goto Error;
    }
    
    //  Alloc array and don't forget about the first entry for size.
    pModes = (int*)malloc( (1 + count) * sizeof(int));
    if (NULL == pModes)
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseGetAvailableBandModes() - ERROR: Could not allocate memory for response.\r\n");
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


    RIL_LOG_VERBOSE("CTE_INF_N721::ParseQueryAvailableBandMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
//
RIL_RESULT_CODE CTE_INF_N721::CoreSetPreferredNetworkType(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_N721::CoreSetPreferredNetworkType() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int nNetworkType = 0;
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::CoreSetPreferredNetworkType() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != sizeof(int *))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::CoreSetPreferredNetworkType() - ERROR: Invalid data size.\r\n");
        goto Error;
    }
    
    nNetworkType = ((int *)pData)[0];
    
    //  We have to be unregistered to do the change.
    if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+COPS=2\r", sizeof(rReqData.szCmd1)))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::CoreSetPreferredNetworkType() - ERROR: Can't construct szCmd1.\r\n");
        goto Error;        
    }
    
    switch (nNetworkType)
    {
        case 0: // WCMDA Preferred
            if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+XRAT=1,2;+COPS=0\r", sizeof(rReqData.szCmd2) ))
            {
                RIL_LOG_CRITICAL("CTE_INF_N721::CoreSetPreferredNetworkType() - ERROR: Can't construct szCmd2 nNetworkType=%d\r\n", nNetworkType);
                goto Error;        
            }
           break;

        case 1: // GSM Only
            if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+XRAT=0,0;+COPS=0\r", sizeof(rReqData.szCmd2) ))
            {
                RIL_LOG_CRITICAL("CTE_INF_N721::CoreSetPreferredNetworkType() - ERROR: Can't construct szCmd2 nNetworkType=%d\r\n", nNetworkType);
                goto Error;        
            }
            break;

        case 2: // WCDMA Only
            if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+XRAT=2,2;+COPS=0\r", sizeof(rReqData.szCmd2) ))
            {
                RIL_LOG_CRITICAL("CTE_INF_N721::CoreSetPreferredNetworkType() - ERROR: Can't construct szCmd2 nNetworkType=%d\r\n", nNetworkType);
                goto Error;        
            }
            break;

        default:
            RIL_LOG_CRITICAL("CTE_INF_N721::CoreSetPreferredNetworkType() - ERROR: Undefined rat code: %d\r\n", nNetworkType);
            res = RIL_E_GENERIC_FAILURE;
            goto Error;
            break;
    }
    
    res = RRIL_RESULT_OK;
    
Error:    
    RIL_LOG_VERBOSE("CTE_INF_N721::CoreSetPreferredNetworkType() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_N721::ParseSetPreferredNetworkType(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_N721::ParseSetPreferredNetworkType() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_VERBOSE("CTE_INF_N721::ParseSetPreferredNetworkType() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
//
RIL_RESULT_CODE CTE_INF_N721::CoreGetPreferredNetworkType(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_N721::CoreGetPreferredNetworkType() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+XRAT?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }
    
    RIL_LOG_VERBOSE("CTE_INF_N721::CoreGetPreferredNetworkType() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_N721::ParseGetPreferredNetworkType(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_N721::ParseGetPreferredNetworkType() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;
    
    UINT32 rat = 0;
    UINT32 pref = 0;

    int * pRat = (int*)malloc(sizeof(int));
    if (NULL == pRat)
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseGetPreferredNetworkType() - ERROR: Could not allocate memory for response.\r\n");
        goto Error;
    }

    // Skip "<prefix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseGetPreferredNetworkType() - ERROR: Could not skip response prefix.\r\n");
        goto Error;
    }

    // Skip "+XRAT: "
    if (!SkipString(pszRsp, "+XRAT: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseGetPreferredNetworkType() - ERROR: Could not skip \"+XRAT: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt(pszRsp, rat, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseGetPreferredNetworkType() - ERROR: Could not extract rat value.\r\n");
        goto Error;
    }

    if (FindAndSkipString(pszRsp, ",", pszRsp))
    {
        if (!ExtractUInt(pszRsp, pref, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseGetPreferredNetworkType() - ERROR: Could not find and skip pref value even though it was expected.\r\n");
            goto Error;
        }
    }

    switch(rat)
    {
        case 0:     // GSM Only
        {
            pRat[0] = 1;
            break;
        }
    
        case 1:     // WCDMA Preferred
        {
            pRat[0] = 0;
            break;
        }
        
        case 2:     // WCDMA only
        {
            pRat[0] = 2;
            break;
        }

        default:
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseGetPreferredNetworkType() - ERROR: Unexpected rat found: %d. Failing out.\r\n", rat);
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


    RIL_LOG_VERBOSE("CTE_INF_N721::ParseGetPreferredNetworkType() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_GET_NEIGHBORING_CELL_IDS 75
//
RIL_RESULT_CODE CTE_INF_N721::CoreGetNeighboringCellIDs(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_N721::CoreGetNeighboringCellIDs() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+XCELLINFO?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }
    
    RIL_LOG_VERBOSE("CTE_INF_N721::CoreGetNeighboringCellIDs() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_N721::ParseGetNeighboringCellIDs(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_N721::ParseGetNeighboringCellIDs() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    BOOL fFoundRsp = FALSE;
    const BYTE* szDummy = pszRsp;
    UINT32 nType = 0;
    UINT32 nMCC = 0;
    UINT32 nFound = 0;
    UINT32 nIndex = 0;
    UINT32 nRssi = 0;
    UINT32 cbMemReq = 0;

    RIL_NeighboringCell ** pNCPtrs = NULL;
    RIL_NeighboringCell * pNCData = NULL;
    P_ND_CELL_ID_STRING pszCIDs = NULL;

    // Loop on +XCELLINFO until no more entries are found or we get 6 valid Cell IDs
    while (FindAndSkipString(szDummy, "+XCELLINFO: ", szDummy))
    {
        if ((!FindAndSkipString(szDummy, ",", szDummy)) ||
            (!ExtractUInt(szDummy, nType, szDummy)) ||
            (!FindAndSkipString(szDummy, ",", szDummy)) ||
            (!ExtractUInt(szDummy, nMCC, szDummy)))
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseGetNeighboringCellIDs() - ERROR: Could not extract type and MCC value.\r\n");
            goto Error;
        }

        if (MCC_DEFAULT_VALUE != nMCC)
        {
            nFound++;
        }
    }

    RIL_LOG_INFO("CTE_INF_N721::ParseGetNeighboringCellIDs() - INFO: Found %d entries\r\n", nFound);

    cbMemReq = nFound * (sizeof(RIL_NeighboringCell*) + 
                         sizeof(RIL_NeighboringCell) + 
                         sizeof(S_ND_CELL_ID_STRING));

    pNCPtrs = (RIL_NeighboringCell**)malloc(cbMemReq);
    pNCData = (RIL_NeighboringCell*)((UINT32)pNCPtrs + nFound * sizeof(RIL_NeighboringCell*));
    pszCIDs = (P_ND_CELL_ID_STRING)((UINT32)pNCData + nFound * sizeof(RIL_NeighboringCell));

    if (NULL == pNCPtrs)
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseGetNeighboringCellIDs() - ERROR: Could not allocate memory for response.\r\n");
        goto Error;
    }

    memset(pNCPtrs, 0, nFound * (cbMemReq));
    
    while (FindAndSkipString(pszRsp, "+XCELLINFO: ", pszRsp))
    {
        fFoundRsp = TRUE;

        if ((!FindAndSkipString(pszRsp, ",", pszRsp)) ||
            (!ExtractUInt(pszRsp, nType, pszRsp)) ||
            (!FindAndSkipString(pszRsp, ",", pszRsp)) ||
            (!ExtractUInt(pszRsp, nMCC, pszRsp)))
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseGetNeighboringCellIDs() - ERROR: Could not extract type and MCC value.\r\n");
            goto Error;
        }

        if (MCC_DEFAULT_VALUE == nMCC)
        {
            RIL_LOG_INFO("CTE_INF_N721::ParseGetNeighboringCellIDs() - INFO: Found invalid XCELLINFO. Skipping.\r\n");
            continue;
        }

        if ((!FindAndSkipString(pszRsp, ",", pszRsp)) ||
            (!FindAndSkipString(pszRsp, ",", pszRsp)) ||
            (!FindAndSkipString(pszRsp, ",", pszRsp)) ||
            (!ExtractUnquotedString(pszRsp, ",", pszCIDs[nIndex].szCellID, CELL_ID_ARRAY_LENGTH, pszRsp)))
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseGetNeighboringCellIDs() - ERROR: Could not extract Cell ID value.\r\n");
            goto Error;
        }

        // Link pointers for string
        pNCData[nIndex].cid = pszCIDs[nIndex].szCellID;

        if ((!FindAndSkipString(pszRsp, ",", pszRsp)) ||
            (!ExtractUInt(pszRsp, nRssi, pszRsp)))
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseGetNeighboringCellIDs() - ERROR: Could not extract RSSI value.\r\n");
            goto Error;
        }

        pNCData[nIndex].rssi = nRssi;

        // Link pointer for data
        pNCPtrs[nIndex] = &(pNCData[nIndex]);

        nIndex++;
    }

    if (!fFoundRsp)
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseGetNeighboringCellIDs() - ERROR: Didn't find +XCELLINFO in response.\r\n");
        goto Error;
    }

    rRspData.pData  = (void*)pNCPtrs;
    rRspData.uiDataSize = nFound * sizeof(RIL_NeighboringCell*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pNCPtrs);
        pNCPtrs = NULL;
    }


    RIL_LOG_VERBOSE("CTE_INF_N721::ParseGetNeighboringCellIDs() - Exit\r\n");
    return res;

}

