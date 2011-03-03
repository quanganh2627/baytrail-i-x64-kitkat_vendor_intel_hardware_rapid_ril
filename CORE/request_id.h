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
// Author:  Mike Worth
// Created: 2009-06-22
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  June  22/09  DP     1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_REQUEST_ID_H
#define RRIL_REQUEST_ID_H

// Note: The WM_REQ_ID_XXX items must be in sync with the CELLCORE registry APIInfo.
enum WM_REQ_ID_ID {
    WM_REQ_ID_NONE                          = -1,
    WM_REQ_ID_GETSUBSCRIBERNUMBERS          = 0,
    WM_REQ_ID_GETOPERATORLIST,              // 1
    WM_REQ_ID_GETPREFERREDOPERATORLIST,     // 2
    WM_REQ_ID_ADDPREFERREDOPERATOR,         // 3
    WM_REQ_ID_REMOVEPREFERREDOPERATOR,      // 4
    WM_REQ_ID_GETCURRENTOPERATOR,           // 5
    WM_REQ_ID_REGISTERONNETWORK,            // 6
    WM_REQ_ID_UNREGISTERFROMNETWORK,        // 7
    WM_REQ_ID_GETREGISTRATIONSTATUS,        // 8
    WM_REQ_ID_GETCALLERIDSETTINGS,          // 9
    WM_REQ_ID_SETCALLERIDSTATUS,            // 10
    WM_REQ_ID_GETHIDEIDSETTINGS,            // 11
    WM_REQ_ID_SETHIDEIDSTATUS,              // 12
    WM_REQ_ID_GETDIALEDIDSETTINGS,          // 13
    WM_REQ_ID_SETDIALEDIDSTATUS,            // 14
    WM_REQ_ID_GETCLOSEDGROUPSETTINGS,       // 15
    WM_REQ_ID_SETCLOSEDGROUPSETTINGS,       // 16
    WM_REQ_ID_GETCALLFORWARDINGSETTINGS,    // 17
    WM_REQ_ID_ADDCALLFORWARDING,            // 18
    WM_REQ_ID_REMOVECALLFORWARDING,         // 19
    WM_REQ_ID_SETCALLFORWARDINGSTATUS,      // 20
    WM_REQ_ID_GETCALLWAITINGSETTINGS,       // 21
    WM_REQ_ID_SETCALLWAITINGSTATUS,         // 22
    WM_REQ_ID_SENDSUPSERVICEDATA,           // 23
    WM_REQ_ID_CANCELSUPSERVICEDATASESSION,  // 24
    WM_REQ_ID_DIAL,                         // 25
    WM_REQ_ID_ANSWER,                       // 26
    WM_REQ_ID_HANGUP,                       // 27
    WM_REQ_ID_SENDDTMF,                     // 28
    WM_REQ_ID_SETDTMFMONITORING,            // 29
    WM_REQ_ID_GETCALLLIST,                  // 30
    WM_REQ_ID_MANAGECALLS,                  // 31
    WM_REQ_ID_TRANSFERCALL,                 // 32
    WM_REQ_ID_GETLINESTATUS,                // 33
    WM_REQ_ID_GETAUDIOGAIN,                 // 34
    WM_REQ_ID_SETAUDIOGAIN,                 // 35
    WM_REQ_ID_GETAUDIODEVICES,              // 36
    WM_REQ_ID_SETAUDIODEVICES,              // 37
    WM_REQ_ID_GETAUDIOMUTING,               // 38
    WM_REQ_ID_SETAUDIOMUTING,               // 39
    WM_REQ_ID_GETHSCSDOPTIONS,              // 40
    WM_REQ_ID_SETHSCSDOPTIONS,              // 41
    WM_REQ_ID_GETHSCSDCALLSETTINGS,         // 42
    WM_REQ_ID_GETDATACOMPRESSION,           // 43
    WM_REQ_ID_SETDATACOMPRESSION,           // 44
    WM_REQ_ID_GETERRORCORRECTION,           // 45
    WM_REQ_ID_SETERRORCORRECTION,           // 46
    WM_REQ_ID_GETBEARERSERVICEOPTIONS,      // 47
    WM_REQ_ID_SETBEARERSERVICEOPTIONS,      // 48
    WM_REQ_ID_GETRLPOPTIONS,                // 49
    WM_REQ_ID_SETRLPOPTIONS,                // 50
    WM_REQ_ID_GETMSGSERVICEOPTIONS,         // 51
    WM_REQ_ID_SETMSGSERVICEOPTIONS,         // 52
    WM_REQ_ID_GETMSGCONFIG,                 // 53
    WM_REQ_ID_SETMSGCONFIG,                 // 54
    WM_REQ_ID_RESTOREMSGCONFIG,             // 55
    WM_REQ_ID_SAVEMSGCONFIG,                // 56
    WM_REQ_ID_READMSG,                      // 57
    WM_REQ_ID_DELETEMSG,                    // 58
    WM_REQ_ID_WRITEMSG,                     // 59
    WM_REQ_ID_SENDMSG,                      // 60
    WM_REQ_ID_SENDSTOREDMSG,                // 61
    WM_REQ_ID_SENDMSGACKNOWLEDGEMENT,       // 62
    WM_REQ_ID_GETUSERIDENTITY,              // 63
    WM_REQ_ID_GETPHONELOCKEDSTATE,          // 64
    WM_REQ_ID_UNLOCKPHONE,                  // 65
    WM_REQ_ID_GETLOCKINGSTATUS,             // 66
    WM_REQ_ID_SETLOCKINGSTATUS,             // 67
    WM_REQ_ID_CHANGELOCKINGPASSWORD,        // 68
    WM_REQ_ID_GETCALLBARRINGSTATUS,         // 69
    WM_REQ_ID_SETCALLBARRINGSTATUS,         // 70
    WM_REQ_ID_CHANGECALLBARRINGPASSWORD,    // 71
    WM_REQ_ID_GETEQUIPMENTINFO,             // 72
    WM_REQ_ID_GETEQUIPMENTSTATE,            // 73
    WM_REQ_ID_SETEQUIPMENTSTATE,            // 74
    WM_REQ_ID_GETPHONEBOOKOPTIONS,          // 75
    WM_REQ_ID_SETPHONEBOOKOPTIONS,          // 76
    WM_REQ_ID_READPHONEBOOKENTRIES,         // 77
    WM_REQ_ID_WRITEPHONEBOOKENTRY,          // 78
    WM_REQ_ID_DELETEPHONEBOOKENTRY,         // 79
    WM_REQ_ID_SENDSIMCMD,                   // 80
    WM_REQ_ID_SENDRESTRICTEDSIMCMD,         // 81
    WM_REQ_ID_GETSIMRECORDSTATUS,           // 82
    WM_REQ_ID_GETSIMTOOLKITPROFILE,         // 83
    WM_REQ_ID_SETSIMTOOLKITPROFILE,         // 84
    WM_REQ_ID_SENDSIMTOOLKITENVELOPECMD,    // 85
    WM_REQ_ID_FETCHSIMTOOLKITCMD,           // 86
    WM_REQ_ID_SENDSIMTOOLKITCMDRESPONSE,    // 87
    WM_REQ_ID_TERMINATESIMTOOLKITSESSION,   // 88
    WM_REQ_ID_GETCOSTINFO,                  // 89
    WM_REQ_ID_SETCOSTINFO,                  // 90
    WM_REQ_ID_GETSIGNALQUALITY,             // 91
    WM_REQ_ID_GETCELLTOWERINFO,             // 92
    WM_REQ_ID_DEVSPECIFIC,                  // 93
    WM_REQ_ID_GETDEVCAPS,                   // 94
    WM_REQ_ID_GETHIDECONNECTEDIDSETTINGS,   // 95
    WM_REQ_ID_SETHIDECONNECTEDIDSTATUS,     // 96
    WM_REQ_ID_GETUUSREQUIREDSTATUS,         // 97
    WM_REQ_ID_CLEARCCBSREGISTRATION,        // 98
    WM_REQ_ID_GETSYSTEMTIME,                    // 99
    WM_REQ_ID_GETGPRSCONTEXTLIST,               // 100
    WM_REQ_ID_SETGPRSCONTEXT,                   // 101
    WM_REQ_ID_DELETEGPRSCONTEXT,                // 102
    WM_REQ_ID_GETREQUESTEDQUALITYOFSERVICELIST, // 103
    WM_REQ_ID_SETREQUESTEDQUALITYOFSERVICE,     // 104
    WM_REQ_ID_DELETEREQUESTEDQUALITYOFSERVICE,  // 105
    WM_REQ_ID_GETMINIMUMQUALITYOFSERVICELIST,   // 106
    WM_REQ_ID_SETMINIMUMQUALITYOFSERVICE,       // 107
    WM_REQ_ID_DELETEMINIMUMQUALITYOFSERVICE,    // 108
    WM_REQ_ID_SETGPRSATTACHED,                  // 109
    WM_REQ_ID_GETGPRSATTACHED,                  // 110
    WM_REQ_ID_SETGPRSCONTEXTACTIVATED,          // 111
    WM_REQ_ID_GETGPRSCONTEXTACTIVATEDLIST,      // 112
    WM_REQ_ID_ENTERGPRSDATAMODE,                // 113
    WM_REQ_ID_GETGPRSADDRESS,                   // 114
    WM_REQ_ID_GPRSANSWER,                       // 115
    WM_REQ_ID_GETGPRSREGISTRATIONSTATUS,        // 116
    WM_REQ_ID_GETGPRSCLASS,                     // 117
    WM_REQ_ID_SETGPRSCLASS,                     // 118
    WM_REQ_ID_GETMOSMSSERVICE,                  // 119
    WM_REQ_ID_SETMOSMSSERVICE,                  // 120
    WM_REQ_ID_GETCBMSGCONFIG,                   // 121
    WM_REQ_ID_SETCBMSGCONFIG,                   // 122
    WM_REQ_ID_GETCURRENTADDRESSID,              // 123
    WM_REQ_ID_SETCURRENTADDRESSID,              // 124
    WM_REQ_ID_GETPACKETBYTECOUNT,               // 125
    WM_REQ_ID_RESETPACKETBYTECOUNT,             // 126
    WM_REQ_ID_GETCURRENTPRIVACYSTATUS,          // 127
    WM_REQ_ID_GETCURRENTSYSTEMTYPE,             // 128
    WM_REQ_ID_GETALLOPERATORSLIST,              // 129
    WM_REQ_ID_REBOOTRADIO,                      // 130
    WM_REQ_ID_REGISTERATCOMMANDLOGGING,         // 131
    WM_REQ_ID_ATCOMMANDLOGFILE,                 // 132
    WM_REQ_ID_GETATR,                           // 133
    WM_REQ_ID_SENDSIMTOOLKITEVENTDOWNLOAD,      // 134
    WM_REQ_ID_LOGEVENTTORADIO,                  // 135
    WM_REQ_ID_GETVTSERIALPORTHANDLE,            // 136
    WM_REQ_ID_CHANGECALLBARRINGPASSWORDEX,      // 137
    WM_REQ_ID_GETNETWORKSELECTIONMODE,          // 138
    WM_REQ_ID_GETIMEI,                          // 139
    WM_REQ_ID_GETIMSI,                          // 140
    WM_REQ_ID_GETBASEBANDVERSION,               // 141
    WM_REQ_ID_REQUESTSETUPDEFAULTPDP,           // 142
    WM_REQ_ID_TOTAL                             // 143
};


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
    ND_REQ_ID_TOTAL                                // 106
};



#define REQ_ID_NONE        ND_REQ_ID_NONE
#define REQ_ID_TOTAL       ND_REQ_ID_TOTAL
typedef ND_REQ_ID_ID       REQ_ID;



#endif // RRIL_REQUEST_ID_H
