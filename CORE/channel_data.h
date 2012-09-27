////////////////////////////////////////////////////////////////////////////
// channel_Data.h
//
// Copyright 2005-2007 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the CChannel_Data class, which is used to
//    facilitate the use of multiple data channels.
//    GPRS/UMTS data (1st primary context)
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(RIL_CHANNEL_DATA_H)
#define RIL_CHANNEL_DATA_H

#include "channel_nd.h"

class CChannel_Data : public CChannel
{
public:
    CChannel_Data(UINT32 uiChannel);
    virtual ~CChannel_Data();

private:
    //  Prevent assignment: Declared but not implemented.
    CChannel_Data(const CChannel_Data& rhs);  // Copy Constructor
    CChannel_Data& operator=(const CChannel_Data& rhs);  //  Assignment operator

public:
    //  public port interface
    BOOL OpenPort();

    void ResetDataCallInfo();

    // get / set functions
    void SetDataFailCause(int cause);
    int GetDataFailCause();

    UINT32 GetContextID() const;
    BOOL SetContextID( UINT32 dwContextID );

    void SetPdpType(const char* pPdpType);
    void GetPdpType(char* pPdpType, const int maxSize);

    void SetInterfaceName(const char* pInterfaceName);
    void GetInterfaceName(char* pInterfaceName, const int maxSize);

    void SetIpAddress(const char* pIpAddr1, const char* pIpAddr2);
    void GetIpAddress(char* pIpAddr, const int maxIpAddrSize,
                                    char* pIpAddr2, const int maxIpAddr2Size);

    void SetDNS(const char* pDNS1, const char* pDNS2,
                                const char* pIpV6DNS1, const char* pIpV6DNS2);
    void GetDNS(char* pDNS1, int maxDNS1Size,
                                    char* pDNS2, const int maxDNS2Size,
                                    char* pIpV6DNS1, const int maxIpV6DNS1Size,
                                    char* pIpV6DNS2, const int maxIpV6DNS2Size);

    void SetGateway(const char* pIpGateway);
    void GetGateway(char* pIpGateway, const int maxSize);

    void SetDataState(int state);
    int GetDataState();

    void GetDataCallInfo(S_DATA_CALL_INFO& rsDataCallInfo);

    //
    // helper functions to convert ContextID, Dlci and Channel
    //
    static CChannel_Data* GetChnlFromContextID(UINT32 dwContextID);
    static CChannel_Data* GetChnlFromRilChannelNumber(UINT32 index);

    // used by 7x60 modems only
    static int GetFreeHSIChannel(UINT32 uiCID, int sindex, int eIndex);
    static bool FreeHSIChannel(UINT32 uiCID);

    //  This function returns the next free data channel.  Also, populates the
    //  context ID of returned channel.
    //  If error, then NULL is returned.
    static CChannel_Data* GetFreeChnl(UINT32& outCID);
    // used by 7x60 modems only
    static CChannel_Data* GetFreeChnlsRilHsi(UINT32& outCID, int dataProfile);

    // used by 7x60 modems only
    int GetDataProfile() { return m_dataProfile; };
    int GetHSIChannel() { return m_hsiChannel; };
    BOOL IsHSIDirect() { return m_hsiDirect; };

private:

    int m_dataFailCause;
    UINT32 m_uiContextID;
    int m_dataState;

    char m_szPdpType[MAX_PDP_TYPE_SIZE];

    char m_szInterfaceName[MAX_INTERFACE_NAME_SIZE];

    //  Local storage of IP address, DNS1, DNS2
    char m_szIpAddr[MAX_IPADDR_SIZE];
    char m_szDNS1[MAX_IPADDR_SIZE];
    char m_szDNS2[MAX_IPADDR_SIZE];

    //  For IPV4V6, there could be 2 IP addresses
    char m_szIpAddr2[MAX_IPADDR_SIZE];

    //  For IPV4V6, there could be 2 DNS addresses for primary and secondary.
    char m_szIpV6DNS1[MAX_IPADDR_SIZE];
    char m_szIpV6DNS2[MAX_IPADDR_SIZE];

    char m_szIpGateways[MAX_IPADDR_SIZE];

    // used by 7x60 modems only
    int m_dataProfile;
    BOOL m_hsiDirect;
    int m_hsiChannel;

protected:
    BOOL FinishInit();
    BOOL AddSilos();

};

#endif  // RIL_CHANNEL_DATA_H
