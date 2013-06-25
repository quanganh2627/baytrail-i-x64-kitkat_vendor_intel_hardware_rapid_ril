////////////////////////////////////////////////////////////////////////////
// silo_misc.cpp
//
// Copyright 2012 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides response handlers and parsing functions for the misc-related
//    RIL components.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "rillog.h"
#include "response.h"
#include "rildmain.h"
#include "callbacks.h"
#include "silo_misc.h"
#include "extract.h"
#include "oemhookids.h"
#include "systemcaps.h"

#include <arpa/inet.h>

//
//
CSilo_MISC::CSilo_MISC(CChannel* pChannel, CSystemCapabilities* pSysCaps)
: CSilo(pChannel, pSysCaps)
{
    RIL_LOG_VERBOSE("CSilo_MISC::CSilo_MISC() - Enter\r\n");

    // AT Response Table
    static ATRSPTABLE pATRspTable[] =
    {
        { "+XDRVI: "   , (PFN_ATRSP_PARSE)&CSilo_MISC::ParseXDRVI  },
        { "+PBREADY"   , (PFN_ATRSP_PARSE)&CSilo_MISC::ParseUnrecognized },
        { "RING CTM"   , (PFN_ATRSP_PARSE)&CSilo_MISC::ParseUnrecognized },
        { "RING"       , (PFN_ATRSP_PARSE)&CSilo_MISC::ParseUnrecognized },
        { "CTM CALL", (PFN_ATRSP_PARSE)&CSilo_MISC::ParseUnrecognized },
        { "NO CTM CALL", (PFN_ATRSP_PARSE)&CSilo_MISC::ParseUnrecognized },
        { "WAITING CALL CTM", (PFN_ATRSP_PARSE)&CSilo_MISC::ParseUnrecognized },
        { ""           , (PFN_ATRSP_PARSE)&CSilo_MISC::ParseNULL }
    };

    m_pATRspTable = pATRspTable;
    RIL_LOG_VERBOSE("CSilo_MISC::CSilo_MISC() - Exit\r\n");
}


//
//
CSilo_MISC::~CSilo_MISC()
{
    RIL_LOG_VERBOSE("CSilo_MISC::~CSilo_MISC() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_MISC::~CSilo_MISC() - Exit\r\n");
}

char* CSilo_MISC::GetBasicInitString()
{
    // misc silo-related channel basic init string
    const char szMiscBasicInitString[] = "+XGENDATA|+XPOW=0,0,0";

    if (!ConcatenateStringNullTerminate(m_szBasicInitString,
            MAX_BUFFER_SIZE - strlen(m_szBasicInitString), szMiscBasicInitString))
    {
        RIL_LOG_CRITICAL("CSilo_MISC::GetBasicInitString() : Failed to copy basic init "
                "string!\r\n");
        return NULL;
    }

    return m_szBasicInitString;
}

//
// Thermal sensor notification
//
BOOL CSilo_MISC::ParseXDRVI(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_MISC::ParseXDRVI() - Enter\r\n");
    BOOL   fRet = FALSE;
    const char* pszEnd = NULL;
    sOEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND* pData = NULL;
    UINT32 nIpcChrGrp;
    UINT32 nIpcChrTempThresholdInd;
    UINT32 nXdrvResult;
    UINT32 nSensorId;
    UINT32 nTemp;

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_MISC::ParseXDRVI() - pResponse is NULL.\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, m_szNewLine, pszEnd))
    {
        RIL_LOG_CRITICAL("CSilo_MISC::ParseXDRVI() : Could not find response end\r\n");
        goto Error;
    }

    // Extract "<IPC_CHR_GRP>"
    if (!ExtractUInt32(rszPointer, nIpcChrGrp, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_MISC::ParseXDRVI() - Could not parse nIpcChrGrp.\r\n");
        goto Error;
    }

    // Parse <IPC_CHR_TEMP_THRESHOLD_IND>
    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractUInt32(rszPointer, nIpcChrTempThresholdInd, rszPointer))
    {
         RIL_LOG_CRITICAL("CSilo_MISC::ParseXDRVI() -"
                 " Unable to parse <IPC_CHR_TEMP_THRESHOLD_IND>!\r\n");
         goto Error;
    }

    // Parse <xdrv_result>
    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractUInt32(rszPointer, nXdrvResult, rszPointer))
    {
         RIL_LOG_CRITICAL("CSilo_MISC::ParseXDRVI() - Unable to parse <xdrv_result>!\r\n");
         goto Error;
    }

    // Parse <temp_sensor_id>
    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractUInt32(rszPointer, nSensorId, rszPointer))
    {
         RIL_LOG_CRITICAL("CSilo_MISC::ParseXDRVI() - Unable to parse <temp_sensor_id>!\r\n");
         goto Error;
    }

    // Parse <temp>
    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractUInt32(rszPointer, nTemp, rszPointer))
    {
         RIL_LOG_CRITICAL("CSilo_MISC::ParseXDRVI() - Unable to parse <temp>!\r\n");
         goto Error;
    }

    RIL_LOG_INFO("CSilo_MISC::ParseXDRVI - IPC_CHR_GRP: %u, IPC_CHR_TEMP_THRESHOLD_IND: %u,"
            " xdrv_result: %u\r\n", nIpcChrGrp, nIpcChrTempThresholdInd, nXdrvResult);
    RIL_LOG_INFO("CSilo_MISC::ParseXDRVI - temp_sensor_id: %u, temp: %u\r\n", nSensorId, nTemp);

    pData = (sOEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND*)malloc(
            sizeof(sOEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND));
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CSilo_MISC::ParseXDRVI() - Could not allocate memory for pData.\r\n");
        goto Error;
    }
    memset(pData, 0, sizeof(sOEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND));

    pData->nCommand = RIL_OEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND;
    pData->nSensorId = nSensorId;
    pData->nTemp = nTemp;

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_OEM_HOOK_RAW);

    if (!pResponse->SetData((void*)pData, sizeof(sOEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND), FALSE))
    {
        goto Error;
    }

    fRet = TRUE;

    rszPointer = pszEnd - strlen(m_szNewLine);

Error:
    if (!fRet)
    {
        free(pData);
        pData = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_MISC::ParseXDRVI() - Exit\r\n");
    return fRet;
}

