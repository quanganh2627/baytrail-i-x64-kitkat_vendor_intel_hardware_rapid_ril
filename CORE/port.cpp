////////////////////////////////////////////////////////////////////////////
// port.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Implements a com port interface
//
// Author:  Mike Worth
// Created: 2009-20-11
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Nov 20/09  MW       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

// system include
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

// local includes
#include "types.h"
#include "rril.h"
#include "sync_ops.h"
//#include "thread_ops.h"
#include "util.h"
#include "rillog.h"
#include "port.h"
#include "rildmain.h"
#include "repository.h"

CPort::CPort() :
    m_fIsPortOpen(FALSE),
    m_pFile(NULL)
{
}

CPort::~CPort()
{
    if (m_fIsPortOpen)
    {
        if (!Close())
        {
            RIL_LOG_CRITICAL("CPort::~CPort() - ERROR: Failed to close port!\r\n");
        }
    }

    delete m_pFile;
    m_pFile = NULL;
}

int CPort::GetFD()
{
    int fd = -1;
    if (m_fIsPortOpen)
    {
        fd = CFile::GetFD(m_pFile);
    }
    return fd;
}

BOOL CPort::Open(const BYTE * pszFileName, BOOL fIsSocket)
{
    RIL_LOG_VERBOSE("CPort::Open() - Enter  fIsSocket=[%d]\r\n", fIsSocket);
    BOOL fRet = FALSE;

    if (NULL == pszFileName)
    {
        RIL_LOG_CRITICAL("CPort::Open() - ERROR: pszFileName is NULL!\r\n");
        return FALSE;
    }

    if (!m_fIsPortOpen)
    {
        if (NULL == m_pFile)
        {
            m_pFile = new CFile();
        }

        if (fIsSocket)
        {
            fRet = OpenSocket(pszFileName);
        }
        else
        {
            fRet = OpenPort(pszFileName);
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CPort::Open() - ERROR: Port is already open!\r\n");
    }

    RIL_LOG_VERBOSE("CPort::Open() - Exit\r\n");
    return fRet;
}

BOOL CPort::Init()
{
    RIL_LOG_VERBOSE("CPort::Init() - Enter\r\n");
    BOOL fRet = FALSE;

    int fd = 0;
    int bit = 0;
    struct termios oldtio, newtio;

    if (m_fIsPortOpen)
    {
        fd = CFile::GetFD(m_pFile);

        if (fd >= 0)
        {
            // save current port settings
            tcgetattr(fd,&oldtio);

            if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
                perror("fcntl()");

            bzero(&newtio, sizeof(newtio));
            newtio.c_cflag = B115200;

            newtio.c_cflag |= CS8 | CLOCAL | CREAD;
            newtio.c_iflag &= ~(INPCK | IGNPAR | PARMRK | ISTRIP | IXANY | ICRNL);
            newtio.c_oflag &= ~OPOST;
            newtio.c_cc[VMIN]  = 1;
            newtio.c_cc[VTIME] = 10;

            // set input mode (non-canonical, no echo,...)
            newtio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
            //newtio.c_lflag &= ~(ICANON | ISIG);

            //newtio.c_cc[VTIME]    = 1000;   /* inter-character timer unused */
            //newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */

            tcflush(fd, TCIFLUSH);
            tcsetattr(fd,TCSANOW,&newtio);

            bit = TIOCM_DTR;
            /*
            if(ioctl(fd, TIOCMBIS, &bit))
            {
                RIL_LOG_CRITICAL("CPort::Init() - ERROR: ioctl(%d, 0x%x, 0x%x) failed with error= %d\r\n", errno);
            }
            else
            {
                fRet = TRUE;
            }
            */
            fRet = TRUE;
        }
        else
        {
            RIL_LOG_CRITICAL("CPort::Init() - ERROR: File descriptor was negative!\r\n");
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CPort::Init() - ERROR: Port is not open!\r\n");
    }

    RIL_LOG_VERBOSE("CPort::Init() - Exit\r\n");
    return fRet;
}

BOOL CPort::Close()
{
    RIL_LOG_CRITICAL("CPort::Close() - Enter\r\n");
    BOOL fRet = FALSE;

    if (!m_fIsPortOpen)
    {
        RIL_LOG_CRITICAL("CPort::Close() - ERROR: Port is already closed!\r\n");
    }
    else
    {
        m_fIsPortOpen = FALSE;
        fRet = CFile::Close(m_pFile);

        if (!fRet)
        {
            RIL_LOG_CRITICAL("CPort::Close() - ERROR: Unable to properly close port!\r\n");
        }
    }

    RIL_LOG_CRITICAL("CPort::Close() - Exit\r\n");
    return fRet;
}

BOOL CPort::Read(BYTE * pszReadBuf, UINT32 uiReadBufSize, UINT32 & ruiBytesRead)
{
    RIL_LOG_VERBOSE("CPort::Read() - Enter\r\n");
    BOOL fRet = FALSE;

    ruiBytesRead = 0;

    if (m_fIsPortOpen)
    {
        if (CFile::Read(m_pFile, pszReadBuf, uiReadBufSize, ruiBytesRead))
        {
            fRet = TRUE;
        }
        else
        {
            RIL_LOG_CRITICAL("CPort::Read() - ERROR: Unable to read from port\r\n");
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CPort::Read() - ERROR: Port is not open!\r\n");
    }

    RIL_LOG_VERBOSE("CPort::Read() - Exit\r\n");
    return fRet;
}

BOOL CPort::Write(const BYTE * pszWriteBuf, const UINT32 uiBytesToWrite, UINT32 & ruiBytesWritten)
{
    RIL_LOG_VERBOSE("CPort::Write() - Enter\r\n");
    BOOL fRet = FALSE;

    ruiBytesWritten = 0;

    if (m_fIsPortOpen)
    {
        if (CFile::Write(m_pFile, pszWriteBuf, uiBytesToWrite, ruiBytesWritten))
        {
            fRet = TRUE;
        }
        else
        {
            RIL_LOG_CRITICAL("CPort::Write() - ERROR: Unable to write to port\r\n");
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CPort::Write() - ERROR: Port is not open!\r\n");
    }

    return fRet;
    RIL_LOG_VERBOSE("CPort::Write() - Exit\r\n");
}

BOOL CPort::OpenPort(const BYTE * pszFileName)
{
    RIL_LOG_VERBOSE("CPort::OpenPort() - Enter\r\n");
    BOOL fRet = FALSE;

    const int iRETRIES_DEFAULT = 30;    // default values
    const int iINTERVAL_DEFAULT = 1000; // default values

    int iRetries = iRETRIES_DEFAULT;
    int iInterval = iINTERVAL_DEFAULT;

    int  iAttempts = 0;

    //  Grab retries and interval from repository.
    CRepository repository;
    if (!repository.Read(g_szGroupRILSettings, g_szOpenPortRetries, iRetries))
    {
        iRetries = iRETRIES_DEFAULT;
    }
    RIL_LOG_INFO("CPort::OpenPort() - iRetries=[%d]\r\n", iRetries);

    if (!repository.Read(g_szGroupRILSettings, g_szOpenPortInterval, iInterval))
    {
        iInterval = iINTERVAL_DEFAULT;
    }
    RIL_LOG_INFO("CPort::OpenPort() - iInterval=[%d]\r\n", iInterval);

    while(!fRet)
    {
        for (iAttempts = 0; iAttempts < iRetries; iAttempts++)
//        for(;;)
        {
            if (iAttempts > 0)
            {
                Sleep(iInterval);
            }

            RIL_LOG_INFO("CPort::OpenPort()  ATTEMPT NUMBER %d\r\n", iAttempts);
            fRet = CFile::Open(m_pFile, pszFileName, FILE_ACCESS_READ_WRITE, FILE_OPEN_EXIST, FILE_OPT_NONE);

            if (fRet)
            {
                m_fIsPortOpen = TRUE;
                break;
            }
            // else
            // {
            //    /* maybe modem is absent, so dont wake up the system too
            //       often in that case.
            //       we do exponential retry with 20 minutes maximum */
            //    if (iAttempts > 9 && iInterval < 1200000)
            //        iInterval *= 2;
            //    Sleep(iInterval);
            //}

            //  Remove this when using for loop
            // iAttempts++;
        }


        //  If we didn't open the port, issue critical reset
        if (!fRet)
        {
            RIL_LOG_CRITICAL("CPort::OpenPort()  CANNOT OPEN PORT after %d attempts, issuing critical reboot\r\n", iAttempts);

#if defined(RESET_MGMT)
            //  TODO: Make sure this is correct.
            //  If we can't open the ports, tell STMD to cleanup and wait forever here.
            do_request_clean_up(eRadioError_OpenPortFailure, __LINE__, __FILE__, TRUE);

#else // RESET_MGMT

            TriggerRadioErrorAsync(eRadioError_OpenPortFailure, __LINE__, __FILE__);

            Sleep(2000);

            while (g_bIsTriggerRadioError)
            {
                RIL_LOG_INFO("CPort::OpenPort() - In g_bIsTriggerRadioError\r\n");
                Sleep(250);
            }

            RIL_LOG_CRITICAL("CPort::OpenPort() ****CALLING EXIT**********\r\n");
            exit(0);
#endif // RESET_MGMT
        }

    }

    RIL_LOG_VERBOSE("CPort::OpenPort() - Exit\r\n");
    return fRet;
}

BOOL CPort::OpenSocket(const BYTE * pszSocketName)
{
    RIL_LOG_VERBOSE("CPort::OpenSocket() - Enter\r\n");

    // TODO : Pull this from repository
    const BYTE szSocketInit[] = "gsm";

    UINT32 uiBytesWritten = 0;
    UINT32 uiBytesRead = 0;
    BYTE szResponse[10] = {0};

    BOOL fRet = FALSE;

    const UINT32 uiRetries = 30;
    const UINT32 uiInterval = 2000;

    for (UINT32 uiAttempts = 0; uiAttempts < uiRetries; uiAttempts++)
    {
        fRet = CFile::Open(m_pFile, pszSocketName, 0, 0, 0, TRUE);

        if (fRet)
        {
            m_fIsPortOpen = TRUE;
            RIL_LOG_INFO("CPort::OpenSocket() - Port is open!!\r\n");
            break;
        }

        Sleep(uiInterval);
    }

    if (fRet)
    {
        if (Write(szSocketInit, strlen(szSocketInit), uiBytesWritten))
        {
            if (Read(szResponse, sizeof(szResponse), uiBytesRead))
            {
                m_fIsPortOpen = TRUE;
            }
            else
            {
                RIL_LOG_CRITICAL("CPort::OpenSocket() - ERROR: Unable to read response from socket\r\n");
                fRet = FALSE;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CPort::OpenSocket() - ERROR: Unable to write \"%s\" to socket\r\n", szSocketInit);
            fRet = FALSE;
        }
    }

    RIL_LOG_VERBOSE("CPort::OpenSocket() - Exit\r\n");
    return fRet;
}

BOOL CPort::WaitForAvailableData(UINT32 uiTimeout)
{
    RIL_LOG_VERBOSE("CPort::WaitForAvailableData() - Enter\r\n");
    BOOL fRet = FALSE;
    UINT32 uiMask = 0;

    if (m_fIsPortOpen)
    {
        fRet = CFile::WaitForEvent(m_pFile, uiMask, uiTimeout);

        if(fRet)
        {
            if (uiMask & FILE_EVENT_ERROR)
            {
                RIL_LOG_CRITICAL("CPort::WaitForAvailableData() - ERROR: FILE_EVENT_ERROR received on port\r\n");
            }

            if (uiMask & FILE_EVENT_BREAK)
            {
                RIL_LOG_INFO("CPort::WaitForAvailableData() - INFO: FILE_EVENT_BREAK received on port\r\n");
            }

            if (uiMask & FILE_EVENT_RXCHAR)
            {
                fRet = TRUE;
            }
        }
        else
        {
            RIL_LOG_CRITICAL("CPort::WaitForAvailableData() - ERROR: Returning failure\r\n");
        }
    }
    else
    {
        RIL_LOG_CRITICAL("CPort::WaitForAvailableData() - ERROR: Port is not open!\r\n");
    }

Error:
    RIL_LOG_VERBOSE("CPort::WaitForAvailableData() - Exit\r\n");
    return fRet;
}
