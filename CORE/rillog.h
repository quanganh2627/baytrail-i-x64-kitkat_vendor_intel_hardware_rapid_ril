////////////////////////////////////////////////////////////////////////////
// rillog.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Declares RIL log class.
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

#ifndef RRIL_LOG_H
#define RRIL_LOG_H

#define LOG_TAG "RILR"

#include "types.h"

#if defined(DEBUG)
#define RIL_LOG_VERBOSE(format, ...)    CRilLog::Verbose(format, ## __VA_ARGS__)
#define RIL_LOG_INFO(format, ...)       CRilLog::Info(format, ## __VA_ARGS__)
#define RIL_LOG_WARNING(format, ...)    CRilLog::Warning(format, ## __VA_ARGS__)
#else
#define RIL_LOG_VERBOSE(format, ...)
#define RIL_LOG_INFO(format, ...)
#define RIL_LOG_WARNING(format, ...)
#endif  // DEBUG

// Critical is always enabled
#define RIL_LOG_CRITICAL(format, ...)   CRilLog::Critical(format, ## __VA_ARGS__)


class CRilLog
{
public:
    static void Init();
    static void Verbose(const char* const szFormatString, ...);
    static void Info(const char* const szFormatString, ...);
    static void Warning(const char* const szFormatString, ...);
    static void Critical(const char* const szFormatString, ...);

private:
    static const UINT32 m_uiMaxLogBufferSize = 1024;
    enum
    {
        E_RIL_VERBOSE_LOG  = 0x01,
        E_RIL_INFO_LOG     = 0x02,
        E_RIL_WARNING_LOG  = 0x04,
        E_RIL_CRITICAL_LOG = 0x08
    };

    static UINT8 m_uiFlags;
    static BOOL  m_bInitialized;
};

#endif // RRIL_LOG_H

