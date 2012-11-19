////////////////////////////////////////////////////////////////////////////
// te_xmm7x60.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Overlay for the IMC 7x60 modem
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_TE_XMM7x60_H
#define RRIL_TE_XMM7x60_H

#include "te_xmm6360.h"
#include "rril.h"
#include "channel_data.h"

class CEvent;

class CTE_XMM7x60 : public CTE_XMM6360
{
public:

    CTE_XMM7x60(CTE& cte);
    virtual ~CTE_XMM7x60();

private:

    CTE_XMM7x60();

    //  Prevent assignment: Declared but not implemented.
    CTE_XMM7x60(const CTE_XMM7x60& rhs);  // Copy Constructor
    CTE_XMM7x60& operator=(const CTE_XMM7x60& rhs);  //  Assignment operator

    int m_currentNetworkType;

public:
    // modem overrides

    // RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
    virtual RIL_RESULT_CODE CoreSetPreferredNetworkType(REQUEST_DATA& rReqData,
            void* pData, UINT32 uiDataSize);

    // RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
    virtual RIL_RESULT_CODE CoreGetPreferredNetworkType(REQUEST_DATA& rReqData,
            void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGetPreferredNetworkType(RESPONSE_DATA& rRspData);

};

#endif
