////////////////////////////////////////////////////////////////////////////
// silo_ims_inf.cpp
//
// Copyright 2013 Intel Corporation, All Rights Reserved.
//
//
// Description:
//    Provides response handlers and parsing functions for the IMS-related
//    RIL components - IMC family of modems.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "silo.h"
#include "silo_ims.h"
#include "silo_ims_inf.h"
#include "rillog.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

CSilo_IMS_INF::CSilo_IMS_INF(CChannel* pChannel)
: CSilo_IMS(pChannel)
{
    RIL_LOG_VERBOSE("CSilo_IMS_INF::CSilo_IMS_INF() - Enter\r\n");

    RIL_LOG_VERBOSE("CSilo_IMS_INF::CSilo_IMS_INF() - Exit\r\n");
}

//
//
CSilo_IMS_INF::~CSilo_IMS_INF()
{
    RIL_LOG_VERBOSE("CSilo_IMS_INF::~CSilo_IMS_INF() - Enter / Exit\r\n");
}

