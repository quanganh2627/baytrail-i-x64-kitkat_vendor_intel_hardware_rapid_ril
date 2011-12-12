////////////////////////////////////////////////////////////////////////////
// silo_network_inf.h
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


#ifndef RRIL_SILO_NETWORK_INF_H
#define RRIL_SILO_NETWORK_INF_H


#include "silo.h"
#include "silo_network.h"

class CChannel;

class CSilo_Network_INF : public CSilo_Network
{
public:
    CSilo_Network_INF(CChannel *pChannel);
    virtual ~CSilo_Network_INF();
};

#endif // RRIL_SILO_NETWORK_INF_H

