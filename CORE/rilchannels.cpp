////////////////////////////////////////////////////////////////////////////
// rilchannels.cpp
//
// Copyright 2011 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Map Android AT command requests to DLC channels.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "rilchannels.h"

// Upper limit on number of RIL channels to create
UINT32 g_uiRilChannelUpperLimit = RIL_CHANNEL_MAX;

// Current RIL channel index maximum (depends on number of data channels created)
UINT32 g_uiRilChannelCurMax = 0;

//  Channel mapping array
UINT32* g_arChannelMapping;

UINT32 g_arChannelMappingDefault[REQ_ID_TOTAL] =
{
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_GETSIMSTATUS                         = 0,
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_ENTERSIMPIN,                         // 1
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_ENTERSIMPUK,                         // 2
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_ENTERSIMPIN2,                        // 3
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_ENTERSIMPUK2,                        // 4
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_CHANGESIMPIN,                        // 5
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_CHANGESIMPIN2,                       // 6
    RIL_CHANNEL_DLC2,  //ND_REQ_ID_ENTERNETWORKDEPERSONALIZATION,       // 7
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_GETCURRENTCALLS,                     // 8
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_DIAL,                                // 9
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_GETIMSI,                             // 10
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_HANGUP,                              // 11
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_HANGUPWAITINGORBACKGROUND,           // 12
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_HANGUPFOREGROUNDRESUMEBACKGROUND,    // 13
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_SWITCHHOLDINGANDACTIVE,              // 14
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_CONFERENCE,                          // 15
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_UDUB,                                // 16
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_LASTCALLFAILCAUSE,                   // 17
    RIL_CHANNEL_DLC2,  //ND_REQ_ID_SIGNALSTRENGTH,                      // 18
    RIL_CHANNEL_DLC2,  //ND_REQ_ID_REGISTRATIONSTATE,                   // 19
    RIL_CHANNEL_DLC2,  //ND_REQ_ID_GPRSREGISTRATIONSTATE,               // 20
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_OPERATOR,                            // 21
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_RADIOPOWER,                          // 22
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_DTMF,                                // 23
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_SENDSMS,                             // 24
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_SENDSMSEXPECTMORE,                   // 25
    RIL_CHANNEL_DATA1, //ND_REQ_ID_SETUPDEFAULTPDP,                     // 26
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_SIMIO,                               // 27
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_SENDUSSD,                            // 28
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_CANCELUSSD,                          // 29
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_GETCLIR,                             // 30
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_SETCLIR,                             // 31
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_QUERYCALLFORWARDSTATUS,              // 32
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_SETCALLFORWARD,                      // 33
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_QUERYCALLWAITING,                    // 34
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_SETCALLWAITING,                      // 35
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_SMSACKNOWLEDGE,                      // 36
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_GETIMEI,                             // 37
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_GETIMEISV,                           // 38
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_ANSWER,                              // 39
    RIL_CHANNEL_DLC2,  //ND_REQ_ID_DEACTIVATEDATACALL,                  // 40
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_QUERYFACILITYLOCK,                   // 41
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_SETFACILITYLOCK,                     // 42
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_CHANGEBARRINGPASSWORD,               // 43
    RIL_CHANNEL_DLC2,  //ND_REQ_ID_QUERYNETWORKSELECTIONMODE,           // 44
    RIL_CHANNEL_DLC2,  //ND_REQ_ID_SETNETWORKSELECTIONAUTOMATIC,        // 45
    RIL_CHANNEL_DLC2,  //ND_REQ_ID_SETNETWORKSELECTIONMANUAL,           // 46
    RIL_CHANNEL_DLC2,  //ND_REQ_ID_QUERYAVAILABLENETWORKS,              // 47
    RIL_CHANNEL_ATCMD,  //ND_REQ_ID_REQUESTDTMFSTART,                    // 48
    RIL_CHANNEL_ATCMD,  //ND_REQ_ID_REQUESTDTMFSTOP,                     // 49
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_BASEBANDVERSION,                     // 50
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_SEPERATECONNECTION,                  // 51
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_SETMUTE,                             // 52
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_GETMUTE,                             // 53
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_QUERYCLIP,                           // 54
    RIL_CHANNEL_DLC2,  //ND_REQ_ID_LASTPDPFAILCAUSE,                    // 55
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_PDPCONTEXTLIST,                      // 56
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_RESETRADIO,                          // 57
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_OEMHOOKRAW,                          // 58
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_OEMHOOKSTRINGS,                      // 59
    RIL_CHANNEL_URC,   //ND_REQ_ID_SCREENSTATE,                         // 60
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_SETSUPPSVCNOTIFICATION,              // 61
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_WRITESMSTOSIM,                       // 62
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_DELETESMSONSIM,                      // 63
    RIL_CHANNEL_DLC2,  //ND_REQ_ID_SETBANDMODE,                         // 64
    RIL_CHANNEL_DLC2,  //ND_REQ_ID_QUERYAVAILABLEBANDMODE,              // 65
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_STKGETPROFILE,                       // 66
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_STKSETPROFILE,                       // 67
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_STKSENDENVELOPECOMMAND,              // 68
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_STKSENDTERMINALRESPONSE,             // 69
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_STKHANDLECALLSETUPREQUESTEDFROMSIM,  // 70
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_EXPLICITCALLTRANSFER,                // 71
    RIL_CHANNEL_DLC2,  //ND_REQ_ID_SETPREFERREDNETWORKTYPE,             // 72
    RIL_CHANNEL_DLC2,  //ND_REQ_ID_GETPREFERREDNETWORKTYPE,             // 73
    RIL_CHANNEL_DLC2,  //ND_REQ_ID_GETNEIGHBORINGCELLIDS,               // 74
    RIL_CHANNEL_URC,   //ND_REQ_ID_SETLOCATIONUPDATES,                  // 75
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_CDMASETSUBSCRIPTION,                 // 76
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_CDMASETROAMINGPREFERENCE,            // 77
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_CDMAQUERYROAMINGPREFERENCE,          // 78
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_SETTTYMODE,                          // 79
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_QUERYTTYMODE,                        // 80
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_CDMASETPREFERREDVOICEPRIVACYMODE,    // 81
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_CDMAQUERYPREFERREDVOICEPRIVACYMODE,  // 82
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_CDMAFLASH,                           // 83
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_CDMABURSTDTMF,                       // 84
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_CDMAVALIDATEAKEY,                    // 85
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_CDMASENDSMS,                         // 86
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_CDMASMSACKNOWLEDGE,                  // 87
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_GETBROADCASTSMSCONFIG,               // 88
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_SETBROADCASTSMSCONFIG,               // 89
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_SMSBROADCASTACTIVATION,              // 90
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_CDMAGETBROADCASTSMSCONFIG,           // 91
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_CDMASETBROADCASTSMSCONFIG,           // 92
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_CDMASMSBROADCASTACTIVATION,          // 93
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_CDMASUBSCRIPTION,                    // 94
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_CDMAWRITESMSTORUIM,                  // 95
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_CDMADELETESMSONRUIM,                 // 96
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_DEVICEIDENTITY,                      // 97
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_EXITEMERGENCYCALLBACKMODE,           // 98
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_GETSMSCADDRESS,                      // 99
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_SETSMSCADDRESS,                      // 100
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_REPORTSMSMEMORYSTATUS,               // 101
    RIL_CHANNEL_URC,   //ND_REQ_ID_REPORTSTKSERVICEISRUNNING,           // 102
    RIL_CHANNEL_DLC2,  //ND_REQ_ID_GETIPADDRESS,                        // 103
    RIL_CHANNEL_DLC2,  //ND_REQ_ID_GETDNS,                              // 104
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_QUERYPIN2,                           // 105
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_PDPCONTEXTLIST_UNSOL                 // 106
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_SIMTRANSMITBASIC                     // 107
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_SIMOPENCHANNEL                       // 108
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_SIMCLOSECHANNEL                      // 109
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_SIMTRANSMITCHANNEL                   // 110
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_QUERY_SIM_SMS_STORE_STATUS           // 111
    RIL_CHANNEL_DLC2,  //ND_REQ_ID_VOICERADIOTECH                       // 112
    RIL_CHANNEL_DLC8,  //ND_REQ_ID_SILENT_PIN_ENTRY                     // 113
    RIL_CHANNEL_DLC6,  //ND_REQ_ID_ACKINCOMINGSMSWITHPDU                // 114
#if defined(M2_VT_FEATURE_ENABLED)
    RIL_CHANNEL_DLC6, //ND_REQ_ID_HANGUPVT                              // 115
    RIL_CHANNEL_ATCMD, //ND_REQ_ID_DIALVT                               // 116
#endif  // M2_VT_FEATURE_ENABLED
#if defined(M2_GET_SIM_SMS_STORAGE_ENABLED)
    RIL_CHANNEL_DLC6, // ND_REQ_ID GET_SIM_SMS_STORAGE                  // 115 or 117
#endif
};
