////////////////////////////////////////////////////////////////////////////
// te_inf_n721.cpp
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
#include "channel_data.h"
#include "atchannel.h"
#include "stk.h"
#include "rildmain.h"
#include "callbacks.h"

#include <cutils/properties.h>
#include <sys/system_properties.h>

//  This is for close().
#include <unistd.h>

//  This is for socket-related calls.
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/route.h>

extern "C" char *hex_to_stk_at(const char *at);

CTE_INF_N721::CTE_INF_N721()
{
}

CTE_INF_N721::~CTE_INF_N721()
{
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
RIL_RESULT_CODE CTE_INF_N721::CoreSetupDataCall(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize, UINT32 uiCID)
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
    // with +CGQREQ to specify requested QoS and +CGACT to activate PDP Context; then
    // if RAW IP is used send AT+CGDATA to enable Raw IP on data channel (which will then switch the channel to data mode).
    //
    (void)PrintStringNullTerminate(rReqData.szCmd1,
        sizeof(rReqData.szCmd1),
        "AT+CGDCONT=%d,\"IP\",\"%s\",,0,0;+CGQREQ=%d;+CGQMIN=%d;+CGACT=0,%d\r", uiCID,
        stPdpData.szApn, uiCID, uiCID, uiCID);

#ifndef RIL_USE_PPP       
   (void)PrintStringNullTerminate(rReqData.szCmd2, sizeof(rReqData.szCmd2), "AT+CGDATA=\"M-RAW_IP\",%d\r", uiCID);
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
    BYTE szCmd[MAX_BUFFER_SIZE];
    CChannel_Data* pDataChannel = NULL;

    // 1st confirm we got "CONNECT"
    const BYTE* szRsp = rRspData.szResponse;

    if (!FindAndSkipString(szRsp, "CONNECT", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseSetupDataCall() - ERROR: Did not get \"CONNECT\" response.\r\n");
        goto Error;
    }

    pDataChannel = CChannel_Data::GetChnlFromRilChannelNumber(rRspData.uiChannel);
    if (!pDataChannel)
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseSetupDataCall() - ERROR: Could not get Data Channel for RIL channel number %d.\r\n", rRspData.uiChannel);
        goto Error;
    }

    // Send AT+CGPADDR and AT+XDNS? commands to query for assigned IP Address and DNS and wait for responses

    CEvent::Reset(pDataChannel->m_pSetupDoneEvent);

    (void)PrintStringNullTerminate(szCmd, MAX_BUFFER_SIZE, "AT+CGPADDR=%d\r", pDataChannel->GetContextID());

    pCmd1 = new CCommand(RIL_CHANNEL_ATCMD, NULL, ND_REQ_ID_GETIPADDRESS, szCmd, &CTE::ParseIpAddress);
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

    uiWaitRes = CEvent::Wait(pDataChannel->m_pSetupDoneEvent, 30000); // TODO: put timeout in repository to allow tweaking?
    switch (uiWaitRes) 
    {
        case WAIT_EVENT_0_SIGNALED:
            RIL_LOG_INFO("CTE_INF_N721::ParseSetupDataCall() : SetupData event signalled\r\n");
            if (NULL == pDataChannel->m_szIpAddr)
            {
                 RIL_LOG_CRITICAL("CTE_INF_N721::ParseSetupDataCall() - ERROR: IP address is null\r\n");
                 goto Error;
            }
            strcpy(szIP, pDataChannel->m_szIpAddr);
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
    sprintf(pDataCallRsp->szCID, "%d", pDataChannel->GetContextID());
    strcpy(pDataCallRsp->szNetworkInterfaceName, "ifx02");
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


    
    //  route add default dev ifx02
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
    CChannel_Data* pDataChannel = NULL;

    // Parse prefix
    if (!FindAndSkipString(szRsp, "+CGPADDR: ", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseIpAddress() - ERROR: Unable to parse \"+CGPADDR\" prefix.!\r\n");
        goto Error;
    }

    // Parse <cid>
    if (!ExtractUInt(szRsp, nCid, szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseIpAddress() - ERROR: Unable to parse <cid>!\r\n");
        goto Error;
    }
    
    pDataChannel = CChannel_Data::GetChnlFromContextID(nCid);
    if (!pDataChannel)
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseSetupDataCall() - ERROR: Could not get Data Channel for Context ID %d.\r\n", nCid);
        goto Error;
    }

    if (pDataChannel->m_szIpAddr)
    {
        delete[] pDataChannel->m_szIpAddr;
        pDataChannel->m_szIpAddr = NULL;
    }

    // Parse <PDP_addr>
    if (!SkipString(szRsp, ",", szRsp) ||
        !ExtractQuotedStringWithAllocatedMemory(szRsp, pDataChannel->m_szIpAddr, cbIpAddr, szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseIpAddress() - ERROR: Unable to parse <PDP_addr>!\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_INF_N721::ParseIpAddress() - IP address: %s\r\n", pDataChannel->m_szIpAddr);

    // set net. properties
    // TODO For multiple PDP context support this will have to be updated to be consistent with whatever changes to
    // the Android framework's use of these properties is made. (Currently there is only one property for the
    // IP address and there needs to be one for each context.  
    property_set("net.interfaces.defaultroute", "gprs");
    property_set("net.gprs.local-ip", pDataChannel->m_szIpAddr);
    property_set("net.gprs.operstate", "up");

    // invoke netcfg commands

    // TODO - Note it is currently assumed here that the interface channel number is the context ID + 1
    nChannel = nCid + 1; 

    snprintf(szInterfaceName, sizeof(szInterfaceName), "ifx%02d", nChannel);
    
    if (!CallDataConfigCmds(szInterfaceName, pDataChannel->m_szIpAddr))
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
    CChannel_Data* pDataChannel = NULL;

    // Parse prefix
    if (!FindAndSkipString(szRsp, "+XDNS: ", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseDns() - ERROR: Unable to parse \"+XDNS\" prefix.!\r\n");
        goto Error;
    }

    // Parse <cid>
    if (!ExtractUInt(szRsp, nCid, szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseDns() - ERROR: Unable to parse <cid>!\r\n");
        goto Error;
    }

    pDataChannel = CChannel_Data::GetChnlFromContextID(nCid);
    if (!pDataChannel)
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseSetupDataCall() - ERROR: Could not get Data Channel for Context ID %d.\r\n", nCid);
        goto Error;
    }

    // TBD for multiple PDP Contexts whether we always use the same DNS values

    // Parse <primary DNS>
    if (!SkipString(szRsp, ", ", szRsp) ||
        !ExtractQuotedStringWithAllocatedMemory(szRsp, szDns1, cbDns1, szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseDns() - ERROR: Unable to parse <primary DNS>!\r\n");
        goto Error;
    }
    RIL_LOG_INFO("CTE_INF_N721::ParseDns() - setting 'net.gprs.dns1' to: %s\r\n", szDns1);
    property_set("net.gprs.dns1", szDns1);

    // Parse <secondary DNS>
    if (!SkipString(szRsp, ", ", szRsp) ||
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
    if (pDataChannel)
        CEvent::Signal(pDataChannel->m_pSetupDoneEvent);

    if (szDns1) delete[] szDns1;
    if (szDns2) delete[] szDns2;

    RIL_LOG_INFO("CTE_INF_N721::ParseDns() - Exit\r\n");
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
RIL_RESULT_CODE CTE_INF_N721::ParseSimIo(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_N721::ParseSimIo() - Enter\r\n");
    
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;
    
    uint  uiSW1 = 0;
    uint  uiSW2 = 0;
    BYTE* szResponseString = NULL;
    UINT32  cbResponseString = 0;

    RIL_SIM_IO_Response* pResponse = NULL;
    

    if (NULL == rRspData.szResponse)
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseSimIo() - ERROR: Response String pointer is NULL.\r\n");
        goto Error;
    }

    // Parse "<prefix>+CRSM: <sw1>,<sw2>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseSimIo() - ERROR: Could not skip over response prefix.\r\n");
        goto Error;
    }

    if (!SkipString(pszRsp, "+CRSM: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseSimIo() - ERROR: Could not skip over \"+CRSM: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt(pszRsp, uiSW1, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseSimIo() - ERROR: Could not extract SW1 value.\r\n");
        goto Error;
    }

    if (!SkipString(pszRsp, ",", pszRsp) ||
        !ExtractUInt(pszRsp, uiSW2, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseSimIo() - ERROR: Could not extract SW2 value.\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_INF_N721::ParseSimIo() - Extracted SW1 = %u and SW2 = %u\r\n", uiSW1, uiSW2);

    // Parse ","
    if (SkipString(pszRsp, ",", pszRsp))
    {
        // Parse <response>
        // NOTE: we take ownership of allocated szResponseString
        if (!ExtractQuotedStringWithAllocatedMemory(pszRsp, szResponseString, cbResponseString, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseSimIo() - ERROR: Could not extract data string.\r\n");
            goto Error;
        }
        else
        {
            RIL_LOG_INFO("CTE_INF_N721::ParseSimIo() - Extracted data string: \"%s\" (%u chars including NULL)\r\n", szResponseString, cbResponseString);
        }

        if (0 != (cbResponseString - 1) % 2)
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseSimIo() : ERROR : String was not a multiple of 2.\r\n");
            goto Error;
        }
        
        // Check for USIM GET_RESPONSE response
        int nCRSMCommand = (int)rRspData.pContextData;
        if ((192 == nCRSMCommand) && ('6' == szResponseString[0]) && ('2' == szResponseString[1]))
        {
            //  USIM GET_RESPONSE response
            RIL_LOG_INFO("CTE_INF_N721::ParseSimIo() - USIM GET_RESPONSE\r\n");
            
            char szTemp[5] = {0};

            struct sUSIMGetResponse sUSIM;
            memset(&sUSIM, 0, sizeof(struct sUSIMGetResponse));
            
            //  Need to convert the response string from ascii to binary.
            BYTE *sNewString = NULL;
            UINT32 cbNewString = (cbResponseString-1)/2;
            sNewString = new BYTE[cbNewString];
            if (NULL == sNewString)
            {
                RIL_LOG_CRITICAL("CTE_INF_N721::ParseSimIo() - ERROR: Cannot create new string!\r\n");
                goto Error;
            }
            memset(sNewString, 0, cbNewString);
            
            UINT32 cbUsed = 0;
            if (!GSMHexToGSM(szResponseString, cbResponseString, sNewString, cbNewString, cbUsed))
            {
                RIL_LOG_CRITICAL("CTE_INF_N721::ParseSimIo() - ERROR: Cannot cconvert szResponseString to GSMHex.\r\n");
                delete[] sNewString;
                sNewString = NULL;
                cbNewString = 0;
                goto Error;
            }
            
            //  OK, now parse!
            if (!ParseUSIMRecordStatus((UINT8*)sNewString, cbNewString, &sUSIM))
            {
                RIL_LOG_CRITICAL("CTE_INF_N721::ParseSimIo() - ERROR: Cannot parse USIM record status\r\n");
                delete[] sNewString;
                sNewString = NULL;
                cbNewString = 0;
                goto Error;
            }
            
                        
            delete[] sNewString;
            sNewString = NULL;
            cbNewString = 0;
            
            RIL_LOG_VERBOSE("CTE_INF_N721::ParseSimIo() - sUSIM.dwRecordType=[0x%04X]\r\n", sUSIM.dwRecordType);
            RIL_LOG_VERBOSE("CTE_INF_N721::ParseSimIo() - sUSIM.dwTotalSize=[0x%04X]\r\n", sUSIM.dwTotalSize);
            RIL_LOG_VERBOSE("CTE_INF_N721::ParseSimIo() - sUSIM.dwRecordSize=[0x%04X]\r\n", sUSIM.dwRecordSize);
            

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
                RIL_LOG_CRITICAL("CTE_INF_N721::ParseSimIo() - ERROR: Cannot create new szResponsestring!\r\n");
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
                RIL_LOG_CRITICAL("CTE_INF_N721::ParseSimIo() - ERROR: sUSIM.dwRecordType is unknown!\r\n");
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
            RIL_LOG_INFO("CTE_INF_N721::ParseSimIo() - new USIM response=[%s]\r\n", szResponseString);

        }

    }

    // Allocate memory for the response struct PLUS a buffer for the response string
    // The char* in the RIL_SIM_IO_Response will point to the buffer allocated directly after the RIL_SIM_IO_Response
    // When the RIL_SIM_IO_Response is deleted, the corresponding response string will be freed as well.
    // Note: cbResponseString is the number of chars in the string, and does not include the NULL terminator.
    pResponse = (RIL_SIM_IO_Response*)malloc(sizeof(RIL_SIM_IO_Response) + cbResponseString + 1);
    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseSimIo() - ERROR: Could not allocate memory for a RIL_SIM_IO_Response struct.\r\n");
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

    RIL_LOG_VERBOSE("CTE_INF_N721::ParseSimIo() - Exit\r\n");
    return res;
}



//
// RIL_REQUEST_SEND_USSD 29
//
RIL_RESULT_CODE CTE_INF_N721::ParseSendUssd(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_N721::ParseSendUssd() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const BYTE* szRsp = rRspData.szResponse;
    P_ND_USSD_STATUS pUssdStatus = NULL;
    
    UINT32 uiStatus = 0;
    BYTE* szDataString = NULL;
    UINT32 uiDataString = 0;
    UINT32 nDCS = 0;
    //BOOL bSupported = FALSE;
    WCHAR* szUcs2 = NULL;
    UINT32 cbUsed = 0;
    
    //  NOTE: This modem currently does USSD synchronously.  Android expects asynchronous operation.
    if (!FindAndSkipString(szRsp, "+CUSD: ", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseSendUssd() - ERROR: Couldn't find CUSD\r\n");
        goto Error;
    }
    
    // Extract "<status>"
    if (!ExtractUInt(szRsp, uiStatus, szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseSendUssd() - ERROR: Couldn't extract status\r\n");
        goto Error;
    }
    
    // Skip ","
    if (!SkipString(szRsp, ",", szRsp))
    {
        // No data parameter present
        pUssdStatus = (P_ND_USSD_STATUS) malloc(sizeof(S_ND_USSD_STATUS));
        if (!pUssdStatus)
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseSendUssd() - ERROR: Couldn't malloc S_ND_USSD_STATUS\r\n");
            goto Error;
        }
        snprintf(pUssdStatus->szType, 2, "%d", (int) uiStatus);
        pUssdStatus->sStatusPointers.pszType    = pUssdStatus->szType;
        pUssdStatus->sStatusPointers.pszMessage = NULL;
    }
    else
    {
        // Extract "<data>,<dcs>"
        // NOTE: we take ownership of allocated szDataString
        if (!ExtractQuotedStringWithAllocatedMemory(szRsp, szDataString, uiDataString, szRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseSendUssd() - ERROR: Couldn't ExtractQuotedStringWithAllocatedMemory\r\n");
            goto Error;
        }
        if (!SkipString(szRsp, ",", szRsp) ||
            !ExtractUInt(szRsp, nDCS, szRsp))
        {
            // default nDCS.  this parameter is supposed to be mandatory but we've
            // seen it missing from otherwise normal USSD responses.
            nDCS = 0;
        }
        

#if 0
        // See GSM 03.38 for Cell Broadcast DCS details
        // Currently support only default alphabet in all languages (i.e., the DCS range 0x00..0x0f)
        // check if we support this DCS
        if (nDCS <= 0x0f)
        {
            bSupported = TRUE;
        }
        else if ((0x040 <= nDCS) && (nDCS <= 0x05f))
        {
            /* handle coding groups of 010* (binary)  */
            /* only character set is of interest here */
        
            /* refer to 3GPP TS 23.038 5 and 3GPP TS 27.007 7.15 for detail */
            UINT32 uDCSCharset = ( ( nDCS >> 2 ) & 0x03 );
            if ( 0x00 == uDCSCharset )
            {
                // GSM 7 bit default alphabet
                if ( ENCODING_TECHARSET == ENCODING_GSMDEFAULT_HEX )
                {
                    // not supported
                    bSupported = FALSE;
                }
            }
            else if ( 0x01 == uDCSCharset )
            {
                // 8 bit data
                // not supported
                bSupported = FALSE;
            }
            else if ( 0x02 == uDCSCharset )
            {
                // UCS2 (16 bit)
                // supported
                bSupported = TRUE;
            }
            else if ( 0x03 == uDCSCharset )
            {
                // reserved
                RIL_LOG_CRITICAL("CTE_INF_N721::ParseSendUssd() - ERROR: Unsupported value 0x03\r\n");
                // not supported
                 bSupported = FALSE;
            }
            else
            {
                /* should not reach here. just in case */
                RIL_LOG_CRITICAL("CTE_INF_N721::ParseSendUssd() - ERROR: Unsupported value [%d]\r\n", uDCSCharset);

                // not supported
                bSupported = FALSE;
            }
        }

        if (!bSupported)
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseSendUssd() - ERROR: Unsupported nDCS=[%d] bSupported=[%d]\r\n", nDCS, bSupported);
            goto Error;
        }
        /*--- currently we only handle the default alphabet encoding ---*/

        // Default alphabet encoding (which has been requested in our init string to allow direct copying of bytes)
        // At this point, uiDataString has the size of the szDataString which ExtractQuotedStringWithAllocatedMemory allocated for us and
        // which holds the TE-encoded characters.
        // However, this byte count also includes an extra byte for the NULL character at the end, which we're not going to translate.
        // Therefore, decrement uiDataString by 1
        uiDataString--;

        // Now figure out the number of Unicode characters needed to hold the string after it's converted
        size_t cbUnicode;
        if (!ConvertSizeToUnicodeSize(ENCODING_TECHARSET, uiDataString, cbUnicode))
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseSendUssd() - ERROR: ConvertSizeToUnicodeSize failed\r\n");
            goto Error;
        }
        
        RIL_LOG_INFO("CTE_INF_N721::ParseSendUssd() - uiDataString=[%d] cbUnicode=[%d]\r\n", uiDataString, cbUnicode);

        // cbUnicode now has the number of bytes we need to hold the Unicode string generated when we
        // convert from the TE-encoded string.
        // However, it doesn't include space for the terminating NULL, so we need to allow for that also.
        // (Note that ParseEncodedString will append the terminating NULL for us automatically.)
        cbUnicode += sizeof(WCHAR);

        // Allocate buffer for conversion
        szUcs2 = new WCHAR[cbUnicode];
        if (!szUcs2)
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseSendUssd() - ERROR: new WCHAR failed  cbUnicode=[%d]\r\n", cbUnicode);
            goto Error;
        }

        if (!ConvertGSMStringToUnicodeString(szDataString, uiDataString, szUcs2, cbUnicode, cbUsed))
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseSendUssd() - ERROR: ConvertGSMStringToUnicodeString failed\r\n");
            goto Error;
        }

        // Now convert the TE-encoded string into Unicode in the buffer we just allocated
        if (!ExtractUnquotedUnicodeHexStringToUnicode(szDataString,
                                                      uiDataString,
                                                      szUcs2,
                                                      cbUnicode))
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseSendUssd() - ERROR: ExtractUnquotedUnicodeHexStringToUnicode failed\r\n");
            goto Error;
        }

        // allocate blob and convert from UCS2 to UTF8
        pUssdStatus = (P_ND_USSD_STATUS) malloc(sizeof(S_ND_USSD_STATUS));
        if (!pUssdStatus)
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseSendUssd() - ERROR: malloc failed\r\n");
            goto Error;
        }
        snprintf(pUssdStatus->szType, 2, "%d", (int) uiStatus);
        if (!ConvertUnicodeToUtf8(szUcs2, pUssdStatus->szMessage, MAX_BUFFER_SIZE))
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseSendUssd() - ERROR: ConvertUnicodeToUtf8 failed\r\n");
            goto Error;
        }
#endif // 0


        //  Allocate blob.
        pUssdStatus = (P_ND_USSD_STATUS) malloc(sizeof(S_ND_USSD_STATUS));
        if (!pUssdStatus)
        {
            RIL_LOG_CRITICAL("CTE_INF_N721::ParseSendUssd() - ERROR: malloc failed\r\n");
            goto Error;
        }
        snprintf(pUssdStatus->szType, 2, "%d", (int) uiStatus);
        (void)CopyStringNullTerminate(pUssdStatus->szMessage, szDataString, MAX_BUFFER_SIZE*2);



        RIL_LOG_INFO("TE_INF_N721::ParseSendUssd() - %s\r\n", pUssdStatus->szMessage);
        
        pUssdStatus->sStatusPointers.pszType    = pUssdStatus->szType;
        pUssdStatus->sStatusPointers.pszMessage = pUssdStatus->szMessage;
    }
    
    
    //  Request notification 0.5 seconds later.
    RIL_requestTimedCallback(triggerUSSDNotification, (void*)pUssdStatus, 0, 500000);
    
    //  Send notification since modem does this synchronously.

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pUssdStatus);
        pUssdStatus = NULL;
    }

    delete[] szDataString;
    szDataString = NULL;
    delete[] szUcs2;
    szUcs2 = NULL;
    
    
    RIL_LOG_VERBOSE("CTE_INF_N721::ParseSendUssd() - Exit\r\n");
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
    
    //  Set the context of this command to the CID (for multiple context support).
    rReqData.pContextData = (void*)((int)(chCid - '0'));  // Store this as an int.
    

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
    // TODO For multiple PDP context support this will have to be updated to be consistent with whatever changes to
    // the Android framework's use of these properties is made.
    property_set("net.interfaces.defaultroute", "");
    property_set("net.gprs.dns1", "");
    property_set("net.gprs.dns2", "");
    property_set("net.gprs.local-ip", "");
    property_set("net.gprs.operstate", "down");
    property_set("gsm.net.interface","");

    res = RRIL_RESULT_OK;

#endif //RIL_USE_PPP

Error:
    RIL_LOG_INFO("CTE_INF_N721::ParseDeactivateDataCall() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SET_MUTE 53
//
RIL_RESULT_CODE CTE_INF_N721::CoreSetMute(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_N721::CoreSetMute() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    //  [DP] Let's NO-OP this command.
    rReqData.szCmd1[0] = NULL;

    RIL_LOG_VERBOSE("CTE_INF_N721::CoreSetMute() - Exit\r\n");
    return res;    
}


RIL_RESULT_CODE CTE_INF_N721::ParseSetMute(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_N721::ParseSetMute() - Enter\r\n");
    RIL_LOG_VERBOSE("CTE_INF_N721::ParseSetMute() - Exit\r\n");
    return RRIL_RESULT_OK;
}



//
// RIL_REQUEST_SCREEN_STATE 61
//
RIL_RESULT_CODE CTE_INF_N721::CoreScreenState(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_N721::CoreScreenState() - Enter\r\n");
    
    // TODO : Mark flags for unsol updates for LAC, CID, CSQ to not send updates
    //        when screen is off. Resume updates when screen turns back on.
    //        will probably need to trigger immediate updates on resume to have correct
    //        info on screen. For now, just return OK and keep throwing updates.
    //
    //  Note that we added +XREG=1 during init.
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int nEnable = 0;
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::CoreScreenState() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    nEnable = ((int *)pData)[0];
    
    //  0 is off
    //  1 is on
    switch(nEnable)
    {
        case 0:
            if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CREG=0;+CGREG=0;+XREG=0\r", sizeof(rReqData.szCmd1)))
            {
                res = RRIL_RESULT_OK;
            }
            break;
        case 1:
            if (CopyStringNullTerminate(rReqData.szCmd1, "AT+CREG=2;+CGREG=2;+XREG=1\r", sizeof(rReqData.szCmd1)))
            {
                res = RRIL_RESULT_OK;
            }
            break;
        default:
            RIL_LOG_CRITICAL("CTE_INF_N721::CoreScreenState() - ERROR: unknown nEnable=[%d]\r\n", nEnable);
            break;
    }

Error:
    RIL_LOG_VERBOSE("CTE_INF_N721::CoreScreenState() - Exit\r\n");
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
// RIL_REQUEST_STK_GET_PROFILE 67
//
RIL_RESULT_CODE CTE_INF_N721::CoreStkGetProfile(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_INFO("CTE_INF_N721::CoreStkGetProfile() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+STKPROF?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }
    
    RIL_LOG_INFO("CTE_INF_N721::CoreStkGetProfile() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_N721::ParseStkGetProfile(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_INF_N721::ParseStkGetProfile() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* pszRsp = rRspData.szResponse;
    char* pszTermProfile = NULL;
    UINT32 uiLength = 0;
     
    if (NULL == pszRsp)
    {
        RIL_LOG_INFO("CTE_INF_N721::ParseStkGetProfile() - ERROR: Response string is NULL!\r\n");
        goto Error;
    }    
    
    pszTermProfile = (char*)malloc(MAX_BUFFER_SIZE);
    if (NULL == pszTermProfile)
    {
        RIL_LOG_INFO("CTE_INF_N721::ParseStkGetProfile() - ERROR: Could not allocate memory for a %u-char string.\r\n", MAX_BUFFER_SIZE);
        goto Error;
    }    

    memset(pszTermProfile, 0x00, MAX_BUFFER_SIZE);

    // Parse "<prefix>+STKPROF: <length>,<data><postfix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_INFO("CTE_INF_N721::ParseStkGetProfile() - ERROR: Could not skip response prefix.\r\n");
        goto Error;
    }

    if (!SkipString(pszRsp, "+STKPROF: ", pszRsp))
    {
        RIL_LOG_INFO("CTE_INF_N721::ParseStkGetProfile() - ERROR: Could not skip \"+STKPROF: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt(pszRsp, uiLength, pszRsp))
    {
        RIL_LOG_INFO("CTE_INF_N721::ParseStkGetProfile() - ERROR: Could not extract length value.\r\n");
        goto Error;
    }

    if (SkipString(pszRsp, ",", pszRsp))
    {
        if (!ExtractQuotedString(pszRsp, pszTermProfile, MAX_BUFFER_SIZE, pszRsp))
        {
            RIL_LOG_INFO("CTE_INF_N721::ParseStkGetProfile() - ERROR: Could not extract the terminal profile.\r\n");
            goto Error;
        }
    }

    if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_INFO("CTE_INF_N721::ParseStkGetProfile() - ERROR: Could not extract the response end.\r\n");
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

    RIL_LOG_INFO("CTE_INF_N721::ParseStkGetProfile() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_SET_PROFILE 68
//
RIL_RESULT_CODE CTE_INF_N721::CoreStkSetProfile(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_INFO("CTE_INF_N721::CoreStkSetProfile() - Enter\r\n");
    char* pszTermProfile = NULL;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (0 == uiDataSize)
    {
        RIL_LOG_INFO("CTE_INF_N721::CoreStkSetProfile() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_INFO("CTE_INF_N721::CoreStkSetProfile() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    // extract data
    pszTermProfile = (char*)pData;
    
    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+STKPROF=%u,\"%s\"\r", uiDataSize, pszTermProfile))
    {
        RIL_LOG_INFO("CTE_INF_N721::CoreStkSetProfile() - ERROR: Could not form string.\r\n");
        goto Error;        
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_INFO("CTE_INF_N721::CoreStkSetProfile() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_N721::ParseStkSetProfile(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_INF_N721::ParseStkSetProfile() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_INFO("CTE_INF_N721::ParseStkSetProfile() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND 69
//
RIL_RESULT_CODE CTE_INF_N721::CoreStkSendEnvelopeCommand(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_INFO("CTE_INF_N721::CoreStkSendEnvelopeCommand() - Enter\r\n");
    char* pszEnvCommand = NULL;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (sizeof(char*) != uiDataSize)
    {
        RIL_LOG_INFO("CTE_INF_N721::CoreStkSendEnvelopeCommand() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_INFO("CTE_INF_N721::CoreStkSendEnvelopeCommand() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    // extract data
    pszEnvCommand = (char*)pData;
    
    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+SATE=\"%s\"\r", pszEnvCommand))
    {
        RIL_LOG_INFO("CTE_INF_N721::CoreStkSendEnvelopeCommand() - ERROR: Could not form string.\r\n");
        goto Error;        
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_INFO("CTE_INF_N721::CoreStkSendEnvelopeCommand() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_N721::ParseStkSendEnvelopeCommand(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_INF_N721::ParseStkSendEnvelopeCommand() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_INFO("CTE_INF_N721::ParseStkSendEnvelopeCommand() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE 70
//
RIL_RESULT_CODE CTE_INF_N721::CoreStkSendTerminalResponse(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_INFO("CTE_INF_N721::CoreStkSendTerminalResponse() - Enter\r\n");
    char* pszTermResponse = NULL;
#ifndef USE_STK_RAW_MODE      
    char* cmd = NULL; 
#endif
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    if (NULL == pData)
    {
        RIL_LOG_INFO("CTE_INF_N721::CoreStkSendTerminalResponse() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != sizeof(char *))
    {
        RIL_LOG_INFO("CTE_INF_N721::CoreStkSendTerminalResponse() - ERROR: Invalid data size.\r\n");
        goto Error;
    }
    
    RIL_LOG_INFO(" uiDataSize= %d\r\n", uiDataSize);
    
    pszTermResponse = (char *)pData;

#if USE_STK_RAW_MODE    
    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+SATR=\"%s\"\r", pszTermResponse))
    {
        RIL_LOG_INFO("CTE_INF_N721::CoreStkSendTerminalResponse() - ERROR: Could not form string.\r\n");
        goto Error;
    }
#else    
    cmd = hex_to_stk_at(pszTermResponse);
    
    if (!CopyStringNullTerminate(rReqData.szCmd1, cmd, sizeof(rReqData.szCmd1)))
    {
        RIL_LOG_INFO("CTE_INF_N721::CoreStkSendTerminalResponse() - ERROR: Could not form string.\r\n");
        goto Error;        
    }     
#endif
    
    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_INFO("CTE_INF_N721::CoreStkSendTerminalResponse() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_N721::ParseStkSendTerminalResponse(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_INF_N721::ParseStkSendTerminalResponse() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_INFO("CTE_INF_N721::ParseStkSendTerminalResponse() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM 71
//
RIL_RESULT_CODE CTE_INF_N721::CoreStkHandleCallSetupRequestedFromSim(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_INFO("CTE_INF_N721::CoreStkHandleCallSetupRequestedFromSim() - Enter\r\n");
    int nConfirmation = 0;
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    if (NULL == pData)
    {
        RIL_LOG_INFO("CTE_INF_N721::CoreStkHandleCallSetupRequestedFromSim() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != 1)
    {
        RIL_LOG_INFO("CTE_INF_N721::CoreStkHandleCallSetupRequestedFromSim() - ERROR: Invalid data size.\r\n");
        goto Error;
    }
    
    nConfirmation = ((int *)pData)[0];
    if (0 == nConfirmation)
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+SATD=0\r"))
        {
            RIL_LOG_INFO("CTE_INF_N721::CoreStkHandleCallSetupRequestedFromSim() - ERROR: Could not form string.\r\n");
            goto Error;
        }        
    }
    else
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+SATD=1\r"))
        {
            RIL_LOG_INFO("CTE_INF_N721::CoreStkHandleCallSetupRequestedFromSim() - ERROR: Could not form string.\r\n");
            goto Error;
        }        
    }
        
    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_INFO("CTE_INF_N721::CoreStkHandleCallSetupRequestedFromSim() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_N721::ParseStkHandleCallSetupRequestedFromSim(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_INF_N721::ParseStkHandleCallSetupRequestedFromSim() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    RIL_LOG_INFO("CTE_INF_N721::ParseStkHandleCallSetupRequestedFromSim() - Exit\r\n");
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

//
// RIL_REQUEST_SET_TTY_MODE 80
//
RIL_RESULT_CODE CTE_INF_N721::CoreSetTtyMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{    
    RIL_LOG_INFO("CTE_INF_N721::CoreSetTtyMode() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int nTtyMode = 0;

    if (sizeof(int*) != uiDataSize)
    {
        RIL_LOG_INFO("CTE_INF_N721::CoreSetTtyMode() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_INFO("CTE_INF_N721::CoreSetTtyMode() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    // extract the data
    nTtyMode = ((int*)pData)[0];

    // check for invalid value
    if ((nTtyMode < 0) || (nTtyMode > 3))
    {
        RIL_LOG_INFO("CTE_INF_N721::CoreSetTtyMode() - ERROR: Undefined CTM/TTY mode: %d\r\n", nTtyMode);
        res = RIL_E_GENERIC_FAILURE;  
        goto Error;      
    }

    if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XCTMS=%d\r", nTtyMode))
    {
        RIL_LOG_INFO("CTE_INF_N721::CoreSetTtyMode() - ERROR: Could not form string.\r\n");
        goto Error;        
    }

    res = RRIL_RESULT_OK;
Error:
    RIL_LOG_INFO("CTE_INF_N721::CoreSetTtyMode() - Exit\r\n");
    return res;          
}

RIL_RESULT_CODE CTE_INF_N721::ParseSetTtyMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_N721::ParseSetTtyMode() - Enter\r\n");
    RIL_LOG_VERBOSE("CTE_INF_N721::ParseSetTtyMode() - Exit\r\n");
    
    return RRIL_RESULT_OK;    
}

//
// RIL_REQUEST_QUERY_TTY_MODE 81
//
RIL_RESULT_CODE CTE_INF_N721::CoreQueryTtyMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_INFO("CTE_INF_N721::CoreQueryTtyMode() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+XCTMS?\r"))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_INFO("CTE_INF_N721::CoreQueryTtyMode() - Exit\r\n");
    return res;    
}

RIL_RESULT_CODE CTE_INF_N721::ParseQueryTtyMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_INF_N721::ParseQueryTtyMode() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const BYTE* szRsp = rRspData.szResponse;
    UINT32 uiTtyMode = 0;

    int* pnMode = (int*)malloc(sizeof(int));
    if (NULL == pnMode)
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseQueryTtyMode() - ERROR: Could not allocate memory for response.\r\n");
        goto Error;
    }

    // Parse prefix
    if (!FindAndSkipString(szRsp, "+XCTMS: ", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseQueryTtyMode() - ERROR: Unable to parse \"+XCTMS\" prefix.!\r\n");
        goto Error;
    }

    // Parse <mode>
    if (!ExtractUInt(szRsp, uiTtyMode, szRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_N721::ParseQueryTtyMode() - ERROR: Unable to parse <mode>!\r\n");
        goto Error;
    }

    pnMode[0] = (int)uiTtyMode;
    
    rRspData.pData  = (void*)pnMode;
    rRspData.uiDataSize = sizeof(int*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pnMode);
        pnMode = NULL;
    }
    RIL_LOG_INFO("CTE_INF_N721::ParseQueryTtyMode() - Exit\r\n");
    return res;    
}
