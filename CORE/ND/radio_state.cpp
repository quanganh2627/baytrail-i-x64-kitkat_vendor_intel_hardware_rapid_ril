////////////////////////////////////////////////////////////////////////////
// radio_state.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//      Stores the current radio state and notifies upon state changes
//
// Author:  Mike Worth
// Created: 2009-06-26
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  June 22/09  MW       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "rril.h"
#include "rillog.h"
#include "rildmain.h"
#include "radio_state.h"


///////////////////////////////////////////////////////////////////////////////
CRadioState::CRadioState()
{
    RIL_LOG_VERBOSE("CRadioState::CRadioState() - Enter\r\n");
    m_rgRadioState[RRIL_RADIO_STATE_OFF]                     = TRUE;
    m_rgRadioState[RRIL_RADIO_STATE_UNAVAILABLE]             = FALSE;
    m_rgRadioState[RRIL_RADIO_STATE_SMS_READY]               = FALSE;
    m_rgRadioState[RRIL_RADIO_STATE_SIM_PB_READY]            = FALSE;
    m_rgRadioState[RRIL_RADIO_STATE_SIM_LOCKED_OR_ABSENT]    = FALSE;
    m_rgRadioState[RRIL_RADIO_STATE_SIM_READY]               = FALSE;

    m_LastStateSet = RRIL_RADIO_STATE_OFF;
    m_fAllowPowerStateChange = TRUE;

    RIL_LOG_VERBOSE("CRadioState::CRadioState() - Exit\r\n");
}

///////////////////////////////////////////////////////////////////////////////
CRadioState::~CRadioState()
{
    RIL_LOG_VERBOSE("CRadioState::CRadioState() - Enter / Exit\r\n");
}

///////////////////////////////////////////////////////////////////////////////
RIL_RadioState CRadioState::GetRadioState()
{
    RIL_RadioState sState;

    if (m_rgRadioState[RRIL_RADIO_STATE_OFF])
    {
        RIL_LOG_INFO("CRadioState::GetRadioState() - Radio off!\r\n");
        sState = RADIO_STATE_OFF;
    }
    // Radio must be on!
    else if (m_rgRadioState[RRIL_RADIO_STATE_UNAVAILABLE])
    {
        RIL_LOG_INFO("CRadioState::GetRadioState() - Radio on but is not available!\r\n");
        sState = RADIO_STATE_UNAVAILABLE;
    }
    // Radio is available!
    else if (m_rgRadioState[RRIL_RADIO_STATE_SIM_LOCKED_OR_ABSENT])
    {
        RIL_LOG_INFO("CRadioState::GetRadioState() - Radio on - SIM locked or absent!\r\n");
        sState = RADIO_STATE_SIM_LOCKED_OR_ABSENT;
    }
    // Radio is on and SIM is not locked!
    else if (m_rgRadioState[RRIL_RADIO_STATE_SMS_READY] && 
             m_rgRadioState[RRIL_RADIO_STATE_SIM_PB_READY] &&
             m_rgRadioState[RRIL_RADIO_STATE_SIM_READY])
    {
        RIL_LOG_INFO("CRadioState::GetRadioState() - Radio on - SIM ready!\r\n");
        sState = RADIO_STATE_SIM_READY;
    }
    // Some part of the SIM isn't ready
    else
    {
        RIL_LOG_INFO("CRadioState::GetRadioState() - Radio on - SIM not ready!\r\n");
        sState = RADIO_STATE_SIM_NOT_READY;
    }

    return sState;
}

///////////////////////////////////////////////////////////////////////////////
void CRadioState::SetRadioOff()
{
    RIL_LOG_INFO("CRadioState::SetRadioOff()\r\n");
    m_rgRadioState[RRIL_RADIO_STATE_OFF]                     = TRUE;
    m_rgRadioState[RRIL_RADIO_STATE_UNAVAILABLE]             = FALSE;
    m_rgRadioState[RRIL_RADIO_STATE_SMS_READY]               = FALSE;
    m_rgRadioState[RRIL_RADIO_STATE_SIM_PB_READY]            = FALSE;
    m_rgRadioState[RRIL_RADIO_STATE_SIM_LOCKED_OR_ABSENT]    = FALSE;
    m_rgRadioState[RRIL_RADIO_STATE_SIM_READY]               = FALSE;

    if (IsRadioStateChanged(RRIL_RADIO_STATE_OFF))
    {
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
    }
}

///////////////////////////////////////////////////////////////////////////////
void CRadioState::SetRadioOn()
{
    RIL_LOG_INFO("CRadioState::SetRadioOn()\r\n");
    m_rgRadioState[RRIL_RADIO_STATE_OFF]                     = FALSE;

    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
}

///////////////////////////////////////////////////////////////////////////////
void CRadioState::SetRadioAvailable()
{
    RIL_LOG_INFO("CRadioState::SetRadioAvailable()\r\n");
    m_rgRadioState[RRIL_RADIO_STATE_UNAVAILABLE]             = FALSE;

    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
}

///////////////////////////////////////////////////////////////////////////////
void CRadioState::SetRadioUnavailable(BOOL fResetState)
{
    RIL_LOG_INFO("CRadioState::SetRadioUnavailable()\r\n");
    m_rgRadioState[RRIL_RADIO_STATE_UNAVAILABLE]             = TRUE;
    if (fResetState)
    {
        m_rgRadioState[RRIL_RADIO_STATE_SMS_READY]           = FALSE;
        m_rgRadioState[RRIL_RADIO_STATE_SIM_PB_READY]        = FALSE;
    }

    if (IsRadioStateChanged(RRIL_RADIO_STATE_UNAVAILABLE))
    {
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
    }
}

///////////////////////////////////////////////////////////////////////////////
void CRadioState::SetRadioSMSReady()
{
    RIL_LOG_INFO("CRadioState::SetRadioSMSReady()\r\n");
    m_rgRadioState[RRIL_RADIO_STATE_SMS_READY]               = TRUE;

    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
}

///////////////////////////////////////////////////////////////////////////////
void CRadioState::SetRadioSIMPBReady()
{
    RIL_LOG_INFO("CRadioState::SetRadioSIMPBReady()\r\n");
    m_rgRadioState[RRIL_RADIO_STATE_SIM_PB_READY]            = TRUE;

    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
}

///////////////////////////////////////////////////////////////////////////////
void CRadioState::SetRadioSIMLocked()
{
    RIL_LOG_INFO("CRadioState::SetRadioSIMLocked()\r\n");
    m_rgRadioState[RRIL_RADIO_STATE_SIM_LOCKED_OR_ABSENT]    = TRUE;

    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
}

///////////////////////////////////////////////////////////////////////////////
void CRadioState::SetRadioSIMAbsent()
{
    RIL_LOG_INFO("CRadioState::SetRadioSIMAbsent()\r\n");
    m_rgRadioState[RRIL_RADIO_STATE_SIM_LOCKED_OR_ABSENT]    = TRUE;

    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
}
    

///////////////////////////////////////////////////////////////////////////////
void CRadioState::SetRadioSIMUnlocked()
{
    RIL_LOG_INFO("CRadioState::SetRadioSIMUnlocked()\r\n");
    m_rgRadioState[RRIL_RADIO_STATE_UNAVAILABLE]             = FALSE;
    m_rgRadioState[RRIL_RADIO_STATE_SIM_LOCKED_OR_ABSENT]    = FALSE;

    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
}

///////////////////////////////////////////////////////////////////////////////
void CRadioState::SetRadioSIMReady()
{
    RIL_LOG_INFO("CRadioState::SetRadioSIMReady()\r\n");
    m_rgRadioState[RRIL_RADIO_STATE_UNAVAILABLE]             = FALSE;
    m_rgRadioState[RRIL_RADIO_STATE_SIM_READY]               = TRUE;

    if (IsRadioStateChanged(RRIL_RADIO_STATE_SIM_READY))
    {
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOL CRadioState::IsRadioStateChanged(RADIO_STATE_INDEX newState)
{
    BOOL fRet = FALSE;

    if (newState != m_LastStateSet)
    {
        RIL_LOG_INFO("IsRadioStateChanged() - State has changed, sending notification!\r\n");
        m_LastStateSet = newState;
        fRet = TRUE;
    }
    else
    {
        RIL_LOG_INFO("IsRadioStateChanged() - State has not changed, ignoring update\r\n");
    }

    return fRet;
}

///////////////////////////////////////////////////////////////////////////////
void CRadioState::DisablePowerStateChange()
{
    RIL_LOG_INFO("CRadioState::DisablePowerStateChange()\r\n");
    m_fAllowPowerStateChange = FALSE;
}

///////////////////////////////////////////////////////////////////////////////
void CRadioState::EnablePowerStateChange()
{
    RIL_LOG_INFO("CRadioState::EnablePowerStateChange()\r\n");
    m_fAllowPowerStateChange = TRUE;
}

