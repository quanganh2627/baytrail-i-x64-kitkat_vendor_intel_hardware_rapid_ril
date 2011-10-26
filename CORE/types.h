////////////////////////////////////////////////////////////////////////////
// types.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Basic types for RapidRIL
//
// Author:  Francesc Vilarino
// Created: 2009-11-24
//
/////////////////////////////////////////////////////////////////////////////
//  Modification Log:
//
//  Date       Who      Ver   Description
//  ---------  -------  ----  -----------------------------------------------
//  Nov 24/09  FV       1.00  Established v1.00 based on current code base.
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(__RIL_TYPES_H__)
#define __RIL_TYPES_H__

typedef bool                BOOL;
typedef unsigned char       BYTE;
typedef unsigned char       UINT8;
typedef signed char         INT8;
typedef unsigned short      UINT16;
typedef short               INT16;
typedef unsigned int        UINT32;
typedef int                 INT32;
typedef long int            LONG;
typedef unsigned long int   ULONG;
typedef wchar_t             WCHAR;

#define FALSE               false
#define TRUE                true

#endif  // __RIL_TYPES_H__
