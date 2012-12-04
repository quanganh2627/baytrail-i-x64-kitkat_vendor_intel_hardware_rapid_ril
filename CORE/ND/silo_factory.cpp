////////////////////////////////////////////////////////////////////////////
// silo_factory.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Creates the modem specific Silos
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "silo_voice_inf.h"
#include "silo_sim_inf.h"
#include "silo_sms_inf.h"
#include "silo_data_inf.h"
#include "silo_network_inf.h"
#include "silo_phonebook_inf.h"
#include "silo_misc_inf.h"
#include "rillog.h"
#include "repository.h"
#include "silo_factory.h"
#include "te.h"

CSilo* CSilo_Factory::GetSiloVoice(CChannel *pChannel)
{
    RIL_LOG_VERBOSE("CSilo_Factory::GetSiloVoice() - Enter\r\n");
    CSilo* pSilo = NULL;

    CRepository repository;
    char szModem[MAX_MODEM_NAME_LEN];

    if (repository.Read(g_szGroupModem, g_szSupportedModem, szModem, MAX_MODEM_NAME_LEN))
    {
        if (0 == strcmp(szModem, szXMM6260)
                || 0 == strcmp(szModem, szXMM6360)
                || 0 == strcmp(szModem, szXMM7160))
        {
            pSilo = new CSilo_Voice_INF(pChannel);
        }
    }

    if (NULL == pSilo)
    {
        // return default silo voice
        pSilo = new CSilo_Voice(pChannel);
    }

    RIL_LOG_VERBOSE("CSilo_Factory::GetSiloVoice() - Exit\r\n");
    return pSilo;
}

CSilo* CSilo_Factory::GetSiloSIM(CChannel *pChannel)
{
    RIL_LOG_VERBOSE("CSilo_Factory::GetSiloSIM() - Enter\r\n");
    CSilo* pSilo = NULL;

    CRepository repository;
    char szModem[MAX_MODEM_NAME_LEN];

    if (repository.Read(g_szGroupModem, g_szSupportedModem, szModem, MAX_MODEM_NAME_LEN))
    {
        if (0 == strcmp(szModem, szXMM6260)
                || 0 == strcmp(szModem, szXMM6360)
                || 0 == strcmp(szModem, szXMM7160))
        {
            pSilo = new CSilo_SIM_INF(pChannel);
        }
    }

    if (NULL == pSilo)
    {
        // return default silo SIM
        pSilo = new CSilo_SIM(pChannel);
    }

    RIL_LOG_VERBOSE("CSilo_Factory::GetSiloSIM() - Exit\r\n");
    return pSilo;
}

CSilo* CSilo_Factory::GetSiloSMS(CChannel *pChannel)
{
    RIL_LOG_VERBOSE("CSilo_Factory::GetSiloSMS() - Enter\r\n");
    CSilo* pSilo = NULL;

    CRepository repository;
    char szModem[MAX_MODEM_NAME_LEN];

    if (repository.Read(g_szGroupModem, g_szSupportedModem, szModem, MAX_MODEM_NAME_LEN))
    {
        if (0 == strcmp(szModem, szXMM6260)
                || 0 == strcmp(szModem, szXMM6360)
                || 0 == strcmp(szModem, szXMM7160))
        {
            pSilo = new CSilo_SMS_INF(pChannel);
        }
    }

    if (NULL == pSilo)
    {
        // return default silo SMS
        pSilo = new CSilo_SMS(pChannel);
    }

    RIL_LOG_VERBOSE("CSilo_Factory::GetSiloSMS() - Exit\r\n");
    return pSilo;
}

CSilo* CSilo_Factory::GetSiloData(CChannel *pChannel)
{
    RIL_LOG_VERBOSE("CSilo_Factory::GetSiloData() - Enter\r\n");
    CSilo* pSilo = NULL;

    CRepository repository;
    char szModem[MAX_MODEM_NAME_LEN];

    if (repository.Read(g_szGroupModem, g_szSupportedModem, szModem, MAX_MODEM_NAME_LEN))
    {
        if (0 == strcmp(szModem, szXMM6260)
                || 0 == strcmp(szModem, szXMM6360)
                || 0 == strcmp(szModem, szXMM7160))
        {
            pSilo = new CSilo_Data_INF(pChannel);
        }
    }

    if (NULL == pSilo)
    {
        // return default silo Data
        pSilo = new CSilo_Data(pChannel);
    }

    RIL_LOG_VERBOSE("CSilo_Factory::GetSiloData() - Exit\r\n");
    return pSilo;
}

CSilo* CSilo_Factory::GetSiloNetwork(CChannel *pChannel)
{
    RIL_LOG_VERBOSE("CSilo_Factory::GetSiloNetwork() - Enter\r\n");
    CSilo* pSilo = NULL;

    CRepository repository;
    char szModem[MAX_MODEM_NAME_LEN];

    if (repository.Read(g_szGroupModem, g_szSupportedModem, szModem, MAX_MODEM_NAME_LEN))
    {
        if (0 == strcmp(szModem, szXMM6260)
                || 0 == strcmp(szModem, szXMM6360)
                || 0 == strcmp(szModem, szXMM7160))
        {
            pSilo = new CSilo_Network_INF(pChannel);
        }
    }

    if (NULL == pSilo)
    {
        // return default silo network
        pSilo = new CSilo_Network(pChannel);
    }

    RIL_LOG_VERBOSE("CSilo_Factory::GetSiloNetwork() - Exit\r\n");
    return pSilo;
}

CSilo* CSilo_Factory::GetSiloPhonebook(CChannel *pChannel)
{
    RIL_LOG_VERBOSE("CSilo_Factory::GetSiloPhonebook() - Enter\r\n");
    CSilo* pSilo = NULL;

    CRepository repository;
    char szModem[MAX_MODEM_NAME_LEN];

    if (repository.Read(g_szGroupModem, g_szSupportedModem, szModem, MAX_MODEM_NAME_LEN))
    {
        if (0 == strcmp(szModem, szXMM6260)
                || 0 == strcmp(szModem, szXMM6360)
                || 0 == strcmp(szModem, szXMM7160))
        {
            pSilo = new CSilo_Phonebook_INF(pChannel);
        }
    }

    if (NULL == pSilo)
    {
        // return default silo phonebook
        pSilo = new CSilo_Phonebook(pChannel);
    }

    RIL_LOG_VERBOSE("CSilo_Factory::GetSiloPhonebook() - Exit\r\n");
    return pSilo;
}

CSilo* CSilo_Factory::GetSiloMISC(CChannel *pChannel)
{
    RIL_LOG_VERBOSE("CSilo_Factory::GetSiloMISC() - Enter\r\n");
    CSilo* pSilo = NULL;

    CRepository repository;
    char szModem[MAX_MODEM_NAME_LEN];

    if (repository.Read(g_szGroupModem, g_szSupportedModem, szModem, MAX_MODEM_NAME_LEN))
    {
        if (0 == strcmp(szModem, szXMM6260)
                || 0 == strcmp(szModem, szXMM6360)
                || 0 == strcmp(szModem, szXMM7160))
        {
            pSilo = new CSilo_MISC_INF(pChannel);
        }
    }

    if (NULL == pSilo)
    {
        // return default silo misc
        pSilo = new CSilo_MISC(pChannel);
    }

    RIL_LOG_VERBOSE("CSilo_Factory::GetSiloMISC() - Exit\r\n");
    return pSilo;
}

