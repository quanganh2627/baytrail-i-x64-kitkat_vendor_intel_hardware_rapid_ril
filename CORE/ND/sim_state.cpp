////////////////////////////////////////////////////////////////////////////
// sim_state.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//      Stores the current sim state and notifies upon state changes
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "rril.h"
#include "rillog.h"
#include "rildmain.h"
#include "sim_state.h"


///////////////////////////////////////////////////////////////////////////////
CSimState::CSimState() : m_eSIMState(RRIL_SIM_STATE_NOT_AVAILABLE)
{
    RIL_LOG_VERBOSE("CSimState::CSimState() - Enter / Exit\r\n");
}

///////////////////////////////////////////////////////////////////////////////
CSimState::~CSimState()
{
    RIL_LOG_VERBOSE("CSimState::~CSimState() - Enter / Exit\r\n");
}

///////////////////////////////////////////////////////////////////////////////
RRIL_SIM_State CSimState::GetSIMState()
{
    RIL_LOG_INFO("[RIL STATE] SIM STATE = %s\r\n", PrintState(m_eSIMState));

    return m_eSIMState;
}

///////////////////////////////////////////////////////////////////////////////
void CSimState::SetSIMState(const RRIL_SIM_State eSimState)
{
    if (m_eSIMState != eSimState) {
        m_eSIMState = eSimState;
    }
}

///////////////////////////////////////////////////////////////////////////////
const char* CSimState::PrintState(const RRIL_SIM_State eState)
{
    switch (eState)
    {
        case RRIL_SIM_STATE_NOT_AVAILABLE:
            return "RRIL_SIM_STATE_NOT_AVAILABLE";
        case RRIL_SIM_STATE_NOT_READY:
            return "RRIL_SIM_STATE_NOT_READY";
        case RRIL_SIM_STATE_READY:
            return "RRIL_SIM_STATE_READY";
        default:
            return "UNKNOWN";
    }
}

