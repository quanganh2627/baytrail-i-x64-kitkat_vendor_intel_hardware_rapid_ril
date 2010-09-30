#ifndef __mem_ops_h__
#define __mem_ops_h__

#define MEM_ZERO_INIT       0x0001

///////////////////////////////////////////////////////////////////////////////
inline void *AllocBlob(UINT32 bytes, BOOL fClearMem = TRUE)
{
#ifndef __linux__
    void * pBlob = (void *)LocalAlloc(LMEM_MOVEABLE,bytes);
#else
    void * pBlob = malloc(bytes);
#endif

    if ((fClearMem) && (NULL != pBlob))
    {
        memset(pBlob, 0, bytes);
    }

    return pBlob;
}

///////////////////////////////////////////////////////////////////////////////
inline void *FreeBlob(void *pblob)
{
#ifndef __linux__
    return (void *)LocalFree(pblob);
#else
    free(pblob);
    return NULL;
#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void *ReallocBlob(void *pblob, UINT32 bytes)
{
    if (!pblob)
    {
#ifndef __linux__
        return (void *)LocalAlloc(LMEM_MOVEABLE,bytes);
#else
        return malloc(bytes);
#endif
    }

#ifndef __linux__
    return (void *)LocalReAlloc(pblob,bytes,LMEM_MOVEABLE);
#else
    return realloc(pblob, bytes);
#endif

}

#endif // __mem_ops_h__
