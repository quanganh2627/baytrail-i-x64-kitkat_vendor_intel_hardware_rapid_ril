////////////////////////////////////////////////////////////////////////////
// request_id.h
//
// Copyright 2005-2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the request constants.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_REQUEST_ID_H
#define RRIL_REQUEST_ID_H

// Note: g_szRequestNames[] in repository.cpp must contain an appropriate value for each valid Request ID.
enum ND_REQ_ID_ID {
    ND_REQ_ID_NONE                                 = -1,
    ND_REQ_ID_GETSIMSTATUS                         = 0,
    ND_REQ_ID_ENTERSIMPIN,                         // 1
    ND_REQ_ID_ENTERSIMPUK,                         // 2
    ND_REQ_ID_ENTERSIMPIN2,                        // 3
    ND_REQ_ID_ENTERSIMPUK2,                        // 4
    ND_REQ_ID_CHANGESIMPIN,                        // 5
    ND_REQ_ID_CHANGESIMPIN2,                       // 6
    ND_REQ_ID_ENTERNETWORKDEPERSONALIZATION,       // 7
    ND_REQ_ID_GETCURRENTCALLS,                     // 8
    ND_REQ_ID_DIAL,                                // 9
    ND_REQ_ID_GETIMSI,                             // 10
    ND_REQ_ID_HANGUP,                              // 11
    ND_REQ_ID_HANGUPWAITINGORBACKGROUND,           // 12
    ND_REQ_ID_HANGUPFOREGROUNDRESUMEBACKGROUND,    // 13
    ND_REQ_ID_SWITCHHOLDINGANDACTIVE,              // 14
    ND_REQ_ID_CONFERENCE,                          // 15
    ND_REQ_ID_UDUB,                                // 16
    ND_REQ_ID_LASTCALLFAILCAUSE,                   // 17
    ND_REQ_ID_SIGNALSTRENGTH,                      // 18
    ND_REQ_ID_REGISTRATIONSTATE,                   // 19
    ND_REQ_ID_GPRSREGISTRATIONSTATE,               // 20
    ND_REQ_ID_OPERATOR,                            // 21
    ND_REQ_ID_RADIOPOWER,                          // 22
    ND_REQ_ID_DTMF,                                // 23
    ND_REQ_ID_SENDSMS,                             // 24
    ND_REQ_ID_SENDSMSEXPECTMORE,                   // 25
    ND_REQ_ID_SETUPDEFAULTPDP,                     // 26
    ND_REQ_ID_SIMIO,                               // 27
    ND_REQ_ID_SENDUSSD,                            // 28
    ND_REQ_ID_CANCELUSSD,                          // 29
    ND_REQ_ID_GETCLIR,                             // 30
    ND_REQ_ID_SETCLIR,                             // 31
    ND_REQ_ID_QUERYCALLFORWARDSTATUS,              // 32
    ND_REQ_ID_SETCALLFORWARD,                      // 33
    ND_REQ_ID_QUERYCALLWAITING,                    // 34
    ND_REQ_ID_SETCALLWAITING,                      // 35
    ND_REQ_ID_SMSACKNOWLEDGE,                      // 36
    ND_REQ_ID_GETIMEI,                             // 37
    ND_REQ_ID_GETIMEISV,                           // 38
    ND_REQ_ID_ANSWER,                              // 39
    ND_REQ_ID_DEACTIVATEDATACALL,                  // 40
    ND_REQ_ID_QUERYFACILITYLOCK,                   // 41
    ND_REQ_ID_SETFACILITYLOCK,                     // 42
    ND_REQ_ID_CHANGEBARRINGPASSWORD,               // 43
    ND_REQ_ID_QUERYNETWORKSELECTIONMODE,           // 44
    ND_REQ_ID_SETNETWORKSELECTIONAUTOMATIC,        // 45
    ND_REQ_ID_SETNETWORKSELECTIONMANUAL,           // 46
    ND_REQ_ID_QUERYAVAILABLENETWORKS,              // 47
    ND_REQ_ID_REQUESTDTMFSTART,                    // 48
    ND_REQ_ID_REQUESTDTMFSTOP,                     // 49
    ND_REQ_ID_BASEBANDVERSION,                     // 50
    ND_REQ_ID_SEPERATECONNECTION,                  // 51
    ND_REQ_ID_SETMUTE,                             // 52
    ND_REQ_ID_GETMUTE,                             // 53
    ND_REQ_ID_QUERYCLIP,                           // 54
    ND_REQ_ID_LASTPDPFAILCAUSE,                    // 55
    ND_REQ_ID_PDPCONTEXTLIST,                      // 56
    ND_REQ_ID_RESETRADIO,                          // 57
    ND_REQ_ID_OEMHOOKRAW,                          // 58
    ND_REQ_ID_OEMHOOKSTRINGS,                      // 59
    ND_REQ_ID_SCREENSTATE,                         // 60
    ND_REQ_ID_SETSUPPSVCNOTIFICATION,              // 61
    ND_REQ_ID_WRITESMSTOSIM,                       // 62
    ND_REQ_ID_DELETESMSONSIM,                      // 63
    ND_REQ_ID_SETBANDMODE,                         // 64
    ND_REQ_ID_QUERYAVAILABLEBANDMODE,              // 65
    ND_REQ_ID_STKGETPROFILE,                       // 66
    ND_REQ_ID_STKSETPROFILE,                       // 67
    ND_REQ_ID_STKSENDENVELOPECOMMAND,              // 68
    ND_REQ_ID_STKSENDTERMINALRESPONSE,             // 69
    ND_REQ_ID_STKHANDLECALLSETUPREQUESTEDFROMSIM,  // 70
    ND_REQ_ID_EXPLICITCALLTRANSFER,                // 71
    ND_REQ_ID_SETPREFERREDNETWORKTYPE,             // 72
    ND_REQ_ID_GETPREFERREDNETWORKTYPE,             // 73
    ND_REQ_ID_GETNEIGHBORINGCELLIDS,               // 74
    ND_REQ_ID_SETLOCATIONUPDATES,                  // 75
    ND_REQ_ID_CDMASETSUBSCRIPTION,                 // 76
    ND_REQ_ID_CDMASETROAMINGPREFERENCE,            // 77
    ND_REQ_ID_CDMAQUERYROAMINGPREFERENCE,          // 78
    ND_REQ_ID_SETTTYMODE,                          // 79
    ND_REQ_ID_QUERYTTYMODE,                        // 80
    ND_REQ_ID_CDMASETPREFERREDVOICEPRIVACYMODE,    // 81
    ND_REQ_ID_CDMAQUERYPREFERREDVOICEPRIVACYMODE,  // 82
    ND_REQ_ID_CDMAFLASH,                           // 83
    ND_REQ_ID_CDMABURSTDTMF,                       // 84
    ND_REQ_ID_CDMAVALIDATEAKEY,                    // 85
    ND_REQ_ID_CDMASENDSMS,                         // 86
    ND_REQ_ID_CDMASMSACKNOWLEDGE,                  // 87
    ND_REQ_ID_GETBROADCASTSMSCONFIG,               // 88
    ND_REQ_ID_SETBROADCASTSMSCONFIG,               // 89
    ND_REQ_ID_SMSBROADCASTACTIVATION,              // 90
    ND_REQ_ID_CDMAGETBROADCASTSMSCONFIG,           // 91
    ND_REQ_ID_CDMASETBROADCASTSMSCONFIG,           // 92
    ND_REQ_ID_CDMASMSBROADCASTACTIVATION,          // 93
    ND_REQ_ID_CDMASUBSCRIPTION,                    // 94
    ND_REQ_ID_CDMAWRITESMSTORUIM,                  // 95
    ND_REQ_ID_CDMADELETESMSONRUIM,                 // 96
    ND_REQ_ID_DEVICEIDENTITY,                      // 97
    ND_REQ_ID_EXITEMERGENCYCALLBACKMODE,           // 98
    ND_REQ_ID_GETSMSCADDRESS,                      // 99
    ND_REQ_ID_SETSMSCADDRESS,                      // 100
    ND_REQ_ID_REPORTSMSMEMORYSTATUS,               // 101
    ND_REQ_ID_REPORTSTKSERVICEISRUNNING,           // 102
    ND_REQ_ID_GETIPADDRESS,                        // 103
    ND_REQ_ID_GETDNS,                              // 104
    ND_REQ_ID_QUERYPIN2,                           // 105
    ND_REQ_ID_PDPCONTEXTLIST_UNSOL,                // 106
    ND_REQ_ID_SIMTRANSMITBASIC,                    // 107
    ND_REQ_ID_SIMOPENCHANNEL,                      // 108
    ND_REQ_ID_SIMCLOSECHANNEL,                     // 109
    ND_REQ_ID_SIMTRANSMITCHANNEL,                  // 110
    ND_REQ_ID_QUERY_SIM_SMS_STORE_STATUS,          // 111
    ND_REQ_ID_VOICERADIOTECH,                      // 112
    ND_REQ_ID_SILENT_PIN_ENTRY,                    // 113
    ND_REQ_ID_ACKINCOMINGSMSWITHPDU,               // 114
#if defined(M2_VT_FEATURE_ENABLED)
    ND_REQ_ID_HANGUPVT,                            // 115
    ND_REQ_ID_DIALVT,                              // 116
#endif // M2_VT_FEATURE_ENABLED
#if defined(M2_GET_SIM_SMS_STORAGE_ENABLED)
    ND_REQ_ID_GET_SIM_SMS_STORAGE,                // 115 or 117
#endif
    ND_REQ_ID_TOTAL                                // 115 or 116, or 117
};



#define REQ_ID_NONE        ND_REQ_ID_NONE
#define REQ_ID_TOTAL       ND_REQ_ID_TOTAL
typedef ND_REQ_ID_ID       REQ_ID;



#endif // RRIL_REQUEST_ID_H
