////////////////////////////////////////////////////////////////////////////
// silo_voice_inf.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides response handlers and parsing functions for the voice-related
//    RIL components - Infineon family of modems.
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

#include "types.h"
#include "silo.h"
#include "silo_voice.h"
#include "silo_voice_inf.h"
#include "rillog.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

CSilo_Voice_INF::CSilo_Voice_INF(CChannel *pChannel)
: CSilo_Voice(pChannel)
{
    RIL_LOG_VERBOSE("CSilo_Voice_INF::CSilo_Voice_INF() - Enter\r\n");

    RIL_LOG_VERBOSE("CSilo_Voice_INF::CSilo_Voice_INF() - Exit\r\n");
}

//
//
CSilo_Voice_INF::~CSilo_Voice_INF()
{
    RIL_LOG_VERBOSE("CSilo_Voice_INF::~CSilo_Voice_INF() - Enter / Exit\r\n");
}

