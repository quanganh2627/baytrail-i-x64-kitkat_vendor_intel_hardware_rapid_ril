////////////////////////////////////////////////////////////////////////////
// te_xmm6360.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Overlay for the IMC 6360 modem
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
#include "te_xmm6360.h"
#include "rildmain.h"
#include "callbacks.h"
#include "oemhookids.h"
#include "repository.h"
#include "reset.h"
#include "data_util.h"


CTE_XMM6360::CTE_XMM6360(CTE& cte)
: CTE_XMM6260(cte),
  m_currentNetworkType(-1)
{
}

CTE_XMM6360::~CTE_XMM6360()
{
}

BOOL CTE_XMM6360::PdpContextActivate(REQUEST_DATA& rReqData, void* pData,
                                                            UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM6360::PdpContextActivate() - Enter\r\n");

    BOOL bRet = FALSE;
    UINT32 uiCID = 0;
    int muxControlChannel = -1;
    int muxDataChannel = -1;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;
    CChannel_Data* pChannelData = NULL;
    BOOL bIsHSIDirect = FALSE;
    int hsiChannel = -1;
    UINT32 uiRilChannel = 0;
    int ipcDataChannelMin = 0;
    char* szModemResourceName = {'\0'};

    if (NULL == pData ||
                    sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::PdpContextActivate() - Invalid input data\r\n");
        goto Error;
    }

    pDataCallContextData = (S_SETUP_DATA_CALL_CONTEXT_DATA*)pData;
    uiCID = pDataCallContextData->uiCID;

    // Get Channel Data according to CID
    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::PdpContextActivate() -"
                " No Data Channel found for CID %u.\r\n",
                uiCID);
        goto Error;
    }

    bIsHSIDirect = pChannelData->IsHSIDirect();
    hsiChannel =  pChannelData->GetHSIChannel();
    uiRilChannel = pChannelData->GetRilChannel();

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
            RIL_LOG_CRITICAL("CTE_XMM6360::PdpContextActivate() - Unknown mux channel"
                    "for RIL Channel [%u] \r\n", uiRilChannel);
            goto Error;
        }

    if (!bIsHSIDirect)
    {
        muxDataChannel = muxControlChannel;

        if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
                "AT+CGACT=1,%d;+XDATACHANNEL=1,1,\"/mux/%d\",\"/mux/%d\",0\r",
                uiCID, muxControlChannel, muxDataChannel))
        {
            RIL_LOG_CRITICAL("CTE_XMM6360::PdpContextActivate() -"
                    "  cannot create CGDATA command\r\n");
            goto Error;
        }
    }
    else
    {
        if (hsiChannel < 0)
        {
            RIL_LOG_CRITICAL("CTE_XMM6360::PdpContextActivate() - No free HSI Channel \r\n");
            goto Error;
        }

       // Get the hsi channel id
        int hsiNetworkPath = -1;

        ipcDataChannelMin = CSystemManager::GetInstance().GetIpcDataChannelMin();

        if (ipcDataChannelMin <= hsiChannel && RIL_MAX_NUM_IPC_CHANNEL > hsiChannel)
        {
           hsiNetworkPath = hsiChannel;
        }
        else
        {
           RIL_LOG_CRITICAL("CTE_XMM6360::PdpContextActivate() - Unknown HSI Channel [%d] \r\n",
                            hsiChannel);
           goto Error;
        }

        szModemResourceName = CSystemManager::GetInstance().GetModemResourceName();

        if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
                "AT+CGACT=1,%d;+XDATACHANNEL=1,1,\"/mux/%d\",\"/%s/%d\",0\r",
                uiCID, muxControlChannel,
                szModemResourceName, hsiNetworkPath))
        {
            RIL_LOG_CRITICAL("CTE_XMM6360::PdpContextActivate() -"
                    "  cannot create CGDATA command\r\n");
            goto Error;
        }
    }

    if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+CEER\r",
                                                    sizeof(rReqData.szCmd2)))
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::PdpContextActivate() - Cannot create CEER command\r\n");
    }

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CTE_XMM6360::PdpContextActivate() - Exit\r\n");
    return bRet;
}

RIL_RESULT_CODE CTE_XMM6360::ParseEnterDataState(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM6360::ParseEnterDataState() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* pszRsp = rRspData.szResponse;
    UINT32 uiCause;
    BOOL bIsHSIDirect = FALSE;
    CChannel_Data* pChannelData = NULL;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;
    UINT32 uiCID = 0;

    if (NULL == rRspData.pContextData ||
            sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA) != rRspData.cbContextData)
    {
        RIL_LOG_INFO("CTE_XMM6360::ParseEnterDataState() - Invalid context data\r\n");
        goto Error;
    }

    pDataCallContextData =
                    (S_SETUP_DATA_CALL_CONTEXT_DATA*)rRspData.pContextData;
    uiCID = pDataCallContextData->uiCID;

    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_INFO("CTE_XMM6360::ParseEnterDataState() - No Data Channel for CID %u.\r\n",
                                                                    uiCID);
        goto Error;
    }

    if (ParseCEER(rRspData, uiCause))
    {
        RIL_LOG_INFO("CTE_XMM6360::ParseEnterDataState() - uiCause: %u\r\n",
                                                                    uiCause);
        int failCause = PDP_FAIL_ERROR_UNSPECIFIED;

        failCause = MapErrorCodeToRilDataFailCause(uiCause);
        pChannelData->SetDataFailCause(failCause);
        goto Error;
    }

    bIsHSIDirect = pChannelData->IsHSIDirect();

    if (!bIsHSIDirect)
    {
        // Block the read thread and then flush the tty and the channel
        // From now, any failure will lead to DataConfigDown
        pChannelData->BlockAndFlushChannel(BLOCK_CHANNEL_BLOCK_ALL, FLUSH_CHANNEL_NO_FLUSH);
        pChannelData->FlushAndUnblockChannel(UNBLOCK_CHANNEL_UNBLOCK_TTY, FLUSH_CHANNEL_FLUSH_ALL);
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTE_XMM6360::ParseEnterDataState() - Exit\r\n");
    return res;
}

BOOL CTE_XMM6360::SetupInterface(UINT32 uiCID)
{
    RIL_LOG_VERBOSE("CTE_XMM6360::SetupInterface() - Enter\r\n");

    BOOL bRet = FALSE;

    char szNetworkInterfaceName[IFNAMSIZ];
    struct gsm_netconfig netconfig;
    int fd = -1;
    int ret = 0;
    UINT32 uiChannel = 0;
    int state = 0;
    int networkInterfaceID = -1;
    CChannel_Data* pChannelData = NULL;
    PDP_TYPE eDataConnectionType = PDP_TYPE_IPV4;  //  dummy for now, set to IPv4.
    char szPdpType[MAX_BUFFER_SIZE] = {'\0'};
    char szIpAddr[MAX_IPADDR_SIZE] = {'\0'};
    char szIpAddr2[MAX_IPADDR_SIZE] = {'\0'};
    int dataProfile = -1;
    BOOL bIsHSIDirect = FALSE;
    int hsiChannel = -1;
    int ipcDataChannelMin = 0;

    UINT32 nw_if_pdp_mux_offset = 0;

    ipcDataChannelMin = CSystemManager::GetInstance().GetIpcDataChannelMin();

    if (ipcDataChannelMin > RIL_MAX_NUM_IPC_CHANNEL)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::SetupInterface() - Invalid data channel range (%u).\r\n",
                                                                    ipcDataChannelMin);
        goto Error;
    }

    nw_if_pdp_mux_offset = (RIL_MAX_NUM_IPC_CHANNEL - ipcDataChannelMin);

    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::SetupInterface() - No Data Channel for CID %u.\r\n",
                                                                    uiCID);
        goto Error;
    }

    state = pChannelData->GetDataState();
    if (E_DATA_STATE_ACTIVE != state)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::SetupInterface() - Invalid data state %d.\r\n",
                                                                    state);
        goto Error;
    }

    bIsHSIDirect = pChannelData->IsHSIDirect();
    dataProfile =  pChannelData->GetDataProfile();
    hsiChannel =  pChannelData->GetHSIChannel();

    if (!bIsHSIDirect){
        // First network interface is rmnet3 for pdp over mux
        switch (dataProfile)
        {
        case RIL_DATA_PROFILE_DEFAULT:
            networkInterfaceID = nw_if_pdp_mux_offset + RIL_DATA_PROFILE_DEFAULT;
            break;
        case RIL_DATA_PROFILE_TETHERED:
            networkInterfaceID = nw_if_pdp_mux_offset + RIL_DATA_PROFILE_TETHERED;
            break;
        case RIL_DATA_PROFILE_IMS:
            networkInterfaceID = nw_if_pdp_mux_offset + RIL_DATA_PROFILE_IMS;
            break;
        case RIL_DATA_PROFILE_MMS:
            networkInterfaceID = nw_if_pdp_mux_offset + RIL_DATA_PROFILE_MMS;
            break;
        case RIL_DATA_PROFILE_CBS:
            networkInterfaceID = nw_if_pdp_mux_offset + RIL_DATA_PROFILE_CBS;
            break;
        case RIL_DATA_PROFILE_FOTA:
            networkInterfaceID = nw_if_pdp_mux_offset + RIL_DATA_PROFILE_FOTA;
            break;
        case RIL_DATA_PROFILE_SUPL:
            networkInterfaceID = nw_if_pdp_mux_offset + RIL_DATA_PROFILE_SUPL;
            break;
        case RIL_DATA_PROFILE_HIPRI:
            networkInterfaceID = nw_if_pdp_mux_offset + RIL_DATA_PROFILE_HIPRI;
            break;
        default:
            RIL_LOG_CRITICAL("CTE_XMM6360::SetupInterface() - Unknown Data Profile [%d] \r\n",
                                                                dataProfile);
            goto Error;
        }
    }
    else
    {
        if (ipcDataChannelMin <= hsiChannel && RIL_MAX_NUM_IPC_CHANNEL > hsiChannel)
        {
            networkInterfaceID = (hsiChannel - ipcDataChannelMin);
            if (networkInterfaceID < 0)
            {
                RIL_LOG_CRITICAL("CTE_XMM6360::SetupInterface() - Invalid network"
                        " interface ID (%d) \r\n", networkInterfaceID);
                goto Error;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CTE_XMM6360::SetupInterface() - Unknown his channel [%d] \r\n",
                                                    hsiChannel);
            goto Error;
        }
    }

    if (!PrintStringNullTerminate(szNetworkInterfaceName, sizeof(szNetworkInterfaceName),
                                        "%s%d", m_szNetworkInterfaceNamePrefix,
                                                            networkInterfaceID))
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::SetupInterface() - Cannot set network interface name\r\n");
        goto Error;
    }
    else
    {
        RIL_LOG_INFO("CTE_XMM6360::SetupInterface() - szNetworkInterfaceName=[%s], CID=[%u]\r\n",
                                                szNetworkInterfaceName, uiCID);
    }

    pChannelData->SetInterfaceName(szNetworkInterfaceName);

    if (!bIsHSIDirect)
    {
        UINT32 uiChannel = pChannelData->GetRilChannel();

        // N_GSM related code
        netconfig.adaption = 3;
        netconfig.protocol = htons(ETH_P_IP);
        strncpy(netconfig.if_name, szNetworkInterfaceName, IFNAMSIZ - 1);
        netconfig.if_name[IFNAMSIZ - 1] = '\0';

        // Add IF NAME
        fd = pChannelData->GetFD();
        if (fd >= 0)
        {
            RIL_LOG_INFO("CTE_XMM6360::SetupInterface() -"
                    " ***** PUTTING channel=[%u] in DATA MODE *****\r\n", uiChannel);
            ret = ioctl( fd, GSMIOC_ENABLE_NET, &netconfig );       // Enable data channel
            if (ret < 0)
            {
                RIL_LOG_CRITICAL("CTE_XMM6360::SetupInterface() -"
                        " Unable to create interface %s : %s \r\n",
                        netconfig.if_name, strerror(errno));
                goto Error;
            }
        }
        else
        {
            //  No FD.
            RIL_LOG_CRITICAL("CTE_XMM6360::SetupInterface() -"
                    " Could not get Data Channel chnl=[%u] fd=[%d].\r\n", uiChannel, fd);
            goto Error;
        }
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
    if (!DataConfigUp(szNetworkInterfaceName, pChannelData, eDataConnectionType))
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::SetupInterface() -"
                " Unable to bringup interface ifconfig\r\n");
        goto Error;
    }

    bRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CTE_XMM6360::SetupInterface() Exit\r\n");
    return bRet;
}

//
//  Call this whenever data is disconnected
//
BOOL CTE_XMM6360::DataConfigDown(UINT32 uiCID)
{
    RIL_LOG_VERBOSE("CTE_XMM6360::DataConfigDown() - Enter\r\n");

    //  First check to see if uiCID is valid
    if (uiCID > MAX_PDP_CONTEXTS || uiCID == 0)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::DataConfigDown() - Invalid CID = [%u]\r\n", uiCID);
        return FALSE;
    }

    BOOL bRet = FALSE;
    char szNetworkInterfaceName[MAX_INTERFACE_NAME_SIZE] = {'\0'};
    CChannel_Data* pChannelData = NULL;
    struct gsm_netconfig netconfig;
    int fd = -1;
    int ret = -1;
    int s = -1;
    UINT32 uiRilChannel = 0;
    BOOL bIsHSIDirect = FALSE;
    int state;

    //  See if CID passed in is valid
    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::DataConfigDown() -"
                " Invalid CID=[%u], no data channel found!\r\n", uiCID);
        return FALSE;
    }

    uiRilChannel = pChannelData->GetRilChannel();
    bIsHSIDirect = pChannelData->IsHSIDirect();
    pChannelData->GetInterfaceName(szNetworkInterfaceName,
                                            sizeof(szNetworkInterfaceName));

    RIL_LOG_INFO("CTE_XMM6360::DataConfigDown() - szNetworkInterfaceName=[%s]  CID=[%u]\r\n",
                                                szNetworkInterfaceName, uiCID);

    // Reset ContextID to 0, to free up the channel for future use
    RIL_LOG_INFO("CTE_XMM6360::DataConfigDown() - ****** Setting chnl=[%u] to CID=[0] ******\r\n",
                                                                uiRilChannel);

    fd = pChannelData->GetFD();
    state = pChannelData->GetDataState();

    if (!bIsHSIDirect)
    {
        if (E_DATA_STATE_IDLE != state
                && E_DATA_STATE_INITING != state
                && E_DATA_STATE_ACTIVATING != state)
        {
            // Blocking TTY flow.
            // Security in order to avoid IP data in response buffer.
            // Not mandatory.
            pChannelData->BlockAndFlushChannel(BLOCK_CHANNEL_BLOCK_TTY,
                    FLUSH_CHANNEL_NO_FLUSH);
        }

        //  Put the channel back into AT command mode
        netconfig.adaption = 3;
        netconfig.protocol = htons(ETH_P_IP);

        if (fd >= 0)
        {
            RIL_LOG_INFO("CTE_XMM6360::DataConfigDown() -"
                    " ***** PUTTING channel=[%u] in AT COMMAND MODE *****\r\n", uiRilChannel);
            ret = ioctl(fd, GSMIOC_DISABLE_NET, &netconfig);
        }
    }
    else
    {
        /*
         * HSI inteface ENABLE/DISABLE can be done only by the HSI driver.
         * Rapid RIL can only bring up or down the interface.
         */
        RIL_LOG_INFO("CTE_XMM6360::DataConfigDown() : Bring down hsi network interface\r\n");

        //  Open socket for ifconfig and setFlags commands
        s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0)
        {
            RIL_LOG_CRITICAL("CTE_XMM6360::DataConfigDown() : cannot open control socket "
                    "(error %d / '%s')\n", errno, strerror(errno));
            goto Error;
        }

        struct ifreq ifr;
        memset(&ifr, 0, sizeof(struct ifreq));
        strncpy(ifr.ifr_name, szNetworkInterfaceName, IFNAMSIZ-1);
        ifr.ifr_name[IFNAMSIZ-1] = '\0';

        // set ipv4 address to 0.0.0.0 to unset ipv4 address,
        // ipv6 addresses are automatically cleared
        if (!setaddr(s, &ifr, "0.0.0.0"))
        {
            RIL_LOG_CRITICAL("CTE_XMM6360::DataConfigDown() : Error removing addr\r\n");
        }

        if (!setflags(s, &ifr, 0, IFF_UP))
        {
            RIL_LOG_CRITICAL("CTE_XMM6360::DataConfigDown() : Error setting flags\r\n");
        }

        if (close(s) < 0)
        {
            RIL_LOG_CRITICAL("CTE_XMM6360::DataConfigDown() : could not close control socket "
                    "(error %d / '%s')\n", errno, strerror(errno));
        }
    }
    pChannelData->ResetDataCallInfo();

    bRet = TRUE;

    RIL_LOG_INFO("[RIL STATE] PDP CONTEXT DEACTIVATION chnl=%d\r\n",
            pChannelData->GetRilChannel());

Error:
    if (!bIsHSIDirect)
    {
        if (E_DATA_STATE_IDLE != state
                && E_DATA_STATE_INITING != state
                && E_DATA_STATE_ACTIVATING != state)
        {
            // Flush buffers and Unblock read thread.
            // Security in order to avoid IP data in response buffer.
            // Will unblock Channel read thread and TTY.
            // Unblock read thread whatever the result is to avoid forever block
            pChannelData->FlushAndUnblockChannel(UNBLOCK_CHANNEL_UNBLOCK_ALL,
                    FLUSH_CHANNEL_FLUSH_ALL);
        }
    }

    RIL_LOG_INFO("CTE_XMM6360::DataConfigDown() EXIT  bRet=[%d]\r\n", bRet);
    return bRet;
}

//
// RIL_REQUEST_BASEBAND_VERSION 51
//
RIL_RESULT_CODE CTE_XMM6360::CoreBasebandVersion(REQUEST_DATA& rReqData,
        void* pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM6360::CoreBasebandVersion() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "at+xgendata\r",
            sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTE_XMM6360::CoreBasebandVersion() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM6360::ParseBasebandVersion(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM6360::ParseBasebandVersion() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* pszRsp = rRspData.szResponse;
    char szTemp[MAX_BUFFER_SIZE] = {0};

    char* pszBasebandVersion = (char*)malloc(MAX_PROP_VALUE);
    if (NULL == pszBasebandVersion)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::ParseBasebandVersion() - Could not "
                "allocate memory for a %u-char string.\r\n", MAX_PROP_VALUE);
        goto Error;
    }
    memset(pszBasebandVersion, 0x00, MAX_PROP_VALUE);

    if (!sscanf(pszRsp,"%*[^*]%*c%[^*]", szTemp))
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::ParseBasebandVersion() - Could not "
                "extract the baseband version string.\r\n");
        goto Error;
    }

    if (!PrintStringNullTerminate(pszBasebandVersion, MAX_PROP_VALUE,
            "%s", szTemp))
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::ParseBasebandVersion() - Could not "
                "extract pszBasebandVersion\r\n");
        goto Error;
    }

    if (strlen(pszBasebandVersion) <= 0)
    {
        RIL_LOG_CRITICAL("CTE_XMM6360::ParseBasebandVersion() - Invalid "
                "baseband version string.\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_XMM6360::ParseBasebandVersion() - "
            "pszBasebandVersion=[%s]\r\n", pszBasebandVersion);

    rRspData.pData   = (void*)pszBasebandVersion;
    rRspData.uiDataSize  = sizeof(char*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pszBasebandVersion);
        pszBasebandVersion = NULL;
    }

    RIL_LOG_VERBOSE("CTE_XMM6360::ParseBasebandVersion() - Exit\r\n");
    return res;
}

RIL_RadioTechnology CTE_XMM6360::MapAccessTechnology(UINT32 uiStdAct)
{
    RIL_LOG_VERBOSE("CTE_XMM6360::MapAccessTechnology() ENTER  uiStdAct=[%u]\r\n", uiStdAct);

    /*
     * 20111103: There is no 3GPP standard value defined for GPRS and HSPA+
     * access technology. So, values 1 and 8 are used in relation with the
     * IMC proprietary +XREG: <Act> parameter.
     *
     * Note: GSM Compact is not supported by IMC modem.
     */
    RIL_RadioTechnology rtAct = RADIO_TECH_UNKNOWN;

    //  Check state and set global variable for network technology
    switch (uiStdAct)
    {
        /* 20130202:
         * case 9 is added for HSPA dual carrier
         */
        case 9: // Proprietary value introduced for HSPA+ DC-FSPA+
        rtAct = RADIO_TECH_HSPAP; // 15
        break;

        default:
        rtAct = CTEBase::MapAccessTechnology(uiStdAct);
        break;
    }
    RIL_LOG_VERBOSE("CTE_XMM6360::MapAccessTechnology() EXIT  rtAct=[%u]\r\n", (UINT32)rtAct);
    return rtAct;
}
