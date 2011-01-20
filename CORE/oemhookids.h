////////////////////////////////////////////////////////////////////////////
// oemhookids.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    List of enums for specific OEM requests via API
//    RIL_REQUEST_OEM_HOOK_STRINGS API.
//
// Author:  Francesc Vilarino Guell
// Created: 2009-05-29
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date           Who    Ver   Description
//  -----------  -------  ----  -----------------------------------------------
//  March 29/09    FV     1.00  Initial implementation.
//
/////////////////////////////////////////////////////////////////////////////


#pragma once

#include "types.h"

/**
 * This enum details the additional requests (OEM) to pass to the RIL
 * via the RIL_REQUEST_OEM_HOOK_RAW API
 */

//  The first byte of the byte[] is the command.  The data follows.
const BYTE RIL_OEM_HOOK_RAW_API1 = 0xAA;
const BYTE RIL_OEM_HOOK_RAW_API2 = 0xBB;
const BYTE RIL_OEM_HOOK_RAW_API3 = 0xCC;



/***********************************************************************/


