////////////////////////////////////////////////////////////////////////////
// ril_result_nd.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
// Enums, defines and functions to work with error types internally in RapidRIL
// with static function implementation for ND
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

#ifndef RIL_RESULT_ND_H
#define RIL_RESULT_ND_H

#include "rril.h"
#include "ril_result.h"

class CRilResult : public CRilResultBase
{
public:

    // Called after getting response from OEM. Converts into framework style result.
    BOOL SetResult(const UINT32 uiRspCode, const void * pRspData, const UINT32 cbRspData);

    // Called before sending response to OS. Converts into OS specific response.
    BOOL GetOSResponse(UINT32 & ruiRspCode, void *& rpRspData, UINT32 & rcbRspData);
};

#endif
