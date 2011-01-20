#pragma once

//
// Notification data
//
class CNotificationData
{
public:
                        CNotificationData();
                        ~CNotificationData();

    BOOL                InitFromDWORDBlob(const UINT32 dwCode, const UINT32 dwBlob);

    // For all Android unsolicited responses, set fCpyMem to FALSE. This ensures our internal string pointers
    // are correct in the memory we return to Android. The framework already ensures this memory will be freed. 
    BOOL                InitFromRealBlob(const UINT32 dwCode, void* pBlob, UINT32 cbBlob, const BOOL fCpyMem = TRUE);

    BOOL                FinishInitFromRspBlob(const CResponse& rRsp);
    BOOL                DelayInitFromRspBlob(const UINT32 dwCode);

    BOOL                FDelayedInitFromRsp() const { return !!m_fDelayedInitFromRsp; };
    UINT32              GetCode() const             { if (FALSE != !!m_fInited) {return m_dwCode;} else return NULL; };
    void*               GetBlob() const             { if (FALSE != !!m_fInited) {return m_pBlob;} else return NULL; };
    UINT32              GetSize() const             { if (FALSE != !!m_fInited) {return m_cbBlob;} else return NULL; };

private:
    UINT32       m_dwCode;
    void*        m_pBlob;
    UINT32       m_cbBlob;
    UINT32       m_fInited : 1;
    UINT32       m_fDelayedInitFromRsp : 1;
    UINT32       m_fCpyMem : 1;
};

