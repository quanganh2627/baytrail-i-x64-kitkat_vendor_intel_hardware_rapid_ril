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
CRadioState::CRadioState() : m_eRadioState(RADIO_STATE_UNAVAILABLE),
                            m_eSIMState(RADIO_STATE_SIM_LOCKED_OR_ABSENT),
                            m_bIsRadioOn(FALSE)
{
    RIL_LOG_VERBOSE("CRadioState::CRadioState() - Enter / Exit\r\n");
}

///////////////////////////////////////////////////////////////////////////////
CRadioState::~CRadioState()
{
    RIL_LOG_VERBOSE("CRadioState::~CRadioState() - Enter / Exit\r\n");
}

///////////////////////////////////////////////////////////////////////////////
RIL_RadioState CRadioState::GetRadioState()
{
    return m_eRadioState;
}

///////////////////////////////////////////////////////////////////////////////
RIL_RadioState CRadioState::GetSIMState()
{
    return m_eSIMState;
}

///////////////////////////////////////////////////////////////////////////////
void CRadioState::TriggerStatusChange()
{
    if (TRUE == m_bIsRadioOn)
    {
        m_eRadioState = m_eSIMState;
    }
    else
    {
        m_eRadioState = RADIO_STATE_OFF;
    }

    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
}

///////////////////////////////////////////////////////////////////////////////
void CRadioState::SetRadioState(BOOL bIsRadioOn)
{
    m_bIsRadioOn = bIsRadioOn;

    TriggerStatusChange();
}

///////////////////////////////////////////////////////////////////////////////
void CRadioState::SetSIMState(RIL_RadioState eSIMState)
{
    RIL_LOG_INFO("CRadioState::SetSIMState  m_eSIMState: %d, eSIMState: %d, m_eRadioState: %d\r\n",
                m_eSIMState, eSIMState, m_eRadioState);

    /**
     * Incase of NO_SIM => LOCKED SIM INSERTED, we need to
     * somehow force the upper layer to query the SIM status.
     * Currently, upper layer is not responding to the SIM status
     * change at all.
     * This is because SimCard functionality in the Android telephony
     * framework registers only for sim_locked_or_absent event which
     * is triggered whenever there is a radio state change.
     * Since the previous radio state and current state for the
     * SIM ABSENT => SIM LOCKED case is same
     * (RADIO_STATE_SIM_LOCKED_OR_ABSENT), sim_locked_or_absent status
     * is not triggered at resulting in GET_SIM_STATUS and
     * QUERY_FACILITY_LOCK requests not triggered. Fix has been done in
     * Telephony framework to not check for previous and current state
     * before triggering the SIM state changes.
     */
    m_eSIMState = eSIMState;

    TriggerStatusChange();
}

