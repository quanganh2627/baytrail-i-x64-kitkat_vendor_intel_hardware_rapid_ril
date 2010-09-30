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

/**
 * This enum details the additional requests (OEM) to pass to the RIL
 * via the RIL_REQUEST_OEM_HOOK_STRINGS API
 */

typedef enum {
    RIL_OEM_STRING_GET_VERSION = 0,	/* Returns 4 strings: RIL version, modem vendor address, hardware support, released to */
    RIL_OEM_STRING_GET_RXGAIN,      /* Returns 1 string: Current gain value in range (0-100) */
    RIL_OEM_STRING_SET_RXGAIN       /* Takes 1 string: Gain value in range (0-100) */
} RIL_OEM_STRING_REQUESTS;



/***********************************************************************/


