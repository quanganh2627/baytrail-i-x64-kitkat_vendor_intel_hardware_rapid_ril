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
// Author:  Mike Worth
// Created: 2009-06-26
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  June 26/09  MW       1.00  Established v1.00 based on current code base.
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
