////////////////////////////////////////////////////////////////////////////
// context_cache.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Implements PDP context cache for LTE
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_CONTEXT_CACHE_H
#define RRIL_CONTEXT_CACHE_H


#define INIT_LTE_PDP_CONTEXT 1
#define MAX_LTE_PDP_CONTEXT  5

class CContextCache
{
#define CACHE_LEN (sizeof(m_cache)/sizeof(m_cache[0]))
public:
    CContextCache() { memset(&m_cache, 0, sizeof(m_cache)); }
    ~CContextCache()
    {
        for(UINT32 i = 1; i < CACHE_LEN; i++)
            delete [] m_cache[i];
    }


public:
    BOOL SetActivatedContext(UINT32 cid, const char* apn)
    {
        if(cid < 1 || cid > MAX_LTE_PDP_CONTEXT || apn == NULL)
               return FALSE;
        if(m_cache[cid])
            delete [] m_cache[cid];
        m_cache[cid] = new char[strlen(apn)+1];
        strcpy(m_cache[cid], apn);
        return TRUE;
    }

    BOOL RemoveActivatedContext(UINT32 cid)
    {
        if(cid < 1 || cid > MAX_LTE_PDP_CONTEXT)
               return FALSE;

        delete [] m_cache[cid];
        m_cache[cid] = NULL;
        return TRUE;
    }

    BOOL IsEmpty() const
    {
        for(UINT32 i = 1; i < CACHE_LEN; i++)
        {
            if(m_cache[i]) return FALSE;
        }
        return TRUE;
    }

    BOOL ContextExists(const char* apn) const
    {
        if(apn)
        {
            for(UINT32 i = 1; i < CACHE_LEN; i++)
            {
                // We check only APN Network identifier
                if (m_cache[i] && (strcasestr(m_cache[i], apn) == m_cache[i]))
                    return TRUE;
            }
        }
        return FALSE;
    }


private:
    char* m_cache[MAX_LTE_PDP_CONTEXT+1];
};

#endif
