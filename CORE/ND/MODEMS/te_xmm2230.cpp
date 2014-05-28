////////////////////////////////////////////////////////////////////////////
// te_xmm2230.cpp
//
// Copyright (C) Intel 2014.
//
//
// Description:
//    Overlay for the IMC 2230 modem
//
/////////////////////////////////////////////////////////////////////////////

#include <wchar.h>
#include <cutils/properties.h>

//  This is for socket-related calls.
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/gsmmux.h>

#include "types.h"
#include "nd_structs.h"
#include "util.h"
#include "extract.h"
#include "rillog.h"
#include "te.h"
#include "sync_ops.h"
#include "command.h"
#include "te_xmm2230.h"
#include "rildmain.h"
#include "callbacks.h"
#include "oemhookids.h"
#include "repository.h"
#include "reset.h"
#include "data_util.h"
#include "init2230.h"


CTE_XMM2230::CTE_XMM2230(CTE& cte)
: CTE_XMM6260(cte)
{
}

CTE_XMM2230::~CTE_XMM2230()
{
}

CInitializer* CTE_XMM2230::GetInitializer()
{
    RIL_LOG_VERBOSE("CTE_XMM2230::GetInitializer() - Enter\r\n");
    CInitializer* pRet = NULL;

    RIL_LOG_INFO("CTE_XMM2230::GetInitializer() - Creating CInit2230 initializer\r\n");
    m_pInitializer = new CInit2230();
    if (NULL == m_pInitializer)
    {
        RIL_LOG_CRITICAL("CTE_XMM2230::GetInitializer() - Failed to create a CInit2230 "
                "initializer!\r\n");
        goto Error;
    }

    pRet = m_pInitializer;

Error:
    RIL_LOG_VERBOSE("CTE_XMM2230::GetInitializer() - Exit\r\n");
    return pRet;
}

char* CTE_XMM2230::GetUnlockInitCommands(UINT32 uiChannelType)
{
    RIL_LOG_VERBOSE("CTE_XMM2230::GetUnlockInitCommands() - Enter\r\n");

    char szInitCmd[MAX_BUFFER_SIZE] = {'\0'};

    if (RIL_CHANNEL_URC == uiChannelType)
    {
        ConcatenateStringNullTerminate(szInitCmd, sizeof(szInitCmd), "|+CGAUTO=0|+CRC=1");
    }

    RIL_LOG_VERBOSE("CTE_XMM2230::GetUnlockInitCommands() - Exit\r\n");
    return strndup(szInitCmd, strlen(szInitCmd));
}

const char* CTE_XMM2230::GetRegistrationInitString()
{
    return "+CREG=2";
}

const char* CTE_XMM2230::GetCsRegistrationReadString()
{
    return "AT+CREG=2;+CREG?;+CREG=0\r";
}

const char* CTE_XMM2230::GetScreenOnString()
{
    return m_cte.IsSignalStrengthReportEnabled()
            ? "AT+CREG=2;+XCSQ=1\r" : "AT+CREG=2\r";
}

//
// RIL_REQUEST_SIM_TRANSMIT_BASIC 114
//
RIL_RESULT_CODE CTE_XMM2230::CoreSimTransmitBasic(REQUEST_DATA& rReqData,
                                                         void* pData,
                                                         UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("XMM2230::CoreSimTransmitBasic() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("XMM2230::CoreSimTransmitBasic() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SIM_OPEN_CHANNEL 115
//
RIL_RESULT_CODE CTE_XMM2230::CoreSimOpenChannel(REQUEST_DATA& rReqData, void* pData, UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM2230::CoreSimOpenChannel() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("CTE_XMM2230::CoreSimOpenChannel() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SIM_CLOSE_CHANNEL 116
//
RIL_RESULT_CODE CTE_XMM2230::CoreSimCloseChannel(REQUEST_DATA& rReqData,
                                                        void* pData,
                                                        UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM2230::CoreSimCloseChannel() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("CTE_XMM2230::CoreSimCloseChannel() - Exit\r\n");
    return res;
}

//
// RIL_REQUEST_SIM_TRANSMIT_CHANNEL 117
//
RIL_RESULT_CODE CTE_XMM2230::CoreSimTransmitChannel(REQUEST_DATA& rReqData,
                                                           void* pData,
                                                           UINT32 uiDataSize)
{
    RIL_LOG_VERBOSE("CTE_XMM2230::CoreSimTransmitChannel() - Enter\r\n");
    RIL_RESULT_CODE res = RRIL_RESULT_NOTSUPPORTED;

    RIL_LOG_VERBOSE("CTE_XMM2230::CoreSimTransmitChannel() - Exit\r\n");
    return res;
}
