////////////////////////////////////////////////////////////////////////////
// data_util.h
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Provides implementations for the RIL Utility functions specific to
//    data.
/////////////////////////////////////////////////////////////////////////////

#include "types.h"

//  This is for socket-related calls.
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/gsmmux.h>

// Helper functions for configuring data connections
BOOL setaddr6(int sockfd6, struct ifreq *ifr, const char *addr);
BOOL setaddr(int s, struct ifreq *ifr, const char *addr);
BOOL setflags(int s, struct ifreq *ifr, int set, int clr);
BOOL setmtu(int s, struct ifreq *ifr);

int MapErrorCodeToRilDataFailCause(UINT32 uiCause);

// Helper function to convert IP addresses to Android-readable format.
// szIpIn [IN] - The IP string to be converted
// szIpOut [OUT] - The converted IPv4 address in Android-readable format if there is an IPv4 address.
// uiIpOutSize [IN] - The size of the szIpOut buffer
// szIpOut2 [OUT] - The converted IPv6 address in Android-readable format if there is an IPv6 address.
// uiIpOutSize [IN] - The size of szIpOut2 buffer
//
// If IPv4 format a1.a2.a3.a4, then szIpIn is copied to szIpOut.
// If Ipv6 format:
// Convert a1.a2.a3.a4.a5. ... .a15.a16 to XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX IPv6 format
// output is copied to szIpOut2
// If Ipv4v6 format:
// Convert a1.a2.a3.a4.a5. ... .a19.a20 to
// a1.a2.a3.a4 to szIpOut
// XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX (a5-a20) to szIpOut2
// If szIpOut2 is NULL, then this parameter is ignored
BOOL ConvertIPAddressToAndroidReadable(char *szIpIn, char *szIpOut, UINT32 uiIpOutSize, char *szIpOut2, UINT32 uiIpOutSize2);
