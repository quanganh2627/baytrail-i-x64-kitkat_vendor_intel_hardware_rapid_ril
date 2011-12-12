////////////////////////////////////////////////////////////////////////////
// silo_sms_inf.cpp
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

#include "types.h"
#include "silo.h"
#include "silo_sms.h"
#include "silo_sms_inf.h"
#include "rillog.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

CSilo_SMS_INF::CSilo_SMS_INF(CChannel *pChannel)
: CSilo_SMS(pChannel)
{
    RIL_LOG_VERBOSE("CSilo_SMS_INF::CSilo_SMS_INF() - Enter\r\n");

    RIL_LOG_VERBOSE("CSilo_SMS_INF::CSilo_SMS_INF() - Exit\r\n");
}

//
//
CSilo_SMS_INF::~CSilo_SMS_INF()
{
    RIL_LOG_VERBOSE("CSilo_SMS_INF::~CSilo_SMS_INF() - Enter / Exit\r\n");
}

