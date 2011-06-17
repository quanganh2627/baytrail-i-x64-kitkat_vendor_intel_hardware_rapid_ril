////////////////////////////////////////////////////////////////////////////
// extract.cpp
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//
//
// Author:  Mike Worth
// Created: 2009-07-08
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  July 8/09  MW       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "../../CORE/util.h"
#include "extract.h"


//char g_szNewLine[] = "\r\n";

// Takes in a string to search for a substring. If found, returns true and rszEnd
// points to the first character after szSkip.
BOOL FindAndSkipString(const BYTE* szStart, const BYTE* szSkip, const BYTE*& rszEnd)
{
    BOOL fRet = FALSE;

    //  Skip over any spaces
    SkipSpaces(szStart, szStart);

    rszEnd = strstr(szStart, szSkip);

    if (rszEnd)
    {
        rszEnd += strlen(szSkip);
        fRet = TRUE;
    }
    else
    {
        rszEnd = szStart;
    }

    return fRet;
}

// Moves the pointer past any spaces in the response. Returns TRUE if spaces were skipped.
BOOL SkipSpaces(const BYTE* szStart, const BYTE*& rszEnd)
{
    BOOL fRet = FALSE;

    unsigned int nSize = strspn(szStart, " ");

    if (nSize > 0)
    {
        rszEnd = szStart + nSize;
        fRet = TRUE;
    }

    return fRet;
}

// Takes in a string to compare with a substring. If the substring matches with
// szStart, the function returns true and rszEnd points to the first character after szSkip.
BOOL SkipString(const BYTE* szStart, const BYTE* szSkip, const BYTE*& rszEnd)
{
    BOOL fRet = FALSE;
    UINT32 dwResult;

    //  Skip over any spaces
    SkipSpaces(szStart, szStart);

    dwResult = strncmp(szStart, szSkip, strlen(szSkip));

    if (!dwResult)
    {
        rszEnd = szStart + strlen(szSkip);
        fRet = TRUE;
    }
    else
    {
        rszEnd = szStart;
    }

    return fRet;
}

// Looks for a carriage return/line feed at the start of szStart. If found, returns true
// and sets rszEnd to the first character after the pattern.
BOOL SkipRspStart(const BYTE* szStart, const BYTE* szSkip, const BYTE*& rszEnd)
{
    BOOL fRet = FALSE;

    //  Skip over any spaces
    SkipSpaces(szStart, szStart);

    if (SkipString(szStart, szSkip, rszEnd))
    {
        fRet = TRUE;
    }

    return fRet;
}

// Looks for a carriage return/line feed at the start of szStart. If found, returns true
// and sets rszEnd to the first character after the pattern. This is identical to SkipRspStart
// but is provided in case the modem responds with a different pattern for response ends and beginnings.
BOOL SkipRspEnd(const BYTE* szStart, const BYTE* szSkip, const BYTE*& rszEnd)
{
    BOOL fRet = FALSE;

    //  Skip over any spaces
    SkipSpaces(szStart, szStart);

    if (SkipString(szStart, szSkip, rszEnd))
    {
        fRet = TRUE;
    }

    return fRet;
}

// Looks for a carriage return/line feed anywhere in szStart. If found, returns true
// and sets rszEnd to the first character after the pattern.
BOOL FindAndSkipRspEnd(const BYTE* szStart, const BYTE* szSkip, const BYTE*& rszEnd)
{
    BOOL fRet = FALSE;

    //  Skip over any spaces
    SkipSpaces(szStart, szStart);

    if (FindAndSkipString(szStart, szSkip, rszEnd))
    {
        fRet = TRUE;
    }

    return fRet;
}

// Takes the digits in szStart and stores them into a UINT32. If a space follows the last
// digit it will also be consumed. Returns TRUE if at least one digit is found.
BOOL ExtractUInt(const BYTE* szStart, UINT32 &rnValue, const BYTE* &rszEnd)
{
    BOOL fRet = FALSE;

    //  Skip over any spaces
    SkipSpaces(szStart, szStart);

    UINT32 nTemp = 0;
    const BYTE* szWalk = szStart;

    while (('0' <= *szWalk) && ('9' >= *szWalk))
    {
        nTemp *= 10;
        nTemp += ((UINT32)*szWalk++ - '0');

        // As long as we found one digit, the result is valid
        fRet = TRUE;
    }

    // If we find a space after, eat it up too
    SkipString(szWalk, " ", szWalk);

    if (fRet)
    {
        rszEnd  = szWalk;
        rnValue = nTemp;
    }
    else
    {
        rszEnd = szStart;
    }

    return fRet;
}


// Extracts a UINT32 from a given string.
BOOL ExtractUInt32(const BYTE* szStart, UINT32 &rdwValue, const BYTE* &rszEnd)
{
    //  Skip over any spaces
    SkipSpaces(szStart, szStart);

    return ExtractUInt(szStart, (UINT32&)rdwValue, rszEnd);
}

// Extracts a string enclosed by quotes into a given buffer. Returns TRUE if two quotes are found and
// the buffer given is large enough to contain the string and a NULL termination character.
BOOL ExtractQuotedString(const BYTE* szStart, BYTE* szOutput, const UINT32 cbOutput, const BYTE* &rszEnd)
{
    BOOL fRet = FALSE;
    const BYTE* szWalk = NULL;
    UINT32 nLen = 0;

    //  Skip over any spaces
    SkipSpaces(szStart, szStart);

    memset(szOutput, 0, cbOutput);
    rszEnd = szStart;

    if ((SkipString(szStart, "\"", szWalk)))
    {
        if (FindAndSkipString(szWalk, "\"", rszEnd))
        {
            nLen = rszEnd - szWalk - 1;

            if (cbOutput > nLen)
            {
                strncpy(szOutput, szWalk, nLen);
                fRet = TRUE;
            }
        }
    }

    return fRet;
}

// Extracts a string ended by cDelimiter into a given buffer. Returns TRUE if cDelimiter is found and
// the buffer given is large enough to contain the string and a NULL termination character.
BOOL ExtractUnquotedString(const BYTE* szStart, const char cDelimiter, BYTE* szOutput, const UINT32 cbOutput, const BYTE* &rszEnd)
{
    char tmp[2] = {cDelimiter, '\0'};

    //  Skip over any spaces
    SkipSpaces(szStart, szStart);

    return ExtractUnquotedString(szStart, tmp, szOutput, cbOutput, rszEnd);
}

// Extracts a string ended by szDelimiter into a given buffer. Returns TRUE if szDelimiter is found and
// the buffer given is large enough to contain the string and a NULL termination character.
BOOL ExtractUnquotedString(const BYTE* szStart, const BYTE* szDelimiter, BYTE* szOutput, const UINT32 cbOutput, const BYTE* &rszEnd)
{
    BOOL fRet = FALSE;
    UINT32 nLen = 0;

    //  Skip over any spaces
    SkipSpaces(szStart, szStart);

    memset(szOutput, 0, cbOutput);
    rszEnd = szStart;

    if (FindAndSkipString(szStart, szDelimiter, rszEnd))
    {
        nLen = rszEnd - szStart - strlen(szDelimiter);

        if (cbOutput > nLen)
        {
            strncpy(szOutput, szStart, nLen);
            rszEnd -= strlen(szDelimiter);
            fRet = TRUE;
        }
    }

    return fRet;
}

// Extracts a UINT32 from hex ascii
BOOL ExtractHexUInt32(const BYTE* szStart, UINT32 &rdwValue, const BYTE* &rszEnd)
{
    BOOL fRet = FALSE;
    UINT32 dwTemp  = 0;
    UINT32 dwTotal = 0;

    //  Skip over any spaces
    SkipSpaces(szStart, szStart);

    const BYTE* szWalk = szStart;

    while ((('A' <= *szWalk) && (*szWalk <= 'F')) ||
           (('a' <= *szWalk) && (*szWalk <= 'f')) ||
           (('0' <= *szWalk) && (*szWalk <= '9')))
    {
        // Figure out the numeric value
        if (('A' <= *szWalk) && (*szWalk <= 'F'))
        {
            dwTemp = 10 + (UINT32)(*szWalk - 'A');
        }
        else if (('a' <= *szWalk) && (*szWalk <= 'f'))
        {
            dwTemp = 10 + (UINT32)(*szWalk - 'a');
        }
        else
        {
            dwTemp = (UINT32)(*szWalk - '0');
        }

        // Add it to our running tally
        dwTotal  = (dwTotal * 16) + dwTemp;

        // If we got one valid digit, we can return TRUE
        fRet = TRUE;

        szWalk++;
    }

    if (fRet)
    {
        rszEnd   = szWalk;
        rdwValue = dwTotal;
    }
    else
    {
        rszEnd = szStart;
    }

    return fRet;
}

// Extracts a UINT32 out from a hex string
BOOL ExtractHexUInt(const BYTE* szStart, UINT32& rnValue, const BYTE* &rszEnd)
{
    //  Skip over any spaces
    SkipSpaces(szStart, szStart);

    return ExtractHexUInt32(szStart, (UINT32&)rnValue, rszEnd);
}

// Allocates memory for the quoted string extracted from the given buffer and returns it. Caller must delete
// the memory when finished with it.
BOOL ExtractQuotedStringWithAllocatedMemory(const BYTE* szStart, BYTE* &rszString, UINT32 &rcbString, const BYTE* &rszEnd)
{
    BOOL fRet = FALSE;
    const BYTE* szWalk = NULL;
    UINT32 nLen = 0;

    //  Skip over any spaces
    SkipSpaces(szStart, szStart);

    rszEnd = szStart;

    if (SkipString(szStart, "\"", szWalk))
    {
        if (FindAndSkipString(szWalk, "\"", rszEnd))
        {
            nLen = rszEnd - szWalk - 1;

            rszString = new char[nLen + 1];

            if (NULL == rszString)
            {
                goto Error;
            }

            memset(rszString, 0, (nLen + 1));

            rcbString = sizeof(char) * (nLen + 1);

            strncpy(rszString, szWalk, nLen);
            rszString[nLen] = '\0';
            fRet = TRUE;
        }
    }

Error:
    if (!fRet)
    {
        rszEnd = szStart;
    }

    return fRet;
}

// Allocates memory for the unquoted string extrated from the given buffer and returns it. Caller must delete
// the memory when finished with it.
BOOL ExtractUnquotedStringWithAllocatedMemory(const BYTE* szStart, const char chDelimiter, BYTE* &rszString, UINT32 &rcbString, const BYTE* &rszEnd)
{
    BOOL fRet = FALSE;

    //  Skip over any spaces
    SkipSpaces(szStart, szStart);

    char szTemp[2] = {0};
    szTemp[0] = chDelimiter;
    szTemp[1] = '\0';
    if (FindAndSkipString(szStart, szTemp, rszEnd))
    {
        UINT32 nSize = rszEnd - szStart;

        rszString = new char[nSize];

        if (NULL == rszString)
        {
            goto Error;
        }

        strncpy(rszString, szStart, nSize - 1);
        rszString[nSize-1] = '\0';
        rcbString = nSize - 1;
        fRet = TRUE;
    }

Error:
    if (!fRet)
    {
        rszEnd = szStart;
    }

    return fRet;
}

// Extracts a decimal number and stores it as a 16.16 fixed point value
BOOL ExtractFixedPointValue(const BYTE* szStart, UINT32 &rdwFPValue, const BYTE* &rszEnd)
{
    BOOL fRet = FALSE;
    UINT32 dwWhole = 0;
    UINT32 dwFraction = 0;
    UINT32 dwBase = 1;

    //  Skip over any spaces
    SkipSpaces(szStart, szStart);

    const BYTE* szWalk = szStart;
    const BYTE* szTemp;


    rszEnd = szStart;

    if (ExtractUInt32(szWalk, dwWhole, szWalk))
    {
        if (SkipString(szWalk, ".", szWalk))
        {
            if (ExtractUInt32(szWalk, dwFraction, szTemp))
            {
                for (int i = 0; i < (szTemp - szWalk); i++)
                {
                    dwBase *= 10;
                }

                // Now combine that with our whole number left shifted 16 bits
                // so we have 0xWWWWFFFF with FFFF being equal to 0.XXX * 65536
                // which we implement by using (0.XXX * (10 ^ n) * Max_Value)/(10 ^ n)
                // Note that we extracted 0.XXX as (0.XXX * (10 ^ n))
                rdwFPValue = (dwWhole << 16) + ((dwFraction << 16) / dwBase);

                rszEnd = szTemp;
                fRet = TRUE;
            }
        }
    }

    return fRet;
}

// Extracts a decimal value and returns it as a double
BOOL ExtractDouble(const BYTE* szStart, double &rdbValue, const BYTE* &rszEnd)
{
    BOOL fRet = FALSE;
    UINT32 dwWhole = 0;
    UINT32 dwFraction = 0;
    UINT32 dwBase = 1;

    //  Skip over any spaces
    SkipSpaces(szStart, szStart);

    const BYTE* szWalk = szStart;
    const BYTE* szTemp;


    rszEnd = szStart;

    if (ExtractUInt32(szWalk, dwWhole, szWalk))
    {
        if (SkipString(szWalk, ".", szWalk))
        {
            if (ExtractUInt32(szWalk, dwFraction, szTemp))
            {
                for (int i = 0; i < (szTemp - szWalk); i++)
                {
                    dwBase *= 10;
                }

                rdbValue = ((double)dwWhole) + (((double)dwFraction)/(double)dwBase);
                rszEnd = szTemp;
                fRet = TRUE;
            }
        }
    }

    return fRet;
}


BOOL ExtractUpperBoundedUInt(const char* szData,
                             const UINT32 nUpperLimit,
                             UINT32& rnVal,
                             const char*& rszRemainder)
{
    //  Skip over any spaces
    SkipSpaces(szData, szData);

    // Extract the number
    BOOL fRet = ExtractUInt(szData, rnVal, rszRemainder);

    // check that the number is below the upper limit
    if (fRet)
    {
        if (nUpperLimit <= rnVal)
        {
            fRet = FALSE;
            rszRemainder = szData;
        }
    }
    return fRet;
}

//
//
//
BOOL ExtractLowerBoundedUInt(const char* szData,
                             const UINT32 nLowerLimit,
                             UINT32& rnVal,
                             const char*& rszRemainder)
{
    //  Skip over any spaces
    SkipSpaces(szData, szData);

    // Extract the number
    BOOL fRet = ExtractUInt(szData, rnVal, rszRemainder);

    // check that the number is above the lower limit
    if (fRet)
    {
        if (nLowerLimit >= rnVal)
        {
            fRet = FALSE;
            rszRemainder = szData;
        }
    }
    return fRet;
}


BOOL ExtractIntAndConvertToUInt(const char* szData, UINT32& rnVal, const char*& rszRemainder)
{
    BOOL fNegative = FALSE;
    BOOL fRet = FALSE;

    //  Skip over any spaces
    SkipSpaces(szData, szData);

    if ('-' == *szData)
    {
        rszRemainder = szData + 1;
        fNegative = TRUE;
    }
    else
    {
        rszRemainder = szData;
    }

    if (ExtractUInt(rszRemainder, rnVal, rszRemainder))
    {
        if (fNegative)
        {
            rnVal = ~rnVal + 1;
        }
        fRet = TRUE;
    }

    return fRet;
}
