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
//    GPRS/UMTS data (1st primary context)
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

private:
    //  Prevent assignment: Declared but not implemented.
    CChannel_Data(const CChannel_Data& rhs);  // Copy Constructor
    CChannel_Data& operator=(const CChannel_Data& rhs);  //  Assignment operator

public:
    //  public port interface
    BOOL    OpenPort();

    // get / set functions

    UINT32 GetContextID() const;

    BOOL SetContextID( UINT32 dwContextID );


    //
    // helper functions to convert ContextID, Dlci and Channel
    //
    static CChannel_Data* GetChnlFromContextID(UINT32 dwContextID);
    static CChannel_Data* GetChnlFromRilChannelNumber(UINT32 index);

    //  This function returns the next free data channel.  Also, populates the
    //  context ID of returned channel.
    //  If error, then NULL is returned.
    static CChannel_Data* GetFreeChnl(UINT32& outCID);

    CEvent *            m_pSetupDoneEvent;


    //  Local storage of IP adress, DNS1, DNS2
    BYTE*               m_szIpAddr;
    BYTE*               m_szDNS1;
    BYTE*               m_szDNS2;

#if defined(M2_IPV6_FEATURE_ENABLED)
    //  For IPV4V6, there could be 2 IP addresses
    BYTE*               m_szIpAddr2;

    //  For IPV4V6, there could be 2 DNS addresses for primary and secondary.
    BYTE*               m_szDNS1_2;
    BYTE*               m_szDNS2_2;
#endif // M2_IPV6_FEATURE_ENABLED

private:

    UINT32              m_uiContextID;


protected:
    BOOL    FinishInit();
    BOOL    AddSilos();

};

#endif  // RIL_CHANNEL_DATA_H
