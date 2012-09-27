////////////////////////////////////////////////////////////////////////////
// file_ops.cpp
//
// Copyright 2009-2011 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Handles opening and closing ports. Also handles reading and writing to ports.
//
/////////////////////////////////////////////////////////////////////////////


#include "types.h"
#include "rillog.h"
#include "file_ops.h"
#include "util.h"
#include <fcntl.h>
#include <cutils/sockets.h>
#include <termios.h>
#include <sys/stat.h>
#include <time.h>

#include <poll.h>

// Use to convert our timeout to a timeval in the future
timeval msFromNowToTimeval(UINT32 msInFuture)
{
    timeval FutureTime;
    timeval Now;
    UINT32 usFromNow;

    gettimeofday(&Now, NULL);

    usFromNow = ((msInFuture %  1000) * 1000) + Now.tv_usec;

    FutureTime.tv_sec = Now.tv_sec + (msInFuture / 1000);

    if (usFromNow >= 1000000)
    {
        FutureTime.tv_sec++;
        FutureTime.tv_usec = (usFromNow - 1000000);
    }
    else
    {
        FutureTime.tv_usec = usFromNow;
    }

    return FutureTime;
}

CFile::CFile() :
    m_file(-1),
    m_fInitialized(FALSE) {};

CFile::~CFile()
{
    if (m_fInitialized)
    {
        if (0 > close(m_file))
        {
            RIL_LOG_CRITICAL("CFile::~CFile() : Error when closing file\r\n");
        }

        m_fInitialized = FALSE;
    }
}

BOOL CFile::OpenSocket(const char * pszSocketName)
{
    if (NULL == pszSocketName)
    {
        RIL_LOG_CRITICAL("CFile::OpenSocket() : pszSocketName is NULL\r\n");
        goto Error;
    }

    m_file = socket_local_client(pszSocketName, ANDROID_SOCKET_NAMESPACE_FILESYSTEM, SOCK_STREAM);

    if (m_file < 0)
    {
        RIL_LOG_CRITICAL("CFile::OpenSocket() : Could not open socket\r\n");
        goto Error;
    }

    m_fInitialized = TRUE;

Error:
   return m_fInitialized;
}

BOOL CFile::Open(   const char * pszFileName,
                    UINT32 dwAccessFlags,
                    UINT32 dwOpenFlags,
                    UINT32 dwOptFlags,
                    BOOL fIsSocket)
{
    RIL_LOG_INFO("CFile::Open() : pszFileName=[%s] fIsSocket=[%d]",
            (NULL == pszFileName ? "NULL" : pszFileName), fIsSocket);

    if (NULL == pszFileName)
    {
        RIL_LOG_CRITICAL("CFile::Open() : pszFilename is NULL!\r\n");
        return FALSE;
    }

    UINT32 dwAttr = 0;
    BOOL fExists = FALSE;
    BOOL fFile = FALSE;
    int iAttr = 0;

    if (m_fInitialized)
    {
        RIL_LOG_CRITICAL("CFile::Open() : File already opened.\r\n");
        goto Error;
    }

    if (fIsSocket)
    {
        OpenSocket(pszFileName);
    }
    else
    {
        dwAttr = GetFileAttributes(pszFileName);

        fExists = (dwAttr == FILE_ATTRIB_DOESNT_EXIST) ? FALSE : TRUE;
        fFile   = (dwAttr == FILE_ATTRIB_DOESNT_EXIST) ? FALSE : ((dwAttr & FILE_ATTRIB_REG) ? TRUE : FALSE);
        RIL_LOG_INFO("CFile::Open() : fExists=[%d]  fFile=[%d]\r\n", fExists, fFile);

        switch (dwAccessFlags)
        {
            case FILE_ACCESS_READ_ONLY:
            {
                iAttr = O_RDONLY;
                break;
            }

            case FILE_ACCESS_WRITE_ONLY:
            {
                iAttr = O_WRONLY;
                break;
            }

            case FILE_ACCESS_READ_WRITE:
            {
                //iAttr = O_RDWR;
                iAttr = O_NONBLOCK | O_RDWR;
                break;
            }

            default:
            {
                RIL_LOG_CRITICAL("CFile::Open() : Invalid access flags: 0x%X\r\n", dwAccessFlags);
                goto Error;
            }
        }

        // Only open if file already exists
        if (FILE_OPEN_EXIST & dwOpenFlags)
        {
            if (!fExists)
            {
                RIL_LOG_CRITICAL("CFile::Open() : File does not exist.\r\n");
                goto Error;
            }
        }

        if (FILE_OPEN_NON_BLOCK & dwOpenFlags)
        {
            iAttr |= O_NONBLOCK;
        }

        if (FILE_OPEN_APPEND & dwOpenFlags)
        {
            iAttr |= O_APPEND;
        }

        if (FILE_OPEN_CREATE & dwOpenFlags)
        {
            iAttr |= O_CREAT;
        }

        if (FILE_OPEN_TRUNCATE & dwOpenFlags)
        {
            iAttr |= O_TRUNC;
        }

        if (FILE_OPEN_EXCL & dwOpenFlags)
        {
            iAttr |= O_EXCL;
        }

        m_file = open(pszFileName, iAttr | O_NONBLOCK);

        if (m_file < 0)
        {
            RIL_LOG_CRITICAL("CFile::Open() : Open failed, m_file=[%d], errno=[%d],[%s]\r\n",
                    m_file, errno, strerror(errno));
            goto Error;
        }
        else
        {
            RIL_LOG_INFO("CFile::Open() : m_file=[%d]\r\n", m_file);
        }

        {
            termios terminalParameters;
            int err = tcgetattr(m_file, &terminalParameters);
            if (-1 != err)
            {
                cfmakeraw(&terminalParameters);
                tcsetattr(m_file, TCSANOW, &terminalParameters);
            }
        }

        m_fInitialized = TRUE;
    }

Error:
    RIL_LOG_VERBOSE("CFile::Open() - Exit m_fInitialized=[%d]\r\n", m_fInitialized);
    return m_fInitialized;
}

BOOL CFile::Close()
{
    if (!m_fInitialized)
    {
        RIL_LOG_CRITICAL("CFile::Close() : File is not open! Unable to close\r\n");
        return FALSE;
    }

    RIL_LOG_INFO("CFile::Close() - Closing %d\r\n", m_file);
    if (0 > close(m_file))
    {
        RIL_LOG_CRITICAL("CFile::Close() : Error when closing file\r\n");
        return FALSE;
    }

    m_file = -1;
    m_fInitialized = FALSE;
    return TRUE;
}

BOOL CFile::Write(const void * pBuffer, UINT32 dwBytesToWrite, UINT32 &rdwBytesWritten)
{
    int bytesWritten = 0;
    int writeAttempt = 0;
    const int MAX_WRITE_ATTEMPT = 5;
    const int TIME_BEFORE_RETRY_IN_MS = 100;
    rdwBytesWritten = 0;

    if (!m_fInitialized)
    {
        RIL_LOG_CRITICAL("CFile::Write() : File is not open!\r\n");
        return FALSE;
    }

    while ((bytesWritten = (write(m_file, pBuffer, dwBytesToWrite))) == -1)
    {
        switch (errno)
        {
            case EAGAIN:
            {
                // Channel is still in opening state, wait and retry the write
                writeAttempt++;
                if (writeAttempt > MAX_WRITE_ATTEMPT)
                {
                    RIL_LOG_CRITICAL("CFile::Write() : Write failed - Channel still not open\r\n");
                    return FALSE;
                }
                Sleep(TIME_BEFORE_RETRY_IN_MS);
            }
            break;

            case ENXIO:
            {
                // Channel was closed internally by the MUX driver (modem self reset) when the RRIL
                // perform the write. In this case, we return TRUE and let the RRIL be informed of
                // the modem self reset by the standard way (means by STMD).
                RIL_LOG_CRITICAL("CFile::Write() : Write failed - Channel close by MUX\r\n");
                return TRUE;
            }
            break;

            default:
                RIL_LOG_CRITICAL("CFile::Write() : Error during write process!  errno=[%d] [%s]\r\n", errno, strerror(errno));
                return FALSE;
        }
    }

    rdwBytesWritten = (UINT32)bytesWritten;

    return TRUE;
}

BOOL CFile::Read(void * pBuffer, UINT32 dwBytesToRead, UINT32 &rdwBytesRead)
{
    int iCount = 0;
    rdwBytesRead = 0;

    if (!m_fInitialized)
    {
        RIL_LOG_CRITICAL("CFile::Read() : File is not open!\r\n");
        return FALSE;
    }

    if ((iCount = read(m_file, pBuffer, dwBytesToRead)) == -1)
    {
        if (errno != EAGAIN)
        {
            RIL_LOG_CRITICAL("CFile::Read() : Error during read process!  errno=[%d] [%s]\r\n", errno, strerror(errno));
            return FALSE;
        }

        rdwBytesRead = 0;
        return TRUE;
    }

    rdwBytesRead = (UINT32)iCount;

    return TRUE;
}

UINT32 CFile::GetFileAttributes(const char * pszFileName)
{
    struct stat statbuf;
    UINT32 dwReturn;

    if (stat(pszFileName, &statbuf))
        return FILE_ATTRIB_DOESNT_EXIST;

    dwReturn = 0;

    if (S_ISDIR(statbuf.st_mode))
    {
        dwReturn |= FILE_ATTRIB_DIR;
    }

    if (S_ISREG(statbuf.st_mode))
    {
        dwReturn |= FILE_ATTRIB_REG;
    }

    if (S_ISSOCK(statbuf.st_mode))
    {
        dwReturn |= FILE_ATTRIB_SOCK;
    }

    if ((statbuf.st_mode & S_IWUSR) == 0)
    {
        dwReturn |= FILE_ATTRIB_RO;
    }

    if (S_ISBLK(statbuf.st_mode))
    {
        dwReturn |= FILE_ATTRIB_BLK;
    }

    if (S_ISCHR(statbuf.st_mode))
    {
        dwReturn |= FILE_ATTRIB_CHR;
    }

    if (!dwReturn)
    {
        dwReturn = FILE_ATTRIB_REG;
    }

    return dwReturn;
}

BOOL CFile::WaitForEvent(UINT32 &rdwFlags, UINT32 dwTimeoutInMS)
{
    struct pollfd fds[1] = { {0,0,0} };
    int nPollVal = 0;

    rdwFlags = 0;

    if (m_file < 0)
    {
        RIL_LOG_CRITICAL("CFile::WaitForEvent() : m_file is not valid");
        return FALSE;
    }

    fds[0].fd = m_file;
    fds[0].events = POLLIN;

    if (WAIT_FOREVER == dwTimeoutInMS)
    {
        nPollVal = poll(fds, 1, -1);
    }
    else
    {
        RIL_LOG_INFO("CFile::WaitForEvent() : calling poll() on fd=[%d]  timeout=[%d]ms\r\n", m_file, dwTimeoutInMS);

        nPollVal = poll(fds, 1, dwTimeoutInMS);
    }

    switch(nPollVal)
    {
        case -1:
            //  Error
            RIL_LOG_CRITICAL("CFile::WaitForEvent() : polling error [%d] [%s]\r\n", errno, strerror(errno));
            return FALSE;

        case 0:
            //  timed out
            // ignore
            break;

        default:
            //  Got an event on the fd
            if (fds[0].revents & POLLIN)
            {
                //  We received valid data
                rdwFlags = FILE_EVENT_RXCHAR;
            }
            else if (fds[0].revents & POLLHUP)
            {
                RIL_LOG_CRITICAL("CFile::WaitForEvent() : **** RECEIVED POLLHUP on fd=[%d]  ignoring...\r\n", m_file);
                //  ignore, should clean-up when uiRead < 0
            }
            else if (fds[0].revents & POLLNVAL)
            {
                RIL_LOG_CRITICAL("CFile::WaitForEvent() : **** RECEIVED POLLNVAL on fd=[%d]\r\n", m_file);

                //  possible that port has been closed
                return FALSE;
            }
            else
            {
                //  not sure if we will ever get here
                RIL_LOG_CRITICAL("CFile::WaitForEvent() : unexpected event=[%08x]\r\n", fds[0].revents);
                //  ignore
            }
            break;
    }

    return TRUE;
}


BOOL CFile::Open(CFile * pFile, const char * pszFileName, UINT32 dwAccessFlags, UINT32 dwOpenFlags, UINT32 dwOptFlags, BOOL fIsSocket)
{
    if (pFile)
    {
        return pFile->Open(pszFileName, dwAccessFlags, dwOpenFlags, dwOptFlags, fIsSocket);
    }
    else
    {
        RIL_LOG_CRITICAL("CFile::Open() : pFile was NULL!\r\n");
        return FALSE;
    }
}

BOOL CFile::Close(CFile * pFile)
{
    if (pFile)
    {
        return pFile->Close();
    }
    else
    {
        RIL_LOG_CRITICAL("CFile::Close() : pFile was NULL!\r\n");
        return FALSE;
    }
}

BOOL CFile::Read(CFile * pFile, void * pBuffer, UINT32 dwBytesToRead, UINT32 &rdwBytesRead)
{
    if (pFile)
    {
        return pFile->Read(pBuffer, dwBytesToRead, rdwBytesRead);
    }
    else
    {
        RIL_LOG_CRITICAL("CFile::Read() : pFile was NULL!\r\n");
        return FALSE;
    }
}

BOOL CFile::Write(CFile * pFile, const void * pBuffer, UINT32 dwBytesToWrite, UINT32 &rdwBytesWritten)
{
    if (pFile)
    {
        return pFile->Write(pBuffer, dwBytesToWrite, rdwBytesWritten);
    }
    else
    {
        RIL_LOG_CRITICAL("CFile::Write() : pFile was NULL!\r\n");
        return FALSE;
    }
}

BOOL CFile::WaitForEvent(CFile * pFile, UINT32 &rdwFlags, UINT32 dwTimeoutInMS)
{
    if (pFile)
    {
        return pFile->WaitForEvent(rdwFlags, dwTimeoutInMS);
    }
    else
    {
        RIL_LOG_CRITICAL("CFile::WaitForEvent() : pFile was NULL!\r\n");
        return FALSE;
    }
}

int CFile::GetFD(CFile * pFile)
{
    if (pFile)
    {
        return pFile->m_file;
    }
    else
    {
        RIL_LOG_CRITICAL("CFile::GetFD() : pFile was NULL!\r\n");
        return -1;
    }
}

