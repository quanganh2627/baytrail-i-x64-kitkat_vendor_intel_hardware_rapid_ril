////////////////////////////////////////////////////////////////////////////
// silo_phonebook_inf.h
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


#ifndef RRIL_SILO_PHONEBOOK_INF_H
#define RRIL_SILO_PHONEBOOK_INF_H


#include "silo.h"
#include "silo_phonebook.h"

class CChannel;

class CSilo_Phonebook_INF : public CSilo_Phonebook
{
public:
    CSilo_Phonebook_INF(CChannel* pChannel);
    virtual ~CSilo_Phonebook_INF();
};

#endif // RRIL_SILO_PHONEBOOK_INF_H

