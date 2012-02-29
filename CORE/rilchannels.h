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
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "request_id.h"

//  List channels here (one per COM port)

#define RIL_CHANNEL_MAX 10
extern int g_dRilChannelCurMax;

#ifdef BOARD_HAVE_IFX7060
#define RIL_HSI_CHANNEL_MAX 4
#endif

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

//  GPRS/UMTS data (1st primary context)
#define RIL_CHANNEL_DATA1 5

//  GPRS/UMTS data (2nd primary context)
#define RIL_CHANNEL_DATA2 6

//  GPRS/UMTS data (3rd primary context)
#define RIL_CHANNEL_DATA3 7

//  GPRS/UMTS data (4th primary context)
#define RIL_CHANNEL_DATA4 8

//  GPRS/UMTS data (5th primary context)
#define RIL_CHANNEL_DATA5 9

// TODO - currently allow up to one reserved channel (e.g., for direct audio use).
// Set this to -1 or a number greater than RIL_CHANNEL_MAX if there is no reserved channel
#define RIL_CHANNEL_RESERVED 555


//  Channel mapping array
extern UINT32* g_arChannelMapping;
extern UINT32 g_arChannelMapping2230[REQ_ID_TOTAL];
extern UINT32 g_arChannelMappingDefault[REQ_ID_TOTAL];

