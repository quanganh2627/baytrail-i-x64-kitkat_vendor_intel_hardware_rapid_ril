////////////////////////////////////////////////////////////////////////////
// radio_state.h
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//      Stores the current radio state and notifies upon state changes
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_RADIO_STATE_H
#define RRIL_RADIO_STATE_H

typedef enum {
        RRIL_RADIO_STATE_UNAVAILABLE = 0,
        RRIL_RADIO_STATE_OFF = 1,
        RRIL_RADIO_STATE_ON = 2
} RRIL_Radio_State;

typedef enum {
        RRIL_SIM_STATE_NOT_READY = 0,
        RRIL_SIM_STATE_LOCKED_OR_ABSENT = 1,
        RRIL_SIM_STATE_READY = 2
} RRIL_SIM_State;


class CRadioState
{
public:
    CRadioState();
    ~CRadioState();

    RIL_RadioState GetRadioState();
    RIL_RadioState GetSIMState();

    void SetRadioState(const RRIL_Radio_State eRadioState);
    void SetSIMState(const RRIL_SIM_State eSIMState);
    void SetToUnavailable();

private:
    RRIL_Radio_State m_eRadioState;
    RRIL_SIM_State m_eSIMState;

    const char* PrintState(const RIL_RadioState eState);

};

#endif // RRIL_RADIO_STATE_H
