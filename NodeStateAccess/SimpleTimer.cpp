/**********************************************************************************************************************
 *
 * Copyright (C) 2017 BMW AG
 *
 * Implements a simple timer which calls a method after a specified timeout in ms.
 * This timer can be canceled using "cancelTimer()"
 *
 * This source file is a part of the NodeStateAccess library (NSMA).
 * The architecture requires that the NodeStateManager (NSM) is independent from the D-Bus binding and code generated by
 * "CommonAPI". Therefore, the D-Bus communication and generated CommonAPI objects are handled inside of this library.
 * The library offers the NSM an interface to use objects generated via CommonAPI.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 **********************************************************************************************************************/

#include "SimpleTimer.h"
#include <unistd.h>

void SimpleTimer::cancelTimer()
{
   std::unique_lock<std::mutex> lock(mMutex);
   timerLock--;
   if(timerLock <= 0)
   {
      mCondVar.notify_all();
   }
   lock.unlock();
}

void SimpleTimer::stopTimer()
{
   std::unique_lock<std::mutex> lock(mMutex);
   timerLock = 0;
   mCondVar.notify_all();
   lock.unlock();
}

void SimpleTimer::joinTimer()
{
   std::unique_lock<std::mutex> lock(mMutex);
   while(joined == false)
   {
      NSMTriggerWatchdog(NsmWatchdogState_Sleep);
      mCondVarJoin.wait(lock);
      NSMTriggerWatchdog(NsmWatchdogState_Active);
   }
   lock.unlock();
}

SimpleTimer::~SimpleTimer()
{
   joinTimer();
}
