////////////////////////////////////////////////////////////////////////////
// rril.h
//
// Copyright 2005-2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides RIL structures and constants.
//
// Author:  Filip Petrovic
// Created: 2009-05-11
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  May 11/09  FP       1.xx  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////
#ifndef RRIL_RRIL_H
#define RRIL_RRIL_H

#include <telephony/ril.h>


///////////////////////////////////////////////////////////////////////////////
// Typedefs
//
typedef long                RIL_CMD_ID;
typedef long                RIL_RESULT_CODE;


////////////////////////////////////////////////////////////////////////////////
// Android / WM structs conversion
//

#define WAIT_FOREVER            (0xFFFFFFFF)






///////////////////////////////////////////////////////////////////////////////
// Number of Calls to Track
//
#define RRIL_MAX_CALL_ID_COUNT       (10)

///////////////////////////////////////////////////////////////////////////////
// Number of Call Forwarding Entries to support
//
#define RIL_MAX_CALLFWD_ENTRIES     10

///////////////////////////////////////////////////////////////////////////////
// Maximum length for various buffers and string parameters
//
#define MAX_BUFFER_SIZE     (1024)
#define MAX_PROP_VALUE      (81)

///////////////////////////////////////////////////////////////////////////////
// Registration States
//
// Check 3GPP 27.007 V6.3.0 section 7.2
enum
{
    E_REGISTRATION_NOT_REGISTERED_NOT_SEARCHING = 0,
    E_REGISTRATION_REGISTERED_HOME_NETWORK,
    E_REGISTRATION_NOT_REGISTERED_SEARCHING,
    E_REGISTRATION_DENIED,
    E_REGISTRATION_UNKNOWN,
    E_REGISTRATION_REGISTERED_ROAMING
};

///////////////////////////////////////////////////////////////////////////////
// SIM States
//
enum
{
    E_SIM_READY = 1,
    E_SIM_PIN,
    E_SIM_PIN2,
    E_SIM_PUK,
    E_SIM_PUK2,
    E_SIM_PH_NETWORK,
    E_SIM_ABSENT
};


///////////////////////////////////////////////////////////////////////////////
// Notify / Result Codes (m_dwCode)
//

#define RRIL_RESULT_OK                  RIL_E_SUCCESS                // 0x00000000
#define RRIL_RESULT_RADIOOFF            RIL_E_RADIO_NOT_AVAILABLE    // 0x00000001
#define RRIL_RESULT_ERROR               RIL_E_GENERIC_FAILURE        // 0x00000002
#define RRIL_RESULT_PASSWORDINCORRECT   RIL_E_PASSWORD_INCORRECT     // 0x00000003
#define RRIL_RESULT_SIMPIN2             RIL_E_SIM_PIN2               // 0x00000004
#define RRIL_RESULT_SIMPUK2             RIL_E_SIM_PUK2               // 0x00000005
#define RRIL_RESULT_NOTSUPPORTED        RIL_E_REQUEST_NOT_SUPPORTED  // 0x00000006
#define RRIL_RESULT_CALLABORTED         RIL_E_CANCELLED              // 0x00000007

// V25 Results
#define RRIL_RESULT_NOCARRIER      RIL_E_GENERIC_FAILURE
#define RRIL_RESULT_NODIALTONE     RIL_E_GENERIC_FAILURE
#define RRIL_RESULT_BUSY           RIL_E_GENERIC_FAILURE
#define RRIL_RESULT_NOANSWER       RIL_E_GENERIC_FAILURE
#define RRIL_NOTIFY_CONNECT        (0x00010000)


///////////////////////////////////////////////////////////////////////////////
// Error codes (in pBlobs) - used to trigger actions during response handling
//

#define RRIL_E_NO_ERROR                             (0x00000000)
#define RRIL_E_UNKNOWN_ERROR                        (0x00001000)
#define RRIL_E_ABORT                                (0x00001002)
#define RRIL_E_DIALSTRING_TOO_LONG                  (0x00001003)
#define RRIL_E_MODEM_NOT_READY                      (0x00001004)
#define RRIL_E_TIMED_OUT                            (0x00001005)
#define RRIL_E_CANCELLED                            (0x00001006)
#define RRIL_E_RADIOOFF                             (0x00001007)


///////////////////////////////////////////////////////////////////////////////
// Information Classes
//
#define RRIL_INFO_CLASS_NONE                        (0x00000000)      // None
#define RRIL_INFO_CLASS_VOICE                       (0x00000001)      // Voice
#define RRIL_INFO_CLASS_DATA                        (0x00000002)      // Data
#define RRIL_INFO_CLASS_FAX                         (0x00000004)      // Fax
#define RRIL_INFO_CLASS_SMS                         (0x00000008)      // Short Message Service
#define RRIL_INFO_CLASS_DATA_CIRCUIT_SYNC           (0x00000010)      // Data Circuit Sync
#define RRIL_INFO_CLASS_DATA_CIRCUIT_ASYNC          (0x00000020)      // Data Circuit Async
#define RRIL_INFO_CLASS_DEDICATED_PACKET_ACCESS     (0x00000040)      // Dedicated packet access
#define RRIL_INFO_CLASS_DEDICATED_PAD_ACCESS        (0x00000080)      // Dedicated PAD access
#define RRIL_INFO_CLASS_ALL                         (0x000000FF)      // All Infoclasses

///////////////////////////////////////////////////////////////////////////////
// Call Forwarding defines
//
#define RRIL_CALL_FWD_UNCONDITIONAL                 (0x00000000)
#define RRIL_CALL_FWD_MOBILE_BUSY                   (0x00000001)
#define RRIL_CALL_FWD_NO_REPLY                      (0x00000002)
#define RRIL_CALL_FWD_NOT_REACHABLE                 (0x00000003)
#define RRIL_CALL_FWD_ALL_CALL_FWD                  (0x00000004)
#define RRIL_CALL_FWD_ALL_COND_CALL_FWD             (0x00000005)

///////////////////////////////////////////////////////////////////////////////
// Call Waiting Status defines
//
#define RRIL_CALL_WAIT_DISABLE                      (0x00000000)
#define RRIL_CALL_WAIT_ENABLE                       (0x00000001)
#define RRIL_CALL_WAIT_QUERY_STATUS                 (0x00000002)

///////////////////////////////////////////////////////////////////////////////
// Caller ID Restriction defines
//
#define RRIL_CLIR_NETWORK                           (0x00000000)
#define RRIL_CLIR_INVOCATION                        (0x00000001)
#define RRIL_CLIR_SUPPRESSION                       (0x00000002)

///////////////////////////////////////////////////////////////////////////////
// CME Error code defines
//
#define RRIL_CME_ERROR_SIM_ABSENT                   10
#define RRIL_CME_ERROR_NOT_READY                    14

///////////////////////////////////////////////////////////////////////////////
// CMS Error code defines
//
#define RRIL_CMS_ERROR_SIM_ABSENT                   310

#endif // RRIL_RRIL_H

