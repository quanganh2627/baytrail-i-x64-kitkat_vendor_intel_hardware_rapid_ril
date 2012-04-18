////////////////////////////////////////////////////////////////////////////
// channel_OEM.cpp
//
// Copyright 2005-2012 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides implementations for helper functions used
//    to facilitate the use of multiple AT channels.
//    Diagnostic commands using OEM HOOK API are sent on this channel.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "rillog.h"
#include "channelbase.h"
#include "silo_factory.h"
#include "channel_OEM.h"

extern char* g_szOEMPort;
extern BOOL  g_bIsSocket;

//  Com init strings for this channel.
//  OEM commands (diagnostic)
INITSTRING_DATA OEMBasicInitString   = { "E0V1Q0X4|+CMEE=1|S0=0" };
INITSTRING_DATA OEMUnlockInitString  = { "" };
INITSTRING_DATA OEMPowerOnInitString = { "" };
INITSTRING_DATA OEMReadyInitString   = { "" };

CChannel_OEM::CChannel_OEM(UINT32 uiChannel)
: CChannel(uiChannel)
{
    RIL_LOG_VERBOSE("CChannel_OEM::CChannel_OEM() - Enter\r\n");
    RIL_LOG_VERBOSE("CChannel_OEM::CChannel_OEM() - Exit\r\n");
}

CChannel_OEM::~CChannel_OEM()
{
    RIL_LOG_VERBOSE("CChannel_OEM::~CChannel_OEM() - Enter\r\n");
    delete []m_prisdModuleInit;
    m_prisdModuleInit = NULL;
    RIL_LOG_VERBOSE("CChannel_OEM::~CChannel_OEM() - Exit\r\n");
}

//  Override from base class
BOOL CChannel_OEM::OpenPort()
{
    BOOL bRetVal = FALSE;

    RIL_LOG_INFO("CChannel_OEM::OpenPort() - Opening COM Port: %s...\r\n", g_szOEMPort);
    RIL_LOG_INFO("CChannel_OEM::OpenPort() - g_bIsSocket=[%d]...\r\n", g_bIsSocket);

    bRetVal = m_Port.Open(g_szOEMPort, g_bIsSocket);

    RIL_LOG_INFO("CChannel_OEM::OpenPort() - Opening COM Port: %s\r\n", bRetVal ? "SUCCESS" : "FAILED!");

    return bRetVal;
}

BOOL CChannel_OEM::FinishInit()
{
    RIL_LOG_VERBOSE("CChannel_OEM::FinishInit() - Enter\r\n");
    BOOL bRet = FALSE;

    //  Init our channel AT init commands.
    m_prisdModuleInit = new INITSTRING_DATA[COM_MAX_INDEX];
    if (!m_prisdModuleInit)
    {
        RIL_LOG_CRITICAL("CChannel_OEM::FinishInit() - chnl=[%d] Could not create new INITSTRING_DATA\r\n", m_uiRilChannel);
        goto Error;
    }

    m_prisdModuleInit[COM_BASIC_INIT_INDEX]     = OEMBasicInitString;
    m_prisdModuleInit[COM_UNLOCK_INIT_INDEX]    = OEMUnlockInitString;
    m_prisdModuleInit[COM_POWER_ON_INIT_INDEX]  = OEMPowerOnInitString;
    m_prisdModuleInit[COM_READY_INIT_INDEX]     = OEMReadyInitString;

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CChannel_OEM::FinishInit() - Exit\r\n");
    return bRet;
}

//
//  Add silos with this channel.
//  Note that the CChannel destructor will destroy these CSilo objects.
//
BOOL CChannel_OEM::AddSilos()
{
    RIL_LOG_VERBOSE("CChannel_OEM::AddSilos() - Enter\r\n");
    BOOL bRet = FALSE;

    //  OEM channel contains 5 silos:
    //     Voice Silo
    //     Phonebook Silo
    CSilo *pSilo = NULL;


    pSilo = CSilo_Factory::GetSiloVoice(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_OEM::AddSilos() : chnl=[%d] Could not add CSilo_Voice\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloPhonebook(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_OEM::AddSilos() : chnl=[%d] Could not add CSilo_Phonebook\r\n", m_uiRilChannel);
        goto Error;
    }

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CChannel_OEM::AddSilos() - Exit\r\n");
    return bRet;
}

