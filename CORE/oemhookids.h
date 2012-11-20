////////////////////////////////////////////////////////////////////////////
// oemhookids.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    List of enums for specific OEM requests via API
//    RIL_REQUEST_OEM_HOOK_STRINGS API.
//
/////////////////////////////////////////////////////////////////////////////


#pragma once

#include "types.h"

#pragma pack(1)

/**
 * This enum details the additional requests (OEM) to pass to the RIL
 * via the RIL_REQUEST_OEM_HOOK_RAW, RIL_REQUEST_OEM_HOOK_STRINGS APIs.
 */

//  The first int (4 bytes) of the byte[] is the command ID.  The data follows.
//
// The command id allocation is the following:
//      0x00000001 -> 0x0000009F : Product Specific
//      0x000000A0 -> 0x000000CF : Platform Requests
//      0x000000D0 -> 0x000000FF : Platform Unsolicited
//      0x00050000 -> 0x000500FF : ENZO specific
//      0x00080000 -> 0x000800FF : ENZO specific
//      0x000A0000 -> 0x000A00FF : ENZO specific
//

///////////////////////////////////////////////////////////////////////////////
typedef struct TAG_OEM_HOOK_RAW_GAN_RIL
{
    int nCommand; //  Command ID
} sOEM_HOOK_RAW_GAN_RIL;

//
//  RIL_OEM_HOOK_RAW_GAN_RIL
//  Command ID = 0x00000001
//
//  This is reserved for future implementation.
//
const int RIL_OEM_HOOK_RAW_GAN_RIL = 0x00000001;

///////////////////////////////////////////////////////////////////////////////

//
//  RIL_OEM_HOOK_STRING_THERMAL_GET_SENSOR
//  Command ID = 0x000000A2
//
// "data" - Sensor Id
//              “0” = RF temperature sensor.
//              “1” = Baseband chip temperature sensor.
//              “3” = PCB temperature sensor.
// "response" - String containg the temperatures separated by space
//              "Filtered temperature Raw Temperature". Both temperature
//              are formated as a 4 digits number: "2300" = 23.00 celcius
//              degrees  (two digits after dot).
//
const int RIL_OEM_HOOK_STRING_THERMAL_GET_SENSOR = 0x000000A2;

///////////////////////////////////////////////////////////////////////////////

//
//  RIL_OEM_HOOK_STRING_THERMAL_SET_THRESHOLD
//  Command ID = 0x000000A3
//
// "data" - String containing the following values separated by a space
//          boolean activate
//                    true - Activates the threshold notification
//                    false - Deactivates the threshold notification
//          int sensorId
//                    “0” = RF temperature sensor.
//                    “1” = Baseband chip temperature sensor.
//                    “3” = PCB temperature sensor.
//          int minTemperature
//                    Temperature formated as a 4 digit number.
//                    "2300" = 23.00 celcius degrees  (two digits after dot)
//          int maxTemperature: Same as minTemperature
//
//  "response" = NULL
//
const int RIL_OEM_HOOK_STRING_THERMAL_SET_THRESHOLD = 0x000000A3;

///////////////////////////////////////////////////////////////////////////////

//
//  RIL_OEM_HOOK_STRING_SET_MODEM_AUTO_FAST_DORMANCY
//  Command ID = 0x000000A4
//
// "data" - String containing the following values separated by a space
//          boolean FDEnable
//                    true - Activates the autonomous fast dormancy
//                    false - Deactivates the autonomous fast dormancy
//          int delayTimer
//                    integer value in range of 1 to 60 (seconds)
//          int SCRI Timer
//                    integer value in range of 1 to 120 (seconds)
//  "response" = NULL
//
const int RIL_OEM_HOOK_STRING_SET_MODEM_AUTO_FAST_DORMANCY = 0x000000A4;

///////////////////////////////////////////////////////////////////////////////

//
//  RIL_OEM_HOOK_STRING_GET_ATR
//  Command ID = 0x000000A5
//
//  This command sends AT+XGATR to the modem.
//
//  "data" = NULL
//  "response" = Answer to Reset
//
const int RIL_OEM_HOOK_STRING_GET_ATR = 0x000000A5;

///////////////////////////////////////////////////////////////////////////////

//
//  RIL_OEM_HOOK_STRING_GET_GPRS_CELL_ENV
//  Command ID = 0x000000A6
//
//  This command sends AT+CGED to the modem.
//
//  "data" = NULL
//  "response" = string containing dump of GPRS cell environment
//
const int RIL_OEM_HOOK_STRING_GET_GPRS_CELL_ENV = 0x000000A6;

///////////////////////////////////////////////////////////////////////////////

//
//  RIL_OEM_HOOK_STRING_DEBUG_SCREEN_COMMAND
//  Command ID = 0x000000A7
//
//  This command sends AT+XCGEDPAGE to the modem.
//
//  "data" = An array of null-terminated UTF-8 strings:
//          ((const char **)data)[0] - mode
//               0 - one shot dump
//               3 - reset statistics of all pages (page_nr ignored)
//               4 - STOP the EM (page_nr ignored)
//
//          ((const char **)data)[1] - Number of response page (page_nr)
//
//  "response" = string containing dump of the cell environment debug screen
//
const int RIL_OEM_HOOK_STRING_DEBUG_SCREEN_COMMAND = 0x000000A7;

///////////////////////////////////////////////////////////////////////////////

//
//  RIL_OEM_HOOK_STRING_RELEASE_ALL_CALLS
//  Command ID = 0x000000A8
//
//  This command sends AT+CHLD=8 to the modem.
//
//  "data" = NULL
//  "response" = NULL
//
const int RIL_OEM_HOOK_STRING_RELEASE_ALL_CALLS = 0x000000A8;

///////////////////////////////////////////////////////////////////////////////

//
//  RIL_OEM_HOOK_STRING_GET_SMS_TRANSPORT_MODE
//  Command ID = 0x000000A9
//
//  This command retrieves the current SMS transport mode.
//
//  "data" = NULL
//  "response" - String containing the value of the service according to the
//               27.007 10.1.21
//               “0” = Packet Domain.
//               “1” = Circuit Switched.
//               “2” = Packet Domain Preferred.
//               “3” = Circuit Switched Preferred.
//
const int RIL_OEM_HOOK_STRING_GET_SMS_TRANSPORT_MODE = 0x000000A9;

///////////////////////////////////////////////////////////////////////////////

//
//  RIL_OEM_HOOK_STRING_SET_SMS_TRANSPORT_MODE
//  Command ID = 0x000000AA
//
//  This command sets the SMS transport mode.
//
// "data" - int service
//                    “0” = Packet Domain.
//                    “1” = Circuit Switched.
//                    “2” = Packet Domain Preferred.
//                    “3” = Circuit Switched Preferred.
//  "response" = NULL
//
const int RIL_OEM_HOOK_STRING_SET_SMS_TRANSPORT_MODE = 0x000000AA;

///////////////////////////////////////////////////////////////////////////////

#if defined(M2_DUALSIM_FEATURE_ENABLED)

//
//  RIL_OEM_HOOK_STRING_SWAP_PS
//  Command ID = 0x000000B2
//
//  This command sends AT+XRAT=8 to the modem.
//
//  "data" = NULL
//  "response" = Error code returned by AT command
//
const int RIL_OEM_HOOK_STRING_SWAP_PS = 0x000000B2;

#endif // M2_DUALSIM_FEATURE_ENABLED

///////////////////////////////////////////////////////////////////////////////

typedef struct TAG_OEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND
{
    int nCommand; //  Command ID
    int nResultCode; // result code
    int nSensorId; // sensor id
    int nTemp; // temperature
} sOEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND;

//
//  RIL_OEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND
//  Command ID = 0x000000D0
//
//  "data" = sOEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND
//  "response" = NULL
//
const int RIL_OEM_HOOK_RAW_UNSOL_THERMAL_ALARM_IND = 0x000000D0;

///////////////////////////////////////////////////////////////////////////////

#if defined(M2_DUALSIM_FEATURE_ENABLED)

//
//  OEM_HOOK_RAW_UNSOL_FAST_OOS_IND
//  Command ID = 0x000000D1
//
//  "data" is a byte[] containing the command id
//  data[0] = command id
//
const int RIL_OEM_HOOK_RAW_UNSOL_FAST_OOS_IND = 0x000000D1;

//
//  OEM_HOOK_RAW_UNSOL_IN_SERVICE_IND
//  Command ID = 0x000000D2
//
//  "data" is a byte[] containing the command id
//  data[0] = command id
//
const int RIL_OEM_HOOK_RAW_UNSOL_IN_SERVICE_IND = 0x000000D2;

///////////////////////////////////////////////////////////////////////////////

#endif // M2_DUALSIM_FEATURE_ENABLED

typedef struct TAG_OEM_HOOK_RAW_UNSOL_DATA_STATUS_IND
{
    int command; //  Command ID
    int status; // result code
} sOEM_HOOK_RAW_UNSOL_DATA_STATUS_IND;

//
//  OEM_HOOK_RAW_UNSOL_DATA_STATUS_IND
//  Command ID = 0x000000D3
//
//  "data" is sOEM_HOOK_RAW_UNSOL_DATA_STATUS_IND
//
const int RIL_OEM_HOOK_RAW_UNSOL_DATA_STATUS_IND = 0x000000D3;

///////////////////////////////////////////////////////////////////////////////

typedef struct TAG_OEM_HOOK_RAW_UNSOL_MT_CLASS_IND
{
    int command; //  Command ID
    int mt_class; // result code
} sOEM_HOOK_RAW_UNSOL_MT_CLASS_IND;

//
//  OEM_HOOK_RAW_UNSOL_MT_CLASS_IND
//  Command ID = 0x000000D4
//
//  "data" is sOEM_HOOK_RAW_UNSOL_MT_CLASS_IND
//
const int RIL_OEM_HOOK_RAW_UNSOL_MT_CLASS_IND = 0x000000D4;

#pragma pack()

/***********************************************************************/


