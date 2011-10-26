////////////////////////////////////////////////////////////////////////////
// channel_DLC8.cpp
//
// Copyright 2005-2011 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides implementations for helper functions used
//    to facilitate the use of multiple AT channels.
//    SIM related functions, SIM toolkit
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
#include "channel_DLC8.h"

extern char* g_szDLC8Port;
extern BOOL  g_bIsSocket;

//  Com init strings for this channel.
//  SIM related functions, SIM toolkit
INITSTRING_DATA DLC8BasicInitString   = { "E0V1Q0X4|+CMEE=1|S0=0" };
INITSTRING_DATA DLC8UnlockInitString  = { "" };
INITSTRING_DATA DLC8PowerOnInitString = { "" };
INITSTRING_DATA DLC8ReadyInitString   = { "" };

CChannel_DLC8::CChannel_DLC8(UINT32 uiChannel)
: CChannel(uiChannel)
{
    RIL_LOG_VERBOSE("CChannel_DLC8::CChannel_DLC8() - Enter\r\n");
    RIL_LOG_VERBOSE("CChannel_DLC8::CChannel_DLC8() - Exit\r\n");
}

CChannel_DLC8::~CChannel_DLC8()
{
    RIL_LOG_VERBOSE("CChannel_DLC8::~CChannel_DLC8() - Enter\r\n");
    delete []m_prisdModuleInit;
    m_prisdModuleInit = NULL;
    RIL_LOG_VERBOSE("CChannel_DLC8::~CChannel_DLC8() - Exit\r\n");
}

//  Override from base class
BOOL CChannel_DLC8::OpenPort()
{
    BOOL bRetVal = FALSE;

    RIL_LOG_INFO("CChannel_DLC8::OpenPort() - Opening COM Port: %s...\r\n", g_szDLC8Port);
    RIL_LOG_INFO("CChannel_DLC8::OpenPort() - g_bIsSocket=[%d]...\r\n", g_bIsSocket);

    bRetVal = m_Port.Open(g_szDLC8Port, g_bIsSocket);

    RIL_LOG_INFO("CChannel_DLC8::OpenPort() - Opening COM Port: %s\r\n", bRetVal ? "SUCCESS" : "FAILED!");

    return bRetVal;
}

BOOL CChannel_DLC8::FinishInit()
{
    RIL_LOG_VERBOSE("CChannel_DLC8::FinishInit() - Enter\r\n");
    BOOL bRet = FALSE;

    //  Init our channel AT init commands.
    m_prisdModuleInit = new INITSTRING_DATA[COM_MAX_INDEX];
    if (!m_prisdModuleInit)
    {
        RIL_LOG_CRITICAL("CChannel_DLC8::FinishInit() - ERROR: chnl=[%d] Could not create new INITSTRING_DATA\r\n", m_uiRilChannel);
        goto Error;
    }

    m_prisdModuleInit[COM_BASIC_INIT_INDEX]     = DLC8BasicInitString;
    m_prisdModuleInit[COM_UNLOCK_INIT_INDEX]    = DLC8UnlockInitString;
    m_prisdModuleInit[COM_POWER_ON_INIT_INDEX]  = DLC8PowerOnInitString;
    m_prisdModuleInit[COM_READY_INIT_INDEX]     = DLC8ReadyInitString;

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CChannel_DLC8::FinishInit() - Exit\r\n");
    return bRet;
}

//
//  Add silos with this channel.
//  Note that the CChannel destructor will destroy these CSilo objects.
//
BOOL CChannel_DLC8::AddSilos()
{
    RIL_LOG_VERBOSE("CChannel_DLC8::AddSilos() - Enter\r\n");
    BOOL bRet = FALSE;

    //  DLC8 channel contains 5 silos:
    //     Voice Silo
    //     Network Silo
    //     SMS Silo
    //     Phonebook Silo
    //     SIM Silo
    CSilo *pSilo = NULL;


    pSilo = CSilo_Factory::GetSiloVoice(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_DLC8::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_Voice\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloNetwork(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_DLC8::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_Network\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloSMS(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_DLC8::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_SMS\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloSIM(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_DLC8::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_SIM\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloPhonebook(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_DLC8::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_Phonebook\r\n", m_uiRilChannel);
        goto Error;
    }

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CChannel_DLC8::AddSilos() - Exit\r\n");
    return bRet;
}
