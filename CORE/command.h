////////////////////////////////////////////////////////////////////////////
// Command.h
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the CCommand class which stores details required to execute
//    and return the result of a specific RIL API
//
// Author:  Mike Worth
// Created: 2009-11-19
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Nov 19/09  MW       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_COMMAND_H
#define RRIL_COMMAND_H

#include "rril.h"
#include "types.h"
#include "rilchannels.h"
#include "systemmanager.h"
#include "rril_OEM.h"

class CContext;
class CTE;

typedef RIL_RESULT_CODE (CTE::*PFN_TE_PARSE) (RESPONSE_DATA & rRspData);


class CCommand
{
public:
    CCommand(   UINT32 uiChannel,
                RIL_Token token,
                UINT32 uiReqId,
                const BYTE* pszATCmd,
                PFN_TE_PARSE pParseFcn = NULL);

    CCommand(   UINT32 uiChannel,
                RIL_Token token,
                UINT32 uiReqId,
                const BYTE* pszATCmd1,
                const BYTE* pszATCmd2,
                PFN_TE_PARSE pParseFcn = NULL);

    CCommand(   UINT32 uiChannel,
                RIL_Token token,
                UINT32 uiReqId,
                REQUEST_DATA reqData,
                PFN_TE_PARSE pParseFcn = NULL);

    ~CCommand();

    UINT32              GetChannel()        { return m_uiChannel;   };
    RIL_Token           GetToken()          { return m_token;       };
    UINT32              GetRequestID()      { return m_uiReqId;     };
    BYTE*               GetATCmd1()         { return m_pszATCmd1;   };
    BYTE*               GetATCmd2()         { return m_pszATCmd2;   };
    PFN_TE_PARSE        GetParseFcn()       { return m_pParseFcn;   };
    UINT32              GetTimeout()        { return m_uiTimeout;   };
    UINT32              GetRetries()        { return m_uiRetries;   };
    CContext *          GetContext()        { return m_pContext;    };
    void*               GetContextData()    { return m_pContextData;};
    unsigned int        GetContextDataSize(){ return m_cbContextData;}; 

    BOOL                IsAlwaysParse()     { return m_fAlwaysParse; };
    BOOL                IsHighPriority()    { return m_fHighPriority; };

    void SetTimeout(UINT32 uiTimeout)       { m_uiTimeout = uiTimeout;  };
    void SetRetries(UINT32 uiRetries)       { m_uiRetries = uiRetries;  };
    void SetAlwaysParse()                   { m_fAlwaysParse = TRUE;    };
    void SetHighPriority()                  { m_fHighPriority = TRUE; };
    void SetContext(CContext*& pContext)    { m_pContext = pContext; pContext = NULL; };

    static BOOL AddCmdToQueue(CCommand *& pCmd);

private:

    UINT32              m_uiChannel;
    RIL_Token           m_token;
    UINT32              m_uiReqId;
    BYTE*               m_pszATCmd1;
    BYTE*               m_pszATCmd2;
    PFN_TE_PARSE        m_pParseFcn;
    UINT32              m_uiTimeout;
    UINT32              m_uiRetries;
    BOOL                m_fAlwaysParse;
    BOOL                m_fHighPriority;
    CContext *          m_pContext;
    void*               m_pContextData;
    unsigned int        m_cbContextData;
};

#endif
