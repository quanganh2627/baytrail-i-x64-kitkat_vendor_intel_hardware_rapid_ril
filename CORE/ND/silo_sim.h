////////////////////////////////////////////////////////////////////////////
// silo_SIM.h
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the CSilo_SIM class, which provides response handlers and
//    parsing functions for the SIM-related RIL components.
//
// Author:  Dennis Peter
// Created: 2007-08-01
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
//  SIM silo class.  This class handles SIM commands and SIM Toolkit functionality including:
//  -Send SIM commands and send restricted SIM commands
//  -Get SIM records and SIM status
//  -Lock/Unlock/PIN/PUK password SIM functions
//  -Fixed dialing/PIN2/PUK2 SIM functions
//  -Call barring
//  -Get user identity
//  -Handle SIM Toolkit functions
//

#ifndef RRIL_SILO_SIM_H
#define RRIL_SILO_SIM_H


#include "silo.h"


class CSilo_SIM : public CSilo
{
public:
    CSilo_SIM(CChannel *pChannel);
    virtual ~CSilo_SIM();

    //  Called at the beginning of CChannel::SendRILCmdHandleRsp() before AT command is
    //  physically sent and before any CCommand checking.
    virtual BOOL PreSendCommandHook(CCommand*& rpCmd, CResponse*& rpRsp /*, BOOL& rfHungUp, BOOL& rfTimedOut*/) { return TRUE; };

    //  Called in CChannel::SendRILCmdHandleRsp() after AT command is physically sent and
    //  a response has been received (or timed out).
    virtual BOOL PostSendCommandHook(CCommand*& rpCmd, CResponse*& rpRsp /*, BOOL& rfHungUp, BOOL& rfTimedOut*/) { return TRUE; };

    //  Called in CChannel::HandleRsp() before CResponse::ParseOKData() is called.
    virtual BOOL PreParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp /*, BOOL& rfHungUp, BOOL& rfRadioOff*/);

    //  Called in CChannel::HandleRsp() after CResponse::ParseOKData() is called, and before
    //  CCommand::SendResponse() is called.
    virtual BOOL PostParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp /*, BOOL& rfHungUp, BOOL& rfRadioOff*/);

protected:
    //  Parse notification functions here.
#ifndef USE_STK_RAW_MODE
    virtual BOOL    ParseSTKProCmd(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL    ParseProSessionStatus(CResponse* const pResponse, const BYTE*& rszPointer);
#else
    virtual BOOL    ParseIndicationSATI(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL    ParseIndicationSATN(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL    ParseTermRespConfirm(CResponse* const pResponse, const BYTE*& rszPointer);
#endif
    virtual BOOL    ParseXSIM(CResponse* const pResponse, const BYTE*& rszPointer);

    //  For XSIM state (initialized to -1)
    int m_nXSIMStatePrev;

private:
    // helper functions
    BOOL ParsePin(CCommand*& rpCmd, CResponse*& rpRsp);
    BOOL ParseSimIO(CCommand*& rpCmd, CResponse*& rpRsp);
    BOOL ParseSimStatus(CCommand*& rpCmd, CResponse*& rpRsp);
};

#endif // RRIL_SILO_SIM_H
