////////////////////////////////////////////////////////////////////////////
// request_info_table.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//      Stores and provides information regarding a particular request.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "rillog.h"
#include "request_info_table.h"
#include "repository.h"
#include "te.h"

///////////////////////////////////////////////////////////////////////////////
CRequestInfoTable::CRequestInfoTable()
{
    memset(m_rgpRequestInfos, 0x00, sizeof(REQ_INFO*) * REQ_ID_TOTAL);
    m_pCacheAccessMutex = new CMutex();
}

///////////////////////////////////////////////////////////////////////////////
CRequestInfoTable::~CRequestInfoTable()
{
    for (int i = 0; i < REQ_ID_TOTAL; i++)
    {
        delete m_rgpRequestInfos[i];
        m_rgpRequestInfos[i] = NULL;
    }
    if (m_pCacheAccessMutex != NULL)
    {
        delete m_pCacheAccessMutex;
    }
}

///////////////////////////////////////////////////////////////////////////////
void CRequestInfoTable::GetRequestInfo(REQ_ID requestID, REQ_INFO& rReqInfo)
{
    RIL_LOG_VERBOSE("CRequestInfoTable::GetRequestInfo() - Enter\r\n");

    // Set defaults if we can't find the request id
    rReqInfo.uiTimeout = CTE::GetTE().GetTimeoutAPIDefault();

    if (REQ_ID_NONE == requestID)
    {
        RIL_LOG_INFO("CRequestInfoTable::GetRequestInfo() - Request ID NONE given; assigning"
                " default values.\r\n");
        goto Error;
    }
    else if ((requestID >= REQ_ID_TOTAL) || (requestID < 0))
    {
        RIL_LOG_CRITICAL("CRequestInfoTable::GetRequestInfo() - Invalid request ID [0x%X]\r\n",
                requestID);
        goto Error;
    }

    if (NULL == m_rgpRequestInfos[requestID])
    {
        CRepository repository;
        int         iTemp = 0;

        memset(&rReqInfo, 0, sizeof(rReqInfo));

        if (repository.Read(g_szGroupRequestTimeouts, g_szRequestNames[requestID], iTemp))
        {
            rReqInfo.uiTimeout = (UINT32)iTemp;
        }
        else
        {
            rReqInfo.uiTimeout = CTE::GetTE().GetTimeoutAPIDefault();
        }

        // Use WAIT_FOREVER timeout if given time was 0
        if (!rReqInfo.uiTimeout)
        {
            rReqInfo.uiTimeout = WAIT_FOREVER;
        }
#if defined(M2_DUALSIM_FEATURE_ENABLED)
        else
        {
            // Timeout values need to be extended for DSDS capable modems,
            // depending on type of command (ie. network/non-network)
            switch ((UINT32)requestID)
            {
                case ND_REQ_ID_SETUPDEFAULTPDP: // +CGACT, +CGDATA, +CGDCONT, +CGPADDR, +XDNS
                case ND_REQ_ID_DEACTIVATEDATACALL:
                    rReqInfo.uiTimeout = 2 * rReqInfo.uiTimeout + 50000;
                    break;

                // network commands
                case ND_REQ_ID_QUERYCALLFORWARDSTATUS:  // +CCFC
                case ND_REQ_ID_SETCALLFORWARD:          // +CCFC
                case ND_REQ_ID_QUERYCALLWAITING:        // +CCWA
                case ND_REQ_ID_SETCALLWAITING:          // +CCWA
                case ND_REQ_ID_RADIOPOWER:              // +CFUN
                case ND_REQ_ID_PDPCONTEXTLIST:          // +CGACT?
                case ND_REQ_ID_HANGUP:                  // +CHLD
                case ND_REQ_ID_HANGUPWAITINGORBACKGROUND: // +CHLD
                case ND_REQ_ID_HANGUPFOREGROUNDRESUMEBACKGROUND: // +CHLD
                case ND_REQ_ID_SWITCHHOLDINGANDACTIVE:  // +CHLD
                case ND_REQ_ID_CONFERENCE:              // +CHLD
                case ND_REQ_ID_UDUB:                    // +CHLD
                case ND_REQ_ID_SEPERATECONNECTION:      // +CHLD
                case ND_REQ_ID_EXPLICITCALLTRANSFER:    // +CHLD
                case ND_REQ_ID_ENTERNETWORKDEPERSONALIZATION: // +CLCK
                case ND_REQ_ID_QUERYFACILITYLOCK:       // +CLCK
                case ND_REQ_ID_SETFACILITYLOCK:         // +CLCK
                case ND_REQ_ID_GETCLIR:                 // +CLIR?
                case ND_REQ_ID_SETCLIR:                 // +CLIR
                case ND_REQ_ID_SENDSMS:                 // +CMGS
                case ND_REQ_ID_SMSACKNOWLEDGE:          // +CNMA
                case ND_REQ_ID_OPERATOR:                // +XCOPS
                case ND_REQ_ID_QUERYNETWORKSELECTIONMODE: // +COPS?
                case ND_REQ_ID_QUERYAVAILABLENETWORKS:  // +COPS=?
                case ND_REQ_ID_SETNETWORKSELECTIONAUTOMATIC: // +COPS
                case ND_REQ_ID_SETNETWORKSELECTIONMANUAL: // +COPS
                case ND_REQ_ID_SENDUSSD:                // +CUSD
                case ND_REQ_ID_CANCELUSSD:              // +CUSD
                case ND_REQ_ID_STKHANDLECALLSETUPREQUESTEDFROMSIM: // +SATD
                case ND_REQ_ID_REQUESTDTMFSTART:        // +XVTS
                case ND_REQ_ID_REQUESTDTMFSTOP:         // +XVTS
                case ND_REQ_ID_DIAL:                    // ATD
#if defined(M2_VT_FEATURE_ENABLED)
                case ND_REQ_ID_DIALVT:                  // ATD
                case ND_REQ_ID_HANGUPVT:                // ATH
#endif // M2_VT_FEATURE_ENABLED
                // non-network cmds requiring response
                case ND_REQ_ID_DELETESMSONSIM:          // +CMGD
                case ND_REQ_ID_SENDSMSEXPECTMORE:       // +CMMS, +CMGS
                case ND_REQ_ID_GETBROADCASTSMSCONFIG:   // +CSCB?
                case ND_REQ_ID_SMSBROADCASTACTIVATION:  // +CSCB
                case ND_REQ_ID_GETNEIGHBORINGCELLIDS:   // +XCELLINFO?
                case ND_REQ_ID_GETCELLINFOLIST:         // +XCELLINFO?
                case ND_REQ_ID_REPORTSMSMEMORYSTATUS:   // +XTESM
                    rReqInfo.uiTimeout = 2 * rReqInfo.uiTimeout + 10000;
                    break;
            }
        }
#endif // M2_DUALSIM_FEATURE_ENABLED

        // Cache the data we just read (taking the cache access lock)
        if (m_pCacheAccessMutex)
        {
            CMutex::Lock(m_pCacheAccessMutex);
        }
        // Recheck if the cache is still empty
        if (NULL == m_rgpRequestInfos[requestID])
        {
            REQ_INFO* pNewReqInfo = new REQ_INFO;
            if (NULL != pNewReqInfo)
            {
                *pNewReqInfo = rReqInfo;
                m_rgpRequestInfos[requestID] = pNewReqInfo;
            }
        }
        if (m_pCacheAccessMutex)
        {
            CMutex::Unlock(m_pCacheAccessMutex);
        }
    }
    else
    {
        rReqInfo = *m_rgpRequestInfos[requestID];
    }

Error:
    RIL_LOG_INFO("CRequestInfoTable::GetRequestInfo() - RequestID 0x%X: Timeout [%d]\r\n",
                   requestID,
                   rReqInfo.uiTimeout);

    RIL_LOG_VERBOSE("CRequestInfoTable::GetRequestInfo() - Exit\r\n");
}
