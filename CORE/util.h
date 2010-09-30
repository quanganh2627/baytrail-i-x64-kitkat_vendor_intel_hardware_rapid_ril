////////////////////////////////////////////////////////////////////////////
// util.h
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides function prototypes for the RIL Utility functions.
//    Also includes related class, constant, and structure definitions.
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

#pragma once

#include "rillog.h"
#include "rril.h"
#include "sync_ops.h"

typedef struct _UnicodeCharMap
{
    unsigned char  cGSMDefault;
    WCHAR          wcUnicode;
} S_UnicodeCharMap;

const S_UnicodeCharMap g_CharMap_Unicode_vs_GSMDefault[] =
{
//   GSM
// Default Unicode
    {0x00, 0x0040},  // COMMERCIAL AT
    {0x01, 0x00A3},  // POUND SIGN
    {0x02, 0x0024},  // DOLLAR SIGN
    {0x03, 0x00A5},  // YEN SIGN
    {0x04, 0x00E8},  // LATIN SMALL LETTER E WITH GRAVE
    {0x05, 0x00E9},  // LATIN SMALL LETTER E WITH ACUTE
    {0x06, 0x00F9},  // LATIN SMALL LETTER U WITH GRAVE
    {0x07, 0x00EC},  // LATIN SMALL LETTER I WITH GRAVE
    {0x08, 0x00F2},  // LATIN SMALL LETTER O WITH GRAVE
    {0x09, 0x00E7},  // LATIN SMALL LETTER C WITH CEDILLA
    {0x0A, 0x000A},  // LINE FEED
    {0x0B, 0x00D8},  // LATIN CAPITAL LETTER O WITH STROKE
    {0x0C, 0x00F8},  // LATIN SMALL LETTER O WITH STROKE
    {0x0D, 0x000D},  // CARRIAGE RETURN
    {0x0E, 0x00C5},  // LATIN CAPITAL LETTER A WITH RING ABOVE
    {0x0F, 0x00E5},  // LATIN SMALL LETTER A WITH RING ABOVE
    {0x10, 0x0394},  // GREEK CAPITAL LETTER DELTA
    {0x11, 0x005F},  // LOW LINE
    {0x12, 0x03A6},  // GREEK CAPITAL LETTER PHI
    {0x13, 0x0393},  // GREEK CAPITAL LETTER GAMMA
    {0x14, 0x039B},  // GREEK CAPITAL LETTER LAMBDA
    {0x15, 0x03A9},  // GREEK CAPITAL LETTER OMEGA
    {0x16, 0x03A0},  // GREEK CAPITAL LETTER PI
    {0x17, 0x03A8},  // GREEK CAPITAL LETTER PSI
    {0x18, 0x03A3},  // GREEK CAPITAL LETTER SIGMA
    {0x19, 0x0398},  // GREEK CAPITAL LETTER THETA
    {0x1A, 0x039E},  // GREEK CAPITAL LETTER XI
    {0x1B, 0x00A0},  // ESCAPE TO EXTENSION TABLE
    {0x1C, 0x00C6},  // LATIN CAPITAL LETTER AE
    {0x1D, 0x00E6},  // LATIN SMALL LETTER AE
    {0x1E, 0x00DF},  // LATIN SMALL LETTER SHARP S (German)
    {0x1F, 0x00C9},  // LATIN CAPITAL LETTER E WITH ACUTE
    {0x20, 0x0020},  // SPACE
    {0x21, 0x0021},  // EXCLAMATION MARK
    {0x22, 0x0022},  // QUOTATION MARK
    {0x23, 0x0023},  // NUMBER SIGN
    {0x24, 0x00A4},  // CURRENCY SIGN
    {0x25, 0x0025},  // PERCENT SIGN
    {0x26, 0x0026},  // AMPERSAND
    {0x27, 0x0027},  // APOSTROPHE
    {0x28, 0x0028},  // LEFT PARENTHESIS
    {0x29, 0x0029},  // RIGHT PARENTHESIS
    {0x2A, 0x002A},  // ASTERISK
    {0x2B, 0x002B},  // PLUS SIGN
    {0x2C, 0x002C},  // COMMA
    {0x2D, 0x002D},  // HYPHEN-MINUS
    {0x2E, 0x002E},  // FULL STOP
    {0x2F, 0x002F},  // SOLIDUS (SLASH)
    {0x30, 0x0030},  // DIGIT ZERO
    {0x31, 0x0031},  // DIGIT ONE
    {0x32, 0x0032},  // DIGIT TWO
    {0x33, 0x0033},  // DIGIT THREE
    {0x34, 0x0034},  // DIGIT FOUR
    {0x35, 0x0035},  // DIGIT FIVE
    {0x36, 0x0036},  // DIGIT SIX
    {0x37, 0x0037},  // DIGIT SEVEN
    {0x38, 0x0038},  // DIGIT EIGHT
    {0x39, 0x0039},  // DIGIT NINE
    {0x3A, 0x003A},  // COLON
    {0x3B, 0x003B},  // SEMICOLON
    {0x3C, 0x003C},  // LESS-THAN SIGN
    {0x3D, 0x003D},  // EQUALS SIGN
    {0x3E, 0x003E},  // GREATER-THAN SIGN
    {0x3F, 0x003F},  // QUESTION MARK
    {0x40, 0x00A1},  // INVERTED EXCLAMATION MARK
    {0x41, 0x0041},  // LATIN CAPITAL LETTER A
    {0x42, 0x0042},  // LATIN CAPITAL LETTER B
    {0x43, 0x0043},  // LATIN CAPITAL LETTER C
    {0x44, 0x0044},  // LATIN CAPITAL LETTER D
    {0x45, 0x0045},  // LATIN CAPITAL LETTER E
    {0x46, 0x0046},  // LATIN CAPITAL LETTER F
    {0x47, 0x0047},  // LATIN CAPITAL LETTER G
    {0x48, 0x0048},  // LATIN CAPITAL LETTER H
    {0x49, 0x0049},  // LATIN CAPITAL LETTER I
    {0x4A, 0x004A},  // LATIN CAPITAL LETTER J
    {0x4B, 0x004B},  // LATIN CAPITAL LETTER K
    {0x4C, 0x004C},  // LATIN CAPITAL LETTER L
    {0x4D, 0x004D},  // LATIN CAPITAL LETTER M
    {0x4E, 0x004E},  // LATIN CAPITAL LETTER N
    {0x4F, 0x004F},  // LATIN CAPITAL LETTER O
    {0x50, 0x0050},  // LATIN CAPITAL LETTER P
    {0x51, 0x0051},  // LATIN CAPITAL LETTER Q
    {0x52, 0x0052},  // LATIN CAPITAL LETTER R
    {0x53, 0x0053},  // LATIN CAPITAL LETTER S
    {0x54, 0x0054},  // LATIN CAPITAL LETTER T
    {0x55, 0x0055},  // LATIN CAPITAL LETTER U
    {0x56, 0x0056},  // LATIN CAPITAL LETTER V
    {0x57, 0x0057},  // LATIN CAPITAL LETTER W
    {0x58, 0x0058},  // LATIN CAPITAL LETTER X
    {0x59, 0x0059},  // LATIN CAPITAL LETTER Y
    {0x5A, 0x005A},  // LATIN CAPITAL LETTER Z
    {0x5B, 0x00C4},  // LATIN CAPITAL LETTER A WITH DIAERESIS
    {0x5C, 0x00D6},  // LATIN CAPITAL LETTER O WITH DIAERESIS
    {0x5D, 0x00D1},  // LATIN CAPITAL LETTER N WITH TILDE
    {0x5E, 0x00DC},  // LATIN CAPITAL LETTER U WITH DIAERESIS
    {0x5F, 0x00A7},  // SECTION SIGN
    {0x60, 0x00BF},  // INVERTED QUESTION MARK
    {0x61, 0x0061},  // LATIN SMALL LETTER A
    {0x62, 0x0062},  // LATIN SMALL LETTER B
    {0x63, 0x0063},  // LATIN SMALL LETTER C
    {0x64, 0x0064},  // LATIN SMALL LETTER D
    {0x65, 0x0065},  // LATIN SMALL LETTER E
    {0x66, 0x0066},  // LATIN SMALL LETTER F
    {0x67, 0x0067},  // LATIN SMALL LETTER G
    {0x68, 0x0068},  // LATIN SMALL LETTER H
    {0x69, 0x0069},  // LATIN SMALL LETTER I
    {0x6A, 0x006A},  // LATIN SMALL LETTER J
    {0x6B, 0x006B},  // LATIN SMALL LETTER K
    {0x6C, 0x006C},  // LATIN SMALL LETTER L
    {0x6D, 0x006D},  // LATIN SMALL LETTER M
    {0x6E, 0x006E},  // LATIN SMALL LETTER N
    {0x6F, 0x006F},  // LATIN SMALL LETTER O
    {0x70, 0x0070},  // LATIN SMALL LETTER P
    {0x71, 0x0071},  // LATIN SMALL LETTER Q
    {0x72, 0x0072},  // LATIN SMALL LETTER R
    {0x73, 0x0073},  // LATIN SMALL LETTER S
    {0x74, 0x0074},  // LATIN SMALL LETTER T
    {0x75, 0x0075},  // LATIN SMALL LETTER U
    {0x76, 0x0076},  // LATIN SMALL LETTER V
    {0x77, 0x0077},  // LATIN SMALL LETTER W
    {0x78, 0x0078},  // LATIN SMALL LETTER X
    {0x79, 0x0079},  // LATIN SMALL LETTER Y
    {0x7A, 0x007A},  // LATIN SMALL LETTER Z
    {0x7B, 0x00E4},  // LATIN SMALL LETTER A WITH DIAERESIS
    {0x7C, 0x00F6},  // LATIN SMALL LETTER O WITH DIAERESIS
    {0x7D, 0x00F1},  // LATIN SMALL LETTER N WITH TILDE
    {0x7E, 0x00FC},  // LATIN SMALL LETTER U WITH DIAERESIS
    {0x7F, 0x00E0},  // LATIN SMALL LETTER A WITH GRAVE
    {0xFF, 0xFFFF}   // CharMap Terminator
};

RIL_RESULT_CODE UnicodeToGSMDefaultHex(WCHAR * wszUnicodeSrc,
                               UINT32    uiUnicodeSrcLength,
                               char *  szGSMDefaultDest,
                               UINT32    uiGSMDefaultDestLength);

RIL_RESULT_CODE UnicodeToGSMDefaultPackedHex(WCHAR * wszUnicodeSrc,
                                     UINT32    uiUnicodeSrcLength,
                                     char *  szGSMDefaultPackedDest,
                                     UINT32    uiGSMDefaultPackedDestLength);

RIL_RESULT_CODE UnicodeToASCIIHex(WCHAR * wszUnicodeSrc,
                          UINT32    uiUnicodeSrcLength,
                          char *  szASCIIHexDest,
                          UINT32    uiASCIIHexDestLength);

RIL_RESULT_CODE UnicodeToUnicodeHex(WCHAR * wszUnicodeSrc,
                            UINT32    uiUnicodeSrcLength,
                            char *  szUnicodeHexDest,
                            UINT32    uiUnicodeHexDestLength);

RIL_RESULT_CODE GSMDefaultPackedHexToUnicode(char  * szGSMDefaultPackedSrc,
                                     UINT32    uiGSMDefaultPackedSrcLength,
                                     WCHAR * wszUnicodeDest,
                                     UINT32    uiUnicodeDestLength);

RIL_RESULT_CODE GSMDefaultHexToUnicode(const char * szGSMDefaultSrc,
                               UINT32   uiGSMDefaultSrcLength,
                               WCHAR* wszUnicodeDest,
                               UINT32   uiUnicodeDestLength);

RIL_RESULT_CODE ASCIIHexToUnicode(char * szASCIIHexSrc,
                          UINT32   uiASCIIHexSrcLength,
                          WCHAR* wszUnicodeDest,
                          UINT32   uiUnicodeDestLength);

RIL_RESULT_CODE UnicodeHexToUnicode(const char * szUnicodeHexSrc,
                            UINT32   uiUnicodeHexSrcLength,
                            WCHAR* wszUnicodeDest,
                            UINT32   uiUnicodeDestLength);

BOOL ConvertUnicodeStringToGSMString(const WCHAR* wsIn,
                                     const UINT32 cbIn,
                                     char* sOut,
                                     UINT32 cbOut,
                                     UINT32& rcbUsed);

BOOL ConvertHexDigitToHexChar(const char cbIn,
                              char& rbOut);

RIL_RESULT_CODE ConvertGSMDefaultCharToUnicodeChar(const char cGSMDefault,
                                           WCHAR &wcUnicode);

BOOL ConvertGSMStringToUnicodeString(const char* sIn,
                                     const UINT32 cbIn,
                                     WCHAR* wsOut,
                                     const UINT32 cbOut,
                                     UINT32& rcbUsed);

BOOL ConvertQuotedUnicodeHexStringToGSM(const BYTE* szData,
                                        BYTE* szOut,
                                        const UINT32 cchOut,
                                        const BYTE*& rszPointer);

BOOL AppendGSMStringToUnQuotedGSMHexString(const char* pszIn,
                                           char*& pszOut,
                                           const char* pszOutEnd);

BOOL AppendGSMStringToQuotedGSMHexString(const char* pszIn,
                                         char*& pszOut,
                                         const char* pszOutEnd);

BOOL AppendUnicodeStringToUnQuotedGSMHexString(const WCHAR* pwszIn,
                                               char*& pszOut,
                                               const char* pszOutEnd);

BOOL AppendUnicodeStringToQuotedGSMHexString(const WCHAR* pwszIn,
                                             char*& pszOut,
                                             const char* pszOutEnd);

BOOL ConvertUnicodeToUtf8(const WCHAR* szIn,
                          char* szOut,
                          size_t outLen);

// Copies as much of input string into output as possible and *ALWAYS* appends a NULL
// character at the end. If the output buffer cannot hold all of the input string, a
// NULL will be placed in the last byte of the output buffer. The BOOL return FALSE
// indicates that the output does not contain the entire input string. cbOut is the size
// of the output buffer in bytes.
BOOL CopyStringNullTerminate(char * const pszOut, const char * pszIn, const UINT32 cbOut);

// Prints out the given arguments as per pszFormat into the output buffer. If the resulting
// string is too large, it will be truncated and NULL terminated. The return will be false
// for truncation and any error condition. If an error occurs, pszOut will contain a zero
// length NULL terminated string.
BOOL PrintStringNullTerminate(char * const pszOut, const UINT32 cbOut, const char * pszFormat, ... );

// Adds pszIn to the end of pszOut overwritting its NULL terminating character. Adds a NULL
// character at the end of the string regardless of truncation. If truncation or some other
// error occurs, the function will return FALSE.
BOOL ConcatenateStringNullTerminate(char * const pszOut, const UINT32 cbOut, const char * const pszIn);

// Converts a NULL terminated TCHAR string into a char string
// Returns TRUE if the conversion is successful, FALSE otherwise
// iMaxLen indicates the maximum capacity of the buffer for the
// converted string in characters
#if !defined(__linux__)
BOOL ConvertTCharToChar(const TCHAR* szIn, char* szOut, int iMaxLen);
#endif  // __linux__

// Converts a NULL terminated char string into a TCHAR string
// Returns TRUE if the conversion is successful, FALSE otherwise
// iMaxLen indicates the maximum capacity of the buffer for the
// converted string in characters
#if !defined(__linux__)
BOOL ConvertCharToTChar(const char* szIn, TCHAR* szOut, int iMaxLen);
#endif  // __linux__

class CRLFExpandedString
{
public:
    CRLFExpandedString(const char * const pszIn, const int nInLen);
    ~CRLFExpandedString();

    char *& GetString() { return m_pszString; };

private:
    char * m_pszString;
};

//
// Character mapping structure used for binary search
//
struct CHARMAP {
    char ch;
    WCHAR wch;
};

//
// Encoding types
//
enum ENCODING_TYPE {
    ENCODING_GSMDEFAULT = 0,// GSM Default alphabet
    ENCODING_GSMDEFAULT_HEX, // GSM Default alphabet, HEX encoded
    ENCODING_GSMDEFAULT_UNPACKED, // GSM Default alphabet, 7 bit packed see GSM 07.07 5.5
    ENCODING_GSMDEFAULT_HEX_UNPACKED, // GSM Default alphabet, HEX encoded, not 7 bit packed see GSM 07.07 5.5
    ENCODING_GSMDEFAULT_UNICODE, // Unicode alphabet, HEX encoded
    ENCODING_GSMDEFAULT_UTF8, // Unicode alphabet, UTF-8 encoded
    // Others to be added later
};

#define ENCODING_TECHARSET ENCODING_GSMDEFAULT_UNICODE

//
// Function declarations
//
BOOL UnicodeCharToGSM(const WCHAR wch, char *pchRet);
BOOL GSMCharToUnicode(const char ch, WCHAR *pwchRet);
#if !defined(__linux__)
char SemiByteToChar(const BYTE bByte, const BOOL fHigh);
#endif  // !__linux__
BYTE SemiByteCharsToByte(const char chHigh, const char chLow);
BOOL GSMHexToGSM(const BYTE* sIn, const UINT32 cbIn, BYTE* sOut, const UINT32 cbOut, UINT32& rcbUsed);
BOOL GSMToGSMHex(const BYTE* sIn, const UINT32 cbIn, BYTE* sOut, const UINT32 cbOut, UINT32& rcbUsed);
BOOL ConvertFromUnicode(const ENCODING_TYPE enc, const WCHAR* wsIn, const UINT32 cchIn, BYTE* sOut, const UINT32 cbOut,
                        UINT32& rcbUsed);
BOOL ConvertToUnicode(const ENCODING_TYPE enc, const BYTE* sIn, const UINT32 cbIn, WCHAR* wsOut, const UINT32 cchOut,
                      UINT32& rcchUsed);

#if !defined(__linux__)
RIL_RESULT_CODE RILAddressToString(const RRilAddress& rraAddress, const BYTE* szOut,
    const UINT32 cbOut, BYTE& rbTypeOfAddress,
    ENCODING_TYPE etEncoding = ENCODING_GSMDEFAULT_HEX);

BOOL StringToRILAddress(const BYTE* szAddress, const BYTE bTypeOfAddress,
    RRilAddress& rraAddress,
    ENCODING_TYPE etEncoding = ENCODING_GSMDEFAULT_HEX);

RIL_RESULT_CODE RILSubAddressToString(const RRilSubaddress& rrsaSubAddress,
    const BYTE* szOut, const UINT32 cbOut, BYTE& rbType);

BOOL StringToRILSubAddress(const BYTE* szSubAddress, const BYTE bType,
    RRilSubaddress& rrsaSubAddress);

RIL_RESULT_CODE DetermineSimResponseError(UINT32 dwSW1, UINT32 dwSW2);

RIL_RESULT_CODE StringFilterW( WCHAR* wszDest, size_t cchDest,  WCHAR* wszSrc, const WCHAR* wszFilter);
#endif

BOOL IsElementarySimFile(UINT32 dwFileID);
BOOL ComposeCmdWithByteArray(const BYTE* szPrefix, const BYTE* const pbBytes, const UINT32 cbBytes,
                             const BYTE* szPostfix, BYTE*& rszCmd);
char * BeginLineSpecificCommand(char *szCmdDst, UINT32 cchSize, UINT32 dwAddressId);
BOOL AllocateOrReallocateStorage(BYTE** const prgrData, const size_t stSize, const UINT32 nUsed, UINT32* const pnAllocated,
                                 const UINT32 nGrowSize);

#ifdef GPRS_CONTEXT_CACHING
void UpdateGPRSContextCommandCache( BYTE* szCmd );
void ClearGPRSContextCommandCache();
bool IsGPRSContextCommandCached(BYTE* szCmd );
#endif //GPRS_CONTEXT_CACHING

BOOL GetMMIServiceCodeFromInfoClass(UINT32 dwInfoClass, const char * szMMICode);

#ifdef __linux__
void Sleep(UINT32 dwTimeInMS);
UINT32 GetTickCount();
#endif

//
// Class declarations
//

// Growable character buffer
#if 0
class CBuffer
{
public:
            CBuffer();
            ~CBuffer();

    BOOL    Append(const BYTE* szString, const UINT32 cbString);
    BYTE*   GetData() const     { return m_szData; };
    UINT32    GetLength() const   { return m_cbLength; };
    BYTE*   GiveUpData();
    void    InheritData(CBuffer* pSrcBuffer);

protected:
    BYTE*   m_szData;
    UINT32    m_cbLength;
    UINT32    m_cbAlloc;
};
#endif

// Doubly-linked list element
class CListElem
{
public:
                CListElem() : m_pNext(NULL), m_pPrev(NULL) {};
    virtual     ~CListElem() {};

    CListElem*  GetNext() const         { return m_pNext; };
    CListElem*  GetPrev() const         { return m_pPrev; };

    void        SetNext(CListElem* p)   { m_pNext = p; };
    void        SetPrev(CListElem* p)   { m_pPrev = p; };

private:
    CListElem*  m_pNext;
    CListElem*  m_pPrev;
};

// Function to be passed to CQueue::Enum().
//    This function should return TRUE for enumeration to stop.
typedef BOOL (*PFN_QUEUE_ENUM)(void* pItem, UINT32 uiData);

// Function to be passed to CQueue::ConditionalGet().
//    This function should return TRUE for for the item to be removed from the queue.
typedef BOOL (*PFN_QUEUE_TEST)(void* pItem, UINT32 uiData);



#ifndef __linux__
// Callback used when entering exclusive use mode
typedef void (*PFN_SHRDUSE_ACTION)(UINT32 dwParam);

// Shared resource with readers/writers synchronization
class CSharedResource
{
public:
            CSharedResource();
            ~CSharedResource();

    BOOL    Init(CEvent * pCancelEvent);
    BOOL    EnterSharedUse();
    BOOL    ExitSharedUse();
    BOOL    EnterExclusiveUse(PFN_SHRDUSE_ACTION pfnAction, const UINT32 dwParam) const;
    BOOL    ExitExclusiveUse() const;

private:
    BOOL                m_fSharedResourceInited;
    CMutex *            m_pSharedResMutex;
    UINT32              m_uiSharedUsers;
    HANDLE              m_hSharedUseSph;
    HANDLE              m_hExclusiveUseSph;
    CEvent *            m_pComPortCancelEvent;
};

// Function to be used to destroy items in the queue
// when the queue is destroyed.
typedef void (*PFN_QUEUE_ITEMDTOR)(void *pItem);

/*
template <class Type, UINT32 Size>
class CBoundedQueue
{
private:
    CEvent              m_eventSpace;
    CEvent              m_eventItems;
    Type *              m_rgpItems[Size];
    UINT32                m_uiUsed;
    PFN_QUEUE_ITEMDTOR  m_pfnItemDtor;
    CMutex *            m_pBoundedQueueMutex;

public:
            CBoundedQueue(PFN_QUEUE_ITEMDTOR pfnItemDtor = NULL);
    virtual ~CBoundedQueue();

    BOOL    Ready() const { return m_eventSpace.Ready() && m_eventItems.Ready(); }
    BOOL    Init();
    BOOL    Put(Type* const pItem);
    BOOL    Get(Type** ppItem);
    BOOL    Peek(Type** ppItem);
    HRESULT ConditionalGet(const PFN_QUEUE_TEST pfnTest, const UINT32 uiData, Type** rpItem);

    const
    TCHAR * GetItemsEventName() { return m_eventItems.GetName(); }
    HANDLE  GetItemsEvent() { return m_eventItems.GetHandle(); }
    HANDLE  GetSpaceEvent() { return m_eventSpace.GetHandle(); }
};

template <class Type, UINT32 Size>
CBoundedQueue<Type, Size>::CBoundedQueue(PFN_QUEUE_ITEMDTOR pfnItemDtor)
:   m_uiUsed(0),
    m_pfnItemDtor(pfnItemDtor)
{
    m_pBoundedQueueMutex = new CMutex();

    for (UINT32 i = 0; i < Size; i++)
        m_rgpItems[i] = NULL;
}

template <class Type, UINT32 Size>
CBoundedQueue<Type, Size>::~CBoundedQueue()
{
    // In case someone is still waiting on
    // this queue, try to avoid conflicts...
    if (m_eventItems.Ready())
        m_eventItems.Reset();   // no items to Dequeue
    if (m_eventSpace.Ready())
        m_eventSpace.Reset();   // no space for Enqueue

    if (m_pfnItemDtor)
    {
        for (UINT32 i = 0; i < m_uiUsed; i++)
        {
            m_pfnItemDtor(m_rgpItems[i]);
            m_rgpItems[i] = NULL;
        }
    }

    m_uiUsed = 0;

    delete m_pBoundedQueueMutex;
    m_pBoundedQueueMutex = NULL;
}

template <class Type, UINT32 Size>
BOOL CBoundedQueue<Type, Size>::Init()
{
    return
        m_eventSpace.Init(TRUE, TRUE) &&
        m_eventItems.Init(TRUE, FALSE, TEXT("CBoundedQueue_"));
}

template <class Type, UINT32 Size>
BOOL CBoundedQueue<Type, Size>::Put(Type * const pItem)
{
    ASSERT(Ready());
    BOOL fRet = FALSE;

    CMutex::Lock(m_pBoundedQueueMutex);

    if (m_uiUsed < Size)
    {
        // Put the element in our queue.
        m_rgpItems[m_uiUsed++] = pItem;

        // If this filled the queue buffer,
        // then reset the space avail event.
        if (m_uiUsed == Size)
            m_eventSpace.Reset();

        // There's something new in the queue,
        // so signal waiting threads.
        m_eventItems.Set();

        fRet = TRUE;
    }
    else
    {
        fRet = FALSE;
    }

    CMutex::Unlock(m_pBoundedQueueMutex);
    return fRet;
}

template <class Type, UINT32 Size>
BOOL CBoundedQueue<Type, Size>::Get(Type ** ppItem)
{
    return ConditionalGet(NULL, 0, ppItem);
}

template <class Type, UINT32 Size>
BOOL CBoundedQueue<Type, Size>::Peek(Type ** ppItem)
{
    ASSERT(Ready());

    BOOL fRet = FALSE;

    ASSERT(ppItem);
    if (!ppItem)
        return fRet;

    CMutex::Lock(m_pBoundedQueueMutex);

    if (m_uiUsed > 0)
    {
        // Copy the element from the queue.
        *ppItem = m_rgpItems[0];

        fRet = TRUE;
    }
    else
    {
        fRet = FALSE;
    }

    CMutex::Unlock(m_pBoundedQueueMutex);
    return fRet;
}

template <class Type, UINT32 Size>
HRESULT CBoundedQueue<Type, Size>::ConditionalGet(const PFN_QUEUE_TEST pfnTest,
                                                  const UINT32 uiData,
                                                  Type** ppItem)
{
    ASSERT(Ready());
    HRESULT hr = S_FALSE;

    ASSERT(ppItem);

    if (!ppItem)
        return hr;

    CMutex::Lock(m_pBoundedQueueMutex);

    if (m_uiUsed > 0)
    {
        // Copy the element from the queue.
        Type * pItem = m_rgpItems[0];

        // If there is a condition, make sure
        // it is satisfied.
        if (pfnTest && !pfnTest(pItem, uiData))
        {
            hr = E_FAIL;
            goto Error;
        }

        // One less in queue now...
        m_uiUsed--;

        if (m_uiUsed)
        {
            // Slide remaining items over to the beginning of the buffer.
            memmove(m_rgpItems, (BYTE*)m_rgpItems + sizeof(Type*), sizeof(Type*) * m_uiUsed);

            // If we just made space, then set this event.
            if (Size - 1 == m_uiUsed)
                m_eventSpace.Set();
        }
        else
        {
            // The last item was copied from the queue,
            // so reset this event.
            m_eventItems.Reset();
        }

        // NULL out the space that was freed up.
        m_rgpItems[m_uiUsed] = NULL;

        // Set the output parameter.
        *ppItem = pItem;

        hr = S_OK;
    }
    else
    {
        hr = S_FALSE;
    }

Error:
    CMutex::Unlock(m_pBoundedQueueMutex);
    return hr;
}
*/

template <class Type>
class CUnboundedQueue
{
private:
    CEvent *            m_pEventItems;
    Type **             m_rgpItems;
    UINT32              m_uiInitialSize;
    UINT32              m_uiCurrentSize;
    UINT32              m_uiUsed;
    PFN_QUEUE_ITEMDTOR  m_pfnItemDtor;
    CMutex *            m_pUnboundedQueueMutex;

protected:
    BOOL    GrowQueue();

public:
            CUnboundedQueue(PFN_QUEUE_ITEMDTOR pfnItemDtor = NULL);
    virtual ~CUnboundedQueue();

    BOOL    Ready() const { return m_rgpItems != NULL && m_pEventItems != NULL; }
    BOOL    Init(UINT32 uiInitialSize);
    BOOL    Put(Type* const pItem);
    BOOL    Get(Type** ppItem);
    BOOL    Peek(Type** ppItem);
    HRESULT ConditionalGet(const PFN_QUEUE_TEST pfnTest, const UINT32 uiData, Type** rpItem);

    const
    char *  GetItemsEventName() { return CEvent::GetName(m_pEventItems); }
    HANDLE  GetItemsEvent() { return CEvent::GetHandle(m_pEventItems); }

    UINT32   GetSize() { return m_uiUsed; }
    UINT32   GetCurrentBufferSize() { return m_uiCurrentSize; }
};

template <class Type>
CUnboundedQueue<Type>::CUnboundedQueue(PFN_QUEUE_ITEMDTOR pfnItemDtor)
:   m_rgpItems(NULL),
    m_uiInitialSize(0),
    m_uiCurrentSize(0),
    m_uiUsed(0),
    m_pfnItemDtor(pfnItemDtor)
{
    m_pUnboundedQueueMutex = new CMutex();
    m_pEventItems = new CEvent("CUnboundedQueue_", TRUE, FALSE, TRUE);
}

template <class Type>
CUnboundedQueue<Type>::~CUnboundedQueue()
{
    CEvent::Reset(m_pEventItems);

    if (m_rgpItems)
    {
        if (m_pfnItemDtor)
        {
            for (UINT32 i = 0; i < m_uiUsed; i++)
            {
                m_pfnItemDtor(m_rgpItems[i]);
                m_rgpItems[i] = NULL;
            }
        }

        delete[] m_rgpItems;
        m_rgpItems = NULL;
    }

    m_uiUsed = 0;

    delete m_pUnboundedQueueMutex;
    m_pUnboundedQueueMutex = NULL;

    delete m_pEventItems;
    m_pEventItems = NULL;
}

template <class Type>
BOOL CUnboundedQueue<Type>::Init(UINT32 uiInitialSize)
{
    if (m_rgpItems || uiInitialSize < 1)
        return FALSE;

    m_rgpItems = new Type*[uiInitialSize];
    if (!m_rgpItems)
        return FALSE;

    m_uiInitialSize = uiInitialSize;
    m_uiCurrentSize = uiInitialSize;

    return TRUE;
}

template <class Type>
BOOL CUnboundedQueue<Type>::GrowQueue()
{
    ASSERT(Ready());

    UINT32 dwNewSize = m_uiCurrentSize + m_uiInitialSize;

    if ( dwNewSize < m_uiUsed )
    {
        return FALSE;
    }

    Type ** rgpNewBuffer = new Type*[dwNewSize];
    if (!rgpNewBuffer)
        return FALSE;

    memcpy(rgpNewBuffer, m_rgpItems, sizeof(Type*) * m_uiUsed);

    delete[] m_rgpItems;

    m_rgpItems = rgpNewBuffer;
    m_uiCurrentSize = dwNewSize;

    return TRUE;
}

template <class Type>
BOOL CUnboundedQueue<Type>::Put(Type * const pItem)
{
    ASSERT(Ready());
    BOOL fRet = TRUE;

    CMutex::Lock(m_pUnboundedQueueMutex);

    ASSERT(m_uiUsed <= m_uiCurrentSize);

    if (m_uiUsed >= m_uiCurrentSize)
    {
        if (!GrowQueue())
            fRet = FALSE;
    }

    if (fRet)
    {
        ASSERT(m_uiUsed < m_uiCurrentSize);

        // Put the element in our queue.
        m_rgpItems[m_uiUsed++] = pItem;

        // There's something new in the queue,
        // so signal waiting threads.
        CEvent::Signal(m_pEventItems);
    }

    CMutex::Unlock(m_pUnboundedQueueMutex);
    return fRet;
}

template <class Type>
BOOL CUnboundedQueue<Type>::Get(Type ** ppItem)
{
    return ConditionalGet(NULL, 0, ppItem);
}

template <class Type>
BOOL CUnboundedQueue<Type>::Peek(Type ** ppItem)
{
    BOOL fRet = FALSE;
    ASSERT(Ready());

    ASSERT(ppItem);
    if (ppItem)
    {
        CMutex::Lock(m_pUnboundedQueueMutex);

        if (m_uiUsed > 0)
        {
            // Copy the element from the queue.
            *ppItem = m_rgpItems[0];
            fRet = TRUE;
        }

        CMutex::Unlock(m_pUnboundedQueueMutex);
    }

    return fRet;
}

template <class Type>
HRESULT CUnboundedQueue<Type>::ConditionalGet(const PFN_QUEUE_TEST pfnTest,
                                              const UINT32 uiData,
                                              Type** ppItem)
{
    ASSERT(Ready());
    HRESULT hr = S_FALSE;
    ASSERT(ppItem);

    if (!ppItem)
        goto Error;

    CMutex::Lock(m_pUnboundedQueueMutex);

    if (m_uiUsed > 0)
    {
        // Copy the element from the queue.
        Type * pItem = m_rgpItems[0];

        // If there is a condition, make sure
        // it is satisfied.
        if (pfnTest && !pfnTest(pItem, uiData))
        {
            hr = E_FAIL;
            goto Error;
        }

        // One less in queue now...
        m_uiUsed--;

        if (m_uiUsed)
        {
            // Slide remaining items over to the beginning of the buffer.
            memmove(m_rgpItems, (BYTE*)m_rgpItems + sizeof(Type*), sizeof(Type*) * m_uiUsed);
        }
        else
        {
            // The last item was copied from the queue,
            // so reset this event.
            CEvent::Reset(m_pEventItems);
        }

        // NULL out the space that was freed up.
        m_rgpItems[m_uiUsed] = NULL;

        // Set the output parameter.
        *ppItem = pItem;

        hr = S_OK;
    }

Error:
    CMutex::Unlock(m_pUnboundedQueueMutex);
    return hr;
}
#endif

// Generic queue
template <class Type, UINT32 Size>
class CQueue
{
public:
                    CQueue(const BOOL fDontCallDestructors = FALSE);
    virtual         ~CQueue();

    BOOL            Init(CEvent * const pEvent);
    BOOL            Put(Type* const pItem, const UINT32 dwTimeout);
    RIL_RESULT_CODE Get(Type*& rpItem, const UINT32 dwTimeout);
    BOOL            Peek(Type*& rpItem);
    RIL_RESULT_CODE ConditionalGet(const PFN_QUEUE_TEST pfnTest, const UINT32 uiData, Type*& rpItem,
                               const UINT32 dwTimeout);
    RIL_RESULT_CODE WaitForNextItem(const UINT32 dwTimeout);
    CEvent *        GetPutEvent() { return m_pPutEvent; }
    void            Enum(const PFN_QUEUE_ENUM pfnEnum, const UINT32 uiData, const BOOL fClear);
    BOOL            FEmpty();
    UINT32            GetSize() { return m_uiUsed; }

protected:
    RIL_RESULT_CODE GetInternal(Type*& rpItem, const UINT32 dwTimeout);
    BOOL            PeekInternal(Type*& rpItem);
    BOOL            WaitForEmptySpaceInternal(const UINT32 dwTimeout);
    RIL_RESULT_CODE WaitForNextItemInternal(const UINT32 dwTimeout);

    Type*               m_rgpItems[Size];
    UINT32                m_uiUsed;
    CEvent *            m_pGetEvent;
    CEvent *            m_pPutEvent;
    CEvent *            m_pCancelEvent;
    CMutex *            m_pQueueMutex;
    BOOL                m_fDontCallDestructors;
    BOOL                m_fInited;
};


// Priority queue
template <class Type, UINT32 Size>
class CPriorityQueue : public CQueue<Type, Size>
{
public:
                CPriorityQueue();
                ~CPriorityQueue();

    BOOL        Put(Type* const pItem, const UINT32 dwTimeout);
};


// Function to be passed to CDblList::Enum().
//    This function should return TRUE for enumeration to stop.
typedef BOOL (*PFN_LIST_ENUM)(void* pItem, UINT32 uiData);


// Generic doubly-linked list
template <class Type>
class CDblList
{
public:
            CDblList();
            ~CDblList();

    BOOL    Add(Type* const pAdd);
    BOOL    Remove(const Type* const pRemove);
    void    Enum(const PFN_LIST_ENUM pfnEnum, const UINT32 uiData);

private:
    Type*               m_pElems;
    CMutex *            m_pDblListMutex;
};



//
// Template definitions
//

//
// Queue ctor
//
template <class Type, UINT32 Size>
CQueue<Type, Size>::CQueue(const BOOL fDontCallDestructors /* = FALSE */)
: m_uiUsed(0),
  m_pGetEvent(NULL),
  m_pPutEvent(NULL),
  m_pCancelEvent(NULL),
  m_fDontCallDestructors(fDontCallDestructors),
  m_fInited(FALSE)
{
    // Initialize the critical section
    m_pQueueMutex = new CMutex();

    for (UINT32 i = 0; i < Size; i++)
    {
        m_rgpItems[i] = NULL;
    }
}


//
// Queue dtor
//
template <class Type, UINT32 Size>
CQueue<Type, Size>::~CQueue()
{
    // Get rid of the events
    if (m_pPutEvent)
    {
        delete m_pPutEvent;
        m_pPutEvent = NULL;
    }

    if (m_pGetEvent)
    {
        delete m_pGetEvent;
        m_pGetEvent = NULL;
    }

    // Delete the elements still in the queue
    for (UINT32 i = 0; i < m_uiUsed; i++)
    {
        if (m_fDontCallDestructors)
        {
            // NOTE: We use this to avoid useless asserts when this destructor (called from RIL driver) frees
            //       memory allocated by RIL proxy
            FreeBlob(m_rgpItems[i]);
        }
        else
        {
            delete m_rgpItems[i];
            m_rgpItems[i] = NULL;
        }
    }

    // Get rid of the critical section
    delete m_pQueueMutex;
    m_pQueueMutex = NULL;
}


//
// Initalize the queue
//
template <class Type, UINT32 Size>
BOOL CQueue<Type, Size>::Init(CEvent * const pEvent)
{
    if (NULL == pEvent)
    {
        goto Error;
    }

    // If the queue is already initialized, skip this
    if (m_fInited)
    {
        goto Success;
    }

    // Create the events
    m_pGetEvent = new CEvent();
    if (!m_pGetEvent)
    {
        goto Error;
    }

    m_pPutEvent = new CEvent();
    if (!m_pPutEvent)
    {
        goto Error;
    }

    m_pCancelEvent = pEvent;
    m_fInited = TRUE;

Success:
    return TRUE;

Error:
    if (m_pGetEvent)
    {
        delete m_pGetEvent;
        m_pGetEvent = NULL;
    }
    return FALSE;
}


//
// Enqueue an element
//
template <class Type, UINT32 Size>
BOOL CQueue<Type, Size>::Put(Type* const pNew, const UINT32 dwTimeout)
{
    CMutex::Lock(m_pQueueMutex);

    BOOL fRet = FALSE;

    if (FALSE == m_fInited)
    {
        RIL_LOG_CRITICAL("CQueue<Type, Size>::Put() : ERROR : m_fInited was FALSE\r\n");
        goto Error;
    }

    if (pNew == NULL)
    {
        RIL_LOG_CRITICAL("CQueue<Type, Size>::Put() : ERROR : pNew was NULL\r\n");
        goto Error;
    }

    if (m_fInited)
    {
        if (!this->WaitForEmptySpaceInternal(dwTimeout))
        {
            goto Error;
        }

        if (m_uiUsed >= Size)
        {
            RIL_LOG_CRITICAL("CQueue<Type, Size>::Put() : ERROR : No space in the queue\r\n");
            goto Error;
        }

        // We have space in the queue
        m_rgpItems[m_uiUsed] = pNew;
        m_uiUsed++;

        // Signal the Put event
        CEvent::Signal(m_pPutEvent);
    }

    fRet = TRUE;

Error:
    CMutex::Unlock(m_pQueueMutex);
    return fRet;
}


//
// Dequeue an element
//
template <class Type, UINT32 Size>
RIL_RESULT_CODE CQueue<Type, Size>::Get(Type*& rpItem, const UINT32 dwTimeout)
{
    CMutex::Lock(m_pQueueMutex);

    RIL_RESULT_CODE resCode = GetInternal(rpItem, dwTimeout);

    CMutex::Unlock(m_pQueueMutex);
    return resCode;
}


//
// Dequeue an element (internal version)
//
template <class Type, UINT32 Size>
RIL_RESULT_CODE CQueue<Type, Size>::GetInternal(Type*& rpItem, const UINT32 dwTimeout)
{
    RIL_RESULT_CODE retCode = RRIL_E_NO_ERROR;

    // Initialize the returned pointer in case we fail
    rpItem = NULL;

    if (m_fInited)
    {
        retCode = WaitForNextItemInternal(dwTimeout);
        if (RRIL_E_NO_ERROR != retCode)
        {
            goto Error;
        }

        // Got an item in the queue
        rpItem = m_rgpItems[0];
        m_uiUsed--;
        if ( (sizeof(m_rgpItems)/sizeof(m_rgpItems[0])) < m_uiUsed )
        {
            retCode = RRIL_E_ABORT;
            goto Error;
        }
        memmove(m_rgpItems, (BYTE*)m_rgpItems + sizeof(Type*), sizeof(Type*) * m_uiUsed);
        m_rgpItems[m_uiUsed] = NULL;

        // Signal the Get event
        CEvent::Signal(m_pGetEvent);
    }
    else
    {
        retCode = RRIL_E_UNKNOWN_ERROR;
    }

Error:
    return retCode;
}


//
// Retrieve a pointer to the first element in the queue
//
template <class Type, UINT32 Size>
BOOL CQueue<Type, Size>::Peek(Type*& rpItem)
{
    CMutex::Lock(m_pQueueMutex);

    BOOL fRet = PeekInternal(rpItem);

    CMutex::Unlock(m_pQueueMutex);

    return fRet;
}


//
// Retrieve a pointer to the first element in the queue (internal version)
//
template <class Type, UINT32 Size>
BOOL CQueue<Type, Size>::PeekInternal(Type*& rpItem)
{
    BOOL fRet = FALSE;

    if (FALSE == m_fInited)
    {
        RIL_LOG_CRITICAL("PeekInternal() - ERROR: m_fInited was FALSE\r\n");
        goto Error;
    }

    rpItem = NULL;

    if (!m_uiUsed)
    {
        RIL_LOG_CRITICAL("PeekInternal() - ERROR: m_uiUsed was 0\r\n");
        goto Error;
    }

    rpItem = m_rgpItems[0];
    if (NULL == rpItem)
    {
        RIL_LOG_CRITICAL("PeekInternal() - ERROR: rpItem was NULL\r\n");
        goto Error;
    }

    fRet = TRUE;

Error:
    return fRet;
}


//
// Dequeue an element from the queue, if it satisfies a condition
//    (will wait for the next element for a specified timeout)
//
template <class Type, UINT32 Size>
RIL_RESULT_CODE CQueue<Type, Size>::ConditionalGet(const PFN_QUEUE_TEST pfnTest, const UINT32 uiData, Type*& rpItem,
                                           const UINT32 dwTimeout)
{
    CMutex::Lock(m_pQueueMutex);

    RIL_RESULT_CODE retCode = RRIL_E_NO_ERROR;

    // Wait for an element to appear in the queue
    retCode = WaitForNextItemInternal(dwTimeout);
    if (RRIL_E_NO_ERROR != retCode)
    {
        goto Error;
    }

    // Peek the first element
    if (!PeekInternal(rpItem))
    {
        retCode = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    // See if the first element satisfies the condition
    if (!pfnTest(rpItem, uiData))
    {
        rpItem = NULL;
        retCode = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

    // Retrieve the first element
    if (RRIL_E_NO_ERROR != GetInternal(rpItem, WAIT_FOREVER))
    {
        rpItem = NULL;
        retCode = RRIL_E_UNKNOWN_ERROR;
        goto Error;
    }

Error:
    CMutex::Unlock(m_pQueueMutex);
    return retCode;
}


//
// Wait until an element appears in the queue
//
template <class Type, UINT32 Size>
RIL_RESULT_CODE CQueue<Type, Size>::WaitForNextItem(const UINT32 dwTimeout)
{
    CMutex::Lock(m_pQueueMutex);

    RIL_RESULT_CODE resCode = WaitForNextItemInternal(dwTimeout);

    CMutex::Unlock(m_pQueueMutex);

    return resCode;
}


//
// Wait until an item appears in the queue (internal version)
//
template <class Type, UINT32 Size>
RIL_RESULT_CODE CQueue<Type, Size>::WaitForNextItemInternal(const UINT32 dwTimeout)
{
    CEvent * rgpEvents[2] = { m_pPutEvent, m_pCancelEvent };
    UINT32 dwWait;
    RIL_RESULT_CODE retCode = RRIL_E_NO_ERROR;

    if (m_fInited)
    {
        while(1)
        {
            // Are there any items in the queue?
            if (m_uiUsed)
            {
                // Yes -- proceed
                break;
            }
            else
            {
                // No - need to wait for Put to happen
                CMutex::Unlock(m_pQueueMutex);

                dwWait = CEvent::WaitForAnyEvent(2, rgpEvents, dwTimeout);

                if (WAIT_EVENT_0_SIGNALED + 1 == dwWait)
                {
                    // We hit the terminate event -- quit
                    retCode = RRIL_E_CANCELLED;
                    goto Error;
                }
                else if (WAIT_TIMEDOUT == dwWait)
                {
                    // We timed out-- quit
                    retCode = RRIL_E_TIMED_OUT;
                    goto Error;
                }
                else
                {
                    if (WAIT_EVENT_0_SIGNALED != dwWait)
                    {
                        retCode = RRIL_E_UNKNOWN_ERROR;
                        goto Error;
                    }
                }

                CMutex::Lock(m_pQueueMutex);
            }
        }

        if (m_uiUsed == 0)
        {
            retCode = RRIL_E_UNKNOWN_ERROR;
        }
    }
    else
    {
        retCode = RRIL_E_UNKNOWN_ERROR;
    }

Error:
    return retCode;
}


//
// Wait until an empty slot appears in the queue (internal version)
//
template <class Type, UINT32 Size>
BOOL CQueue<Type, Size>::WaitForEmptySpaceInternal(const UINT32 dwTimeout)
{
    CEvent * rgpEvents[2] = { m_pGetEvent, m_pCancelEvent };
    UINT32 dwWait;
    BOOL fRet = FALSE;

    if (m_fInited)
    {
        while (1)
        {
            // Is there space in the queue?
            if (m_uiUsed < Size)
            {
                // Yes -- proceed
                break;
            }
            else
            {
                if (Size != m_uiUsed)
                {
                    goto Error;
                }

                // No -- need to wait for Get to happen

                CMutex::Unlock(m_pQueueMutex);

                dwWait = CEvent::WaitForAnyEvent(2, rgpEvents, dwTimeout);
                if (WAIT_EVENT_0_SIGNALED != dwWait)
                {
                    // We hit the terminate event or timed out
                    goto Error;
                }

                CMutex::Lock(m_pQueueMutex);
            }
        }
    }

    if (m_uiUsed >= Size)
    {
        goto Error;
    }

    fRet = TRUE;

Error:
    return fRet;
}


//
// Enum all queue elements, calling the provided routine for each item
//
template <class Type, UINT32 Size>
void CQueue<Type, Size>::Enum(const PFN_QUEUE_ENUM pfnEnum, const UINT32 uiData, const BOOL fClear)
{
    CMutex::Lock(m_pQueueMutex);

    if (m_fInited)
    {
        for (UINT32 i = 0; i < m_uiUsed; i++)
        {
            if (pfnEnum((void*)m_rgpItems[i], uiData))
            {
                break;
            }
        }

        if (fClear)
        {
            // We also need to clear the queue
            for (UINT32 i = 0; i < m_uiUsed; i++)
            {
                delete m_rgpItems[i];
                m_rgpItems[i] = NULL;
            }
            m_uiUsed = 0;
        }
    }

    CMutex::Unlock(m_pQueueMutex);
}


//
// Determine if the queue is empty
//
template <class Type, UINT32 Size>
BOOL CQueue<Type, Size>::FEmpty()
{
    CMutex::Lock(m_pQueueMutex);

    BOOL fRet = !m_uiUsed;

    CMutex::Unlock(m_pQueueMutex);
    return fRet;
}


//
// Priority queue ctor
//
template <class Type, UINT32 Size>
CPriorityQueue<Type, Size>::CPriorityQueue()
: CQueue<Type, Size>(FALSE)
{
}


//
// Priority queue dtor
//
template <class Type, UINT32 Size>
CPriorityQueue<Type, Size>::~CPriorityQueue()
{
}


//
// Enqueue an element
//
template <class Type, UINT32 Size>
BOOL CPriorityQueue<Type, Size>::Put(Type* const pNew, const UINT32 dwTimeout)
{
    CMutex::Lock(this->m_pQueueMutex);

    UINT32 i;
    BOOL fRet = FALSE;

    if (FALSE == this->m_fInited)
    {
        RIL_LOG_CRITICAL("CPriorityQueue<Type, Size>::Put() : ERROR : m_fInited was FALSE\r\n");
        goto Error;
    }

    if (pNew == NULL)
    {
        RIL_LOG_CRITICAL("CPriorityQueue<Type, Size>::Put() : ERROR : pNew was NULL\r\n");
        goto Error;
    }

    if (this->m_fInited)
    {
        if (!this->WaitForEmptySpaceInternal(dwTimeout))
        {
            goto Error;
        }

        if (this->m_uiUsed >= Size)
        {
            RIL_LOG_CRITICAL("CPriorityQueue<Type, Size>::Put() : ERROR : m_uiUsed >= Size!\r\n");
            goto Error;
        }

        // We have space in the queue -- find the correct spot for this item
        for (i = 0; i < this->m_uiUsed; i++)
        {
            if (pNew->GetPriority() < this->m_rgpItems[i]->GetPriority())
            {
                // We found the first item whose pri is lower (i.e., greater) than the one for the new item --
                //     shift other items in the queue
                memmove(&this->m_rgpItems[i + 1], &this->m_rgpItems[i], sizeof(Type*) * (this->m_uiUsed - i));
                break;
            }
        }

        // Insert the new item
        this->m_rgpItems[i] = pNew;
        this->m_uiUsed++;

        // Signal the Put event
        CEvent::Signal(this->m_pPutEvent);
    }

    fRet = TRUE;

Error:
    CMutex::Unlock(this->m_pQueueMutex);
    return fRet;
}


//
// List ctor
//
template <class Type>
CDblList<Type>::CDblList()
: m_pElems(NULL)
{
    m_pDblListMutex = new CMutex();
}


//
// List dtor
//
template <class Type>
CDblList<Type>::~CDblList()
{
    delete m_pDblListMutex;
    m_pDblListMutex = NULL;
}


//
// Add an item to the list
//
template <class Type>
BOOL CDblList<Type>::Add(Type* const pAdd)
{
    CMutex::Lock(m_pDblListMutex);

    BOOL fRet = FALSE;

    // Check that the new element exists
    if (!pAdd)
    {
        goto Error;
    }

    // Add the new element at the front
    pAdd->SetNext(m_pElems);
    pAdd->SetPrev(NULL);

    if (pAdd->GetNext())
    {
        pAdd->GetNext()->SetPrev(pAdd);
    }

    m_pElems = pAdd;
    fRet = TRUE;

Error:
    CMutex::Unlock(m_pDblListMutex);
    return fRet;
}


//
// Remove an item from the list
//
template <class Type>
BOOL CDblList<Type>::Remove(const Type* const pRemove)
{
    CMutex::Lock(m_pDblListMutex);

    BOOL fRet = FALSE;

    // Check that the element to be removed exists
    if (!pRemove)
    {
        goto Error;
    }

    // Is this element head of the list?
    if (pRemove == m_pElems)
    {
        // Yes
        m_pElems = (Type*)pRemove->GetNext();
    }
    else
    {
        // No
        pRemove->GetPrev()->SetNext(pRemove->GetNext());
    }

    // Re-route the links
    if (pRemove->GetNext())
    {
        pRemove->GetNext()->SetPrev(pRemove->GetPrev());
    }

    fRet = TRUE;

Error:
    CMutex::Unlock(m_pDblListMutex);
    return fRet;
}


//
// Enum all list elements, calling the provided routine for each item
//
template <class Type>
void CDblList<Type>::Enum(const PFN_LIST_ENUM pfnEnum, const UINT32 uiData)
{
    CMutex::Lock(m_pDblListMutex);

    Type* pNext;
    for (Type* pWalk = m_pElems; pWalk; pWalk = pNext)
    {
        pNext = (Type*)pWalk->GetNext();
        if (pfnEnum((void*)pWalk, uiData))
        {
            break;
        }
    }

    CMutex::Unlock(m_pDblListMutex);
}

//
// Class declarations
//

// self-expanding buffer
class CSelfExpandBuffer
{
public:
    CSelfExpandBuffer();
    virtual ~CSelfExpandBuffer();

    BOOL            Append(const BYTE *szIn, UINT32 nLength);
    const BYTE*     Data() const    { return m_szBuffer; };
    UINT32          Size() const    { return m_uiUsed; };

private:
    static const UINT32 m_nChunkSize = 1024;

protected:
    BYTE*   m_szBuffer;
    UINT32  m_uiUsed;
    UINT32  m_nCapacity;
};

