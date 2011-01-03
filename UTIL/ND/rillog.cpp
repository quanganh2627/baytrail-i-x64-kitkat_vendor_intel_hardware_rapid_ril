////////////////////////////////////////////////////////////////////////////
// rillog.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Implements RIL log class.
//
// Author:  Chris Whitehead
// Created: 2009-02-05
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Feb 05/09  CW       1.00  Established v1.00 based on current code base.
//  Dec 17/09  FV       1.10  Refactor logging by using a class with
//                            static functions
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "repository.h"
#include "rillog.h"
#include <utils/Log.h>

UINT8 CRilLog::m_uiFlags = 0x00;
BOOL  CRilLog::m_bInitialized = FALSE;


void CRilLog::Init()
{
    CRepository repository;
    int         iLogLevel;

    if (repository.Read(g_szGroupLogging, g_szLogLevel, iLogLevel))
    {
        LOGI("Log level [%d]\r\n", iLogLevel);
        switch(iLogLevel)
        {
            case 1: // Verbose
                m_uiFlags |= E_RIL_VERBOSE_LOG;
                // fall through
            case 2: // Info
                m_uiFlags |= E_RIL_INFO_LOG;
                // fall through
            case 3:
                m_uiFlags |= E_RIL_WARNING_LOG;
                // fall through
            case 4:
            default:
                m_uiFlags |= E_RIL_CRITICAL_LOG;
                break;
        }
    }
    else
    {
        m_uiFlags = E_RIL_CRITICAL_LOG;
    }

    m_bInitialized = TRUE;
}

void CRilLog::Verbose(const BYTE* const szFormatString, ...)
{
    if (m_bInitialized && (m_uiFlags & E_RIL_VERBOSE_LOG))
    {
        va_list argList;
        BYTE szLogText[m_uiMaxLogBufferSize];

        va_start(argList, szFormatString);
        vsnprintf(szLogText, m_uiMaxLogBufferSize, szFormatString, argList);
        va_end(argList);

        LOGD("%s", szLogText);
    }
}

void CRilLog::Info(const BYTE* const szFormatString, ...)
{
    if (m_bInitialized && (m_uiFlags & E_RIL_INFO_LOG))
    {
        va_list argList;
        BYTE szLogText[m_uiMaxLogBufferSize];

        va_start(argList, szFormatString);
        vsnprintf(szLogText, m_uiMaxLogBufferSize, szFormatString, argList);
        va_end(argList);

        LOGI("%s", szLogText);
    }
}

void CRilLog::Warning(const BYTE* const szFormatString, ...)
{
    if (m_bInitialized && (m_uiFlags & E_RIL_WARNING_LOG))
    {
        va_list argList;
        BYTE szLogText[m_uiMaxLogBufferSize];

        va_start(argList, szFormatString);
        vsnprintf(szLogText, m_uiMaxLogBufferSize, szFormatString, argList);
        va_end(argList);

        LOGW("%s", szLogText);
    }
}

void CRilLog::Critical(const BYTE* const szFormatString, ...)
{
    if (m_bInitialized && (m_uiFlags & E_RIL_CRITICAL_LOG))
    {
        va_list argList;
        BYTE szLogText[m_uiMaxLogBufferSize];

        va_start(argList, szFormatString);
        vsnprintf(szLogText, m_uiMaxLogBufferSize, szFormatString, argList);
        va_end(argList);

        LOGE("%s", szLogText);
    }
}

