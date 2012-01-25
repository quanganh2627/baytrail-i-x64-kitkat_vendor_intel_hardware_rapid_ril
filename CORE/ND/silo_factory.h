////////////////////////////////////////////////////////////////////////////
// silo_factory.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Creates the modem specific Silos
//
/////////////////////////////////////////////////////////////////////////////

class CSilo;
class CChannel;

class CSilo_Factory
{
public:
    static CSilo* GetSiloVoice(CChannel*);
    static CSilo* GetSiloSIM(CChannel*);
    static CSilo* GetSiloSMS(CChannel*);
    static CSilo* GetSiloData(CChannel*);
    static CSilo* GetSiloNetwork(CChannel*);
    static CSilo* GetSiloPhonebook(CChannel*);
    static CSilo* GetSiloMISC(CChannel*);
};

