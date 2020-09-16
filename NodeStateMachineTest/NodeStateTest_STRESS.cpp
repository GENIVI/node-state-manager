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

#include <CommonAPI/CommonAPI.hpp>
#include <v1/org/genivi/nodestatemanager/ConsumerProxy.hpp>
#include <v1/org/genivi/nodestatemanager/ConsumerProxyBase.hpp>
#include <v1/org/genivi/nodestatemanager/LifecycleControlProxy.hpp>

#include <v1/org/genivi/nodestatemachinetest/TestProxy.hpp>

#include "../NodeStateManager/NodeStateTypes.h"
#include "../NodeStateManager/NodeStateManager.h"

namespace GENIVI = v1::org::genivi;
using namespace GENIVI::nodestatemanager;

#define NUM_CONSUMER 30

DLT_DECLARE_CONTEXT(gNSMTestContext);

class NSMTest_STRESS: public testing::Test
{
protected:
   virtual void SetUp()
   {
      //Define application ID
      CommonAPI::Runtime::setProperty("LogContext", "CAPI");

      runtime = CommonAPI::Runtime::get();

      proxyLifecycleControl = runtime->buildProxy<LifecycleControlProxy>("local", "NSMLifecycleControl", "LifecycleControl");
      ASSERT_FALSE(proxyLifecycleControl == NULL);
      ASSERT_TRUE(proxyLifecycleControl->isAvailableBlocking());

      int i = 0;
      for (i = 0; i < NUM_CONSUMER; i++)
      {
         char name[255];
         sprintf(name, "Consumer_%i", i);
         proxyConsumer[i] = runtime->buildProxy<ConsumerProxy>("local", "NSMConsumer", name);
         ASSERT_FALSE(proxyConsumer[i] == NULL);
         ASSERT_TRUE(proxyConsumer[i]->isAvailableBlocking());
      }
   }

   virtual void TearDown()
   {
      // nothing to do
   }

   CommonAPI::CallStatus callStatus;
   int32_t var_i32 = 0;
   uint32_t var_ui32 = 0;
   GENIVI::NodeStateManagerTypes::NsmErrorStatus_e errorCode = GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_NotSet;

   std::shared_ptr<CommonAPI::Runtime> runtime;
   std::shared_ptr<LifecycleControlProxyDefault> proxyLifecycleControl;


   std::shared_ptr<ConsumerProxyDefault> proxyConsumer[NUM_CONSUMER];

public:

};

/********************************************************************************************
 *    main()
 ********************************************************************************************/

int main(int argc, char **argv)
{
   int retVal = 1;
   DLT_REGISTER_APP("NSMT", "Unit tests for NSM");
   DLT_REGISTER_CONTEXT(gNSMTestContext, "STR", "Context for Stress Tests");

   testing::InitGoogleTest(&argc, argv);
   retVal = RUN_ALL_TESTS();

   // unregister debug log and trace
   DLT_UNREGISTER_CONTEXT(gNSMTestContext);
   DLT_UNREGISTER_APP();
   return retVal;
}

/********************************************************************************************
 *    Stress Tests
 ********************************************************************************************/

TEST_F(NSMTest_STRESS, StressTest)
{
	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_STRESS_StressTest ==> start"));

	 RecordProperty("TestCaseID", "STRESS_StressTest");
	 RecordProperty("TestCaseDescription", "");

   int barrier = 0;
   int i = 0;
   for (i = 0; i < NUM_CONSUMER; i++)
   {
      proxyConsumer[i]->getShutdownEventsSelectiveEvent().subscribe([&](const uint32_t mode)
      {
         ASSERT_EQ(NSM_SHUTDOWNTYPE_NORMAL, mode);
         barrier = 1;
      });
      do
      {
         proxyConsumer[i]->RegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL, 2000, callStatus, errorCode);
         ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      } while (errorCode != GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   // Set NodeState to ShuttingDown
   proxyLifecycleControl->SetNodeState(GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_ShuttingDown, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   for (i = NUM_CONSUMER-1; i >=0 ; i--)
   {
      // Wait until SelectiveEvent is received...
      while (0 == barrier)
      {
         usleep(10);
      }
      barrier = 1;
      proxyConsumer[i]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   }

	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_STRESS_StressTest ==> end"));
}
