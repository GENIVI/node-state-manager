/**********************************************************************************************************************
*
* Copyright (C) 2017 BMW AG
*
* Implementation of the watchdog helper function
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
**********************************************************************************************************************/
#ifndef NODESTATEMANAGER_WATCHDOG_HPP_
#define NODESTATEMANAGER_WATCHDOG_HPP_

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"

typedef enum _NsmWatchdogState_e {
  NsmWatchdogState_Unknown,
  NsmWatchdogState_Active,
  NsmWatchdogState_Sleep
}NsmWatchdogState_e;

typedef enum _NsmWatchdogThread_e
{
  NsmThreadAuto,
  NsmThreadSDMG
}NsmWatchdogThread_e;

void NSMTriggerWatchdog(NsmWatchdogState_e state);

void NSMTriggerSpecificWatchdog(NsmWatchdogState_e state, const long int thread);

void NSMUnregisterWatchdog();

bool NSMWatchdogIsHappy();

#ifdef __cplusplus
}
#endif

#endif /* NODESTATEMANAGER_WATCHDOG_HPP_ */
