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

#ifdef __linux__
#include "channel_nd.h"
#else
#include "rilndis.h"
#endif

class CChannel_Data : public CChannel
{
public:
    CChannel_Data(EnumRilChannel eChannel);
    virtual ~CChannel_Data();

#ifndef __linux__
    BOOL    OpenDownstreamPort();
#endif

    //  public port interface
    BOOL    OpenPort();

    // get / set functions

#ifndef __linux__
    const RILNDISGPRSCONTEXT* GetNdisContext() const { return &m_NdisContext; }
    BOOL SetNdisContext( const RILNDISGPRSCONTEXT* pContext );
#endif // __linux__

#ifdef __linux__
    UINT32 GetContextID() const { return m_uiContextID; }
#else  // __linux__
    UINT32 GetContextID() const { return m_NdisContext.dwContextID; }
#endif // __linux__

    BOOL SetContextID( UINT32 dwContextID );

    BOOL IsInDataMode() const { return m_bDataMode; };
    BOOL SetDataMode( BOOL bDataMode );                  // [in ] TRUE - data mode, FALSE - command mode 

    virtual BOOL SendDataInDataMode();

    //virtual BOOL HandleRxData(char *szData, UINT32 dwRead, void* pRxData);

    UINT32 ActivateContext();

#ifndef __linux__
    HRESULT ReturnRxNdisPacket(
        RILNDISPACKET* pPacket  // @parm 
    );

    //RILNDISGPRSCONTEXTRESPONSE::pfnNdisReceivePacketDone
    static void NdisReceivePacketDone(
        const LPRILNDISPACKET pPacket  // @parm
    );

    //RILNDISGPRSCONTEXTRESPONSE::pfnNdisSendPacket
    static void NdisSendPacket(
        const LPRILNDISPACKET pPacket  // @parm 
    );
#endif // __linux__

    //
    // helper functions to convert ContextID, Dlci and Channel
    //
    static CChannel_Data* GetChnlFromContextID(UINT32 dwContextID);
    static CChannel_Data* GetChnlFromEnumRilChannel(EnumRilChannel index);
    static CChannel_Data* GetFreeChnl();

    CEvent *              m_pContextPhaseDoneEvent;

private:
#ifndef __linux__
    CRilNDIS            m_RilNdis;
    RILNDISGPRSCONTEXT  m_NdisContext;
#else  // __linux__
    UINT32               m_uiContextID;
#endif // __linux__
    BOOL                m_bDataMode;

protected:
    BOOL    FinishInit();
    BOOL    AddSilos();

};

#endif  // RIL_CHANNEL_DATA_H
