/* silo_misc.h
**
** Copyright (C) Intel 2012
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** Description:
**    Defines the CSilo_MISC class, which provides response handlers and
**    parsing functions for the MISC features.
**
** Author: Frederic Predon <frederic.predon@intel.com>
*/

#ifndef RRIL_SILO_MISC_H
#define RRIL_SILO_MISC_H


#include "silo.h"


class CSilo_MISC : public CSilo
{
public:
    CSilo_MISC(CChannel* pChannel);
    virtual ~CSilo_MISC();

protected:
    //  Parse notification functions here.
    virtual BOOL ParseXDRVI(CResponse* const pResponse, const char*& rszPointer);

};

#endif // RRIL_SILO_MISC_H
