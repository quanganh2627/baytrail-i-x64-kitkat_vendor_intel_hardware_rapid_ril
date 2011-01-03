////////////////////////////////////////////////////////////////////////////
// rril_OEM.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Header providing structs and defines used in the OEM
//
// Author:  Mike Worth
// Created: 2009-09-30
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Sept 30/09  FV      1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_RRIL_OEM_H
#define RRIL_RRIL_OEM_H

typedef struct s_request_data
{
    char szCmd1[512];           // AT command buffer to be sent to modem
    char szCmd2[512];           // AT command buffer to send after cmd1 returns if populated

    unsigned long uiTimeout;    // Time to wait for response in milliseconds
    unsigned long uiRetries;    // If command fails, retry this many times.
    bool fForceParse;           // Always calls associated parse function

    void * pContextData;        // Point it to any object you will need during
                                // the parsing process that otherwise is not available.
                                // Can be left NULL if not required

    unsigned int cbContextData;  // Size in bytes of the context data object 
} REQUEST_DATA;

typedef struct s_response_data
{
    char * szResponse;          // AT response string from modem
    void * pData;               // Point to blob of memory containing response expected by
                                // upper layers
                                
    unsigned long uiDataSize;   // Size of response blob

    unsigned long uiChannel;    // Channel the response was received on

    void * pContextData;        // Points to object given in request phase or NULL
                                // if not supplied

    unsigned int cbContextData; // Size in bytes of the context data object

} RESPONSE_DATA;

#endif
