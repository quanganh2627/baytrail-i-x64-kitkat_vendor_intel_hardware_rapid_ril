////////////////////////////////////////////////////////////////////////////
// silo_phonebook_inf.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides response handlers and parsing functions for the Phonebook-related
//    RIL components - Infineon family of modems.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "silo.h"
#include "silo_phonebook.h"
#include "silo_phonebook_inf.h"
#include "rillog.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

CSilo_Phonebook_INF::CSilo_Phonebook_INF(CChannel *pChannel)
: CSilo_Phonebook(pChannel)
{
    RIL_LOG_VERBOSE("CSilo_Phonebook_INF::CSilo_Phonebook_INF() - Enter\r\n");

    RIL_LOG_VERBOSE("CSilo_Phonebook_INF::CSilo_Phonebook_INF() - Exit\r\n");
}

//
//
CSilo_Phonebook_INF::~CSilo_Phonebook_INF()
{
    RIL_LOG_VERBOSE("CSilo_Phonebook_INF::~CSilo_Phonebook_INF() - Enter / Exit\r\n");
}

