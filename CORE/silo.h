////////////////////////////////////////////////////////////////////////////
// silo.h
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the base class from which the various RIL Silos are derived.
//
/////////////////////////////////////////////////////////////////////////////
//
//  Silo abstract class
//

#ifndef RRIL_SILO_H
#define RRIL_SILO_H

#include "command.h"

class CSilo;
class CResponse;
class CChannel;
class CRilHandle;

typedef BOOL (CSilo::*PFN_ATRSP_PARSE) (CResponse* const pResponse, const char*& rszPointer);

typedef struct atrsptable_tag
{
    const char*     szATResponse;
    PFN_ATRSP_PARSE pfnATParseRsp;
} ATRSPTABLE;


class CSilo
{
public:
    CSilo(CChannel *pChannel);
    virtual ~CSilo();

    //  Called at beginning of CResponse::ParseUnsolicitedResponse().
    //  [in] pResponse = Pointer to CResponse class
    //  [in/out] rszPointer = Pointer to string response buffer.
    //  [in/out] fGotoError = Set to TRUE if we wish to stop response chain and goto Error in CResponse::ParseUnsolicitedResponse().
    //  Return values:
    //  TRUE if response is handled by this hook, then handling still stop.
    //  FALSE if response is not handled by this hook, and handling will continue to other silos, then framework.
    virtual BOOL ParseUnsolicitedResponse(CResponse* const pResponse, const char*& rszPointer, BOOL& fGotoError, BOOL& fPendingSolicitedResponse);

    //  Called at the beginning of CChannel::SendCommand() before AT command is
    //  physically sent and before any CCommand checking.
    virtual BOOL PreSendCommandHook(CCommand*& rpCmd, CResponse*& rpRsp) = 0;

    //  Called in CChannel::SendCommand() after AT command is physically sent and
    //  a response has been received (or timed out).
    virtual BOOL PostSendCommandHook(CCommand*& rpCmd, CResponse*& rpRsp) = 0;

    //  Called in CChannel::ParseResponse() before CResponse::ParseResponse() is called.
    virtual BOOL PreParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp) = 0;

    //  Called in CChannel::ParseResponse() after CResponse::ParseResponse() is called, and before
    //  CCommand::SendResponse() is called.
    virtual BOOL PostParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp) = 0;

protected:
    CChannel *m_pChannel;

    ATRSPTABLE* m_pATRspTable;
    ATRSPTABLE* m_pATRspTableExt;

    // Stub function that is never called but used by ParseUnsolicitedResponse to mark
    // the end of the parse tables.
    BOOL ParseNULL(CResponse* const pResponse, const char*& rszPointer);

    //  General function to skip this response and flag as unrecognized.
    BOOL ParseUnrecognized(CResponse* const pResponse, const char*& rszPointer);

private:
    // helper functions
    // fPendingSolicitedResponse is used to indicate if there is pending unprocessed
    // solicited response ahead of the Unsolicited response

    PFN_ATRSP_PARSE FindParser(ATRSPTABLE* pRspTable, const char*& pszStr, BOOL& fPendingSolicitedResponse, UINT32& uiSizeOfATRes);
};

#endif // RRIL_SILO_H
