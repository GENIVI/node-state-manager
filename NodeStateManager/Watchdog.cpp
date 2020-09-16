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

#include "Watchdog.hpp"
#include <thread>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <dlt.h>
#include <sys/types.h>
#include <sys/syscall.h>

static std::mutex mutex;
static std::unordered_map<long int, NsmWatchdogState_e> threads;
static std::unordered_map<long int, long int> specificThreads;


DLT_IMPORT_CONTEXT(NsmContext);

void NSMTriggerSpecificWatchdog(NsmWatchdogState_e state, const long int thread)
{
  std::lock_guard<std::mutex> lock(mutex);
  long int threadID = thread * (-1);

  if(threadID == 0)
  {
    threadID = (long int)syscall(SYS_gettid);
  }
  else
  {
    auto search = specificThreads.find(threadID);
    if (search != specificThreads.end())
    {
      search->second = (long int) syscall(SYS_gettid); // Update last thread id of specific thread
    }
    else
    {
      specificThreads.insert(std::make_pair(threadID, (long int) syscall(SYS_gettid)));
    }
  }

  auto search = threads.find(threadID);
  if(search != threads.end()) {
    search->second = state;
  }
  else {
    threads.insert(std::make_pair(threadID, state));
  }
}

void NSMTriggerWatchdog(NsmWatchdogState_e state)
{
  NSMTriggerSpecificWatchdog(state, 0);
}

void NSMUnregisterSpecificWatchdog(const long int thread)
{
  std::lock_guard<std::mutex> lock(mutex);
  long int threadID = thread * (-1);

  if(threadID == 0)
    threadID = (long int)syscall(SYS_gettid);

  threads.erase(threadID);
}

void NSMUnregisterWatchdog()
{
  NSMUnregisterSpecificWatchdog(0);
}

bool NSMWatchdogIsHappy()
{
  bool retVal = true;

  std::lock_guard<std::mutex> lock(mutex);
  for(auto &iter: threads)
  {
      if(iter.second == NsmWatchdogState_Unknown)
      {
        DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("Watchdog timeout, thread"), DLT_INT64(iter.first), DLT_STRING("is in an unknown state!"));
        if(iter.first < 0)
        {
          auto search = specificThreads.find(iter.first);
          if (search != specificThreads.end())
          {
            DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("Thread id of specific thread"), DLT_INT64(iter.first), DLT_STRING("is"), DLT_INT64(search->second));
            search->second = (long int) syscall(SYS_gettid);
          }
        }
        retVal = false;
        usleep(100000); //Sleep 100ms to make sure the DLT is send
        break;
      }
      else if(iter.second == NsmWatchdogState_Active)
      {
        iter.second = NsmWatchdogState_Unknown;
      }
  }

  return retVal;
}
