
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
// Author:  Dennis Peter
// Created: 2007-07-12
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  June 3/08  DP       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include "../../CORE/util.h"
#include "extract.h"

#include <wchar.h>
#include <sys/select.h>

#ifdef assert
#undef assert
#endif

#if 1
    unsigned char const GSM_DEFAULT(0xFF);
#else
    unsigned char const GSM_DEFAULT(0x7F);
#endif

#define ALLOC_SIZE   1024
#define minimum_of(a,b) (((a) < (b)) ? (a) : (b))

RIL_RESULT_CODE ConvertUnicodeCharToGSMDefaultChar(WCHAR wcUnicode, char &cGSMDefault)
{
    RIL_RESULT_CODE hr = RRIL_E_UNKNOWN_ERROR;
    UINT32 uiCharMapIndex = 0;

    while (g_CharMap_Unicode_vs_GSMDefault[uiCharMapIndex].wcUnicode != 0xFFFF)
    {
        if (g_CharMap_Unicode_vs_GSMDefault[uiCharMapIndex].wcUnicode == wcUnicode)
        {
            cGSMDefault = g_CharMap_Unicode_vs_GSMDefault[uiCharMapIndex].cGSMDefault;
            hr = RRIL_E_NO_ERROR;
            break;
        }

        uiCharMapIndex++;
    }

    return hr;
}


RIL_RESULT_CODE ConvertGSMDefaultCharToUnicodeChar(const char cGSMDefault, WCHAR &wcUnicode)
{
    RIL_RESULT_CODE hr = RRIL_E_UNKNOWN_ERROR;
    UINT32 uiCharMapIndex = 0;

    while (g_CharMap_Unicode_vs_GSMDefault[uiCharMapIndex].cGSMDefault != 0xFF)
    {
        if (g_CharMap_Unicode_vs_GSMDefault[uiCharMapIndex].cGSMDefault == cGSMDefault)
        {
            wcUnicode = g_CharMap_Unicode_vs_GSMDefault[uiCharMapIndex].wcUnicode;
            hr = RRIL_E_NO_ERROR;
            break;
        }

        uiCharMapIndex++;
    }

    return hr;
}

RIL_RESULT_CODE UnicodeToGSMDefaultPackedHex(WCHAR * wszUnicodeSrc,
                                     UINT32    uiUnicodeSrcLength,
                                     char *  szGSMDefaultPackedDest,
                                     UINT32    uiGSMDefaultPackedDestLength)
{
    RIL_RESULT_CODE hr = RRIL_E_NO_ERROR;
    BYTE * convertedByteArray = NULL;
    UINT32 uiArrayIndex  = 0;
    UINT32 uiSrcIndex    = 0;
    char cGSMDefault = GSM_DEFAULT;

    // 14 Encoded Destination chars per 8 Source chars
    // Note that there are two special cases that we need to accomodate,
    // one of which results in an extra byte (and thus two extra Dest chars).
    const UINT32 uiNumSourceBytes = (((uiUnicodeSrcLength * 7) / 8) + 1);
    const UINT32 uiNumDestChars   = (uiNumSourceBytes * 2) + 1;

    if (uiGSMDefaultPackedDestLength < uiNumDestChars)
    {
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    if ((NULL == wszUnicodeSrc) || (NULL == szGSMDefaultPackedDest))
    {
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    // First convert the source string into an encoded byte array.
    // The string "Testing" thus becomes the following array:
    //  array[0] = 0xD4
    //  array[1] = 0xF2
    //  array[2] = 0x9C
    //  array[3] = 0x9E
    //  array[4] = 0x76
    //  array[5] = 0x9F
    //  array[6] = 0x1B

    // Note that the above case illustrates one of the special cases:
    // If the encoded string ends one 7-bit char before a byte boundary,
    // pad the last byte with a <CR> char instead of 7 0s.

    // Once the source string has been converted to an encoded byte array,
    // we will then convert it into the destination string.
    // In the above case, the destination string is "D4F29C9E769F1B".

    convertedByteArray = new BYTE[uiNumSourceBytes];
    if (NULL == convertedByteArray)
    {
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    memset(convertedByteArray, 0x00, sizeof(BYTE) * uiNumSourceBytes);

    while (uiSrcIndex < uiUnicodeSrcLength)
    {
        // CHAR 1 of 8

        if ((uiUnicodeSrcLength - uiSrcIndex) > 0)
        {
            hr = ConvertUnicodeCharToGSMDefaultChar(wszUnicodeSrc[uiSrcIndex], cGSMDefault);
            if (RRIL_E_NO_ERROR != hr)
            {
                hr = RRIL_E_UNKNOWN_ERROR;
                goto Error;
            }

            convertedByteArray[uiArrayIndex + 0] = (convertedByteArray[uiArrayIndex + 0] & 0x80) + (cGSMDefault & 0x7F);

            uiSrcIndex++;
        }

        // CHAR 2 of 8

        if ((uiUnicodeSrcLength - uiSrcIndex) > 0)
        {
            hr = ConvertUnicodeCharToGSMDefaultChar(wszUnicodeSrc[uiSrcIndex], cGSMDefault);
            if (RRIL_E_NO_ERROR != hr)
            {
                hr = RRIL_E_UNKNOWN_ERROR;
                goto Error;
            }

            convertedByteArray[uiArrayIndex + 0] = (convertedByteArray[uiArrayIndex + 0] & 0x7F) + ((cGSMDefault & 0x01) << 7);
            convertedByteArray[uiArrayIndex + 1] = (convertedByteArray[uiArrayIndex + 1] & 0xC0) + ((cGSMDefault & 0x7E) >> 1);

            uiSrcIndex++;
        }

        // CHAR 3 of 8

        if ((uiUnicodeSrcLength - uiSrcIndex) > 0)
        {
            hr = ConvertUnicodeCharToGSMDefaultChar(wszUnicodeSrc[uiSrcIndex], cGSMDefault);
            if (RRIL_E_NO_ERROR != hr)
            {
                hr = RRIL_E_UNKNOWN_ERROR;
                goto Error;
            }

            convertedByteArray[uiArrayIndex + 1] = (convertedByteArray[uiArrayIndex + 1] & 0x3F) + ((cGSMDefault & 0x03) << 6);
            convertedByteArray[uiArrayIndex + 2] = (convertedByteArray[uiArrayIndex + 2] & 0xE0) + ((cGSMDefault & 0x7C) >> 2);

            uiSrcIndex++;
        }

        // CHAR 4 of 8

        if ((uiUnicodeSrcLength - uiSrcIndex) > 0)
        {
            hr = ConvertUnicodeCharToGSMDefaultChar(wszUnicodeSrc[uiSrcIndex], cGSMDefault);
            if (RRIL_E_NO_ERROR != hr)
            {
                hr = RRIL_E_UNKNOWN_ERROR;
                goto Error;
            }

            convertedByteArray[uiArrayIndex + 2] = (convertedByteArray[uiArrayIndex + 2] & 0x1F) + ((cGSMDefault & 0x07) << 5);
            convertedByteArray[uiArrayIndex + 3] = (convertedByteArray[uiArrayIndex + 3] & 0xF0) + ((cGSMDefault & 0x78) >> 3);

            uiSrcIndex++;
        }

        // CHAR 5 of 8

        if ((uiUnicodeSrcLength - uiSrcIndex) > 0)
        {
            hr = ConvertUnicodeCharToGSMDefaultChar(wszUnicodeSrc[uiSrcIndex], cGSMDefault);
            if (RRIL_E_NO_ERROR != hr)
            {
                hr = RRIL_E_UNKNOWN_ERROR;
                goto Error;
            }

            convertedByteArray[uiArrayIndex + 3] = (convertedByteArray[uiArrayIndex + 3] & 0x0F) + ((cGSMDefault & 0x0F) << 4);
            convertedByteArray[uiArrayIndex + 4] = (convertedByteArray[uiArrayIndex + 4] & 0xF8) + ((cGSMDefault & 0x70) >> 4);

            uiSrcIndex++;
        }

        // CHAR 6 of 8

        if ((uiUnicodeSrcLength - uiSrcIndex) > 0)
        {
            hr = ConvertUnicodeCharToGSMDefaultChar(wszUnicodeSrc[uiSrcIndex], cGSMDefault);
            if (RRIL_E_NO_ERROR != hr)
            {
                hr = RRIL_E_UNKNOWN_ERROR;
                goto Error;
            }

            convertedByteArray[uiArrayIndex + 4] = (convertedByteArray[uiArrayIndex + 4] & 0x07) + ((cGSMDefault & 0x1F) << 3);
            convertedByteArray[uiArrayIndex + 5] = (convertedByteArray[uiArrayIndex + 5] & 0xFC) + ((cGSMDefault & 0x60) >> 5);

            uiSrcIndex++;
        }

        // CHAR 7 of 8

        if ((uiUnicodeSrcLength - uiSrcIndex) > 0)
        {
            hr = ConvertUnicodeCharToGSMDefaultChar(wszUnicodeSrc[uiSrcIndex], cGSMDefault);
            if (RRIL_E_NO_ERROR != hr)
            {
                hr = RRIL_E_UNKNOWN_ERROR;
                goto Error;
            }

            convertedByteArray[uiArrayIndex + 5] = (convertedByteArray[uiArrayIndex + 5] & 0x03) + ((cGSMDefault & 0x3F) << 2);
            convertedByteArray[uiArrayIndex + 6] = (convertedByteArray[uiArrayIndex + 6] & 0xFE) + ((cGSMDefault & 0x40) >> 6);

            uiSrcIndex++;
        }

        // CHAR 8 of 8

        if ((uiUnicodeSrcLength - uiSrcIndex) > 0)
        {
            hr = ConvertUnicodeCharToGSMDefaultChar(wszUnicodeSrc[uiSrcIndex], cGSMDefault);
            if (RRIL_E_NO_ERROR != hr)
            {
                hr = RRIL_E_UNKNOWN_ERROR;
                goto Error;
            }

            convertedByteArray[uiArrayIndex + 6] = (convertedByteArray[uiArrayIndex + 6] & 0x01) + ((cGSMDefault & 0x7F) << 1);

            uiSrcIndex++;
        }

        uiArrayIndex  += 7;
    }

    // Special Case #1: If we fill 7 of the 8 chars in the final 7-byte block,
    //                  pad the last byte with a <CR> character, not 0s.

    if (7 == (uiUnicodeSrcLength % 8))
    {
        // <CR> code is 0x0d = binary 0001101, so right shift by 1 bit to get 00011010 = 0x1A
        convertedByteArray[uiNumSourceBytes - 1] = (convertedByteArray[uiNumSourceBytes - 1] & 0x01) + 0x1A;
    }

    // Special Case #2: If we fill 8 of the last 8 chars in the final 7-byte block
    //                  and if the last char is <CR>, then add another <CR> after it.

    if (0 == (uiUnicodeSrcLength % 8))
    {
        // We allocated memory for uiNumSourceBytes bytes, but the conversion
        // in this case only uses (uiNumSourceBytes - 1) bytes. We thus have
        // an extra byte left over; use it to store the terminating <CR>.

        convertedByteArray[uiNumSourceBytes - 1] = 0x0D;
    }

    // The byte array has been created, so we now use it to create the final encoded string.

    // HEX CODE
    for (UINT32 i = 0; i < uiNumSourceBytes; i++)
    {
        if (sprintf(szGSMDefaultPackedDest, "%02X", convertedByteArray[i]) != 2)
        {
            hr = RRIL_E_UNKNOWN_ERROR;
            goto Error;
        }

        szGSMDefaultPackedDest += 2;
    }

    // Ensure we are NULL terminated

    *szGSMDefaultPackedDest = '\0';

Error:
    return hr;
}


RIL_RESULT_CODE UnicodeToGSMDefaultHex(WCHAR * wszUnicodeSrc,
                               UINT32    uiUnicodeSrcLength,
                               char *  szGSMDefaultDest,
                               UINT32    uiGSMDefaultDestLength)
{
    RIL_RESULT_CODE hr = RRIL_E_NO_ERROR;
    UINT32   uiNumCharsProcessed = 0;
    UINT32   uiStrDestIndex      = 0;
    char * szDestWalk          = szGSMDefaultDest;

    if ((NULL == wszUnicodeSrc) || (NULL == szGSMDefaultDest))
    {
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    // 2 Encoded Destination chars per Source char
    if (((2 * uiUnicodeSrcLength) + 1) > uiGSMDefaultDestLength)
    {
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    // Do we have a NULL String?
    if ((0 == uiUnicodeSrcLength) || ((WCHAR)'\0' == wszUnicodeSrc[0]))
    {
        szGSMDefaultDest[0] = '\0';
        goto Error;
    }

    // Traverse the Unicode Source String, converting it to GSM Default hex in the process
    while ((uiNumCharsProcessed < uiUnicodeSrcLength) && (uiStrDestIndex < uiGSMDefaultDestLength))
    {
        char cGSMDefaultCode = GSM_DEFAULT;

        hr = ConvertUnicodeCharToGSMDefaultChar(wszUnicodeSrc[uiNumCharsProcessed], cGSMDefaultCode);
        if (RRIL_E_NO_ERROR != hr)
        {
            hr = RRIL_E_UNKNOWN_ERROR;
            goto Error;
        }

        if (2 != sprintf(szDestWalk, "%02X", cGSMDefaultCode))
        {
            hr = RRIL_E_UNKNOWN_ERROR;
            goto Error;
        }

        uiNumCharsProcessed++;
        uiStrDestIndex += 2;

        szDestWalk += 2;
    }

    // Ensure that the Destination String is NULL Terminated

    szGSMDefaultDest[uiStrDestIndex] = '\0';

Error:
    return hr;
}

RIL_RESULT_CODE UnicodeToASCIIHex(WCHAR * wszUnicodeSrc,
                          UINT32    uiUnicodeSrcLength,
                          char *  szASCIIHexDest,
                          UINT32    uiASCIIHexDestLength)
{
    RIL_RESULT_CODE hr = RRIL_E_NO_ERROR;
    UINT32   uiNumCharsProcessed = 0;
    UINT32   uiStrDestIndex      = 0;
    char * szDestWalk          = szASCIIHexDest;

    if ((NULL == wszUnicodeSrc) || (NULL == szASCIIHexDest))
    {
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    // 2 Encoded Destination chars per Source char

    if (((2 * uiUnicodeSrcLength) + 1) > uiASCIIHexDestLength)
    {
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    // Do we have a NULL String?

    if ((0 == uiUnicodeSrcLength) || ((WCHAR)'\0' == wszUnicodeSrc[0]))
    {
        szASCIIHexDest[0] = '\0';
        goto Error;
    }

    // Traverse the Unicode Source String, converting it to ASCII hex in the process
    while ((uiNumCharsProcessed < uiUnicodeSrcLength) && (uiStrDestIndex < uiASCIIHexDestLength))
    {
        if (2 != sprintf(szDestWalk, "%02X", (0x000000FF & wszUnicodeSrc[uiNumCharsProcessed])))
        {
            hr = RRIL_E_UNKNOWN_ERROR;
            goto Error;
        }

        uiNumCharsProcessed++;
        uiStrDestIndex += 2;

        szDestWalk += 2;
    }

    // Ensure that the Destination String is NULL Terminated

    szASCIIHexDest[uiStrDestIndex] = '\0';

Error:
    return hr;
}

RIL_RESULT_CODE UnicodeToUnicodeHex(WCHAR * wszUnicodeSrc,
                            UINT32    uiUnicodeSrcLength,
                            char *  szUnicodeHexDest,
                            UINT32    uiUnicodeHexDestLength)
{
    RIL_RESULT_CODE hr = RRIL_E_NO_ERROR;
    UINT32   uiNumCharsProcessed = 0;
    UINT32   uiStrDestIndex      = 0;
    char * szDestWalk          = szUnicodeHexDest;

    if ((NULL == wszUnicodeSrc) || (NULL == szUnicodeHexDest))
    {
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    // 4 Encoded Destination chars per Source char

    if (((4 * uiUnicodeSrcLength) + 1) > uiUnicodeHexDestLength)
    {
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    // Do we have a NULL String?

    if ((0 == uiUnicodeSrcLength) || ((WCHAR)'\0' == wszUnicodeSrc[0]))
    {
        szUnicodeHexDest[0] = '\0';
        goto Error;
    }

    // Traverse the Unicode Source String, converting it to unicode hex in the process
    while ((uiNumCharsProcessed < uiUnicodeSrcLength) && (uiStrDestIndex < uiUnicodeHexDestLength))
    {
        if (4 != sprintf(szDestWalk, "%04X", (0x0000FFFF & wszUnicodeSrc[uiNumCharsProcessed])))
        {
            hr = RRIL_E_UNKNOWN_ERROR;
            goto Error;
        }

        uiNumCharsProcessed++;
        uiStrDestIndex += 4;

        szDestWalk += 4;
    }

    // Ensure that the Destination String is NULL Terminated

    szUnicodeHexDest[uiStrDestIndex] = '\0';

Error:
    return hr;
}

RIL_RESULT_CODE GSMDefaultPackedHexToUnicode(char  * szGSMDefaultPackedSrc,
                                     UINT32    uiGSMDefaultPackedSrcLength,
                                     WCHAR * wszUnicodeDest,
                                     UINT32    uiUnicodeDestLength)
{
    RIL_RESULT_CODE hr = RRIL_E_NO_ERROR;
    BYTE * convertedByteArray = NULL;
    char * szSrcWalk   = szGSMDefaultPackedSrc;
    UINT32   uiNumBytes  = (uiGSMDefaultPackedSrcLength / 2);
    UINT32   uiSrcIndex  = 0;
    UINT32   uiDestIndex = 0;
    char   cGSMDefault = GSM_DEFAULT;

    // 14 Encoded Source chars per 8 Destination chars
    const UINT32 uiNumDestChars = (((uiGSMDefaultPackedSrcLength / 2) * 8) / 7);
    if (uiUnicodeDestLength < uiNumDestChars + 1)
    {
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    if ((NULL == szGSMDefaultPackedSrc) || (NULL == wszUnicodeDest))
    {
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    // 2 chars per byte, and we need a whole number of bytes.

    if ((uiGSMDefaultPackedSrcLength % 2) != 0)
    {
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    // First convert the source string into a byte array.
    // The string "5673626732" thus becomes the following array:
    //  array[0] = 0x56
    //  array[1] = 0x73
    //  array[2] = 0x62
    //  array[3] = 0x67
    //  array[4] = 0x32

    convertedByteArray = new BYTE[uiNumBytes];
    if (NULL == convertedByteArray)
    {
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    for (UINT32 i = 0; i < uiNumBytes; i++)
    {
        if (sscanf(szSrcWalk, "%02X", (unsigned int*)&(convertedByteArray[i])) <= 0)
        {
            hr = RRIL_E_UNKNOWN_ERROR;
            goto Error;
        }

        szSrcWalk += 2;
    }

    while (uiDestIndex < uiNumDestChars)
    {
        // Convert an entire block of 7 encoded source chars into 8 dest chars.

        if ((uiNumDestChars - uiDestIndex) > 0)
        {
            cGSMDefault = convertedByteArray[uiSrcIndex + 0] & 0x7F;

            hr = ConvertGSMDefaultCharToUnicodeChar(cGSMDefault, wszUnicodeDest[uiDestIndex]);
            if (RRIL_E_NO_ERROR != hr)
            {
                hr = RRIL_E_UNKNOWN_ERROR;
                goto Error;
            }

            uiDestIndex++;
        }

        if ((uiNumDestChars - uiDestIndex) > 0)
        {
            cGSMDefault = ((convertedByteArray[uiSrcIndex + 0] & 0x80) >> 7) + ((convertedByteArray[uiSrcIndex + 1] & 0x3F) << 1);

            hr = ConvertGSMDefaultCharToUnicodeChar(cGSMDefault, wszUnicodeDest[uiDestIndex]);
            if (RRIL_E_NO_ERROR != hr)
            {
                hr = RRIL_E_UNKNOWN_ERROR;
                goto Error;
            }

            uiDestIndex++;
        }

        if ((uiNumDestChars - uiDestIndex) > 0)
        {
            cGSMDefault = ((convertedByteArray[uiSrcIndex + 1] & 0xC0) >> 6) + ((convertedByteArray[uiSrcIndex + 2] & 0x1F) << 2);

            hr = ConvertGSMDefaultCharToUnicodeChar(cGSMDefault, wszUnicodeDest[uiDestIndex]);
            if (RRIL_E_NO_ERROR != hr)
            {
                hr = RRIL_E_UNKNOWN_ERROR;
                goto Error;
            }

            uiDestIndex++;
        }

        if ((uiNumDestChars - uiDestIndex) > 0)
        {
            cGSMDefault = ((convertedByteArray[uiSrcIndex + 2] & 0xE0) >> 5) + ((convertedByteArray[uiSrcIndex + 3] & 0x0F) << 3);

            hr = ConvertGSMDefaultCharToUnicodeChar(cGSMDefault, wszUnicodeDest[uiDestIndex]);
            if (RRIL_E_NO_ERROR != hr)
            {
                hr = RRIL_E_UNKNOWN_ERROR;
                goto Error;
            }

            uiDestIndex++;
        }

        if ((uiNumDestChars - uiDestIndex) > 0)
        {
            cGSMDefault = ((convertedByteArray[uiSrcIndex + 3] & 0xF0) >> 4) + ((convertedByteArray[uiSrcIndex + 4] & 0x07) << 4);

            hr = ConvertGSMDefaultCharToUnicodeChar(cGSMDefault, wszUnicodeDest[uiDestIndex]);
            if (RRIL_E_NO_ERROR != hr)
            {
                hr = RRIL_E_UNKNOWN_ERROR;
                goto Error;
            }

            uiDestIndex++;
        }

        if ((uiNumDestChars - uiDestIndex) > 0)
        {
            cGSMDefault = ((convertedByteArray[uiSrcIndex + 4] & 0xF8) >> 3) + ((convertedByteArray[uiSrcIndex + 5] & 0x03) << 5);

            hr = ConvertGSMDefaultCharToUnicodeChar(cGSMDefault, wszUnicodeDest[uiDestIndex]);
            if (RRIL_E_NO_ERROR != hr)
            {
                hr = RRIL_E_UNKNOWN_ERROR;
                goto Error;
            }

            uiDestIndex++;
        }

        if ((uiNumDestChars - uiDestIndex) > 0)
        {
            cGSMDefault = ((convertedByteArray[uiSrcIndex + 5] & 0xFC) >> 2) + ((convertedByteArray[uiSrcIndex + 6] & 0x01) << 6);

            hr = ConvertGSMDefaultCharToUnicodeChar(cGSMDefault, wszUnicodeDest[uiDestIndex]);
            if (RRIL_E_NO_ERROR != hr)
            {
                hr = RRIL_E_UNKNOWN_ERROR;
                goto Error;
            }

            uiDestIndex++;
        }

        if ((uiNumDestChars - uiDestIndex) > 0)
        {
            cGSMDefault = ((convertedByteArray[uiSrcIndex + 6] & 0xFE) >> 1);

            hr = ConvertGSMDefaultCharToUnicodeChar(cGSMDefault, wszUnicodeDest[uiDestIndex]);
            if (RRIL_E_NO_ERROR != hr)
            {
                hr = RRIL_E_UNKNOWN_ERROR;
                goto Error;
            }

            uiDestIndex++;
        }

        uiSrcIndex  += 7;
    }

    // Ensure we are NULL terminated

    wszUnicodeDest[uiDestIndex] = (WCHAR)'\0';

    // Special Cases

    // If we end one character after a byte boundary and the last
    // two characters are <CR> then replace the last <CR> with a NULL.

    if (1 == (uiNumDestChars % 8))
    {
        if (((WCHAR)'\r' == wszUnicodeDest[uiDestIndex - 1]) && ((WCHAR)'\r' == wszUnicodeDest[uiDestIndex - 2]))
        {
            wszUnicodeDest[uiDestIndex - 1] = (WCHAR)'\0';
        }
    }

    // If we end on a byte boundary and the last char is <CR>
    // then replace the final <CR> with NULL.

    if (0 == (uiNumDestChars % 8))
    {
        if ((WCHAR)'\r' == wszUnicodeDest[uiDestIndex - 1])
        {
            wszUnicodeDest[uiDestIndex - 1] = (WCHAR)'\0';
        }
    }

Error:
    return hr;
}


RIL_RESULT_CODE GSMDefaultHexToUnicode(const char * szGSMDefaultSrc,
                               UINT32   uiGSMDefaultSrcLength,
                               WCHAR* wszUnicodeDest,
                               UINT32   uiUnicodeDestLength)
{
    RIL_RESULT_CODE hr = RRIL_E_NO_ERROR;
    UINT32    uiNumCharsProcessed = 0;
    UINT32    uiStrDestIndex      = 0;
    const char * szSrcWalk = szGSMDefaultSrc;

    if ((NULL == szGSMDefaultSrc) || (NULL == wszUnicodeDest))
    {
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    if ((uiGSMDefaultSrcLength % 2) != 0)
    {
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    // 2 Encoded Source chars per Destination char

    if (((uiGSMDefaultSrcLength / 2) + 1) > uiUnicodeDestLength)
    {
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    // Do we have a NULL String?

    if ((0 == uiGSMDefaultSrcLength) || ('\0' == szGSMDefaultSrc[0]))
    {
        wszUnicodeDest[0] = (WCHAR)'\0';
        goto Error;
    }

    // Traverse the Encoded GSM Default Hex Source String, converting it to Unicode in the process
    while ((uiNumCharsProcessed < uiGSMDefaultSrcLength) && (uiStrDestIndex < uiUnicodeDestLength))
    {
        char cGSMDefaultChar = GSM_DEFAULT;
        if (sscanf(szSrcWalk, "%2X", (unsigned int*)&cGSMDefaultChar) > 0)
        {
            hr = ConvertGSMDefaultCharToUnicodeChar(cGSMDefaultChar, wszUnicodeDest[uiStrDestIndex]);
            if (RRIL_E_NO_ERROR != hr)
            {
                hr = RRIL_E_UNKNOWN_ERROR;
                goto Error;
            }
        }
        else
        {
            hr = RRIL_E_UNKNOWN_ERROR;
            goto Error;
        }

        szSrcWalk += 2;

        uiNumCharsProcessed += 2;
        uiStrDestIndex++;
    }

    // Ensure that the Destination String is NULL Terminated

    wszUnicodeDest[uiStrDestIndex] = (WCHAR)'\0';

Error:
    return hr;
}


RIL_RESULT_CODE ASCIIHexToUnicode(char * szASCIIHexSrc,
                          UINT32   uiASCIIHexSrcLength,
                          WCHAR* wszUnicodeDest,
                          UINT32   uiUnicodeDestLength)
{
    RIL_RESULT_CODE hr = RRIL_E_NO_ERROR;
    UINT32   uiNumCharsProcessed = 0;
    UINT32   uiStrDestIndex      = 0;
    char * szSrcWalk = szASCIIHexSrc;

    if (NULL == szASCIIHexSrc)
    {
        RIL_LOG_CRITICAL("ASCIIHexToUnicode - ERROR: szASCIIHexSrc is NULL.\r\n");
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    if (NULL == wszUnicodeDest)
    {
        RIL_LOG_CRITICAL("ASCIIHexToUnicode - ERROR: pwszUnicodeDest is NULL.\r\n");
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    if ((uiASCIIHexSrcLength % 2) != 0)
    {
        RIL_LOG_CRITICAL("ASCIIHexToUnicode - ERROR: Source String Length must be a multiple of 2.\r\n");
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    // 2 Encoded Source chars per Destination char

    if (((uiASCIIHexSrcLength / 2) + 1) > uiUnicodeDestLength)
    {
        RIL_LOG_CRITICAL("ASCIIHexToUnicode - ERROR: Destination String Buffer is not long enough to contain the decoded text.\r\n");
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    // Do we have a NULL String?

    if ((0 == uiASCIIHexSrcLength) || ('\0' == szASCIIHexSrc[0]))
    {
        RIL_LOG_INFO("ASCIIHexToUnicode - Handling empty string.\r\n");
        wszUnicodeDest[0] = (WCHAR)'\0';
        goto Error;
    }

    // Traverse the ASCII Hex Source String, converting it to Unicode in the process
    RIL_LOG_INFO("ASCIIHexToUnicode - Encoded String: %s.\r\n", szASCIIHexSrc);
    while ((uiNumCharsProcessed < uiASCIIHexSrcLength) && (uiStrDestIndex < uiUnicodeDestLength))
    {
        if (sscanf(szSrcWalk, "%2X", (unsigned int*)&(wszUnicodeDest[uiStrDestIndex])) <= 0)
        {
            hr = RRIL_E_UNKNOWN_ERROR;
            goto Error;
        }

        szSrcWalk += 2;

        uiNumCharsProcessed += 2;
        uiStrDestIndex++;
    }

    // Ensure that the Destination String is NULL Terminated

    wszUnicodeDest[uiStrDestIndex] = (WCHAR)'\0';

    RIL_LOG_INFO("ASCIIHexToUnicode - Decoded String: %s.\r\n", wszUnicodeDest);


Error:
    return hr;
}


RIL_RESULT_CODE UnicodeHexToUnicode(const char * szUnicodeHexSrc,
                            UINT32   uiUnicodeHexSrcLength,
                            WCHAR* wszUnicodeDest,
                            UINT32   uiUnicodeDestLength)
{
    RIL_RESULT_CODE hr = RRIL_E_NO_ERROR;
    UINT32   uiNumCharsProcessed = 0;
    UINT32   uiStrDestIndex      = 0;
    const char * szSrcWalk = szUnicodeHexSrc;

    if ((NULL == szUnicodeHexSrc) || (NULL == wszUnicodeDest))
    {
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    if ((uiUnicodeHexSrcLength % 4) != 0)
    {
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    // 4 Encoded Source chars per Destination char

    if (((uiUnicodeHexSrcLength / 4) + 1) > uiUnicodeDestLength)
    {
        hr = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    // Do we have a NULL String?

    if ((0 == uiUnicodeHexSrcLength) || ('\0' == szUnicodeHexSrc[0]))
    {
        wszUnicodeDest[0] = (WCHAR)'\0';
        goto Error;
    }

    // Traverse the Encoded UCS2 Source String, converting it to Unicode in the process
    while ((uiNumCharsProcessed < uiUnicodeHexSrcLength) && (uiStrDestIndex < uiUnicodeDestLength))
    {
        if (sscanf(szSrcWalk, "%4X", (unsigned int*)&(wszUnicodeDest[uiStrDestIndex])) <= 0)
        {
            hr = RRIL_E_UNKNOWN_ERROR;
            goto Error;
        }

        szSrcWalk += 4;

        uiStrDestIndex++;
        uiNumCharsProcessed += 4;
    }

    // Ensure that the Destination String is NULL Terminated

    wszUnicodeDest[uiStrDestIndex] = (WCHAR)'\0';

Error:
    return hr;
}

// Convert a string from Unicode to GSM
BOOL ConvertUnicodeStringToGSMString(const WCHAR* wsIn,
                                     const UINT32 cbIn,
                                     char* sOut,
                                     const UINT32 cbOut,
                                     UINT32& rcbUsed)
{
    UINT32 uiLimit = cbIn > cbOut ? cbOut : cbIn;
    for (UINT32 uiCount = 0; uiCount < uiLimit; ++uiCount)
    {
        if (!ConvertUnicodeCharToGSMDefaultChar(wsIn[uiCount], sOut[uiCount]))
        {
            sOut[uiCount] = '?';
        }
    }
    rcbUsed = uiLimit;
    return TRUE;
}

// Convert a string from GSM to Unicode
BOOL ConvertGSMStringToUnicodeString(const char* sIn,
                                     const UINT32 cbIn,
                                     WCHAR* wsOut,
                                     const UINT32 cbOut,
                                     UINT32& rcbUsed)
{
    UINT32 uiLimit = cbIn > cbOut ? cbOut : cbIn;
    for (UINT32 uiCount = 0; uiCount < uiLimit; ++uiCount)
    {
        if (!ConvertGSMDefaultCharToUnicodeChar(sIn[uiCount], wsOut[uiCount]))
        {
            wsOut[uiCount] = (WCHAR) '?';
        }
    }

    rcbUsed = uiLimit;
    return TRUE;
}

BOOL ConvertHexDigitToHexChar(const char cbIn, char& rbOut)
{
    BOOL fRet = FALSE;

    // the g++ compiler thinks that char is unsigned and gives a warning that
    // (0 <= cbIn) is always true.
    //if ((0 <= cbIn) && (9 >= cbIn))
    if (((0 == cbIn) ||( 0 < cbIn)) && (9 >= cbIn))
    {
        rbOut = '0' + cbIn;
        fRet = TRUE;
    }
    else if ((0x0A <= cbIn) && (0x0F >= cbIn))
    {
        rbOut = 'A' + cbIn - 0x0A;
        fRet = TRUE;
    }
    return fRet;
}


//
//
//  Converts an encoded quoted string to a char buffer.
//  (eg. "003300300032003300370030" -> 302720)
BOOL ConvertQuotedUnicodeHexStringToGSM(const BYTE* szData,
                                        BYTE*       szOut,
                                        const UINT32 cchOut,
                                        const BYTE*& rszPointer)
{
    BOOL fRet = TRUE;
    const BYTE* pszInTmp = szData;
    WCHAR* wszTempBuf = NULL;
    UINT32 cchUsed = 0;
    UINT32 nInLen = 0;

    // Skip over the leading quote
    char c = *pszInTmp++;
    if (c != '\"')
    {
        return FALSE;
    }

    // Calculate string length
    const BYTE* pszInEnd = strchr(pszInTmp, '\"');
    if (!pszInEnd)
    {
        return FALSE;
    }

    UINT32 cbLen = (UINT32)(pszInEnd - szData);
    wszTempBuf = new WCHAR[cbLen];
    if (!wszTempBuf)
    {
        fRet = FALSE;
        goto Error;
    }
    memset(wszTempBuf, 0, sizeof(WCHAR)*cbLen);

    if (!ExtractUnquotedUnicodeHexStringToUnicode(pszInTmp, cbLen, wszTempBuf, cbLen))
    {
        fRet = FALSE;
        goto Error;
    }

    //  We now have the decoded string in a WCHAR buffer, convert unicode to GSM.
    nInLen = wcslen(wszTempBuf);
    if (nInLen > cbLen)
    {
        nInLen = cbLen;
    }
    if (!ConvertUnicodeStringToGSMString(wszTempBuf, nInLen, szOut, cchOut, cchUsed))
    {
        fRet = FALSE;
        goto Error;
    }

    rszPointer = pszInEnd+1;

Error:
    delete[] wszTempBuf;
    wszTempBuf = NULL;
    return fRet;
}

BOOL AppendGSMStringToUnQuotedGSMHexString(const char* pszIn, char*& pszOut, const char* pszOutEnd)
{
    BOOL fRet = FALSE;
    char* pszOutTmp = pszOut;
    UINT32 nLen = 0;

    if (pszOutTmp >= (BYTE*) pszOutEnd)
    {
        goto Error;
    }

    // convert characters
    nLen = strlen(pszIn);
    for (UINT32 i = 0; (i < nLen) && ((pszOutTmp + 4) <= (BYTE*) pszOutEnd); ++i)
    {
        WCHAR wc;
        if (!ConvertGSMDefaultCharToUnicodeChar(pszIn[i], wc))
        {
            goto Error;
        }
        for (UINT32 j = 0; j < 4; ++j)
        {
            // get the 4 most significant bits, the next 4 bits and so on
            char val = (wc >> (12 - j * 4)) & 0x0f;
            if (!ConvertHexDigitToHexChar(val, *pszOutTmp++))
            {
                goto Error;
            }
        }
    }

    if (pszOutTmp >= (BYTE*) pszOutEnd)
    {
        goto Error;
    }

    *pszOutTmp = '\0';
    pszOut = pszOutTmp;
    fRet = TRUE;

Error:
    return fRet;

}

BOOL AppendGSMStringToQuotedGSMHexString(const char* pszIn, char*& pszOut, const char* pszOutEnd)
{
    BOOL fRet = FALSE;
    char* pszOutTmp = pszOut;

    if (pszOutTmp >= (BYTE*) pszOutEnd)
    {
        goto Error;
    }

    *pszOutTmp++='\"';
    if (pszOutTmp >= (BYTE*) pszOutEnd)
    {
        goto Error;
    }

    // append unquoted string, leaving room for the end quote (hence the -1)
    if (!AppendGSMStringToUnQuotedGSMHexString(pszIn, pszOutTmp, pszOutEnd - 1))
    {
        goto Error;
    }

    *pszOutTmp++='\"';
    if (pszOutTmp >= (BYTE*) pszOutEnd)
    {
        goto Error;
    }

    *pszOutTmp = '\0'; // Put on a NULL just to be safe
    pszOut = pszOutTmp;
    fRet = TRUE;

Error:
    return fRet;
}

BOOL AppendUnicodeStringToUnQuotedGSMHexString(const WCHAR* pwszIn, char*& pszOut, const char* pszOutEnd)
{
    BOOL fRet = FALSE;
    char* pszOutTmp = pszOut;
    UINT32 nLen = 0;

    if (pszOutTmp >= (BYTE*) pszOutEnd)
    {
        goto Error;
    }

    // convert characters
    nLen = wcslen(pwszIn);
    for (UINT32 i = 0; (i < nLen) && ((pszOutTmp + 4) <= (BYTE*) pszOutEnd); ++i)
    {
        for (UINT32 j = 0; j < 4; ++j)
        {
            // get the 4 most significant bits, the next 4 bits and so on
            char val = (pwszIn[i] >> (12 - j * 4)) & 0x0f;
            if (!ConvertHexDigitToHexChar(val, *pszOutTmp++))
            {
                goto Error;
            }
        }
    }

    if (pszOutTmp >= (BYTE*) pszOutEnd)
    {
        goto Error;
    }

    *pszOutTmp = '\0';
    pszOut = pszOutTmp;
    fRet = TRUE;

Error:
    return fRet;

}


BOOL AppendUnicodeStringToQuotedGSMHexString(const WCHAR* pwszIn, char*& pszOut, const char* pszOutEnd)
{
    BOOL fRet = FALSE;
    char* pszOutTmp = pszOut;

    if (pszOutTmp >= (BYTE*) pszOutEnd)
    {
        goto Error;
    }

    *pszOutTmp++='\"';
    if (pszOutTmp >= (BYTE*) pszOutEnd)
    {
        goto Error;
    }

    // append unquoted string, leaving room for the end quote (hence the -1)
    if (!AppendUnicodeStringToUnQuotedGSMHexString(pwszIn, pszOutTmp, pszOutEnd - 1))
    {
        goto Error;
    }

    *pszOutTmp++='\"';
    if (pszOutTmp >= (BYTE*) pszOutEnd)
    {
        goto Error;
    }

    *pszOutTmp = '\0'; // Put on a NULL just to be safe
    pszOut = pszOutTmp;
    fRet = TRUE;

Error:
    return fRet;
}


BOOL ConvertUnicodeToUtf8(const WCHAR* szIn, char* szOut, size_t outLen)
{
    const WCHAR* w;

    // Convert unicode to utf8
    unsigned int len = 0;

    // calculate length of output string
    for (w = szIn; *w; w++)
    {
        if (*w < 0x0080)
            len++;
        else if (*w < 0x0800)
            len += 2;
        else
            len += 3;
    }
    ++len;  // for NULL terminator
    if (len > outLen)
        return FALSE;

    int i = 0;
    for (w = szIn; *w; w++)
    {
        if (*w < 0x0080)
        {
            szOut[i++] = (unsigned char) *w;
        }
        else if (*w < 0x0800)
        {
            szOut[i++] = 0xc0 | ((*w) >> 6);
            szOut[i++] = 0x80 | ((*w) & 0x3f);
        }
        else
        {
            szOut[i++] = 0xe0 | ((*w) >> 12);
            szOut[i++] = 0x80 | (((*w) >> 6 ) & 0x3f);
            szOut[i++] = 0x80 | ((*w) & 0x3f);
        }
    }
    szOut[i] = '\0';

    return TRUE;
}

//
// Table used to convert from GSM default alphabet to Unicode
//
static const WCHAR g_rgwchGSMToUnicode[128] =
{
    L'@',   0x00a3, L'$',   0x00a5, 0x00e8, 0x00e9, 0x00f9, 0x00ec, // 0x00 - 0x07
    0x00f2, 0x00c7, L'\n',  0x00d8, 0x00f8,  L'\r', 0x00c5, 0x00e5, // 0x08 - 0x0f
    0x0394, L'_',   0x03a6, 0x0393, 0x039b, 0x03a9, 0x03a0, 0x03a8, // 0x10 - 0x17
    0x03a3, 0x0398, 0x039e, L' ',   0x00c6, 0x00e6, 0x00df, 0x00c9, // 0x18 - 0x1f
    L' ',   L'!',   L'"',   L'#',   0x00a4, L'%',   L'&',   L'\'',  // 0x20 - 0x27
    L'(',   L')',   L'*',   L'+',   L',',   L'-',   L'.',   L'/',   // 0x28 - 0x2f
    L'0',   L'1',   L'2',   L'3',   L'4',   L'5',   L'6',   L'7',   // 0x30 - 0x37
    L'8',   L'9',   L':',   L';',   L'<',   L'=',   L'>',   L'?',   // 0x38 - 0x3f
    0x00a1, L'A',   L'B',   L'C',   L'D',   L'E',   L'F',   L'G',   // 0x40 - 0x47
    L'H',   L'I',   L'J',   L'K',   L'L',   L'M',   L'N',   L'O',   // 0x48 - 0x4f
    L'P',   L'Q',   L'R',   L'S',   L'T',   L'U',   L'V',   L'W',   // 0x50 - 0x57
    L'X',   L'Y',   L'Z',   0x00c4, 0x00d6, 0x00d1, 0x00dc, 0x00a7, // 0x58 - 0x5f
    0x00bf, L'a',   L'b',   L'c',   L'd',   L'e',   L'f',   L'g',   // 0x60 - 0x67
    L'h',   L'i',   L'j',   L'k',   L'l',   L'm',   L'n',   L'o',   // 0x68 - 0x6f
    L'p',   L'q',   L'r',   L's',   L't',   L'u',   L'v',   L'w',   // 0x70 - 0x77
    L'x',   L'y',   L'z',   0x00e4, 0x00f6, 0x00f1, 0x00fc, 0x00e0, // 0x78 - 0x7f
};


//
// Table used to convert from Unicode to GSM default alphabet
//
static const CHARMAP g_rgcmUnicodeToGSMExceptions[] =
{
    { 0x02, L'$'}, { 0x00, L'@'}, { 0x11, L'_'}, { 0x40, 0x00a1},
    { 0x01, 0x00a3}, { 0x24, 0x00a4}, { 0x03, 0x00a5}, { 0x5f, 0x00a7},
    { 0x60, 0x00bf}, { 0x41, 0x00c0}, { 0x41, 0x00c1}, { 0x41, 0x00c2},
    { 0x41, 0x00c3}, { 0x5b, 0x00c4}, { 0x0e, 0x00c5}, { 0x1c, 0x00c6},
    { 0x09, 0x00c7}, { 0x45, 0x00c8}, { 0x1f, 0x00c9}, { 0x45, 0x00ca},
    { 0x45, 0x00cb}, { 0x49, 0x00cc}, { 0x49, 0x00cd}, { 0x49, 0x00ce},
    { 0x49, 0x00cf}, { 0x5d, 0x00d1}, { 0x4f, 0x00d2}, { 0x4f, 0x00d3},
    { 0x4f, 0x00d4}, { 0x4f, 0x00d5}, { 0x5c, 0x00d6}, { 0x0b, 0x00d8},
    { 0x55, 0x00d9}, { 0x55, 0x00da}, { 0x55, 0x00db}, { 0x5e, 0x00dc},
    { 0x59, 0x00dd}, { 0x1e, 0x00df}, { 0x7f, 0x00e0}, { 0x61, 0x00e1},
    { 0x61, 0x00e2}, { 0x61, 0x00e3}, { 0x7b, 0x00e4}, { 0x0f, 0x00e5},
    { 0x1d, 0x00e6}, { 0x09, 0x00e7}, { 0x04, 0x00e8}, { 0x05, 0x00e9},
    { 0x65, 0x00ea}, { 0x65, 0x00eb}, { 0x07, 0x00ec}, { 0x69, 0x00ed},
    { 0x69, 0x00ee}, { 0x69, 0x00ef}, { 0x7d, 0x00f1}, { 0x08, 0x00f2},
    { 0x6f, 0x00f3}, { 0x6f, 0x00f4}, { 0x6f, 0x00f5}, { 0x7c, 0x00f6},
    { 0x0c, 0x00f8}, { 0x06, 0x00f9}, { 0x75, 0x00fa}, { 0x75, 0x00fb},
    { 0x7e, 0x00fc}, { 0x79, 0x00fd}, { 0x79, 0x00ff}, { 0x13, 0x0393},
    { 0x10, 0x0394}, { 0x19, 0x0398}, { 0x14, 0x039b}, { 0x1a, 0x039e},
    { 0x16, 0x03a0}, { 0x18, 0x03a3}, { 0x12, 0x03a6}, { 0x17, 0x03a8},
    { 0x15, 0x03a9},
};
#define NUM_EXCEPTIONS  (sizeof(g_rgcmUnicodeToGSMExceptions) / sizeof(CHARMAP))


//
// Table used to map semi-byte values to hex characters
//
static const char g_rgchSemiByteToCharMap[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};



//
// Convert a semi-byte into its character representation
//
BYTE SemiByteToChar(const BYTE bByte, const BOOL fHigh)
{
    UINT8 bSemiByte = (fHigh ? (bByte & 0xf0) >> 4 : bByte & 0x0f);

    if (0x10 <= bSemiByte)
    {
        RIL_LOG_CRITICAL("SemiByteToChar() : ERROR : Invalid bByte argument: bByte: 0x%X fHigh: %s\r\n", bByte, fHigh ? "TRUE" : "FALSE");
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
// Comparison routine used for binary search below
//
static int BSCompareChars(const void* pElem1, const void* pElem2)
{
    WCHAR wch1 = ((CHARMAP*)pElem1)->wch;
    WCHAR wch2 = ((CHARMAP*)pElem2)->wch;
    int iRet;

    if (wch1 < wch2)
    {
        iRet = -1;
    }
    else if (wch1 == wch2)
    {
        iRet = 0;
    }
    else
    {
        iRet = 1;
    }
    return iRet;
}


//
// Convert a single Unicode character to GSM default alphabet
//
BOOL UnicodeCharToGSM(const WCHAR wch, char *pchRet)
{
    // Does this char map directly to Unicode?
    if (0x000a == wch ||
        0x000d == wch ||
        (0x0020 <= wch && 0x0023 >= wch) ||
        (0x0025 <= wch && 0x003f >= wch) ||
        (0x0041 <= wch && 0x005a >= wch) ||
        (0x0061 <= wch && 0x007a >= wch))
    {
        *pchRet = (char)(wch & 0x007f);
        return TRUE;
    }
    else
    {
        CHARMAP cmKey = { '\0', wch};
        CHARMAP* pcmFound;

        pcmFound = (CHARMAP*)bsearch(&cmKey, g_rgcmUnicodeToGSMExceptions, NUM_EXCEPTIONS, sizeof(CHARMAP), BSCompareChars);
        if (pcmFound)
        {
            *pchRet = pcmFound->ch;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL GSMCharToUnicode(const char ch, WCHAR *pwchRet)
{
    *pwchRet = g_rgwchGSMToUnicode[(int)ch];
    return TRUE;
}

//
//
//
BOOL GSMHexToGSM(const BYTE* sIn, const UINT32 cbIn, BYTE* sOut, const UINT32 cbOut, UINT32& rcbUsed)
{
    const BYTE* pchIn = sIn;
    const BYTE* pchInEnd = sIn + cbIn;
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
BOOL GSMToGSMHex(const BYTE* sIn, const UINT32 cbIn, BYTE* sOut, const UINT32 cbOut, UINT32& rcbUsed)
{
    const BYTE* pchIn = sIn;
    const BYTE* pchInEnd = sIn + cbIn;
    BYTE* pchOut = sOut;
    BYTE* pchOutEnd = sOut + cbOut;
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

//
// Convert a specified string from Unicode
//
BOOL ConvertFromUnicode(const ENCODING_TYPE enc, const WCHAR* wsIn, const UINT32 cchIn, BYTE* sOut, const UINT32 cbOut,
                        UINT32& rcbUsed)
{
    const WCHAR* pwchInEnd;
    const WCHAR* pwchIn;
    BYTE* pchOutEnd;
    BYTE* pchOut;
    BYTE* sOutTemp = NULL;
    BYTE* pchOutTemp;
    UINT32 cbOutUsed;
    char ch7Bit;
    UINT32 nBits = 0;
    BOOL fRet = FALSE;

    rcbUsed = 0;

    if (ENCODING_GSMDEFAULT == enc)
    {
        pwchInEnd = wsIn + cchIn;
        pchOutEnd = sOut + cbOut;
        pwchIn = wsIn;
        pchOut = sOut;

        while (pwchIn < pwchInEnd && pchOut < pchOutEnd)
        {
            if (!nBits)
            {
                if (!UnicodeCharToGSM(*pwchIn++,pchOut))
                {
                    *pchOut = '?';
                }
                //if (*pchOut >= 0x80)
                //{
                //    RIL_LOG_CRITICAL("ConvertFromUnicode() : ERROR : UnicodeCharToGSM returned invalid char: *pchOut = 0x%X", *pchOut);
                //    goto Error;
                //}
            }
            else
            {
                if (!UnicodeCharToGSM(*pwchIn++,&ch7Bit))
                {
                    ch7Bit = '?';
                }

                //if (ch7Bit >= 0x80)
                //{
                //    RIL_LOG_CRITICAL("ConvertFromUnicode() : ERROR : UnicodeCharToGSM returned invalid char: ch7Bit = 0x%X", ch7Bit);
                //    goto Error;
                //}

                *pchOut++ += ch7Bit << (8 - nBits);
                *pchOut    = ch7Bit >> nBits;
            }
            nBits = (nBits + 1) % 8;
        }

        if (!nBits && pchOut > sOut)
        {
            // We incremented the destination pointer once too many
            pchOut--;
        }

        // Do various tricks to distinguish fill bits from real characters (GSM 03.38, ver. 6.0.1, chap. 6.1.2.3.1)
        // NOTE: <CR> in GSM default alphabet maps directly to Unicode <CR>
        if (7 == cchIn % 8 && !(*pchOut & 0xfe))
        {
            // We have 7 fill 0-bits at the end -- need to replace them with <CR>
            *pchOut &= 0x01;
            *pchOut |= ('\r' << 1);
        }
        else if (!(cchIn % 8) && ('\r' << 1) == *pchOut && pchOut < pchOutEnd - 1)
        {
            // We have no fill bits and the last character in the text is <CR> --
            //    we need to add another <CR>, so that terminating <CR> isn't confused with fill bits
            pchOut++;
            *pchOut = '\r' & 0x7f;
        }

        rcbUsed = pchOut - sOut + 1;
        fRet = TRUE;
    }
    else if (ENCODING_GSMDEFAULT_UNPACKED == enc)
    {
        UINT32 dwCharCount = 0;
        while ((dwCharCount < cchIn) && (dwCharCount < cbOut))
        {
            if (!UnicodeCharToGSM(wsIn[dwCharCount], &(sOut[dwCharCount])))
            {
                sOut[dwCharCount] = '?';
            }
            dwCharCount++;
        }
        rcbUsed = cchIn;
        fRet = TRUE;
    }
    else if ((ENCODING_GSMDEFAULT_HEX == enc) || (ENCODING_GSMDEFAULT_HEX_UNPACKED == enc))
    {
        ENCODING_TYPE etSubEncoding;
        if (ENCODING_GSMDEFAULT_HEX_UNPACKED == enc)
        {
            etSubEncoding = ENCODING_GSMDEFAULT_UNPACKED;
        }
        else
        {
            etSubEncoding = ENCODING_GSMDEFAULT;
        }
        sOutTemp = new char[cbOut / 2 + 1];
        if (!sOutTemp)
        {
            goto Error;
        }
        if (!ConvertFromUnicode(etSubEncoding, wsIn, cchIn, sOutTemp, cbOut / 2 + 1, cbOutUsed))
        {
            goto Error;
        }

        pchOutTemp = sOutTemp;
        pchOut = sOut;
        pchOutEnd = sOut + cbOut;

        while ((pchOut <= pchOutEnd) && ((cbOutUsed--) > 0))
        {
            *pchOut++ = SemiByteToChar(*pchOutTemp, TRUE);
            *pchOut++ = SemiByteToChar(*pchOutTemp++, FALSE);
        }
        fRet = TRUE;
        rcbUsed = pchOut - sOut;
    }
    else if (ENCODING_GSMDEFAULT_UNICODE == enc)
    {
        pwchIn = wsIn;
        pwchInEnd = wsIn + cchIn;

        pchOut = sOut;
        pchOutEnd = sOut + cbOut;

        while (pwchIn < pwchInEnd && (pchOut+3) < pchOutEnd)
        {
            pchOut[0] = g_rgchSemiByteToCharMap[ (int)(( *pwchIn >> 12 ) & 0x0f ) ];
            pchOut[1] = g_rgchSemiByteToCharMap[ (int)(( *pwchIn >> 8 ) & 0x0f ) ];
            pchOut[2] = g_rgchSemiByteToCharMap[ (int)(( *pwchIn >> 4 ) & 0x0f ) ];
            pchOut[3] = g_rgchSemiByteToCharMap[ (int)( *pwchIn & 0x0f ) ];

            pwchIn++;
            pchOut += 4;
        }

        rcbUsed = pchOut - sOut;
        fRet = TRUE;
    }
    else if (ENCODING_GSMDEFAULT_UTF8 == enc)
    {
        pwchIn = wsIn;
        pwchInEnd = wsIn + cchIn;

        pchOut = sOut;
        pchOutEnd = sOut + cbOut;

        while (pwchIn < pwchInEnd)
        {
            WCHAR wch = *pwchIn++;

            if (0 == (wch & 0xFF80))
            {   // 7 bits
                if (pchOut + 1 > pchOutEnd)
                    goto Error;
                *pchOut++ = (char) wch;
            }
            else if (0 == (wch & 0xF800))
            {   // 11 bits
                if (pchOut + 2 > pchOutEnd)
                    goto Error;
                *pchOut++ = (char) (0xC0 | ((wch >> 6) & 0x1F));
                *pchOut++ = (char) (0x80 | (wch & 0x3F));
            }
            else
            {   // 16 bits
                if (pchOut + 3 > pchOutEnd)
                    goto Error;
                *pchOut++ = (char) (0xE0 | ((wch >> 12) & 0x0F));
                *pchOut++ = (char) (0x80 | ((wch >> 6) & 0x3F));
                *pchOut++ = (char) (0x80 | (wch & 0x3F));
            }
        }

        rcbUsed = pchOut - sOut;
        fRet = TRUE;
    }

    Error:
    delete[] sOutTemp;
    sOutTemp = NULL;
    return fRet;
}


//
// Convert a specified string to Unicode
//
BOOL ConvertToUnicode(const ENCODING_TYPE enc, const BYTE* sIn, const UINT32 cbIn, WCHAR* wsOut, const UINT32 cchOut,
                      UINT32& rcchUsed)
{
    const BYTE* pchInEnd;
    const BYTE* pchIn;
     WCHAR* pwchOutEnd;
     WCHAR* pwchOut;
    BYTE* sInTemp = NULL;
    UINT32 cbUsedTemp;
    char ch7Bit;
    UINT32 nBits;
    BYTE bValue;
    BOOL fRet = FALSE;

    rcchUsed = 0;

    if (ENCODING_GSMDEFAULT == enc)
    {
        pchInEnd = sIn + cbIn;
        pwchOutEnd = wsOut + cchOut;
        pchIn = sIn;
        pwchOut = wsOut;
        ch7Bit = 0x00;
        nBits = 0;

        while (pchIn < pchInEnd && pwchOut < pwchOutEnd)
        {
            bValue = (BYTE)*pchIn++;

            ch7Bit += (bValue << nBits) & 0x7f;

            //if (ch7Bit >= 0x80)
            //{
            //    goto Error;
            //}

            *pwchOut++ = g_rgwchGSMToUnicode[(int)ch7Bit];

            ch7Bit = (bValue >> (7 - nBits)) & 0x7f;
            nBits = (nBits + 1) % 7;
            if (!nBits && pwchOut < pwchOutEnd)
            {
                *pwchOut++ = g_rgwchGSMToUnicode[(int)ch7Bit];
                ch7Bit = 0x00;
            }
        }

        rcchUsed = pwchOut - wsOut;
        fRet = TRUE;
    }
    else if (ENCODING_GSMDEFAULT_UNPACKED == enc)
    {
        UINT32 dwCharCount = 0;
        while ((dwCharCount < cbIn) && (dwCharCount < cchOut))
        {
            wsOut[dwCharCount] = g_rgwchGSMToUnicode[(int)(sIn[dwCharCount])];
            dwCharCount++;
        }
        rcchUsed = dwCharCount;
        fRet = TRUE;
    }
    else if ((ENCODING_GSMDEFAULT_HEX == enc) || (ENCODING_GSMDEFAULT_HEX_UNPACKED == enc))
    {
        ENCODING_TYPE etSubEncoding;
        if (ENCODING_GSMDEFAULT_HEX_UNPACKED == enc)
        {
            etSubEncoding = ENCODING_GSMDEFAULT_UNPACKED;
        }
        else
        {
            etSubEncoding = ENCODING_GSMDEFAULT;
        }
        sInTemp = new char[cbIn / 2 + 1];
        if (!sInTemp)
        {
            goto Error;
        }

        if (!GSMHexToGSM(sIn, cbIn, sInTemp, cbIn / 2 + 1, cbUsedTemp))
        {
            goto Error;
        }
        if (!ConvertToUnicode(etSubEncoding, sInTemp, cbUsedTemp, wsOut, cchOut, rcchUsed))
        {
            goto Error;
        }
        fRet = TRUE;
    }
    else if (ENCODING_GSMDEFAULT_UNICODE == enc)
    {
        pchIn = sIn;
        pchInEnd = sIn + cbIn;

        pwchOut = wsOut;
        pwchOutEnd = wsOut + cchOut;

        while ((pchIn+3) < pchInEnd && pwchOut < pwchOutEnd)
        {
            int i1 = SemiByteCharsToByte( pchIn[0], pchIn[1] );
            int i0 = SemiByteCharsToByte( pchIn[2], pchIn[3] );
            *pwchOut = (WCHAR)( ( i1 << 8 ) | i0 );

            pchIn += 4;
            pwchOut++;
        }

        rcchUsed = pwchOut - wsOut;
        fRet = TRUE;
    }
    else if (ENCODING_GSMDEFAULT_UTF8 == enc)
    {
        pchIn = sIn;
        pchInEnd = sIn + cbIn;

        pwchOut = wsOut;
        pwchOutEnd = wsOut + cchOut;

        while ((pchIn < pchInEnd) && (pwchOut < pwchOutEnd))
        {
            BYTE ch = *pchIn++;
            WCHAR wch;

            if (0 == (ch & 0x80))
            {   // 7 bits, 1 byte
                wch = (WCHAR) ch;
            }
            else if (0xC0 == (ch & 0xE0))
            {   // 11 bits, 2 bytes
                if (pchIn + 1 > pchInEnd)
                    goto Error;
               BYTE ch2 = *pchIn++;
               if (0x80 != (ch2 & 0xC0))
                   goto Error;
                wch = ((ch & 0x1F) << 6) | (ch2 & 0x3F);
            }
            else if (0xE0 == (ch & 0xF0))
            {   // 16 bits, 3 bytes
                if (pchIn + 2 > pchInEnd)
                    goto Error;
                BYTE ch2 = *pchIn++;
                BYTE ch3 = *pchIn++;
                if (0x80 != (ch2 & 0xC0) || 0x80 != (ch3 & 0xC0))
                    goto Error;
                wch = ((ch & 0x0F) << 12) | ((ch2 & 0x3F) << 6) | (ch3 & 0x3F);
           }
           else
                goto Error;

           *pwchOut++ = wch;
        }

        rcchUsed = pwchOut - wsOut;
        fRet = TRUE;
    }

    Error:
    delete[] sInTemp;
    sInTemp = NULL;
    return fRet;
}



namespace
{
///////////////////////////////////////////////////////////////////////////////
bool CopyString(
  char const* source,
  char* dest,
  size_t destSize)
{
    if (NULL == source)
        return false;

    if (NULL == dest)
        return false;

    size_t const COUNT_OF_CHARS_TO_COPY(
        minimum_of(destSize - 1, strlen(source)));

    strncpy(dest, source, COUNT_OF_CHARS_TO_COPY);
    dest[COUNT_OF_CHARS_TO_COPY] = '\0';

    return true;
}
}

#ifdef GPRS_CONTEXT_CACHING

char g_szGPRSContextCmdCache[MAX_BUFFER_SIZE];

void UpdateGPRSContextCommandCache( BYTE* szCmd )
{
    CopyStringNullTerminate(g_szGPRSContextCmdCache, szCmd, ARRAY_LENGTH(g_szGPRSContextCmdCache));
    return;
}

void ClearGPRSContextCommandCache()
{
    g_szGPRSContextCmdCache[0] = '\0';
}

bool IsGPRSContextCommandCached(BYTE* szCmd )
{
    return (0 == strncmp(szCmd, g_szGPRSContextCmdCache, ARRAY_LENGTH(g_szGPRSContextCmdCache)));
}

#endif // GPRS_CONTEXT_CACHING

//
// Creates a cmd string that includes a byte array (in HEX representation)
// NOTE: the caller is responsible for releasing memory allocated for rszCmd
//
BOOL ComposeCmdWithByteArray(const BYTE* szPrefix, const BYTE* const pbBytes, const UINT32 cbBytes,
                             const BYTE* szPostfix, BYTE*& rszCmd)
{
    //RIL_LOG_VERBOSE("ComposeCmdWithByteArray() - Enter\r\n");
    UINT32 i;
    UINT32 cbCmd;
    BYTE* szWalk;
    BYTE* pbBytesWalk;
    BOOL fRet = FALSE;

    if (NULL == szPrefix)
    {
        RIL_LOG_CRITICAL("ComposeCmdWithByteArray() : ERROR : szPrefix was NULL\r\n");
        goto Error;
    }

    if (NULL == szPostfix)
    {
        RIL_LOG_CRITICAL("ComposeCmdWithByteArray() : ERROR : szPostfix was NULL\r\n");
        goto Error;
    }

    rszCmd = 0;

    // Allocate the command string
    cbCmd = MAX_BUFFER_SIZE + strlen(szPrefix) + strlen(szPostfix) + cbBytes * 2;
    rszCmd = new char[cbCmd];
    if (!rszCmd)
    {
        goto Error;
    }
    szWalk = rszCmd;

    // Add "<prefix>"
    (void)CopyStringNullTerminate(szWalk, szPrefix, cbCmd - (szWalk - rszCmd));
    szWalk = strchr(szWalk, '\0');  // NO_TYPO: 27
    if (NULL == szWalk)
    {
        goto Error;
    }

    // Add the byte array coverted to HEX representation, if provided
    if (cbBytes)
    {
        // Quotes are required by GSM spec
        *szWalk++ = '\"';

        pbBytesWalk = (BYTE*)pbBytes;
        for (i = 0; i < cbBytes; i++)
        {
            if (cbCmd < (UINT32)(szWalk - rszCmd + 2))
            {
                RIL_LOG_CRITICAL("ComposeCmdWithByteArray() : ERROR : cbCmd smaller than required size\r\n");
                goto Error;
            }

            *szWalk++ = SemiByteToChar(*pbBytesWalk,   TRUE);
            *szWalk++ = SemiByteToChar(*pbBytesWalk++, FALSE);
        }

        // Quotes are required by GSM spec
        *szWalk++ = '\"';
    }

    // Add "<postfix>"
    (void)CopyStringNullTerminate(szWalk, szPostfix, cbCmd - (szWalk - rszCmd));  // NO_TYPO: 30
    fRet = TRUE;

    Error:
    if (!fRet)
    {
        delete[] rszCmd;
        rszCmd = NULL;
    }

    //RIL_LOG_VERBOSE("ComposeCmdWithByteArray() - Exit\r\n");
    return fRet;
}


//
// Alternate line support
//
// The following function helps us separate the implementation of
// alternate line support without cluttering the original
// source code too much.
//
//TODO:
// When our OEM1 hardware supports %ALS, uncomment the rest of
// the function definition below and remove this comment.
//

#define UNUSED_PARAM(x) x

//
// Fills the command buffer with AT plus any required modifier to specify
// the line on which the command should apply.
//
// Returns a pointer to where the rest of the command can be written
// to the buffer.
//
char * BeginLineSpecificCommand(char *szCmdDst, UINT32 cchSize, UINT32 dwAddressId)
{
    UNUSED_PARAM(dwAddressId);
    (void)CopyStringNullTerminate(szCmdDst, "AT", cchSize);
    return strchr(szCmdDst, '\0');  // NO_TYPO: 27
}




//
// (Re)Allocates storage for data elements
//      prgrData - Pointer to current buffer (buffer must be NULL if (0 == nAllocated))
//        stSize - Size (in bytes) of the data elements in the buffer
//         nUsed - Number of valid data elements in the buffer
//   pnAllocated - Total number of data elements in the buffer
//     nGrowSize - Number of elements by which to increase the size of the buffer
//
BOOL AllocateOrReallocateStorage(BYTE** const prgrData, const size_t stSize, const UINT32 nUsed, UINT32* const pnAllocated, const UINT32 nGrowSize)
{
    //RIL_LOG_VERBOSE("AllocateOrReallocateStorage() - Enter\r\n");
    BOOL fSuccess = FALSE;

    if (NULL == prgrData)
    {
        RIL_LOG_CRITICAL("AllocateOrReallocateStorage() : ERROR : prgrData was NULL\r\n");
        goto Error;
    }

    if (NULL == pnAllocated)
    {
        RIL_LOG_CRITICAL("AllocateOrReallocateStorage() : ERROR : pnAllocated was NULL\r\n");
        goto Error;
    }

    if (NULL == *prgrData && 0 != *pnAllocated)
    {
        RIL_LOG_CRITICAL("AllocateOrReallocateStorage() : ERROR : *prgrData was NULL and *pnAllocated was not 0\r\n");
        goto Error;
    }

    {
        *pnAllocated += nGrowSize;
        const size_t stAllocateBytes = *pnAllocated * stSize;
        BYTE* const prbTemp = (BYTE*)malloc(stAllocateBytes);
        if (prbTemp)
        {
            const size_t stUsedBytes = nUsed * stSize;
            memcpy(prbTemp, *prgrData, stUsedBytes);
            // The following line seems like a good idea, but may waste resources
            // 0-initializing memory that is never used.
            // memset(prbTemp + stUsedBytes, 0x00, stAllocateBytes - stUsedBytes);
            free(*prgrData);
            *prgrData = prbTemp;
            fSuccess = TRUE;
        }
        else
        {
            *pnAllocated -= nGrowSize;
        }
    }

Error:
    //RIL_LOG_VERBOSE("AllocateOrReallocateStorage() - Exit\r\n");
    return fSuccess;
}


//
// Buffer ctor
//
#if 0

CBuffer::CBuffer()
: m_szData(NULL),
m_cbLength(0),
m_cbAlloc(0)
{
}


//
// Buffer dtor
//
CBuffer::~CBuffer()
{
    delete[] m_szData;
    m_szData = NULL;
}

__inline BOOL safeIntUAddThree(unsigned int addend1, unsigned int addend2, unsigned int addend3, unsigned int *pSum)
{
    BOOL ReturnValue = FALSE;

    if ( pSum )
    {
        if ( TRUE == safeIntUAdd(addend1, addend2, pSum))
        {
            if ( TRUE == safeIntUAdd( *pSum, addend3, pSum ) )
            {
                ReturnValue = TRUE;
            }
        }
    }
    return ReturnValue;
}

//
// Append data to the response
//
BOOL CBuffer::Append(const BYTE* szString, const UINT32 cbString)
{
    BYTE* szTemp = NULL;
    BOOL fRet = FALSE;
    UINT32 SafeAdd = 0;

    if (!m_szData)
    {
        m_cbLength = 0;
        m_cbAlloc = ALLOC_SIZE;
        m_szData = new char[m_cbAlloc];
        if (!m_szData)
        {
            goto Error;
        }
        m_szData[ALLOC_SIZE-1]=0;
    }


    if ( FALSE == safeIntUAddThree(m_cbLength, cbString, 1, &SafeAdd))
    {
        goto Error;
    }

    if (SafeAdd > m_cbAlloc)
    {
        do
        {
            if ( FALSE == safeIntUAdd(m_cbAlloc, ALLOC_SIZE, &m_cbAlloc))
            {
                goto Error;
            }
        } while (SafeAdd > m_cbAlloc);

        szTemp = new char[m_cbAlloc];
        if (!szTemp)
        {
            goto Error;
        }
        szTemp[m_cbAlloc-1] = 0;
        memcpy(szTemp, m_szData, m_cbLength);
        delete[] m_szData;
        m_szData = szTemp;
    }

    memcpy(m_szData + m_cbLength, szString, cbString);
    m_cbLength += cbString;
    fRet = TRUE;

    Error:
    return fRet;
}


//
// Get the data out of the buffer
//    (the caller takes ownership of the m_szData storage)
//
BYTE* CBuffer::GiveUpData()
{
    BYTE* szTemp = m_szData;

    m_szData = NULL;
    m_cbLength = 0;
    m_cbAlloc = 0;

    return szTemp;
}


//
//
//
void CBuffer::InheritData(CBuffer* pSrcBuffer)
{
    delete[] m_szData;
    m_szData = NULL;
    m_cbLength = 0;
    m_cbAlloc = 0;

    m_cbAlloc = pSrcBuffer->m_cbAlloc;
    m_cbLength = pSrcBuffer->m_cbLength;
    m_szData = pSrcBuffer->GiveUpData();
}

BOOL GetMMIServiceCodeFromInfoClass(UINT32 dwInfoClass, const char * szMMICode)
{
    BOOL fRet = TRUE;

    switch(dwInfoClass)
    {
        // All Teleservices
        case RRIL_INFO_CLASS_VOICE | RRIL_INFO_CLASS_FAX | RRIL_INFO_CLASS_DATA:
            szMMICode = "10";
            break;

        // Telephony
        case RRIL_INFO_CLASS_VOICE:
            szMMICode = "11";
            break;

        // All data teleservices
        case RRIL_INFO_CLASS_FAX | RRIL_INFO_CLASS_SMS:
            szMMICode = "12";
            break;

        // Facsimile services
        case RRIL_INFO_CLASS_FAX:
            szMMICode = "13";
            break;

        // Short Message Services
        case RRIL_INFO_CLASS_SMS:
            szMMICode = "16";
            break;

        // All teleservices except SMS
        case RRIL_INFO_CLASS_VOICE | RRIL_INFO_CLASS_FAX:
            szMMICode = "19";
            break;

        // All bearer services
        case RRIL_INFO_CLASS_DATA:
            szMMICode = "20";
            break;

        // All async services
        case RRIL_INFO_CLASS_DATA_CIRCUIT_ASYNC | RRIL_INFO_CLASS_DEDICATED_PAD_ACCESS:
            szMMICode = "21";
            break;

        // All sync services
        case RRIL_INFO_CLASS_DATA_CIRCUIT_SYNC | RRIL_INFO_CLASS_DEDICATED_PACKET_ACCESS:
            szMMICode = "22";
            break;

        // All data circuit sync
        case RRIL_INFO_CLASS_DATA_CIRCUIT_SYNC:
            szMMICode = "24";
            break;

        // All data circuit async
        case RRIL_INFO_CLASS_DATA_CIRCUIT_ASYNC:
            szMMICode = "25";
            break;

        default:
            fRet = FALSE;
            break;
    }

    return fRet;
}
#endif

CSelfExpandBuffer::CSelfExpandBuffer() : m_szBuffer(NULL), m_uiUsed(0), m_nCapacity(0)
{
}

CSelfExpandBuffer::~CSelfExpandBuffer()
{
    delete[] m_szBuffer;
    m_szBuffer = NULL;
}

BOOL CSelfExpandBuffer::Append(const BYTE *szIn, UINT32 nLength)
{
    BOOL   bRet = FALSE;
    UINT32 nNewSize;

    if (NULL != szIn && nLength != 0)
    {
        if (NULL == m_szBuffer)
        {
            m_szBuffer = new BYTE[m_nChunkSize];
            if (NULL == m_szBuffer)
                goto Error;

            m_nCapacity = m_nChunkSize;
        }

        for (nNewSize = m_nCapacity; (nNewSize - m_uiUsed) <= nLength; nNewSize += m_nChunkSize);
        if (nNewSize != m_nCapacity)
        {
            // allocate more space for the data
            BYTE* tmp = new BYTE[nNewSize];
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

    if ((NULL != pszIn) || (NULL != pszOut))
    {
        UINT32 cbIn = strlen(pszIn);

        strncpy(pszOut, pszIn, cbOut);

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

    if (NULL == pszIn)
    {
        RIL_LOG_CRITICAL("CRLFExpandedString::CRLFExpandedString() : ERROR : pszIn was NULL\r\n");
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
    m_pszString = new char[nInLen + (nCRLFs * 3) + (nOther * 3) + 1];
    memset(m_pszString, 0, nInLen + (nCRLFs * 3) + (nOther * 3) + 1);

    for (int nWalk = 0; nWalk < nInLen; nWalk++)
    {
        if (0x0A == pszIn[nWalk])
        {
            strcat(m_pszString, "<lf>");
        }
        else if (0x0D == pszIn[nWalk])
        {
            strcat(m_pszString, "<cr>");
        }
        else if ((pszIn[nWalk] >= 0x20) && (pszIn[nWalk] <= 0x7E))
        {
            strncat(m_pszString, &pszIn[nWalk], 1);
        }
        else
        {
            char szTmp[5] = {0};
            snprintf(szTmp, 5, "[%02X]", pszIn[nWalk]);
            strcat(m_pszString, szTmp);
        }
    }
}

CRLFExpandedString::~CRLFExpandedString()
{
    //RIL_LOG_INFO("CRLFExpandedString::~CRLFExpandedString()\r\n");
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
