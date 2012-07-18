////////////////////////////////////////////////////////////////////////////
// data_util.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides implementations for the RIL Utility functions specific to
//    data.
/////////////////////////////////////////////////////////////////////////////

//  This is for close().
#include <unistd.h>
#include <wchar.h>

//  This is for socket-related calls.
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/gsmmux.h>

#include <errno.h>

#include "data_util.h"
#include "repository.h"
#include "types.h"
#include "rillog.h"
#include "channel_data.h"
#include "extract.h"
#include "util.h"

int MapErrorCodeToRilDataFailCause(UINT32 uiCause)
{
    RIL_LOG_VERBOSE("MapErrorCodeToRilDataFailCause() - Enter\r\n");

    switch (uiCause)
    {
        case RRIL_CEER_CAUSE_OPERATOR_DETERMINED_BARRING:
            return PDP_FAIL_OPERATOR_BARRED;
        case RRIL_CEER_CAUSE_INSUFFICIENT_RESOURCES:
            return PDP_FAIL_INSUFFICIENT_RESOURCES;
        case RRIL_CEER_CAUSE_MISSING_UNKNOWN_APN:
            return PDP_FAIL_MISSING_UKNOWN_APN;
        case RRIL_CEER_CAUSE_UNKNOWN_PDP_ADDRESS_TYPE:
            return PDP_FAIL_UNKNOWN_PDP_ADDRESS_TYPE;
        case RRIL_CEER_CAUSE_USER_AUTHENTICATION_FAILED:
            return PDP_FAIL_USER_AUTHENTICATION;
        case RRIL_CEER_CAUSE_ACTIVATION_REJECTED_BY_GGSN:
            return PDP_FAIL_ACTIVATION_REJECT_GGSN;
        case RRIL_CEER_CAUSE_ACTIVATION_REJECT_UNSPECIFIED:
            return PDP_FAIL_ACTIVATION_REJECT_UNSPECIFIED;
        case RRIL_CEER_CAUSE_OPTION_NOT_SUPPORTED:
            return PDP_FAIL_SERVICE_OPTION_NOT_SUPPORTED;
        case RRIL_CEER_CAUSE_OPTION_NOT_SUBSCRIBED:
            return PDP_FAIL_SERVICE_OPTION_NOT_SUBSCRIBED;
        case RRIL_CEER_CAUSE_OPTION_TEMP_OUT_OF_ORDER:
            return PDP_FAIL_SERVICE_OPTION_OUT_OF_ORDER;
        case RRIL_CEER_CAUSE_NSPAI_ALREADY_USED:
            return PDP_FAIL_NSAPI_IN_USE;
        case RRIL_CEER_CAUSE_PDP_AUTHENTICATION_FAILURE:
            return PDP_FAIL_USER_AUTHENTICATION;
        default:
            return PDP_FAIL_ERROR_UNSPECIFIED;
    }
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
        RIL_LOG_CRITICAL("setaddr6() : SIOCSIFADDR : %s\r\n",
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
        RIL_LOG_CRITICAL("setaddr() : SIOCSIFADDR : %s\r\n",
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
        RIL_LOG_CRITICAL("setflags : SIOCGIFFLAGS : %s\r\n", strerror(errno));
        return FALSE;
    }

    ifr->ifr_flags = (ifr->ifr_flags & (~clr)) | set;
    RIL_LOG_INFO("setflags - calling SIOCGIFFLAGS 2\r\n");
    ret = ioctl(s, SIOCSIFFLAGS, ifr);
    if (ret < 0)
    {
        RIL_LOG_CRITICAL("setflags: SIOCSIFFLAGS 2 : %s\r\n", strerror(errno));
        return FALSE;
    }
    return TRUE;
}

static BOOL setmtu(int s, struct ifreq *ifr)
{
    ifr->ifr_mtu = g_MTU;
    RIL_LOG_INFO("setmtu - calling SIOCSIFMTU\r\n");
    if(ioctl(s, SIOCSIFMTU, ifr) < 0)
    {
        RIL_LOG_CRITICAL("setmtu: ERROR: SIOCSIFMTU\r\n");
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

        case PDP_TYPE_IPV6:
            RIL_LOG_INFO("DataConfigUp() - IPV6 - Calling DataConfigUpIpV6()\r\n");
            bRet = DataConfigUpIpV6(szNetworkInterfaceName, pChannelData);
            break;

        case PDP_TYPE_IPV4V6:
            RIL_LOG_INFO("DataConfigUp() - IPV4V6 - Calling DataConfigUpIpV4V6()\r\n");
            bRet = DataConfigUpIpV4V6(szNetworkInterfaceName, pChannelData);
            break;

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
    char *szIpAddr = pChannelData->m_szIpAddr;
    char *szDNS1 = pChannelData->m_szDNS1;
    char *szDNS2 = pChannelData->m_szDNS2;


    RIL_LOG_INFO("DataConfigUpIpV4() ENTER  szNetworkInterfaceName=[%s]  szIpAddr=[%s]\r\n", szNetworkInterfaceName, szIpAddr);
    RIL_LOG_INFO("DataConfigUpIpV4() ENTER  szDNS1=[%s]  szDNS2=[%s]\r\n", szDNS1, szDNS2);

    //  Open socket for ifconfig command
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        RIL_LOG_CRITICAL("DataConfigUpIpV4() : cannot open control socket\n");
        goto Error;
    }

    //  Code in this function is from system/core/toolbox/ifconfig.c
    //  also from system/core/toolbox/route.c

    //  ifconfig rmnetX <ip address>
    {
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(struct ifreq));
        strncpy(ifr.ifr_name, szNetworkInterfaceName, IFNAMSIZ-1);
        ifr.ifr_name[IFNAMSIZ-1] = '\0';  //  KW fix

        RIL_LOG_INFO("DataConfigUpIpV4() : Setting flags\r\n");
        if (!setflags(s, &ifr, IFF_UP | IFF_POINTOPOINT, 0))
        {
            //goto Error;
            RIL_LOG_CRITICAL("DataConfigUpIpV4() : Error setting flags\r\n");
        }

        RIL_LOG_INFO("DataConfigUpIpV4() : Setting addr\r\n");
        if (!setaddr(s, &ifr, szIpAddr)) // ipaddr
        {
            //goto Error;
            RIL_LOG_CRITICAL("DataConfigUpIpV4() : Error setting addr\r\n");
        }

        RIL_LOG_INFO("DataConfigUpIpV4() : Setting mtu\r\n");
        if (!setmtu(s, &ifr))
        {
            //goto Error;
            RIL_LOG_CRITICAL("DataConfigUpIpV4() : Error setting mtu\r\n");
        }

    }

    //  we have to set a fake ipv4 gateway if not android do not setup network correctly
    if (pChannelData->m_szIpGateways != NULL)
    {
        delete[] pChannelData->m_szIpGateways;
        pChannelData->m_szIpGateways = NULL;
    }

    in_addr_t gw;
    struct in_addr gwaddr;
    in_addr_t addr;

    RIL_LOG_INFO("DataConfigUpIpV4() : set default gateway to fake value\r\n");
    if (inet_pton(AF_INET, szIpAddr, &addr) <= 0)
    {
        RIL_LOG_INFO("DataConfigUpIpV4() : inet_pton() failed for %s!\r\n", szIpAddr);
        goto Error;
    }
    gw = ntohl(addr) & 0xFFFFFF00;
    gw |= 1;
    gwaddr.s_addr = htonl(gw);

    pChannelData->m_szIpGateways = strdup(inet_ntoa(gwaddr));

    if (pChannelData->m_szIpGateways == NULL)
    {
        RIL_LOG_CRITICAL("DataConfigUpIpV4() : Error creating GW\r\n");
        goto Error;
    }

    bRet = TRUE;

Error:

    if (s >= 0)
    {
        close(s);
    }

    RIL_LOG_INFO("DataConfigUpIpV4() EXIT  bRet=[%d]\r\n", bRet);

    return bRet;
}


BOOL DataConfigUpIpV6(char *szNetworkInterfaceName, CChannel_Data* pChannelData)
{
    BOOL bRet = FALSE;
    int s = -1;
    char *szIpAddr = pChannelData->m_szIpAddr2;

    char szIpAddrOut[50];
    struct in6_addr ifIdAddr;
    struct in6_addr ifPrefixAddr;
    struct in6_addr ifOutAddr;

    RIL_LOG_INFO("DataConfigUpIpV6() ENTER  szNetworkInterfaceName=[%s]  szIpAddr=[%s]\r\n", szNetworkInterfaceName, szIpAddr);

    //  Open socket for ifconfig command (note this is ipv6 socket)
    s = socket(AF_INET6, SOCK_DGRAM, 0);
    if (s < 0)
    {
        RIL_LOG_CRITICAL("DataConfigUpIpV6() : cannot open control socket\n");
        goto Error;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, szNetworkInterfaceName, IFNAMSIZ-1);
    ifr.ifr_name[IFNAMSIZ-1] = '\0';  //  KW fix

    inet_pton(AF_INET6, szIpAddr, &ifIdAddr);
    inet_pton(AF_INET6, "FE80::", &ifPrefixAddr);


    // Set local prefix from FE80::
    memcpy(ifOutAddr.s6_addr,ifPrefixAddr.s6_addr,8);
    // Set interface identifier from address given by network
    memcpy((ifOutAddr.s6_addr)+8,(ifIdAddr.s6_addr)+8,8);

    inet_ntop(AF_INET6, &ifOutAddr, szIpAddrOut, sizeof(szIpAddrOut));
    strncpy(szIpAddr,szIpAddrOut,sizeof(szIpAddrOut));

    RIL_LOG_INFO("DataConfigUpIpV6() : Setting flags\r\n");
    if (!setflags(s, &ifr, IFF_UP | IFF_POINTOPOINT, 0))
    {
        //goto Error;
        RIL_LOG_CRITICAL("DataConfigUpIpV6(): Error setting flags\r\n");
    }

    RIL_LOG_INFO("DataConfigUpIpV6() : Setting addr :%s\r\n",szIpAddr);
    if (!setaddr6(s, &ifr, szIpAddr))
    {
        //goto Error;
        RIL_LOG_CRITICAL("DataConfigUpIpV6() : Error setting addr %s\r\n", szIpAddr);
    }

    RIL_LOG_INFO("DataConfigUpV6() : Setting mtu\r\n");
    if (!setmtu(s, &ifr))
    {
        //goto Error;
        RIL_LOG_CRITICAL("DataConfigUpV6() : Error setting mtu\r\n");
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
            RIL_LOG_CRITICAL("DataConfigUpIpV6() : file=[%s] cannot write value [%s]\r\n", file_to_open, szData);
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
        RIL_LOG_CRITICAL("DataConfigUpIpV6() : Cannot open [%s]\r\n", file_to_open);
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
            RIL_LOG_CRITICAL("DataConfigUpIpV6() : file=[%s] cannot write value [%s]\r\n", file_to_open, szData);
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
        RIL_LOG_CRITICAL("DataConfigUpIpV6() : Cannot open [%s]\r\n", file_to_open);
    }

    if (NULL == pChannelData->m_szIpGateways)
    {
        pChannelData->m_szIpGateways = new char[MAX_BUFFER_SIZE];
    }

    if (NULL == pChannelData->m_szIpGateways)
    {
        RIL_LOG_CRITICAL("DataConfigUpIpV6() : Error creating GW\r\n");
        goto Error;
    }
    strcpy(pChannelData->m_szIpGateways,"");
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
    strncpy(ifr.ifr_name, szNetworkInterfaceName, IFNAMSIZ-1);
    ifr.ifr_name[IFNAMSIZ-1] = '\0';  //  KW fix

    RIL_LOG_INFO("DataConfigUpIpV4V6() : Setting addr\r\n");
    if (!setaddr(s, &ifr, szIpAddr))
    {
        //goto Error;
        RIL_LOG_CRITICAL("DataConfigUpIpV4V6() : Error setting add\r\n");
    }

    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, szNetworkInterfaceName, IFNAMSIZ-1);
    ifr.ifr_name[IFNAMSIZ-1] = '\0';  //  KW fix

    // Set link local address to start the SLAAC process
    inet_pton(AF_INET6, szIpAddr2, &ifIdAddr);
    inet_pton(AF_INET6, "FE80::", &ifPrefixAddr);

    // Set local prefix from FE80::
    memcpy(ifOutAddr.s6_addr,ifPrefixAddr.s6_addr,8);
    // Set interface identifier from address given by network
    memcpy((ifOutAddr.s6_addr)+8,(ifIdAddr.s6_addr)+8,8);

    inet_ntop(AF_INET6, &ifOutAddr, szIpAddrOut, sizeof(szIpAddrOut));
    strncpy(szIpAddr2,szIpAddrOut,sizeof(szIpAddrOut));

    RIL_LOG_INFO("DataConfigUpIpV4V6() : Setting flags\r\n");
    if (!setflags(s, &ifr, IFF_UP | IFF_POINTOPOINT, 0))
    {
        //goto Error;
        RIL_LOG_CRITICAL("DataConfigUpIpV4V6() : Error setting flags\r\n");
    }

    if (!setaddr6(s6, &ifr, szIpAddr2))
    {
        //goto Error;
        RIL_LOG_CRITICAL("DataConfigUpIpV4V6() : Error setting add\r\n");
    }

    RIL_LOG_INFO("DataConfigUpV4V6() : Setting mtu\r\n");
    if (!setmtu(s, &ifr))
    {
        //goto Error;
        RIL_LOG_CRITICAL("DataConfigUpV4V6() : Error setting mtu\r\n");
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
            RIL_LOG_CRITICAL("DataConfigUpIpV4V6() : file=[%s] cannot write value [%s]\r\n", file_to_open, szData);
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
        RIL_LOG_CRITICAL("DataConfigUpIpV4V6() : Cannot open [%s]\r\n", file_to_open);
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
            RIL_LOG_CRITICAL("DataConfigUpIpV4V6() : file=[%s] cannot write value [%s]\r\n", file_to_open, szData);

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
        RIL_LOG_CRITICAL("DataConfigUpIpV4V6() : Cannot open [%s]\r\n", file_to_open);
    }

    // we have to set a fake ipv4 gateway if not android do not setup network correctly
    if (pChannelData->m_szIpGateways != NULL)
    {
        delete[] pChannelData->m_szIpGateways;
        pChannelData->m_szIpGateways = NULL;
    }

    in_addr_t gw;
    struct in_addr gwaddr;
    in_addr_t addr;

    RIL_LOG_INFO("DataConfigUpIpV4V6() : set default gateway to fake value\r\n");
    if (inet_pton(AF_INET, szIpAddr, &addr) <= 0)
    {
        RIL_LOG_INFO("DataConfigUpIpV4V6() : inet_pton() failed for %s!\r\n", szIpAddr);
        goto Error;
    }
    gw = ntohl(addr) & 0xFFFFFF00;
    gw |= 1;
    gwaddr.s_addr = htonl(gw);

    pChannelData->m_szIpGateways = strdup(inet_ntoa(gwaddr));

    if (pChannelData->m_szIpGateways == NULL)
    {
        RIL_LOG_CRITICAL("DataConfigUpIpV4V6() : Error creating GW\r\n");
        goto Error;
    }

    bRet = TRUE;

Error:

    if (s >= 0)
    {
        close(s);
    }

    if (s6 >= 0)
    {
        close(s6);
    }

    RIL_LOG_INFO("DataConfigUpIpV4V6() EXIT  bRet=[%d]\r\n", bRet);

    return bRet;
}

//
//  Call this whenever data is disconnected
//
#if defined(BOARD_HAVE_IFX7060)
BOOL DataConfigDown(UINT32 uiCID)
{
    //  First check to see if uiCID is valid
    if (uiCID > MAX_PDP_CONTEXTS || uiCID == 0)
    {
        RIL_LOG_CRITICAL("DataConfigDown() - Invalid CID = [%u]\r\n", uiCID);
        return FALSE;
    }

    CRepository repository;
    BOOL bRet = FALSE;
    char szNetworkInterfaceName[MAX_BUFFER_SIZE] = {0};
    CChannel_Data* pChannelData = NULL;
    struct gsm_netconfig netconfig;
    int fd = -1;
    int ret = -1;
    int s = -1;

    //  See if CID passed in is valid
    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("DataConfigDown() - Invalid CID=[%u], no data channel found!\r\n", uiCID);
        return FALSE;
    }

    //  Grab the network interface name
    if (!CopyStringNullTerminate(szNetworkInterfaceName, pChannelData->m_szInterfaceName, MAX_BUFFER_SIZE))
    {
        RIL_LOG_CRITICAL("DataConfigDown() - Could not create network interface name\r\n");
        strcpy(szNetworkInterfaceName, "");
        goto Error;
    }
    else
    {
        RIL_LOG_INFO("DataConfigDown() - ENTER  szNetworkInterfaceName=[%s]  CID=[%u]\r\n", szNetworkInterfaceName, uiCID);
    }

    // Reset ContextID to 0, to free up the channel for future use
    RIL_LOG_INFO("DataConfigDown() - ****** Setting chnl=[%d] to CID=[0] ******\r\n", pChannelData->GetRilChannel());


    fd = pChannelData->GetFD();

    if (!pChannelData->m_hsiDirect)
    {
        //  Put the channel back into AT command mode
        netconfig.adaption = 3;
        netconfig.protocol = htons(ETH_P_IP);

        if (fd >= 0)
        {
            RIL_LOG_INFO("DataConfigDown() - ***** PUTTING channel=[%d] in AT COMMAND MODE *****\r\n", pChannelData->GetRilChannel());
            ret = ioctl( fd, GSMIOC_DISABLE_NET, &netconfig );
        }

        // Flush the response buffer to restart from scratch.
        // Any data received by the RRIL during the DATA MODE should be trashed
        //pChannelData->FlushResponse();
    }
    else
    {
        RIL_LOG_INFO("DataConfigDown() : disable hsi network interface\r\n");
        //  Open socket for ifconfig and setFlags commands
        s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0)
        {
            RIL_LOG_CRITICAL("DataConfigDown() : cannot open control socket\n");
            goto Error;
        }

        struct ifreq ifr;
        memset(&ifr, 0, sizeof(struct ifreq));
        strncpy(ifr.ifr_name, szNetworkInterfaceName, IFNAMSIZ-1);
        ifr.ifr_name[IFNAMSIZ-1] = '\0';  //  KW fix
        if (!setflags(s, &ifr, 0, IFF_UP))
        {
            RIL_LOG_CRITICAL("DataConfigDown() : Error setting flags\r\n");
            goto Error;
        }
    }

    bRet = TRUE;

    RIL_LOG_INFO("[RIL STATE] PDP CONTEXT DEACTIVATION chnl=%d\r\n", pChannelData->GetRilChannel());

Error:
    pChannelData->FreeContextID();
    RIL_LOG_INFO("DataConfigDown() EXIT  bRet=[%d]\r\n", bRet);
    return bRet;
}
#else
BOOL DataConfigDown(UINT32 uiCID)
{
    //  First check to see if uiCID is valid
    if (uiCID > MAX_PDP_CONTEXTS || uiCID == 0)
    {
        RIL_LOG_CRITICAL("DataConfigDown() - Invalid CID = [%u]\r\n", uiCID);
        return FALSE;
    }

    CRepository repository;
    BOOL bRet = FALSE;
    char szNetworkInterfaceName[MAX_BUFFER_SIZE] = {0};
    CChannel_Data* pChannelData = NULL;
    struct gsm_netconfig netconfig;
    int fd = -1;
    int ret = -1;

    //  See if CID passed in is valid
    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("DataConfigDown() - Invalid CID=[%u], no data channel found!\r\n", uiCID);
        return FALSE;
    }

    //  Grab the network interface name
    if (!repository.Read(g_szGroupModem, g_szNetworkInterfaceNamePrefix, szNetworkInterfaceName, MAX_BUFFER_SIZE))
    {
        RIL_LOG_CRITICAL("DataConfigDown() - Could not read network interface name prefix from repository\r\n");
        strcpy(szNetworkInterfaceName, "");
        goto Error;
    }
    RIL_LOG_INFO("DataConfigDown() - ENTER  szNetworkInterfaceName prefix=[%s]  CID=[%u]\r\n", szNetworkInterfaceName, uiCID);
    //  Don't forget to append the Context ID!
    if (!PrintStringNullTerminate(szNetworkInterfaceName, MAX_BUFFER_SIZE, "%s%u", szNetworkInterfaceName, uiCID-1))
    {
        RIL_LOG_CRITICAL("DataConfigDown() - Could not create network interface name\r\n");
        strcpy(szNetworkInterfaceName, "");
        goto Error;
    }
    else
    {
        RIL_LOG_INFO("DataConfigDown() - ENTER  szNetworkInterfaceName=[%s]  CID=[%u]\r\n", szNetworkInterfaceName, uiCID);
    }


    // Reset ContextID to 0, to free up the channel for future use
    RIL_LOG_INFO("DataConfigDown() - ****** Setting chnl=[%d] to CID=[0] ******\r\n", pChannelData->GetRilChannel());

    pChannelData->FreeContextID();
    fd = pChannelData->GetFD();

    //  Put the channel back into AT command mode
    netconfig.adaption = 3;
    netconfig.protocol = htons(ETH_P_IP);

    if (fd >= 0)
    {
        RIL_LOG_INFO("DataConfigDown() - ***** PUTTING channel=[%d] in AT COMMAND MODE *****\r\n", pChannelData->GetRilChannel());
        ret = ioctl( fd, GSMIOC_DISABLE_NET, &netconfig );
    }

    // Flush the response buffer to restart from scratch.
    // Any data received by the RRIL during the DATA MODE should be trashed
    //pChannelData->FlushResponse();

    bRet = TRUE;

    RIL_LOG_INFO("[RIL STATE] PDP CONTEXT DEACTIVATION chnl=%d\r\n", pChannelData->GetRilChannel());

Error:

    RIL_LOG_INFO("DataConfigDown() EXIT  bRet=[%d]\r\n", bRet);
    return bRet;
}
#endif

void CleanupAllDataConnections()
{
    //  Bring down all data contexts internally.
    CChannel_Data* pChannelData = NULL;

    for (UINT32 i = RIL_CHANNEL_DATA1; i < g_uiRilChannelCurMax; i++)
    {
        if (NULL == g_pRilChannel[i]) // could be NULL if reserved channel
            continue;

        pChannelData = static_cast<CChannel_Data*>(g_pRilChannel[i]);

        //  We are taking down all data connections here, so we are looping over each data channel.
        //  Don't call DataConfigDown with invalid CID.
        if (pChannelData && pChannelData->GetContextID() > 0)
        {
            RIL_LOG_INFO("CleanupAllDataConnections() - Calling DataConfigDown  chnl=[%d], cid=[%d]\r\n", i, pChannelData->GetContextID());
            DataConfigDown(pChannelData->GetContextID());
        }
    }
}

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

    //  Sanity checks
    if ( (NULL == szIpIn) || (NULL == szIpOut) || (0 == uiIpOutSize))
    {
        RIL_LOG_CRITICAL("ConvertIPAddressToAndroidReadable() : Invalid inputs!\r\n");
        return FALSE;
    }

    //  Count number of '.'
    int nDotCount = 0;
    for (UINT32 i = 0; szIpIn[i] != '\0'; i++)
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
            UINT32 aIP[MAX_AIPV6_INDEX] = {0};
            unsigned char acIP[MAX_AIPV6_INDEX] = {0};
            if (EOF == sscanf(szIpIn, "%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u",
                            &aIP[0], &aIP[1], &aIP[2], &aIP[3], &aIP[4], &aIP[5], &aIP[6], &aIP[7],
                            &aIP[8], &aIP[9], &aIP[10], &aIP[11], &aIP[12], &aIP[13], &aIP[14], &aIP[15]))
            {
                RIL_LOG_CRITICAL("ConvertIPAddressToAndroidReadable() - cannot sscanf into aIP[]! ipv6\r\n");
                goto Error;
            }

            //  Loop through array, check values from modem is from 0-255.
            for (int i=0; i<MAX_AIPV6_INDEX; i++)
            {
                if (aIP[i] > 255)
                {
                    //  Value is not between 0-255.
                    RIL_LOG_CRITICAL("ConvertIPAddressToAndroidReadable() - ipv6 aIP[%d] not in range 0-255. val=%u\r\n", i, aIP[i]);
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
                    RIL_LOG_CRITICAL("ConvertIPAddressToAndroidReadable() - cannot inet_ntop ipv6\r\n");
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
            UINT32 aIP[MAX_AIPV4V6_INDEX] = {0};
            if (EOF == sscanf(szIpIn, "%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u.%u",
                            &aIP[0], &aIP[1], &aIP[2], &aIP[3],
                            &aIP[4], &aIP[5], &aIP[6], &aIP[7], &aIP[8], &aIP[9], &aIP[10], &aIP[11],
                            &aIP[12], &aIP[13], &aIP[14], &aIP[15], &aIP[16], &aIP[17], &aIP[18], &aIP[19]
                            ))
            {
                RIL_LOG_CRITICAL("ConvertIPAddressToAndroidReadable() - cannot sscanf into aIP[]! ipv4v6\r\n");
                goto Error;
            }

            //  Loop through array, check values from modem is from 0-255.
            for (int i=0; i<MAX_AIPV4V6_INDEX; i++)
            {
                if (aIP[i] > 255)
                {
                    //  Value is not between 0-255.
                    RIL_LOG_CRITICAL("ConvertIPAddressToAndroidReadable() - ipv4v6 aIP[%d] not in range 0-255. val=%u\r\n", i, aIP[i]);
                    goto Error;
                }
            }

            if (snprintf(szIpOut, uiIpOutSize,
                    "%u.%u.%u.%u",
                    aIP[0], aIP[1], aIP[2], aIP[3]) <= 0)
            {
                RIL_LOG_CRITICAL("ConvertIPAddressToAndroidReadable() - error with snprintf()! ipv4v6 v4 part\r\n");
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
                    RIL_LOG_CRITICAL("ConvertIPAddressToAndroidReadable() - cannot inet_ntop ipv4v6\r\n");
                    goto Error;
                }
            }
        }
        break;

        default:
            RIL_LOG_CRITICAL("ConvertIPAddressToAndroidReadable() - Unknown address format nDotCount=[%d]\r\n", nDotCount);
            goto Error;
    }

    bRet = TRUE;

Error:
    RIL_LOG_VERBOSE("ConvertIPAddressToAndroidReadable() - Exit\r\n");
    return bRet;
}
