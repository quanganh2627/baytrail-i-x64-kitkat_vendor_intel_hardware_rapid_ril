////////////////////////////////////////////////////////////////////////////
// channel_SIM.cpp
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
// Created: 2008-07-09
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Jul 9/08   DP       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "rillog.h"
#include "channelbase.h"
#include "silo_factory.h"
#include "channel_sim.h"


//  Com init strings for this channel.
INITSTRING_DATA SIMBasicInitString   = { "E0V0Q0X4|+CMEE=1", 0 };
INITSTRING_DATA SIMUnlockInitString  = { "", 0 };
INITSTRING_DATA SIMPowerOnInitString = { "+CSCS=\"UCS2\"", 0 };
INITSTRING_DATA SIMReadyInitString   = { "", 0 };

CChannel_SIM::CChannel_SIM(EnumRilChannel eChannel)
: CChannel(eChannel)
{
    RIL_LOG_VERBOSE("CChannel_SIM::CChannel_SIM() - Enter\r\n");
    RIL_LOG_VERBOSE("CChannel_SIM::CChannel_SIM() - Exit\r\n");
}

CChannel_SIM::~CChannel_SIM()
{
    RIL_LOG_VERBOSE("CChannel_SIM::~CChannel_SIM() - Enter\r\n");
    delete []m_prisdModuleInit;
    m_prisdModuleInit = NULL;
    RIL_LOG_VERBOSE("CChannel_SIM::~CChannel_SIM() - Exit\r\n");
}

BOOL CChannel_SIM::FinishInit()
{
    RIL_LOG_VERBOSE("CChannel_SIM::FinishInit() - Enter\r\n");
    BOOL bRet = FALSE;

    //  Init our channel AT init commands.
    m_prisdModuleInit = new INITSTRING_DATA[COM_MAX_INDEX];
    if (!m_prisdModuleInit)
    {
        RIL_LOG_CRITICAL("CChannel_SIM::FinishInit() - ERROR: chnl=[%d] Could not create new INITSTRING_DATA\r\n", m_eRilChannel);
        goto Error;
    }

    m_prisdModuleInit[COM_BASIC_INIT_INDEX]     = SIMBasicInitString;
    m_prisdModuleInit[COM_UNLOCK_INIT_INDEX]    = SIMUnlockInitString;
    m_prisdModuleInit[COM_POWER_ON_INIT_INDEX]  = SIMPowerOnInitString;
    m_prisdModuleInit[COM_READY_INIT_INDEX]     = SIMReadyInitString;

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CChannel_SIM::FinishInit() - Exit\r\n");
    return bRet;
}

//
//  Add silos with this channel.
//  Note that the CChannel destructor will destroy these CSilo objects.
//
BOOL CChannel_SIM::AddSilos()
{
    RIL_LOG_VERBOSE("CChannel_SIM::AddSilos() - Enter\r\n");

    //  SIM channel contains 2 silos:
    //    SIM silo
    //    Phonebook silo
    CSilo *pSilo = NULL;
    BOOL bRet = FALSE;

#if !defined(__linux__)
    if (NULL == m_pRilHandle)
    {
        RIL_LOG_CRITICAL("CChannel_SIM::AddSilos() : ERROR : m_pRilHandle was NULL\r\n");
        goto Error;
    }
#endif

#if defined(__linux__)
    pSilo = CSilo_Factory::GetSiloSIM(this);
#else
    pSilo = new CSilo_SIM(this, m_pRilHandle);
#endif
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_SIM::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_SIM\r\n", m_eRilChannel);
        goto Error;
    }

#if defined(__linux__)
    pSilo = CSilo_Factory::GetSiloPhonebook(this);
#else
    pSilo = new CSilo_Phonebook(this, m_pRilHandle);
#endif
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_SIM::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_Phonebook\r\n", m_eRilChannel);
        goto Error;
    }

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CChannel_SIM::AddSilos() - Exit\r\n");
    return bRet;
}

