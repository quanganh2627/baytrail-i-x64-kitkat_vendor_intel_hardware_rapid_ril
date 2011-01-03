////////////////////////////////////////////////////////////////////////////
// channel_ATCmd.h
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the CChannel_ATCmd class, which is used to
//    facilitate the use of multiple command channels.
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

#if !defined(RIL_CHANNEL_ATCMD_H)
#define RIL_CHANNEL_ATCMD_H

#include "channel_nd.h"


class CChannel_ATCmd : public CChannel
{
public:
    CChannel_ATCmd(UINT32 uiChannel);
    virtual ~CChannel_ATCmd();

protected:
    BOOL    FinishInit();
    BOOL    AddSilos();
};

#endif // RIL_CHANNEL_ATCMD_H
