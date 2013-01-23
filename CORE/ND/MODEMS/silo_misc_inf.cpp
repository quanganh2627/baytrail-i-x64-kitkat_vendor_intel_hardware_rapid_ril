/* silo_misc_inf.cpp
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

#include "types.h"
#include "silo.h"
#include "silo_misc.h"
#include "silo_misc_inf.h"
#include "rillog.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

CSilo_MISC_INF::CSilo_MISC_INF(CChannel* pChannel)
: CSilo_MISC(pChannel)
{
    RIL_LOG_VERBOSE("CSilo_MISC_INF::CSilo_MISC_INF() - Enter\r\n");

    RIL_LOG_VERBOSE("CSilo_MISC_INF::CSilo_MISC_INF() - Exit\r\n");
}

//
//
CSilo_MISC_INF::~CSilo_MISC_INF()
{
    RIL_LOG_VERBOSE("CSilo_MISC_INF::~CSilo_MISC_INF() - Enter / Exit\r\n");
}
