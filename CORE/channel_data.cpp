////////////////////////////////////////////////////////////////////////////
// channel_Data.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides implementations for helper functions used
//    to facilitate the use of multiple data channels.
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
//  Sep 18/08  MW       1.01  Renamed class to channel_data as we will use multiple
//                            instances rather than seperate classes for each channel
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "rillog.h"
#include "channelbase.h"
#include "silo_factory.h"
#include "channel_data.h"

extern BYTE* g_szDataPort1;
extern BOOL  g_bIsSocket;

//  Init commands for this channel.
INITSTRING_DATA DataBasicInitString = { "E0V1Q0X4|+CMEE=1", 0 };
INITSTRING_DATA DataUnlockInitString = { "", 0 };
INITSTRING_DATA DataPowerOnInitString = { "", 0 };
INITSTRING_DATA DataReadyInitString = { "", 0 };

CChannel_Data::CChannel_Data(EnumRilChannel eChannel)
:   CChannel(eChannel)
{
    RIL_LOG_VERBOSE("CChannel_Data::CChannel_Data() - Enter\r\n");

#ifndef __linux__
    memset(&m_NdisContext, 0, sizeof(m_NdisContext));
#else  // __linux__
    m_uiContextID = 0;
#endif // __linux__

    m_bDataMode = FALSE;
    m_pContextPhaseDoneEvent = new CEvent();

    RIL_LOG_VERBOSE("CChannel_Data::CChannel_Data() - Exit\r\n");
}

CChannel_Data::~CChannel_Data()
{
    RIL_LOG_VERBOSE("CChannel_Data::~CChannel_Data() - Enter\r\n");

    delete []m_prisdModuleInit;
    m_prisdModuleInit = NULL;

    delete m_pContextPhaseDoneEvent;
    m_pContextPhaseDoneEvent = NULL;

    RIL_LOG_VERBOSE("CChannel_Data::~CChannel_Data() - Exit\r\n");
}

//  Override from base class
BOOL CChannel_Data::OpenPort()
{
    BOOL bRetVal = FALSE;

    RIL_LOG_INFO("CChannel_Data::OpenPort() - Opening COM Port: %s...\r\n", g_szDataPort1);
    RIL_LOG_INFO("CChannel_Data::OpenPort() - g_bIsSocket=[%d]...\r\n", g_bIsSocket);

    // TODO: Grab this from repository
    bRetVal = m_Port.Open(g_szDataPort1, g_bIsSocket);

    RIL_LOG_INFO("CChannel_Data::OpenPort() - Opening COM Port: %s\r\n", bRetVal ? "SUCCESS" : "FAILED!");
    
    return bRetVal;
}

BOOL CChannel_Data::FinishInit()
{
    RIL_LOG_VERBOSE("CChannel_Data::FinishInit() - Enter\r\n");
    BOOL bRet = FALSE;

    //  Init our channel AT init commands.
    m_prisdModuleInit = new INITSTRING_DATA[COM_MAX_INDEX];
    if (!m_prisdModuleInit)
    {
        RIL_LOG_CRITICAL("CChannel_Data::Init : ERROR : chnl=[%d] Could not create new INITSTRING_DATA\r\n", m_eRilChannel);
        goto Error;
    }

    m_prisdModuleInit[COM_BASIC_INIT_INDEX]     = DataBasicInitString;
    m_prisdModuleInit[COM_UNLOCK_INIT_INDEX]    = DataUnlockInitString;
    m_prisdModuleInit[COM_POWER_ON_INIT_INDEX]  = DataPowerOnInitString;
    m_prisdModuleInit[COM_READY_INIT_INDEX]     = DataReadyInitString;

#ifndef __linux__
    if (!m_RilNdis.Init(this, m_pComPortCancelEvent))
    {
        RIL_LOG_CRITICAL("CChannel_Data::Init : chnl=[%d] FAILED!\r\n", m_eRilChannel);
        goto Error;
    }
#endif // __linux__

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CChannel_Data::FinishInit() - Exit\r\n");
    return bRet;
}


//
//  Add silos with this channel.
//  Note that the CChannel destructor will destroy these CSilo objects.
//
BOOL CChannel_Data::AddSilos()
{
    RIL_LOG_VERBOSE("CChannel_Data::AddSilos() - Enter\r\n");

    //  Data1 channel contains 1 silo:
    //    Data Silo
    CSilo *pSilo = NULL;
    BOOL bRet = FALSE;

#if !defined(__linux__)
    if (NULL == m_pRilHandle)
    {
        RIL_LOG_CRITICAL("CChannel_Data::AddSilos() : ERROR : m_pRilHandle was NULL\r\n");
        goto Error;
    }
#endif

#if defined(__linux__)
    pSilo = CSilo_Factory::GetSiloData(this);
#else
    pSilo = new CSilo_Data(this, m_pRilHandle);
#endif
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_Data::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_Data\r\n", m_eRilChannel);
        goto Error;
    }

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CChannel_Data::AddSilos() - Exit\r\n");
    return bRet;
}

//
//  Returns a pointer to the channel linked to the given context ID
//
CChannel_Data* CChannel_Data::GetChnlFromContextID(UINT32 dwContextID)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetChnlFromContextID() - Enter\r\n");

    extern CChannel* g_pRilChannel[RIL_CHANNEL_MAX];
    CChannel_Data* pChannelData = NULL;

#ifdef RIL_ENABLE_CHANNEL_DATA1
    for (int i = RIL_CHANNEL_DATA1; i < RIL_CHANNEL_MAX; i++)
    {
        if (NULL == g_pRilChannel[i])
        {
            RIL_LOG_CRITICAL("CChannel_Data::GetChnlFromContextID() : ERROR : g_pRilChannel[%d] was NULL\r\n", i);
            goto Error;
        }

        CChannel_Data* pTemp = static_cast<CChannel_Data*>(g_pRilChannel[i]);
        if (pTemp && pTemp->GetContextID() == dwContextID)
        {
            pChannelData = pTemp;
            break;
        }
    }

Error:
#endif // RIL_ENABLE_CHANNEL_DATA1

	RIL_LOG_VERBOSE("CChannel_Data::GetChnlFromContextID() - Exit\r\n");
    return pChannelData;
}

//
//  Returns a free channel to use for data
//
CChannel_Data* CChannel_Data::GetFreeChnl()
{
    RIL_LOG_VERBOSE("CChannel_Data::GetFreeChnl() - Enter\r\n");

    extern CChannel* g_pRilChannel[RIL_CHANNEL_MAX];
    CChannel_Data* pChannelData = NULL;

#ifdef RIL_ENABLE_CHANNEL_DATA1
    for (int i = RIL_CHANNEL_DATA1; i < RIL_CHANNEL_MAX; i++)
    {
        if (NULL == g_pRilChannel[i])
        {
            RIL_LOG_CRITICAL("CChannel_Data::GetFreeChnl() : ERROR : g_pRilChannel[%d] was NULL\r\n", i);
            goto Error;
        }

        CChannel_Data* pTemp = static_cast<CChannel_Data*>(g_pRilChannel[i]);
        if (pTemp && pTemp->GetContextID() == 0)
        {
            pChannelData = pTemp;
            break;
        }
    }

Error:
#endif // RIL_ENABLE_CHANNEL_DATA1

	RIL_LOG_VERBOSE("CChannel_Data::GetFreeChnl() - Exit\r\n");
    return pChannelData;
}

//
//  Sets this channel's context ID value
//
BOOL CChannel_Data::SetContextID(UINT32 dwContextID)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetContextID() - Enter\r\n");

    BOOL fRet = FALSE;

    // If we are setting a value other than 0, make sure it isn't already in use!
    if (dwContextID)
    {
#ifndef __linux__
        if (m_NdisContext.dwContextID)
        {
            RIL_LOG_CRITICAL("CChannel_Data::SetContextID() : ERROR : m_NdisContext.dwContextID was NULL\r\n");
            goto Error;
        }
#endif // __linux__
    }

#ifdef __linux__
    m_uiContextID = dwContextID;
#else  // __linux__
    m_NdisContext.dwContextID = dwContextID;
#endif // __linux__

    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CChannel_Data::SetContextID() - Exit\r\n");
    return fRet;
}

//
//  Can be used to notify MUX of switch to Data mode if required
//
BOOL CChannel_Data::SetDataMode(
    BOOL bDataMode   // [in ] TRUE - data mode, FALSE - command mode
)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetDataMode() - Enter\r\n");
    BOOL bRet = TRUE;

    m_bDataMode = bDataMode;

    if(!bDataMode)
    {
#ifndef __linux__
        m_RilNdis.ExitDataMode();  // break sendQ
#endif // __linux__
    }

    RIL_LOG_VERBOSE("CChannel_Data::SetDataMode() - Exit\r\n");
    return bRet;
}


#ifndef __linux__
BOOL CChannel_Data::SetNdisContext(const RILNDISGPRSCONTEXT* pContext)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetNdisContext() - Enter\r\n");

    if(pContext)
    {
        if(GetContextID() != pContext->dwContextID)
        {
            RIL_LOG_CRITICAL("CChannel_Data::SetContextID() : ERROR : Context ids don't match\r\n");
            return FALSE;
        }
        m_NdisContext = *pContext;
    }
    else // delete Ndis context
    {
        memset(&m_NdisContext, 0, sizeof(m_NdisContext));
    }

    RIL_LOG_VERBOSE("CChannel_Data::SetNdisContext() - Exit\r\n");
    return TRUE;
}

HRESULT CChannel_Data::ReturnRxNdisPacket(
    RILNDISPACKET* pPacket // @parm
)
{
    m_RilNdis.ReleaseNdisPacket(pPacket, TRUE);
    return S_OK;
}

// static function
void CChannel_Data::NdisReceivePacketDone(
    const LPRILNDISPACKET pPacket // @parm
)
{
    RIL_LOG_VERBOSE("CChannel_Data::NdisReceivePacketDone() - Enter\r\n");

    if(!pPacket || !pPacket->dwContextId)
    {
        RIL_LOG_CRITICAL("CChannel_Data::NdisReceivePacketDone() : ERROR : pPacket or pPacket->dwContextId were NULL\r\n");
        return;
    }

    CChannel_Data* pChannelData = CChannel_Data::GetChnlFromContextID(pPacket->dwContextId);

    if(pChannelData)
    {
        HRESULT hr = pChannelData->ReturnRxNdisPacket(pPacket);
        if (!SUCCEEDED(hr))
        {
            RIL_LOG_CRITICAL("CChannel_Data::NdisReceivePacketDone() : ERROR : pChannelData->ReturnRxNdisPacket failed\r\n");
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CChannel_Data::NdisReceivePacketDone() : ERROR : pChannelData was NULL\r\n");
    }

    RIL_LOG_VERBOSE("CChannel_Data::NdisReceivePacketDone() - Exit\r\n");
}


// static function
void CChannel_Data::NdisSendPacket(
    const LPRILNDISPACKET pPacket // @parm
)
{
    RIL_LOG_VERBOSE("CChannel_Data::NdisSendPacket() - Enter\r\n");

    if(!pPacket || !pPacket->dwContextId)
    {
        RIL_LOG_CRITICAL("CChannel_Data::NdisSendPacket() : ERROR : pPacket or pPacket->dwContextId was NULL\r\n");
        return;
    }

    CChannel_Data* pChannelData = CChannel_Data::GetChnlFromContextID(pPacket->dwContextId);

    if(pChannelData)
    {
        HRESULT hr = pChannelData->m_RilNdis.QTxNdisPacket(pPacket);
        if (!SUCCEEDED(hr))
        {
            RIL_LOG_CRITICAL("CChannel_Data::NdisSendPacket() : ERROR : pChannelData->m_RilNdis.QTxNdisPacket failed\r\n");
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CChannel_Data::NdisSendPacket() : ERROR : pChannelData was NULL\r\n");
    }

    RIL_LOG_VERBOSE("CChannel_Data::NdisSendPacket() - Exit\r\n");
}
#endif // __linux__

// Triggers the RNDIS on the channel to send the next packet
BOOL CChannel_Data::SendDataInDataMode()
{
    RIL_LOG_VERBOSE("CChannel_Data::SendDataInDataMode() - Enter\r\n");

    while (IsInDataMode())
    {
#ifndef __linux__
        if (FAILED(m_RilNdis.SendNdisPacket()))
        {
            RIL_LOG_CRITICAL("CChannel_Data::SendDataInDataMode() : ERROR : m_RilNdis.SendNdisPacket() failed\r\n");
            return FALSE;
        }
#endif // __linux__
    }

    RIL_LOG_VERBOSE("CChannel_Data::SendDataInDataMode() - Exit\r\n");
    return TRUE;
}
/*
BOOL CChannel_Data::HandleRxData(char *szData, UINT32 dwRead, void* pRxData)
{
    RIL_LOG_VERBOSE("CChannel_Data::HandleRxData() - Enter\r\n");
    BOOL bRet = FALSE;

    if(IsInDataMode())
    {
#ifndef __linux__
        if(SUCCEEDED(m_RilNdis.ProcessBPData(pRxData, (BYTE*)szData, dwRead)))
        {
            bRet = TRUE;
        }
        else
        {
            RIL_LOG_CRITICAL("CChannel_Data::HandleRxData() : ERROR : m_RilNdis.ProcessBPData() failed\r\n");
        }
#endif // __linux__
    }
    else
    {
        bRet = CChannel::HandleRxData(szData, dwRead, pRxData);
    }

    RIL_LOG_VERBOSE("CChannel_Data::HandleRxData() - Exit\r\n");

    return bRet;
}
*/
/*
BYTE* CChannel_Data::ProvideRxBuf(UINT32& cbSize, void*& pRxData)
{
    if (FDataMode())
    {
#ifdef __linux__
        return NULL;
#else  // __linux__
        return m_RilNdis.GetRxBuf(cbSize, pRxData);
#endif // __linux__
    }
    else
    {
        return NULL;
    }
}
*/

/*
void CChannel_Data::ReturnRxBuf(BYTE* pBuf)
{
#ifndef __linux__
    m_RilNdis.ReturnRxBuf(pBuf);
#endif // __linux__
}
*/

