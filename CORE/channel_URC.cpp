////////////////////////////////////////////////////////////////////////////
// channel_URC.cpp
//
// Copyright 2005-2011 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides implementations for helper functions used
//    to facilitate the use of multiple AT channels.
//    All URCs go on this channel.
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
#include "channel_URC.h"

extern BYTE* g_szURCPort;
extern BOOL  g_bIsSocket;

//  Com init strings for this channel.
//  All URCs go on this channel.
INITSTRING_DATA URCBasicInitString   = { "E0V1Q0X4|S0=0|+CMEE=1|+XSIMSTATE=1|+XCALLSTAT=1|+CTZU=1|+CTZR=1|+XREG=1|+CGEREP=1,0|+CSSN=1,1|+CMGF=0|+XLEMA=1" };
INITSTRING_DATA URCUnlockInitString  = { "+CNMI=2,2,2,1|+CRC=1|+CCWA=1|+CUSD=1" };
INITSTRING_DATA URCPowerOnInitString = { "" };
INITSTRING_DATA URCReadyInitString   = { "" };

CChannel_URC::CChannel_URC(UINT32 uiChannel)
: CChannel(uiChannel)
{
    RIL_LOG_VERBOSE("CChannel_URC::CChannel_URC() - Enter\r\n");
    RIL_LOG_VERBOSE("CChannel_URC::CChannel_URC() - Exit\r\n");
}

CChannel_URC::~CChannel_URC()
{
    RIL_LOG_VERBOSE("CChannel_URC::~CChannel_URC() - Enter\r\n");
    delete []m_prisdModuleInit;
    m_prisdModuleInit = NULL;
    RIL_LOG_VERBOSE("CChannel_URC::~CChannel_URC() - Exit\r\n");
}

//  Override from base class
BOOL CChannel_URC::OpenPort()
{
    BOOL bRetVal = FALSE;

    RIL_LOG_INFO("CChannel_URC::OpenPort() - Opening COM Port: %s...\r\n", g_szURCPort);
    RIL_LOG_INFO("CChannel_URC::OpenPort() - g_bIsSocket=[%d]...\r\n", g_bIsSocket);

    bRetVal = m_Port.Open(g_szURCPort, g_bIsSocket);

    RIL_LOG_INFO("CChannel_URC::OpenPort() - Opening COM Port: %s\r\n", bRetVal ? "SUCCESS" : "FAILED!");

    return bRetVal;
}

BOOL CChannel_URC::FinishInit()
{
    RIL_LOG_VERBOSE("CChannel_URC::FinishInit() - Enter\r\n");
    BOOL bRet = FALSE;

    //  Init our channel AT init commands.
    m_prisdModuleInit = new INITSTRING_DATA[COM_MAX_INDEX];
    if (!m_prisdModuleInit)
    {
        RIL_LOG_CRITICAL("CChannel_URC::FinishInit() - ERROR: chnl=[%d] Could not create new INITSTRING_DATA\r\n", m_uiRilChannel);
        goto Error;
    }

    m_prisdModuleInit[COM_BASIC_INIT_INDEX]     = URCBasicInitString;
    m_prisdModuleInit[COM_UNLOCK_INIT_INDEX]    = URCUnlockInitString;
    m_prisdModuleInit[COM_POWER_ON_INIT_INDEX]  = URCPowerOnInitString;
    m_prisdModuleInit[COM_READY_INIT_INDEX]     = URCReadyInitString;

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CChannel_URC::FinishInit() - Exit\r\n");
    return bRet;
}

//
//  Add silos with this channel.
//  Note that the CChannel destructor will destroy these CSilo objects.
//
BOOL CChannel_URC::AddSilos()
{
    RIL_LOG_VERBOSE("CChannel_URC::AddSilos() - Enter\r\n");
    BOOL bRet = FALSE;

    //  URC channel contains 5 silos:
    //     Voice Silo
    //     Network Silo
    //     SMS Silo
    //     Phonebook Silo
    //     SIM Silo
    CSilo *pSilo = NULL;


    pSilo = CSilo_Factory::GetSiloVoice(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_URC::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_Voice\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloNetwork(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_URC::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_Network\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloSMS(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_URC::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_SMS\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloSIM(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_URC::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_SIM\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloPhonebook(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_URC::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_Phonebook\r\n", m_uiRilChannel);
        goto Error;
    }

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CChannel_URC::AddSilos() - Exit\r\n");
    return bRet;
}

