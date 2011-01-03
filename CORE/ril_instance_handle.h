////////////////////////////////////////////////////////////////////////////
// CRilInstanceHandle.h
//
// Copyright 2005-2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the CRilInstanceHandle class, which manages RIL interaction
//    with external applications.
//
// Author:  Dennis Peter
// Created: 2007-07-12
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  June  3/08  DP       1.00  Established v1.00 based on current code base.
//  May   8/09  FP       1.xx  Factored out from rilhand.h.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_RILGSM_RILINSTANCEHANDLE_H
#define RRIL_RILGSM_RILINSTANCEHANDLE_H

#ifndef __linux__
#include "util.h"

class CRilHandle;


///////////////////////////////////////////////////////////////////////////////
typedef CUnboundedQueue<RILDRVNOTIFICATION> CRilNotificationQueue;

///////////////////////////////////////////////////////////////////////////////
class CRilInstanceHandle : public CListElem
{
public:
                        CRilInstanceHandle();
                        ~CRilInstanceHandle();

    BOOL                Init(CRilHandle* const pDevice);
    void                Notify(const UINT32 dwCode, const HRESULT hrCmdID, const void* const pBlob, const UINT32 cbBlob);
    BOOL                GetNextNotification(RILDRVNOTIFICATION& rrdnNotifyData);
    BOOL                CreateNotificationQueue(const BYTE* tszEventName, PBYTE pBufOut, UINT32 dwLenOut, PDWORD pdwActualOut);
    BOOL                FReadyForNotifications() const  { RIL_LOG_INFO("FReadyForNotifications() - RIL Instance handle is %s and Notification Queue is %s.\r\n", (m_fInited ? "READY" : "NOT READY"), (m_notificationQ.Ready() ? "READY" : "NOT READY"));  return ((!!m_fInited) && m_notificationQ.Ready()); };
    CRilHandle*         GetDevice() const               { DEBUGCHK(FALSE != !!m_fInited); return m_pDevice; };
    CDblList<CCommand>* GetCmdList() const              { DEBUGCHK(FALSE != !!m_fInited); return m_pCmdList; };
    void                CancelNotifications() const     { CEvent::Signal(m_pNotifyCancelEvent); }
    CRilHandle*         GetCrilHandle (void)            { return m_pDevice; }
    BOOL                MakePreferred();
    BOOL                FPreferred() const              { return !!m_fPreferred; };

private:
    CRilHandle *                    m_pDevice;
    CDblList<CCommand>*             m_pCmdList;
    CEvent *                         m_pNotifyCancelEvent;
    CRilNotificationQueue           m_notificationQ;
    UINT32                           m_fInited : 1;
    UINT32                           m_fPreferred : 1;
};

#endif

#endif //RRIL_RILGSM_RILINSTANCEHANDLE_H

