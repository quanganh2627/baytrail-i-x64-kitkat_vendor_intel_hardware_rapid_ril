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
#include "../../util.h"
#include "extract.h"
#include "rillog.h"
#include "te.h"
#include "../../globals.h"
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

CTE_INF_7x60::CTE_INF_7x60()
:CTE_INF_6260(),
m_currentNetworkType(-1)
{

}

CTE_INF_7x60::~CTE_INF_7x60()
{

}

// RIL_REQUEST_SETUP_DATA_CALL 27
RIL_RESULT_CODE CTE_INF_7x60::CoreSetupDataCall(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize, UINT32 uiCID)
{
    RIL_LOG_VERBOSE("CTE_INF_7x60::CoreSetupDataCall() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    char szIPV4V6[] = "IPV4V6";
    PdpData stPdpData;
    memset(&stPdpData, 0, sizeof(PdpData));

    int muxControlChannel = -1;
    // Get Channel Data according to CID
    CChannel_Data* pChannelData = NULL;
    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::CoreSetupDataCall() - Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize < (6 * sizeof(char*)))
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::CoreSetupDataCall() - Invalid data size. Was given %d bytes\r\n", uiDataSize);
        goto Error;
    }

    RIL_LOG_INFO("CTE_INF_7x60::CoreSetupDataCall() - uiDataSize=[%d]\r\n", uiDataSize);


    // extract data
    stPdpData.szRadioTechnology = ((char**)pData)[0];  // not used
    stPdpData.szRILDataProfile  = ((char**)pData)[1];
    stPdpData.szApn             = ((char**)pData)[2];
    stPdpData.szUserName        = ((char**)pData)[3];  // not used
    stPdpData.szPassword        = ((char**)pData)[4];  // not used
    stPdpData.szPAPCHAP         = ((char**)pData)[5];  // not used

    RIL_LOG_INFO("CTE_INF_7x60::CoreSetupDataCall() - stPdpData.szRadioTechnology=[%s]\r\n", stPdpData.szRadioTechnology);
    RIL_LOG_INFO("CTE_INF_7x60::CoreSetupDataCall() - stPdpData.szRILDataProfile=[%s]\r\n", stPdpData.szRILDataProfile);
    RIL_LOG_INFO("CTE_INF_7x60::CoreSetupDataCall() - stPdpData.szApn=[%s]\r\n", stPdpData.szApn);
    RIL_LOG_INFO("CTE_INF_7x60::CoreSetupDataCall() - stPdpData.szUserName=[%s]\r\n", stPdpData.szUserName);
    RIL_LOG_INFO("CTE_INF_7x60::CoreSetupDataCall() - stPdpData.szPassword=[%s]\r\n", stPdpData.szPassword);
    RIL_LOG_INFO("CTE_INF_7x60::CoreSetupDataCall() - stPdpData.szPAPCHAP=[%s]\r\n", stPdpData.szPAPCHAP);
    RIL_LOG_INFO("CTE_INF_7x60::CoreSetupDataCall() - m_hsiDirect=[%d]\r\n", pChannelData->m_hsiDirect);

    if (RIL_VERSION >= 4 && (uiDataSize >= (7 * sizeof(char*))))
    {
        stPdpData.szPDPType         = ((char **)pData)[6];  // new in Android 2.3.4.
        RIL_LOG_INFO("CTE_INF_7x60::CoreSetupDataCall() - stPdpData.szPDPType=[%s]\r\n", stPdpData.szPDPType);
    }

    // For setting up data call we need to send 2 sets of chained commands: AT+CGDCONT to define PDP Context, then
    // if RAW IP is used send AT+CGDATA to enable Raw IP on data channel (which will then switch the channel to data mode).
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
            "AT+CGDCONT=%d,\"%s\",\"%s\",,0,0;+XDNS=%d,1\r", uiCID, stPdpData.szPDPType,
            stPdpData.szApn, uiCID))
        {
            RIL_LOG_CRITICAL("CTE_INF_7x60::CoreSetupDataCall() - cannot create CGDCONT command, stPdpData.szPDPType\r\n");
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
            RIL_LOG_CRITICAL("CTE_INF_7x60::CoreSetupDataCall() - cannot create CGDCONT command, stPdpData.szPDPType\r\n");
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
            RIL_LOG_CRITICAL("CTE_INF_7x60::CoreSetupDataCall() - cannot create CGDCONT command, stPdpData.szPDPType\r\n");
            goto Error;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::CoreSetupDataCall() - Wrong PDP type\r\n");
        goto Error;
    }


    if (!pChannelData->m_hsiDirect)
    {
        if (!PrintStringNullTerminate(rReqData.szCmd2, sizeof(rReqData.szCmd2),
            "AT+CGACT=1,%d;+CGDATA=\"M-RAW_IP\",%d\r", uiCID, uiCID))
        {
            RIL_LOG_CRITICAL("CTE_INF_7x60::CoreSetupDataCall() -  cannot create CGDATA command\r\n");
            goto Error;
        }
    }
    else
    {

        if (pChannelData->m_hsiChannel < 0)
        {
            RIL_LOG_CRITICAL("CTE_INF_7x60::CoreSetupDataCall() - No free HSI Channel \r\n");
            goto Error;
        }

       // Get the hsi channel id
        int hsiNetworkPath = -1;
        switch (pChannelData->m_hsiChannel)
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
                RIL_LOG_CRITICAL("CTE_INF_7x60::CoreSetupDataCall() - Unknown HSI Channel [%d] \r\n", pChannelData->m_hsiChannel);
                goto Error;
        }

       // Get the mux  channel id corresponding to the control of the data channel
        switch (pChannelData->GetRilChannel())
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
                RIL_LOG_CRITICAL("CTE_INF_7x60::CoreSetupDataCall() - Unknown mux channel for RIL Channel [%d] \r\n", pChannelData->GetRilChannel());
                goto Error;
        }

        if (!PrintStringNullTerminate(rReqData.szCmd2, sizeof(rReqData.szCmd2),
            "AT+CGACT=1,%d;+XDATACHANNEL=1,1,\"/mux/%d\",\"/mipi_ipc/%d\",0;+CGDATA=\"M-RAW_IP\",%d\r",
            uiCID, muxControlChannel, hsiNetworkPath, uiCID))
        {
            RIL_LOG_CRITICAL("CTE_INF_7x60::CoreSetupDataCall() -  cannot create CGDATA command\r\n");
            goto Error;
        }
    }

    //  Store the CID in  pContext

    rReqData.pContextData = (void*)uiCID;
    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTE_INF_7x60::CoreSetupDataCall() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_INF_7x60::ParseSetupDataCall(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_INF_7x60::ParseSetupDataCall() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char szIP[PROPERTY_VALUE_MAX] = {0};
    P_ND_SETUP_DATA_CALL pDataCallRsp = NULL;

    int networkInterfaceID = -1;

    /*
     * For RAW IP, when we get the CONNECT response to AT+CGDATA, we then need
     * to send AT+CGPADDR (or AT+CGDCONT?) to get the IP address which needs to
     * be returned in the response to the RIL_REQUEST_SETUP_DATA_CALL.
     * DNS address of the activated context is got from AT+XDNS?
     * Response to RIL_REQUEST_SETUP_DATA_CALL is sent only after the IP
     * address and DNS address is got.
     */

    struct gsm_netconfig netconfig;

    CCommand* pCmd1 = NULL;
    CCommand* pCmd2 = NULL;
    const int nIPADDR_TIMEOUT_DEFAULT = 5000, nDNS_TIMEOUT_DEFAULT = 5000;
    int nCommandTimeout = 5000;
    UINT32 uiWaitRes;
    char szCmd[MAX_BUFFER_SIZE];
    CChannel_Data* pChannelData = NULL;
    UINT32 uiCID = 0;
    int fd = -1;
    int ret = 0;
    CRepository repository;
    PDP_TYPE eDataConnectionType = PDP_TYPE_IPV4;  //  dummy for now, set to IPv4.
    const char* szRsp = rRspData.szResponse;

    pDataCallRsp = (P_ND_SETUP_DATA_CALL)malloc(sizeof(S_ND_SETUP_DATA_CALL));
    if (NULL == pDataCallRsp)
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - Cannot allocate pDataCallRsp  size=[%d]\r\n", sizeof(S_ND_SETUP_DATA_CALL));
        goto Error;
    }
    memset(pDataCallRsp, 0, sizeof(S_ND_SETUP_DATA_CALL));

    //  Get CID
    uiCID = (UINT32)rRspData.pContextData;

    if (uiCID == 0)
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - CID must be >= 1!! CID=[%u]\r\n", uiCID);
        goto Error;
    }

    // Get Channel Data according to CID
    pChannelData = CChannel_Data::GetChnlFromContextID(uiCID);
    if (!pChannelData)
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - ERROR: Could not get Data Channel for RIL channel number %d.\r\n", rRspData.uiChannel);
        goto Error;
    }

    m_dataCallFailCause = PDP_FAIL_ERROR_UNSPECIFIED;
    if (FindAndSkipString(szRsp, "ERROR", szRsp) ||
        FindAndSkipString(szRsp, "+CME ERROR:", szRsp))
    {
        QueryDataCallFailCause();

        RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - ERROR/CME ERROR occured\r\n");
        goto Error;
    }

    // Confirm we got "CONNECT" for data over MUX
    // Confirm we get "OK" for data directly over hsi
    if (!pChannelData->m_hsiDirect)
    {
        if (!FindAndSkipString(szRsp, "CONNECT", szRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() -  Did not get \"CONNECT\" response.\r\n");
            goto Error;
        }
    }
    else
    {
        if (!FindAndSkipString(szRsp, "OK", szRsp))
        {
            RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() -  Did not get \"OK\" response.\r\n");
            goto Error;
        }
    }

    // Following code-block is moved up here from the end of this function to get if_name needed for netconfig (N_GSM)
    // But the IP address is filled in end of function.

    //  Populate pDataCallRsp
    pDataCallRsp->sPDPData.cid = uiCID;

    RIL_LOG_INFO("CTE_INF_7x60::ParseSetupDataCall() - uiDataProfile =[%d]\r\n", pChannelData->m_dataProfile);

    if (!pChannelData->m_hsiDirect){
        // First network interface is rmnet3 for pdp over mux
        switch (pChannelData->m_dataProfile)
        {
            case RIL_DATA_PROFILE_DEFAULT:
                networkInterfaceID = NETWORK_INTERFACE_PDP_MUX_OFFSET + RIL_DATA_PROFILE_DEFAULT;
                break;
            case RIL_DATA_PROFILE_TETHERED:
                networkInterfaceID = NETWORK_INTERFACE_PDP_MUX_OFFSET + RIL_DATA_PROFILE_TETHERED;
                break;
            case RIL_DATA_PROFILE_IMS:
                networkInterfaceID = NETWORK_INTERFACE_PDP_MUX_OFFSET + RIL_DATA_PROFILE_IMS;
                break;
            case RIL_DATA_PROFILE_MMS:
                networkInterfaceID = NETWORK_INTERFACE_PDP_MUX_OFFSET + RIL_DATA_PROFILE_MMS;
                break;
            case RIL_DATA_PROFILE_CBS:
                networkInterfaceID = NETWORK_INTERFACE_PDP_MUX_OFFSET + RIL_DATA_PROFILE_CBS;
                break;
            case RIL_DATA_PROFILE_FOTA:
                networkInterfaceID = NETWORK_INTERFACE_PDP_MUX_OFFSET + RIL_DATA_PROFILE_FOTA;
                break;
            case RIL_DATA_PROFILE_SUPL:
                networkInterfaceID = NETWORK_INTERFACE_PDP_MUX_OFFSET + RIL_DATA_PROFILE_SUPL;
                break;
            case RIL_DATA_PROFILE_HIPRI:
                networkInterfaceID = NETWORK_INTERFACE_PDP_MUX_OFFSET + RIL_DATA_PROFILE_HIPRI;
                break;
            default:
                RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - Unknown Data Profile [%d] \r\n", pChannelData->m_dataProfile);
                goto Error;
        }
    }
    else
    {
        // First network interface is rmnet0 for pdp directly over hsi
        switch (pChannelData->m_hsiChannel)
        {
            case RIL_HSI_CHANNEL1:
                networkInterfaceID = RIL_HSI_CHANNEL1 - NETWORK_INTERFACE_PDP_HSI_DIRECT_OFFSET;
                break;
            case RIL_HSI_CHANNEL2:
                networkInterfaceID = RIL_HSI_CHANNEL2 - NETWORK_INTERFACE_PDP_HSI_DIRECT_OFFSET;
                break;
            case RIL_HSI_CHANNEL3:
                networkInterfaceID = RIL_HSI_CHANNEL3 - NETWORK_INTERFACE_PDP_HSI_DIRECT_OFFSET;
                break;
            default:
                RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - Unknown his channel [%d] \r\n", pChannelData->m_hsiChannel);
                goto Error;
        }
    }

    if (!PrintStringNullTerminate(pDataCallRsp->szNetworkInterfaceName, MAX_BUFFER_SIZE, "%s%d", m_szNetworkInterfaceNamePrefix, networkInterfaceID))
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - Cannot set network interface name\r\n");
        goto Error;
    }
    else
    {
        RIL_LOG_INFO("CTE_INF_7x60::ParseSetupDataCall() - szNetworkInterfaceName=[%s], CID=[%u]\r\n", pDataCallRsp->szNetworkInterfaceName, uiCID);
    }

    pDataCallRsp->sPDPData.ifname = pDataCallRsp->szNetworkInterfaceName;

    //  Store interface name in pChannelData
    pChannelData->m_szInterfaceName = new char[MAX_BUFFER_SIZE];
    if (NULL == pChannelData->m_szInterfaceName)
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - Could not allocate interface name for Data Channel chnl=[%d] fd=[%d].\r\n", rRspData.uiChannel, fd);
        goto Error;
    }
    strncpy(pChannelData->m_szInterfaceName, pDataCallRsp->szNetworkInterfaceName, MAX_BUFFER_SIZE-1);
    pChannelData->m_szInterfaceName[MAX_BUFFER_SIZE-1] = '\0';  //  KW fix
    RIL_LOG_INFO("CTE_INF_7x60::ParseSetupDataCall() - networkInterfaceName =[%s]\r\n", pChannelData->m_szInterfaceName);

    if (!pChannelData->m_hsiDirect)
    {
      // N_GSM related code
      netconfig.adaption = 3;
      netconfig.protocol = htons(ETH_P_IP);
      strncpy(netconfig.if_name, pDataCallRsp->szNetworkInterfaceName, IFNAMSIZ-1);
      netconfig.if_name[IFNAMSIZ-1] = '\0';  //  KW fix

      // Add IF NAME
      fd = pChannelData->GetFD();
      if (fd >= 0)
      {
          RIL_LOG_INFO("CTE_INF_7x60::ParseSetupDataCall() - ***** PUTTING channel=[%d] in DATA MODE *****\r\n", rRspData.uiChannel);
          ret = ioctl( fd, GSMIOC_ENABLE_NET, &netconfig );       // Enable data channel
          if (ret < 0)
          {
              RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - Unable to create interface %s : %s \r\n",netconfig.if_name,strerror(errno));
              goto Error;
          }
      }
      else
      {
          //  No FD.
          RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - Could not get Data Channel chnl=[%d] fd=[%d].\r\n", rRspData.uiChannel, fd);
          goto Error;
      }
    }

    // Send AT+CGPADDR and AT+XDNS? commands to query for assigned IP Address and DNS and wait for responses

    CEvent::Reset(pChannelData->m_pSetupIntermediateEvent);

    if (!PrintStringNullTerminate(szCmd, MAX_BUFFER_SIZE, "AT+CGPADDR=%u\r", uiCID))
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - cannot create CGPADDR command\r\n");
        goto Error;
    }

    pCmd1 = new CCommand(g_arChannelMapping[ND_REQ_ID_GETIPADDRESS], NULL, ND_REQ_ID_GETIPADDRESS, szCmd, &CTE::ParseIpAddress);
    if (pCmd1)
    {
        if (!CCommand::AddCmdToQueue(pCmd1))
        {
            RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - Unable to queue AT+CGPADDR command!\r\n");
            delete pCmd1;
            pCmd1 = NULL;
            goto Error;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - Unable to allocate memory for new AT+CGPADDR command!\r\n");
        goto Error;
    }

    //  Now wait for the intermediate command to complete.
    //  Get nCommandTimeout
    if (!repository.Read(g_szGroupRequestTimeouts, g_szRequestNames[ND_REQ_ID_GETIPADDRESS], nCommandTimeout))
    {
        nCommandTimeout = nIPADDR_TIMEOUT_DEFAULT;
    }

    RIL_LOG_INFO("CTE_INF_7x60::ParseSetupDataCall() - Wait for intermediate response, nCommandTimeout=[%d]\r\n",
                    nCommandTimeout);
    uiWaitRes = CEvent::Wait(pChannelData->m_pSetupIntermediateEvent, nCommandTimeout);
    switch(uiWaitRes)
    {
        case WAIT_EVENT_0_SIGNALED:
            RIL_LOG_INFO("CTE_INF_7x60::ParseSetupDataCall() : SetupData intermediate event signalled\r\n");
            RIL_LOG_INFO("m_szIpAddr = [%s]    m_szIpAddr2 = [%s]\r\n",
                pChannelData->m_szIpAddr, pChannelData->m_szIpAddr2);

            //  If the IP address command timed-out, then the data channel's IP addr
            //  buffers will be NULL.
            if (NULL == pChannelData->m_szIpAddr || NULL == pChannelData->m_szIpAddr2)
            {
                 RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - IP addresses are null\r\n");
                 goto Error;
            }

            // pChannelData->m_szIpAddr and pChannelData->m_szIpAddr2 can not be null because if initialization fails
            // the connection manager do not call this function but they can be empty
            if (0 == pChannelData->m_szIpAddr[0] && 0 == pChannelData->m_szIpAddr2[0])
            {
                 RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - IP addresses are empty\r\n");
                 goto Error;
            }
            break;

        case WAIT_TIMEDOUT:
             RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - Timed out waiting for IP Address\r\n");
             goto Error;

        default:
             RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - Unexpected event result on Wait for IP Address, res: %d\r\n", uiWaitRes);
             goto Error;

    }

    CEvent::Reset(pChannelData->m_pSetupDoneEvent);

    if (!PrintStringNullTerminate(szCmd, MAX_BUFFER_SIZE, "AT+XDNS?;+XDNS=%u,0\r", uiCID))
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - Cannot create XDNS command\r\n");
        goto Error;
    }

    pCmd2 = new CCommand(g_arChannelMapping[ND_REQ_ID_GETDNS], NULL, ND_REQ_ID_GETDNS, szCmd, &CTE::ParseDns);
    if (pCmd2)
    {
        //  Need to send context ID to parse function so set contextData for this command
        pCmd2->SetContextData(rRspData.pContextData);
        if (!CCommand::AddCmdToQueue(pCmd2))
        {
            RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - Unable to queue AT+XDNS? command!\r\n");
            delete pCmd2;
            pCmd2 = NULL;
            goto Error;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - Unable to allocate memory for new AT+XDNS? command!\r\n");
        goto Error;
    }

    //  Get nCommentTimeout
    //  Wait for DNS command (m_pSetupDoneEvent)
    if (!repository.Read(g_szGroupRequestTimeouts, g_szRequestNames[ND_REQ_ID_GETDNS], nCommandTimeout))
    {
        nCommandTimeout = nDNS_TIMEOUT_DEFAULT;
    }

    RIL_LOG_INFO("CTE_INF_7x60::ParseSetupDataCall() - Wait for DNS response, nCommandTimeout=[%d]\r\n",
                    nCommandTimeout);
    uiWaitRes = CEvent::Wait(pChannelData->m_pSetupDoneEvent, nCommandTimeout);
    switch (uiWaitRes)
    {
        case WAIT_EVENT_0_SIGNALED:
            RIL_LOG_INFO("CTE_INF_7x60::ParseSetupDataCall() : SetupData event signalled\r\n");
            RIL_LOG_INFO("m_szDNS1 = [%s]    m_szDNS2 = [%s]\r\n",
                pChannelData->m_szDNS1, pChannelData->m_szDNS2);

            if (NULL == pChannelData->m_szDNS1)
            {
                 RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - DNS1 is null\r\n");
                 goto Error;
            }
            if (NULL == pChannelData->m_szDNS2)
            {
                 RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - DNS2 is null\r\n");
                 goto Error;
            }
            if (NULL == pChannelData->m_szPdpType)
            {
                pChannelData->m_szPdpType = new char[sizeof("IPV4V6")];
                if (NULL == pChannelData->m_szPdpType)
                {
                    RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - cannot allocate m_szPdpType!\r\n");
                    goto Error;
                }
            }

            if (0 == pChannelData->m_szIpAddr[0])
            {
                strncpy(szIP, pChannelData->m_szIpAddr2, PROPERTY_VALUE_MAX-1);
                szIP[PROPERTY_VALUE_MAX-1] = '\0';  //  KW fix
            }
            else
            {
                strncpy(szIP, pChannelData->m_szIpAddr, PROPERTY_VALUE_MAX-1);
                szIP[PROPERTY_VALUE_MAX-1] = '\0';  //  KW fix
            }

            if (pChannelData->m_szIpAddr2[0] == 0)
            {
                eDataConnectionType = PDP_TYPE_IPV4;
                strcpy(pChannelData->m_szPdpType, "IPV4");
            }
            else if (pChannelData->m_szIpAddr[0] == 0)
            {
                eDataConnectionType = PDP_TYPE_IPV6;
                strcpy(pChannelData->m_szPdpType, "IPV6");
            }
            else
            {
                eDataConnectionType = PDP_TYPE_IPV4V6;
                strcpy(pChannelData->m_szPdpType, "IPV4V6");
            }
            break;

        case WAIT_TIMEDOUT:
             RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - Timed out waiting for IP Address and DNS\r\n");
             goto Error;

        default:
             RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - Unexpected event result on Wait for IP Address and DNS, res: %d\r\n", uiWaitRes);
             goto Error;
    }

    // set interface address(es) and bring up interface
    if (!DataConfigUp(pDataCallRsp->szNetworkInterfaceName, pChannelData, eDataConnectionType))
    {
        RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall() - Unable to set ifconfig\r\n");
        goto Error;
    }

    snprintf(pDataCallRsp->szDNS, MAX_BUFFER_SIZE-1, "%s %s %s %s",
        (pChannelData->m_szDNS1 ? pChannelData->m_szDNS1 : ""),
        (pChannelData->m_szDNS2 ? pChannelData->m_szDNS2 : ""),
        (pChannelData->m_szIpV6DNS1 ? pChannelData->m_szIpV6DNS1 : ""),
        (pChannelData->m_szIpV6DNS2 ? pChannelData->m_szIpV6DNS2 : ""));
    pDataCallRsp->szDNS[MAX_BUFFER_SIZE-1] = '\0';  //  KW fix

    snprintf(pDataCallRsp->szIPAddress, MAX_BUFFER_SIZE-1, "%s %s",
        (pChannelData->m_szIpAddr ? pChannelData->m_szIpAddr : ""),
        (pChannelData->m_szIpAddr2 ? pChannelData->m_szIpAddr2 : ""));
    pDataCallRsp->szIPAddress[MAX_BUFFER_SIZE-1] = '\0';  //  KW fix

    strncpy(pDataCallRsp->szGateway, pChannelData->m_szIpGateways, MAX_BUFFER_SIZE-1);
    pDataCallRsp->szGateway[MAX_BUFFER_SIZE-1] = '\0';  //  KW fix
    strncpy(pDataCallRsp->szPdpType, pChannelData->m_szPdpType, MAX_BUFFER_SIZE-1);
    pDataCallRsp->szPdpType[MAX_BUFFER_SIZE-1] = '\0';  //  KW fix

    pDataCallRsp->sPDPData.status = pChannelData->m_iStatus;
    pDataCallRsp->sPDPData.suggestedRetryTime = -1;

    pDataCallRsp->sPDPData.active = 2;
    pDataCallRsp->sPDPData.type = pDataCallRsp->szPdpType;
    pDataCallRsp->sPDPData.addresses = pDataCallRsp->szIPAddress;
    pDataCallRsp->sPDPData.dnses = pDataCallRsp->szDNS;
    pDataCallRsp->sPDPData.gateways = pDataCallRsp->szGateway;

    res = RRIL_RESULT_OK;

    RIL_LOG_INFO("[RIL STATE] PDP CONTEXT ACTIVATION chnl=%d ContextID=%d\r\n",
        pChannelData->GetRilChannel(), pChannelData->GetContextID());

Error:
    if (RRIL_RESULT_OK != res)
    {
        RIL_LOG_INFO("CTE_INF_7x60::ParseSetupDataCall() - Error cleanup\r\n");
        if (pChannelData)
        {
            RIL_LOG_INFO("CTE_INF_7x60::ParseSetupDataCall() - Calling DataConfigDown  chnl=[%d], cid=[%u]\r\n", rRspData.uiChannel, uiCID);

            //  Explicitly deactivate context ID = uiCID
            //  This will call DataConfigDown(uiCID) and convert channel to AT mode.
            //  Otherwise the data channel hangs and is unresponsive.
            if (!DataConfigDown(pChannelData->GetContextID()))
            {
                RIL_LOG_CRITICAL("CTE_INF_7x60::ParseSetupDataCall - DataConfigDown FAILED cid=[%d]\r\n", pChannelData->GetContextID());
            }
        }

        if (pDataCallRsp)
        {
            // If pDataCallRsp, then cause can be sent in status field.
            res = RRIL_RESULT_OK;
            pDataCallRsp->sPDPData.status = m_dataCallFailCause;
            pDataCallRsp->sPDPData.cid = uiCID;
        }
        else
        {
            res = RIL_E_GENERIC_FAILURE;
        }
    }

    if (pDataCallRsp)
    {
        rRspData.pData = (void*)pDataCallRsp;
        rRspData.uiDataSize = sizeof(RIL_Data_Call_Response_v6);
    }

    RIL_LOG_INFO("CTE_INF_7x60::ParseSetupDataCall() - Exit\r\n");
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
            res = RIL_E_GENERIC_FAILURE;
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
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
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

