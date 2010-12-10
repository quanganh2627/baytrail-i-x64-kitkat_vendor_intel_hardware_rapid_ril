////////////////////////////////////////////////////////////////////////////
// silo_data.h                       
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the CSilo_Data class, which provides response handlers and
//    parsing functions for the data-related RIL components.
//
// Author:  Dennis Peter
// Created: 2007-08-03
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
//  Data silo class.  This class handles all data functionality including:
//  -Get/Set Bearer service options
//  -Get/Set High-speed CSD options and call settings
//  -Get/Set Radio-link protocol options
//  -Get/Set Data compression settings
//  -Get/Set Error correction settings
//  -GPRS connection including:
//    -Get/Set/Delete GPRS context
//    -Get/Set/Delete requested and minimum quality-of-service
//    -Get/Set GPRS class
//    -Get GPRS registration status
//    -Get/Set GPRS context activated list


#ifndef RRIL_SILO_DATA_H
#define RRIL_SILO_DATA_H


#include "silo.h"


class CSilo_Data : public CSilo
{
public:
    CSilo_Data(CChannel *pChannel);
    virtual ~CSilo_Data();

    //  Called at the beginning of CChannel::SendRILCmdHandleRsp() before AT command is
    //  physically sent and before any CCommand checking.
    virtual BOOL PreSendCommandHook(CCommand*& rpCmd, CResponse*& rpRsp) { return TRUE; };

    //  Called in CChannel::SendRILCmdHandleRsp() after AT command is physically sent and
    //  a response has been received (or timed out).
    virtual BOOL PostSendCommandHook(CCommand*& rpCmd, CResponse*& rpRsp) { return TRUE; };

    //  Called in CChannel::HandleRsp() before CResponse::ParseOKData() is called.
    virtual BOOL PreParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp) { return TRUE; };

    //  Called in CChannel::HandleRsp() after CResponse::ParseOKData() is called, and before
    //  CCommand::SendResponse() is called.
    virtual BOOL PostParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp);

protected:
    //  Parse notification functions here.
    //virtual BOOL    ParseTriplePlus(CResponse* const pResponse, const char*& rszPointer);
    virtual BOOL    ParseConnect(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL    ParseNoCarrier(CResponse* const pResponse, const BYTE*& rszPointer);
};

#endif // RRIL_SILO_DATA_H
