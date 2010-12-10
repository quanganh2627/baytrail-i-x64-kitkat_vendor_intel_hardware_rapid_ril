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

//  Com init strings for this channel.

INITSTRING_DATA ATCmdBasicInitString    = { "E0V1Q0X4|S0=0|+CMEE=1",  0};
INITSTRING_DATA ATCmdUnlockInitString   = { "+XRAT=1,2|+CRC=1|+CR=1", 0 };
INITSTRING_DATA ATCmdPowerOnInitString  = { "", 0 };
INITSTRING_DATA ATCmdReadyInitString    = { "+CSSN=1,1|+CMGF=0|+CCWA=1|+CUSD=1|+CTZU=1|+CTZR=1|+XCALLSTAT=1|+XREG=1|+CREG=2|+CGREG=2", 0 };

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

BOOL CChannel_ATCmd::FinishInit()
{
    RIL_LOG_VERBOSE("CChannel_ATCmd::FinishInit() - Enter\r\n");
    BOOL bRet = FALSE;

    //  Init our channel AT init commands.
    m_prisdModuleInit = new INITSTRING_DATA[COM_MAX_INDEX];
    if (!m_prisdModuleInit)
    {
        RIL_LOG_CRITICAL("CChannel_ATCmd::Init : chnl=[%d] Could not create new INITSTRING_DATA\r\n", m_uiRilChannel);
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

    //  ATCmd channel contains 6 silos:
    //     Voice Silo
    //     Network Silo
    //     SMS Silo
    //     Phonebook Silo
    //     SIM Silo
    //     Data Silo
    CSilo *pSilo = NULL;


    pSilo = CSilo_Factory::GetSiloVoice(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_ATCmd::RegisterSilos : ERROR : chnl=[%d] Could not add CSilo_Voice\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloNetwork(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_ATCmd::RegisterSilos : ERROR : chnl=[%d] Could not add CSilo_Network\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloSMS(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_ATCmd::RegisterSilos : ERROR : chnl=[%d] Could not add CSilo_SMS\r\n", m_uiRilChannel);
        goto Error;
    }

#ifndef RIL_ENABLE_CHANNEL_SIM
    pSilo = CSilo_Factory::GetSiloSIM(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_ATCmd::RegisterSilos : ERROR : chnl=[%d] Could not add CSilo_SIM\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloPhonebook(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_ATCmd::RegisterSilos : ERROR : chnl=[%d] Could not add CSilo_Phonebook\r\n", m_uiRilChannel);
        goto Error;
    }
#endif // RIL_ENABLE_CHANNEL_SIM

    pSilo = CSilo_Factory::GetSiloData(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_ATCmd::RegisterSilos : ERROR : chnl=[%d] Could not add CSilo_Data\r\n", m_uiRilChannel);
        goto Error;
    }


    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CChannel_ATCmd::AddSilos() - Exit\r\n");
    return bRet;
}

