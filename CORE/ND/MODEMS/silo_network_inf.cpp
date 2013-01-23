////////////////////////////////////////////////////////////////////////////
// silo_network_inf.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides response handlers and parsing functions for the Network-related
//    RIL components - Infineon family of modems.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "silo.h"
#include "silo_network.h"
#include "silo_network_inf.h"
#include "rillog.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

CSilo_Network_INF::CSilo_Network_INF(CChannel* pChannel)
: CSilo_Network(pChannel)
{
    RIL_LOG_VERBOSE("CSilo_Network_INF::CSilo_Network_INF() - Enter\r\n");

    RIL_LOG_VERBOSE("CSilo_Network_INF::CSilo_Network_INF() - Exit\r\n");
}

//
//
CSilo_Network_INF::~CSilo_Network_INF()
{
    RIL_LOG_VERBOSE("CSilo_Network_INF::~CSilo_Network_INF() - Enter / Exit\r\n");
}

