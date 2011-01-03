////////////////////////////////////////////////////////////////////////////
// silo_sms_swi.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides response handlers and parsing functions for the SMS-related
//    RIL components - Sierra Wireless family of modems.
//
// Author:  Francesc J. Vilarino Guell
// Created: 2009-12-15
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date         Who      Ver   Description
//  ----------  -------  ----  -----------------------------------------------
//  Dec 15/09    FV      1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////


#ifndef RRIL_SILO_SMS_SWI_H
#define RRIL_SILO_SMS_SWI_H


#include "silo.h"
#include "silo_sms.h"

class CChannel;

class CSilo_SMS_SW : public CSilo_SMS
{
public:
    CSilo_SMS_SW(CChannel *pChannel);
    virtual ~CSilo_SMS_SW();
};

#endif // RRIL_SILO_SMS_SWI_H

