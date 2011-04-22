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


typedef enum
{
    RRIL_RADIO_STATE_OFF             = 0,        // TRUE if radio is off (CFUN=4)
    RRIL_RADIO_STATE_UNAVAILABLE,                // TRUE if radio is rebooting or powering up
    RRIL_RADIO_STATE_SMS_READY,                  // TRUE if SMS functionality is ready (CPMS, CNMI sent)
    RRIL_RADIO_STATE_SIM_PB_READY,               // TRUE if SIM PB functionality is ready
    RRIL_RADIO_STATE_SIM_LOCKED_OR_ABSENT,       // TRUE if SIM is locked for PIN/PUK
    RRIL_RADIO_STATE_SIM_READY,                  // TRUE if SIM is ready (+CPIN: READY)
    RRIL_RADIO_STATE_TOTAL                       // Used to track number of states for radio
} RADIO_STATE_INDEX;

class CRadioState
{
private:
    BOOL                m_rgRadioState[RRIL_RADIO_STATE_TOTAL];
    BOOL                m_fAllowPowerStateChange;

public:

    CRadioState();
    ~CRadioState();

    RIL_RadioState   GetRadioState();

    // Updates the internal state to off and resets all flags
    void    SetRadioOff();

    // Sets the internal state to on but unavailable. All other flags remain unchanged.
    void    SetRadioOn();

    // Sets the radio as available
    void SetRadioAvailable();

    // Sets the radio to report unavailable. Used when we are turning the radio on or rebooting/re-initializing.
    void    SetRadioUnavailable(BOOL fResetState);

    // Set when the SIM Ready thread has initialized the SMS functionality
    void    SetRadioSMSReady();

    // Set when the SIM Ready thread has initialized the SIM Phonebook functionality
    void    SetRadioSIMPBReady();

    // Set if the SIM is PIN locked
    void    SetRadioSIMLocked();

    // Set when the SIM is not present
    void    SetRadioSIMAbsent();

    // Set if the SIM has been unlocked
    void    SetRadioSIMUnlocked();

    // Set if the SIM is ready
    void    SetRadioSIMReady();

    // The following are used to block the upper layers from turning the radio on
    // during a SetPreferredNetworkType request.
    void    DisablePowerStateChange();
    void    EnablePowerStateChange();
    BOOL    IsPowerStateChangeable()    { return m_fAllowPowerStateChange; }

    BOOL    IsRadioOff()            { return m_rgRadioState[RRIL_RADIO_STATE_OFF]; }
    BOOL    IsRadioUnavailable()    { return m_rgRadioState[RRIL_RADIO_STATE_UNAVAILABLE]; }
    BOOL    IsRadioSMSReady()       { return m_rgRadioState[RRIL_RADIO_STATE_SMS_READY]; }
    BOOL    IsRadioSIMPBReady()     { return m_rgRadioState[RRIL_RADIO_STATE_SIM_PB_READY]; }
    BOOL    IsRadioSIMLocked()      { return m_rgRadioState[RRIL_RADIO_STATE_SIM_LOCKED_OR_ABSENT]; }
    BOOL    IsRadioSIMReady()       { return m_rgRadioState[RRIL_RADIO_STATE_SIM_READY]; }
};

#endif // RRIL_RADIO_STATE_H
