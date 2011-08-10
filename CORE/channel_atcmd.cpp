////////////////////////////////////////////////////////////////////////////
// channel_ATCmd.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides implementations for helper functions used
//    to facilitate the use of multiple command channels.
//    Call control commands, misc commands
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

#include "types.h"
#include "rillog.h"
#include "channelbase.h"
#include "channel_atcmd.h"
#include "silo_factory.h"

extern BYTE* g_szCmdPort;
extern BOOL  g_bIsSocket;

//  Com init strings for this channel.
//  Call control commands, misc commands

INITSTRING_DATA ATCmdBasicInitString    = { "E0V1Q0X4|+XGENDATA|+XPOW=0,0,0|S0=0|+CMEE=1" };
INITSTRING_DATA ATCmdUnlockInitString   = { "" };
INITSTRING_DATA ATCmdPowerOnInitString  = { "" };
INITSTRING_DATA ATCmdReadyInitString    = { "" };

CChannel_ATCmd::CChannel_ATCmd(UINT32 uiChannel)
: CChannel(uiChannel)
{
    RIL_LOG_VERBOSE("CChannel_ATCmd::CChannel_ATCmd() - Enter\r\n");
    RIL_LOG_VERBOSE("CChannel_ATCmd::CChannel_ATCmd() - Exit\r\n");
}

CChannel_ATCmd::~CChannel_ATCmd()
{
    RIL_LOG_VERBOSE("CChannel_ATCmd::~CChannel_ATCmd() - Enter\r\n");
    delete []m_prisdModuleInit;
    RIL_LOG_VERBOSE("CChannel_ATCmd::~CChannel_ATCmd() - Exit\r\n");
    m_prisdModuleInit = NULL;

}

//  Override from base class
BOOL CChannel_ATCmd::OpenPort()
{
    BOOL bRetVal = FALSE;

    RIL_LOG_INFO("CChannel_ATCmd::OpenPort() - Opening COM Port: %s...\r\n", g_szCmdPort);
    RIL_LOG_INFO("CChannel_ATCmd::OpenPort() - g_bIsSocket=[%d]...\r\n", g_bIsSocket);

    bRetVal = m_Port.Open(g_szCmdPort, g_bIsSocket);

    RIL_LOG_INFO("CChannel_ATCmd::OpenPort() - Opening COM Port: %s\r\n", bRetVal ? "SUCCESS" : "FAILED!");

    return bRetVal;
}


BOOL CChannel_ATCmd::FinishInit()
{
    RIL_LOG_VERBOSE("CChannel_ATCmd::FinishInit() - Enter\r\n");
    BOOL bRet = FALSE;

    //  Init our channel AT init commands.
    m_prisdModuleInit = new INITSTRING_DATA[COM_MAX_INDEX];
    if (!m_prisdModuleInit)
    {
        RIL_LOG_CRITICAL("CChannel_ATCmd::FinishInit() : chnl=[%d] Could not create new INITSTRING_DATA\r\n", m_uiRilChannel);
        goto Error;
    }

    m_prisdModuleInit[COM_BASIC_INIT_INDEX]     = ATCmdBasicInitString;
    m_prisdModuleInit[COM_UNLOCK_INIT_INDEX]    = ATCmdUnlockInitString;
    m_prisdModuleInit[COM_POWER_ON_INIT_INDEX]  = ATCmdPowerOnInitString;
    m_prisdModuleInit[COM_READY_INIT_INDEX]     = ATCmdReadyInitString;

    bRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CChannel_ATCmd::FinishInit() - Exit\r\n");
    return bRet;
}

//
//  Add silos with this channel.
//  Note that the CChannel destructor will destroy these CSilo objects.
//
BOOL CChannel_ATCmd::AddSilos()
{
    RIL_LOG_VERBOSE("CChannel_ATCmd::AddSilos() - Enter\r\n");
    BOOL bRet = FALSE;

    //  ATCmd channel contains 5 silos:
    //     Voice Silo
    //     Network Silo
    //     SMS Silo
    //     Phonebook Silo
    //     SIM Silo
    CSilo *pSilo = NULL;


    pSilo = CSilo_Factory::GetSiloVoice(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_ATCmd::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_Voice\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloNetwork(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_ATCmd::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_Network\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloSMS(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_ATCmd::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_SMS\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloSIM(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_ATCmd::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_SIM\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloPhonebook(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_ATCmd::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_Phonebook\r\n", m_uiRilChannel);
        goto Error;
    }


    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CChannel_ATCmd::AddSilos() - Exit\r\n");
    return bRet;
}

