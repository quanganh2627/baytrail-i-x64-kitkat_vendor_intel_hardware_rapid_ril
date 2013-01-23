////////////////////////////////////////////////////////////////////////////
// silo_sim_inf.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides response handlers and parsing functions for the SIM-related
//    RIL components - Infineon family of modems.
//
/////////////////////////////////////////////////////////////////////////////


#ifndef RRIL_SILO_SIM_INF_H
#define RRIL_SILO_SIM_INF_H


#include "silo.h"
#include "silo_sim.h"

class CChannel;

class CSilo_SIM_INF : public CSilo_SIM
{
public:
    CSilo_SIM_INF(CChannel* pChannel);
    virtual ~CSilo_SIM_INF();
};

#endif // RRIL_SILO_SIM_INF_H

