////////////////////////////////////////////////////////////////////////////
// te_inf_6260.h
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

#ifndef RRIL_TE_INF_6260_H
#define RRIL_TE_INF_6260_H

#include "te_base.h"
#include "rril.h"
#include "channel_data.h"

class CEvent;

class CTE_INF_6260 : public CTEBase
{
public:

    CTE_INF_6260();
    virtual ~CTE_INF_6260();

private:
    //  Prevent assignment: Declared but not implemented.
    CTE_INF_6260(const CTE_INF_6260& rhs);  // Copy Constructor
    CTE_INF_6260& operator=(const CTE_INF_6260& rhs);  //  Assignment operator

    int m_nCurrentNetworkType;

protected:
    char m_szNetworkInterfaceNamePrefix[MAX_BUFFER_SIZE];

    //  Locally store CPIN2 query result for SIM_IO
    char m_szCPIN2Result[MAX_BUFFER_SIZE];

    CEvent *m_pQueryPIN2Event;

public:
    // modem overrides

    // RIL_REQUEST_GET_SIM_STATUS 1
    virtual RIL_RESULT_CODE CoreGetSimStatus(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGetSimStatus(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SIGNAL_STRENGTH 19
    //virtual RIL_RESULT_CODE ParseSignalStrength(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SETUP_DATA_CALL 27
    virtual RIL_RESULT_CODE CoreSetupDataCall(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize, UINT32 uiCID);
    virtual RIL_RESULT_CODE ParseSetupDataCall(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SIM_IO 28
    virtual RIL_RESULT_CODE CoreSimIo(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSimIo(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_DEACTIVATE_DATA_CALL 41
    virtual RIL_RESULT_CODE CoreDeactivateDataCall(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseDeactivateDataCall(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_OEM_HOOK_RAW 59
    virtual RIL_RESULT_CODE CoreHookRaw(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseHookRaw(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SET_BAND_MODE 65
    virtual RIL_RESULT_CODE CoreSetBandMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSetBandMode(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE 66
    virtual RIL_RESULT_CODE CoreQueryAvailableBandMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseQueryAvailableBandMode(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_STK_GET_PROFILE 67
    virtual RIL_RESULT_CODE CoreStkGetProfile(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseStkGetProfile(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_STK_SET_PROFILE 68
    virtual RIL_RESULT_CODE CoreStkSetProfile(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseStkSetProfile(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND 69
    virtual RIL_RESULT_CODE CoreStkSendEnvelopeCommand(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseStkSendEnvelopeCommand(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE 70
    virtual RIL_RESULT_CODE CoreStkSendTerminalResponse(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseStkSendTerminalResponse(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM 71
    virtual RIL_RESULT_CODE CoreStkHandleCallSetupRequestedFromSim(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseStkHandleCallSetupRequestedFromSim(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
    virtual RIL_RESULT_CODE CoreSetPreferredNetworkType(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSetPreferredNetworkType(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
    virtual RIL_RESULT_CODE CoreGetPreferredNetworkType(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGetPreferredNetworkType(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_GET_NEIGHBORING_CELL_IDS 75
    virtual RIL_RESULT_CODE CoreGetNeighboringCellIDs(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGetNeighboringCellIDs(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SET_TTY_MODE 80
    virtual RIL_RESULT_CODE CoreSetTtyMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSetTtyMode(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_QUERY_TTY_MODE 81
    virtual RIL_RESULT_CODE CoreQueryTtyMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseQueryTtyMode(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SMS_MEMORY_STATUS 102
    virtual RIL_RESULT_CODE CoreReportSmsMemoryStatus(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseReportSmsMemoryStatus(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING 103
    virtual RIL_RESULT_CODE CoreReportStkServiceRunning(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseReportStkServiceRunning(RESPONSE_DATA & rRspData);

    // internal response handlers
    virtual RIL_RESULT_CODE ParseIpAddress(RESPONSE_DATA & rRspData);
    virtual RIL_RESULT_CODE ParseDns(RESPONSE_DATA & rRspData);
    virtual RIL_RESULT_CODE ParseQueryPIN2(RESPONSE_DATA & rRspData);

};

//  Call these functions to set up data and bring down data.
BOOL DataConfigUp(char *szNetworkInterfaceName, CChannel_Data *pChannelData, PDP_TYPE eDataConnectionType);
BOOL DataConfigUpIpV4(char *szNetworkInterfaceName, CChannel_Data *pChannelData);
BOOL DataConfigDown(int nCID);

#if defined(M2_IPV6_FEATURE_ENABLED)
//  Helper function to convert IP addresses to Android-readable format.
//  szIpIn [IN] - The IP string to be converted
//  szIpOut [OUT] - The converted IP address in Android-readable format.
//  uiIpOutSize [IN] - The size of the szIpOut buffer
//  szIpOut2 [OUT] - The converted IP address in Android-readable format if szIpIn is IPv4v6 format.
//  uiIpOutSize [IN] - The size of szIpOut2 buffer
//
//  If IPv4 format a1.a2.a3.a4, then szIpIn is copied to szIpOut.
//  If Ipv6 format:
//    Convert a1.a2.a3.a4.a5. ... .a15.a16 to XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX IPv6 format
//    output is copied to szIpOut
//  If Ipv4v6 format:
//    Convert a1.a2.a3.a4.a5. ... .a19.a20 to
//      a1.a2.a3.a4 to szIpOut
//      XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX (a5-a20) to szIpOut2
//  If szIpOut2 is NULL, then this parameter is ignored
BOOL ConvertIPAddressToAndroidReadable(char *szIpIn, char *szIpOut, UINT32 uiIpOutSize, char *szIpOut2, UINT32 uiIpOut2Size);

//  Call these functions to set up data specifically for IpV6 or IpV4V6 pdp context
BOOL DataConfigUpIpV6(char *szNetworkInterfaceName, CChannel_Data *pChannelData);
BOOL DataConfigUpIpV4V6(char *szNetworkInterfaceName, CChannel_Data *pChannelData);
#endif // M2_IPV6_FEATURE_ENABLED

#endif
