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
// Author:  Dennis Peter
// Created: 2007-08-01
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date        Who      Ver   Description
//  ----------  -------  ----  -----------------------------------------------
//  June 03/08  DP       1.00  Established v1.00 based on current code base.
//  May  04/09  CW       1.01  Moved common code to base class, identified
//                             platform-specific implementations, implemented
//                             general code clean-up.
//
/////////////////////////////////////////////////////////////////////////////

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

#ifdef RIL_ENABLE_SIMTK
CResponse* g_CachedToolKitNotifs[MAX_TOOLKITNOTIFS];
#endif // RIL_ENABLE_SIMTK

//
//
CSilo_SIM::CSilo_SIM(CChannel *pChannel)
: CSilo(pChannel)
{
    RIL_LOG_VERBOSE("CSilo_SIM::CSilo_SIM() - Enter\r\n");

    // AT Response Table
    static ATRSPTABLE pATRspTable[] =
    {
        { ""           , (PFN_ATRSP_PARSE)&CSilo_SIM::ParseNULL     }
    };

    m_pATRspTable = pATRspTable;

    RIL_LOG_VERBOSE("CSilo_SIM::CSilo_SIM() - Exit\r\n");
}

//
//
CSilo_SIM::~CSilo_SIM()
{
    RIL_LOG_VERBOSE("CSilo_SIM::~CSilo_SIM() - Enter\r\n");
    RIL_LOG_VERBOSE("CSilo_SIM::~CSilo_SIM() - Exit\r\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
//  Parse functions here
///////////////////////////////////////////////////////////////////////////////////////////////

BOOL CSilo_SIM::PreParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp)
{
    RIL_LOG_VERBOSE("CSilo_SIM::PreParseResponseHook() - Enter\r\n");
    BOOL bRetVal = TRUE;
    
    switch(rpCmd->GetRequestID())
    {
        case ND_REQ_ID_ENTERSIMPIN:
        case ND_REQ_ID_ENTERSIMPUK:
        case ND_REQ_ID_ENTERSIMPUK2:
        case ND_REQ_ID_ENTERNETWORKDEPERSONALIZATION:
        case ND_REQ_ID_CHANGESIMPIN:
        case ND_REQ_ID_CHANGESIMPIN2:
        case ND_REQ_ID_SETFACILITYLOCK:
            bRetVal = ParsePin(rpCmd, rpRsp);
            break;

        case ND_REQ_ID_SIMIO:
            bRetVal = ParseSimIO(rpCmd, rpRsp);
            break;

        //case ND_REQ_ID_GETSIMSTATUS:
        //    bRetVal = ParseSimStatus(rpCmd, rpRsp);
        //    break;

        default:
            break;
    }

Error:
    RIL_LOG_VERBOSE("CSilo_SIM::PreParseResponseHook() - Exit\r\n");
    return bRetVal;
}

BOOL CSilo_SIM::PostParseResponseHook(CCommand*& rpCmd, CResponse*& rpRsp)
{
    RIL_LOG_VERBOSE("CSilo_SIM::PostParseResponseHook() - Enter\r\n");
    BOOL bRetVal = TRUE;
    
    switch(rpCmd->GetRequestID())
    {
        case ND_REQ_ID_GETSIMSTATUS:
            bRetVal = ParseSimStatus(rpCmd, rpRsp);
            break;

        default:
            break;
    }

Error:
    RIL_LOG_VERBOSE("CSilo_SIM::PostParseResponseHook() - Exit\r\n");
    return bRetVal;
}

// Helper functions
BOOL CSilo_SIM::ParsePin(CCommand*& rpCmd, CResponse*& rpRsp)
{
    BOOL bRetVal = FALSE;

    RIL_LOG_VERBOSE("CSilo_SIM::ParsePin() - Enter\r\n");

    if (RIL_E_SUCCESS != rpRsp->GetResultCode())
    {
        
        switch(rpRsp->GetErrorCode())
        {
            case CME_ERROR_INCORRECT_PASSWORD:
                RIL_LOG_INFO("CSilo_SIM::ParsePin() - Incorrect password");
                rpRsp->SetResultCode(RIL_E_PASSWORD_INCORRECT);
                break;
                
            case CME_ERROR_SIM_PUK_REQUIRED:
                RIL_LOG_INFO("CSilo_SIM::ParsePin() - SIM PUK required");
                rpRsp->SetResultCode(RIL_E_PASSWORD_INCORRECT);
                
                //  Set radio state to sim locked or absent.
                //  Note that when the radio state *changes*, the upper layers will query
                //  for more information.  This is why we cannot change the radio state for
                //  incorrect PIN (not PUKd yet) because state is
                //  "sim locked or absent" -> "sim locked or absent".
                //  But PIN -> PUK we have to do this otherwise phone will think we are still PIN'd.
                RIL_requestTimedCallback(notifySIMLocked, NULL, 0, 500000);
                break;
                
            case CME_ERROR_SIM_PUK2_REQUIRED:
                RIL_LOG_INFO("CSilo_SIM::ParsePin() - SIM PUK2 required");
                rpRsp->SetResultCode(RIL_E_PASSWORD_INCORRECT);
                
                //  Set radio state to sim locked or absent.
                //  Note that when the radio state *changes*, the upper layers will query
                //  for more information.  This is why we cannot change the radio state for
                //  incorrect PIN2 (not PUK2d yet) because state is
                //  "sim locked or absent" -> "sim locked or absent".
                //  But PIN2 -> PUK2 we have to do this otherwise phone will think we are still PIN2'd.
                RIL_requestTimedCallback(notifySIMLocked, NULL, 0, 500000);
                break;
            
            default:
                RIL_LOG_INFO("CSilo_SIM::ParsePin() - Unknown error [%d]", rpRsp->GetErrorCode());
                rpRsp->SetResultCode(RIL_E_GENERIC_FAILURE);
                break;
        }            

        rpRsp->FreeData();
        int* pInt = (int *) malloc(sizeof(int));
        
        if (NULL == pInt)
        {
            RIL_LOG_CRITICAL("CSilo_SIM::ParsePin() : Unable to allocate memory for SIM unlock retries\r\n");
            goto Error;
        }

        //  (Dec 22/09) I tried entering different values for this, but I don't think it does anything in the UI.
        *pInt = -1;

        if (!rpRsp->SetData((void*) pInt, sizeof(int*), FALSE))
        {
            RIL_LOG_CRITICAL("CSilo_SIM::ParsePin() : Unable to set data with number of SIM unlock retries left\r\n");
            delete pInt;
            pInt = NULL;
            goto Error;
        }
        
    }

    
    bRetVal = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_SIM::ParsePin() - Exit\r\n");
    return bRetVal;
}

BOOL CSilo_SIM::ParseSimIO(CCommand*& rpCmd, CResponse*& rpRsp)
{
    BOOL bRetVal = FALSE;

    RIL_LOG_VERBOSE("CSilo_SIM::ParseSimIO() - Enter\r\n");

    if (RIL_E_SUCCESS != rpRsp->GetResultCode())
    {
        switch(rpRsp->GetErrorCode())
        {
            case CME_ERROR_SIM_PIN2_REQUIRED:
                RIL_LOG_INFO("CSilo_SIM::ParseSimIO() - SIM PIN2 required");
                rpRsp->SetResultCode(RIL_E_SIM_PIN2);
                break;

            case CME_ERROR_SIM_PUK2_REQUIRED:
                RIL_LOG_INFO("CSilo_SIM::ParseSimIO() - SIM PUK2 required");
                rpRsp->SetResultCode(RIL_E_SIM_PUK2);
                break;

            default:
                RIL_LOG_INFO("CSilo_SIM::ParseSimIO() - Unknown error [%d]", rpRsp->GetErrorCode());
                rpRsp->SetResultCode(RIL_E_GENERIC_FAILURE);
                break;
        }            
    }
    
    bRetVal = TRUE;

Error:
    RIL_LOG_VERBOSE("CSilo_SIM::ParseSimIO() - Exit\r\n");
    return bRetVal;
}

BOOL CSilo_SIM::ParseSimStatus(CCommand*& rpCmd, CResponse*& rpRsp)
{
    RIL_LOG_VERBOSE("CSilo_SIM::ParseSimStatus() - Enter\r\n");
        
    BOOL bRetVal = FALSE;
    if (RIL_E_SUCCESS != rpRsp->GetResultCode())
    {
        RIL_LOG_INFO("CSilo_SIM::ParseSimStatus() : Found CME Error on AT+CPIN? request.\r\n");

        switch (rpRsp->GetErrorCode())
        {
            case RRIL_CME_ERROR_SIM_ABSENT:
            {
                RIL_LOG_INFO("CSilo_SIM::ParseSimStatus() : SIM Card is absent!\r\n");
                rpRsp->FreeData();
                rpRsp->SetResultCode(RIL_E_SUCCESS);
    
                RIL_CardStatus* pCardStatus = (RIL_CardStatus *) malloc(sizeof(RIL_CardStatus));
                if (NULL == pCardStatus)
                {
                    RIL_LOG_CRITICAL("CSilo_SIM::ParseSimStatus() : Unable to allocate memory for RIL_CardStatus\r\n");
                    goto Error;
                }
                memset(pCardStatus, 0, sizeof(RIL_CardStatus));
    
                // Initialize as per reference ril as insufficient documentation currently is available
                pCardStatus->gsm_umts_subscription_app_index = RIL_CARD_MAX_APPS;
                pCardStatus->cdma_subscription_app_index = RIL_CARD_MAX_APPS;
                pCardStatus->universal_pin_state = RIL_PINSTATE_UNKNOWN;
                pCardStatus->card_state = RIL_CARDSTATE_ABSENT;
                pCardStatus->num_applications = 0;
    
                // Don't copy the memory, just pass along the pointer as is.
                if (!rpRsp->SetData((void*) pCardStatus, sizeof(RIL_CardStatus*), FALSE))
                {
                    RIL_LOG_CRITICAL("CSilo_SIM::ParseSimStatus() : Unable to set data with sim state\r\n");
                    delete pCardStatus;
                    pCardStatus = NULL;
                    goto Error;
                }
            }
            break;
            
            case RRIL_CME_ERROR_NOT_READY:
            {
                RIL_LOG_INFO("CSilo_SIM::ParseSimStatus() : SIM Card is not ready!\r\n");
                rpRsp->FreeData();
                rpRsp->SetResultCode(RIL_E_SUCCESS);
    
                RIL_CardStatus* pCardStatus = (RIL_CardStatus*) malloc(sizeof(RIL_CardStatus));
                if (NULL == pCardStatus)
                {
                    RIL_LOG_CRITICAL("CSilo_SIM::ParseSimStatus() : Unable to allocate memory for RIL_CardStatus\r\n");
                    goto Error;
                }
                memset(pCardStatus, 0, sizeof(RIL_CardStatus));
    
                // Initialize as per reference ril as insufficient documentation currently is available
                pCardStatus->gsm_umts_subscription_app_index = 0;
                pCardStatus->cdma_subscription_app_index = RIL_CARD_MAX_APPS;
                pCardStatus->universal_pin_state = RIL_PINSTATE_UNKNOWN;
                pCardStatus->card_state = RIL_CARDSTATE_PRESENT;
                pCardStatus->num_applications = 1;
    
                pCardStatus->applications[0].app_type = RIL_APPTYPE_SIM;
                pCardStatus->applications[0].app_state = RIL_APPSTATE_DETECTED;
                pCardStatus->applications[0].perso_substate = RIL_PERSOSUBSTATE_UNKNOWN;
                pCardStatus->applications[0].aid_ptr = NULL;
                pCardStatus->applications[0].app_label_ptr = NULL;
                pCardStatus->applications[0].pin1_replaced = 0;
                pCardStatus->applications[0].pin1 = RIL_PINSTATE_UNKNOWN;
                pCardStatus->applications[0].pin2 = RIL_PINSTATE_UNKNOWN;
    
                // Don't copy the memory, just pass along the pointer as is.
                if (!rpRsp->SetData((void*) pCardStatus, sizeof(RIL_CardStatus*), FALSE))
                {
                    RIL_LOG_CRITICAL("CSilo_SIM::ParseSimStatus() : Unable to set data with sim state\r\n");
                    delete pCardStatus;
                    pCardStatus = NULL;
                    goto Error;
                }
            }
            
            default:
                break;
        }
    }

    bRetVal = TRUE;
Error:
    RIL_LOG_VERBOSE("CSilo_SIM::ParseSimStatus() - Exit\r\n");

    return bRetVal;
}

