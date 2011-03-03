////////////////////////////////////////////////////////////////////////////
// channel_DLC8.h
//
// Copyright 2005-2011 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the CChannel_DLC8 class, which is used to
//    facilitate the use of multiple AT channels.
//
// Author:  Dennis Peter
// Created: 2011-02-08
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Feb 8/11   DP       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(RIL_CHANNEL_DLC8_H)
#define RIL_CHANNEL_DLC8_H

#include "channel_nd.h"

class CChannel_DLC8 : public CChannel
{
public:
    CChannel_DLC8(UINT32 uiChannel);
    virtual ~CChannel_DLC8();

    //  public port interface
    BOOL    OpenPort();

protected:
    BOOL    FinishInit();
    BOOL    AddSilos();
};

#endif  // RIL_CHANNEL_DLC8_H

