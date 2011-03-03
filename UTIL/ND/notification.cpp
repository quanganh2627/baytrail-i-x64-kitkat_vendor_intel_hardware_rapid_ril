////////////////////////////////////////////////////////////////////////////
// Notification.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides implementations for the RIL Utility functions.
//    Also includes related class implementations.
//
// Author:  Dennis Peter
// Created: 2007-07-12
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  June 3/08  DP       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////


#include "types.h"
#include "response.h"
#include "util.h"
#include "extract.h"
#include "notification.h"

//
//
//
CNotificationData::CNotificationData()
: m_dwCode(0),
m_pBlob(NULL),
m_cbBlob(0),
m_fInited(FALSE),
m_fDelayedInitFromRsp(FALSE),
m_fCpyMem(TRUE)
{
}


//
//
//
CNotificationData::~CNotificationData()
{
    if (m_fCpyMem)
    {
        free(m_pBlob);
    }

    m_pBlob = NULL;
    m_cbBlob = 0;
    m_dwCode = 0;
    m_fInited = FALSE;
    m_fDelayedInitFromRsp = FALSE;
    m_fCpyMem = TRUE;
}


//
//
//
BOOL CNotificationData::InitFromDWORDBlob(const UINT32 dwCode, const UINT32 dwBlob)
{
    BOOL fRet = FALSE;

    if (NULL != m_pBlob)
    {
        RIL_LOG_WARNING("CNotificationData::InitFromDWORDBlob() : WARN : m_pBlob was not NULL\r\n");
    }

    if (0 != m_cbBlob)
    {
        RIL_LOG_WARNING("CNotificationData::InitFromDWORDBlob() : WARN : m_cbBlob was not 0\r\n");
    }

    if (FALSE != !!m_fInited)
    {
        RIL_LOG_CRITICAL("CNotificationData::InitFromDWORDBlob() : ERROR : Notification Data was already inited\r\n");
        goto Error;
    }

    if (FALSE != !!m_fDelayedInitFromRsp)
    {
        RIL_LOG_CRITICAL("CNotificationData::InitFromDWORDBlob() : ERROR : m_fDelayedInitFromRsp was set to TRUE\r\n");
        goto Error;
    }

    m_dwCode = dwCode;
    m_cbBlob = sizeof(UINT32);
    m_pBlob = (void*)malloc(m_cbBlob);
    if (!m_pBlob)
    {
        goto Error;
    }
    memcpy(m_pBlob, &dwBlob, m_cbBlob);
    m_fInited = TRUE;
    fRet = TRUE;

    Error:
    return fRet;
}


//
//
//
BOOL CNotificationData::InitFromRealBlob(const UINT32 dwCode, void* pBlob, UINT32 cbBlob, const BOOL fCpyMem)
{
    BOOL fRet = FALSE;

    if (NULL != m_pBlob)
    {
        RIL_LOG_WARNING("CNotificationData::InitFromRealBlob() : WARN : m_pBlob was not NULL\r\n");
    }

    if (0 != m_cbBlob)
    {
        RIL_LOG_WARNING("CNotificationData::InitFromRealBlob() : WARN : m_cbBlob was not 0\r\n");
    }

    if (FALSE != !!m_fInited)
    {
        RIL_LOG_CRITICAL("CNotificationData::InitFromRealBlob() : ERROR : Notification Data was already inited\r\n");
        goto Error;
    }

    if (FALSE != !!m_fDelayedInitFromRsp)
    {
        RIL_LOG_CRITICAL("CNotificationData::InitFromRealBlob() : ERROR : m_fDelayedInitFromRsp was set to TRUE\r\n");
        goto Error;
    }

    m_dwCode = dwCode;
    if (cbBlob)
    {
        m_cbBlob = cbBlob;

        if (fCpyMem)
        {
            m_pBlob = malloc(m_cbBlob);
            if (!m_pBlob)
            {
                goto Error;
            }
            memcpy(m_pBlob, pBlob, m_cbBlob);
        }
        else
        {
            m_fCpyMem = FALSE;
            m_pBlob = pBlob;
        }
    }
    m_fInited = TRUE;
    fRet = TRUE;

    Error:
    return fRet;
}


//
//
//
BOOL CNotificationData::FinishInitFromRspBlob(const CResponse& rRsp)
{
    void* pBlob;
    UINT32 cbBlob;
    BOOL fRet = FALSE;

    if (NULL != m_pBlob)
    {
        RIL_LOG_WARNING("CNotificationData::FinishInitFromRspBlob() : WARN : m_pBlob was not NULL\r\n");
    }

    if (0 != m_cbBlob)
    {
        RIL_LOG_WARNING("CNotificationData::FinishInitFromRspBlob() : WARN : m_cbBlob was not 0\r\n");
    }

    if (FALSE != !!m_fInited)
    {
        RIL_LOG_CRITICAL("CNotificationData::FinishInitFromRspBlob() : ERROR : Notification Data was already inited\r\n");
        goto Error;
    }

    if (FALSE == !!m_fDelayedInitFromRsp)
    {
        RIL_LOG_CRITICAL("CNotificationData::FinishInitFromRspBlob() : ERROR : m_fDelayedInitFromRsp was set to TRUE\r\n");
        goto Error;
    }

    rRsp.GetData(pBlob, cbBlob);

    if (cbBlob)
    {
        m_cbBlob = cbBlob;
        m_pBlob = malloc(m_cbBlob);
        if (!m_pBlob)
        {
            goto Error;
        }
        memcpy(m_pBlob, pBlob, m_cbBlob);
    }

    // Turn off delayed init flag
    m_fDelayedInitFromRsp = FALSE;
    m_fInited = TRUE;
    fRet = TRUE;

    Error:
    return fRet;
}


//
//
//
BOOL CNotificationData::DelayInitFromRspBlob(const UINT32 dwCode)
{
    BOOL fRet = FALSE;

    if (NULL != m_pBlob)
    {
        RIL_LOG_WARNING("CNotificationData::DelayInitFromRspBlob() : WARN : m_pBlob was not NULL\r\n");
    }

    if (0 != m_cbBlob)
    {
        RIL_LOG_WARNING("CNotificationData::DelayInitFromRspBlob() : WARN : m_cbBlob was not 0\r\n");
    }

    if (FALSE != !!m_fInited)
    {
        RIL_LOG_CRITICAL("CNotificationData::DelayInitFromRspBlob() : ERROR : Notification Data was already inited\r\n");
        goto Error;
    }

    if (FALSE != !!m_fDelayedInitFromRsp)
    {
        RIL_LOG_CRITICAL("CNotificationData::DelayInitFromRspBlob() : ERROR : m_fDelayedInitFromRsp was set to TRUE\r\n");
        goto Error;
    }

    m_dwCode = dwCode;
    m_fDelayedInitFromRsp = TRUE;

Error:
    return fRet;
}
