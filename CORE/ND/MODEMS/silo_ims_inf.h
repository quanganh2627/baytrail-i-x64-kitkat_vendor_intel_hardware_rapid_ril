////////////////////////////////////////////////////////////////////////////
// silo_ims_inf.h
//
// Copyright 2013 Intel Corporation, All Rights Reserved.
//
//
// Description:
//    Provides response handlers and parsing functions for the IMS-related
//    RIL components - IMC family of modems.
//
/////////////////////////////////////////////////////////////////////////////


#ifndef RRIL_SILO_IMS_INF_H
#define RRIL_SILO_IMS_INF_H


#include "silo.h"
#include "silo_ims.h"

class CChannel;

class CSilo_IMS_INF : public CSilo_IMS
{
public:
    CSilo_IMS_INF(CChannel* pChannel);
    virtual ~CSilo_IMS_INF();
};

#endif // RRIL_SILO_IMS_INF_H

