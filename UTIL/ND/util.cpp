////////////////////////////////////////////////////////////////////////////
// util.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides implementations for the RIL Utility functions.
//    Also includes related class implementations.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "../../CORE/util.h"
#include "extract.h"

#include <wchar.h>
#include <sys/select.h>
#include <arpa/inet.h>

#ifdef assert
#undef assert
#endif


#define minimum_of(a,b) (((a) < (b)) ? (a) : (b))



//
// Table used to map semi-byte values to hex characters
//
static const char g_rgchSemiByteToCharMap[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};



//
// Convert a semi-byte into its character representation
//
char SemiByteToChar(const BYTE bByte, const BOOL fHigh)
{
    UINT8 bSemiByte = (fHigh ? (bByte & 0xf0) >> 4 : bByte & 0x0f);

    if (0x10 <= bSemiByte)
    {
        RIL_LOG_CRITICAL("SemiByteToChar() : Invalid bByte argument: bByte: 0x%X fHigh: %s\r\n", bByte, fHigh ? "TRUE" : "FALSE");
        return g_rgchSemiByteToCharMap[0];
    }
    else
    {
        return g_rgchSemiByteToCharMap[bSemiByte];
    }
}


//
// Combine 2 characters representing semi-bytes into a byte
//
BYTE SemiByteCharsToByte(const char chHigh, const char chLow)
{
    BYTE bRet;

    if ('0' <= chHigh && '9' >= chHigh)
    {
        bRet = (chHigh - '0') << 4;
    }
    else
    {
        bRet = (0x0a + chHigh - 'A') << 4;
    }

    if ('0' <= chLow && '9' >= chLow)
    {
        bRet |= (chLow - '0');
    }
    else
    {
        bRet |= (0x0a + chLow - 'A');
    }
    return bRet;
}


//
//
//
BOOL GSMHexToGSM(const char* sIn, const UINT32 cbIn, BYTE* sOut, const UINT32 cbOut, UINT32& rcbUsed)
{
    const char* pchIn = sIn;
    const char* pchInEnd = sIn + cbIn;
    BYTE* pchOut = sOut;
    BYTE* pchOutEnd = sOut + cbOut;
    BOOL fRet = FALSE;

    while (pchIn < pchInEnd - 1 && pchOut < pchOutEnd)
    {
        *pchOut++ = SemiByteCharsToByte(*pchIn, *(pchIn + 1));
        pchIn += 2;
    }

    rcbUsed = pchOut - sOut;
    fRet = TRUE;
    return fRet;
}


//
//
//
BOOL GSMToGSMHex(const BYTE* sIn, const UINT32 cbIn, char* sOut, const UINT32 cbOut, UINT32& rcbUsed)
{
    const BYTE* pchIn = sIn;
    const BYTE* pchInEnd = sIn + cbIn;
    char* pchOut = sOut;
    char* pchOutEnd = sOut + cbOut;
    BOOL fRet = FALSE;

    while (pchIn < pchInEnd && pchOut < pchOutEnd - 1)
    {
        *pchOut = g_rgchSemiByteToCharMap[((*pchIn) & 0xf0) >> 4];
        pchOut++;
        *pchOut = g_rgchSemiByteToCharMap[(*pchIn) & 0x0f];
        pchOut++;

        pchIn++;
    }

    rcbUsed = pchOut - sOut;
    fRet = TRUE;
    return fRet;
}

//  UCS2 to UTF8
//
// Helper fuction for UCS2 to UTF8 conversion below.
// This function was copied from external/qemu/telephony/gsm.c
int
utf8_write( unsigned char*  utf8, int  offset, int  v )
{
    int  result;

    if (v < 128) {
        result = 1;
        if (utf8)
            utf8[offset] = (unsigned char) v;
    } else if (v < 0x800) {
        result = 2;
        if (utf8) {
            utf8[offset+0] = (unsigned char)( 0xc0 | (v >> 6) );
            utf8[offset+1] = (unsigned char)( 0x80 | (v & 0x3f) );
        }
    } else if (v < 0x10000) {
        result = 3;
        if (utf8) {
            utf8[offset+0] = (unsigned char)( 0xe0 |  (v >> 12) );
            utf8[offset+1] = (unsigned char)( 0x80 | ((v >> 6) & 0x3f) );
            utf8[offset+2] = (unsigned char)( 0x80 |  (v & 0x3f) );
        }
    } else {
        result = 4;
        if (utf8) {
            utf8[offset+0] = (unsigned char)( 0xf0 | ((v >> 18) & 0x7) );
            utf8[offset+1] = (unsigned char)( 0x80 | ((v >> 12) & 0x3f) );
            utf8[offset+2] = (unsigned char)( 0x80 | ((v >> 6) & 0x3f) );
            utf8[offset+3] = (unsigned char)( 0x80 |  (v & 0x3f) );
        }
    }
    return  result;
}

// convert a UCS2 string into a UTF8 byte string, assumes 'buf' is correctly sized
// This function was copied from external/qemu/telephony/gsm.c
int
ucs2_to_utf8( const unsigned char*  ucs2,
              int       ucs2len,
              unsigned char*   buf )
{
    int  nn;
    int  result = 0;

    for (nn = 0; nn < ucs2len; ucs2 += 2, nn++) {
        int  c= (ucs2[0] << 8) | ucs2[1];
        result += utf8_write(buf, result, c);
    }
    return result;
}


CSelfExpandBuffer::CSelfExpandBuffer() : m_szBuffer(NULL), m_uiUsed(0), m_nCapacity(0)
{
}

CSelfExpandBuffer::~CSelfExpandBuffer()
{
    delete[] m_szBuffer;
    m_szBuffer = NULL;
}

BOOL CSelfExpandBuffer::Append(const char *szIn, UINT32 nLength)
{
    BOOL   bRet = FALSE;
    UINT32 nNewSize;

    if (NULL != szIn && nLength != 0)
    {
        if (NULL == m_szBuffer)
        {
            m_szBuffer = new char[m_nChunkSize];
            if (NULL == m_szBuffer)
                goto Error;

            m_nCapacity = m_nChunkSize;
        }

        for (nNewSize = m_nCapacity; (nNewSize - m_uiUsed) <= nLength; nNewSize += m_nChunkSize);
        if (nNewSize != m_nCapacity)
        {
            // allocate more space for the data
            char* tmp = new char[nNewSize];
            if (NULL == tmp)
                goto Error;

            memcpy(tmp, m_szBuffer, m_uiUsed);
            delete[] m_szBuffer;
            m_szBuffer = tmp;
            m_nCapacity = nNewSize;
        }
        memcpy(m_szBuffer + m_uiUsed, szIn, nLength);
        m_uiUsed += nLength;
        m_szBuffer[m_uiUsed] = '\0';
    }

    bRet = TRUE;

Error:
    return bRet;
}

BOOL CopyStringNullTerminate(char * const pszOut, const char * pszIn, const UINT32 cbOut)
{
    BOOL fRet = TRUE;

    //RIL_LOG_VERBOSE("CopyStringNullTerminate() - Enter\r\n");

    if ((NULL != pszIn) && (NULL != pszOut))
    {
        UINT32 cbIn = strlen(pszIn);

        strncpy(pszOut, pszIn, cbOut);

        //  Klokworks fix here
        pszOut[cbOut - 1] = '\0';
        //  End Klokworks fix

        if (cbOut <= cbIn)
        {
            fRet = FALSE;
            pszOut[cbOut - 1] = '\0';
        }
    }

    //RIL_LOG_VERBOSE("CopyStringNullTerminate() - Exit\r\n");

    return fRet;
}

BOOL PrintStringNullTerminate(char * const pszOut, const UINT32 cbOut, const char * pszFormat, ... )
{
    BOOL fRet = TRUE;
    int iWritten;

    //RIL_LOG_VERBOSE("PrintStringNullTerminate() - Enter\r\n");

    va_list args;
    va_start(args, pszFormat);

    iWritten = vsprintf(pszOut, pszFormat, args);

    if (0 > iWritten)
    {
        fRet = FALSE;
        pszOut[0] = '\0';
    }
    else if ((UINT32)iWritten >= cbOut)
    {
        fRet = FALSE;
        pszOut[cbOut - 1] = '\0';
    }

    va_end(args);

    //RIL_LOG_VERBOSE("PrintStringNullTerminate() - Exit\r\n");
    return fRet;
}

BOOL ConcatenateStringNullTerminate(char * const pszOut, const UINT32 cbOut, const char * const pszIn)
{
    BOOL fRet = FALSE;
    UINT32 cbOutStrLen = strnlen(pszOut, cbOut);
    UINT32 cbInStrLen = strlen(pszIn);

    //RIL_LOG_VERBOSE("ConcatenateStringNullTerminate() - Enter\r\n");

    strncat(pszOut, pszIn, cbOut - cbOutStrLen);

    if (cbOut > cbOutStrLen + cbInStrLen)
    {
        // The string fit with no truncation
        fRet = TRUE;
    }
    else
    {
        // Not enough room! Null terminate the string for safety.
        pszOut[cbOut - 1] = '\0';
    }

    //RIL_LOG_VERBOSE("ConcatenateStringNullTerminate() - Exit\r\n");

    return fRet;
}

CRLFExpandedString::CRLFExpandedString(const char * const pszIn, const int nInLen)
{
    UINT32 nCRLFs = 0;
    UINT32 nNewLen = 0;
    UINT32 nOther = 0;
    char * pszNewString = NULL;
    m_pszString = NULL;

    if (NULL == pszIn)
    {
        return;
    }

    for (int nWalk = 0; nWalk < nInLen; nWalk++)
    {
        if ((0x0A == pszIn[nWalk]) || (0x0D == pszIn[nWalk]))
        {
            nCRLFs++;
        }
        else if ( (pszIn[nWalk] < 0x20) || (pszIn[nWalk] > 0x7E) )
        {
            nOther++;
        }
    }

    //RIL_LOG_INFO("CRLFExpandedString::CRLFExpandedString() : Found %d instances of CR and LF combined\r\n", nCRLFs);

    // Size increase for each instance is from 1 char to 4 chars
    nNewLen = nInLen + (nCRLFs * 3) + (nOther * 3) + 1;
    m_pszString = new char[nNewLen];
    if (NULL == m_pszString)
    {
        return;
    }
    memset(m_pszString, 0, nNewLen);

    for (int nWalk = 0; nWalk < nInLen; nWalk++)
    {
        if (0x0A == pszIn[nWalk])
        {
            strncat(m_pszString, "<lf>", nNewLen - strlen(m_pszString) - 1);
        }
        else if (0x0D == pszIn[nWalk])
        {
            strncat(m_pszString, "<cr>", nNewLen - strlen(m_pszString) - 1);
        }
        else if ((pszIn[nWalk] >= 0x20) && (pszIn[nWalk] <= 0x7E))
        {
            strncat(m_pszString, &pszIn[nWalk], 1);
        }
        else
        {
            char szTmp[5] = {0};
            snprintf(szTmp, 5, "[%02X]", pszIn[nWalk]);
            strncat(m_pszString, szTmp, nNewLen - strlen(m_pszString) - 1);
        }
    }
}

CRLFExpandedString::~CRLFExpandedString()
{
    delete [] m_pszString;
    m_pszString = NULL;
}

void Sleep(UINT32 dwTimeInMs)
{
    struct timeval tv;

    tv.tv_sec = dwTimeInMs / 1000;
    tv.tv_usec = (dwTimeInMs % 1000) * 1000;

    select(0, NULL, NULL, NULL, &tv);
}

UINT32 GetTickCount()
{
    struct timeval t;
    gettimeofday( &t, NULL );
    return (t.tv_sec * 1000) + (t.tv_usec / 1000);
}

// Convert a C-string representing hexadecimal buffer ("4E2D...") to an hex buffer {0x4E, 0x2D}
// inStr: string representing hexadecimals to convert
// pOutHex: pointer to output buffer (size should be enough to contain the information)
void convertStrToHexBuf(const char* inStr, unsigned char** pOutHexStr)
{
    unsigned int i;
    unsigned int hex = 0;
    unsigned int len = strlen(inStr)/2;

    if (NULL == inStr)
    {
        RIL_LOG_CRITICAL("convertStrToHexBuf() - inStr is NULL\r\n");
        return;
    }

    if (NULL == pOutHexStr || NULL == *pOutHexStr)
    {
        RIL_LOG_CRITICAL("convertStrToHexBuf() - pOutHexStr is NULL\r\n");
        return;
    }

    for(i=0; i<len; i++)
    {
        sscanf(inStr + i*2, "%2x", &hex);
        (*pOutHexStr)[i] = hex;
    }
}

// convert an Integer into a byte array in Big Endian format
void convertIntToByteArray(unsigned char* byteArray, int value)
{
    convertIntToByteArrayAt(byteArray, value, 0);
}

// convert an Integer into a byte array in Big Endian format starting at 'pos'
void convertIntToByteArrayAt(unsigned char* byteArray, int value, int pos)
{
    for (unsigned int i = 0; i < sizeof(int); i++)
    {
        byteArray[i + pos] = (unsigned char) ((htonl(value) >> (i*8)) & 0xFF);
    }
}


