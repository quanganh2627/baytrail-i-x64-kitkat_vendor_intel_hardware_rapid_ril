////////////////////////////////////////////////////////////////////////////
// te_xmm7260.h
//
// Copyright (C) Intel 2013.
//
//
// Description:
//    Overlay for the IMC 7260 modem
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_TE_XMM7260_H
#define RRIL_TE_XMM7260_H

#include "te_xmm7160.h"

class CInitializer;

class CTE_XMM7260 : public CTE_XMM7160
{
public:

    enum { DEFAULT_PDN_CID = 1 };

    CTE_XMM7260(CTE& cte);
    virtual ~CTE_XMM7260();

private:

    CTE_XMM7260();

    //  Prevent assignment: Declared but not implemented.
    CTE_XMM7260(const CTE_XMM7260& rhs);  // Copy Constructor
    CTE_XMM7260& operator=(const CTE_XMM7260& rhs);  //  Assignment operator

public:
    // modem overrides

    virtual CInitializer* GetInitializer();
};

#endif
