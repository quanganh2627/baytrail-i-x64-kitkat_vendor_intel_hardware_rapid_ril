////////////////////////////////////////////////////////////////////////////
// te_xmm7260.cpp
//
// Copyright (C) Intel 2013.
//
//
// Description:
//    Overlay for the IMC 7260 modem
//
/////////////////////////////////////////////////////////////////////////////

#include "rillog.h"
#include "te.h"
#include "te_xmm7260.h"
#include "init7260.h"

CTE_XMM7260::CTE_XMM7260(CTE& cte)
: CTE_XMM7160(cte)
{
    m_cte.SetDefaultPDNCid(DEFAULT_PDN_CID);
}

CTE_XMM7260::~CTE_XMM7260()
{
}

CInitializer* CTE_XMM7260::GetInitializer()
{
    RIL_LOG_VERBOSE("CTE_XMM7260::GetInitializer() - Enter\r\n");
    CInitializer* pRet = NULL;

    RIL_LOG_INFO("CTE_XMM7260::GetInitializer() - Creating CInit7260 initializer\r\n");
    m_pInitializer = new CInit7260();
    if (NULL == m_pInitializer)
    {
        RIL_LOG_CRITICAL("CTE_XMM7260::GetInitializer() - Failed to create a CInit7260 "
                "initializer!\r\n");
        goto Error;
    }

    pRet = m_pInitializer;

Error:
    RIL_LOG_VERBOSE("CTE_XMM7260::GetInitializer() - Exit\r\n");
    return pRet;
}
