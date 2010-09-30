////////////////////////////////////////////////////////////////////////////
// silo_data_swi.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides response handlers and parsing functions for the Data-related
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

#include "types.h"
#include "silo.h"
#include "silo_data.h"
#include "silo_data_swi.h"
#include "rillog.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

CSilo_Data_SW::CSilo_Data_SW(CChannel *pChannel)
: CSilo_Data(pChannel)
{
    RIL_LOG_VERBOSE("CSilo_Data_SW::CSilo_Data_SW() - Enter\r\n");

    RIL_LOG_VERBOSE("CSilo_Data_SW::CSilo_Data_SW() - Exit\r\n");
}

//
//
CSilo_Data_SW::~CSilo_Data_SW()
{
    RIL_LOG_VERBOSE("CSilo_Data_SW::~CSilo_Data_SW() - Enter / Exit\r\n");
}

