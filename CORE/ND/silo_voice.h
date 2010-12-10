////////////////////////////////////////////////////////////////////////////
// silo_voice.h                       
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the CSilo_Voice class, which provides response handlers and
//    parsing functions for the voice-related RIL components.
//
// Author:  Dennis Peter
// Created: 2007-07-30
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date        Who      Ver   Description
//  ----------  -------  ----  -----------------------------------------------
//  June 03/08  DP       1.00  Established v1.00 based on current code base.
//  May  04/09  CW       1.01  Moved common code to base class, identified
//                             platform-specific implementations, implemented
//                             general code clean-up.
//
/////////////////////////////////////////////////////////////////////////////
//
//  Voice silo class.  This class handles the voice call and CSD call functionality including:
//  -Dial/Answer/Hangup voice call
//  -Dial/Answer/Hangup CSD call
//  -DTMF
//  -Call management (hold, transfer, conference)
//  -Supplementary services including:
//    -Call forwarding
//    -Call waiting
//    -Caller ID
//    -USSD
//    -Handle GSM 02.30 commands
//  -Voice call audio volume and muting
//  -Set audio device
//  -Get/Set equipment state
//  -Misc functions


#ifndef RRIL_SILO_VOICE_H
#define RRIL_SILO_VOICE_H


#include "silo.h"


class CSilo_Voice : public CSilo
{
public:
    CSilo_Voice(CChannel *pChannel);
    virtual ~CSilo_Voice();

    //  Called at the beginning of CChannel::SendRILCmdHandleRsp() before AT command is
    //  physically sent and before any CCommand checking.
    virtual BOOL PreSendCommandHook(CCommand*& rpCmd, CResponse*& rpRsp) { return TRUE; };

    //  Called in CChannel::SendRILCmdHandleRsp() after AT command is physically sent and
    //  a response has been received (or timed out).
    virtual BOOL PostSendCommandHook(CCommand*& rpCmd, CResponse*& rpRsp) { return TRUE; };

    //  Called in CChannel::HandleRsp() before CResponse::ParseOKData() is called.
    virtual BOOL PreParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp);

    //  Called in CChannel::HandleRsp() after CResponse::ParseOKData() is called, and before
    //  CCommand::SendResponse() is called.
    virtual BOOL PostParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp) { return TRUE; };

protected:
    //  Parse notification functions here.

    virtual BOOL    ParseExtRing(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL    ParseNoCarrier(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL    ParseConnect(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL    ParseCallWaitingInfo(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL    ParseUnsolicitedSSInfo(CResponse* const pResponse, const BYTE*& szPointer);
    virtual BOOL    ParseIntermediateSSInfo(CResponse* const pResponse, const BYTE*& szPointer);
    virtual BOOL    ParseCallMeter(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL    ParseCallProgressInformation(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL    ParseUSSDInfo(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL    ParseDISCONNECT(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL    ParseIndicatorEvent(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL    ParseXCALLSTAT(CResponse* const pResponse, const BYTE*& rszPointer);
};

#endif // RRIL_SILO_VOICE_H
