/**********************************************************************************************************************
 *
 * Copyright (C) 2017 BMW AG
 *
 * Implements tests for NSM
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 **********************************************************************************************************************/

#include <gtest/gtest.h>
#include <iostream>
#include <dlt.h>

#include "../NodeStateManager/Watchdog.hpp"

DLT_DECLARE_CONTEXT(NsmContext);

class NSMTest_WATCHDOG: public testing::Test
{
protected:
   virtual void SetUp()
   {

   }

   virtual void TearDown()
   {

   }

public:

};

/********************************************************************************************
 *    main()
 ********************************************************************************************/

int main(int argc, char **argv)
{
   int retVal = 1;
   DLT_REGISTER_APP("NSMT", "Unit tests for NSM");
   DLT_REGISTER_CONTEXT(NsmContext,"WD","Context for Watchdog Tests");

   testing::InitGoogleTest(&argc, argv);
   retVal = RUN_ALL_TESTS();

   // unregister debug log and trace
   DLT_UNREGISTER_CONTEXT(NsmContext);
   DLT_UNREGISTER_APP();
   return retVal;
}

/********************************************************************************************
 *    Timeout Tests
 ********************************************************************************************/

TEST_F(NSMTest_WATCHDOG, WatchdogTimeout)
{
   RecordProperty("TestCaseID", "WatchdogTimeout");
   RecordProperty("TestCaseDescription", "");

   DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("run_test_WatchdogTimeout ==> start"));

   NSMTriggerWatchdog(NsmWatchdogState_Active);
   ASSERT_TRUE(NSMWatchdogIsHappy());
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   ASSERT_TRUE(NSMWatchdogIsHappy());
   ASSERT_FALSE(NSMWatchdogIsHappy());
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   ASSERT_TRUE(NSMWatchdogIsHappy());
   NSMUnregisterWatchdog();
   ASSERT_TRUE(NSMWatchdogIsHappy());
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   ASSERT_TRUE(NSMWatchdogIsHappy());
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   NSMTriggerWatchdog(NsmWatchdogState_Sleep);
   ASSERT_TRUE(NSMWatchdogIsHappy());
   ASSERT_TRUE(NSMWatchdogIsHappy());
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   ASSERT_TRUE(NSMWatchdogIsHappy());
   ASSERT_FALSE(NSMWatchdogIsHappy());

   NSMTriggerWatchdog(NsmWatchdogState_Sleep);
   NSMTriggerSpecificWatchdog(NsmWatchdogState_Active, 1);
   ASSERT_TRUE(NSMWatchdogIsHappy());
   ASSERT_FALSE(NSMWatchdogIsHappy());

   DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("run_test_WatchdogTimeout ==> end"));
}


