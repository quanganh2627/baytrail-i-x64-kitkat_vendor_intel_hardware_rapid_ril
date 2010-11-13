////////////////////////////////////////////////////////////////////////////
// te_sw_8790.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    TE overlay for the Sierra Wireless 8790
//
// Author:  Mike Worth
// Created: 2009-09-30
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Sept 30/09  MW      1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "nd_structs.h"
#include "../../util.h"
#include "extract.h"
#include "rillog.h"
#include "te_sw_8790.h"
#include "../../globals.h"
#include "te_base.h"


#include <cutils/properties.h>
#include <sys/system_properties.h>

CTE_SW_8790::CTE_SW_8790()
{
}

CTE_SW_8790::~CTE_SW_8790()
{
}

//
// RIL_REQUEST_OPERATOR 22
//
RIL_RESULT_CODE CTE_SW_8790::ParseOperator(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_SW_8790::ParseOperator() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const BYTE* pszRsp = rRspData.szResponse;
    UINT32    uiMode    = 0;
    UINT32    uiFormat  = 0;
    
    extern ACCESS_TECHNOLOGY g_uiAccessTechnology;        

    P_ND_OP_NAMES pOpNames = NULL;

    pOpNames = (P_ND_OP_NAMES) malloc(sizeof(S_ND_OP_NAMES));
    if (NULL == pOpNames)
    {
        RIL_LOG_CRITICAL("CTE_SW_8790::ParseOperator() - ERROR: Could not allocate memory for S_ND_OP_NAMES struct.\r\n");
        goto Error;
    }
    memset(pOpNames, 0, sizeof(S_ND_OP_NAMES));

    // Parse "<prefix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_SW_8790::ParseOperator() - ERROR: Could not skip response prefix.\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CTE_SW_8790::ParseOperator() - Response: %s\r\n", CRLFExpandedString(pszRsp, strlen(pszRsp)).GetString());

    // Skip "+COPS: "
    while (SkipString(pszRsp, "+COPS: ", pszRsp))
    {
        // Extract "<mode>"
        if (!ExtractUInt(pszRsp, uiMode, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_SW_8790::ParseOperator() - ERROR: Could not extract <mode>.\r\n");
            goto Error;
        }

        // Extract ",<format>,<oper>" - these are optional
        if (SkipString(pszRsp, ",", pszRsp))
        {
            if (!ExtractUInt(pszRsp, uiFormat, pszRsp) ||
                !SkipString(pszRsp, ",", pszRsp))
            {
                RIL_LOG_CRITICAL("CTE_SW_8790::ParseOperator() - ERROR: Could not extract <format>.\r\n");
                goto Error;
            }


            switch (uiFormat)
            {
                case 0:
                {
                    // SWI provides the name in ASCII
                    if (!ExtractQuotedString(pszRsp, pOpNames->szOpNameLong, MAX_BUFFER_SIZE, pszRsp))
                    {
                        RIL_LOG_CRITICAL("CTE_SW_8790::ParseOperator() - ERROR - Failed to extract long name\r\n");
                        goto Error;
                    }
                    RIL_LOG_INFO("CTE_SW_8790::ParseOperator() - Long oper: %s\r\n", pOpNames->szOpNameLong);
                }
                break;

                case 1:
                {
                    // SWI provides the name in ASCII
                    if (!ExtractQuotedString(pszRsp, pOpNames->szOpNameShort, MAX_BUFFER_SIZE, pszRsp))
                    {
                        RIL_LOG_CRITICAL("CTE_SW_8790::ParseOperator() - ERROR - Failed to extract short name\r\n");
                        goto Error;
                    }
                    RIL_LOG_INFO("CTE_SW_8790::ParseOperator() - Short oper: %s\r\n", pOpNames->szOpNameShort);
                }
                break;

                case 2:
                {
                    if (!ExtractQuotedString(pszRsp, pOpNames->szOpNameNumeric, MAX_BUFFER_SIZE, pszRsp))
                    {
                        RIL_LOG_CRITICAL("CTE_SW_8790::ParseOperator() - ERROR - Failed to extract numeric\r\n");
                        goto Error;
                    }

                    // if we're in limited service, the numeric contains the
                    // string "Limited Service" -> nuke it
                    const BYTE* szLimitedService = "Limited Service";
                    if (0 == strncmp(pOpNames->szOpNameNumeric, szLimitedService, strlen(szLimitedService)))
                    {
                        pOpNames->szOpNameNumeric[0] = '\0';
                    }
                    RIL_LOG_INFO("CTE_SW_8790::ParseOperator() - Numeric: %s\r\n", pOpNames->szOpNameNumeric);
                }
                break;

                default:
                {
                    RIL_LOG_CRITICAL("CTE_SW_8790::ParseOperator() - ERROR: Unknown Format.\r\n");
                    goto Error;
                }
                break;
            }

            if (SkipString(pszRsp, ",", pszRsp))
            {
                uint act;
                ExtractUInt(pszRsp, act, pszRsp);
                switch (act)
                {
                    case 0:
                        g_uiAccessTechnology = ACT_GSM;
                        break;
                    case 1:
                        g_uiAccessTechnology = ACT_GSM_COMPACT;
                        break;
                    case 2:
                        g_uiAccessTechnology = ACT_UMTS;
                        break;
                    default:
                        g_uiAccessTechnology = ACT_UNKNOWN;
                }
            }
            else
            {
                g_uiAccessTechnology = ACT_UNKNOWN;
            }

            pOpNames->sOpNamePtrs.pszOpNameLong    = pOpNames->szOpNameLong;
            pOpNames->sOpNamePtrs.pszOpNameShort   = pOpNames->szOpNameShort;
            pOpNames->sOpNamePtrs.pszOpNameNumeric = pOpNames->szOpNameNumeric;
            
            RIL_LOG_INFO("CTE_SW_8790::ParseOperator() - Long    Name: \"%s\"\r\n", pOpNames->sOpNamePtrs.pszOpNameLong);
            RIL_LOG_INFO("CTE_SW_8790::ParseOperator() - Short   Name: \"%s\"\r\n", pOpNames->sOpNamePtrs.pszOpNameShort);
            RIL_LOG_INFO("CTE_SW_8790::ParseOperator() - Numeric Name: \"%s\"\r\n", pOpNames->sOpNamePtrs.pszOpNameNumeric);
        }
        else
        {
            RIL_LOG_INFO("CTE_SW_8790::ParseOperator() - <format> and <oper> not present.\r\n");
            pOpNames->sOpNamePtrs.pszOpNameLong    = NULL;
            pOpNames->sOpNamePtrs.pszOpNameShort   = NULL;
            pOpNames->sOpNamePtrs.pszOpNameNumeric = NULL;
            g_uiAccessTechnology = ACT_UNKNOWN;
        }

        // Extract "<CR><LF>"
        if (!SkipRspEnd(pszRsp, g_szNewLine, pszRsp))
        {
            RIL_LOG_CRITICAL("CTE_SW_8790::ParseOperator() - ERROR: Could not extract response postfix.\r\n");
            goto Error;
        }

        // If we have another line to parse, get rid of its prefix now.
        // Note that this will do nothing if we don't have another line to parse.

        SkipRspStart(pszRsp, g_szNewLine, pszRsp);

        RIL_LOG_INFO("CTE_SW_8790::ParseOperator() - Response: %s\r\n", CRLFExpandedString(pszRsp, strlen(pszRsp)).GetString());
    }

    // We cheat with the size here.
    // Although we have allocated a S_ND_OP_NAMES struct, we tell
    // Android that we have only allocated a S_ND_OP_NAME_POINTERS
    // struct since Android is expecting to receive an array of string pointers.

    rRspData.pData  = pOpNames;
    rRspData.uiDataSize = sizeof(S_ND_OP_NAME_POINTERS);
    
    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pOpNames);
        pOpNames = NULL;
    }

    RIL_LOG_VERBOSE("CTE_SW_8790::ParseOperator() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SETUP_DATA_CALL 27
// TODO - this needs to be updated to use the Context ID passed in, instead of hardcoding it to 1
RIL_RESULT_CODE CTE_SW_8790::CoreSetupDataCall(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize, UINT32 uiCID)
{
    RIL_LOG_VERBOSE("CTE_SW_8790::CoreSetupDataCall() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    PdpData stPdpData;
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_SW_8790::CoreSetupDataCall() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (6 * sizeof(char*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_SW_8790::CoreSetupDataCall() - ERROR: Invalid data size. Was given %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    // extract data
    stPdpData.szRadioTechnology = ((char **)pData)[0];  // not used
    stPdpData.szRILDataProfile  = ((char **)pData)[1];  // not used
    stPdpData.szApn             = ((char **)pData)[2];
    stPdpData.szUserName        = ((char **)pData)[3];  // not used
    stPdpData.szPassword        = ((char **)pData)[4];  // not used
    stPdpData.szPAPCHAP         = ((char **)pData)[5];  // not used

    (void)PrintStringNullTerminate(rReqData.szCmd1,
          sizeof(rReqData.szCmd1),
          "AT+CGDCONT=1,\"IP\",\"%s\",,0,0;+CGQREQ=1;+CGQMIN=1;+CGACT=0,1\r",
          stPdpData.szApn);
        
    //(void)CopyStringNullTerminate(rReqData.szCmd2, "ATD*99***1#\r", sizeof(rReqData.szCmd2));
    
    res = RRIL_RESULT_OK;

Error:
    RIL_LOG_VERBOSE("CTE_SW_8790::CoreSetupDataCall() - Exit\r\n");
    return res;
    
}


RIL_RESULT_CODE CTE_SW_8790::ParseSetupDataCall(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_SW_8790::ParseSetupDataCall() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;


    char szOperstate[PROPERTY_VALUE_MAX];
    char szIP[PROPERTY_VALUE_MAX];
    char* pszDefault = NULL;
    BOOL fPPPUp = FALSE;
    const unsigned int nSleep = 2000;
    const int nRetries = 29;

    property_set("ctl.start", "pppd_gprs");

    for (int i = 0; i < nRetries; i++)
    {
        RIL_LOG_INFO("CTE_SW_8790::ParseSetupDataCall() - INFO: Waiting %d seconds for ppp connection. (Attempt #%d/%d)\r\n",
                    nSleep/1000, i + 1, nRetries);
        Sleep(nSleep);

        property_get("net.gprs.operstate", szOperstate, pszDefault);
        RIL_LOG_INFO("CTE_SW_8790::ParseSetupDataCall() - INFO: net.grps.operstate=[%s]\r\n", szOperstate);

        if (0 == strcmp(szOperstate, "up"))
        {
            RIL_LOG_INFO("CTE_SW_8790::ParseSetupDataCall() - INFO: ppp connection is up!\r\n");
            fPPPUp = TRUE;
            break;
        }
        else
        {
            RIL_LOG_CRITICAL("CTE_SW_8790::ParseSetupDataCall() - ERROR: ppp connection is not up\r\n");
        }
    }

    if (fPPPUp)
    {
        property_get("net.grps.local-ip", szIP, pszDefault);
        RIL_LOG_INFO("CTE_SW_8790::ParseSetupDataCall() - INFO: net.grps.local-ip=[%s]\r\n", szIP);

        P_ND_SETUP_DATA_CALL pDataCallRsp = (P_ND_SETUP_DATA_CALL)malloc(sizeof(S_ND_SETUP_DATA_CALL));
        strcpy(pDataCallRsp->szCID, "1");
        strcpy(pDataCallRsp->szNetworkInterfaceName, "FakeInterfaceName");
        strcpy(pDataCallRsp->szIPAddress, szIP);
        pDataCallRsp->sSetupDataCallPointers.pszCID = pDataCallRsp->szCID;
        pDataCallRsp->sSetupDataCallPointers.pszNetworkInterfaceName = pDataCallRsp->szNetworkInterfaceName;
        pDataCallRsp->sSetupDataCallPointers.pszIPAddress = pDataCallRsp->szIPAddress;
        rRspData.pData = (void*)pDataCallRsp;
        rRspData.uiDataSize = sizeof(S_ND_SETUP_DATA_CALL_POINTERS);
        res = RRIL_RESULT_OK;
    }


    RIL_LOG_INFO("CTE_SW_8790::ParseSetupDataCall() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_DEACTIVATE_DATA_CALL 41
//
RIL_RESULT_CODE CTE_SW_8790::CoreDeactivateDataCall(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_SW_8790::CoreDeactivateDataCall() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char * pszCid = 0;
    char chCid = '1';

    if (sizeof(char **) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_SW_8790::CoreDeactivateDataCall() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_SW_8790::CoreDeactivateDataCall() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
       
    pszCid = ((char **)pData)[0];
    chCid = pszCid[0];
  
    //if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CGACT=0,%c;+CGDCONT=%c\r", chCid, chCid))
    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT+CGDCONT=%c\r", chCid))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTE_SW_8790::CoreDeactivateDataCall() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_SW_8790::ParseDeactivateDataCall(RESPONSE_DATA & rRspData)
{
    RIL_LOG_INFO("CTE_SW_8790::ParseDeactivateDataCall() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    char szOperstate[PROPERTY_VALUE_MAX];
    char* pszDefault = NULL;
    BOOL fPPPDown = FALSE;
    const unsigned int nSleep = 2000;
    const int nRetries = 29;

    property_set("ctl.stop", "pppd_gprs");

    for (int i = 0; i < nRetries; i++)
    {
        RIL_LOG_INFO("CTE_SW_8790::ParseDeactivateDataCall() - INFO: Waiting %d seconds for stop ppp connection. (Attempt #%d/%d)\r\n",
                    nSleep/1000, i + 1, nRetries);
        Sleep(nSleep);

        property_get("net.gprs.operstate", szOperstate, pszDefault);
        RIL_LOG_INFO("CTE_SW_8790::ParseDeactivateDataCall() - INFO: net.grps.operstate=[%s]\r\n", szOperstate);

        if (0 == strcmp(szOperstate, "down"))
        {
            RIL_LOG_INFO("CTE_SW_8790::ParseDeactivateDataCall() - INFO: ppp connection is down!\r\n");
            fPPPDown = TRUE;
            break;
        }
        else
        {
            RIL_LOG_CRITICAL("CTE_SW_8790::ParseDeactivateDataCall() - ERROR: ppp connection is not down\r\n");
        }
    }

    if (fPPPDown)
    {
        res = RRIL_RESULT_OK;
    }


    RIL_LOG_INFO("CTE_SW_8790::ParseDeactivateDataCall() - Exit\r\n");
    return res;

}


//
// RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE 45
//
RIL_RESULT_CODE CTE_SW_8790::CoreQueryNetworkSelectionMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_SW_8790::CoreQueryNetworkSelectionMode() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_OK;

    //  [DP] Let's NO-OP this command.
    rReqData.szCmd1[0] = NULL;

    RIL_LOG_VERBOSE("CTE_SW_8790::CoreQueryNetworkSelectionMode() - Exit\r\n");
    return res;    
}


RIL_RESULT_CODE CTE_SW_8790::ParseQueryNetworkSelectionMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_SW_8790::ParseQueryNetworkSelectionMode() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int * pnMode = NULL;

    pnMode = (int*)malloc(sizeof(int));
    if (NULL == pnMode)
    {
        RIL_LOG_CRITICAL("ParseQueryNetworkSelectionMode() - ERROR: Could not allocate memory for an int.\r\n");
        goto Error;
    }
    
    *pnMode = 0;  // automatic

    res = RRIL_RESULT_OK;

    rRspData.pData   = (void*)pnMode;
    rRspData.uiDataSize  = sizeof(int *);

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pnMode);
        pnMode = NULL;
    }

    RIL_LOG_VERBOSE("CTE_SW_8790::ParseQueryNetworkSelectionMode() - Exit\r\n");
    return res;
}


//
// RIL_REQUEST_QUERY_AVAILABLE_NETWORKS 48
//
RIL_RESULT_CODE CTE_SW_8790::ParseQueryAvailableNetworks(RESPONSE_DATA& rRspData)
{
    RIL_LOG_VERBOSE("CTE_SW_8790::ParseQueryAvailableNetworks() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    UINT32 uiValue;
    UINT32 uiEntries = 0;
    UINT32 uiCurrent = 0;

    P_ND_OPINFO_PTRS pOpInfoPtr = NULL;
    P_ND_OPINFO_DATA pOpInfoData = NULL;

    void * pOpInfoPtrBase = NULL;
    void * pOpInfoDataBase = NULL;

    const BYTE* szRsp = rRspData.szResponse;
    const BYTE* szDummy = NULL;
    
    // Skip "<prefix>+COPS: "
    SkipRspStart(szRsp, g_szNewLine, szRsp);
    
    if (!FindAndSkipString(szRsp, "+COPS: ", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_SW_8790::ParseQueryAvailableNetworks() - ERROR: Unable to find +COPS: in response\r\n");
        goto Error;
    }

    szDummy = szRsp;

    while (FindAndSkipString(szDummy, "(", szDummy))
    {
        uiEntries++;
    }

    // SWI provides the modes and formats, hence do not count the last 2 entries
    uiEntries -= 2;
    
    RIL_LOG_VERBOSE("CTE_SW_8790::ParseQueryAvailableNetworks() - DEBUG: Found %d entries. Allocating memory...\r\n", uiEntries);

    rRspData.pData = malloc(uiEntries * (sizeof(S_ND_OPINFO_PTRS) + sizeof(S_ND_OPINFO_DATA)));
    memset(rRspData.pData, 0, uiEntries * (sizeof(S_ND_OPINFO_PTRS) + sizeof(S_ND_OPINFO_DATA)));
    rRspData.uiDataSize = uiEntries * sizeof(S_ND_OPINFO_PTRS);

    pOpInfoPtrBase = rRspData.pData;
    pOpInfoDataBase = (void*)((UINT32)rRspData.pData + (uiEntries * sizeof(S_ND_OPINFO_PTRS)));

    pOpInfoPtr = (P_ND_OPINFO_PTRS)pOpInfoPtrBase;
    pOpInfoData = (P_ND_OPINFO_DATA)pOpInfoDataBase;

    // Skip "("
    while (SkipString(szRsp, "(", szRsp))
    {
        // Extract "<stat>"
        if (!ExtractUInt(szRsp, uiValue, szRsp))
        {
            RIL_LOG_CRITICAL("CTE_SW_8790::ParseGetOperatorList() - ERROR: Unable to extract status\r\n");
            goto Error;
        }

        switch(uiValue)
        {
            case 0:
            {
                const BYTE* const szTemp = "unknown";
                strcpy(pOpInfoData[uiCurrent].szOpInfoStatus, szTemp);
                pOpInfoPtr[uiCurrent].pszOpInfoStatus = pOpInfoData[uiCurrent].szOpInfoStatus;
                break;
            }

            case 1:
            {
                const BYTE* const szTemp = "available";
                strcpy(pOpInfoData[uiCurrent].szOpInfoStatus, szTemp);
                pOpInfoPtr[uiCurrent].pszOpInfoStatus = pOpInfoData[uiCurrent].szOpInfoStatus;
                break;
            }

            case 2:
            {
                const BYTE* const szTemp = "current";
                strcpy(pOpInfoData[uiCurrent].szOpInfoStatus, szTemp);
                pOpInfoPtr[uiCurrent].pszOpInfoStatus = pOpInfoData[uiCurrent].szOpInfoStatus;
                break;
            }

            case 3:
            {
                const BYTE* const szTemp = "forbidden";
                strcpy(pOpInfoData[uiCurrent].szOpInfoStatus, szTemp);
                pOpInfoPtr[uiCurrent].pszOpInfoStatus = pOpInfoData[uiCurrent].szOpInfoStatus;
                break;
            }

            default:
            {
                RIL_LOG_CRITICAL("CTE_SW_8790::ParseGetOperatorList() - ERROR: Invalid status found: %d\r\n", uiValue);
                goto Error;
            }
        }

        // Extract ",<long_name>"
        if (!SkipString(szRsp, ",", szRsp) ||
           (!ExtractQuotedString(szRsp, pOpInfoData[uiCurrent].szOpInfoLong, MAX_BUFFER_SIZE, szRsp)))
        {
            RIL_LOG_CRITICAL("CTE_SW_8790::ParseGetOperatorList() - ERROR: Could not extract the Long Format Operator Name.\r\n");
            goto Error;
        }
        else
        {
            pOpInfoPtr[uiCurrent].pszOpInfoLong = pOpInfoData[uiCurrent].szOpInfoLong;
            RIL_LOG_INFO("CTE_SW_8790::ParseGetOperatorList() - Long oper: %s\r\n", pOpInfoData[uiCurrent].szOpInfoLong);
        }

        // Extract ",<short_name>"
        if (!SkipString(szRsp, ",", szRsp) ||
           (!ExtractQuotedString(szRsp, pOpInfoData[uiCurrent].szOpInfoShort, MAX_BUFFER_SIZE, szRsp)))
        {
            RIL_LOG_CRITICAL("CTE_SW_8790::ParseGetOperatorList() - ERROR: Could not extract the Short Format Operator Name.\r\n");
            goto Error;
        }
        else
        {
            pOpInfoPtr[uiCurrent].pszOpInfoShort = pOpInfoData[uiCurrent].szOpInfoShort;
            RIL_LOG_INFO("CTE_SW_8790::ParseGetOperatorList() - Short oper: %s\r\n", pOpInfoData[uiCurrent].szOpInfoShort);
        }

        // Extract ",<num_name>"
        if (!SkipString(szRsp, ",", szRsp) ||
           (!ExtractQuotedString(szRsp, pOpInfoData[uiCurrent].szOpInfoNumeric, MAX_BUFFER_SIZE, szRsp)))
        {
            RIL_LOG_CRITICAL("CTE_SW_8790::ParseGetOperatorList() - ERROR: Could not extract the Numeric Format Operator Name.\r\n");
            goto Error;
        }
        else
        {
            pOpInfoPtr[uiCurrent].pszOpInfoNumeric = pOpInfoData[uiCurrent].szOpInfoNumeric;
            RIL_LOG_INFO("CTE_SW_8790::ParseGetOperatorList() - Numeric oper: %s\r\n", pOpInfoData[uiCurrent].szOpInfoNumeric);
        }

        //Extract ")"
        if (!FindAndSkipString(szRsp, ")", szRsp))
        {
            RIL_LOG_CRITICAL("CTE_SW_8790::ParseGetOperatorList() - ERROR: Did not find closing bracket\r\n");
            goto Error;
        }

        // Increment the array index
        uiCurrent++;

Continue:
        // Extract ","
        if (!FindAndSkipString(szRsp, ",", szRsp))
        {
            RIL_LOG_INFO("CTE_SW_8790::ParseGetOperatorList() - INFO: Finished parsing entries\r\n");
            break;
        }
    }

    // NOTE: there may be more data here, but we don't care about it

    // Find "<postfix>"
    if (!FindAndSkipRspEnd(szRsp, g_szNewLine, szRsp))
    {
        goto Error;
    }

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(rRspData.pData);
        rRspData.pData   = NULL;
        rRspData.uiDataSize  = 0;
    }

    RIL_LOG_VERBOSE("CTE_SW_8790::ParseQueryAvailableNetworks() - Exit\r\n");
    return res;
}


//
// RIL_REQUEST_SET_BAND_MODE 65
//
RIL_RESULT_CODE CTE_SW_8790::CoreSetBandMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_SW_8790::CoreSetBandMode() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    UINT32* pnBandMode;

    if (sizeof(int*) != uiDataSize)
    {
        RIL_LOG_CRITICAL("CTE_SW_8790::CoreSetBandMode() - ERROR: Passed data size mismatch. Found %d bytes\r\n", uiDataSize);
        goto Error;
    }
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_SW_8790::CoreSetBandMode() - ERROR: Passed data pointer was NULL\r\n");
        goto Error;
    }
    
    pnBandMode = (UINT32*)pData;

    switch(*pnBandMode)
    {
    case 0:
    case 1:
    case 2:
    case 4:
    case 5:
        if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT!BAND=00\r"))
        {
            res = RRIL_RESULT_OK;
        }
        break;

    case 3:
    case 6:
        if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT!BAND=02\r"))
        {
            res = RRIL_RESULT_OK;
        }
        break;

    case 7:
        if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT!BAND=04\r"))
        {
            res = RRIL_RESULT_OK;
        }
        break;

    default:
        RIL_LOG_CRITICAL("CTE_SW_8790::CoreSetBandMode() - ERROR: Unsupported region code: %d\r\n", *pnBandMode);
        res = RIL_E_GENERIC_FAILURE;
        break;
    }

Error:
    RIL_LOG_VERBOSE("CTE_SW_8790::CoreSetBandMode() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE 66
//
RIL_RESULT_CODE CTE_SW_8790::CoreQueryAvailableBandMode(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_SW_8790::CoreQueryAvailableBandMode() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;

    if (PrintStringNullTerminate(rReqData.szCmd1, sizeof(rReqData.szCmd1), "AT!BAND=?\r"))
    {
        res = RRIL_RESULT_OK;
    }

Error:
    RIL_LOG_VERBOSE("CTE_SW_8790::CoreQueryAvailableBandMode() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_SW_8790::ParseQueryAvailableBandMode(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_SW_8790::ParseQueryAvailableBandMode() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const BYTE* szRsp = rRspData.szResponse;

    BOOL automatic = FALSE;
    BOOL euro = FALSE;
    BOOL usa = FALSE;
    BOOL japan = FALSE;
    BOOL aus = FALSE;
    BOOL aus2 = FALSE;
    UINT32 count = 0;
    int *pModes = NULL;

    // Skip "Index, Name"
    if (!FindAndSkipString(szRsp, "Index, Name", szRsp))
    {
        RIL_LOG_CRITICAL("CTE_SW_8790::ParseQueryAvailableBandMode() - ERROR: Could not skip \"Index, Name\".\r\n");
        goto Error;
    }

    SkipRspStart(szRsp, g_szNewLine, szRsp);
    BYTE szCurBand[MAX_BUFFER_SIZE];
    while (ExtractUnquotedString(szRsp, "\r\n", szCurBand, MAX_BUFFER_SIZE, szRsp))
    {
        UINT32 uiBand;
        const BYTE* szDummy;
        if (!ExtractUInt(szCurBand, uiBand, szDummy))
        {
            RIL_LOG_CRITICAL("CTE_SW_8790::ParseQueryAvailableBandMode() - ERROR: Could not extract band #\r\n");
            goto Error;
        }
        switch (uiBand)
        {
            case 0x00:
            case 0x02:
            case 0x04:
            case 0x06:
            case 0x07:
            case 0x09:
            case 0x0A:
            case 0x0B:
            case 0x0C:
            case 0x0D:
            case 0x0E:
            case 0x0F:
            case 0x10:
            case 0x11:
                // Automatic
                automatic = TRUE;
                break;

            case 0x01:
                // 2100 -> no entry in RIL.H for this
                break;

            case 0x03:  // GSM 900/1800
                euro = TRUE;
                aus = TRUE;
                aus2 = TRUE;
                break;

            case 0x05:  // GSM all
                euro = TRUE;
                aus = TRUE;
                aus2 = TRUE;
                usa = TRUE;
                break;

            case 0x08:  // WCDMA all
                euro = TRUE;
                aus = TRUE;
                aus2 = TRUE;
                usa = TRUE;
                break;

            default:
                RIL_LOG_CRITICAL("CTE_SW_8790::ParseQueryAvailableBandMode() - ERROR: Unknown band [%d]\r\n", uiBand);
                goto Error;
                break;

        }
    }

    // pModes is an array of ints where the first is the number of ints to follow
    // then an int representing each available band mode where:
    /* 0 for "unspecified" (selected by baseband automatically)
    *  1 for "EURO band" (GSM-900 / DCS-1800 / WCDMA-IMT-2000)
    *  2 for "US band" (GSM-850 / PCS-1900 / WCDMA-850 / WCDMA-PCS-1900)
    *  3 for "JPN band" (WCDMA-800 / WCDMA-IMT-2000)
    *  4 for "AUS band" (GSM-900 / DCS-1800 / WCDMA-850 / WCDMA-IMT-2000)
    *  5 for "AUS band 2" (GSM-900 / DCS-1800 / WCDMA-850)
    */

    pModes = (int*)malloc(7 * sizeof(int));
    if (NULL == pModes)
    {
        RIL_LOG_CRITICAL("CTE_SW_8790::ParseQueryAvailableBandMode() - ERROR: Could not allocate memory for response.\r\n");
        goto Error;
    }

    count = 0;
    if (automatic)
    {
        ++count;
        pModes[count] = 0;
    }

    if (euro)
    {
        ++count;
        pModes[count] = 1;
    }

    if (usa)
    {
        ++count;
        pModes[count] = 2;
    }

    if (japan)
    {
        ++count;
        pModes[count] = 3;
    }

    if (aus)
    {
        ++count;
        pModes[count] = 4;
    }

    if (aus2)
    {
        ++count;
        pModes[count] = 5;
    }

    pModes[0] = count;

    res = RRIL_RESULT_OK;

    rRspData.pData   = (void *) pModes;
    rRspData.uiDataSize  = sizeof (int *);

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pModes);
        pModes = NULL;
    }

    RIL_LOG_VERBOSE("CTE_SW_8790::ParseQueryAvailableBandMode() - Exit\r\n");
    return res;
}



//
// RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
//
RIL_RESULT_CODE CTE_SW_8790::CoreSetPreferredNetworkType(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_SW_8790::CoreSetPreferredNetworkType() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    int nNetworkType = 0;
    
    if (NULL == pData)
    {
        RIL_LOG_CRITICAL("CTE_SW_8790::CoreSetPreferredNetworkType() - ERROR: Data pointer is NULL.\r\n");
        goto Error;
    }

    if (uiDataSize != sizeof(int *))
    {
        RIL_LOG_CRITICAL("CTE_SW_8790::CoreSetPreferredNetworkType() - ERROR: Invalid data size.\r\n");
        goto Error;
    }
    
    nNetworkType = ((int *) pData)[0];
    switch (nNetworkType)
    {
        case 0: // WCMDA Preferred
            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT!BAND=00\r", sizeof(rReqData.szCmd1)))
            {
                RIL_LOG_CRITICAL("CTE_SW_8790::CoreSetPreferredNetworkType() - ERROR: Can't construct szCmd1 nNetworkType=%d\r\n", nNetworkType);
                goto Error;        
            }
           break;

        case 1: // GSM Only
            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT!BAND=05\r", sizeof(rReqData.szCmd1)))
            {
                RIL_LOG_CRITICAL("CTE_SW_8790::CoreSetPreferredNetworkType() - ERROR: Can't construct szCmd1 nNetworkType=%d\r\n", nNetworkType);
                goto Error;        
            }
            break;

        case 2: // WCDMA Only
            if (!CopyStringNullTerminate(rReqData.szCmd1, "AT!BAND=02\r", sizeof(rReqData.szCmd1)))
            {
                RIL_LOG_CRITICAL("CTE_SW_8790::CoreSetPreferredNetworkType() - ERROR: Can't construct szCmd1 nNetworkType=%d\r\n", nNetworkType);
                goto Error;        
            }
            break;

        default:
            RIL_LOG_CRITICAL("CTE_SW_8790::CoreSetPreferredNetworkType() - ERROR: Undefined rat code: %d\r\n", nNetworkType);
            res = RIL_E_GENERIC_FAILURE;
            goto Error;
            break;
    }
    
    res = RRIL_RESULT_OK;
    
Error:    
    RIL_LOG_VERBOSE("CTE_SW_8790::CoreSetPreferredNetworkType() - Exit\r\n");
    return res;
}



//
// RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
//
RIL_RESULT_CODE CTE_SW_8790::CoreGetPreferredNetworkType(REQUEST_DATA & rReqData, void * pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_SW_8790::CoreGetPreferredNetworkType() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    
    if (CopyStringNullTerminate(rReqData.szCmd1, "AT!BAND?\r", sizeof(rReqData.szCmd1)))
    {
        res = RRIL_RESULT_OK;
    }
    
    RIL_LOG_VERBOSE("CTE_SW_8790::CoreGetPreferredNetworkType() - Exit\r\n");
    return res;
}

RIL_RESULT_CODE CTE_SW_8790::ParseGetPreferredNetworkType(RESPONSE_DATA & rRspData)
{
    RIL_LOG_VERBOSE("CTE_SW_8790::ParseGetPreferredNetworkType() - Enter\r\n");

    RIL_RESULT_CODE res = RRIL_RESULT_ERROR;
    const char * pszRsp = rRspData.szResponse;
    
    UINT32 uiPref = 0;

    int * pRat = (int *) malloc(sizeof(int));
    if (NULL == pRat)
    {
        RIL_LOG_CRITICAL("CTE_SW_8790::ParseGetPreferredNetworkType() - ERROR: Could not allocate memory for response.\r\n");
        goto Error;
    }

    // Skip "<prefix>"
    if (!SkipRspStart(pszRsp, g_szNewLine, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_SW_8790::ParseGetPreferredNetworkType() - ERROR: Could not skip response prefix.\r\n");
        goto Error;
    }

    // Skip "!BAND: "
    if (!SkipString(pszRsp, "!BAND: ", pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_SW_8790::ParseGetPreferredNetworkType() - ERROR: Could not skip \"!BAND: \".\r\n");
        goto Error;
    }

    SkipRspStart(pszRsp, g_szNewLine, pszRsp);
    if (!ExtractUInt(pszRsp, uiPref, pszRsp))
    {
        RIL_LOG_CRITICAL("CTE_SW_8790::ParseGetPreferredNetworkType() - ERROR: Could not extract rat value.\r\n");
        goto Error;
    }

    switch (uiPref)
    {
        case 0: // All bands
        case 1: // N/A (default to All)
        case 6: // N/A (default to All)
        case 7: // N/A (default to All)
            pRat[0] = 0;
            break;

        case 2: // WCDMA 800/1900
            pRat[0] = 2;
            break;

        case 3: // GSM 900/1800
        case 4: // GSM 850/1900
        case 5: // GSM all
            pRat[0] = 1;
            break;

        default:
            RIL_LOG_CRITICAL("CTE_SW_8790::ParseGetPreferredNetworkType() - ERROR: Unexpected value found: %d. Failing out.\r\n", uiPref);
            goto Error;
            break;
    }

    rRspData.pData  = (void*)pRat;
    rRspData.uiDataSize = sizeof(int*);

    // NOTE: there may be more data here, but we don't care about it
    // Find "<postfix>"
    while(FindAndSkipRspEnd(pszRsp, g_szNewLine, pszRsp));

    res = RRIL_RESULT_OK;

Error:
    if (RRIL_RESULT_OK != res)
    {
        free(pRat);
        pRat = NULL;
    }


    RIL_LOG_VERBOSE("CTE_SW_8790::ParseGetPreferredNetworkType() - Exit\r\n");
    return res;
}

