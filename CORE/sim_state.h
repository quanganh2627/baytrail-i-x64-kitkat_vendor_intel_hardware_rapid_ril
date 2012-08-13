////////////////////////////////////////////////////////////////////////////
// sim_state.h
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//      Stores the current SIM state
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_SIM_STATE_H
#define RRIL_SIM_STATE_H

typedef enum {
    RRIL_SIM_STATE_NOT_AVAILABLE = 0,
    RRIL_SIM_STATE_NOT_READY = 1,
    RRIL_SIM_STATE_READY = 2
    // ADD NEW STATES HERE
} RRIL_SIM_State;

class CSimState
{
public:
    CSimState();
    ~CSimState();

    RRIL_SIM_State GetSIMState();

    void SetSIMState(const RRIL_SIM_State eRadioState);

private:
    RRIL_SIM_State m_eSIMState;

    const char* PrintState(const RRIL_SIM_State eState);
};

#endif // RRIL_SIM_STATE_H
