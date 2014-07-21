////////////////////////////////////////////////////////////////////////////
// silo_SIM.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides response handlers and parsing functions for the SIM-related
//    RIL components.
//
/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "types.h"
#include "rillog.h"
#include "rril.h"
#include "ril_result.h"
#include "channel_nd.h"
#include "response.h"
#include "extract.h"
#include "callbacks.h"
#include "rildmain.h"
#include "silo_sim.h"
#include "callbacks.h"
#include "te.h"
#include "reset.h"
#include "ccatprofile.h"
#include "usat_init_state_machine.h"
#include "hardwareconfig.h"
#include "bertlv_util.h"

#include <cutils/properties.h>

//
//
CSilo_SIM::CSilo_SIM(CChannel* pChannel)
: CSilo(pChannel)
{
    RIL_LOG_VERBOSE("CSilo_SIM::CSilo_SIM() - Enter\r\n");

    // AT Response Table
    static ATRSPTABLE pATRspTable[] =
    {
        { "+STKCTRLIND: "   , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseUnrecognized },
        { "+STKCC: "   , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseUnrecognized },
        { "+SATI: "    , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseIndicationSATI },
        { "+SATN: "    , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseIndicationSATN },
        { "+SATF: "    , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseTermRespConfirm },
        { "+XLOCK: "   , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseXLOCK },
        { "+XSIM: "    , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseXSIM },
        { "+XLEMA: "   , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseXLEMA },
        { "+CUSATS: "   , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseIndicationCusats },
        { "+CUSATP: "   , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseIndicationCusatp },
        { "+CUSATEND"   , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseIndicationCusatend },
        { ""           , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseNULL }
    };

    m_pATRspTable = pATRspTable;

    memset(m_szECCList, 0, sizeof(m_szECCList));

    RIL_LOG_VERBOSE("CSilo_SIM::CSilo_SIM() - Exit\r\n");
}

//
//
CSilo_SIM::~CSilo_SIM()
{
}


char* CSilo_SIM::GetURCInitString()
{
    if (CTE::GetTE().IsVoiceCapable() || CTE::GetTE().IsSmsCapable())
    {
        // SIM silo-related URC channel basic init string
        const char szSimURCInitString[] = "+XLEMA=1";

        if (!ConcatenateStringNullTerminate(m_szURCInitString,
                sizeof(m_szURCInitString), szSimURCInitString))
        {
            RIL_LOG_CRITICAL("CSilo_SIM::GetURCInitString() : Failed to copy URC init "
                    "string!\r\n");
            return NULL;
        }
    }

    if (!ConcatenateStringNullTerminate(m_szURCInitString,
            sizeof(m_szURCInitString), "|+XSIMSTATE=1"))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::GetURCInitString() : Failed to copy XSIMSTATE to URC"
                " init string!\r\n");
        return NULL;
    }

    const char* pszTmp = CTE::GetTE().GetEnableFetchingString();
    if (pszTmp != NULL)
    {
        ConcatenateStringNullTerminate(m_szURCInitString, sizeof(m_szURCInitString), pszTmp);
    }

    return m_szURCInitString;
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
//  Parse functions here
///////////////////////////////////////////////////////////////////////////////////////////////

/**
 * This method checks if the proactive command PDU received is handled by AP and so reported to
 * Android as a proactive command, else it will be considered as a notification.
 * It uses the C-AT Profile class to find out if it is an indication URC or
 * a notification URC.
 * First, the TE profile must be set to the CAT Profile utility class.
 * Then, URC is parsed.
 * Finally, the ProactiveCommandInfo structure gives the type of URC.
 *
 * @param szUrcPointer : The URC string received through +CUSATP command.
 * @param puiCmdId : The command id
 * @return true is the received command is a proactive command
 */
BOOL CSilo_SIM::IsProactiveCmd(const char* szUrcPointer, UINT8* puiCmdId)
{
    RIL_LOG_VERBOSE("CSilo_SIM::IsProactiveCmd() - Enter\r\n");
    BOOL bRet = FALSE;
    CCatProfile::ProactiveCommandInfo info;

    if (!szUrcPointer)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::IsProactiveCmd() - ERROR NULL PARAMETER!\r\n");
        goto Out;
    }

    // Parse given URC
    RIL_LOG_INFO("CSilo_SIM::IsProactiveCmd() : GOT URC:%s\r\n", szUrcPointer);

    if (UsatInitStateMachine::GetStateMachine()
            .GetCatProfile()->ExtractPduInfo(szUrcPointer, strlen(szUrcPointer), &info))
    {
        bRet = info.isProactiveCmd;
    }
    else
    {
        // No proactive command was found in URC.
        // For safety return TRUE to assume this is, indeed, a Proactive command.
        bRet = TRUE;
    }

    *puiCmdId = info.uiCommandId;

Out:
    RIL_LOG_VERBOSE("CSilo_SIM::IsProactiveCmd() : RETURN:%d\r\n - Exit", bRet);
    return bRet;
}

BOOL CSilo_SIM::ParseIndicationSATI(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATI() - Enter\r\n");

    char* pszProactiveCmd = NULL;
    UINT32 uiLength = 0;
    const char* pszEnd = NULL;
    BOOL fRet = FALSE;
    UINT32 uiModemType = CTE::GetTE().GetModemType();

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATI() : pResponse was NULL\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);

    if (MODEM_TYPE_XMM7260 <= uiModemType)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATI() - +SATI not expected for : "
                " %u \r\n", uiModemType);
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, m_szNewLine, pszEnd))
    {
        // incomplete message notification
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATI() : Could not find response end\r\n");
        goto Error;
    }
    else
    {
        // PDU is followed by m_szNewLine, so look for m_szNewLine and use its
        // position to determine length of PDU string.

        // Calculate PDU length + NULL byte
        uiLength = ((UINT32)(pszEnd - rszPointer)) - strlen(m_szNewLine) + 1;
        RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATI() - Calculated PDU String length: %u"
                " chars.\r\n", uiLength);
    }

    pszProactiveCmd = (char*)malloc(sizeof(char) * uiLength);
    if (NULL == pszProactiveCmd)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATI() - Could not alloc mem for"
                " command.\r\n");
        goto Error;
    }
    memset(pszProactiveCmd, 0, sizeof(char) * uiLength);

    // Parse <"hex_string">
    if (!ExtractQuotedString(rszPointer, pszProactiveCmd, uiLength, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATI() - Could not parse hex String.\r\n");
        goto Error;
    }

    // Ensure NULL termination
    pszProactiveCmd[uiLength-1] = '\0';

    RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATI() - Hex String: \"%s\".\r\n", pszProactiveCmd);

    pResponse->SetResultCode(RIL_UNSOL_STK_PROACTIVE_COMMAND);

    if (!pResponse->SetData((void*) pszProactiveCmd, sizeof(char) * uiLength, FALSE))
    {
        goto Error;
    }

    fRet = TRUE;

Error:
    if (!fRet)
    {
        free(pszProactiveCmd);
        pszProactiveCmd = NULL;
    }

    RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATI() - Exit\r\n");
    return fRet;
}

BOOL CSilo_SIM::ParseIndicationSATN(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() - Enter\r\n");
    char* pszProactiveCmd = NULL;
    UINT32 uiLength = 0;
    const char* pszEnd = NULL;
    BOOL fRet = FALSE;
    UINT32 uiModemType = CTE::GetTE().GetModemType();

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATN() : pResponse was NULL\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);

    if (MODEM_TYPE_XMM7260 <= uiModemType)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATN() - +SATN not expected for : "
                " %u \r\n", uiModemType);
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got the whole message
    if (!FindAndSkipRspEnd(rszPointer, m_szNewLine, pszEnd))
    {
        // incomplete message notification
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATN() : Could not find response end\r\n");
        goto Error;
    }
    else
    {
        // PDU is followed by m_szNewLine, so look for m_szNewLine and use its
        // position to determine length of PDU string.

        // Calculate PDU length  - including NULL byte, not including quotes
        uiLength = ((UINT32)(pszEnd - rszPointer)) - strlen(m_szNewLine) - 1;
        RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() - Calculated PDU String length: %u"
                " chars.\r\n", uiLength);
    }

    pszProactiveCmd = (char*)malloc(sizeof(char) * uiLength);
    if (NULL == pszProactiveCmd)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATN() - Could not alloc mem for command.\r\n");
        goto Error;
    }
    memset(pszProactiveCmd, 0, sizeof(char) * uiLength);

    // Parse <"hex_string">
    if (!ExtractQuotedString(rszPointer, pszProactiveCmd, uiLength, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATN() - Could not parse hex String.\r\n");
        goto Error;
    }

    // Ensure NULL termination
    pszProactiveCmd[uiLength-1] = '\0';

    RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() - Hex String: \"%s\".\r\n", pszProactiveCmd);

    ParsePduForRefresh(pszProactiveCmd);

    // Normal STK Event notify
    pResponse->SetResultCode(RIL_UNSOL_STK_EVENT_NOTIFY);

    if (!pResponse->SetData((void*) pszProactiveCmd, sizeof(char) * uiLength, FALSE))
    {
        goto Error;
    }

    fRet = TRUE;

Error:
    if (!fRet)
    {
        free(pszProactiveCmd);
        pszProactiveCmd = NULL;
    }

    RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() - Exit\r\n");
    return fRet;
}

void CSilo_SIM::ParsePduForRefresh(const char* pszPdu)
{
    RIL_LOG_INFO("CSilo_SIM::ParsePduForRefresh() - Enter\r\n");

    // Generic naming as described in ETSI 102 223 section 8.6
    enum REFRESH_TYPE {
        REFRESH_TYPE_NAA_INIT_AND_FULL_FILE_CHANGE = 0x00,
        REFRESH_TYPE_FILE_CHANGE_NOTIFICATION = 0x01,
        REFRESH_TYPE_NAA_INIT_AND_FILE_CHANGE = 0x02,
        REFRESH_TYPE_NAA_INIT = 0x03,
        REFRESH_TYPE_UICC_RESET = 0x04,
        REFRESH_TYPE_NAA_APPLICATION_RESET = 0x05,
        REFRESH_TYPE_NAA_SESSION_RESET = 0x06
    };

    const BYTE COMPREHENSION_TAG_MASK = 0x7F;

    BerTlv berTlv;
    typedef BerTlv ComprehensionTlv;
    ComprehensionTlv ctlv;
    const BYTE* pCTlvs = NULL;
    size_t leftLen = 0;
    int refreshType = -1;
    RIL_SimRefreshResponse_v7* pSimRefreshResp = NULL;
    BYTE* pPCmdByteBuffer = NULL;
    size_t pcmdByteBufferLen = 0;
    bool isEFpnnOrEFoplChanged = false;
    int nFiles = 0;

    if (!pszPdu) return;

    size_t pduLen = strlen(pszPdu);

    if (0 == pduLen || pduLen % 2 != 0 ) return;

    pcmdByteBufferLen = pduLen / 2;
    pPCmdByteBuffer = (BYTE*)malloc(pcmdByteBufferLen);
    if (!pPCmdByteBuffer)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParsePduForRefresh() - memory allocation failure\r\n");
        goto Error;
    }

    if (!extractByteArrayFromString(pszPdu, pduLen, pPCmdByteBuffer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParsePduForRefresh() - extractByteArrayFromString failed\r\n");
        goto Error;
    }

    if (berTlv.Parse(pPCmdByteBuffer, pcmdByteBufferLen))
    {
        const BYTE BER_PROACTIVE_CMD_TAG = 0xD0;

        if (berTlv.GetTag() != BER_PROACTIVE_CMD_TAG)
        {
            RIL_LOG_CRITICAL("CSilo_SIM::ParsePduForRefresh() - "
                    "BER Proactive cmd tag not found\r\n");
            goto Error;
        }

        if (pcmdByteBufferLen != berTlv.GetTotalSize())
        {
            RIL_LOG_CRITICAL("CSilo_SIM::ParsePduForRefresh() - "
                    "pCmdByteBufferLen != berTlv.GetTotalSize()\r\n");
        }


        // Get the length of all Comprehension TLV data objects
        leftLen = berTlv.GetLength();

        // Update pCTlvs to point to next CTLV
        pCTlvs = berTlv.GetValue();

        // Parse CTLV buffer into ctlv which must be Command details CTLV. leftLen is greater
        // than the CTLV length
        if (pCTlvs && ctlv.Parse(pCTlvs, leftLen))
        {
            const BYTE CMD_DETAILS_TAG = 0x01;
            const BYTE CMD_DETAILS_LENGTH = 3;

            /*
             * As Command Details comprehension TLV data object tag could be 0x81 or 0x01 based on
             * comprehension required flag bit, get only tag part of the byte by
             * & with COMPREHENSION_TAG_MASK.
             */
            if (CMD_DETAILS_TAG == (ctlv.GetTag() & COMPREHENSION_TAG_MASK)
                    && (CMD_DETAILS_LENGTH == ctlv.GetLength()))
            {
                const int TYPE_OF_CMD_INDEX = 1;
                const int CMD_QUALIFIER_INDEX = 2;
                const BYTE* pValue = ctlv.GetValue();

                if (pValue && CCatProfile::REFRESH == pValue[TYPE_OF_CMD_INDEX])
                {
                    refreshType = static_cast<int>(pValue[CMD_QUALIFIER_INDEX]);
                }

                // Update pCTlvs to point to next CTLV
                pCTlvs += ctlv.GetTotalSize();
                leftLen -= ctlv.GetTotalSize();
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CSilo_SIM::ParsePduForRefresh() - Command Details CTLV "
                    "parsing failed\r\n");
            goto Error;
        }
    }
    else
    {
        RIL_LOG_INFO("CSilo_SIM::ParsePduForRefresh() - BerTlv parsing failed\r\n");
        goto Error;
    }

    // Not a refresh proactive command or invalid refresh type
    if (refreshType < 0)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParsePduForRefresh() - Invalid refresh type\r\n");
        goto Error;
    }

    pSimRefreshResp = (RIL_SimRefreshResponse_v7*)malloc(sizeof(RIL_SimRefreshResponse_v7));
    if (!pSimRefreshResp)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParsePduForRefresh() - memory allocation failure"
                " for RIL_SimRefreshResponse_v7\r\n");
        goto Error;
    }
    pSimRefreshResp->result = SIM_INIT;
    pSimRefreshResp->ef_id = 0;
    pSimRefreshResp->aid = NULL;

    /*
     *                    Structure OF REFRESH proactive command
     *
     * Description                 Section    M/O/C    Min    Length
     *
     * Proactive UICC command Tag    9.2        M       Y       1
     * Length (A+B+C+D+E+F+G+H)       -         M       Y     1 or 2
     * Command details               8.6        M       Y       A
     * Device identities             8.7        M       Y       B
     * File List                     8.18       C       N       C
     * AID                           8.60       O       N       D
     * Alpha Identifier              8.2        O       N       E
     * Icon Identifier               8.31       O       N       F
     * Text Attribute                8.72       C       N       G
     * Frame Identifier              8.80       O       N       H
     *
     * For more details please refer ETSI TS 102 220 v11.2.0
     */

    // Proactive UICC command tag, length and Command Details are already parsed.
    // pCTlvs is now on the Device Identities CTLV

    // Parse CTLV buffer into ctlv which must be Device identities CTLV. leftLen may be greater
    // than the CTLV length
    if (ctlv.Parse(pCTlvs, leftLen))
    {
        // Update pCTlvs to point to next CTLV
        pCTlvs += ctlv.GetTotalSize();
        leftLen -= ctlv.GetTotalSize();
    }
    else
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParsePduForRefresh() - Parsing of CTLV failed\r\n");
        goto Error;
    }

    // Parse CTLV buffer into ctlv. leftLen may be greater than the CTLV length
    if (pCTlvs && ctlv.Parse(pCTlvs, leftLen))
    {
        const BYTE FILE_LIST_TAG = 0x12;
        const BYTE AID_TAG = 0x2F;

        /*
         * As File List comprehension TLV data object tag could be 0x92 or 0x12 based on
         * comprehension required flag bit, get only tag part of the byte by
         * & with COMPREHENSION_TAG_MASK.
         *
         * Note: File List CTLV is optional.
         */
        if (FILE_LIST_TAG == (ctlv.GetTag() & COMPREHENSION_TAG_MASK))
        {
            const int NUMBER_OF_FILES_INDEX = 0;
            const int FILES_INDEX = 1;
            // 1 byte for Number of files + minimum 2 bytes for file identifier
            const int MIN_FILE_LIST_CTLV_DATA_LENGTH = 3;

            const BYTE* pValue = ctlv.GetValue();
            size_t valueLen = ctlv.GetLength();

            nFiles = ((valueLen >= MIN_FILE_LIST_CTLV_DATA_LENGTH) && pValue)
                    ? pValue[NUMBER_OF_FILES_INDEX]
                    : 0;

            if (nFiles >= 1)
            {
                const int MASTER_FILE = 0x3F;
                const int FIRST_LEVEL_DEDICATED_FILE = 0x7F;
                const int SECOND_LEVEL_DEDICATED_FILE = 0x5F;
                const int FILE_ID_LENGTH = 2;
                const int EF_PNN = 0x6FC5;
                const int EF_OPL = 0x6FC6;

                // Skip to file index
                pValue += FILES_INDEX;
                valueLen -= FILES_INDEX;

                while (valueLen >= FILE_ID_LENGTH)
                {
                    /*
                     * First byte of file identifer is type of file. If the file type is master
                     * or 1st or 2nd level dedicated file, send only the elementary file
                     * identifers to framework.
                     */
                    if (MASTER_FILE == pValue[0]
                            || FIRST_LEVEL_DEDICATED_FILE == pValue[0]
                            || SECOND_LEVEL_DEDICATED_FILE == pValue[0])
                    {
                        pValue += FILE_ID_LENGTH;
                        valueLen -= FILE_ID_LENGTH;
                    }
                    else
                    {
                        pSimRefreshResp->ef_id = (pValue[0] << 8) + pValue[1] ;
                        isEFpnnOrEFoplChanged = (EF_PNN == pSimRefreshResp->ef_id
                                || EF_OPL == pSimRefreshResp->ef_id);

                        pValue += FILE_ID_LENGTH;
                        valueLen -= FILE_ID_LENGTH;
                    }
                }
            }

            // Update pCTlvs to point to next CTLV
            pCTlvs += ctlv.GetTotalSize();
            leftLen -= ctlv.GetTotalSize();
        }
        else
        {
            RIL_LOG_INFO("CSilo_SIM::ParsePduForRefresh() - File List CTLV not present\r\n");
        }

        // Parse CTLV buffer into ctlv. leftLen may be greater than the CTLV length
        if (ctlv.Parse(pCTlvs, leftLen))
        {
            /*
             * As AID comprehension TLV data object tag could be 0xAF or 0x2F based on
             * comprehension required flag bit, get only tag part of the byte by
             * & with COMPREHENSION_TAG_MASK.
             *
             * Note: AID CTLV is optional.
             */
            if (AID_TAG == (ctlv.GetTag() & COMPREHENSION_TAG_MASK))
            {
                size_t valueLen = ctlv.GetLength();
                const BYTE* pValue = ctlv.GetValue();

                if (valueLen > 0 && pValue)
                {
                    pSimRefreshResp->aid = (char*)malloc((valueLen * 2) + 1);
                    if (pSimRefreshResp->aid)
                    {
                        if (!convertByteArrayIntoString(pValue, valueLen, pSimRefreshResp->aid))
                        {
                            RIL_LOG_INFO("CSilo_SIM::ParsePduForRefresh - AID extraction"
                                    "failed\r\n");
                            free(pSimRefreshResp->aid);
                            pSimRefreshResp->aid = NULL;
                        }
                        RIL_LOG_INFO("CSilo_SIM::ParsePduForRefresh - Aid: %s\r\n",
                                pSimRefreshResp->aid ? pSimRefreshResp->aid : "");
                    }
                }

                // Update pCTlvs to point to next CTLV
                pCTlvs += ctlv.GetTotalSize();
                leftLen -= ctlv.GetTotalSize();
            }
            else
            {
                RIL_LOG_INFO("CSilo_SIM::ParsePduForRefresh() - AID CTLV not present\r\n");
            }
        }
    }

    switch (refreshType)
    {
        case REFRESH_TYPE_NAA_INIT_AND_FULL_FILE_CHANGE:
        case REFRESH_TYPE_NAA_INIT_AND_FILE_CHANGE:
        case REFRESH_TYPE_NAA_INIT:
            RIL_LOG_INFO("CSilo_SIM::ParsePduForRefresh() - SIM_INIT\r\n");

            /*
             * On REFRESH with NAA INIT, if rapid ril notifies the framework of REFRESH,
             * framework will issue requests to read SIM files. REFRESH notification from
             * modem is to indicate that REFRESH is ongoing but not yet completed. REFRESH
             * with NAA INIT completion is based on sim status reported via XSIM URC. So,
             * upon REFRESH with NAA init, don't notify the framework of SIM REFRESH but
             * set the internal sim state to NOT READY inorder to restrict SIM related
             * requests during the SIM REFRESH handling on modem side.
             */
            CTE::GetTE().SetSimAppState(RIL_APPSTATE_UNKNOWN);
            CTE::GetTE().SetNotifyRefreshOnSimReady(true);
            goto Error;

        case REFRESH_TYPE_UICC_RESET:
            // In case of UICC reset, don't send aid as reset applies to all applications in UICC
            free(pSimRefreshResp->aid);
            pSimRefreshResp->aid = NULL;
            // Fall through

        case REFRESH_TYPE_NAA_APPLICATION_RESET:
        case REFRESH_TYPE_NAA_SESSION_RESET:
            RIL_LOG_INFO("CSilo_SIM::ParsePduForRefresh() - SIM_RESET\r\n");

            /*
             * On REFRESH with UICC RESET or NAA Application reset or NAA session reset,
             * if rapid ril notifies the framework of REFRESH, framemwork will reset the
             * caches but won't trigger any sim file read request. Refresh completion is
             * notified via XSIM URC.  So, notify the framework of SIM REFRESH but also
             * set the internal sim state to NOT READY and refresh ongoing to true.
             */
            pSimRefreshResp->result = SIM_RESET;
            CTE::GetTE().SetSimAppState(RIL_APPSTATE_UNKNOWN);
            CTE::GetTE().SetNotifyRefreshOnSimReady(true);
            break;

        case REFRESH_TYPE_FILE_CHANGE_NOTIFICATION:
            RIL_LOG_INFO("CSilo_SIM::ParsePduForRefresh() - SIM_FILE_UPDATE\r\n");

            if (1 == nFiles)
            {
                pSimRefreshResp->result = SIM_FILE_UPDATE;
            }
            else
            {
                /*
                 * In case of refresh - file change notification with multiple files, overwrite
                 * the SIM_FILE_UPDATE value with SIM_INIT and also set ef_id to 0.
                 */
                pSimRefreshResp->result = SIM_INIT;
                pSimRefreshResp->ef_id = 0;
            }
            break;

        default:
            RIL_LOG_INFO("CSilo_SIM::ParsePduForRefresh() - refreshType: %d not handled\r\n",
                    refreshType);
            goto Error;
    }

    // Send out SIM_REFRESH notification
    RIL_onUnsolicitedResponse(RIL_UNSOL_SIM_REFRESH, (void*)pSimRefreshResp,
            sizeof(RIL_SimRefreshResponse_v7));

    if (isEFpnnOrEFoplChanged)
    {
        /*
         * Force the framework to query operator name when there is a change in EFopl or EFpnn
         * files.
         */
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED, NULL, 0);
    }

Error:
    free(pSimRefreshResp->aid);
    pSimRefreshResp->aid = NULL;

    free(pSimRefreshResp);
    pSimRefreshResp = NULL;

    free(pPCmdByteBuffer);
    pPCmdByteBuffer = NULL;

    RIL_LOG_INFO("CSilo_SIM::ParsePduForRefresh() - Exit\r\n");
}

BOOL CSilo_SIM::ParseTermRespConfirm(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_INFO("CSilo_SIM::ParseTermRespConfirm() - Enter\r\n");
    BOOL fRet = FALSE;
    UINT32 uiStatus1;
    UINT32 uiStatus2;
    UINT32 uiModemType = CTE::GetTE().GetModemType();

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseTermRespConfirm() : pResponse was NULL\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);

    if (MODEM_TYPE_XMM7260 <= uiModemType)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseTermRespConfirm() - +SATF not expected for : "
                " %u \r\n", uiModemType);
        goto Error;
    }

    // Extract "<sw1>"
    if (!ExtractUInt32(rszPointer, uiStatus1, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseTermRespConfirm() - Could not parse sw1.\r\n");
        goto Error;
    }

    RIL_LOG_INFO(" Status 1: %u.\r\n", uiStatus1);

    // Extract "<sw2>"
    if ( (!FindAndSkipString(rszPointer, ",", rszPointer))     ||
         (!ExtractUInt32(rszPointer, uiStatus2, rszPointer)))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseTermRespConfirm() - Could not parse sw2.\r\n");
        goto Error;
    }

    RIL_LOG_INFO(" Status 2: %u.\r\n", uiStatus2);

    if (uiStatus1 == 0x90 && uiStatus2 == 0x00)
    {
        pResponse->SetResultCode(RIL_UNSOL_STK_SESSION_END);
    }

    fRet = TRUE;

Error:
    RIL_LOG_INFO("CSilo_SIM::ParseTermRespConfirm() - Exit\r\n");
    return fRet;
}

BOOL CSilo_SIM::ParseXSIM(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SIM::ParseXSIM() - Enter\r\n");
    BOOL fRet = FALSE;
    const char* pszEnd = NULL;
    UINT32 uiSIMState = 0;
    BOOL bNotifySimStatusChange = TRUE;
    int oldAppState = CTE::GetTE().GetSimAppState();

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXSIM() : pResponse was NULL\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);

    // Extract "<SIM state>"
    if (!ExtractUInt32(rszPointer, uiSIMState, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXSIM() - Could not parse nSIMState.\r\n");
        goto Error;
    }

    CTE::GetTE().HandleSimState(uiSIMState, bNotifySimStatusChange);

    if (RIL_APPSTATE_UNKNOWN != CTE::GetTE().GetSimAppState()
            && RIL_APPSTATE_UNKNOWN == oldAppState)
    {
        RIL_requestTimedCallback(triggerQueryUiccInfo, NULL, 0, 0);
    }
    else
    {
        if (bNotifySimStatusChange)
        {
            pResponse->SetResultCode(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED);
        }
    }

    fRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CSilo_SIM::ParseXSIM() - Exit\r\n");
    return fRet;
}


BOOL CSilo_SIM::ParseXLOCK(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SIM::ParseXLOCK() - Enter\r\n");

    /*
     * If the MT is waiting for any of the following passwords
     * PH-NET PIN, PH-NETSUB PIN, PH-SP PIN then only XLOCK URC will be
     * received.
     */

    BOOL fRet = FALSE;
    int i = 0;
    BOOL bIsDataValid = FALSE;
    int perso_substate = RIL_PERSOSUBSTATE_UNKNOWN;

    //  The number of locks returned by +XLOCK URC.
    const int nMAX_LOCK_INFO = 5;

    typedef struct
    {
        char fac[3];
        UINT32 lock_state;
        UINT32 lock_result;
    } S_LOCK_INFO;

    S_LOCK_INFO lock_info[nMAX_LOCK_INFO];

    if (NULL == pResponse)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXLOCK() : pResponse was NULL\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);

    if (RIL_APPSTATE_IMEI == CTE::GetTE().GetSimAppState())
    {
        RIL_LOG_INFO("CSilo_SIM::ParseXLOCK() - ignore XLOCK when UICC is locked to a"
                " device\r\n");
        return TRUE;
    }

    memset(lock_info, 0, sizeof(lock_info));

    // Change the number to the number of facility locks supported via XLOCK URC.
    while (i < nMAX_LOCK_INFO)
    {
        memset(lock_info[i].fac, '\0', sizeof(lock_info[i].fac));

        // Extract "<fac>"
        if (ExtractQuotedString(rszPointer, lock_info[i].fac, sizeof(lock_info[i].fac),
                rszPointer))
        {
            // Extract ",<Lock state>"
            if (!SkipString(rszPointer, ",", rszPointer) ||
                !ExtractUInt32(rszPointer, lock_info[i].lock_state, rszPointer))
            {
                RIL_LOG_CRITICAL("CSilo_SIM::ParseXLOCK() - Could not parse <lock state>.\r\n");
                goto Error;
            }

            // Extract ",<Lock result>"
            if (!SkipString(rszPointer, ",", rszPointer) ||
                !ExtractUInt32(rszPointer, lock_info[i].lock_result, rszPointer))
            {
                RIL_LOG_CRITICAL("CSilo_SIM::ParseXLOCK() - Could not parse <lock result>.\r\n");
                goto Error;
            }

            bIsDataValid = TRUE;
        }
        else
        {
            RIL_LOG_INFO("CSilo_SIM::ParseXLOCK() - Unable to find <fac>!\r\n");
        }

        SkipString(rszPointer, ",", rszPointer);

        i++;
    }

    if (bIsDataValid)
    {
        i = 0;

        // notify Android if SIM lock state has changed
        while (i < nMAX_LOCK_INFO)
        {
            RIL_LOG_INFO("lock:%s state:%d result:%d", lock_info[i].fac, lock_info[i].lock_state,
                            lock_info[i].lock_result);

            if (0 == strncmp(lock_info[i].fac, "PN", 2))
            {
                if (lock_info[i].lock_state == 1 && lock_info[i].lock_result == 1)
                {
                    perso_substate =  RIL_PERSOSUBSTATE_SIM_NETWORK;
                }
                else if (lock_info[i].lock_state == 3 && lock_info[i].lock_result == 2)
                {
                    perso_substate = RIL_PERSOSUBSTATE_SIM_NETWORK_PUK;
                }

                if (RIL_PERSOSUBSTATE_UNKNOWN != perso_substate)
                {
                    CTE::GetTE().SetPersonalisationSubState(perso_substate);
                    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED);
                }
                break;
            }
            else
            {
                if ((lock_info[i].lock_state == 1 && lock_info[i].lock_result == 1) ||
                        (lock_info[i].lock_state == 3 && lock_info[i].lock_result == 2))
                {
                    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED);
                    break;
                }
            }

            i++;
        }
    }

    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_SIM::ParseXLOCK() - Exit\r\n");
    return fRet;
}


BOOL CSilo_SIM::ParseXLEMA(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SIM::ParseXLEMA() - Enter\r\n");
    BOOL fRet = FALSE;
    UINT32 uiIndex = 0;
    UINT32 uiTotalCnt = 0;
    char szECCItem[MAX_BUFFER_SIZE] = {0};
    const char szRIL_ECCLIST[] = "ril.ecclist";
    int subscriptionId = CHardwareConfig::GetInstance().GetSubscriptionId();

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXLEMA() : pResponse was NULL\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);

    // Extract "<index>"
    if (!ExtractUInt32(rszPointer, uiIndex, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXLEMA() - Could not parse uiIndex.\r\n");
        goto Error;
    }

    // Extract ",<total cnt>"
    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractUInt32(rszPointer, uiTotalCnt, rszPointer) )
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXLEMA() - Could not parse uiTotalCnt.\r\n");
        goto Error;
    }

    if (!SkipString(rszPointer, ",", rszPointer) ||
        !ExtractQuotedString(rszPointer, szECCItem, MAX_BUFFER_SIZE, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXLEMA() - Could not parse szECCItem.\r\n");
        goto Error;
    }

    RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() - Found ECC item=[%d] out of [%d]  ECC=[%s]\r\n",
            uiIndex, uiTotalCnt, szECCItem);

    //  If the uiIndex is 1, then assume reading first ECC code.
    //  Clear the master list and store the code.
    if (1 == uiIndex)
    {
        RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() - First index, clear master ECC list,"
                " store code=[%s]\r\n", szECCItem);
        if (!PrintStringNullTerminate(m_szECCList, MAX_BUFFER_SIZE, "%s", szECCItem))
        {
            RIL_LOG_CRITICAL("CSilo_SIM::ParseXLEMA() - Could not create m_szCCList\r\n");
            goto Error;
        }
    }
    else
    {
        //  else add code to the end.
        RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() - store code=[%s]\r\n", szECCItem);
        strncat(m_szECCList, ",", MAX_BUFFER_SIZE - strlen(m_szECCList) - 1);
        strncat(m_szECCList, szECCItem, MAX_BUFFER_SIZE - strlen(m_szECCList) - 1);
    }

    RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() - m_szECCList=[%s]\r\n", m_szECCList);


    //  If the uiIndex is the total count, assume we have all ECC codes.
    //  In that case, set property!
    if (uiIndex == uiTotalCnt)
    {
        char szEccListProp[PROPERTY_VALUE_MAX] = {0};

        //  If subscription id is 0 then continue to use "ril.ecclist" property name.
        if (!subscriptionId)
        {
            strncpy(szEccListProp, szRIL_ECCLIST, PROPERTY_VALUE_MAX-1);
            szEccListProp[PROPERTY_VALUE_MAX-1] = '\0'; // KW fix
        }
        else
        {
            snprintf(szEccListProp, PROPERTY_VALUE_MAX, "%s%d",
                    szRIL_ECCLIST,
                    subscriptionId);
        }

        RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() - uiIndex == uiTotalCnt == %d\r\n", uiTotalCnt);
        RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() - setting %s = [%s]\r\n", szEccListProp,
                m_szECCList);
        property_set(szEccListProp, m_szECCList);
    }

    fRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CSilo_SIM::ParseXLEMA() - Exit\r\n");
    return fRet;
}


BOOL CSilo_SIM::ParseIndicationCusats(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SIM::ParseIndicationCusats() - Enter\r\n");
    BOOL fRet = FALSE;
    UINT32 uiUiccState = 0;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationCusats() : pResponse was NULL\r\n");
        goto Out;
    }

    pResponse->SetUnsolicitedFlag(TRUE);

    /* +CUSATS: <UICC_state> */

    // Parse UICC state
    if (!ExtractUInt32(rszPointer, uiUiccState, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationCusats() -"
                " Could not find UICC state argument.\r\n");
        goto Out;
    }
    else
    {
        RIL_LOG_INFO("CSilo_SIM::ParseIndicationCusats() -"
                " uiUiccState= %u.\r\n", uiUiccState);

        UsatInitStateMachine::GetStateMachine().SetUiccState(uiUiccState);
        if (uiUiccState == UsatInitStateMachine::PROFILE_DOWNLOAD_COMPLETED)
        {
            CTE::GetTE().SetProfileDownloadForNextUiccStartup(1, 1);
        }
        else if (uiUiccState == UsatInitStateMachine::UICC_ACTIVE)
        {
            UsatInitStateMachine::GetStateMachine()
                    .SendEvent(UsatInitStateMachine::SIM_READY_TO_ACTIVATE);
        }
    }

    fRet = TRUE;

Out:
    RIL_LOG_VERBOSE("CSilo_SIM::ParseIndicationCusats() - Exit\r\n");
    return fRet;
}

BOOL CSilo_SIM::ParseIndicationCusatp(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_VERBOSE("CSilo_SIM::ParseIndicationCusatp() - Enter\r\n");
    char* pszProactiveCmd = NULL;
    UINT32 uiLength = 0;
    const char* pszEnd = NULL;
    BOOL fRet = FALSE;
    UINT8 uiCmdTag = 0;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationCusatp() : pResponse was NULL\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);

    RIL_LOG_INFO("CSilo_SIM::ParseIndicationCusatp() : Resp:%s\r\n", rszPointer);

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, m_szNewLine, pszEnd))
    {
        // incomplete message notification
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationCusatp() : Could not find response end\r\n");
        goto Error;
    }
    else
    {
        // PDU is followed by m_szNewLine, so look for m_szNewLine and use its
        // position to determine length of PDU string.

        // Calculate PDU length + NULL byte
        uiLength = ((UINT32)(pszEnd - rszPointer)) - strlen(m_szNewLine) + 1;
        RIL_LOG_INFO("CSilo_SIM::ParseIndicationCusatp() - Calculated PDU String length: %u"
                " chars.\r\n", uiLength);
    }

    pszProactiveCmd = (char*)malloc(sizeof(char) * uiLength);
    if (NULL == pszProactiveCmd)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationCusatp() - Could not alloc mem for"
                " command.\r\n");
        goto Error;
    }
    memset(pszProactiveCmd, 0, sizeof(char) * uiLength);

    // Parse <"hex_string">
    if (!ExtractQuotedString(rszPointer, pszProactiveCmd, uiLength, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationCusatp() - Could not parse hex String.\r\n");
        goto Error;
    }

    // Ensure NULL termination
    pszProactiveCmd[uiLength-1] = '\0';

    RIL_LOG_INFO("CSilo_SIM::ParseIndicationCusatp() - Hex String: \"%s\".\r\n", pszProactiveCmd);

    if (IsProactiveCmd(pszProactiveCmd, &uiCmdTag))
    {
        pResponse->SetResultCode(RIL_UNSOL_STK_PROACTIVE_COMMAND);
        RIL_LOG_INFO("CSilo_SIM::ParseIndicationCusatp() - IS A PROACTIVE COMMAND.\r\n",
                pszProactiveCmd);
    }
    else
    {
        pResponse->SetResultCode(RIL_UNSOL_STK_EVENT_NOTIFY);
        RIL_LOG_INFO("CSilo_SIM::ParseIndicationCusatp() - IS AN EVENT NOTIFICATION.\r\n",
                pszProactiveCmd);
    }

    RIL_LOG_INFO("CSilo_SIM::ParseIndicationCusatp() - CmdId:0x%X\r\n", uiCmdTag);

    if (uiCmdTag == CCatProfile::REFRESH)
    {
        ParsePduForRefresh(pszProactiveCmd);
    }

    if (!pResponse->SetData((void*) pszProactiveCmd, sizeof(char) * uiLength, FALSE))
    {
        goto Error;
    }

    fRet = TRUE;

Error:
    if (!fRet)
    {
        free(pszProactiveCmd);
        pszProactiveCmd = NULL;
    }

    RIL_LOG_VERBOSE("CSilo_SIM::ParseIndicationCusatp() - Exit\r\n");
    return fRet;
}


BOOL CSilo_SIM::ParseIndicationCusatend(CResponse* const pResponse, const char*& /*rszPointer*/)
{
    RIL_LOG_VERBOSE("CSilo_SIM::ParseIndicationCusatend() - Enter\r\n");
    BOOL fRet = FALSE;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationCusatend() : pResponse was NULL\r\n");
        goto Error;
    }

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_STK_SESSION_END);

    fRet = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_SIM::ParseIndicationCusatend() - Exit\r\n");
    return fRet;
}
