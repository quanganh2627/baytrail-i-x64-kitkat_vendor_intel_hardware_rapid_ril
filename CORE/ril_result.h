////////////////////////////////////////////////////////////////////////////
// ril_result.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
// Enums, defines and functions to work with error types internally in RapidRIL
//
// Author:  Mike Worth
// Created: 2009-11-10
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Nov 10/09  MW       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RIL_RESULT_H
#define RIL_RESULT_H

#include "rril.h"


// CME error codes are mapped to 3GPP spec
#define CME_ERROR_OPERATION_NOT_SUPPORTED                   4
#define CME_ERROR_SIM_NOT_INSERTED                          10
#define CME_ERROR_SIM_PIN_REQUIRED                          11
#define CME_ERROR_SIM_PUK_REQUIRED                          12
#define CME_ERROR_INCORRECT_PASSWORD                        16
#define CME_ERROR_SIM_PIN2_REQUIRED                         17
#define CME_ERROR_SIM_PUK2_REQUIRED                         18
#define CME_ERROR_SIM_NOT_READY                             515
#define CME_ERROR_NETWORK_PUK_REQUIRED                      41

// CMS error codes are mapped to 3GPP spec
#define CMS_ERROR_SIM_NOT_INSERTED                          310


#endif
