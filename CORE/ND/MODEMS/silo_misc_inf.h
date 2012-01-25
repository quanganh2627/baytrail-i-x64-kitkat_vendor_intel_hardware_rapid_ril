/* silo_misc_inf.h
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
** Author: Frederic Predon <frederic.predon@intel.com>
*/

#ifndef RRIL_SILO_MISC_INF_H
#define RRIL_SILO_MISC_INF_H


#include "silo.h"
#include "silo_misc.h"

class CChannel;

class CSilo_MISC_INF : public CSilo_MISC
{
public:
    CSilo_MISC_INF(CChannel *pChannel);
    virtual ~CSilo_MISC_INF();
};

#endif // RRIL_SILO_MISC_INF_H
