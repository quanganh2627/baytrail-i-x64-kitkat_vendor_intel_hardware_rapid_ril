////////////////////////////////////////////////////////////////////////////
// channel_SIM.h
//
// Copyright 2005-2008 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the CChannel_Data1 class, which is used to
//    facilitate the use of multiple data channels.
//
// Author:  Dennis Peter
// Created: 2008-07-09
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Jul 9/08   DP       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(RIL_CHANNEL_SIM_H)
#define RIL_CHANNEL_SIM_H

#ifdef __linux__
#include "channel_nd.h"
#endif

class CChannel_SIM : public CChannel
{
public:
    CChannel_SIM(EnumRilChannel eChannel);
    virtual ~CChannel_SIM();

#ifndef __linux__
    BOOL    OpenDownstreamPort();
#endif

protected:
    BOOL    FinishInit();
    BOOL    AddSilos();
};

#endif  // RIL_CHANNEL_SIM_H

