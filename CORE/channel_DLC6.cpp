////////////////////////////////////////////////////////////////////////////
// channel_DLC6.cpp
//
// Copyright 2005-2011 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides implementations for helper functions used
//    to facilitate the use of multiple AT channels.
//    Call settings, SMS, supplementary services
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "rillog.h"
#include "channelbase.h"
#include "silo_factory.h"
#include "channel_DLC6.h"

extern char* g_szDLC6Port;
extern BOOL  g_bIsSocket;

//  Com init strings for this channel.
//  Call settings, SMS, supplementary services
INITSTRING_DATA DLC6BasicInitString   = { "E0V1Q0X4|+CMEE=1|S0=0|+CMGF=0" };
INITSTRING_DATA DLC6UnlockInitString  = { "+CSMS=1|+CGSMS=3" };
INITSTRING_DATA DLC6PowerOnInitString = { "" };
INITSTRING_DATA DLC6ReadyInitString   = { "" };

CChannel_DLC6::CChannel_DLC6(UINT32 uiChannel)
: CChannel(uiChannel)
{
    RIL_LOG_VERBOSE("CChannel_DLC6::CChannel_DLC6() - Enter\r\n");
    RIL_LOG_VERBOSE("CChannel_DLC6::CChannel_DLC6() - Exit\r\n");
}

CChannel_DLC6::~CChannel_DLC6()
{
    RIL_LOG_VERBOSE("CChannel_DLC6::~CChannel_DLC6() - Enter\r\n");
    delete []m_prisdModuleInit;
    m_prisdModuleInit = NULL;
    RIL_LOG_VERBOSE("CChannel_DLC6::~CChannel_DLC6() - Exit\r\n");
}

//  Override from base class
BOOL CChannel_DLC6::OpenPort()
{
    BOOL bRetVal = FALSE;

    RIL_LOG_INFO("CChannel_DLC6::OpenPort() - Opening COM Port: %s...\r\n", g_szDLC6Port);
    RIL_LOG_INFO("CChannel_DLC6::OpenPort() - g_bIsSocket=[%d]...\r\n", g_bIsSocket);

    bRetVal = m_Port.Open(g_szDLC6Port, g_bIsSocket);

    RIL_LOG_INFO("CChannel_DLC6::OpenPort() - Opening COM Port: %s\r\n", bRetVal ? "SUCCESS" : "FAILED!");

    return bRetVal;
}

BOOL CChannel_DLC6::FinishInit()
{
    RIL_LOG_VERBOSE("CChannel_DLC6::FinishInit() - Enter\r\n");
    BOOL bRet = FALSE;

    //  Init our channel AT init commands.
    m_prisdModuleInit = new INITSTRING_DATA[COM_MAX_INDEX];
    if (!m_prisdModuleInit)
    {
        RIL_LOG_CRITICAL("CChannel_DLC6::FinishInit() - chnl=[%d] Could not create new INITSTRING_DATA\r\n", m_uiRilChannel);
        goto Error;
    }

    m_prisdModuleInit[COM_BASIC_INIT_INDEX]     = DLC6BasicInitString;
    m_prisdModuleInit[COM_UNLOCK_INIT_INDEX]    = DLC6UnlockInitString;
    m_prisdModuleInit[COM_POWER_ON_INIT_INDEX]  = DLC6PowerOnInitString;
    m_prisdModuleInit[COM_READY_INIT_INDEX]     = DLC6ReadyInitString;

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CChannel_DLC6::FinishInit() - Exit\r\n");
    return bRet;
}

//
//  Add silos with this channel.
//  Note that the CChannel destructor will destroy these CSilo objects.
//
BOOL CChannel_DLC6::AddSilos()
{
    RIL_LOG_VERBOSE("CChannel_DLC6::AddSilos() - Enter\r\n");
    BOOL bRet = FALSE;

    //  DLC6 channel contains 5 silos:
    //     Voice Silo
    //     Network Silo
    //     SMS Silo
    //     Phonebook Silo
    //     SIM Silo
    CSilo *pSilo = NULL;


    pSilo = CSilo_Factory::GetSiloVoice(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_DLC6::AddSilos() : chnl=[%d] Could not add CSilo_Voice\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloNetwork(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_DLC6::AddSilos() : chnl=[%d] Could not add CSilo_Network\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloSMS(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_DLC6::AddSilos() : chnl=[%d] Could not add CSilo_SMS\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloSIM(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_DLC6::AddSilos() : chnl=[%d] Could not add CSilo_SIM\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloPhonebook(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_DLC6::AddSilos() : chnl=[%d] Could not add CSilo_Phonebook\r\n", m_uiRilChannel);
        goto Error;
    }

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CChannel_DLC6::AddSilos() - Exit\r\n");
    return bRet;
}
