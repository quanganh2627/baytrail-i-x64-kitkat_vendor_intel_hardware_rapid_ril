////////////////////////////////////////////////////////////////////////////
// port.h
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

#ifndef RRIL_PORT_H
#define RRIL_PORT_H

#include "file_ops.h"
#include "rril.h"

class CPort
{
public:
    CPort();
    ~CPort();
    
    BOOL Open(const BYTE * pszFileName, BOOL fIsSocket);
    BOOL Init();

    BOOL Close();

    BOOL Read(BYTE * pszReadBuf, UINT32 uiReadBufSize, UINT32 & ruiBytesRead);
    BOOL Write(const BYTE * pszWriteBuf, const UINT32 uiBytesToWrite, UINT32 & ruiBytesWritten);

    BOOL WaitForAvailableData(UINT32 uiTimeout);

    BOOL IsOpen()   {   return m_fIsPortOpen;   };

private:
    BOOL OpenPort(const BYTE * pszFileName);
    BOOL OpenSocket(const BYTE * pszSocketName);
    
    BOOL        m_fIsPortOpen;
    CFile *     m_pFile;
};

#endif
