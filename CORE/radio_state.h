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

class CRadioState
{
private:
    RIL_RadioState m_eRadioState;
    RIL_RadioState m_eSIMState;
    BOOL m_bIsRadioOn;

    const char* PrintRadioState(RIL_RadioState eSIMState) const;

public:
    CRadioState();
    ~CRadioState();

    RIL_RadioState GetRadioState();
    RIL_RadioState GetSIMState();

    void SetRadioState(BOOL bIsRadioOnState);
    void SetSIMState(RIL_RadioState state);

private:
    void TriggerStatusChange();
};

#endif // RRIL_RADIO_STATE_H
