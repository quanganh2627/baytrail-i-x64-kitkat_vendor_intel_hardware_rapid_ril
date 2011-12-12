////////////////////////////////////////////////////////////////////////////
// cmdcontext.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Implementation specific contexts
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
#include "reset.h"

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


// CContextEvent
void CContextEvent::Execute(BOOL bRes, UINT32 uiErrorCode)
{
    if (bRes)
    {
        RIL_LOG_VERBOSE("CContextEvent::Execute() - Signalling event!\r\n");
        CEvent::Signal(&m_pEvent);
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
            do_request_clean_up(eRadioError_InitFailure, __LINE__, __FILE__);
        }
        else
        {
            CSystemManager::GetInstance().TriggerInitStringCompleteEvent(m_uiChannel, m_eInitIndex);
        }
    }
}

