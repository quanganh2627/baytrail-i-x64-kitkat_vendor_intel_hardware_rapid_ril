////////////////////////////////////////////////////////////////////////////
// te_xmm7x60.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Overlay for the IMC 7x60 modem
//
/////////////////////////////////////////////////////////////////////////////

#include <wchar.h>
#include <cutils/properties.h>
#include <sys/system_properties.h>

//  This is for socket-related calls.
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/gsmmux.h>

#include "types.h"
#include "nd_structs.h"
#include "util.h"
#include "extract.h"
#include "rillog.h"
#include "te.h"
#include "sync_ops.h"
#include "command.h"
#include "te_xmm7x60.h"
#include "rildmain.h"
#include "callbacks.h"
#include "oemhookids.h"
#include "repository.h"
#include "reset.h"
#include "data_util.h"


CTE_XMM7x60::CTE_XMM7x60(CTE& cte)
: CTE_XMM6360(cte),
  m_currentNetworkType(-1)
{
}

CTE_XMM7x60::~CTE_XMM7x60()
{
}

//
// RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
//
RIL_RESULT_CODE CTE_XMM7x60::CoreSetPreferredNetworkType(REQUEST_DATA& rReqData,
        void* pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM7x60::CoreSetPreferredNetworkType() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    RIL_PreferredNetworkType networkType = PREF_NET_TYPE_GSM_WCDMA; // 0

    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_XMM7x60::CoreSetPreferredNetworkType() - Data "
                "pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != sizeof(RIL_PreferredNetworkType*))
    {
        RIL_LOG_CRITICAL("CTE_XMM7x60::CoreSetPreferredNetworkType() - "
                "Invalid data size.\r\n");
        goto Error;
    }

    networkType = ((RIL_PreferredNetworkType*)pData)[0];

    // if network type already set, NO-OP this command
    if (m_currentNetworkType == networkType)
    {
        rReqData.szCmd1[0] = '\0';
        res = RRIL_RESULT_OK;
        RIL_LOG_INFO("CTE_XMM7x60::CoreSetPreferredNetworkType() - "
                "Network type {%d} already set.\r\n", networkType);
        goto Error;
    }

    switch (networkType)
    {
        case PREF_NET_TYPE_GSM_WCDMA: // WCDMA Preferred
            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT=3,1\r",
                    sizeof(rReqData.szCmd1) ))
            {
                RIL_LOG_CRITICAL("CTE_XMM6360::HandleNetworkType() - Can't "
                        "construct szCmd1 networkType=%d\r\n", networkType);
                break;
            }
            res = RRIL_RESULT_OK;
            break;

        case PREF_NET_TYPE_GSM_ONLY: // GSM Only
            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT=0\r",
                    sizeof(rReqData.szCmd1) ))
            {
                RIL_LOG_CRITICAL("CTE_XMM6360::HandleNetworkType() - Can't "
                        "construct szCmd1 networkType=%d\r\n", networkType);
                break;
            }
            res = RRIL_RESULT_OK;
            break;

        case PREF_NET_TYPE_WCDMA: // WCDMA Only
            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT=1\r",
                    sizeof(rReqData.szCmd1)))
            {
                RIL_LOG_CRITICAL("CTE_XMM6360::HandleNetworkType() - Can't "
                        "construct szCmd1 networkType=%d\r\n", networkType);
                break;
            }
            res = RRIL_RESULT_OK;
            break;

        case PREF_NET_TYPE_LTE_ONLY: // LTE Only
            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT=2\r",
                    sizeof(rReqData.szCmd1)))
            {
                RIL_LOG_CRITICAL("CTE_XMM7x60::CoreSetPreferredNetworkType() - "
                        "Can't construct szCmd1 networkType=%d\r\n", networkType);
                goto Error;
            }
            break;

        /*
         * PREF_NET_TYPE_GSM_WCDMA_CDMA_EVDO_AUTO value is received as a result
         * of the recovery mechanism in the framework.
         */
        case PREF_NET_TYPE_LTE_GSM_WCDMA: // LTE Preferred
        case PREF_NET_TYPE_GSM_WCDMA_CDMA_EVDO_AUTO:
            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT=6,2,1\r",
                    sizeof(rReqData.szCmd1)))
            {
                RIL_LOG_CRITICAL("CTE_XMM7x60::CoreSetPreferredNetworkType() - "
                    "Can't construct szCmd1 networkType=%d\r\n", networkType);
                goto Error;
            }

        default:
            RIL_LOG_CRITICAL("CTE_XMM6360::CoreSetPreferredNetworkType() - "
                    "Undefined rat code: %d\r\n", networkType);
            res = RIL_E_MODE_NOT_SUPPORTED;
            goto Error;
    }

    //  Set the context of this command to the network type we're attempting to set
    rReqData.pContextData = (void*)networkType;  // Store this as an int.

    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTE_XMM7x60::CoreSetPreferredNetworkType() - Exit\r\n");
    return res;
}

// RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
//
RIL_RESULT_CODE CTE_XMM7x60::CoreGetPreferredNetworkType(REQUEST_DATA& rReqData,
        void* pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM7x60::CoreGetPreferredNetworkType() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (CopyStringNullTerminate(rReqData.szCmd1, "AT+XACT?\r",
            sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }

    RIL_LOG_VERBOSE("CTE_XMM7x60::CoreGetPreferredNetworkType() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_XMM7x60::ParseGetPreferredNetworkType(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_XMM7x60::ParseGetPreferredNetworkType() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char* pszRsp = rRspData.szResponse;

    UINT32 rat = 0;
    UINT32 pref = 0;

    int* pRat = (int*)malloc(sizeof(int));
    if (NULL == pRat)
    {
        RIL_LOG_CRITICAL("CTE_XMM7x60::ParseGetPreferredNetworkType() - Could "
                "not allocate memory for response.\r\n");
        goto Error;
    }

    // Skip "<prefix>"
    if (!SkipRspStart(pszRsp, m_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_XMM7x60::ParseGetPreferredNetworkType() - Could "
                "not skip response prefix.\r\n");
        goto Error;
    }

    // Skip "+XACT: "
    if (!SkipString(pszRsp, "+XACT: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_XMM7x60::ParseGetPreferredNetworkType() - Could "
                "not skip \"+XACT: \".\r\n");
        goto Error;
    }

    if (!ExtractUInt32(pszRsp, rat, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_XMM7x60::ParseGetPreferredNetworkType() - Could "
                "not extract rat value.\r\n");
        goto Error;
    }

    if (FindAndSkipString(pszRsp, ",", pszRsp))
    {
        if (!ExtractUInt32(pszRsp, pref, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_XMM7x60::ParseGetPreferredNetworkType() - "
                    "Could not find and skip pref value even though it was expected.\r\n");
            goto Error;
        }
    }

    switch (rat)
    {
        case 0:     // GSM Only
        {
            pRat[0] = PREF_NET_TYPE_GSM_ONLY;
            m_currentNetworkType = PREF_NET_TYPE_GSM_ONLY;
            break;
        }

        case 1:     // WCDMA Only
        {
            pRat[0] = PREF_NET_TYPE_WCDMA;
            m_currentNetworkType = PREF_NET_TYPE_WCDMA;
            break;
        }

        case 2:     // LTE only
        {
            pRat[0] = PREF_NET_TYPE_LTE_ONLY;
            m_currentNetworkType = PREF_NET_TYPE_LTE_ONLY;
            break;
        }

        case 3:     // WCDMA prefered
        {
            pRat[0] = PREF_NET_TYPE_GSM_WCDMA;
            m_currentNetworkType = PREF_NET_TYPE_GSM_WCDMA;
            break;
        }

        case 6:     // LTE prefered
        {
            pRat[0] = PREF_NET_TYPE_LTE_GSM_WCDMA;
            m_currentNetworkType = PREF_NET_TYPE_LTE_GSM_WCDMA;
            break;
        }

        default:
        {
            RIL_LOG_CRITICAL("CTE_XMM7x60::ParseGetPreferredNetworkType() - "
                    "Unexpected rat found: %d. Failing out.\r\n", rat);
            goto Error;
        }
    }

    rRspData.pData  = (void*)pRat;
    rRspData.uiDataSize = sizeof(int*);

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pRat);
        pRat = NULL;
    }

    RIL_LOG_VERBOSE("CTE_XMM7x60::ParseGetPreferredNetworkType() - Exit\r\n");
    return res;
}

