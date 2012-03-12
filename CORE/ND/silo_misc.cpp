/* silo_misc.cpp
**
** Copyright (C) Intel 2012
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** Description:
**    Provides response handlers and parsing functions for misc features
**
** Author: Frederic Predon <frederic.predon@intel.com>
*/

#include "types.h"
#include "rillog.h"
#include "response.h"
#include "rildmain.h"
#include "callbacks.h"
#include "silo_misc.h"
#include "extract.h"
#include "oemhookids.h"

#include <arpa/inet.h>

//
//
CSilo_MISC::CSilo_MISC(CChannel *pChannel)
: CSilo(pChannel)
{
    RIL_LOG_VERBOSE("CSilo_MISC::CSilo_MISC() - Enter\r\n");

    // AT Response Table
    static ATRSPTABLE pATRspTable[] =
    {
        { "+XDRVI: "   , (PFN_ATRSP_PARSE)&CSilo_MISC::ParseXDRVI  },
        { ""         , (PFN_ATRSP_PARSE)&CSilo_MISC::ParseNULL }
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


//
// Thermal sensor notification
//
BOOL CSilo_MISC::ParseXDRVI(CResponse * const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_MISC::ParseXDRVI() - Enter\r\n");
    BOOL   fRet = FALSE;
    const char* pszEnd = NULL;
    unsigned char*  pszData = NULL;
    UINT32 nIpcChrGrp;
    UINT32 nIpcChrTempThresholdInd;
    UINT32 nXdrvResult;
    UINT32 nSensorId;
    UINT32 nTemp;
    int pos = 0;

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_MISC::ParseXDRVI() - pResponse is NULL.\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, g_szNewLine, pszEnd))
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
         RIL_LOG_CRITICAL("CSilo_MISC::ParseXDRVI() - Unable to parse <IPC_CHR_TEMP_THRESHOLD_IND>!\r\n");
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

    RIL_LOG_INFO("CSilo_MISC::ParseXDRVI - IPC_CHR_GRP: %u, IPC_CHR_TEMP_THRESHOLD_IND: %u, xdrv_result: %u\r\n", nIpcChrGrp, nIpcChrTempThresholdInd, nXdrvResult);
    RIL_LOG_INFO("CSilo_MISC::ParseXDRVI - temp_sensor_id: %u, temp: %u\r\n", nSensorId, nTemp);

    pszData = (unsigned char *)malloc(sizeof(sOEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND));
    if (NULL == pszData)
    {
        RIL_LOG_CRITICAL("CSilo_MISC::ParseXDRVI() - Could not allocate memory for pszData.\r\n");
        goto Error;
    }
    memset(pszData, 0, sizeof(sOEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND));

    convertIntToByteArrayAt(pszData, RIL_OEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND, pos);
    pos += sizeof(int);
    convertIntToByteArrayAt(pszData, nSensorId, pos);
    pos += sizeof(int);
    convertIntToByteArrayAt(pszData, nTemp, pos);

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_OEM_HOOK_RAW);

    if (!pResponse->SetData(pszData, sizeof(sOEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND), FALSE))
    {
        goto Error;
    }

    fRet = TRUE;

Error:
    if (!fRet)
    {
        free(pszData);
        pszData = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_MISC::ParseXDRVI() - Exit\r\n");
    return fRet;
}

