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
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

//  List channels here (one per COM port)

typedef enum
{
    RIL_CHANNEL_ATCMD = 0,

#ifdef RIL_ENABLE_CHANNEL_SIM
    RIL_CHANNEL_SIM,
#endif // RIL_ENABLE_CHANNEL_SIM

#ifdef RIL_ENABLE_CHANNEL_DATA1
    RIL_CHANNEL_DATA1,
#endif // RIL_ENABLE_CHANNEL_DATA1

#ifdef RIL_ENABLE_CHANNEL_DATA2
    RIL_CHANNEL_DATA2,
#endif // RIL_ENABLE_CHANNEL_DATA2

#ifdef RIL_ENABLE_CHANNEL_DATA3
    RIL_CHANNEL_DATA3,
#endif // RIL_ENABLE_CHANNEL_DATA3

    RIL_CHANNEL_MAX,

} EnumRilChannel;

#ifndef RIL_ENABLE_CHANNEL_SIM
#define RIL_CHANNEL_SIM     RIL_CHANNEL_ATCMD
#endif

#ifndef RIL_ENABLE_CHANNEL_DATA1
#define RIL_CHANNEL_DATA1   RIL_CHANNEL_ATCMD
#endif

//  In atcmd.cpp.
BOOL IsChannelIndexValid(EnumRilChannel eChannel);
