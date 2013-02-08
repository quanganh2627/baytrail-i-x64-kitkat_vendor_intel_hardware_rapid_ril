////////////////////////////////////////////////////////////////////////////
// silo_sim_inf.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides response handlers and parsing functions for the Sim-related
//    RIL components - Infineon family of modems.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "silo.h"
#include "silo_sim.h"
#include "silo_sim_inf.h"
#include "rillog.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

CSilo_SIM_INF::CSilo_SIM_INF(CChannel* pChannel)
: CSilo_SIM(pChannel)
{
    RIL_LOG_VERBOSE("CSilo_SIM_INF::CSilo_SIM_INF() - Enter\r\n");

    RIL_LOG_VERBOSE("CSilo_SIM_INF::CSilo_SIM_INF() - Exit\r\n");
}

//
//
CSilo_SIM_INF::~CSilo_SIM_INF()
{
    RIL_LOG_VERBOSE("CSilo_SIM_INF::~CSilo_SIM_INF() - Enter / Exit\r\n");
}

