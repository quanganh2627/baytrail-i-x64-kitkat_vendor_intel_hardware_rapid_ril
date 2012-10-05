////////////////////////////////////////////////////////////////////////////
// te_inf_7x60.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Overlay for the Infineon 7x60 modem
//
/////////////////////////////////////////////////////////////////////////////

#include <wchar.h>

#include "types.h"
#include "nd_structs.h"
#include "util.h"
#include "extract.h"
#include "rillog.h"
#include "te.h"
#include "sync_ops.h"
#include "command.h"
#include "te_inf_7x60.h"
#include "rildmain.h"
#include "callbacks.h"
#include "oemhookids.h"
#include "repository.h"
#include "reset.h"
#include <cutils/properties.h>
#include <sys/system_properties.h>
#include "data_util.h"

//  This is for socket-related calls.
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/gsmmux.h>

CTE_INF_7x60::CTE_INF_7x60() :
    CTE_INF_6260(),
    m_currentNetworkType(-1)
{
}

CTE_INF_7x60::~CTE_INF_7x60()
{
}

BOOL CTE_INF_7x60::PdpContextActivate(REQUEST_DATA& rReqData, void* pData,
                                                            UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_7x60::PdpContextActivate() - Enter\r\n");

    BOOL bRet = FALSE;
    UINT32 uiCID = 0;
    int muxControlChannel = -1;
    S_SETUP_DATA_CALL_CONTEXT_DATA* pDataCallContextData = NULL;
    CChannel_Data* pChannelData = NULL;
    BOOL bIsHSIDirect = FALSE;
    int hsiChannel = -1;
    UINT32 uiRilChannel = 0;

    if (NULL == pData ||
                    sizeof(S_SETUP_DATA_CALL_CONTEXT_DATA) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::PdpContextActivate() - Invalid input data\r\n");
        goto Error;
    }

    pDataCallContextData = (S_SETUP_DATA_CALL_CONTEXT_DATA*)pData;
    uiCID = pDataCallContextData->uiCID;

    // Get Channel Data according to CID
    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::PdpContextActivate() - No Data Channel found for RIL channel number %u.\r\n",
                                                pChannelData->GetRilChannel());
        goto Error;
    }

    bIsHSIDirect = pChannelData->IsHSIDirect();
    hsiChannel =  pChannelData->GetHSIChannel();
    uiRilChannel = pChannelData->GetRilChannel();

    if (!bIsHSIDirect)
    {
        if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
                                                    "AT+CGACT=1,%d\r", uiCID))
        {
            RIL_LOG_CRITICAL("CTE_INF_7x60::PdpContextActivate() -  cannot create CGDATA command\r\n");
            goto Error;
        }
    }
    else
    {
        if (hsiChannel < 0)
        {
            RIL_LOG_CRITICAL("CTE_INF_7x60::PdpContextActivate() - No free HSI Channel \r\n");
            goto Error;
        }

       // Get the hsi channel id
        int hsiNetworkPath = -1;
        switch (hsiChannel)
        {
            case RIL_HSI_CHANNEL1:
                hsiNetworkPath = RIL_HSI_CHANNEL1;
                break;
            case RIL_HSI_CHANNEL2:
                hsiNetworkPath = RIL_HSI_CHANNEL2;
                break;
            case RIL_HSI_CHANNEL3:
                hsiNetworkPath = RIL_HSI_CHANNEL3;
                break;
            default:
                RIL_LOG_CRITICAL("CTE_INF_7x60::PdpContextActivate() - Unknown HSI Channel [%d] \r\n",
                                                                hsiChannel);
                goto Error;
        }

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
                RIL_LOG_CRITICAL("CTE_INF_7x60::PdpContextActivate() - Unknown mux channel for RIL Channel [%u] \r\n",
                                                            uiRilChannel);
                goto Error;
        }

        if (!PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1),
            "AT+CGACT=1,%d;+XDATACHANNEL=1,1,\"/mux/%d\",\"/mipi_ipc/%d\",0\r",
            uiCID, muxControlChannel, hsiNetworkPath))
        {
            RIL_LOG_CRITICAL("CTE_INF_7x60::PdpContextActivate() -  cannot create CGDATA command\r\n");
            goto Error;
        }
    }

    if (!CopyStringNullTerminate(rReqData.szCmd2, "AT+CEER\r",
                                                    sizeof(rReqData.szCmd2)))
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::PdpContextActivate() - Cannot create CEER command\r\n");
    }

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CTE_INF_7x60::PdpContextActivate() - Exit\r\n");
    return bRet;
}

RIL_RESULT_CODE CTE_INF_7x60::ParseEnterDataState(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_7x60::ParseEnterDataState() - Enter\r\n");

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
        RIL_LOG_INFO("CTE_INF_7x60::ParseEnterDataState() - Invalid context data\r\n");
        goto Error;
    }

    pDataCallContextData =
                    (S_SETUP_DATA_CALL_CONTEXT_DATA*)rRspData.pContextData;
    uiCID = pDataCallContextData->uiCID;

    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_INFO("CTE_INF_7x60::ParseEnterDataState() - No Data Channel for CID %u.\r\n",
                                                                    uiCID);
        goto Error;
    }

    if (ParseCEER(rRspData, uiCause))
    {
        RIL_LOG_INFO("CTE_INF_7x60::ParseEnterDataState() - uiCause: %u\r\n",
                                                                    uiCause);
        int failCause = PDP_FAIL_ERROR_UNSPECIFIED;

        failCause = MapErrorCodeToRilDataFailCause(uiCause);
        pChannelData->SetDataFailCause(failCause);
        goto Error;
    }

    bIsHSIDirect = pChannelData->IsHSIDirect();

    // Confirm we got "CONNECT" for data over MUX
    // Confirm we get "OK" for data directly over hsi
    if (!bIsHSIDirect)
    {
        if (!FindAndSkipString(pszRsp, "CONNECT", pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_7x60::ParseEnterDataState() -  Did not get \"CONNECT\" response.\r\n");
            goto Error;
        }
        // Block the read thread and then flush the tty and the channel
        // From now, any failure will lead to DataConfigDown
        pChannelData->BlockAndFlushChannel(BLOCK_CHANNEL_BLOCK_ALL, FLUSH_CHANNEL_NO_FLUSH);
        pChannelData->FlushAndUnblockChannel(UNBLOCK_CHANNEL_UNBLOCK_TTY, FLUSH_CHANNEL_FLUSH_ALL);
    }
    else
    {
        if (!FindAndSkipString(pszRsp, "OK", pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_7x60::ParseEnterDataState() -  Did not get \"OK\" response.\r\n");
            goto Error;
        }
    }

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTE_INF_7x60::ParseEnterDataState() - Exit\r\n");
    return res;
}

// This method is a derivation from 6260
// For 7x60 XACT replace XRAT and add support of LTE.
// RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
//
RIL_RESULT_CODE CTE_INF_7x60::CoreSetPreferredNetworkType(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_7x60::CoreSetPreferredNetworkType() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_PreferredNetworkType networkType = PREF_NET_TYPE_GSM_WCDMA; // 0

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::CoreSetPreferredNetworkType() - Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != sizeof(RIL_PreferredNetworkType *))
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::CoreSetPreferredNetworkType() - Invalid data size.\r\n");
        goto Error;
    }

    networkType = ((RIL_PreferredNetworkType *)pData)[0];

    // if network type already set, NO-OP this command
    if (m_currentNetworkType == networkType)
    {
        rReqData.szCmd1[0] = '\0';
        res = RRIL_RESULT_OK;
        RIL_LOG_INFO("CTE_INF_7x60::CoreSetPreferredNetworkType() - Network type {%d} already set.\r\n", networkType);
        goto Error;
    }

    switch (networkType)
    {
        case PREF_NET_TYPE_LTE_GSM_WCDMA: // LTE Preferred

            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT=6,2,1\r", sizeof(rReqData.szCmd1)))
            {
                RIL_LOG_CRITICAL("CTE_INF_7x60::CoreSetPreferredNetworkType() - Can't construct szCmd1 networkType=%d\r\n", networkType);
                goto Error;
            }

           break;

        case PREF_NET_TYPE_GSM_WCDMA: // WCDMA Preferred

            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT=3,1\r", sizeof(rReqData.szCmd1)))
            {
                RIL_LOG_CRITICAL("CTE_INF_7x60::CoreSetPreferredNetworkType() - Can't construct szCmd1 networkType=%d\r\n", networkType);
                goto Error;
            }

           break;

        case PREF_NET_TYPE_GSM_ONLY: // GSM Only

            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT=0\r", sizeof(rReqData.szCmd1)))
            {
                RIL_LOG_CRITICAL("CTE_INF_7x60::CoreSetPreferredNetworkType() - Can't construct szCmd1 networkType=%d\r\n", networkType);
                goto Error;
            }

            break;

        case PREF_NET_TYPE_WCDMA: // WCDMA Only

            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT=1\r", sizeof(rReqData.szCmd1)))
            {
                RIL_LOG_CRITICAL("CTE_INF_7x60::CoreSetPreferredNetworkType() - Can't construct szCmd1 networkType=%d\r\n", networkType);
                goto Error;
            }

            break;

        case PREF_NET_TYPE_LTE_ONLY: // LTE Only

            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT=2\r", sizeof(rReqData.szCmd1)))
            {
                RIL_LOG_CRITICAL("CTE_INF_7x60::CoreSetPreferredNetworkType() - Can't construct szCmd1 networkType=%d\r\n", networkType);
                goto Error;
            }

            break;

        default:
            RIL_LOG_CRITICAL("CTE_INF_7x60::CoreSetPreferredNetworkType() - Undefined rat code: %d\r\n", networkType);
            res = RIL_E_MODE_NOT_SUPPORTED;
            goto Error;

    }

    //  Set the context of this command to the network type we're attempting to set
    rReqData.pContextData = (void*)networkType;  // Store this as an int.

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTE_INF_7x60::CoreSetPreferredNetworkType() - Exit\r\n");
    return res;
}


//
// RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
//
RIL_RESULT_CODE CTE_INF_7x60::CoreGetPreferredNetworkType(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_INF_7x60::CoreGetPreferredNetworkType() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTE_INF_7x60::CoreGetPreferredNetworkType() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_7x60::ParseGetPreferredNetworkType(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_INF_7x60::ParseGetPreferredNetworkType() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;

    UINT32 rat = 0;
    UINT32 pref = 0;

    int * pRat = (int*)malloc(sizeof(int));
    if (NULL == pRat)
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::ParseGetPreferredNetworkType() - Could not allocate memory for response.\r\n");
        goto Error;
    }

    // Skip "<prefix>"
    if (!SkipRspStart(pszRsp, m_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::ParseGetPreferredNetworkType() - Could not skip response prefix.\r\n");
        goto Error;
    }

    // Skip "+XACT: "
    if (!SkipString(pszRsp, "+XACT: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::ParseGetPreferredNetworkType() - Could not skip \"+XACT: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt32(pszRsp, rat, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::ParseGetPreferredNetworkType() - Could not extract rat value.\r\n");
        goto Error;
    }

    if (FindAndSkipString(pszRsp, ",", pszRsp))
    {
        if (!ExtractUInt32(pszRsp, pref, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_7x60::ParseGetPreferredNetworkType() - Could not find and skip pref value even though it was expected.\r\n");
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
            RIL_LOG_CRITICAL("CTE_INF_7x60::ParseGetPreferredNetworkType() - Unexpected rat found: %d. Failing out.\r\n", rat);
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


    RIL_LOG_VERBOSE("CTE_INF_7x60::ParseGetPreferredNetworkType() - Exit\r\n");
    return res;
}

BOOL CTE_INF_7x60::SetupInterface(UINT32 uiCID)
{
    RIL_LOG_VERBOSE("CTE_INF_7x60::SetupInterface() - Enter\r\n");

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

    const UINT32 NW_IF_PDP_MUX_OFFSET = 3;
    const UINT32 NW_IF_PDP_HSI_DIRECT_OFFSET = 2;

    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::SetupInterface() - No Data Channel for CID %u.\r\n",
                                                                    uiCID);
        goto Error;
    }

    state = pChannelData->GetDataState();
    if (E_DATA_STATE_ACTIVE != state)
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::SetupInterface() - Invalid data state %d.\r\n",
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
            networkInterfaceID = NW_IF_PDP_MUX_OFFSET + RIL_DATA_PROFILE_DEFAULT;
            break;
        case RIL_DATA_PROFILE_TETHERED:
            networkInterfaceID = NW_IF_PDP_MUX_OFFSET + RIL_DATA_PROFILE_TETHERED;
            break;
        case RIL_DATA_PROFILE_IMS:
            networkInterfaceID = NW_IF_PDP_MUX_OFFSET + RIL_DATA_PROFILE_IMS;
            break;
        case RIL_DATA_PROFILE_MMS:
            networkInterfaceID = NW_IF_PDP_MUX_OFFSET + RIL_DATA_PROFILE_MMS;
            break;
        case RIL_DATA_PROFILE_CBS:
            networkInterfaceID = NW_IF_PDP_MUX_OFFSET + RIL_DATA_PROFILE_CBS;
            break;
        case RIL_DATA_PROFILE_FOTA:
            networkInterfaceID = NW_IF_PDP_MUX_OFFSET + RIL_DATA_PROFILE_FOTA;
            break;
        case RIL_DATA_PROFILE_SUPL:
            networkInterfaceID = NW_IF_PDP_MUX_OFFSET + RIL_DATA_PROFILE_SUPL;
            break;
        case RIL_DATA_PROFILE_HIPRI:
            networkInterfaceID = NW_IF_PDP_MUX_OFFSET + RIL_DATA_PROFILE_HIPRI;
            break;
        default:
            RIL_LOG_CRITICAL("CTE_INF_7x60::SetupInterface() - Unknown Data Profile [%d] \r\n",
                                                                dataProfile);
            goto Error;
        }
    }
    else
    {
        // First network interface is rmnet0 for pdp directly over hsi
        switch (hsiChannel)
        {
        case RIL_HSI_CHANNEL1:
            networkInterfaceID = RIL_HSI_CHANNEL1 - NW_IF_PDP_HSI_DIRECT_OFFSET;
            break;
        case RIL_HSI_CHANNEL2:
            networkInterfaceID = RIL_HSI_CHANNEL2 - NW_IF_PDP_HSI_DIRECT_OFFSET;
            break;
        case RIL_HSI_CHANNEL3:
            networkInterfaceID = RIL_HSI_CHANNEL3 - NW_IF_PDP_HSI_DIRECT_OFFSET;
            break;
        default:
            RIL_LOG_CRITICAL("CTE_INF_7x60::SetupInterface() - Unknown his channel [%d] \r\n",
                                                    hsiChannel);
            goto Error;
        }
    }


    if (!PrintStringNullTerminate(szNetworkInterfaceName, sizeof(szNetworkInterfaceName),
                                        "%s%d", m_szNetworkInterfaceNamePrefix,
                                                            networkInterfaceID))
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::SetupInterface() - Cannot set network interface name\r\n");
        goto Error;
    }
    else
    {
        RIL_LOG_INFO("CTE_INF_7x60::SetupInterface() - szNetworkInterfaceName=[%s], CID=[%u]\r\n",
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
            RIL_LOG_INFO("CTE_INF_7x60::SetupInterface() - ***** PUTTING channel=[%u] in DATA MODE *****\r\n",
                                                                    uiChannel);
            ret = ioctl( fd, GSMIOC_ENABLE_NET, &netconfig );       // Enable data channel
            if (ret < 0)
            {
                RIL_LOG_CRITICAL("CTE_INF_7x60::SetupInterface() - Unable to create interface %s : %s \r\n",
                                            netconfig.if_name, strerror(errno));
                goto Error;
            }
        }
        else
        {
            //  No FD.
            RIL_LOG_CRITICAL("CTE_INF_7x60::SetupInterface() - Could not get Data Channel chnl=[%u] fd=[%d].\r\n",
                                                                uiChannel, fd);
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
        RIL_LOG_CRITICAL("CTE_INF_7x60::SetupInterface() - Unable to bringup interface ifconfig\r\n");
        goto Error;
    }

    bRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CTE_INF_7x60::SetupInterface() Exit\r\n");
    return bRet;
}

//
//  Call this whenever data is disconnected
//
BOOL CTE_INF_7x60::DataConfigDown(UINT32 uiCID)
{
    RIL_LOG_VERBOSE("CTE_INF_7x60::DataConfigDown() - Enter\r\n");

    //  First check to see if uiCID is valid
    if (uiCID > MAX_PDP_CONTEXTS || uiCID == 0)
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::DataConfigDown() - Invalid CID = [%u]\r\n", uiCID);
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

    //  See if CID passed in is valid
    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (NULL == pChannelData)
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::DataConfigDown() - Invalid CID=[%u], no data channel found!\r\n",
                                                                        uiCID);
        return FALSE;
    }

    uiRilChannel = pChannelData->GetRilChannel();
    bIsHSIDirect = pChannelData->IsHSIDirect();
    pChannelData->GetInterfaceName(szNetworkInterfaceName,
                                            sizeof(szNetworkInterfaceName));

    RIL_LOG_INFO("CTE_INF_7x60::DataConfigDown() - szNetworkInterfaceName=[%s]  CID=[%u]\r\n",
                                                szNetworkInterfaceName, uiCID);

    // Reset ContextID to 0, to free up the channel for future use
    RIL_LOG_INFO("CTE_INF_7x60::DataConfigDown() - ****** Setting chnl=[%u] to CID=[0] ******\r\n",
                                                                uiRilChannel);

    fd = pChannelData->GetFD();

    if (!bIsHSIDirect)
    {
        // Blocking TTY flow.
        // Security in order to avoid IP data in response buffer.
        // Not mandatory.
        pChannelData->BlockAndFlushChannel(BLOCK_CHANNEL_BLOCK_TTY, FLUSH_CHANNEL_NO_FLUSH);

        //  Put the channel back into AT command mode
        netconfig.adaption = 3;
        netconfig.protocol = htons(ETH_P_IP);

        if (fd >= 0)
        {
            RIL_LOG_INFO("CTE_INF_7x60::DataConfigDown() - ***** PUTTING channel=[%u] in AT COMMAND MODE *****\r\n",
                                                                uiRilChannel);
            ret = ioctl(fd, GSMIOC_DISABLE_NET, &netconfig);
        }
    }
    else
    {
        /*
         * HSI inteface ENABLE/DISABLE can be done only by the HSI driver.
         * Rapid RIL can only bring up or down the interface.
         */
        RIL_LOG_INFO("CTE_INF_7x60::DataConfigDown() : Bring down hsi network interface\r\n");

        //  Open socket for ifconfig and setFlags commands
        s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0)
        {
            RIL_LOG_CRITICAL("CTE_INF_7x60::DataConfigDown() : cannot open control socket\n");
            goto Error;
        }

        struct ifreq ifr;
        memset(&ifr, 0, sizeof(struct ifreq));
        strncpy(ifr.ifr_name, szNetworkInterfaceName, IFNAMSIZ-1);
        ifr.ifr_name[IFNAMSIZ-1] = '\0';
        if (!setflags(s, &ifr, 0, IFF_UP))
        {
            RIL_LOG_CRITICAL("CTE_INF_7x60::DataConfigDown() : Error setting flags\r\n");
        }
    }
    pChannelData->ResetDataCallInfo();

    bRet = TRUE;

    RIL_LOG_INFO("[RIL STATE] PDP CONTEXT DEACTIVATION chnl=%d\r\n", pChannelData->GetRilChannel());

Error:
    if (!bIsHSIDirect)
    {
        // Flush buffers and Unblock read thread.
        // Security in order to avoid IP data in response buffer.
        // Will unblock Channel read thread and TTY.
        // Unblock read thread whatever the result is to avoid forever block
        pChannelData->FlushAndUnblockChannel(UNBLOCK_CHANNEL_UNBLOCK_ALL, FLUSH_CHANNEL_FLUSH_ALL);
    }

    RIL_LOG_INFO("CTE_INF_7x60::DataConfigDown() EXIT  bRet=[%d]\r\n", bRet);
    return bRet;
}
