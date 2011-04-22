////////////////////////////////////////////////////////////////////////////
// extract.h
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//  Provides helper functions from extracting information elements from response
//  strings.
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

#pragma once



// Takes in a string to search for a substring. If found, returns true and rszEnd
// points to the first character after szSkip.
BOOL FindAndSkipString(const BYTE* szStart, const BYTE* szSkip, const BYTE*& rszEnd);

// Moves the pointer past any spaces in the response. Returns TRUE if spaces were skipped.
BOOL SkipSpaces(const BYTE* szStart, const BYTE*& rszEnd);

// Takes in a string to compare with a substring. If the substring matches with
// szStart, the function returns true and rszEnd points to the first character after szSkip.
BOOL SkipString(const BYTE* szStart, const BYTE* szSkip, const BYTE*& rszEnd);

// Looks for a carriage return/line feed at the start of szStart. If found, returns true
// and sets rszEnd to the first character after the pattern.
BOOL SkipRspStart(const BYTE* szStart, const BYTE* szSkip, const BYTE*& rszEnd);

// Looks for a carriage return/line feed at the start of szStart. If found, returns true
// and sets rszEnd to the first character after the pattern. This is identical to SkipRspStart
// but is provided in case the modem responds with a different pattern for responds ends than beginnings.
BOOL SkipRspEnd(const BYTE* szStart, const BYTE* szSkip, const BYTE*& rszEnd);

// Looks for a carriage return/line feed anywhere in szStart. If found, returns true
// and sets rszEnd to the first character after the pattern.
BOOL FindAndSkipRspEnd(const BYTE* szStart, const BYTE* szSkip, const BYTE*& rszEnd);

// Takes the digits in szStart and stores them into a UINT32. If a space follows the last
// digit it will also be consumed. Returns TRUE if at least one digit is found.
BOOL ExtractUInt(const BYTE* szStart, UINT32 &rnValue, const BYTE* &rszEnd);

// Extracts a UINT32 from a given string.
BOOL ExtractUInt32(const BYTE* szStart, UINT32 &rdwValue, const BYTE* &rszEnd);

// Extracts a string enclosed by quotes into a given buffer. Returns TRUE if two quotes are found and
// the buffer given is large enough to contain the string and a NULL termination character.
BOOL ExtractQuotedString(const BYTE* szStart, BYTE* szOutput, const UINT32 cbOutput, const BYTE* &rszEnd);

// Extracts a string ended by szDelimiter into a given buffer. Returns TRUE if szDelimiter is found and
// the buffer given is large enough to contain the string and a NULL termination character.
BOOL ExtractUnquotedString(const BYTE* szStart, const char cDelimiter, BYTE* szOutput, const UINT32 cbOutput, const BYTE* &rszEnd);
BOOL ExtractUnquotedString(const BYTE* szStart, const BYTE* szDelimiter, BYTE* szOutput, const UINT32 cbOutput, const BYTE* &rszEnd);

// Extracts a UINT32 from hex ascii
BOOL ExtractHexUInt32(const BYTE* szStart, UINT32 &rdwValue, const BYTE* &rszEnd);

// Extracts a UINT32 out from a hex string
BOOL ExtractHexUInt(const BYTE* szStart, UINT32& rnValue, const BYTE* &rszEnd);

// Allocates memory for the quoted string extracted from the given buffer and returns it. Caller must delete
// the memory when finished with it.
BOOL ExtractQuotedStringWithAllocatedMemory(const BYTE* szStart, BYTE* &rszString, UINT32 &rcbString, const BYTE* &rszEnd);

// Allocates memory for the unquoted string extracted from the given buffer and returns it. Caller must delete
// the memory when finished with it.
BOOL ExtractUnquotedStringWithAllocatedMemory(const BYTE* szStart, const char chDelimiter, BYTE* &rszString, UINT32 &rcbString, const BYTE* &rszEnd);

// Extracts a decimal number and stores it as a 16.16 fixed point value
BOOL ExtractFixedPointValue(const BYTE* szStart, UINT32 &rdwFPValue, const BYTE* &rszEnd);

// Extracts a decimal value and returns it as a double
BOOL ExtractDouble(const BYTE* szStart, double &rdbValue, const BYTE* &rszEnd);


BOOL ExtractIntAndConvertToUInt(const char* szData, UINT32& rnVal, const char*& rszRemainder);

BOOL ExtractUpperBoundedUInt(const char* szData, const UINT32 nUpperLimit, UINT32& rnVal, const char*& rszRemainder);

BOOL ExtractLowerBoundedUInt(const char* szData, const UINT32 nLowerLimit, UINT32& rnVal, const char*& rszRemainder);
