////////////////////////////////////////////////////////////////////////////
// cmccontext.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Implementation specific contexts
//
// Author:  Francesc Vilarino
// Created: 2009-11-19
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Nov 19/09  FV       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

// NOTE: the Execute function runs in the context of the RX thread - the
//       Execute function should NOT run any potentially blocking code or
//       any long-running operations.  Ideally the Execute function should
//       just set a flag or trigger an event.

#include "types.h"
#include "rril.h"
#include "sync_ops.h"
#include "thread_ops.h"
#include "silo.h"
#include "channelbase.h"
#include "channel_nd.h"
#include "cmdcontext.h"
#include "util.h"

// CContextContainer
CContextContainer::~CContextContainer()
{
    while (m_pFront != NULL)
    {
        ListNode* pTmp = m_pFront;
        m_pFront = m_pFront->m_pNext;
        delete pTmp->m_pContext;
        pTmp->m_pContext = NULL;
        delete pTmp;
        pTmp = NULL;
    }
}

void CContextContainer::Execute(BOOL bRes, UINT32 uiErrorCode)
{
    for (ListNode* pTmp = m_pFront;
         pTmp != NULL;
         pTmp->m_pContext->Execute(bRes, uiErrorCode), pTmp = pTmp->m_pNext);
}

void CContextContainer::Add(CContext* pContext)
{
    if (NULL == m_pFront)
        m_pBack = m_pFront = new ListNode(pContext);
    else
        m_pBack = m_pBack->m_pNext = new ListNode(pContext);
}

// CContextPower
void CContextPower::Execute(BOOL bRes, UINT32 uiErrorCode)
{
    RIL_LOG_VERBOSE("CContextPower::Execute() - Enter");

    // update the global on/off state.
    if (bRes)
    {
        if (m_bPowerOn)
        {
            if (g_RadioState.IsRadioOff())
            {
                g_RadioState.SetRadioOn();
                CSystemManager::GetInstance().TriggerModemPowerOnEvent();
                CSystemManager::GetInstance().ResumeSystemFromFlightMode();
                RIL_LOG_INFO("CContextPower::Execute() - Radio turned on.");
            }
        }
        else
        {
            RIL_LOG_INFO("CContextPower::Execute() - Radio turned off. Call StopSimInitialization()");
            g_RadioState.SetRadioOff();
            CSystemManager::GetInstance().StopSimInitialization();
        }
    }

    RIL_LOG_VERBOSE("CContextPower::Execute() - Exit");
}


// CContextEvent
void CContextEvent::Execute(BOOL bRes, UINT32 uiErrorCode)
{
    if (bRes)
    {
        RIL_LOG_VERBOSE("CContextEvent::Execute() - Signalling event!\r\n");
        CEvent::Signal(&m_pEvent);
    }
}

// CContextUnlock
void CContextUnlock::Execute(BOOL bRes, UINT32 uiErrorCode)
{
    if (bRes)
    {
        // phone successfully unlocked
        RIL_LOG_VERBOSE("CContextUnlock::Execute() - Phone is unlocking... (2 seconds)\r\n");
        Sleep(2000);
        RIL_LOG_INFO("CContextUnlock::Execute() - The phone is now unlocked\r\n");
        g_RadioState.SetRadioSIMUnlocked();
        CSystemManager::GetInstance().TriggerSimUnlockedEvent();
    }
}

// CContextInitString
void CContextInitString::Execute(BOOL bRes, UINT32 uiErrorCode)
{
    if (m_bFinalCmd)
    {
        RIL_LOG_INFO("CContextInitString::Execute() - Last command for init index [%d] on channel [%d] had result [%s]\r\n", 
            m_eInitIndex, 
            m_uiChannel,
            bRes ? "OK" : "FAIL");

        if (!bRes)
        {
            RIL_LOG_CRITICAL("CContextInitString::Execute() - ERROR: Shutting Down!\r\n");
            CSystemManager::GetInstance().SetInitializationUnsuccessful();
            TriggerRadioErrorAsync(eRadioError_InitFailure, __LINE__, __FILE__);
        }
        else
        {
            CSystemManager::GetInstance().TriggerInitStringCompleteEvent(m_uiChannel, m_eInitIndex);
        }
    }
}

// CContextNetworkType
void CContextNetworkType::Execute(BOOL bRes, UINT32 uiErrorCode)
{
    RIL_LOG_VERBOSE("CContextNetworkType::Execute() - Enter\r\n");

    g_RadioState.EnablePowerStateChange();
    if (bRes)
    {
        // We need to set the radio off first so the SIM status flags are reset
        g_RadioState.SetRadioOff();
        g_RadioState.SetRadioOn();
        CSystemManager::GetInstance().ResumeSystemFromFlightMode();
    }

    RIL_LOG_VERBOSE("CContextNetworkType::Execute() - Exit\r\n");
}

// CContextSimStatus
void CContextPinQuery::Execute(BOOL bRes, UINT32 uiErrorCode)
{
    RIL_LOG_VERBOSE("CContextPinQuery::Execute() - Enter\r\n");

    if (RRIL_CME_ERROR_SIM_ABSENT == uiErrorCode)
    {
        RIL_LOG_INFO("CContextPinQuery::Execute() : RRIL_CME_ERROR_SIM_ABSENT\r\n");
        g_RadioState.SetRadioSIMAbsent();
        CSystemManager::GetInstance().StopSimInitialization();
    }

    RIL_LOG_VERBOSE("CContextPinQuery::Execute() - Exit\r\n");
}

// CContextSimPhonebookQuery
void CContextSimPhonebookQuery::Execute(BOOL bRes, UINT32 uiErrorCode)
{
    RIL_LOG_VERBOSE("CContextSimPhonebookQuery::Execute() - Enter\r\n");
    
    RIL_LOG_INFO("CContextSimPhonebookQuery::Execute() - Signalling event and setting result to [%s]!\r\n", bRes ? "TRUE" : "FALSE");
    m_rbResult = bRes;
    CEvent::Signal(&m_pEvent);
    
    RIL_LOG_VERBOSE("CContextSimPhonebookQuery::Execute() - Exit\r\n");
}
