////////////////////////////////////////////////////////////////////////////
// channel_Data.h
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the CChannel_Data class, which is used to
//    facilitate the use of multiple data channels.
//
// Author:  Dennis Peter
// Created: 2007-09-20
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Jun 3/08   DP       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(RIL_CHANNEL_DATA_H)
#define RIL_CHANNEL_DATA_H

#include "channel_nd.h"

class CChannel_Data : public CChannel
{
public:
    CChannel_Data(UINT32 uiChannel);
    virtual ~CChannel_Data();


    //  public port interface
    BOOL    OpenPort();

    // get / set functions

    UINT32 GetContextID() const { return m_uiContextID; }

    BOOL SetContextID( UINT32 dwContextID );

    BOOL IsInDataMode() const { return m_bDataMode; };
    BOOL SetDataMode( BOOL bDataMode );                  // [in ] TRUE - data mode, FALSE - command mode

    virtual BOOL SendDataInDataMode();

    //virtual BOOL HandleRxData(char *szData, UINT32 dwRead, void* pRxData);

    //UINT32 ActivateContext();

    //
    // helper functions to convert ContextID, Dlci and Channel
    //
    static CChannel_Data* GetChnlFromContextID(UINT32 dwContextID);
    static CChannel_Data* GetChnlFromRilChannelNumber(UINT32 index);
    static CChannel_Data* GetFreeChnl();
    static UINT32 GetNextContextID();

    CEvent *            m_pSetupDoneEvent;

    //  Local storage of IP adress, DNS1, DNS2
    BYTE*               m_szIpAddr;
    BYTE*               m_szDNS1;
    BYTE*               m_szDNS2;

private:

    UINT32              m_uiContextID;
    BOOL                m_bDataMode;

protected:
    BOOL    FinishInit();
    BOOL    AddSilos();

};

#endif  // RIL_CHANNEL_DATA_H
