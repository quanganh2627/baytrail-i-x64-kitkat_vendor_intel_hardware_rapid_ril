////////////////////////////////////////////////////////////////////////////
// te_xmm7160.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Overlay for the IMC 7160 modem
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_TE_XMM7160_H
#define RRIL_TE_XMM7160_H

#include "te_xmm6360.h"
#include "rril.h"
#include "channel_data.h"

class CEvent;
class CInitializer;

class CTE_XMM7160 : public CTE_XMM6360
{
public:

    enum { DEFAULT_PDN_CID = 1 };

    CTE_XMM7160(CTE& cte);
    virtual ~CTE_XMM7160();

private:

    CTE_XMM7160();

    //  Prevent assignment: Declared but not implemented.
    CTE_XMM7160(const CTE_XMM7160& rhs);  // Copy Constructor
    CTE_XMM7160& operator=(const CTE_XMM7160& rhs);  //  Assignment operator

    RIL_RESULT_CODE CreateIMSRegistrationReq(REQUEST_DATA& rReqData,
                                            const char** pszRequest,
                                            const UINT32 uiDataSize);
    RIL_RESULT_CODE CreateIMSConfigReq(REQUEST_DATA& rReqData,
                                       const char** pszRequest,
                                       const int nNumStrings);

    RIL_RESULT_CODE CreateSetDefaultApnReq(REQUEST_DATA& rReqData,
            const char** pszRequest, const int nNumStrings);

    // RIL_REQUEST_SET_BAND_MODE 65
    virtual RIL_RESULT_CODE CoreSetBandMode(REQUEST_DATA& rReqData,
                                                       void* pData,
                                                       UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseSetBandMode(RESPONSE_DATA& rRspData);

public:
    // modem overrides

    virtual CInitializer* GetInitializer();

    // RIL_REQUEST_DATA_REGISTRATION_STATE 21
    virtual RIL_RESULT_CODE CoreGPRSRegistrationState(REQUEST_DATA& rReqData,
             void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGPRSRegistrationState(RESPONSE_DATA& rRspData);

    // RIL_REQUEST_DEACTIVATE_DATA_CALL 41
    virtual RIL_RESULT_CODE CoreDeactivateDataCall(REQUEST_DATA& rReqData,
                                                                void* pData,
                                                                UINT32 uiDataSize);

    // RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE 73
    virtual RIL_RESULT_CODE CoreSetPreferredNetworkType(REQUEST_DATA& rReqData,
            void* pData, UINT32 uiDataSize);

    // RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE 74
    virtual RIL_RESULT_CODE CoreGetPreferredNetworkType(REQUEST_DATA& rReqData,
            void* pData, UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseGetPreferredNetworkType(RESPONSE_DATA& rRspData);

    virtual BOOL IMSRegister(REQUEST_DATA& rReqData, void* pData,
                                                    UINT32 uiDataSize);
    virtual RIL_RESULT_CODE ParseIMSRegister(RESPONSE_DATA & rRspData);

    /*
     * AT commands to configure the channel for default PDN and also to put the
     * channel in data mode is added to the command queue.
     */
    virtual RIL_RESULT_CODE HandleSetupDefaultPDN(RIL_Token rilToken,
            CChannel_Data* pChannelData);

    // Parser function for Setup default PDN.
    virtual RIL_RESULT_CODE ParseSetupDefaultPDN(RESPONSE_DATA& rRspData);

    /*
     * Post Command handler function for the setting up of default PDN.
     *
     * Upon success, data state of default PDN will be set to E_DATA_STATE_ACTIVE
     * and interface is setup and the RIL_REEQUEST_SETUP_DATA_CALL request will
     * be completed.
     *
     * Upon failure, RIL_REEQUEST_SETUP_DATA_CALL request will be completed.
     */
    virtual void PostSetupDefaultPDN(POST_CMD_HANDLER_DATA& rData);

    virtual BOOL DataConfigDown(UINT32 uiCID, BOOL bForceCleanup = FALSE);

protected:

    virtual const char* GetRegistrationInitString();
    virtual const char* GetPsRegistrationReadString();

    virtual const char* GetScreenOnString();
    virtual const char* GetScreenOffString();
};

#endif
