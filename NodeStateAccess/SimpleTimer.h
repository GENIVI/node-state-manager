/**********************************************************************************************************************
 *
 * Copyright (C) 2017 BMW AG
 *
 * Interface between NodeStateManager and IPC
 *
 * This source file is a part of the NodeStateAccess library (NSMA).
 * The architecture requires that the NodeStateManager (NSM) is independent from the CommonAPI binding.
 * Therefore, the CommonAPI communication is handled inside of this library.
 * The library offers the NSM an interface to use objects generated via CommonAPI.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 **********************************************************************************************************************/

#ifndef NODESTATEACCESS_SIMPLETIMER_H_
#define NODESTATEACCESS_SIMPLETIMER_H_

#include <functional>
#include <chrono>
#include <future>
#include <cstdio>
#include <iostream>
#include "Watchdog.hpp"

class SimpleTimer
{
private:
   std::mutex mMutex;
   std::condition_variable mCondVar;
   std::condition_variable mCondVarJoin;
   bool joined = false;
public:
   std::shared_ptr<SimpleTimer> self;
   volatile uint timerLock;
   template<class callable, class ... arguments>
      SimpleTimer(int timeOut, int timerLocks, callable&& f, arguments&&... args)
      {
         std::function<typename std::result_of<callable(arguments...)>::type()> task(std::bind(std::forward<callable>(f), std::forward<arguments>(args)...));
         timerLock = timerLocks;

         std::thread([this, timeOut, task]()
         {
            NSMTriggerWatchdog(NsmWatchdogState_Active);
            std::unique_lock<std::mutex> lock(mMutex);

            while (timerLock > 0)
            {
               NSMTriggerWatchdog(NsmWatchdogState_Sleep);
               if (std::cv_status::timeout == mCondVar.wait_until(lock, std::chrono::steady_clock::now() + std::chrono::milliseconds(timeOut)))
               {
                  NSMTriggerWatchdog(NsmWatchdogState_Active);
                  break;
               }
               NSMTriggerWatchdog(NsmWatchdogState_Active);
            }

            if(timerLock > 0)
            {
               lock.unlock();
               task();
               lock.lock();
            }
            NSMUnregisterWatchdog();

            joined = true;
            mCondVarJoin.notify_all();
            lock.unlock();

            /* Set own pointer to NULL - timer can be automatically deleted from now on */
            self = NULL;
         }).detach();
      };

   ~SimpleTimer();

   /**
    * SimpleTimer starts a thread which executes a callback after timeOut milliseconds.
    * It is possible to cancel the execution of this callback by calling cancel.
    * The constructor is hidden. This way it is ensured that all timers are created with new.
    * After the timer has finished it will destroy itself.
    */
   template<class callable, class ... arguments>
   static std::shared_ptr<SimpleTimer> CreateTimer(int timeOut, int timerLocks, callable&& f, arguments&&... args)
   {
      std::shared_ptr<SimpleTimer> timer = std::make_shared<SimpleTimer>(timeOut, timerLocks, std::forward<callable>(f), std::forward<arguments>(args)...);
      /* Save timer in timer itself so it will only be cleaned when thread has finished*/
      timer->self = timer;
      return timer;
   }

   /**
    * This function decrements the timerLocks by one. timerLocks has been initialized in CreateTimer.
    * When this locks have reached zero the timer is considered as canceled/deprecated
    * and the specified task will not be executed anymore.
    */
   void cancelTimer();
   void stopTimer();
   void joinTimer();

};

#endif /* NODESTATEACCESS_SIMPLETIMER_H_ */
