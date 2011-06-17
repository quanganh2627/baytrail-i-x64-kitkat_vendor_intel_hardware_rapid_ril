////////////////////////////////////////////////////////////////////////////
// ril_result_nd.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
// Enums, defines and functions to work with error types internally in RapidRIL
// with static functions defined for ND
//
// Author:  Mike Worth
// Created: 2009-11-10
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Nov 10/09  MW       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "rillog.h"
#include "rril.h"
#include "ril_result_nd.h"

BOOL CRilResult::SetResult( const UINT32 uiRspCode,
                            const void* pRspData,
                            const UINT32 cbRspData)
{
    BOOL fRet = FALSE;

    switch ((RIL_Errno)uiRspCode)
    {
        case RIL_E_SUCCESS:
        {
            result      = RIL_API_RESULT_OK;
            resultType  = RIL_RESULT_TYPE_NONE;
            resultCode  = RRIL_RESULT_CODE_NONE;
            break;
        }

        case RIL_E_RADIO_NOT_AVAILABLE:
        {
            result      = RIL_API_RESULT_FAIL;
            resultType  = RIL_RESULT_TYPE_RRIL;
            resultCode  = RRIL_RESULT_CODE_RADIO_NOT_AVAILABLE;
            break;
        }

        case RIL_E_GENERIC_FAILURE:
        {
            result      = RIL_API_RESULT_FAIL;
            resultType  = RIL_RESULT_TYPE_RRIL;
            resultCode  = RRIL_RESULT_CODE_NONE;
            break;
        }

        case RIL_E_PASSWORD_INCORRECT:
        {
            result      = RIL_API_RESULT_FAIL;
            resultType  = RIL_RESULT_TYPE_CME;
            resultCode  = CME_ERROR_INCORRECT_PASSWORD;
            break;
        }

        case RIL_E_SIM_PIN2:
        {
            result      = RIL_API_RESULT_FAIL;
            resultType  = RIL_RESULT_TYPE_CME;
            resultCode  = CME_ERROR_SIM_PIN2_REQUIRED;
            break;
        }

        case RIL_E_SIM_PUK2:
        {
            result      = RIL_API_RESULT_FAIL;
            resultType  = RIL_RESULT_TYPE_CME;
            resultCode  = CME_ERROR_SIM_PUK2_REQUIRED;
            break;
        }

        case RIL_E_REQUEST_NOT_SUPPORTED:
        {
            result      = RIL_API_RESULT_FAIL;
            resultType  = RIL_RESULT_TYPE_CME;
            resultCode  = CME_ERROR_OPERATION_NOT_SUPPORTED;
            break;
        }

        case RIL_E_CANCELLED:
        {
            result      = RIL_API_RESULT_FAIL;
            resultType  = RIL_RESULT_TYPE_RRIL;
            resultCode  = RRIL_RESULT_CODE_CMD_CANCELLED;
            break;
        }

        case RIL_E_OP_NOT_ALLOWED_DURING_VOICE_CALL:
        {
            result      = RIL_API_RESULT_FAIL;
            resultType  = RIL_RESULT_TYPE_RRIL;
            resultCode  = RRIL_RESULT_CODE_CMD_NOT_ALLOWED_DURING_VOICE_CALL;
            break;
        }

        case RIL_E_OP_NOT_ALLOWED_BEFORE_REG_TO_NW:
        {
            result      = RIL_API_RESULT_FAIL;
            resultType  = RIL_RESULT_TYPE_RRIL;
            resultCode  = RRIL_RESULT_CODE_CMD_NOT_ALLOWED_BEFORE_NW_REG;
            break;
        }

        case RIL_E_SMS_SEND_FAIL_RETRY:
        {
            result      = RIL_API_RESULT_FAIL;
            resultType  = RIL_RESULT_TYPE_RRIL;
            resultCode  = RRIL_RESULT_CODE_SMS_SEND_FAILED;
            break;
        }

        case RIL_E_SIM_ABSENT:
        {
            result      = RIL_API_RESULT_FAIL;
            resultType  = RIL_RESULT_TYPE_CME;
            resultCode  = CME_ERROR_SIM_NOT_INSERTED;
            break;
        }

        case RIL_E_SUBSCRIPTION_NOT_AVAILABLE:
        {
            result      = RIL_API_RESULT_FAIL;
            resultType  = RIL_RESULT_TYPE_RRIL;
            resultCode  = RRIL_RESULT_CODE_SUBSCRIPTION_NOT_AVAILABLE;
            break;
        }

        case RIL_E_MODE_NOT_SUPPORTED:
        {
            result      = RIL_API_RESULT_FAIL;
            resultType  = RIL_RESULT_TYPE_RRIL;
            resultCode  = RRIL_RESULT_CODE_NW_MODE_NOT_SUPPORTED;
            break;
        }

        case RIL_E_FDN_CHECK_FAILURE:
        {
            result      = RIL_API_RESULT_FAIL;
            resultType  = RIL_RESULT_TYPE_RRIL;
            resultCode  = RRIL_RESULT_CODE_FDN_CHECK_FAILED;
            break;
        }

        default:
        {
            result      = RIL_API_RESULT_FAIL;
            resultType  = RIL_RESULT_TYPE_RRIL;
            resultCode  = RRIL_RESULT_CODE_RESULT_CONVERSION_FAILED;
            return fRet;
        }
    }

    fRet = TRUE;
    return fRet;
}

RIL_Errno GetErrnoForCME(UINT32 resultCode)
{
    RIL_Errno rilErrno;

    switch (resultCode)
    {
        case CME_ERROR_OPERATION_NOT_SUPPORTED:
            rilErrno = RIL_E_REQUEST_NOT_SUPPORTED;
            break;

        case CME_ERROR_SIM_NOT_INSERTED:
            rilErrno = RIL_E_SIM_ABSENT;
            break;

        case CME_ERROR_INCORRECT_PASSWORD:
            rilErrno = RIL_E_PASSWORD_INCORRECT;
            break;

        case CME_ERROR_SIM_PIN2_REQUIRED:
            rilErrno = RIL_E_SIM_PIN2;
            break;

        case CME_ERROR_SIM_PUK2_REQUIRED:
            rilErrno = RIL_E_SIM_PUK2;
            break;

        case CME_ERROR_SIM_NOT_READY:
        default:
            rilErrno = RIL_E_GENERIC_FAILURE;
            break;
    }

    return rilErrno;
}

RIL_Errno GetErrnoForCMS(UINT32 resultCode)
{
    RIL_Errno rilErrno;

    switch (resultCode)
    {
        case CMS_ERROR_SIM_NOT_INSERTED:
            rilErrno = RIL_E_SIM_ABSENT;
            break;

        default:
            rilErrno = RIL_E_GENERIC_FAILURE;
            break;
    }

    return rilErrno;
}

RIL_Errno GetErrnoForV25(UINT32 resultCode)
{
    RIL_Errno rilErrno;

    switch (resultCode)
    {
        case V25_RESULT_CODE_CONNECT:
            rilErrno = RIL_E_SUCCESS;
            break;

        case V25_RESULT_CODE_NO_CARRIER:
        case V25_RESULT_CODE_NO_DIALTONE:
        case V25_RESULT_CODE_NO_ANSWER:
        case V25_RESULT_CODE_BUSY:
        case V25_RESULT_CODE_ABORTED:
        default:
            rilErrno = RIL_E_GENERIC_FAILURE;
            break;
    }

    return rilErrno;
}

RIL_Errno GetErrnoForRRIL(UINT32 resultCode)
{
    RIL_Errno rilErrno;

    switch (resultCode)
    {
        case RRIL_RESULT_CODE_NOT_IMPLEMENTED:
            rilErrno = RIL_E_REQUEST_NOT_SUPPORTED;
            break;

        case RRIL_RESULT_CODE_CMD_CANCELLED:
            rilErrno = RIL_E_CANCELLED;
            break;

        case RRIL_RESULT_CODE_CMD_NOT_ALLOWED_DURING_VOICE_CALL:
            rilErrno = RIL_E_OP_NOT_ALLOWED_DURING_VOICE_CALL;
            break;
        case RRIL_RESULT_CODE_CMD_NOT_ALLOWED_BEFORE_NW_REG:
            rilErrno = RIL_E_OP_NOT_ALLOWED_BEFORE_REG_TO_NW;
            break;

        case RRIL_RESULT_CODE_RADIO_OFF:
        case RRIL_RESULT_CODE_RADIO_NOT_AVAILABLE:
            rilErrno = RIL_E_RADIO_NOT_AVAILABLE;
            break;

        case RRIL_RESULT_CODE_SMS_SEND_FAILED:
            rilErrno = RIL_E_SMS_SEND_FAIL_RETRY;
            break;

        case RRIL_RESULT_CODE_NW_MODE_NOT_SUPPORTED:
            rilErrno = RIL_E_MODE_NOT_SUPPORTED;
            break;

        case RRIL_RESULT_CODE_SUBSCRIPTION_NOT_AVAILABLE:
            rilErrno = RIL_E_SUBSCRIPTION_NOT_AVAILABLE;
            break;

        case RRIL_RESULT_CODE_FDN_CHECK_FAILED:
            rilErrno = RIL_E_FDN_CHECK_FAILURE;
            break;

        case RRIL_RESULT_CODE_CMD_TIMED_OUT:
        case RRIL_RESULT_CODE_PARSE_FAILED:
        case RRIL_RESULT_CODE_RESULT_CONVERSION_FAILED:
        case RRIL_RESULT_CODE_RADIO_FAILED_INIT:
        case RRIL_RESULT_CODE_DIAL_STRING_TOO_LONG:
        case RRIL_RESULT_CODE_NONE:
        default:
            rilErrno = RIL_E_GENERIC_FAILURE;
            break;
    }

    return rilErrno;
}

// NOTE: We do not update the data pointers in Android. We assume the parsing
//       functions did their job properly and populated them as required.
BOOL CRilResult::GetOSResponse( UINT32 & ruiRspCode,
                                void *& rpRspData,
                                UINT32 & rcbRspData)
{
    BOOL fRet = FALSE;

    if (RIL_RESULT_TYPE_V25 == resultType)
    {
        ruiRspCode = GetErrnoForV25(resultCode);
    }
    else if (RIL_API_RESULT_OK == result)
    {
        ruiRspCode = RIL_E_SUCCESS;
    }
    else if (RIL_RESULT_TYPE_NONE == resultType)
    {
        ruiRspCode = RIL_E_GENERIC_FAILURE;
    }
    else if (RIL_RESULT_TYPE_CME == resultType)
    {
        ruiRspCode = GetErrnoForCME(resultCode);
    }
    else if (RIL_RESULT_TYPE_CMS == resultType)
    {
        ruiRspCode = GetErrnoForCMS(resultCode);
    }
    else if (RIL_RESULT_TYPE_RRIL == resultType)
    {
        ruiRspCode = GetErrnoForRRIL(resultCode);
    }
    else
    {
        RIL_LOG_CRITICAL("CRilResult::GetOSResponse() - ERROR: Invalid result type found: %d\r\n", resultType);
        return fRet;
    }

    RIL_LOG_INFO("CRilResult::GetOSResponse() - INFO: Setting response rilErrno to %d\r\n", ruiRspCode);

    fRet = TRUE;
    return fRet;
}
