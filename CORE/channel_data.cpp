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
#include "rril.h"
#include "te.h"

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

// used for 7x60 modems only
extern int m_hsiChannelsReservedForClass1;
extern int m_hsiDataDirect;
extern int m_dataProfilePathAssignation[NUMBER_OF_APN_PROFILE];

// used by 7x60 modems only
UINT32 g_uiHSIChannel[RIL_HSI_CHANNEL_MAX] = {0};


CChannel_Data::CChannel_Data(UINT32 uiChannel)
:   CChannel(uiChannel),
    m_dataFailCause(PDP_FAIL_NONE),
    m_uiContextID(0),
    m_dataState(E_DATA_STATE_IDLE)
{
    RIL_LOG_VERBOSE("CChannel_Data::CChannel_Data() - Enter\r\n");

    m_szPdpType[0] = '\0';
    m_szInterfaceName[0] = '\0';
    m_szIpAddr[0] = '\0';
    m_szIpAddr2[0] = '\0';
    m_szDNS1[0] = '\0';
    m_szDNS2[0] = '\0';
    m_szIpV6DNS1[0] = '\0';
    m_szIpV6DNS2[0] = '\0';
    m_szIpGateways[0] = '\0';

    RIL_LOG_VERBOSE("CChannel_Data::CChannel_Data() - Exit\r\n");
}

CChannel_Data::~CChannel_Data()
{
    RIL_LOG_VERBOSE("CChannel_Data::~CChannel_Data() - Enter\r\n");

    delete []m_prisdModuleInit;
    m_prisdModuleInit = NULL;

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
    RIL_LOG_VERBOSE("CChannel_Data::GetChnlFromContextID() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetDataChannelAccessorMutex());

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
    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetChnlFromContextID() - Exit\r\n");
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
CChannel_Data* CChannel_Data::GetFreeChnlsRilHsi(UINT32& outCID, int dataProfile)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetFreeChnlsRilHsi() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetDataChannelAccessorMutex());

    extern CChannel* g_pRilChannel[RIL_CHANNEL_MAX];

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
                    hsiChannel = GetFreeHSIChannel(outCID, RIL_HSI_CHANNEL1 + m_hsiChannelsReservedForClass1, RIL_HSI_CHANNEL1 + m_hsiDataDirect);
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

    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetFreeChnlsRilHsi() - Exit\r\n");
    return pChannelData;
}

int CChannel_Data::GetFreeHSIChannel(UINT32 uiCID, int sIndex, int eIndex)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetFreeHSIChannel() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetDataChannelAccessorMutex());

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

    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetFreeHSIChannel() - Not Success - Exit\r\n");
    return -1;
}

bool CChannel_Data::FreeHSIChannel(UINT32 uiCID)
{
    RIL_LOG_VERBOSE("CChannel_Data::FreeHSIChannel() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetDataChannelAccessorMutex());

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

    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::FreeHSIChannel() - Exit\r\n");
    return false;
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
    RIL_LOG_VERBOSE("CChannel_Data::GetFreeChnl() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetDataChannelAccessorMutex());

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

    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetFreeChnl() - Exit\r\n");
    return pChannelData;
}

//
//  Returns the Data Channel for the specified RIL channel number
//
CChannel_Data* CChannel_Data::GetChnlFromRilChannelNumber(UINT32 index)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetChnlFromRilChannelNumber() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetDataChannelAccessorMutex());

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
    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetChnlFromRilChannelNumber() - Exit\r\n");
    return pChannelData;
}



UINT32 CChannel_Data::GetContextID() const
{
    RIL_LOG_VERBOSE("CChannel_Data::GetContextID() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetDataChannelAccessorMutex());

    UINT32 nCID = m_uiContextID;

    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetContextID() - Exit\r\n");
    return nCID;
}


//
//  Sets this channel's context ID value
//
BOOL CChannel_Data::SetContextID(UINT32 dwContextID)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetContextID() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetDataChannelAccessorMutex());

    m_uiContextID = dwContextID;

    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::SetContextID() - Exit\r\n");
    return TRUE;
}

void CChannel_Data::ResetDataCallInfo()
{
    RIL_LOG_VERBOSE("CChannel_Data::ResetDataCallInfo() - Enter\r\n");

    SetDataFailCause(PDP_FAIL_NONE);
    SetDataState(E_DATA_STATE_IDLE);

    if (MODEM_TYPE_IFX7060 == CTE::GetTE().GetModemType())
    {
        FreeHSIChannel(m_uiContextID);
    }

    SetContextID(0);
    RIL_LOG_VERBOSE("CChannel_Data::ResetDataCallInfo() - Exit\r\n");
}

void CChannel_Data::SetDataFailCause(int cause)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetDataFailCause() - Enter\r\n");

    m_dataFailCause = cause;

    RIL_LOG_VERBOSE("CChannel_Data::SetDataFailCause() - Exit\r\n");
}

int CChannel_Data::GetDataFailCause()
{
    RIL_LOG_VERBOSE("CChannel_Data::GetDataFailCause() - Enter/Exit\r\n");
    return m_dataFailCause;
}

void CChannel_Data::SetPdpType(const char* pPdpType)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetPdpType() - Enter\r\n");

    strncpy(m_szPdpType, pPdpType, MAX_PDP_TYPE_SIZE-1);
    m_szPdpType[MAX_PDP_TYPE_SIZE-1] = '\0';

    RIL_LOG_VERBOSE("CChannel_Data::SetPdpType() - Exit\r\n");
}

void CChannel_Data::GetPdpType(char* pPdpType, const int maxSize)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetPdpType() - Enter\r\n");

    if (NULL != pPdpType && 0 < maxSize)
    {
        strncpy(pPdpType, m_szPdpType, maxSize-1);
        pPdpType[maxSize-1] = '\0';
    }

    RIL_LOG_VERBOSE("CChannel_Data::GetPdpType() - Exit\r\n");
}

void CChannel_Data::SetInterfaceName(const char* pInterfaceName)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetInterfaceName() - Enter\r\n");

    strncpy(m_szInterfaceName, pInterfaceName, MAX_INTERFACE_NAME_SIZE-1);
    m_szInterfaceName[MAX_INTERFACE_NAME_SIZE-1] = '\0';

    RIL_LOG_VERBOSE("CChannel_Data::SetInterfaceName() - Exit\r\n");
}

void CChannel_Data::GetInterfaceName(char* pInterfaceName, const int maxSize)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetInterfaceName() - Enter\r\n");

    if (NULL != pInterfaceName && 0 < maxSize)
    {
        strncpy(pInterfaceName, m_szInterfaceName, maxSize-1);
        pInterfaceName[maxSize-1] = '\0';
    }

    RIL_LOG_VERBOSE("CChannel_Data::GetInterfaceName() - Exit\r\n");
}

void CChannel_Data::SetIpAddress(const char* pIpAddr1,
                                        const char* pIpAddr2)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetIpAddress() - Enter\r\n");

    strncpy(m_szIpAddr, pIpAddr1, MAX_IPADDR_SIZE-1);
    m_szIpAddr[MAX_IPADDR_SIZE-1] = '\0';

    strncpy(m_szIpAddr2, pIpAddr2, MAX_IPADDR_SIZE-1);
    m_szIpAddr2[MAX_IPADDR_SIZE-1] = '\0';

    RIL_LOG_VERBOSE("CChannel_Data::SetIpAddress() - Exit\r\n");
}

void CChannel_Data::GetIpAddress(char* pIpAddr, const int maxIpAddrSize,
                                char* pIpAddr2, const int maxIpAddr2Size)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetIpAddress() - Enter\r\n");

    if (NULL != pIpAddr && 0 < maxIpAddrSize)
    {
        strncpy(pIpAddr, m_szIpAddr, maxIpAddrSize-1);
        pIpAddr[maxIpAddrSize-1] = '\0';
    }

    if (NULL != pIpAddr2 && 0 < maxIpAddr2Size)
    {
        strncpy(pIpAddr2, m_szIpAddr2, maxIpAddr2Size-1);
        pIpAddr2[maxIpAddr2Size-1] = '\0';
    }

    RIL_LOG_VERBOSE("CChannel_Data::GetIpAddress() - Exit\r\n");
}

void CChannel_Data::SetDNS(const char* pDNS1, const char* pDNS2,
                                const char* pIpV6DNS1, const char* pIpV6DNS2)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetDNS() - Enter\r\n");

    strncpy(m_szDNS1, pDNS1, MAX_IPADDR_SIZE-1);
    m_szDNS1[MAX_IPADDR_SIZE-1] = '\0';

    strncpy(m_szDNS2, pDNS2, MAX_IPADDR_SIZE-1);
    m_szDNS2[MAX_IPADDR_SIZE-1] = '\0';

    strncpy(m_szIpV6DNS1, pIpV6DNS1, MAX_IPADDR_SIZE-1);
    m_szIpV6DNS1[MAX_IPADDR_SIZE-1] = '\0';

    strncpy(m_szIpV6DNS2, pIpV6DNS2, MAX_IPADDR_SIZE-1);
    m_szIpV6DNS2[MAX_IPADDR_SIZE-1] = '\0';

    RIL_LOG_VERBOSE("CChannel_Data::SetDNS() - Exit\r\n");
}

void CChannel_Data::GetDNS(char* pDNS1, const int maxDNS1Size,
                                char* pDNS2, const int maxDNS2Size,
                                char* pIpV6DNS1, const int maxIpV6DNS1Size,
                                char* pIpV6DNS2, const int maxIpV6DNS2Size)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetDNS() - Enter\r\n");

    if (NULL != pDNS1 && 0 < maxDNS1Size)
    {
        strncpy(pDNS1, m_szDNS1, maxDNS1Size-1);
        pDNS1[maxDNS1Size-1] = '\0';
    }

    if (NULL != pDNS2 && 0 < maxDNS2Size)
    {
        strncpy(pDNS2, m_szDNS2, maxDNS2Size-1);
        pDNS2[maxDNS2Size-1] = '\0';
    }

    if (NULL != pIpV6DNS1 && 0 < maxIpV6DNS1Size)
    {
        strncpy(pIpV6DNS1, m_szIpV6DNS1, maxIpV6DNS1Size-1);
        pIpV6DNS1[maxIpV6DNS1Size-1] = '\0';
    }

    if (NULL != pIpV6DNS2 && 0 < maxIpV6DNS2Size)
    {
        strncpy(pIpV6DNS2, m_szIpV6DNS2, maxIpV6DNS2Size-1);
        pIpV6DNS2[maxIpV6DNS2Size-1] = '\0';
    }

    RIL_LOG_VERBOSE("CChannel_Data::GetDNS() - Exit\r\n");
}

void CChannel_Data::SetGateway(const char* pIpGateways)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetGateway() - Enter\r\n");

    strncpy(m_szIpGateways, pIpGateways, MAX_IPADDR_SIZE-1);
    m_szIpGateways[MAX_IPADDR_SIZE-1] = '\0';

    RIL_LOG_VERBOSE("CChannel_Data::SetGateway() - Exit\r\n");
}

void CChannel_Data::GetGateway(char* pIpGateways, const int maxSize)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetGateway() - Enter\r\n");

    if (NULL != pIpGateways && 0 < maxSize)
    {
        strncpy(pIpGateways, m_szIpGateways, maxSize-1);
        pIpGateways[maxSize-1] = '\0';
    }

    RIL_LOG_VERBOSE("CChannel_Data::GetGateway() - Exit\r\n");
}

void CChannel_Data::SetDataState(int state)
{
    RIL_LOG_VERBOSE("CChannel_Data::SetDataState() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetDataChannelAccessorMutex());

    m_dataState = state;

    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::SetDataState() - Exit\r\n");
}

int CChannel_Data::GetDataState()
{
    RIL_LOG_VERBOSE("CChannel_Data::GetDataState() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetDataChannelAccessorMutex());

    int state = m_dataState;

    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetDataState() - Exit\r\n");
    return state;
}

void CChannel_Data::GetDataCallInfo(S_DATA_CALL_INFO& rDataCallInfo)
{
    RIL_LOG_VERBOSE("CChannel_Data::GetDataCallInfo() - Enter\r\n");

    CMutex::Lock(CSystemManager::GetDataChannelAccessorMutex());

    rDataCallInfo.failCause = m_dataFailCause;
    rDataCallInfo.uiCID = m_uiContextID;
    rDataCallInfo.state = m_dataState;

    strncpy(rDataCallInfo.szPdpType, m_szPdpType, MAX_PDP_TYPE_SIZE-1);
    rDataCallInfo.szPdpType[MAX_PDP_TYPE_SIZE-1] = '\0';

    strncpy(rDataCallInfo.szInterfaceName, m_szInterfaceName,
                                        MAX_INTERFACE_NAME_SIZE-1);
    rDataCallInfo.szInterfaceName[MAX_INTERFACE_NAME_SIZE-1] = '\0';

    strncpy(rDataCallInfo.szIpAddr1, m_szIpAddr, MAX_IPADDR_SIZE-1);
    rDataCallInfo.szIpAddr1[MAX_IPADDR_SIZE-1] = '\0';

    strncpy(rDataCallInfo.szIpAddr2, m_szIpAddr2, MAX_IPADDR_SIZE-1);
    rDataCallInfo.szIpAddr2[MAX_IPADDR_SIZE-1] = '\0';

    strncpy(rDataCallInfo.szDNS1, m_szDNS1, MAX_IPADDR_SIZE-1);
    rDataCallInfo.szDNS1[MAX_IPADDR_SIZE-1] = '\0';

    strncpy(rDataCallInfo.szDNS2, m_szDNS2, MAX_IPADDR_SIZE-1);
    rDataCallInfo.szDNS2[MAX_IPADDR_SIZE-1] = '\0';

    strncpy(rDataCallInfo.szIpV6DNS1, m_szIpV6DNS1, MAX_IPADDR_SIZE-1);
    rDataCallInfo.szIpV6DNS1[MAX_IPADDR_SIZE-1] = '\0';

    strncpy(rDataCallInfo.szIpV6DNS2, m_szIpV6DNS2, MAX_IPADDR_SIZE-1);
    rDataCallInfo.szIpV6DNS2[MAX_IPADDR_SIZE-1] = '\0';

    strncpy(rDataCallInfo.szGateways, m_szIpGateways, MAX_IPADDR_SIZE-1);
    rDataCallInfo.szGateways[MAX_IPADDR_SIZE-1] = '\0';

    CMutex::Unlock(CSystemManager::GetDataChannelAccessorMutex());

    RIL_LOG_VERBOSE("CChannel_Data::GetDataCallInfo() - Exit\r\n");
}
