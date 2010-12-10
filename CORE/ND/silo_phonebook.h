////////////////////////////////////////////////////////////////////////////
// silo_Phonebook.h                       
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the CSilo_Phonebook class, which provides response handlers and
//    parsing functions for the phonebook-related RIL components.
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
//  Phonebook silo class.  This class handles phonebook functionality including:
//  -Read/Write/Delete phonebook entries
//  -Get/Set phonebook options
//

#ifndef RRIL_SILO_PHONEBOOK_H
#define RRIL_SILO_PHONEBOOK_H

#include "silo.h"

class CSilo_Phonebook : public CSilo
{
public:
    CSilo_Phonebook(CChannel *pChannel);
    virtual ~CSilo_Phonebook();

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
    virtual BOOL PostParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp) { return TRUE; };

protected:
    //  Parse notification functions here.
};

#endif // RRIL_SILO_PHONEBOOK_H

