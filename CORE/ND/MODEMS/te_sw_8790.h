////////////////////////////////////////////////////////////////////////////
// te_sw_8790.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Overlay for the Sierra Wireless 8790
//
// Author:  Mike Worth
// Created: 2009-09-30
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Sept 30/09  MW      1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_TE_SW_8790_H
#define RRIL_TE_SW_8790_H

#include "te_base.h"
#include "rril.h"

class CTE_SW_8790 : public CTEBase
{
public:

    CTE_SW_8790();
    virtual ~CTE_SW_8790();

public:
    // Modem overrides
    
    // RIL_REQUEST_OPERATOR 22
    virtual RIL_RESULT_CODE ParseOperator(RESPONSE_DATA& rRspData);
    
    // RIL_REQUEST_SETUP_DATA_CALL 27
    virtual RIL_RESULT_CODE CoreSetupDataCall(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSetupDataCall(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_DEACTIVATE_DATA_CALL 41
    virtual RIL_RESULT_CODE CoreDeactivateDataCall(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseDeactivateDataCall(RESPONSE_DATA & rRspData);
    
    // RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE 45
    virtual RIL_RESULT_CODE CoreQueryNetworkSelectionMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseQueryNetworkSelectionMode(RESPONSE_DATA & rRspData);
    
    // RIL_REQUEST_QUERY_AVAILABLE_NETWORKS 48
    virtual RIL_RESULT_CODE ParseQueryAvailableNetworks(RESPONSE_DATA & rRspData);
    
    // RIL_REQUEST_SET_BAND_MODE 65
    virtual RIL_RESULT_CODE CoreSetBandMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);

    // RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE 66
    virtual RIL_RESULT_CODE CoreQueryAvailableBandMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseQueryAvailableBandMode(RESPONSE_DATA & rRspData);

    // RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
    virtual RIL_RESULT_CODE CoreSetPreferredNetworkType(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    
    // RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
    virtual RIL_RESULT_CODE CoreGetPreferredNetworkType(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGetPreferredNetworkType(RESPONSE_DATA & rRspData);


};

#endif
