////////////////////////////////////////////////////////////////////////////
// reset.h
//
// Copyright 2005-2011 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Implementation of modem reset.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_RESET_H
#define RRIL_RESET_H


#include "../../../IFX-modem/stmd.h"

enum eRadioError
{
    eRadioError_ForceShutdown,      //  Critical error occured
    eRadioError_RequestCleanup,     //  General request clean up
    eRadioError_LowMemory,          //  Couldn't allocate memory
    eRadioError_ChannelDead,        //  Modem non-responsive
    eRadioError_InitFailure,        //  AT command init sequence failed
    eRadioError_OpenPortFailure,    //  Couldn't open the tty ports successfully

};

void ModemResetUpdate();
extern BOOL g_bSpoofCommands;


void do_request_clean_up(eRadioError eError, UINT32 uiLineNum, const char* lpszFileName);
extern BOOL CreateModemWatchdogThread();


#endif // RRIL_RESET_H

