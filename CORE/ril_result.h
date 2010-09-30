////////////////////////////////////////////////////////////////////////////
// ril_result.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
// Enums, defines and functions to work with error types internally in RapidRIL
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

#ifndef RIL_RESULT_H
#define RIL_RESULT_H

#include "rril.h"

typedef enum
{
    RIL_API_RESULT_OK = 0,
    RIL_API_RESULT_FAIL
} RIL_API_RESULT;

typedef enum
{
    RIL_RESULT_TYPE_NONE = 0,
    RIL_RESULT_TYPE_CME,
    RIL_RESULT_TYPE_CMS,
    RIL_RESULT_TYPE_V25,
    RIL_RESULT_TYPE_RRIL
} RIL_RESULT_TYPE;

typedef enum
{
    RRIL_RESULT_CODE_NONE = 0,
    RRIL_RESULT_CODE_NOT_IMPLEMENTED,
    RRIL_RESULT_CODE_PARSE_FAILED,
    RRIL_RESULT_CODE_RESULT_CONVERSION_FAILED,
    RRIL_RESULT_CODE_CMD_TIMED_OUT,
    RRIL_RESULT_CODE_CMD_CANCELLED,
    RRIL_RESULT_CODE_CMD_NOT_ALLOWED_DURING_VOICE_CALL,
    RRIL_RESULT_CODE_CMD_NOT_ALLOWED_BEFORE_NW_REG,
    RRIL_RESULT_CODE_RADIO_OFF,
    RRIL_RESULT_CODE_RADIO_NOT_AVAILABLE,
    RRIL_RESULT_CODE_RADIO_FAILED_INIT,
    RRIL_RESULT_CODE_SMS_SEND_FAILED,
    RRIL_RESULT_CODE_NW_MODE_NOT_SUPPORTED,
    RRIL_RESULT_CODE_SUBSCRIPTION_NOT_AVAILABLE,
    RRIL_RESULT_CODE_FDN_CHECK_FAILED,
    RRIL_RESULT_CODE_DIAL_STRING_TOO_LONG
} RRIL_RESULT_CODE;

typedef enum
{
    V25_RESULT_CODE_NONE = 0,
    V25_RESULT_CODE_CONNECT,
    V25_RESULT_CODE_NO_CARRIER,
    V25_RESULT_CODE_NO_DIALTONE,
    V25_RESULT_CODE_NO_ANSWER,
    V25_RESULT_CODE_BUSY,
    V25_RESULT_CODE_ABORTED
} V25_RESULT_CODE;

// CME error codes are mapped to 3GPP spec
#define CME_ERROR_OPERATION_NOT_SUPPORTED                   4
#define CME_ERROR_SIM_NOT_INSERTED                          10
#define CME_ERROR_SIM_PIN_REQUIRED                          11
#define CME_ERROR_SIM_PUK_REQUIRED                          12
#define CME_ERROR_INCORRECT_PASSWORD                        16
#define CME_ERROR_SIM_PIN2_REQUIRED                         17
#define CME_ERROR_SIM_PUK2_REQUIRED                         18
#define CME_ERROR_SIM_NOT_READY                             515

// CMS error codes are mapped to 3GPP spec
#define CMS_ERROR_SIM_NOT_INSERTED                          310

class CRilResultBase
{
public:
    CRilResultBase() : 
        result(RIL_API_RESULT_OK),
        resultType(RIL_RESULT_TYPE_NONE),
        resultCode(RRIL_RESULT_CODE_NONE) {};

    virtual ~CRilResultBase() {};
    
    // Called after getting response from OEM.
    virtual BOOL SetResult(const UINT32 uiRspCode, const void * pRspData, const UINT32 cbRspData) = 0;
    
    // Called before sending response to OS.
    virtual BOOL GetOSResponse(UINT32 & ruiRspCode, void *& rpRspData, UINT32 & rcbRspData) = 0;

    RIL_API_RESULT      result;
    RIL_RESULT_TYPE     resultType;
    UINT32              resultCode;
};

#endif
