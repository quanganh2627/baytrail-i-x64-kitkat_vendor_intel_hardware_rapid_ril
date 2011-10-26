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

extern char* g_szDataPort1;
extern char* g_szDataPort2;
extern char* g_szDataPort3;
extern char* g_szDataPort4;
extern char* g_szDataPort5;
extern BOOL  g_bIsSocket;

//  Init commands for this channel.
//  GPRS/UMTS data (1st primary context)
INITSTRING_DATA DataBasicInitString = { "E0V1Q0X4|+CMEE=1|S0=0" };
INITSTRING_DATA DataUnlockInitString = { "" };
INITSTRING_DATA DataPowerOnInitString = { "" };
INITSTRING_DATA DataReadyInitString = { "" };

CChannel_Data::CChannel_Data(UINT32 uiChannel)
:   CChannel(uiChannel),
    m_szIpAddr(NULL),
    m_szDNS1(NULL),
    m_szDNS2(NULL)
#if defined(M2_IPV6_FEATURE_ENABLED)
    ,
    m_szIpAddr2(NULL),
    m_szDNS1_2(NULL),
    m_szDNS2_2(NULL)
#endif // M2_IPV6_FEATURE_ENABLED
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

#if defined(M2_IPV6_FEATURE_ENABLED)
    delete[] m_szIpAddr2;
    m_szIpAddr2 = NULL;

    delete[] m_szDNS1_2;
    m_szDNS1_2 = NULL;

    delete[] m_szDNS2_2;
    m_szDNS2_2 = NULL;
#endif // M2_IPV6_FEATURE_ENABLED

    RIL_LOG_VERBOSE("CChannel_Data::~CChannel_Data() - Exit\r\n");
}

//  Override from base class
BOOL CChannel_Data::OpenPort()
{
    BOOL bRetVal = FALSE;

    switch(m_uiRilChannel)
    {
        case RIL_CHANNEL_DATA1:
            RIL_LOG_INFO("CChannel_Data::OpenPort() - Opening COM Port: %s...\r\n", g_szDataPort1);
            RIL_LOG_INFO("CChannel_Data::OpenPort() - g_bIsSocket=[%d]...\r\n", g_bIsSocket);
            bRetVal = m_Port.Open(g_szDataPort1, g_bIsSocket);
            break;

#if defined(M2_MULTIPLE_PDP_FEATURE_ENABLED)
        case RIL_CHANNEL_DATA2:
            RIL_LOG_INFO("CChannel_Data::OpenPort() - Opening COM Port: %s...\r\n", g_szDataPort2);
            RIL_LOG_INFO("CChannel_Data::OpenPort() - g_bIsSocket=[%d]...\r\n", g_bIsSocket);
            bRetVal = m_Port.Open(g_szDataPort2, g_bIsSocket);
            break;

        case RIL_CHANNEL_DATA3:
            RIL_LOG_INFO("CChannel_Data::OpenPort() - Opening COM Port: %s...\r\n", g_szDataPort3);
            RIL_LOG_INFO("CChannel_Data::OpenPort() - g_bIsSocket=[%d]...\r\n", g_bIsSocket);
            bRetVal = m_Port.Open(g_szDataPort3, g_bIsSocket);
            break;

        case RIL_CHANNEL_DATA4:
            RIL_LOG_INFO("CChannel_Data::OpenPort() - Opening COM Port: %s...\r\n", g_szDataPort4);
            RIL_LOG_INFO("CChannel_Data::OpenPort() - g_bIsSocket=[%d]...\r\n", g_bIsSocket);
            bRetVal = m_Port.Open(g_szDataPort4, g_bIsSocket);
            break;

        case RIL_CHANNEL_DATA5:
            RIL_LOG_INFO("CChannel_Data::OpenPort() - Opening COM Port: %s...\r\n", g_szDataPort5);
            RIL_LOG_INFO("CChannel_Data::OpenPort() - g_bIsSocket=[%d]...\r\n", g_bIsSocket);
            bRetVal = m_Port.Open(g_szDataPort5, g_bIsSocket);
            break;
#endif // M2_MULTIPLE_PDP_FEATURE_ENABLED

        default:
            RIL_LOG_CRITICAL("CChannel_Data::OpenPort() - ERROR channel does not exist m_uiRilChannel=%d\r\n", m_uiRilChannel);
            bRetVal = FALSE;
            break;
    }



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
//  [out] UINT32 outCID - If successful, the new context ID, else 0.
//  return value - CChannel_Data* - If successful, the free data channel, else NULL.
//  Note: If successful the returned CChannel_Data* will have the new context ID set.
//
//  The scheme is that the context ID will be set to the data channel number.
//  i.e. RIL_CHANNEL_DATA1 = CID of 1
//       RIL_CHANNEL_DATA2 = CID of 2
//       ...
//       RIL_CHANNEL_DATA5 = CID of 5
//  No other context ID
CChannel_Data* CChannel_Data::GetFreeChnl(UINT32& outCID)
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
            //  We found a free data channel.
            pChannelData = pTemp;
            outCID = (i - RIL_CHANNEL_DATA1) + 1;

            RIL_LOG_INFO("CChannel_Data::GetFreeChnl() - ****** Setting chnl=[%d] to CID=[%d] ******\r\n", i, outCID);
            pChannelData->SetContextID(outCID);
            break;
        }
    }

Error:
    if (NULL == pChannelData)
    {
        // Error, all channels full!
        RIL_LOG_CRITICAL("CChannel_Data::GetFreeChnl() - ERROR: All channels full!!\r\n");
        outCID = 0;
    }

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

    m_uiContextID = dwContextID;

    RIL_LOG_VERBOSE("CChannel_Data::SetContextID() - Exit\r\n");

    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());

    return TRUE;
}



