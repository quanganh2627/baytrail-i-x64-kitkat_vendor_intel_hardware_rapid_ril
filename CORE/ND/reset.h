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

enum ePCache_Code
{
    NO_PIN_AVAILABLE = -4,
    WRONG_INTEGRITY = -3,
    INVALID_UICC = -2,
    NOK = -1,
    OK = 0
};

ePCache_Code PCache_Store_PIN(const char *szUICC, const char *szPIN);
ePCache_Code PCache_Get_PIN(const char *szUICC, char *szPIN);
ePCache_Code PCache_Clear();

ePCache_Code PCache_SetUseCachedPIN(bool bFlag);
bool PCache_GetUseCachedPIN();

const char szRIL_usecachedpin[] = "ril.usecachedpin";
const char szRIL_cachedpin[] = "ril.cachedpin";
const char szRIL_cacheduicc[] = "ril.cacheduicc";


#endif // RRIL_RESET_H

