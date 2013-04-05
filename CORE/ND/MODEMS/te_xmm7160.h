////////////////////////////////////////////////////////////////////////////
// te_xmm7160.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Overlay for the IMC 7160 modem
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_TE_XMM7160_H
#define RRIL_TE_XMM7160_H

#include "te_xmm6360.h"
#include "rril.h"
#include "channel_data.h"

class CEvent;

class CTE_XMM7160 : public CTE_XMM6360
{
public:

    CTE_XMM7160(CTE& cte);
    virtual ~CTE_XMM7160();

private:

    CTE_XMM7160();

    //  Prevent assignment: Declared but not implemented.
    CTE_XMM7160(const CTE_XMM7160& rhs);  // Copy Constructor
    CTE_XMM7160& operator=(const CTE_XMM7160& rhs);  //  Assignment operator

    int m_currentNetworkType;

public:
    // modem overrides
    virtual char* GetBasicInitCommands(UINT32 uiChannelType);

    // RIL_REQUEST_SETUP_DATA_CALL 27
    virtual RIL_RESULT_CODE CoreSetupDataCall(REQUEST_DATA& rReqData,
                                                         void* pData,
                                                         UINT32 uiDataSize,
                                                         UINT32 uiCID);
    virtual RIL_RESULT_CODE ParseSetupDataCall(RESPONSE_DATA& rRspData);
    virtual void PostSetupDataCallCmdHandler(POST_CMD_HANDLER_DATA& rData);

    virtual void HandleSetupDataCallSuccess(UINT32 uiCID, void* pRilToken);

    // RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
    virtual RIL_RESULT_CODE CoreSetPreferredNetworkType(REQUEST_DATA& rReqData,
            void* pData, UINT32 uiDataSize);

    // RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
    virtual RIL_RESULT_CODE CoreGetPreferredNetworkType(REQUEST_DATA& rReqData,
            void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGetPreferredNetworkType(RESPONSE_DATA& rRspData);

};

#endif
