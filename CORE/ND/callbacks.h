////////////////////////////////////////////////////////////////////////////
// callbacks.h
//
// Copyright 2005-2008 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Headers for all functions provided to RIL_requestTimedCallback
//
// Author:  Mike Worth
// Created: 2009-10-20
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Oct 20/09  MW       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef CALLBACKS_H
#define CALLBACKS_H

//
// Used to determine frequency of callstate polling
//
static const struct timeval CallStateSlowPoll = {0, 5000000};
static const struct timeval CallStateHyperPoll = {0, 500000};

//
// Callback to trigger radio state changed
//
void notifySIMLocked(void *param);

//
// Callback to trigger call state update
//
void notifyChangedCallState(void *param);

//
// Callback to trigger SIM status changed
//
void notifySIMStatusChanged(void *param);

//
// Callback to trigger signal strength update
//
void triggerSignalStrength(void *param);

//
// Callback to send incoming SMS acknowledgement
//
void triggerSMSAck(void *param);

//
// Callback to send USSD notification
//
void triggerUSSDNotification(void *param);


//
// Callback to get data call list
//
void triggerDataCallListChanged(void *param);

#endif
