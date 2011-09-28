////////////////////////////////////////////////////////////////////////////
// channel_nd.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines channel-related classes, constants, and structures.
//
// Author:  Francesc Vilarino
// Created: 2009-06-18
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  June 18/06  FV       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////


#ifndef RRIL_CHANNEL_ND_H
#define RRIL_CHANNEL_ND_H

#include "channelbase.h"

class CCommand;
class CResponse;

class CChannel : public CChannelBase
{
public:
    CChannel(UINT32 uiChannel);
    virtual ~CChannel();

private:
    //  Prevent assignment: Declared but not implemented.
    CChannel(const CChannel& rhs);  // Copy Constructor
    CChannel& operator=(const CChannel& rhs);  //  Assignment operator

public:
    //  Init functions
    virtual UINT32  CommandThread()  { return CChannelBase::CommandThread(); }
    virtual UINT32  ResponseThread() { return CChannelBase::ResponseThread(); }

protected:
    //  Init functions
    virtual BOOL    FinishInit() = 0;

    //  Silo-related functions
    virtual BOOL    AddSilos() = 0;

    //  Framework functions
    virtual BOOL            SendCommand(CCommand*& rpCmd);
    virtual RIL_RESULT_CODE GetResponse(CCommand*& rpCmd, CResponse*& rpRsp);
    virtual BOOL            ParseResponse(CCommand*& rpCmd, CResponse*& rpRsp/*, BOOL& rfHungUp, BOOL& rfRadioOff*/);

    // Called at end of ResponseThread()
    // Give GPRS response thread a chance to handle Rx data in Data mode
    virtual BOOL    ProcessModemData(BYTE *szData, UINT32 uiRead);

    //  Handle the timeout scenario (ABORT command, PING)
    virtual BOOL    HandleTimeout(CCommand*& rpCmd, CResponse*& rpRsp);

private:
    // Helper functions
    RIL_RESULT_CODE ReadQueue(CResponse*& rpRsp, UINT32 uiTimeout);
    BOOL            ProcessResponse(CResponse*& rpRsp);
    BOOL            ProcessNoop(CResponse*& rpRsp);
    BOOL            RejectRadioOff(CResponse*& rpRsp);

    //  Go through Tx queue and find identical request IDs.  Send response to IDs that match.
    //  Maybe make this a global function?
    BOOL FindIdenticalRequestsAndSendResponses(UINT32 uiReqID, RIL_Errno eErrNo, void *pResponse, size_t responseLen);

protected:
    CResponse*      m_pResponse;
};


#endif // RRIL_CHANNEL_ND_H

