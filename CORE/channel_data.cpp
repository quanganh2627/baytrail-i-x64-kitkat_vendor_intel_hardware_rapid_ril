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
//    GPRS/UMTS data (1st primary context)
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

extern BYTE* g_szDataPort1; // TODO replace this with base port name e.g., /dev/ttyGSM
extern BOOL  g_bIsSocket;

//  Init commands for this channel.
//  GPRS/UMTS data (1st primary context)
INITSTRING_DATA DataBasicInitString = { "E0V1Q0X4|S0=0|+CMEE=1" };
INITSTRING_DATA DataUnlockInitString = { "" };
INITSTRING_DATA DataPowerOnInitString = { "" };
INITSTRING_DATA DataReadyInitString = { "" };

CChannel_Data::CChannel_Data(UINT32 uiChannel)
:   CChannel(uiChannel),
    m_szIpAddr(NULL),
    m_szDNS1(NULL),
    m_szDNS2(NULL)
{
    RIL_LOG_VERBOSE("CChannel_Data::CChannel_Data() - Enter\r\n");

    m_uiContextID = 0;

    m_pSetupDoneEvent = new CEvent();
    if (!m_pSetupDoneEvent)
    {
        RIL_LOG_CRITICAL("CChannel_Data::CChannel_Data() - Could not create m_pSetupDoneEvent\r\n");
    }

    RIL_LOG_VERBOSE("CChannel_Data::CChannel_Data() - Exit\r\n");
}

CChannel_Data::~CChannel_Data()
{
    RIL_LOG_VERBOSE("CChannel_Data::~CChannel_Data() - Enter\r\n");

    delete []m_prisdModuleInit;
    m_prisdModuleInit = NULL;

    delete m_pSetupDoneEvent;
    m_pSetupDoneEvent = NULL;

    delete[] m_szIpAddr;
    m_szIpAddr = NULL;

    delete[] m_szDNS1;
    m_szDNS1 = NULL;

    delete[] m_szDNS2;
    m_szDNS2 = NULL;

    RIL_LOG_VERBOSE("CChannel_Data::~CChannel_Data() - Exit\r\n");
}

//  Override from base class
BOOL CChannel_Data::OpenPort()
{
    BOOL bRetVal = FALSE;

    RIL_LOG_INFO("CChannel_Data::OpenPort() - Opening COM Port: %s...\r\n", g_szDataPort1);
    RIL_LOG_INFO("CChannel_Data::OpenPort() - g_bIsSocket=[%d]...\r\n", g_bIsSocket);

    // TODO: Instead of using g_szDatatPort1, use channel number to create port name from
    // the base port name + channel# + 1. E.g, data channel 1 uses port /dev/ttyGSM2
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
        RIL_LOG_CRITICAL("CChannel_Data::FinishInit() : ERROR : chnl=[%d] Could not create new INITSTRING_DATA\r\n", m_uiRilChannel);
        goto Error;
    }

    m_prisdModuleInit[COM_BASIC_INIT_INDEX]     = DataBasicInitString;
    m_prisdModuleInit[COM_UNLOCK_INIT_INDEX]    = DataUnlockInitString;
    m_prisdModuleInit[COM_POWER_ON_INIT_INDEX]  = DataPowerOnInitString;
    m_prisdModuleInit[COM_READY_INIT_INDEX]     = DataReadyInitString;

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

    pSilo = CSilo_Factory::GetSiloData(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_Data::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_Data\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloPhonebook(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_Data::AddSilos() : ERROR : chnl=[%d] Could not add CSilo_Phonebook\r\n", m_uiRilChannel);
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
CChannel_Data* CChannel_Data::GetChnlFromContextID(UINT32 uiContextID)
{
    CMutex::Lock(CSystemManager::GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetChnlFromContextID() - Enter\r\n");


    extern CChannel* g_pRilChannel[RIL_CHANNEL_MAX];
    CChannel_Data* pChannelData = NULL;

    for (int i = RIL_CHANNEL_DATA1; i < RIL_CHANNEL_MAX; i++)
    {
        if (NULL == g_pRilChannel[i]) // could be NULL if reserved channel
            continue;

        CChannel_Data* pTemp = static_cast<CChannel_Data*>(g_pRilChannel[i]);
        if (pTemp && pTemp->GetContextID() == uiContextID)
        {
            pChannelData = pTemp;
            break;
        }
    }

Error:
    RIL_LOG_VERBOSE("CChannel_Data::GetChnlFromContextID() - Exit\r\n");

    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());
    return pChannelData;
}

//
//  Returns a free channel to use for data
//
CChannel_Data* CChannel_Data::GetFreeChnl()
{
    CMutex::Lock(CSystemManager::GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetFreeChnl() - Enter\r\n");

    extern CChannel* g_pRilChannel[RIL_CHANNEL_MAX];
    CChannel_Data* pChannelData = NULL;

    for (int i = RIL_CHANNEL_DATA1; i < RIL_CHANNEL_MAX; i++)
    {
        if (NULL == g_pRilChannel[i]) // could be NULL if reserved channel
            continue;

        CChannel_Data* pTemp = static_cast<CChannel_Data*>(g_pRilChannel[i]);
        if (pTemp && pTemp->GetContextID() == 0)
        {
            pChannelData = pTemp;
            break;
        }
    }

Error:
    RIL_LOG_VERBOSE("CChannel_Data::GetFreeChnl() - Exit\r\n");

    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());
    return pChannelData;
}

//
//  Returns the Data Channel for the specified RIL channel number
//
CChannel_Data* CChannel_Data::GetChnlFromRilChannelNumber(UINT32 index)
{
    CMutex::Lock(CSystemManager::GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetChnlFromRilChannelNumber() - Enter\r\n");

    extern CChannel* g_pRilChannel[RIL_CHANNEL_MAX];
    CChannel_Data* pChannelData = NULL;

    for (int i = RIL_CHANNEL_DATA1; i < RIL_CHANNEL_MAX; i++)
    {
        if (NULL == g_pRilChannel[i]) // could be NULL if reserved channel
            continue;

        CChannel_Data* pTemp = static_cast<CChannel_Data*>(g_pRilChannel[i]);
        if (pTemp && pTemp->GetRilChannel() == index)
        {
            pChannelData = pTemp;
            break;
        }
    }

Error:
    RIL_LOG_VERBOSE("CChannel_Data::GetChnlFromRilChannelNumber() - Exit\r\n");

    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());
    return pChannelData;
}

//
//  Returns the next available Context ID
//
UINT32 CChannel_Data::GetNextContextID()
{
    CMutex::Lock(CSystemManager::GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetNextContextID() - Enter\r\n");

    extern CChannel* g_pRilChannel[RIL_CHANNEL_MAX];
    UINT32 uiCID = 1;
    BOOL fAvailable = FALSE;
    BOOL fFound = FALSE;

    while (!fAvailable)
    {
        fFound = FALSE;
        for (int i = RIL_CHANNEL_DATA1; i < RIL_CHANNEL_MAX; i++)
        {
            if (NULL == g_pRilChannel[i]) // could be NULL if reserved channel
                continue;

            CChannel_Data* pTemp = static_cast<CChannel_Data*>(g_pRilChannel[i]);
            if (pTemp && pTemp->GetContextID() == uiCID)
            {
                fFound = TRUE;
                break;
            }
        }
        if (!fFound)
        {
            fAvailable = TRUE;
            break;
        }
        else
            uiCID++;
    }

Error:
    RIL_LOG_VERBOSE("CChannel_Data::GetNextContextID() - Exit\r\n");

    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());

    return uiCID;
}

UINT32 CChannel_Data::GetContextID() const
{
    CMutex::Lock(CSystemManager::GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetContextID() - Enter\r\n");

    UINT32 nCID = m_uiContextID;

    RIL_LOG_VERBOSE("CChannel_Data::GetContextID() - Exit\r\n");

    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());

    return nCID;
}


//
//  Sets this channel's context ID value
//
BOOL CChannel_Data::SetContextID(UINT32 dwContextID)
{
    CMutex::Lock(CSystemManager::GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::SetContextID() - Enter\r\n");

    BOOL fRet = FALSE;

    // If we are setting a value other than 0, make sure it isn't already in use!
    if (dwContextID)
    {
    }

    m_uiContextID = dwContextID;

    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CChannel_Data::SetContextID() - Exit\r\n");

    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());

    return fRet;
}



