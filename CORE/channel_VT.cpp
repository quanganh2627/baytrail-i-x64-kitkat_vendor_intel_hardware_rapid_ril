////////////////////////////////////////////////////////////////////////////
// channel_VT.cpp
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

#if defined(M2_FEATURE_ENABLED)
#include "types.h"
#include "rillog.h"
#include "channelbase.h"
#include "silo_factory.h"
#include "channel_VT.h"

extern BYTE* g_szVTPort;
extern BOOL  g_bIsSocket;

//  Com init strings for this channel.
//  SIM related functions, SIM toolkit
INITSTRING_DATA VTBasicInitString   = { "E0V1Q0X4|S0=0|+CMEE=1" };
INITSTRING_DATA VTUnlockInitString  = { "" };
INITSTRING_DATA VTPowerOnInitString = { "" };
INITSTRING_DATA VTReadyInitString   = { "" };

CChannel_VT::CChannel_VT(UINT32 uiChannel)
: CChannel(uiChannel)
{
    RIL_LOG_VERBOSE("CChannel_VT::CChannel_VT() - Enter\r\n");
    RIL_LOG_VERBOSE("CChannel_VT::CChannel_VT() - Exit\r\n");
}

CChannel_VT::~CChannel_VT()
{
    RIL_LOG_VERBOSE("CChannel_VT::~CChannel_VT() - Enter\r\n");
    delete []m_prisdModuleInit;
    m_prisdModuleInit = NULL;
    RIL_LOG_VERBOSE("CChannel_VT::~CChannel_VT() - Exit\r\n");
}

//  Override from base class
BOOL CChannel_VT::OpenPort()
{
    BOOL bRetVal = FALSE;

    RIL_LOG_INFO("CChannel_VT::OpenPort() - Opening COM Port: %s...\r\n", g_szVTPort);
    RIL_LOG_INFO("CChannel_VT::OpenPort() - g_bIsSocket=[%d]...\r\n", g_bIsSocket);

    bRetVal = m_Port.Open(g_szVTPort, g_bIsSocket);

    RIL_LOG_INFO("CChannel_VT::OpenPort() - Opening COM Port: %s\r\n", bRetVal ? "SUCCESS" : "FAILED!");

    return bRetVal;
}

BOOL CChannel_VT::FinishInit()
{
    RIL_LOG_VERBOSE("CChannel_VT::FinishInit() - Enter\r\n");
    BOOL bRet = FALSE;

    //  Init our channel AT init commands.
    m_prisdModuleInit = new INITSTRING_DATA[COM_MAX_INDEX];
    if (!m_prisdModuleInit)
    {
        RIL_LOG_CRITICAL("CChannel_VT::FinishInit() - ERROR: chnl=[%d] Could not create new INITSTRING_DATA\r\n", m_uiRilChannel);
        goto Error;
    }

    m_prisdModuleInit[COM_BASIC_INIT_INDEX]     = VTBasicInitString;
    m_prisdModuleInit[COM_UNLOCK_INIT_INDEX]    = VTUnlockInitString;
    m_prisdModuleInit[COM_POWER_ON_INIT_INDEX]  = VTPowerOnInitString;
    m_prisdModuleInit[COM_READY_INIT_INDEX]     = VTReadyInitString;

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CChannel_VT::FinishInit() - Exit\r\n");
    return bRet;
}

//
//  Add silos with this channel.
//  Note that the CChannel destructor will destroy these CSilo objects.
//
BOOL CChannel_VT::AddSilos()
{
    RIL_LOG_VERBOSE("CChannel_VT::AddSilos() - Enter\r\n");
    BOOL bRet = FALSE;

    //  VT channel contains 5 silos:
    //     Voice Silo
    //     Network Silo
    //     SMS Silo
    //     Phonebook Silo
    //     SIM Silo
    CSilo *pSilo = NULL;


    pSilo = CSilo_Factory::GetSiloVoice(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_VT::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_Voice\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloNetwork(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_VT::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_Network\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloSMS(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_VT::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_SMS\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloSIM(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_VT::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_SIM\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloPhonebook(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_VT::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_Phonebook\r\n", m_uiRilChannel);
        goto Error;
    }

    bRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CChannel_VT::AddSilos() - Exit\r\n");
    return bRet;
}
#endif // M2_FEATURE_ENABLED
