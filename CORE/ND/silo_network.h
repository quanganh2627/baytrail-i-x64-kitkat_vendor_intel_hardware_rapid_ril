////////////////////////////////////////////////////////////////////////////
// silo_network.h                       
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the CSilo_Network class, which provides response handlers and
//    parsing functions for the network-related RIL components.
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
//  Network silo class.  This class handles all network functionality including:
//  -Register on network
//  -Unregister from network
//  -Get operator list
//  -Get current operator
//  -Get own number (Get Subscriber Number)
//  -Get preferred operator list
//  -Add/Remove preferred operator
//

#ifndef RRIL_SILO_NETWORK_H
#define RRIL_SILO_NETWORK_H

#include "silo.h"

class CSilo_Network : public CSilo
{
public:
    CSilo_Network(CChannel *pChannel);
    virtual ~CSilo_Network();

    //  Called at the beginning of CChannel::SendRILCmdHandleRsp() before AT command is
    //  physically sent and before any CCommand checking.
    virtual BOOL PreSendCommandHook(CCommand*& rpCmd, CResponse*& rpRsp) { return TRUE; };

    //  Called in CChannel::SendCommand() after AT command is physically sent and
    //  a response has been received (or timed out).
    virtual BOOL PostSendCommandHook(CCommand*& rpCmd, CResponse*& rpRsp);

    //  Called in CChannel::ParseResponse() before CResponse::ParseResponse() is called.
    virtual BOOL PreParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp) { return TRUE; };

    //  Called in CChannel::ParseResponse() after CResponse::ParseResponse() is called, and before
    //  CCommand::SendResponse() is called.
    virtual BOOL PostParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp);

protected:
    //  Parse notification functions here.
    virtual BOOL    ParseRegistrationStatus(CResponse* const pResponse, const BYTE*& rszPointer, BOOL const bGPRS);
    virtual BOOL    ParseCTZV(CResponse* const pResponse, const BYTE*& rszPointer);
    //virtual BOOL    ParseCTZDST(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL    ParseCREG(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL    ParseCGREG(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL    ParseXREG(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL    ParseCGEV(CResponse* const pResponse, const BYTE*& rszPointer);
    virtual BOOL    ParseXCGEDPAGE(CResponse* const pResponse, const BYTE*& rszPointer);
};

#endif // RRIL_SILO_NETWORK_H
