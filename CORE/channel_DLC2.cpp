////////////////////////////////////////////////////////////////////////////
// channel_DLC2.cpp
//
// Copyright 2005-2011 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides implementations for helper functions used
//    to facilitate the use of multiple AT channels.
//    GPRS/UMTS management (GPRS attach/detach), network commands
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

#include "types.h"
#include "rillog.h"
#include "channelbase.h"
#include "silo_factory.h"
#include "channel_DLC2.h"

extern BYTE* g_szDLC2Port;
extern BOOL  g_bIsSocket;

//  Com init strings for this channel.
//  GPRS/UMTS management (GPRS attach/detach), network commands
INITSTRING_DATA DLC2BasicInitString   = { "E0V1Q0X4|S0=0|+CMEE=1" };
INITSTRING_DATA DLC2UnlockInitString  = { "" };
INITSTRING_DATA DLC2PowerOnInitString = { "" };
INITSTRING_DATA DLC2ReadyInitString   = { "" };

CChannel_DLC2::CChannel_DLC2(UINT32 uiChannel)
: CChannel(uiChannel)
{
    RIL_LOG_VERBOSE("CChannel_DLC2::CChannel_DLC2() - Enter\r\n");
    RIL_LOG_VERBOSE("CChannel_DLC2::CChannel_DLC2() - Exit\r\n");
}

CChannel_DLC2::~CChannel_DLC2()
{
    RIL_LOG_VERBOSE("CChannel_DLC2::~CChannel_DLC2() - Enter\r\n");
    delete []m_prisdModuleInit;
    m_prisdModuleInit = NULL;
    RIL_LOG_VERBOSE("CChannel_DLC2::~CChannel_DLC2() - Exit\r\n");
}

//  Override from base class
BOOL CChannel_DLC2::OpenPort()
{
    BOOL bRetVal = FALSE;

    RIL_LOG_INFO("CChannel_DLC2::OpenPort() - Opening COM Port: %s...\r\n", g_szDLC2Port);
    RIL_LOG_INFO("CChannel_DLC2::OpenPort() - g_bIsSocket=[%d]...\r\n", g_bIsSocket);

    bRetVal = m_Port.Open(g_szDLC2Port, g_bIsSocket);

    RIL_LOG_INFO("CChannel_DLC2::OpenPort() - Opening COM Port: %s\r\n", bRetVal ? "SUCCESS" : "FAILED!");

    return bRetVal;
}

BOOL CChannel_DLC2::FinishInit()
{
    RIL_LOG_VERBOSE("CChannel_DLC2::FinishInit() - Enter\r\n");
    BOOL bRet = FALSE;

    //  Init our channel AT init commands.
    m_prisdModuleInit = new INITSTRING_DATA[COM_MAX_INDEX];
    if (!m_prisdModuleInit)
    {
        RIL_LOG_CRITICAL("CChannel_DLC2::FinishInit() - ERROR: chnl=[%d] Could not create new INITSTRING_DATA\r\n", m_uiRilChannel);
        goto Error;
    }

    m_prisdModuleInit[COM_BASIC_INIT_INDEX]     = DLC2BasicInitString;
    m_prisdModuleInit[COM_UNLOCK_INIT_INDEX]    = DLC2UnlockInitString;
    m_prisdModuleInit[COM_POWER_ON_INIT_INDEX]  = DLC2PowerOnInitString;
    m_prisdModuleInit[COM_READY_INIT_INDEX]     = DLC2ReadyInitString;

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CChannel_DLC2::FinishInit() - Exit\r\n");
    return bRet;
}

//
//  Add silos with this channel.
//  Note that the CChannel destructor will destroy these CSilo objects.
//
BOOL CChannel_DLC2::AddSilos()
{
    RIL_LOG_VERBOSE("CChannel_DLC2::AddSilos() - Enter\r\n");
    BOOL bRet = FALSE;

    //  DLC2 channel contains 5 silos:
    //     Voice Silo
    //     Network Silo
    //     SMS Silo
    //     Phonebook Silo
    //     SIM Silo
    CSilo *pSilo = NULL;


    pSilo = CSilo_Factory::GetSiloVoice(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_DLC2::RegisterSilos : ERROR : chnl=[%d] Could not add CSilo_Voice\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloNetwork(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_DLC2::RegisterSilos : ERROR : chnl=[%d] Could not add CSilo_Network\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloSMS(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_DLC2::RegisterSilos : ERROR : chnl=[%d] Could not add CSilo_SMS\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloSIM(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_DLC2::RegisterSilos : ERROR : chnl=[%d] Could not add CSilo_SIM\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloPhonebook(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_DLC2::RegisterSilos : ERROR : chnl=[%d] Could not add CSilo_Phonebook\r\n", m_uiRilChannel);
        goto Error;
    }

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CChannel_DLC2::AddSilos() - Exit\r\n");
    return bRet;
}

