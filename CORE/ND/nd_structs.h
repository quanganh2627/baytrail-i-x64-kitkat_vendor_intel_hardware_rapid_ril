////////////////////////////////////////////////////////////////////////////
// nd_structs.h
//
// Copyright 2005-2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//      Defines blobs for use in Android to support expected return structures
//      of pointer arrays to valid data
//
// Author:  Mike Worth
// Created: 2009-10-19
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Oct 19/09  MW       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef ND_STRUCTS_H
#define ND_STRUCTS_H

#include "rril.h"

//
// Struct for reporting Current Call List to Android
//
typedef struct
{
    RIL_Call*           pCallPointers      [RRIL_MAX_CALL_ID_COUNT];
    RIL_Call            pCallData          [RRIL_MAX_CALL_ID_COUNT];
    char                pCallNumberBuffers [RRIL_MAX_CALL_ID_COUNT][MAX_BUFFER_SIZE];
    char                pCallNameBuffers   [RRIL_MAX_CALL_ID_COUNT][MAX_BUFFER_SIZE];
} S_ND_CALL_LIST_DATA, *P_ND_CALL_LIST_DATA;


//
// Struct for reporting Current Call Forwarding to Android
//
typedef struct
{
    RIL_CallForwardInfo* pCallFwdPointers[RIL_MAX_CALLFWD_ENTRIES];
    RIL_CallForwardInfo  pCallFwdData[RIL_MAX_CALLFWD_ENTRIES];
    char                 pCallFwdBuffers[RIL_MAX_CALLFWD_ENTRIES][MAX_BUFFER_SIZE];
} S_ND_CALLFWD_DATA, *P_ND_CALLFWD_DATA;

//
// Structs for getting the available operator list to Android
//
typedef struct
{
    char * pszOpInfoLong;
    char * pszOpInfoShort;
    char * pszOpInfoNumeric;
    char * pszOpInfoStatus;
} S_ND_OPINFO_PTRS, *P_ND_OPINFO_PTRS;

typedef struct
{
    char szOpInfoLong[MAX_BUFFER_SIZE];
    char szOpInfoShort[MAX_BUFFER_SIZE];
    char szOpInfoNumeric[MAX_BUFFER_SIZE];
    char szOpInfoStatus[MAX_BUFFER_SIZE];
} S_ND_OPINFO_DATA, *P_ND_OPINFO_DATA;


#define RIL_MAX_BROADCASTSMSCONFIGINFO_ENTRIES     10
//
// Struct for reporting Broadcast SMS config to Android
//
typedef struct
{
    RIL_GSM_BroadcastSmsConfigInfo* pBroadcastSmsConfigInfoPointers[RIL_MAX_BROADCASTSMSCONFIGINFO_ENTRIES];
    RIL_GSM_BroadcastSmsConfigInfo BroadcastSmsConfigInfoData[RIL_MAX_BROADCASTSMSCONFIGINFO_ENTRIES];
} S_ND_BROADCASTSMSCONFIGINFO_DATA, *P_ND_BROADCASTSMSCONFIGINFO_DATA;

typedef struct
{
    RIL_SMS_Response smsRsp;
    char szAckPDU[160];
} S_ND_SEND_MSG, *P_ND_SEND_MSG;

struct PdpData
{
    char* szRadioTechnology;
    char* szRILDataProfile;
    char* szApn;
    char* szUserName;
    char* szPassword;
    char* szPAPCHAP;
    char* szPDPType;
};


//
// Struct for reporting Setup Data Call to Android
//
typedef struct
{
    char* pszCID;
    char* pszNetworkInterfaceName;
    char* pszIPAddress;
    char* pszDNS;       // Ignored prior to Android 3.0
    char* pszGateway;   // Ignored prior to Android 3.0
} S_ND_SETUP_DATA_CALL_POINTERS, *P_ND_SETUP_DATA_CALL_POINTERS;

typedef struct
{
    S_ND_SETUP_DATA_CALL_POINTERS sSetupDataCallPointers;
    char szCID[MAX_BUFFER_SIZE];
    char szNetworkInterfaceName[MAX_BUFFER_SIZE];
    char szIPAddress[MAX_BUFFER_SIZE];
    char szDNS[MAX_BUFFER_SIZE];        // Ignored prior to Android 3.0
    char szGateway[MAX_BUFFER_SIZE];    // Ignored prior to Android 3.0
} S_ND_SETUP_DATA_CALL, *P_ND_SETUP_DATA_CALL;



//
// Struct for reporting PDP Context List to Android
//
#define MAX_PDP_CONTEXTS                            ( 5)

typedef struct
{
    RIL_Data_Call_Response  pPDPData[MAX_PDP_CONTEXTS];
    char                    pTypeBuffers[MAX_PDP_CONTEXTS][MAX_BUFFER_SIZE];
    char                    pApnBuffers[MAX_PDP_CONTEXTS][MAX_BUFFER_SIZE];
    char                    pAddressBuffers[MAX_PDP_CONTEXTS][MAX_BUFFER_SIZE];
} S_ND_PDP_CONTEXT_DATA, *P_ND_PDP_CONTEXT_DATA;

//
// Struct for reporting Neighboring Cell List to Android
//
#define RRIL_MAX_CELL_ID_COUNT                      (30)
#define CELL_ID_ARRAY_LENGTH                        (9)

typedef struct
{
    RIL_NeighboringCell*     pnCellPointers      [RRIL_MAX_CELL_ID_COUNT];
    RIL_NeighboringCell      pnCellData          [RRIL_MAX_CELL_ID_COUNT];
    char                     pnCellCIDBuffers    [RRIL_MAX_CELL_ID_COUNT][CELL_ID_ARRAY_LENGTH];
} S_ND_N_CELL_DATA, *P_ND_N_CELL_DATA;


#define REG_STATUS_LENGTH 8

typedef struct
{
    char * pszStat;
    char * pszLAC;
    char * pszCID;
    char * pszNetworkType;
    char * pszBaseStationID;
    char * pszBaseStationLat;
    char * pszBaseStationLon;
    char * pszConcurrentServices;
    char * pszSystemID;
    char * pszNetworkID;
    char * pszTSB58;
    char * pszPRL;
    char * pszDefaultRoaming;
    char * pszReasonDenied;
} S_ND_REG_STATUS_POINTERS, *P_ND_REG_STATUS_POINTERS;

typedef struct
{
    S_ND_REG_STATUS_POINTERS sStatusPointers;
    char szStat[REG_STATUS_LENGTH];
    char szLAC[REG_STATUS_LENGTH];
    char szCID[REG_STATUS_LENGTH];
    char szNetworkType[REG_STATUS_LENGTH];
    char szBaseStationID[REG_STATUS_LENGTH];
    char szBaseStationLat[REG_STATUS_LENGTH];
    char szBaseStationLon[REG_STATUS_LENGTH];
    char szConcurrentServices[REG_STATUS_LENGTH];
    char szSystemID[REG_STATUS_LENGTH];
    char szNetworkID[REG_STATUS_LENGTH];
    char szTSB58[REG_STATUS_LENGTH];
    char szPRL[REG_STATUS_LENGTH];
    char szDefaultRoaming[REG_STATUS_LENGTH];
    char szReasonDenied[REG_STATUS_LENGTH];
} S_ND_REG_STATUS, *P_ND_REG_STATUS;

typedef struct
{
    char * pszStat;
    char * pszLAC;
    char * pszCID;
    char * pszNetworkType;
} S_ND_GPRS_REG_STATUS_POINTERS, *P_ND_GPRS_REG_STATUS_POINTERS;

typedef struct
{
    S_ND_GPRS_REG_STATUS_POINTERS sStatusPointers;
    char szStat[REG_STATUS_LENGTH];
    char szLAC[REG_STATUS_LENGTH];
    char szCID[REG_STATUS_LENGTH];
    char szNetworkType[REG_STATUS_LENGTH];
} S_ND_GPRS_REG_STATUS, *P_ND_GPRS_REG_STATUS;


typedef struct
{
    char * pszOpNameLong;
    char * pszOpNameShort;
    char * pszOpNameNumeric;
} S_ND_OP_NAME_POINTERS, *P_ND_OP_NAME_POINTERS;

typedef struct
{
    S_ND_OP_NAME_POINTERS sOpNamePtrs;
    char szOpNameLong[MAX_BUFFER_SIZE];
    char szOpNameShort[MAX_BUFFER_SIZE];
    char szOpNameNumeric[MAX_BUFFER_SIZE];
} S_ND_OP_NAMES, *P_ND_OP_NAMES;

enum ACCESS_TECHNOLOGY
{
    ACT_UNKNOWN = 0,
    ACT_GPRS = 1,
    ACT_EDGE = 2,
    ACT_UMTS = 3,
    ACT_IS95A = 4,
    ACT_IS95B = 5,
    ACT_1xRTT = 6,
    ACT_EVDO_0 = 7,
    ACT_EVDO_1 = 8,
    ACT_HSDPA = 9,
    ACT_HSUPA = 10,
    ACT_HSPA = 11
};

typedef struct
{
    BYTE* pszType;
    BYTE* pszMessage;
} S_ND_USSD_POINTERS, *P_ND_USSD_POINTERS;

typedef struct
{
    S_ND_USSD_POINTERS sStatusPointers;
    BYTE szType[MAX_BUFFER_SIZE];
    BYTE szMessage[MAX_BUFFER_SIZE];
} S_ND_USSD_STATUS, *P_ND_USSD_STATUS;


#endif
