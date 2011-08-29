////////////////////////////////////////////////////////////////////////////
// rilchannels.h
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides function prototypes for
//    channel-related helper functions.
//    Also enumerates all of the channels that
//    can be used when sending AT Commands.
//
// Author:  Dennis Peter
// Created: 2007-07-30
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  June 3/08  DP       1.00  Established v1.00 based on current code base.
//  Oct 27/10  GR       1.01  Re-worked to support variable number of data channels.
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "request_id.h"

//  List channels here (one per COM port)

#if defined(M2_FEATURE_ENABLED)
#define RIL_CHANNEL_MAX 11
#else // M2_FEATURE_ENABLED
//Note : Max channel doesn't change as RIL_CHANNEL_VT==RIL_CHANNEL_ATCMD
#define RIL_CHANNEL_MAX 6
#endif // M2_FEATURE_ENABLED

//  Call control commands, misc commands
#define RIL_CHANNEL_ATCMD 0

//  GPRS/UMTS management (GPRS attach/detach), network commands
#define RIL_CHANNEL_DLC2 1

//  Call settings, SMS, supplementary services
#define RIL_CHANNEL_DLC6 2

//  SIM related functions, SIM toolkit
#define RIL_CHANNEL_DLC8 3

//  URC channel
#define RIL_CHANNEL_URC 4

#if defined(M2_FEATURE_ENABLED)
//  GPRS/UMTS data (1st primary context)
#define RIL_CHANNEL_DATA1 6
#else // M2_FEATURE_ENABLED
// In order to avoid gap when M2 is not enabled
#define RIL_CHANNEL_DATA1 5
#endif // M2_FEATURE_ENABLED


//  GPRS/UMTS data (2nd primary context)
#define RIL_CHANNEL_DATA2 7

//  GPRS/UMTS data (3rd primary context)
#define RIL_CHANNEL_DATA3 8

//  GPRS/UMTS data (4th primary context)
#define RIL_CHANNEL_DATA4 9

//  GPRS/UMTS data (5th primary context)
#define RIL_CHANNEL_DATA5 10

#if defined(M2_FEATURE_ENABLED)
//Note : RIL_CHANNEL_VT must be the fifth as there are tests on RIL_CHANNEL to know if
//	they are > RIL_CHANNEL_DATA1. In this case they are considered as data channels
#define RIL_CHANNEL_VT 5
#else // M2_FEATURE_ENABLED
#define RIL_CHANNEL_VT RIL_CHANNEL_ATCMD
#endif // M2_FEATURE_ENABLED

// TODO - currently allow up to one reserved channel (e.g., for direct audio use).
// Set this to -1 or a number greater than RIL_CHANNEL_MAX if there is no reserved channel
#define RIL_CHANNEL_RESERVED 555


//  Channel mapping array
extern UINT32 g_arChannelMapping[REQ_ID_TOTAL];

