////////////////////////////////////////////////////////////////////////////
// silo_SMS.h
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the CSilo_SMS class, which provides response handlers and
//    parsing functions for the SMS-related RIL components.
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
//  SMS silo class.  This class handles SMS functionality including:
//  -Get/Set message service options
//  -Get/Set message configuration
//  -Get/Set cell broadcast message configuration
//  -Read/Write/Delete message
//  -Send SMS
//  -Send stored SMS
//  -Send SMS acknowledgement


#ifndef RRIL_SILO_SMS_H
#define RRIL_SILO_SMS_H


#include "silo.h"


class CSilo_SMS : public CSilo
{
public:
    CSilo_SMS(CChannel *pChannel);
    virtual ~CSilo_SMS();

    //  Called at the beginning of CChannel::SendCommand() before AT command is
    //  physically sent and before any CCommand checking.
    virtual BOOL PreSendCommandHook(CCommand*& rpCmd, CResponse*& rpRsp) { return TRUE; };

    //  Called in CChannel::SendCommand() after AT command is physically sent and
    //  a response has been received (or timed out).
    virtual BOOL PostSendCommandHook(CCommand*& rpCmd, CResponse*& rpRsp) { return TRUE; };

    //  Called in CChannel::ParseResponse() before CResponse::ParseResponse() is called.
    virtual BOOL PreParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp);


    //  Called in CChannel::ParseResponse() after CResponse::ParseResponse() is called, and before
    //  CCommand::SendResponse() is called.
    virtual BOOL PostParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp) { return TRUE; };

protected:
    enum SILO_SMS_MSG_TYPES
        {
        SILO_SMS_MSG_IN_SIM,
        SILO_SMS_CELLBC_MSG_IN_SIM,
        SILO_SMS_STATUS_MSG_IN_SIM
        };

protected:
    //  Parse notification functions here.
    virtual BOOL ParseMessage(CResponse* const pResponse, const BYTE*& rszPointer, SILO_SMS_MSG_TYPES msgType);
    virtual BOOL ParseMessageInSim(CResponse* const pResponse, const BYTE*& rszPointer, SILO_SMS_MSG_TYPES msgType);
    virtual BOOL ParseCMT(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL ParseCBM(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL ParseCDS(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL ParseCMTI(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL ParseCBMI(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL ParseCDSI(CResponse* const pResponse, const BYTE*& rszPointer);

};

#endif // RRIL_SILO_SMS_H
