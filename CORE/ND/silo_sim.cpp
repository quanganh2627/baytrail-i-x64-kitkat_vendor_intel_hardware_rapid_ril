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

#include <cutils/properties.h>
#include <sys/system_properties.h>

//
//
CSilo_SIM::CSilo_SIM(CChannel* pChannel, CSystemCapabilities* pSysCaps)
: CSilo(pChannel, pSysCaps),
  m_IsReadyForAttach(FALSE)
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
    RIL_LOG_VERBOSE("CSilo_SIM::~CSilo_SIM() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_SIM::~CSilo_SIM() - Exit\r\n");
}


char* CSilo_SIM::GetURCInitString()
{
    // SIM silo-related URC channel basic init string
    const char szSimURCInitString[] = "+XSIMSTATE=1|+XLEMA=1";

    if (!ConcatenateStringNullTerminate(m_szURCInitString,
            MAX_BUFFER_SIZE - strlen(m_szURCInitString), szSimURCInitString))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::GetURCInitString() : Failed to copy URC init "
                "string!\r\n");
        return NULL;
    }

    if (!m_pSystemCapabilities->IsStkCapable())
    {
        /*
         * STK is disabled by sending all bytes of terminal profile set to 0.
         * This is already taken care as part of nvm configuration file. In order
         * to get the EAP-SIM authentication working, rapid ril needs to enable
         * proactive fetching. By sending AT+XSATK=1,0, proactive fetching is enabled
         * but the STK URCs are disabled.
         */
        if (!ConcatenateStringNullTerminate(m_szURCInitString,
                MAX_BUFFER_SIZE - strlen(m_szURCInitString), "|+XSATK=1,0"))
        {
            RIL_LOG_CRITICAL("CSilo_SIM::GetURCInitString() : Failed to concat XSATK to URC "
                    "init string!\r\n");
            return NULL;
        }
    }
    return m_szURCInitString;
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
//  Parse functions here
///////////////////////////////////////////////////////////////////////////////////////////////

BOOL CSilo_SIM::ParseIndicationSATI(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATI() - Enter\r\n");
    char* pszProactiveCmd = NULL;
    UINT32 uiLength = 0;
    const char* pszEnd = NULL;
    BOOL fRet = FALSE;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATI() : pResponse was NULL\r\n");
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

    pResponse->SetUnsolicitedFlag(TRUE);
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
    char* pRefresh = NULL;
    char* pFileTag = NULL;
    char szRefreshCmd[] = "8103";
    char szFileTag[] = "92";
    char szFileTagLength[3] = {0};
    char szFileID[5] = {0};
    UINT32 uiFileTagLength = 0;
    char szRefreshType[3] = {0};
    RIL_SimRefreshResponse_v7* pSimRefreshResp = NULL;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATN() : pResponse was NULL\r\n");
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

    pResponse->SetUnsolicitedFlag(TRUE);

    //  Need to see if this is a SIM_REFRESH command.
    //  Check for "8103", jump next byte and verify if followed by "01".
    pRefresh = strstr(pszProactiveCmd, szRefreshCmd);
    if (pRefresh)
    {
        UINT32 uiPos = pRefresh - pszProactiveCmd;
        //RIL_LOG_INFO("uiPos = %d\r\n", uiPos);

        uiPos += strlen(szRefreshCmd);
        //RIL_LOG_INFO("uiPos = %d  uiLength=%d\r\n", uiPos, uiLength);
        if ( (uiPos + 6) < uiLength)  // 6 is next byte plus "01" plus type of SIM refresh
        {
            //  Skip next byte
            uiPos += 2;
            //RIL_LOG_INFO("uiPos = %d\r\n", uiPos);

            if (0 == strncmp(&pszProactiveCmd[uiPos], "01", 2))
            {
                //  Skip to type of SIM refresh
                uiPos += 2;
                //RIL_LOG_INFO("uiPos = %d\r\n", uiPos);

                //  See what our SIM Refresh command is
                strncpy(szRefreshType, &pszProactiveCmd[uiPos], 2);
                uiPos += 2;

                RIL_LOG_INFO("*** We found %s SIM_REFRESH   type=[%s]***\r\n",
                        szRefreshCmd, szRefreshType);

                //  If refresh type = "01"  -> SIM_FILE_UPDATE
                //  If refresh type = "00","02","03" -> SIM_INIT
                //  If refresh type = "04","05","06" -> SIM_RESET

                pSimRefreshResp =
                        (RIL_SimRefreshResponse_v7*)malloc(sizeof(RIL_SimRefreshResponse_v7));
                if (NULL != pSimRefreshResp)
                {
                    //  default to SIM_INIT
                    pSimRefreshResp->result = SIM_INIT;
                    pSimRefreshResp->ef_id = 0;
                    pSimRefreshResp->aid = NULL;

                    //  Check for type of refresh
                    if ( (0 == strncmp(szRefreshType, "00", 2)) ||
                         (0 == strncmp(szRefreshType, "02", 2)) ||
                         (0 == strncmp(szRefreshType, "03", 2)) )
                    {
                        //  SIM_INIT
                        RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() - SIM_INIT\r\n");
                        pSimRefreshResp->result = SIM_INIT;
                        // See ril.h: aid : For SIM_INIT result this field is set to AID of
                        //      application that caused REFRESH
                        pSimRefreshResp->aid = NULL;
                    }
                    else if ( (0 == strncmp(szRefreshType, "04", 2)) ||
                              (0 == strncmp(szRefreshType, "05", 2)) ||
                              (0 == strncmp(szRefreshType, "06", 2)) )
                    {
                        //  SIM_RESET
                        RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() - SIM_RESET\r\n");
                        /*
                         * Incase of IMC SUNRISE platform, SIM_RESET refresh
                         * is handled on the modem side. If the Android telephony
                         * framework is informed of this refresh, then it will
                         * initiate a RADIO_POWER off which will interfere with
                         * the SIM RESET procedure on the modem side. So, don't send
                         * the RIL_UNSOL_SIM_REFRESH for SIM_RESET refresh type.
                         */
                        goto event_notify;
                    }
                    else if ( (0 == strncmp(szRefreshType, "01", 2)) )
                    {
                        //  SIM_FILE_UPDATE
                        RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() - SIM_FILE_UPDATE\r\n");
                        pSimRefreshResp->result = SIM_FILE_UPDATE;

                        //  Tough part here - need to read file ID(s)
                        //  Android looks for EF_MBDN 0x6FC7 or EF_MAILBOX_CPHS 0x6F17
                        //  File tag is "92".  Just look for "92" from uiPos.
                        if ('\0' != pszProactiveCmd[uiPos])
                        {
                            //  Look for "92"
                            pFileTag = strstr(&pszProactiveCmd[uiPos], szFileTag);
                            if (pFileTag)
                            {
                                //  Found "92" somewhere in rest of string
                                uiPos = pFileTag - pszProactiveCmd;
                                uiPos += strlen(szFileTag);
                                RIL_LOG_INFO("FOUND FileTag uiPos now = %d\r\n", uiPos);
                                if (uiPos < uiLength)
                                {
                                    //  Read length of tag
                                    strncpy(szFileTagLength, &pszProactiveCmd[uiPos], 2);
                                    //RIL_LOG_INFO("file tag length = %s\r\n", szFileTagLength);
                                    uiFileTagLength = (UINT8)SemiByteCharsToByte(
                                            szFileTagLength[0], szFileTagLength[1]);
                                    RIL_LOG_INFO("file tag length = %d\r\n", uiFileTagLength);
                                    uiPos += 2;  //  we read the tag length
                                    uiPos += (uiFileTagLength * 2);
                                    //RIL_LOG_INFO("uiPos is now end hopefully = %d
                                    //         "uilength=%d\r\n", uiPos, uiLength);

                                    if (uiPos <= uiLength)
                                    {
                                        //  Read last 2 bytes (last 4 characters) of tag
                                        uiPos -= 4;
                                        //RIL_LOG_INFO("uiPos before reading fileID=%d\r\n", uiPos);
                                        strncpy(szFileID, &pszProactiveCmd[uiPos], 4);
                                        RIL_LOG_INFO("szFileID[%s]\r\n", szFileID);

                                        //  Convert hex string to int
                                        UINT32 ui1 = 0, ui2 = 0;
                                        ui1 = (UINT8)SemiByteCharsToByte(szFileID[0], szFileID[1]);
                                        ui2 = (UINT8)SemiByteCharsToByte(szFileID[2], szFileID[3]);
                                        // See ril.h:
                                        //  ef_id : is the EFID of the updated file if the result is
                                        //  SIM_FILE_UPDATE or 0 for any other result.
                                        //  aid: For SIM_FILE_UPDATE result it can be set to AID of
                                        //  application in which updated EF resides or it can be
                                        //  NULL if EF is outside of an application.
                                        pSimRefreshResp->ef_id = (ui1 << 8) + ui2;
                                        RIL_LOG_INFO("pInts[1]=[%d],%04X\r\n",
                                            pSimRefreshResp->ef_id, pSimRefreshResp->ef_id);
                                    }
                                }
                            }
                        }
                    }

                    //  Send out SIM_REFRESH notification
                    RIL_onUnsolicitedResponse(RIL_UNSOL_SIM_REFRESH, (void*)pSimRefreshResp,
                                                sizeof(RIL_SimRefreshResponse_v7));
                }
                else
                {
                    RIL_LOG_CRITICAL("CSilo_SIM::ParseIndicationSATN() -"
                            " cannot allocate pInts\r\n");
                }
            }
        }
    }

event_notify:
    //  Normal STK Event notify
    pResponse->SetResultCode(RIL_UNSOL_STK_EVENT_NOTIFY);

    if (!pResponse->SetData((void*) pszProactiveCmd, sizeof(char) * uiLength, FALSE))
    {
        goto Error;
    }

    free(pSimRefreshResp);
    pSimRefreshResp = NULL;

    fRet = TRUE;

Error:
    if (!fRet)
    {
        free(pszProactiveCmd);
        pszProactiveCmd = NULL;

        free(pSimRefreshResp);
        pSimRefreshResp = NULL;
    }

    RIL_LOG_INFO("CSilo_SIM::ParseIndicationSATN() - Exit\r\n");
    return fRet;
}

BOOL CSilo_SIM::ParseTermRespConfirm(CResponse* const pResponse, const char*& rszPointer)
{
    RIL_LOG_INFO("CSilo_SIM::ParseTermRespConfirm() - Enter\r\n");
    BOOL fRet = FALSE;
    const char* pszEnd = NULL;
    UINT32 uiStatus1;
    UINT32 uiStatus2;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseTermRespConfirm() : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, m_szNewLine, pszEnd))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseTermRespConfirm() : Could not find response end\r\n");
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
        pResponse->SetUnsolicitedFlag(TRUE);
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
    UINT32 nSIMState = 0;

    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXSIM() : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, m_szNewLine, pszEnd))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXSIM() : Could not find response end\r\n");
        goto Error;
    }

    // Extract "<SIM state>"
    if (!ExtractUInt32(rszPointer, nSIMState, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXSIM() - Could not parse nSIMState.\r\n");
        goto Error;
    }

    // Here we assume we don't have card error.
    // This will be changed in case we received XSIM=8.
    CTE::GetTE().SetSimTechnicalProblem(FALSE);

    /// @TODO: Need to revisit the XSIM and radio state mapping
    switch (nSIMState)
    {
        /*
         * XSIM: 3 will be received upon PIN related operations.
         *
         * For PIN related operation occuring after the SIM initialisation,
         * no XSIM: 7 will be sent by modem. So, trigger the radio state
         * change with SIM ready upon XSIM: 3.
         *
         * For PIN related operation occuring during the boot or before
         * the SIM initialisation, then XSIM: 7 will be sent by modem. In this
         * case, radio state  change with SIM ready will be sent upon the
         * receival of XSIM: 7 event.
         */
        case 3: // PIN verified - Ready
            if (m_IsReadyForAttach) {
                RIL_LOG_INFO("CSilo_SIM::ParseXSIM() - READY FOR ATTACH\r\n");
                CTE::GetTE().SetSIMState(RRIL_SIM_STATE_READY);
            }
            break;
        /*
         * XSIM: 10 and XSIM: 11 will be received when the SIM driver has
         * lost contact of SIM and re-established the contact respectively.
         * After XSIM: 10, either XSIM: 9 or XSIM: 11 will be received.
         * So, no need to trigger SIM_NOT_READY on XSIM: 10. Incase of
         * XSIM: 11, network registration will be restored by the modem
         * itself.
         */
        case 10: // SIM Reactivating
            break;
        case 11: // SIM Reactivated
            CTE::GetTE().SetSIMState(RRIL_SIM_STATE_READY);
            break;
        /*
         * XSIM: 2 means PIN verification not needed but not ready for attach.
         * SIM_READY should be triggered only when the modem is ready
         * to attach.(XSIM: 7 or XSIM: 3(in some specific case))
         */
        case 2:
        case 6: // SIM Error
            // The SIM is initialized, but modem is still in the process of it.
            // we can inform Android that SIM is still not ready.
            RIL_LOG_INFO("CSilo_SIM::ParseXSIM() - SIM NOT READY\r\n");
            CTE::GetTE().SetSIMState(RRIL_SIM_STATE_NOT_READY);
            break;
        case 8: // SIM Technical problem
            RIL_LOG_INFO("CSilo_SIM::ParseXSIM() - SIM TECHNICAL PROBLEM\r\n");
            CTE::GetTE().SetSimTechnicalProblem(TRUE);
            break;
        case 7: // ready for attach (+COPS)
            RIL_LOG_INFO("CSilo_SIM::ParseXSIM() - READY FOR ATTACH\r\n");
            m_IsReadyForAttach = TRUE;
            CTE::GetTE().SetSIMState(RRIL_SIM_STATE_READY);
            CSystemManager::GetInstance().TriggerSimUnlockedEvent();
            break;
        case 12: // SIM SMS caching completed
            RIL_LOG_INFO("[RIL STATE] SIM SMS CACHING COMPLETED\r\n");
            CTE::GetTE().TriggerQuerySimSmsStoreStatus();
            break;
        case 0: // SIM not present
        case 9: // SIM Removed
            RIL_LOG_INFO("CSilo_SIM::ParseXSIM() - SIM REMOVED/NOT PRESENT\r\n");
            m_IsReadyForAttach = FALSE;
            CTE::GetTE().SetSIMState(RRIL_SIM_STATE_ABSENT);
            break;
        case 14: // SIM powered off by modem
            RIL_LOG_INFO("CSilo_SIM::ParseXSIM() - SIM Powered off by modem\r\n");
            m_IsReadyForAttach = FALSE;
            // Fall through to notify Radio and Sim status
        case 1: // PIN verification needed
        case 4: // PUK verification needed
        case 5: // SIM permanently blocked
        default:
            CTE::GetTE().SetSIMState(RRIL_SIM_STATE_NOT_READY);
            break;
    }

    pResponse->SetUnsolicitedFlag(TRUE);
    pResponse->SetResultCode(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED);

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
    const char* pszEnd = NULL;

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

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, m_szNewLine, pszEnd))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXLOCK() : Could not find response end\r\n");
        goto Error;
    }

    memset(lock_info, 0, sizeof(lock_info));

    // Change the number to the number of facility locks supported via XLOCK URC.
    while (i < nMAX_LOCK_INFO)
    {
        memset(lock_info[i].fac, '\0', sizeof(lock_info[i].fac));

        // Extract "<fac>"
        if (!ExtractQuotedString(rszPointer, lock_info[i].fac, sizeof(lock_info[i].fac),
                rszPointer))
        {
            RIL_LOG_INFO("CSilo_SIM::ParseXLOCK() - Unable to find <fac>!\r\n");
            goto complete;
        }

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

        SkipString(rszPointer, ",", rszPointer);

        i++;
    }

complete:
    // Look for "<postfix>"
    if (!FindAndSkipRspEnd(rszPointer, m_szNewLine, rszPointer))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXLOCK() - Could not extract response postfix.\r\n");
        goto Error;
    }

    // Walk back over the <CR><LF>
    rszPointer -= strlen(m_szNewLine);

    pResponse->SetUnsolicitedFlag(TRUE);

    i = 0;
    // Change the number to the number of facility locks supported via XLOCK URC.
    while (i < nMAX_LOCK_INFO)
    {
        RIL_LOG_INFO("lock:%s state:%d result:%d", lock_info[i].fac, lock_info[i].lock_state,
                        lock_info[i].lock_result);

        /// @TODO: Need to revisit the lock state mapping.
        if ((lock_info[i].lock_state == 1 && lock_info[i].lock_result == 1) ||
           (lock_info[i].lock_state == 3 && lock_info[i].lock_result == 2))
        {
            CTE::GetTE().SetSIMState(RRIL_SIM_STATE_NOT_READY);
            pResponse->SetResultCode(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED);
            break;
        }
        i++;
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
    const char* pszEnd = NULL;
    UINT32 uiIndex = 0;
    UINT32 uiTotalCnt = 0;
    char szECCItem[MAX_BUFFER_SIZE] = {0};
    const char szRIL_ECCLIST[] = "ril.ecclist";


    if (pResponse == NULL)
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXLEMA() : pResponse was NULL\r\n");
        goto Error;
    }

    // Look for a "<postfix>" to be sure we got a whole message
    if (!FindAndSkipRspEnd(rszPointer, m_szNewLine, pszEnd))
    {
        RIL_LOG_CRITICAL("CSilo_SIM::ParseXLEMA() : Could not find response end\r\n");
        goto Error;
    }

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

    //  Skip the rest of the parameters (if any)
    // Look for a "<postfix>" to be sure we got a whole message
    FindAndSkipRspEnd(rszPointer, m_szNewLine, rszPointer);

    //  Back up over the "\r\n".
    rszPointer -= strlen(m_szNewLine);


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
        char szEccListProp[MAX_PROP_VALUE] = {0};

        //  If sim id == 0 or if sim id is not provided by RILD, then continue
        //  to use "ril.ecclist" property name.
        if ( (NULL == g_szSIMID) || ('0' == g_szSIMID[0]) )
        {
            strncpy(szEccListProp, szRIL_ECCLIST, MAX_PROP_VALUE-1);
            szEccListProp[MAX_PROP_VALUE-1] = '\0'; // KW fix
        }
        else
        {
            snprintf(szEccListProp, MAX_PROP_VALUE, "%s%s", szRIL_ECCLIST, g_szSIMID);
        }

        RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() - uiIndex == uiTotalCnt == %d\r\n", uiTotalCnt);
        RIL_LOG_INFO("CSilo_SIM::ParseXLEMA() - setting %s = [%s]\r\n", szEccListProp,
                m_szECCList);
        property_set(szEccListProp, m_szECCList);
    }

    //  Flag as unrecognized.
    //pResponse->SetUnrecognizedFlag(TRUE);

    fRet = TRUE;
Error:
    RIL_LOG_VERBOSE("CSilo_SIM::ParseXLEMA() - Exit\r\n");
    return fRet;
}
