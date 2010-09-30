////////////////////////////////////////////////////////////////////////////
// silo_data_inf.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides response handlers and parsing functions for the Data-related
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


#ifndef RRIL_SILO_DATA_INF_H
#define RRIL_SILO_DATA_INF_H


#include "silo.h"
#include "silo_data.h"

class CChannel;

class CSilo_Data_INF : public CSilo_Data
{
public:
    CSilo_Data_INF(CChannel *pChannel);
    virtual ~CSilo_Data_INF();
};

#endif // RRIL_SILO_DATA_INF_H

