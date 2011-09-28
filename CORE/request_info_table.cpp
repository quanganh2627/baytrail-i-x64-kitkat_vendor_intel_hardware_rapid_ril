////////////////////////////////////////////////////////////////////////////
// request_info_table.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//      Stores and provides information regarding a particular request.
//
// Author:  Mike Worth
// Created: 2009-06-22
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  June 22/09  MW       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "rillog.h"
#include "globals.h"
#include "request_info_table.h"
#include "repository.h"


///////////////////////////////////////////////////////////////////////////////
CRequestInfoTable::CRequestInfoTable()
{
    memset(m_rgpRequestInfos, 0x00, REQ_ID_TOTAL * sizeof(REQ_INFO*));
}

///////////////////////////////////////////////////////////////////////////////
CRequestInfoTable::~CRequestInfoTable()
{
    for (int i = 0; i < REQ_ID_TOTAL; i++)
    {
        delete m_rgpRequestInfos[i];
        m_rgpRequestInfos[i] = NULL;
    }
}

///////////////////////////////////////////////////////////////////////////////
void CRequestInfoTable::GetRequestInfo(REQ_ID requestID, REQ_INFO &rReqInfo)
{
    RIL_LOG_VERBOSE("CRequestInfoTable::GetRequestInfo() - Enter\r\n");
    REQ_INFO * pNewReqInfo = NULL;

    // Set defaults if we can't find the request id
    rReqInfo.uiTimeout       = g_TimeoutAPIDefault;

    if (REQ_ID_NONE == requestID)
    {
        RIL_LOG_INFO("CRequestInfoTable::GetRequestInfo() - Request ID NONE given; assigning default values.\r\n");
        goto Error;
    }
    else if ((requestID >= REQ_ID_TOTAL) || (requestID < 0))
    {
        RIL_LOG_CRITICAL("CRequestInfoTable::GetRequestInfo() - ERROR: Invalid request ID [0x%X]\r\n", requestID);
        goto Error;
    }

    if (NULL == m_rgpRequestInfos[requestID])
    {
        CRepository repository;
        int         iTemp = 0;

        pNewReqInfo = new REQ_INFO;
        if (!pNewReqInfo)
        {
            RIL_LOG_CRITICAL("CRequestInfoTable::GetRequestInfo() - Unable to allocate memory for a new REQ_INFO.\r\n");
            goto Error;
        }

        memset(pNewReqInfo, 0, sizeof(REQ_INFO));

        // Set up non-pre-defined strings

        // Group Strings are constant, while Item Strings are Request-Dependent.
        memset(g_szItemRequestTimeouts, 0, sizeof(g_szItemRequestTimeouts));
        strncpy(g_szItemRequestTimeouts,   g_szRequestNames[requestID], MAX_REQUEST_ITEM_LENGTH-1);

        if (repository.Read(g_szGroupRequestTimeouts, g_szItemRequestTimeouts, iTemp))
        {
            pNewReqInfo->uiTimeout = (UINT32)iTemp;
        }
        else
        {
            pNewReqInfo->uiTimeout = g_TimeoutAPIDefault;
        }

        // Use WAIT_FOREVER timeout if given time was 0
        if (!pNewReqInfo->uiTimeout)
        {
            pNewReqInfo->uiTimeout = WAIT_FOREVER;
        }

        // Cache the data we just read
        m_rgpRequestInfos[requestID] = pNewReqInfo;
        pNewReqInfo = NULL;
    }

    rReqInfo = *m_rgpRequestInfos[requestID];

Error:
    delete pNewReqInfo;
    pNewReqInfo = NULL;

    RIL_LOG_INFO("CRequestInfoTable::GetRequestInfo() - RequestID 0x%X: Timeout [%d]\r\n",
                   requestID,
                   rReqInfo.uiTimeout);

    RIL_LOG_VERBOSE("CRequestInfoTable::GetRequestInfo() - Exit\r\n");
}
