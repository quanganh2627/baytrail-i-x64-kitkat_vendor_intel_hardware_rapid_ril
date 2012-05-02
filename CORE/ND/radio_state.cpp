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
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "rril.h"
#include "rillog.h"
#include "rildmain.h"
#include "radio_state.h"


///////////////////////////////////////////////////////////////////////////////
CRadioState::CRadioState() : m_eRadioState(RRIL_RADIO_STATE_UNAVAILABLE),
                            m_eSIMState(RRIL_SIM_STATE_LOCKED_OR_ABSENT)
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
    RIL_RadioState radioState;
    switch (m_eRadioState)
    {
    case RRIL_RADIO_STATE_UNAVAILABLE:
        radioState =  RADIO_STATE_UNAVAILABLE;
        break;
    case RRIL_RADIO_STATE_OFF:
        radioState = RADIO_STATE_OFF;
        break;
    case RRIL_RADIO_STATE_ON:
        switch (m_eSIMState)
        {
        case RRIL_SIM_STATE_LOCKED_OR_ABSENT:
            radioState = RADIO_STATE_SIM_LOCKED_OR_ABSENT;
            break;
        case RRIL_SIM_STATE_READY:
            radioState = RADIO_STATE_SIM_READY;
            break;
        case RRIL_SIM_STATE_NOT_READY:
            radioState = RADIO_STATE_SIM_NOT_READY;
            break;
        default:
            radioState = RADIO_STATE_OFF;
        }
        break;
    default:
        radioState =  RADIO_STATE_UNAVAILABLE;
    }

    RIL_LOG_INFO("[RIL STATE] RADIO STATE = %s\r\n", PrintState(radioState));

    return radioState;
}

///////////////////////////////////////////////////////////////////////////////
RIL_RadioState CRadioState::GetSIMState()
{
    RIL_RadioState simState;
    switch (m_eSIMState)
    {
    case RRIL_SIM_STATE_NOT_READY:
        simState = RADIO_STATE_SIM_NOT_READY;
        break;
    case RRIL_SIM_STATE_LOCKED_OR_ABSENT:
        simState = RADIO_STATE_SIM_LOCKED_OR_ABSENT;
        break;
    case RRIL_SIM_STATE_READY:
        simState = RADIO_STATE_SIM_READY;
        break;
    default:
        simState = RADIO_STATE_SIM_LOCKED_OR_ABSENT;
    }
    RIL_LOG_INFO("[RIL STATE] SIM STATE = %s\r\n", PrintState(simState));

    return simState;
}

///////////////////////////////////////////////////////////////////////////////
void CRadioState::SetRadioState(const RRIL_Radio_State eRadioState)
{
    if (m_eRadioState != eRadioState) {
        m_eRadioState = eRadioState;
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
    }
}

///////////////////////////////////////////////////////////////////////////////
void CRadioState::SetSIMState(const RRIL_SIM_State eSIMState)
{
    RIL_LOG_INFO("CRadioState::SetSIMState  m_eSIMState: %d, eSIMState: %d, m_eRadioState: %d\r\n",
                m_eSIMState, eSIMState, m_eRadioState);

    if (m_eSIMState != eSIMState) {
        m_eSIMState = eSIMState;
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
    }
}

///////////////////////////////////////////////////////////////////////////////
const char* CRadioState::PrintState(const RIL_RadioState eState)
{
    switch (eState)
    {
        case RADIO_STATE_OFF:
            return "RADIO_STATE_OFF";
        case RADIO_STATE_UNAVAILABLE:
            return "RADIO_STATE_UNAVAILABLE";
        case RADIO_STATE_SIM_NOT_READY:
            return "RADIO_STATE_SIM_NOT_READY";
        case RADIO_STATE_SIM_LOCKED_OR_ABSENT:
            return "RADIO_STATE_SIM_LOCKED_OR_ABSENT";
        case RADIO_STATE_SIM_READY:
            return "RADIO_STATE_SIM_READY";
        case RADIO_STATE_RUIM_NOT_READY:
            return "RADIO_STATE_RUIM_NOT_READY";
        case RADIO_STATE_RUIM_READY:
            return "RADIO_STATE_RUIM_READY";
        case RADIO_STATE_RUIM_LOCKED_OR_ABSENT:
            return "RADIO_STATE_RUIM_LOCKED_OR_ABSENT";
        case RADIO_STATE_NV_NOT_READY:
            return "RADIO_STATE_NV_NOT_READY";
        case RADIO_STATE_NV_READY:
            return "RADIO_STATE_NV_READY";
        default:
            return "UNKNOWN";
    }
}

