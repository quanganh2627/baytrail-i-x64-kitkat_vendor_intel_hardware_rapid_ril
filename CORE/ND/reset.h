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
    ePCache_Code_NOK_NoPINAvailable = -3,
    ePCache_Code_NOK_WrongIntegrity = -2,
    ePCache_Code_NOK_InvalidUICC = -1,
    ePCache_Code_NOK = 0,
    ePCache_Code_OK = 1
};

ePCache_Code PCache_Store_PIN(char *szUICC, char *szPIN);
ePCache_Code PCache_Get_PIN(char *szUICC, char *szPINOut);
ePCache_Code PCache_Clear();

ePCache_Code PCache_SetUseCachedPIN(bool bFlag);
bool PCache_GetUseCachedPIN();

const char szRIL_usecachedpin[] = "ril.usecachedpin";
const char szRIL_cachedpin[] = "ril.cachedpin";
const char szRIL_cacheduicc[] = "ril.cacheduicc";


#endif // RRIL_RESET_H

