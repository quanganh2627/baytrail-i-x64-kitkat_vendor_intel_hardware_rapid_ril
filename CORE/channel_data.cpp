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

#if defined(BOARD_HAVE_IFX7060)
extern int m_hsiChannelsReservedForClass1;
extern int m_hsiChannelsReservedForDataDirectlyoverHsi;
extern int m_dataProfilePathAssignation[NUMBER_OF_APN_PROFILE];
#endif

CChannel_Data::CChannel_Data(UINT32 uiChannel)
:   CChannel(uiChannel),
    m_szIpAddr(NULL),
    m_szDNS1(NULL),
    m_szDNS2(NULL),
    m_szIpAddr2(NULL),
    m_szIpV6DNS1(NULL),
    m_szIpV6DNS2(NULL),
    m_szIpGateways(NULL),
    m_szPdpType(NULL),
    m_szInterfaceName(NULL)
{
    RIL_LOG_VERBOSE("CChannel_Data::CChannel_Data() - Enter\r\n");

    m_uiContextID = 0;
    m_iStatus = 0;

    m_pSetupIntermediateEvent = new CEvent();
    if (!m_pSetupIntermediateEvent)
    {
        RIL_LOG_CRITICAL("CChannel_Data::CChannel_Data() - Could not create m_pSetupIntermediateEvent\r\n");
    }

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

    delete m_pSetupIntermediateEvent;
    m_pSetupIntermediateEvent = NULL;

    delete m_pSetupDoneEvent;
    m_pSetupDoneEvent = NULL;

    delete[] m_szIpAddr;
    m_szIpAddr = NULL;

    delete[] m_szDNS1;
    m_szDNS1 = NULL;

    delete[] m_szDNS2;
    m_szDNS2 = NULL;

    delete[] m_szIpGateways;
    m_szIpGateways = NULL;

    delete[] m_szPdpType;
    m_szPdpType = NULL;

    delete[] m_szIpAddr2;
    m_szIpAddr2 = NULL;

    delete[] m_szIpV6DNS1;
    m_szIpV6DNS1 = NULL;

    delete[] m_szIpV6DNS2;
    m_szIpV6DNS2 = NULL;

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

        default:
            RIL_LOG_CRITICAL("CChannel_Data::OpenPort() - channel does not exist m_uiRilChannel=%d\r\n", m_uiRilChannel);
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
        RIL_LOG_CRITICAL("CChannel_Data::FinishInit() : chnl=[%d] Could not create new INITSTRING_DATA\r\n", m_uiRilChannel);
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
        RIL_LOG_CRITICAL("CChannel_Data::AddSilos() : chnl=[%d] Could not add CSilo_Data\r\n", m_uiRilChannel);
        goto Error;
    }

    pSilo = CSilo_Factory::GetSiloPhonebook(this);
    if (!pSilo || !AddSilo(pSilo))
    {
        RIL_LOG_CRITICAL("CChannel_Data::AddSilos() : chnl=[%d] Could not add CSilo_Phonebook\r\n", m_uiRilChannel);
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

    for (UINT32 i = RIL_CHANNEL_DATA1; i < g_uiRilChannelCurMax && i < RIL_CHANNEL_MAX; i++)
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

//  Used for 7x60 modems.
//  Returns a free RIL channel and a free hsi channel to use for data.
//  [out] UINT32 outCID - If successful, the new context ID, else 0.
//  return value - CChannel_Data* - If successful, the free data channel, else NULL.
//  Note: If successful the returned CChannel_Data* will have the new context ID set.
//        m_hsiDirect will be set to TRUE if the data channel use directly a hsi channel.
//        m_hsiChannel will contains the free hsi channel to use.
//  The scheme is that the context ID will be set to the data channel number.
//  i.e. RIL_CHANNEL_DATA1 = CID of 1
//       RIL_CHANNEL_DATA2 = CID of 2
//       ...
//       RIL_CHANNEL_DATA5 = CID of 5
//  No other context ID
#if defined(BOARD_HAVE_IFX7060)
CChannel_Data* CChannel_Data::GetFreeChnlsRilHsi(UINT32& outCID, int dataProfile)
{
    CMutex::Lock(CSystemManager::GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetFreeChnlsRilHsi() - Enter\r\n");

    extern CChannel* g_pRilChannel[RIL_CHANNEL_MAX];
    extern UINT32 g_uiHSIChannel[RIL_HSI_CHANNEL_MAX];
    CChannel_Data* pChannelData = NULL;
    int hsiChannel = -1;
    int hsiDirect = FALSE;

    //First try to get a free RIL channel, then for class 1 or class 2 apn, try to get a hsi channel.
    pChannelData = CChannel_Data::GetFreeChnl(outCID);
    if (pChannelData != NULL)
    {
        if (dataProfile >= 0 && dataProfile < NUMBER_OF_APN_PROFILE)
        {
            // According to the data profile class try to associate a hsi channel to the RIL channel.
            switch (m_dataProfilePathAssignation[dataProfile])
            {
                case 1:
                    // For APN of the class 1 a hsi channel is available. Find the first available.
                    RIL_LOG_INFO("CChannel_Data::GetFreeChnlsRilHsi() - data profile class: %d.\r\n", m_dataProfilePathAssignation[dataProfile]);
                    hsiChannel = GetFreeHSIChannel(outCID, RIL_HSI_CHANNEL1, RIL_HSI_CHANNEL1 + m_hsiChannelsReservedForClass1);
                    if (hsiChannel == -1)
                    {
                        RIL_LOG_INFO("CChannel_Data::GetFreeChnlsRilHsi() - No hsi channel for a data profile class 1.\r\n");
                        pChannelData->SetContextID(0);
                        pChannelData = NULL;
                        goto Error;
                    }
                    hsiDirect = TRUE;
                    break;

                case 2:
                    // For APN of the class 2, check if there is a free hsi channel that can be used.
                    RIL_LOG_INFO("CChannel_Data::GetFreeChnlsRilHsi() - data profile class: %d.\r\n", m_dataProfilePathAssignation[dataProfile]);
                    hsiChannel = GetFreeHSIChannel(outCID, RIL_HSI_CHANNEL1 + m_hsiChannelsReservedForClass1, RIL_HSI_CHANNEL1 + m_hsiChannelsReservedForDataDirectlyoverHsi);
                    if (hsiChannel != -1)
                    {
                        hsiDirect = TRUE;
                    }

                    break;

                default:
                    break;

            }
        }
        pChannelData->m_hsiChannel = hsiChannel;
        pChannelData->m_hsiDirect = hsiDirect;
        pChannelData->m_dataProfile = dataProfile;
    }

Error:
    if (NULL == pChannelData)
    {
        // Error, all channels full!
        RIL_LOG_CRITICAL("CChannel_Data::GetFreeChnlsRilHsi() - All channels full!!\r\n");
        outCID = 0;
    }

    RIL_LOG_VERBOSE("CChannel_Data::GetFreeChnlsRilHsi() - Exit\r\n");

    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());
    return pChannelData;
}

int CChannel_Data::GetFreeHSIChannel(UINT32 uiCID, int sIndex, int eIndex)
{
    CMutex::Lock(CSystemManager::GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetFreeHSIChannel() - Enter\r\n");

    extern UINT32 g_uiHSIChannel[RIL_HSI_CHANNEL_MAX];

    if (sIndex < 0 || eIndex > RIL_HSI_CHANNEL_MAX)
    {
        RIL_LOG_VERBOSE("CChannel_Data::GetFreeHSIChannel() - Index error\r\n");
        return -1;
    }

    for (int i = sIndex; i < eIndex; i++)
    {
        if (g_uiHSIChannel[i] == 0)
        {
            g_uiHSIChannel[i] = uiCID ;
            RIL_LOG_VERBOSE("CChannel_Data::GetFreeHSIChannel() - Success - Exit\r\n");
            CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());
            return i;
        }
    }

    RIL_LOG_VERBOSE("CChannel_Data::GetFreeHSIChannel() - Not Success - Exit\r\n");
    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());
    return -1;
}

bool CChannel_Data::FreeHSIChannel(UINT32 uiCID)
{
    CMutex::Lock(CSystemManager::GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::FreeHSIChannel() - Enter\r\n");

    extern UINT32 g_uiHSIChannel[RIL_HSI_CHANNEL_MAX];

    for (int i = 0; i < RIL_HSI_CHANNEL_MAX; i++)
    {
        if (g_uiHSIChannel[i] == uiCID)
        {
            g_uiHSIChannel[i] = 0 ;
            RIL_LOG_VERBOSE("CChannel_Data::FreeHSIChannel() - Exit\r\n");
            CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());
            return true;
        }
    }

    RIL_LOG_VERBOSE("CChannel_Data::FreeHSIChannel() - Exit\r\n");
    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());
    return false;
}
#endif

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

    for (UINT32 i = RIL_CHANNEL_DATA1; i < g_uiRilChannelCurMax && i < RIL_CHANNEL_MAX; i++)
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
        RIL_LOG_CRITICAL("CChannel_Data::GetFreeChnl() - All channels full!!\r\n");
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

    for (UINT32 i = RIL_CHANNEL_DATA1; i < g_uiRilChannelCurMax && i < RIL_CHANNEL_MAX; i++)
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

//
//  Free's the context Id by setting it to 0
//
void CChannel_Data::FreeContextID()
{
#if defined(BOARD_HAVE_IFX7060)
    FreeHSIChannel(m_uiContextID);
#endif
    SetContextID(0);
}
