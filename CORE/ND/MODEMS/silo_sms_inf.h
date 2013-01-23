////////////////////////////////////////////////////////////////////////////
// silo_sms_inf.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides response handlers and parsing functions for the SMS-related
//    RIL components - Infineon family of modems.
//
/////////////////////////////////////////////////////////////////////////////


#ifndef RRIL_SILO_SMS_INF_H
#define RRIL_SILO_SMS_INF_H


#include "silo.h"
#include "silo_sms.h"

class CChannel;

class CSilo_SMS_INF : public CSilo_SMS
{
public:
    CSilo_SMS_INF(CChannel* pChannel);
    virtual ~CSilo_SMS_INF();
};

#endif // RRIL_SILO_SMS_INF_H

