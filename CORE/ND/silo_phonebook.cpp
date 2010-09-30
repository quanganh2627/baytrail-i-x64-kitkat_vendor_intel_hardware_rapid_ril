////////////////////////////////////////////////////////////////////////////
// silo_Phonebook.cpp                       
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides response handlers and parsing functions for the phonebook-related
//    RIL components.
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

#include "types.h"
#include "rillog.h"
#include "channel_nd.h"
#include "silo_phonebook.h"


//
//
CSilo_Phonebook::CSilo_Phonebook(CChannel *pChannel)
: CSilo(pChannel)
{
    RIL_LOG_VERBOSE("CSilo_Phonebook::CSilo_Phonebook() - Enter\r\n");

    // AT Response Table
    static ATRSPTABLE pATRspTable[] =
    {
        { ""  , (PFN_ATRSP_PARSE)&CSilo_Phonebook::ParseNULL }
    };

    m_pATRspTable = pATRspTable;
    RIL_LOG_VERBOSE("CSilo_Phonebook::CSilo_Phonebook() - Exit\r\n");
}

//
//
CSilo_Phonebook::~CSilo_Phonebook()
{
    RIL_LOG_VERBOSE("CSilo_Phonebook::~CSilo_Phonebook() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_Phonebook::~CSilo_Phonebook() - Exit\r\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
//  Parse functions here
///////////////////////////////////////////////////////////////////////////////////////////////
