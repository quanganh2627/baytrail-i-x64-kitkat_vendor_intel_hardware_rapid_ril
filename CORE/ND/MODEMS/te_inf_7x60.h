////////////////////////////////////////////////////////////////////////////
// te_inf_7x60.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Overlay for the Infineon 7x60 modem
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_TE_INF_7x60_H
#define RRIL_TE_INF_7x60_H

#include "te_inf_6260.h"
#include "rril.h"
#include "channel_data.h"

class CEvent;

class CTE_INF_7x60 : public CTE_INF_6260
{
public:

    CTE_INF_7x60();
    virtual ~CTE_INF_7x60();

private:
    //  Prevent assignment: Declared but not implemented.
    CTE_INF_7x60(const CTE_INF_7x60& rhs);  // Copy Constructor
    CTE_INF_7x60& operator=(const CTE_INF_7x60& rhs);  //  Assignment operator

    int m_currentNetworkType;

public:
    // modem overrides

    // RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
    virtual RIL_RESULT_CODE CoreSetPreferredNetworkType(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);

    // RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
    virtual RIL_RESULT_CODE CoreGetPreferredNetworkType(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGetPreferredNetworkType(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_VOICE_RADIO_TECH 108
    virtual RIL_RESULT_CODE CoreVoiceRadioTech(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseVoiceRadioTech(RESPONSE_DATA& rRspData);

    virtual BOOL PdpContextActivate(REQUEST_DATA& rReqData, void* pData,
                                                        UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseEnterDataState(RESPONSE_DATA& rRspData);
    virtual BOOL SetupInterface(UINT32 uiCID);
};

#endif
