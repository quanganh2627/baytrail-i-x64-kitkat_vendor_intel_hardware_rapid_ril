////////////////////////////////////////////////////////////////////////////
// request_info_table.h
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//      Stores and provides information regarding a particular request.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_REQUEST_INFO_TABLE_H
#define RRIL_REQUEST_INFO_TABLE_H
#include "types.h"
#include "rril.h"
#include "request_id.h"
#include "sync_ops.h"

struct REQ_INFO
{
    UINT32 uiTimeout;
};

class CRequestInfoTable
{
public:

    CRequestInfoTable();
    ~CRequestInfoTable();

    void GetRequestInfo(REQ_ID requestID, REQ_INFO& rReqInfo);

private:
    REQ_INFO* m_rgpRequestInfos[REQ_ID_TOTAL];
    CMutex* m_pCacheAccessMutex;
};

#endif // RRIL_REQUEST_INFO_TABLE_H
