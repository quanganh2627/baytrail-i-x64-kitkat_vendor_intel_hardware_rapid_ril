////////////////////////////////////////////////////////////////////////////
// cmccontext.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Interface for command context used to execute command
//    specific actions
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

#if !defined (__RIL_CMD_CONTEXT__)
#define __RIL_CMD_CONTEXT__

#include "sync_ops.h"
#include "com_init_index.h"
#include "rilchannels.h"
#include "rildmain.h"

// forward declaration
class CCommand;

class CContext
{
public:
    CContext() {}
    virtual ~CContext() {}

    // interface
    virtual void Execute(BOOL, UINT32) = 0;

private:
    // copy constructor and assigment operator disallowed
    CContext(const CContext& rhs);
    const CContext& operator= (const CContext& rhs);
};

class CContextContainer : public CContext
{
public:
    CContextContainer() { m_pFront = m_pBack = NULL;}
    virtual ~CContextContainer();

    virtual void Execute(BOOL, UINT32);
    void Add(CContext*);

private:
    struct ListNode
    {
        CContext*   m_pContext;
        ListNode*   m_pNext;

        ListNode(CContext* pContext, ListNode* pNext = NULL)
          : m_pContext(pContext), m_pNext(pNext) { }
    };

    ListNode* m_pFront;
    ListNode* m_pBack;
};

class CContextPower : public CContext
{
public:
    CContextPower (BOOL bFlag, CCommand& rCmd) :
        m_bPowerOn(bFlag), m_rCmd(rCmd) {}
    virtual ~CContextPower() {}

    virtual void Execute(BOOL, UINT32);

private:
    BOOL          m_bPowerOn;
    CCommand&     m_rCmd;
};

class CContextEvent : public CContext
{
public:
    CContextEvent(CEvent& pEvent) : m_pEvent(pEvent) {}
    virtual ~CContextEvent() {}

    virtual void Execute(BOOL, UINT32);

private:
    CEvent&     m_pEvent;
};

class CContextUnlock : public CContext
{
public:
    CContextUnlock() {}
    virtual ~CContextUnlock() {}

    virtual void Execute(BOOL, UINT32);
};

class CContextInitString : public CContext
{
public:
    CContextInitString(eComInitIndex eInitIndex, UINT32 uiChannel, BOOL bFinalCmd)
        : m_eInitIndex(eInitIndex), m_uiChannel(uiChannel), m_bFinalCmd(bFinalCmd) {}
    virtual ~CContextInitString() {}

    virtual void Execute(BOOL, UINT32);

private:
    eComInitIndex m_eInitIndex;
    UINT32 m_uiChannel;
    BOOL m_bFinalCmd;
};

class CContextNetworkType : public CContext
{
public:
    CContextNetworkType() {}
    virtual ~CContextNetworkType() {}

    virtual void Execute(BOOL, UINT32);
};

class CContextPinQuery : public CContext
{
public:
    CContextPinQuery() {}
    virtual ~CContextPinQuery() {}

    virtual void Execute(BOOL, UINT32);
};

class CContextSimPhonebookQuery : public CContext
{
public:
    CContextSimPhonebookQuery(CEvent& pEvent, BOOL& rbResult) :
        m_pEvent(pEvent),
        m_rbResult(rbResult) {}
    virtual ~CContextSimPhonebookQuery() {}

    virtual void Execute(BOOL, UINT32);

private:
    CEvent&     m_pEvent;
    BOOL&       m_rbResult;
};

#endif  // __RIL_CMD_CONTEXT__
