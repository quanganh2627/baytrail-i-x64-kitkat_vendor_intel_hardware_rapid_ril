#ifndef __linux__


#include "precomp.h"
#include "ril_instance_handle.h"


namespace
{
///////////////////////////////////////////////////////////////////////////////
// Used for cleaning up notification items
// when the notification queue is destroyed.
//
void NotificationItemDtor(void *pItem)
{
    LocalFree(pItem);
}

///////////////////////////////////////////////////////////////////////////////
// Function passed to CQueue::ConditionalGet() below
//
BOOL FEnoughStorage(void* pItem, UINT32 uiData)
{
    DEBUGCHK(NULL != pItem);

    RILDRVNOTIFICATION* prdnNextNotification = (RILDRVNOTIFICATION*)pItem;
    RILDRVNOTIFICATION* prdnSuppliedStruct   = (RILDRVNOTIFICATION*)uiData;

    prdnSuppliedStruct->cbSizeNeeded = prdnNextNotification->cbSizeNeeded;
    return (prdnSuppliedStruct->cbSize >= prdnSuppliedStruct->cbSizeNeeded);
}


}
///////////////////////////////////////////////////////////////////////////////
CRilInstanceHandle::CRilInstanceHandle()
  : CListElem(),
    m_pDevice(NULL),
    m_pCmdList(NULL),
    m_pNotifyCancelEvent(NULL),
    m_notificationQ(NotificationItemDtor),
    m_fInited(FALSE),
    m_fPreferred(FALSE)
{
    RIL_LOG_VERBOSE("CRilInstanceHandle::CRilInstanceHandle() - Enter / Exit \r\n");
}

///////////////////////////////////////////////////////////////////////////////
CRilInstanceHandle::~CRilInstanceHandle()
{
    RIL_LOG_VERBOSE("CRilInstanceHandle::~CRilInstanceHandle() - Enter\r\n");
    if (m_fPreferred)
    {
        // Switch RIL out of emergency mode
        (void)m_pDevice->SetEmergencyMode(FALSE);

        // This handle is not preferred anymore
        m_fPreferred = FALSE;
    }

    CancelNotifications();

    delete m_pCmdList;
    m_pCmdList = NULL;

    if (m_pNotifyCancelEvent)
    {
        delete m_pNotifyCancelEvent;
        m_pNotifyCancelEvent = NULL;
    }
    m_fInited = FALSE;

    RIL_LOG_VERBOSE("CRilInstanceHandle::~CRilInstanceHandle() - Exit\r\n");
}

///////////////////////////////////////////////////////////////////////////////
BOOL CRilInstanceHandle::Init(CRilHandle* const pDevice)
{
    RIL_LOG_VERBOSE("CRilInstanceHandle::Init() - Enter\r\n");
    BOOL fRet = FALSE;

    if (pDevice->FInEmergencyMode())
    {
        // We can't create any additional RIL instances in emergency mode
        DEBUGCHK(FALSE == m_fPreferred);
        goto Error;
    }

    // Create command list
    m_pCmdList = new CDblList<CCommand>;
    if (NULL == m_pCmdList)
    {
        RIL_LOG_CRITICAL("CRilInstanceHandle::Init() - ERROR: Could not create Command List.\r\n");
        goto Error;
    }

    m_pDevice = pDevice;
    m_fInited = TRUE;

    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CRilInstanceHandle::Init() - Exit (returning %s)\r\n", (fRet ? "TRUE" : "FALSE"));
    return fRet;
}

///////////////////////////////////////////////////////////////////////////////
void CRilInstanceHandle::Notify(const UINT32 dwCode,
                                const HRESULT hrCmdID,
                                const void* const pBlob,
                                const UINT32 cbBlob)
{
    RIL_LOG_VERBOSE("CRilInstanceHandle::Notify() - Enter\r\n");
    DEBUGCHK(FALSE != m_fInited);

    RILDRVNOTIFICATION* prdn = NULL;
    UINT32 cbTotal;

    // Alocate a notification data structure
    // NOTE: LocalAlloc() is used to prevent allocated memory from being tracked
    //       (since it may be freed by a thread coming from RIL proxy)
    cbTotal = sizeof(RILDRVNOTIFICATION) + cbBlob;
    prdn = (RILDRVNOTIFICATION*)LocalAlloc(0, cbTotal);
    if (!prdn)
    {
        goto Error;
    }

    // Fill in the structure fields
    prdn->cbSize = cbTotal;
    prdn->cbSizeNeeded = cbTotal;
    prdn->dwCode = dwCode;
    prdn->hrCmdID = hrCmdID;
    if (cbBlob)
    {
        memcpy(prdn->pbData, pBlob, cbBlob);
    }

    // Try to queue the structure into the Notification Queue
    // (NOTE: this will fail if the queue is already full -- this guarantees that this RIL thread can't
    //        be blocked by a hanging RIL client)
    if (!m_notificationQ.Put(prdn))
    {
        RIL_LOG_CRITICAL("CRilInstanceHandle::Notify() : ERROR : Dropped RIL notification - out of memory? (0x%x)\r\n", (unsigned int)this);
        goto Error;
    }

#ifdef DEBUG
    if (m_notificationQ.GetSize() > 10)
    {
        RIL_LOG_CRITICAL("CRilInstanceHandle::Notify() : ERROR : Notify queue for handle 0x%x has size %d\r\n", (unsigned int)this, m_notificationQ.GetSize());
    }
#endif // DEBUG

    // The queue took ownership of the notification data
    //prdn = NULL;
    RIL_LOG_VERBOSE("CRilInstanceHandle::Notify() - Exit\r\n");
    return;

Error:
    // NOTE: Since we allocated this structure using LocalAlloc(), we need to use LocalFree() to delete it
    //       (or else the memory tracker will assert)
    LocalFree(prdn);
    RIL_LOG_VERBOSE("CRilInstanceHandle::Notify() - Exit\r\n");
}

///////////////////////////////////////////////////////////////////////////////
BOOL CRilInstanceHandle::GetNextNotification(RILDRVNOTIFICATION& rrdnSuppliedStruct)
{
    RILDRVNOTIFICATION* prdnNextNotification = NULL;
    HRESULT hr;
    UINT32    cbBlob;
    BOOL    fRet = FALSE;


    RIL_LOG_VERBOSE("CRilInstanceHandle::GetNextNotification() - Enter\r\n");

    if (!m_fInited)
    {
        RIL_LOG_CRITICAL("CRilInstanceHandle::GetNextNotification() - ERROR: RIL Instance Handle has not been initialized.\r\n");
    }

    // Initialize cbSizeNeeded field
    rrdnSuppliedStruct.cbSizeNeeded = rrdnSuppliedStruct.cbSize;

    // Get the next notification from the queue, if the provided size is sufficient
    hr = m_notificationQ.ConditionalGet(FEnoughStorage, (UINT32)&rrdnSuppliedStruct, &prdnNextNotification);
    if (E_FAIL == hr)
    {
        RIL_LOG_WARNING("CRilInstanceHandle::GetNextNotification() - WARNING: Supplied Size is too small.\r\n");

        // The supplied size wasn't enough - return TRUE
        fRet = TRUE;
        goto Error;
    }
    else if (S_OK != hr)
    {
        RIL_LOG_CRITICAL("CRilInstanceHandle::GetNextNotification() - ERROR: Could not get the next notification from the queue.\r\n");
        goto Error;
    }

    // The supplied size is enough, so we retrieved the next notification

    if (NULL == prdnNextNotification)
    {
        RIL_LOG_CRITICAL("CRilInstanceHandle::GetNextNotification() - ERROR: Retrieved an invalid notification from the queue.\r\n");
        goto Error;
    }

    if (prdnNextNotification->cbSize != prdnNextNotification->cbSizeNeeded)
    {
        RIL_LOG_CRITICAL("CRilInstanceHandle::GetNextNotification() - ERROR: Retrieved a notification with bad size parameters from the queue.\r\n");
        goto Error;
    }

    rrdnSuppliedStruct.dwCode  = prdnNextNotification->dwCode;
    rrdnSuppliedStruct.hrCmdID = prdnNextNotification->hrCmdID;

    cbBlob = prdnNextNotification->cbSize - sizeof(RILDRVNOTIFICATION);
    if (cbBlob)
    {
        memcpy(rrdnSuppliedStruct.pbData, prdnNextNotification->pbData, cbBlob);
    }

    fRet = TRUE;

Error:
    (void)LocalFree(prdnNextNotification);

    RIL_LOG_VERBOSE("CRilInstanceHandle::GetNextNotification() - Exit (returning %s)\r\n", (fRet ? "TRUE" : "FALSE"));
    return fRet;
}

///////////////////////////////////////////////////////////////////////////////
BOOL CRilInstanceHandle::MakePreferred()
{
    RIL_LOG_VERBOSE("CRilInstanceHandle::MakePreferred() - Enter\r\n");
    DEBUGCHK(NULL != m_pDevice);

    BOOL fRet = FALSE;

    // Switch RIL into emergency mode
    if (!m_pDevice->SetEmergencyMode(TRUE))
    {
        goto Error;
    }

    // Mark this handle as preferred
    m_fPreferred = TRUE;
    fRet = TRUE;

    Error:
    RIL_LOG_VERBOSE("CRilInstanceHandle::MakePreferred() - Exit\r\n");
    return fRet;
}

///////////////////////////////////////////////////////////////////////////////
BOOL CRilInstanceHandle::CreateNotificationQueue(const BYTE* tszCancelEventName,
                                                 PBYTE   pBufOut,
                                                 UINT32   dwLenOut,
                                                 PDWORD  pdwActualOut)
{
    BOOL fRet = FALSE;
    UINT32 dwOut = 0;


    RIL_LOG_VERBOSE("CRilInstanceHandle::CreateNotificationQueue() - Enter\r\n");

    if (!m_fInited)
    {
        RIL_LOG_CRITICAL("CRilInstanceHandle::CreateNotificationQueue() - ERROR: RIL Instance Handle has not been initialized.\r\n");
        goto Error;
    }

    if (NULL == pBufOut)
    {
        RIL_LOG_CRITICAL("CRilInstanceHandle::CreateNotificationQueue() - ERROR: Output Buffer pointer is NULL.\r\n");
        goto Error;
    }

    if (0 == dwLenOut)
    {
        RIL_LOG_CRITICAL("CRilInstanceHandle::CreateNotificationQueue() - ERROR: Output Buffer size is ZERO.\r\n");
        goto Error;
    }

    if (NULL == pdwActualOut)
    {
        RIL_LOG_CRITICAL("CRilInstanceHandle::CreateNotificationQueue() - ERROR: Actual Output Buffer Size pointer is NULL.\r\n");
        goto Error;
    }

    // Initialize Notification Q
    if (!m_notificationQ.Init(5))
    {
        RIL_LOG_CRITICAL("CRilInstanceHandle::CreateNotificationQueue() - ERROR: Could not initialize Notification Queue.\r\n");
        goto Error;
    }

    // Create the cancel event
    if (NULL == m_pNotifyCancelEvent)
    {
        if (!tszCancelEventName)
        {
            RIL_LOG_WARNING("CRilInstanceHandle::CreateNotificationQueue() - WARNING: Creating Notify Cancel Event with no name!\r\n");
            m_pNotifyCancelEvent = new CEvent(NULL, TRUE);
        }
        else
        {
            char * szEventName = NULL;
            int    iEventNameLength = 0;


            iEventNameLength = _tcslen(tszCancelEventName) + 1;

            szEventName = new char[iEventNameLength];

            if (NULL == szEventName)
            {
                RIL_LOG_CRITICAL("CRilInstanceHandle::CreateNotificationQueue() - ERROR: Could not allocate memory for a %d-char string buffer.\r\n", iEventNameLength);
                goto Error;
            }

            if (!ConvertTCharToChar(tszCancelEventName, szEventName, iEventNameLength))
            {
                RIL_LOG_CRITICAL("CRilInstanceHandle::CreateNotificationQueue() - ERROR: Could not convert the Notify Cancel Event name from TCHAR format to standard format.\r\n");
                goto Error;
            }

            m_pNotifyCancelEvent = new CEvent(szEventName, TRUE);

            delete [] szEventName;
            szEventName = NULL;
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CRilInstanceHandle::CreateNotificationQueue() - ERROR: Notify Cancel event has already been created.\r\n");
    }

    if (NULL == m_pNotifyCancelEvent)
    {
        RIL_LOG_CRITICAL("CRilInstanceHandle::CreateNotificationQueue() - ERROR: Could not create Notify Cancel event.\r\n");
        goto Error;
    }

    // Clear the output buffer
    memset(pBufOut, 0, dwLenOut);

    UINT32 uiGetItemsEventNameSize = (strlen(m_notificationQ.GetItemsEventName()) + 1) * sizeof(TCHAR);

    if (uiGetItemsEventNameSize > dwLenOut)
    {
        RIL_LOG_CRITICAL("CRilInstanceHandle::CreateNotificationQueue() - ERROR: Output Buffer is not big enough to hold Get Items Event name.\r\n");
        goto Error;
    }

    if (!ConvertCharToTChar(m_notificationQ.GetItemsEventName(), (TCHAR*)pBufOut, uiGetItemsEventNameSize))
    {
        RIL_LOG_CRITICAL("CRilInstanceHandle::CreateNotificationQueue() - ERROR: Could not convert Get Items Event name to TCHAR format.\r\n");
        goto Error;
    }
    else
    {
        *pdwActualOut = uiGetItemsEventNameSize;
    }

    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CRilInstanceHandle::CreateNotificationQueue() - Exit\r\n");
    return fRet;
}

#endif // __linux__
