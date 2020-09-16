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

namespace NodeStateMachineTest = v1::org::genivi::nodestatemachinetest;

namespace GENIVI = v1::org::genivi;
using namespace GENIVI::nodestatemanager;

#define NUM_CONSUMER_PARALLEL 8
#define NUM_CONSUMER_SEQUENTIAL 8

DLT_DECLARE_CONTEXT(gNSMTestContext);

class NSMTest_TIMEOUT: public testing::Test
{
protected:
   virtual void SetUp()
   {
      //Define application ID
      CommonAPI::Runtime::setProperty("LogContext", "CAPI");

      runtime = CommonAPI::Runtime::get();

      proxyConsumer_1 = runtime->buildProxy<ConsumerProxy>("local", "NSMConsumer", "Consumer_1");
      ASSERT_FALSE(proxyConsumer_1 == NULL);
      ASSERT_TRUE(proxyConsumer_1->isAvailableBlocking());

      proxyConsumer_2 = runtime->buildProxy<ConsumerProxy>("local", "NSMConsumer", "Consumer_2");
      ASSERT_FALSE(proxyConsumer_2 == NULL);
      ASSERT_TRUE(proxyConsumer_2->isAvailableBlocking());

      int i = 0;
      for (i = 0; i < NUM_CONSUMER_PARALLEL; i++)
      {
         char name[255];
         sprintf(name, "Consumer_parallel_%i", i);
         proxyConsumerParallel[i] = runtime->buildProxy<ConsumerProxy>("local", "NSMConsumer", name);
         ASSERT_FALSE(proxyConsumerParallel[i] == NULL);
         ASSERT_TRUE(proxyConsumerParallel[i]->isAvailableBlocking());
      }

      for (i = 0; i < NUM_CONSUMER_SEQUENTIAL; i++)
      {
         char name[255];
         sprintf(name, "Consumer_sequential_%i", i);
         proxyConsumerSequential[i] = runtime->buildProxy<ConsumerProxy>("local", "NSMConsumer", name);
         ASSERT_FALSE(proxyConsumerSequential[i] == NULL);
         ASSERT_TRUE(proxyConsumerSequential[i]->isAvailableBlocking());
      }

      proxyLifecycleControl = runtime->buildProxy<LifecycleControlProxy>("local", "NSMLifecycleControl", "LifecycleControl");
      proxyTest = runtime->buildProxy<NodeStateMachineTest::TestProxy>("local", "NSMTest", "Test");

      ASSERT_FALSE(proxyConsumer_1 == NULL);
      ASSERT_FALSE(proxyConsumer_2 == NULL);
      ASSERT_FALSE(proxyLifecycleControl == NULL);
      ASSERT_FALSE(proxyTest == NULL);
      ASSERT_TRUE(proxyConsumer_1->isAvailableBlocking());
      ASSERT_TRUE(proxyConsumer_2->isAvailableBlocking());
      ASSERT_TRUE(proxyLifecycleControl->isAvailableBlocking());
      ASSERT_TRUE(proxyTest->isAvailableBlocking());
   }

   virtual void TearDown()
   {
      int i = 0;
      proxyConsumer_1 = NULL;
      proxyConsumer_2 = NULL;
      proxyLifecycleControl = NULL;
      proxyTest = NULL;

      for (i = 0; i < NUM_CONSUMER_PARALLEL; i++)
      {
         proxyConsumerParallel[i] = NULL;
      }

      for (i = 0; i < NUM_CONSUMER_SEQUENTIAL; i++)
      {
         proxyConsumerSequential[i] = NULL;
      }
   }

   CommonAPI::CallStatus callStatus;
   int32_t var_i32 = 0;
   uint32_t var_ui32 = 0;
   GENIVI::NodeStateManagerTypes::NsmErrorStatus_e errorCode = GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_NotSet;

   std::shared_ptr<CommonAPI::Runtime> runtime;
   std::shared_ptr<ConsumerProxyDefault> proxyConsumer_1;
   std::shared_ptr<ConsumerProxyDefault> proxyConsumer_2;
   std::shared_ptr<ConsumerProxyDefault> proxyConsumerParallel[NUM_CONSUMER_PARALLEL];
   std::shared_ptr<ConsumerProxyDefault> proxyConsumerSequential[NUM_CONSUMER_SEQUENTIAL];

   std::shared_ptr<LifecycleControlProxyDefault> proxyLifecycleControl;
   std::shared_ptr<NodeStateMachineTest::TestProxyDefault> proxyTest;

   uint32_t expectedModeSequential = NSM_SHUTDOWNTYPE_NORMAL;
   uint32_t expectedModeParallel = NSM_SHUTDOWNTYPE_NORMAL | NSM_SHUTDOWNTYPE_PARALLEL;

   int barrierSequential = 0;
   int barrierParallel = 0;

public:

};

/********************************************************************************************
 *    main()
 ********************************************************************************************/

int main(int argc, char **argv)
{
   int retVal = 1;
   DLT_REGISTER_APP("NSMT", "Unit tests for NSM");
   DLT_REGISTER_CONTEXT(gNSMTestContext,"TO","Context for Timeout Tests");

   testing::InitGoogleTest(&argc, argv);
   retVal = RUN_ALL_TESTS();

   // unregister debug log and trace
   DLT_UNREGISTER_CONTEXT(gNSMTestContext);
   DLT_UNREGISTER_APP();
   return retVal;
}

/********************************************************************************************
 *    Timeout Tests
 ********************************************************************************************/

TEST_F(NSMTest_TIMEOUT, TimeoutTest)
{
   RecordProperty("TestCaseID", "TIMEOUT_TimeoutTest");
   RecordProperty("TestCaseDescription", "");

   int barrier_1 = 0;
   int barrier_2 = 0;

   uint32_t expected_mode = NSM_SHUTDOWNTYPE_NOT;
   // Initialize

   proxyConsumer_1->getShutdownEventsSelectiveEvent().subscribe([&](const uint32_t mode)
   {
      ASSERT_EQ(mode, expected_mode);
      barrier_1 = 1;
   });
   do
   {
      proxyConsumer_1->RegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL, 5000, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   } while (errorCode != GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer_2->getShutdownEventsSelectiveEvent().subscribe([&](const uint32_t mode)
   {
      ASSERT_EQ(mode, expected_mode);
      barrier_2 = 1;
   });
   do
   {
      proxyConsumer_2->RegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL, 750, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   } while (errorCode != GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   expected_mode = NSM_SHUTDOWNTYPE_NORMAL;

   // Set NodeState to ShuttingDown
   proxyLifecycleControl->SetNodeState(GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_ShuttingDown, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   // delay so proxyConsumer_2 will timeout
   sleep(1);

   // NOTE: The error is printed in DLT. This log is currently checked in run_tests.sh

   while(0 == barrier_2) { usleep(10); } barrier_2 = 0;

   proxyConsumer_2->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   // consumer_2 should get an error because he is to late
   ASSERT_EQ(NsmErrorStatus_WrongClient, errorCode);


   expected_mode = NSM_SHUTDOWNTYPE_RUNUP;
   proxyLifecycleControl->SetNodeState(GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_BaseRunning, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   // Shutdown call
   while(0 == barrier_1) { usleep(10); } barrier_1 = 0;
   proxyConsumer_1->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(NsmErrorStatus_Ok, errorCode);

   // Runup call
   while(0 == barrier_1) { usleep(10); } barrier_1 = 0;
   proxyConsumer_1->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(NsmErrorStatus_Ok, errorCode);

   // NOTE: The error is printed in DLT. This log is currently checked in run_tests.sh

   while(0 == barrier_2) { usleep(10); } barrier_2 = 0;

   // delay so proxyConsumer_2 will timeout
   sleep(1);

   proxyConsumer_2->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   // consumer_2 should get an error because he is to late
   ASSERT_EQ(NsmErrorStatus_WrongClient, errorCode);

   proxyConsumer_1->UnRegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer_2->UnRegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_TIMEOUT_TimeoutTest ==> end"));
}

TEST_F(NSMTest_TIMEOUT, TimeoutTestParallel)
{
   DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_TIMEOUT_TimeoutTestParallel ==> start"));

   RecordProperty("TestCaseID", "TIMEOUT_TimeoutTestParallel");
   RecordProperty("TestCaseDescription", "");

   int i = 0;
   for (i = 0; i < NUM_CONSUMER_PARALLEL; i++)
   {
      proxyConsumerParallel[i]->getShutdownEventsSelectiveEvent().subscribe([&](const uint32_t mode)
      {
         ASSERT_EQ(mode, expectedModeParallel);
         __sync_fetch_and_add(&barrierParallel, 1);
      });
      do
      {
         proxyConsumerParallel[i]->RegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL | NSM_SHUTDOWNTYPE_PARALLEL, 500, callStatus, errorCode);
         ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      } while (errorCode != GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   for (i = 0; i < NUM_CONSUMER_SEQUENTIAL; i++)
   {
      proxyConsumerSequential[i]->getShutdownEventsSelectiveEvent().subscribe([&](const uint32_t mode)
      {
         ASSERT_EQ(mode, expectedModeSequential);
         barrierSequential = 1;
      });
      do
      {
         proxyConsumerSequential[i]->RegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL, 500, callStatus, errorCode);
         ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      } while (errorCode != GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   proxyLifecycleControl->SetNodeState(GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_ShuttingDown, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   // Wait until SelectiveEvent is received...
   while (NUM_CONSUMER_PARALLEL > barrierParallel)
   {
      usleep(10);
   }

   for (i = 0; i < NUM_CONSUMER_PARALLEL - 1; i++)
   {
      proxyConsumerParallel[i]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   usleep(750*1000);
   proxyConsumerParallel[NUM_CONSUMER_PARALLEL - 1]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_WrongClient);

   /* Don't complete all so nsm can runup again */
   for (i = NUM_CONSUMER_SEQUENTIAL - 1; i > 1; i--)
   {
      // Wait until SelectiveEvent is received...
      while (0 == barrierSequential) { usleep(10); }
      barrierSequential = 0;
      proxyConsumerSequential[i]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   usleep(750*1000);
   proxyConsumerSequential[1]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_WrongClient);

   barrierParallel = 0;

   // Set NodeState to BaseRunning
   proxyLifecycleControl->SetNodeState(GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_BaseRunning, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   while (0 == barrierSequential) { usleep(10); }
   barrierSequential = 0;

   expectedModeParallel = NSM_SHUTDOWNTYPE_RUNUP | NSM_SHUTDOWNTYPE_PARALLEL;
   expectedModeSequential = NSM_SHUTDOWNTYPE_RUNUP;

   proxyConsumerSequential[0]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   for (i = 0; i < NUM_CONSUMER_SEQUENTIAL - 1; i++)
   {
      // Wait until SelectiveEvent is received...
      while (0 == barrierSequential) { usleep(10); }
      barrierSequential = 0;
      proxyConsumerSequential[i]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   usleep(750 * 1000);
   proxyConsumerSequential[NUM_CONSUMER_SEQUENTIAL - 1]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_WrongClient);

   while (NUM_CONSUMER_PARALLEL > barrierParallel)
   {
      usleep(10);
   }

   for (i = 0; i < NUM_CONSUMER_PARALLEL-1; i++)
   {
      proxyConsumerParallel[i]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   usleep(750*1000);
   proxyConsumerParallel[NUM_CONSUMER_PARALLEL - 1]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_WrongClient);

   for (i = 0; i < NUM_CONSUMER_PARALLEL; i++)
   {
      proxyConsumerParallel[i]->UnRegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL | NSM_SHUTDOWNTYPE_PARALLEL, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   for (i = 0; i < NUM_CONSUMER_SEQUENTIAL; i++)
   {
      proxyConsumerSequential[i]->UnRegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_TIMEOUT_TimeoutTestParallel ==> end"));
}

TEST_F(NSMTest_TIMEOUT, TestNoTimeout)
{
   DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_TIMEOUT_TestNoTimeout ==> start"));

   RecordProperty("TestCaseID", "TIMEOUT_TestNoTimeout");
   RecordProperty("TestCaseDescription", "");

   int i = 0;
   for (i = 0; i < NUM_CONSUMER_PARALLEL; i++)
   {
      proxyConsumerParallel[i]->getShutdownEventsSelectiveEvent().subscribe([&](const uint32_t mode)
      {
         ASSERT_EQ(mode, expectedModeParallel);
         __sync_fetch_and_add(&barrierParallel, 1);
      });
      do
      {
         proxyConsumerParallel[i]->RegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL | NSM_SHUTDOWNTYPE_PARALLEL, 500, callStatus, errorCode);
         ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      } while (errorCode != GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   for (i = 0; i < NUM_CONSUMER_SEQUENTIAL; i++)
   {
      proxyConsumerSequential[i]->getShutdownEventsSelectiveEvent().subscribe([&](const uint32_t mode)
      {
         ASSERT_EQ(mode, expectedModeSequential);
         barrierSequential = 1;
      });
      do
      {
         proxyConsumerSequential[i]->RegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL, 500, callStatus, errorCode);
         ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      } while (errorCode != GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   // Set NodeState to ShuttingDown
   proxyLifecycleControl->SetNodeState(GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_ShuttingDown, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   // Wait until SelectiveEvent is received...
   while (NUM_CONSUMER_PARALLEL > barrierParallel)
   {
      usleep(10);
   }

   for (i = 0; i < NUM_CONSUMER_PARALLEL; i++)
   {
      proxyConsumerParallel[i]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   for (i = NUM_CONSUMER_SEQUENTIAL - 1; i >= 0; i--)
   {
      // Wait until SelectiveEvent is received...
      while (0 == barrierSequential) { usleep(10); }
      barrierSequential = 0;

      proxyConsumerSequential[i]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   sleep(1);

   expectedModeParallel = NSM_SHUTDOWNTYPE_RUNUP | NSM_SHUTDOWNTYPE_PARALLEL;
   expectedModeSequential = NSM_SHUTDOWNTYPE_RUNUP;

   // Set NodeState to ShuttingDown
   barrierSequential = 0;
   barrierParallel = 0;
   proxyLifecycleControl->SetNodeState(GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_BaseRunning, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   for (i = 0; i < NUM_CONSUMER_SEQUENTIAL; i++)
   {
      // Wait until SelectiveEvent is received...
      while (0 == barrierSequential) { usleep(10); }
      barrierSequential = 0;

      proxyConsumerSequential[i]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   while (NUM_CONSUMER_PARALLEL > barrierParallel)
   {
      usleep(10);
   }

   for (i = 0; i < NUM_CONSUMER_PARALLEL; i++)
   {
      proxyConsumerParallel[i]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   for (i = 0; i < NUM_CONSUMER_PARALLEL; i++)
   {
      proxyConsumerParallel[i]->UnRegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL | NSM_SHUTDOWNTYPE_PARALLEL, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   for (i = 0; i < NUM_CONSUMER_SEQUENTIAL; i++)
   {
      proxyConsumerSequential[i]->UnRegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_TIMEOUT_TestNoTimeout ==> end"));
}

TEST_F(NSMTest_TIMEOUT, TimeoutTestCollective)
{
   DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_TIMEOUT_TimeoutTestCollective ==> start"));

   RecordProperty("TestCaseID", "TIMEOUT_TimeoutTestCollective");
   RecordProperty("TestCaseDescription", "");

   int i = 0;
   GENIVI::NodeStateManagerTypes::NsmNodeState_e nodeState;
   expectedModeSequential = NSM_SHUTDOWNTYPE_FAST;
   expectedModeParallel = NSM_SHUTDOWNTYPE_FAST | NSM_SHUTDOWNTYPE_PARALLEL;

   proxyConsumer_1->getShutdownEventsSelectiveEvent().subscribe([&](const uint32_t mode)
   {
      ASSERT_EQ(mode, expectedModeSequential);
      barrierSequential = 1;
   });
   do
   {
      proxyConsumer_1->RegisterShutdownClient(NSM_SHUTDOWNTYPE_FAST, 61000, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   } while (errorCode != GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   /* This client shouldn't count to the collective timeout as it unregisters */
   proxyConsumer_1->UnRegisterShutdownClient(NSM_SHUTDOWNTYPE_FAST, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   for (i = 0; i < NUM_CONSUMER_PARALLEL; i++)
   {
      proxyConsumerParallel[i]->getShutdownEventsSelectiveEvent().subscribe([&](const uint32_t mode)
      {
         ASSERT_EQ(mode, expectedModeParallel);
         __sync_fetch_and_add(&barrierParallel, 1);
      });
      do
      {
         proxyConsumerParallel[i]->RegisterShutdownClient(NSM_SHUTDOWNTYPE_FAST | NSM_SHUTDOWNTYPE_PARALLEL, 61000, callStatus, errorCode);
         ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      } while (errorCode != GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   for (i = 0; i < NUM_CONSUMER_SEQUENTIAL; i++)
   {
      proxyConsumerSequential[i]->getShutdownEventsSelectiveEvent().subscribe([&](const uint32_t mode)
      {
         ASSERT_EQ(mode, expectedModeSequential);
         barrierSequential = 1;
      });
      do
      {
         proxyConsumerSequential[i]->RegisterShutdownClient(NSM_SHUTDOWNTYPE_FAST, 61000, callStatus, errorCode);
         ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      } while (errorCode != GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   // Set NodeState to ShuttingDown
   proxyLifecycleControl->SetNodeState(GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_FastShutdown, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   // Wait until SelectiveEvent is received...
   while (NUM_CONSUMER_PARALLEL > barrierParallel)
   {
      usleep(10);
   }

   for (i = 0; i < NUM_CONSUMER_PARALLEL-1; i++)
   {
      proxyConsumerParallel[i]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   sleep(3);

   /* This client should get an error as it is to late */
   proxyConsumerParallel[NUM_CONSUMER_PARALLEL-1]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_WrongClient);

   for (i = NUM_CONSUMER_SEQUENTIAL - 1; i > 1; i--)
   {
      // Wait until SelectiveEvent is received...
      while (0 == barrierSequential) { usleep(10); }
      barrierSequential = 0;

      proxyConsumerSequential[i]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   /* Trigger collective timeout of 3 seconds on fast shutdown */
   sleep(4);

   /* NodeState should be NsmNodeState_Shutdown already */
   proxyConsumerSequential[1]->GetNodeState(callStatus, nodeState, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   ASSERT_EQ(nodeState, GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_Shutdown);

   /* This client should get an error as it is to late */
   proxyConsumerSequential[1]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_WrongClient);

   sleep(1);

   /* barrierSequential should still be 1 because it has not been set to 0 by proxyConsumerSequential[0] */
   ASSERT_EQ(1, barrierSequential);

   expectedModeParallel = NSM_SHUTDOWNTYPE_RUNUP | NSM_SHUTDOWNTYPE_PARALLEL;
   expectedModeSequential = NSM_SHUTDOWNTYPE_RUNUP;

   // Set NodeState to ShuttingDown
   barrierSequential = 0;
   barrierParallel = 0;
   proxyLifecycleControl->SetNodeState(GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_BaseRunning, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   /* start at 1 because proxyConsumerSequential[0] should have never been informed about shutdown */
   for (i = 1; i < NUM_CONSUMER_SEQUENTIAL; i++)
   {
      // Wait until SelectiveEvent is received...
      while (0 == barrierSequential) { usleep(10); }
      barrierSequential = 0;

      proxyConsumerSequential[i]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   while (NUM_CONSUMER_PARALLEL > barrierParallel)
   {
      usleep(10);
   }

   for (i = 0; i < NUM_CONSUMER_PARALLEL; i++)
   {
      proxyConsumerParallel[i]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   for (i = 0; i < NUM_CONSUMER_PARALLEL; i++)
   {
      proxyConsumerParallel[i]->UnRegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL | NSM_SHUTDOWNTYPE_PARALLEL, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   for (i = 0; i < NUM_CONSUMER_SEQUENTIAL; i++)
   {
      proxyConsumerSequential[i]->UnRegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_TIMEOUT_TimeoutTestCollective ==> end"));
}

TEST_F(NSMTest_TIMEOUT, TimeoutTestEarlyTimeout)
{
   DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_TIMEOUT_TimeoutTestEarlyTimeout ==> start"));

   RecordProperty("TestCaseID", "TIMEOUT_TimeoutTestEarlyTimeout");
   RecordProperty("TestCaseDescription", "");

   int i = 0;
   GENIVI::NodeStateManagerTypes::NsmNodeState_e nodeState;
   expectedModeSequential = NSM_SHUTDOWNTYPE_NORMAL;
   expectedModeParallel = NSM_SHUTDOWNTYPE_NORMAL | NSM_SHUTDOWNTYPE_PARALLEL;

   int timeouts[] = { 500, 1000, 2500, 5000, 10000, 15000, 20000, 25000 };

   for (i = 0; i < NUM_CONSUMER_PARALLEL; i++)
   {
      proxyConsumerParallel[i]->getShutdownEventsSelectiveEvent().subscribe([&](const uint32_t mode)
      {
         ASSERT_EQ(mode, expectedModeParallel);
         __sync_fetch_and_add(&barrierParallel, 1);
      });
      do
      {
         proxyConsumerParallel[i]->RegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL | NSM_SHUTDOWNTYPE_PARALLEL, timeouts[i], callStatus, errorCode);
         ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      } while (errorCode != GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }
   for (i = 0; i < NUM_CONSUMER_SEQUENTIAL; i++)
   {
      proxyConsumerSequential[i]->getShutdownEventsSelectiveEvent().subscribe([&](const uint32_t mode)
      {
         ASSERT_EQ(mode, expectedModeSequential);
         barrierSequential = 1;
      });
      do
      {
         proxyConsumerSequential[i]->RegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL, 1000, callStatus, errorCode);
         ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      } while (errorCode != GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   clock_t begin = clock();

   // Set NodeState to ShuttingDown
   proxyLifecycleControl->SetNodeState(GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_ShuttingDown, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   // Wait until SelectiveEvent is received...
   while (NUM_CONSUMER_PARALLEL > barrierParallel)
   {
      usleep(10);
   }

   // 500ms timeout
   proxyConsumerParallel[0]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   sleep(2);
   // 1000ms timeout -> client 1 should be to late

   // 2500ms timeout
   proxyConsumerParallel[2]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   // 5000ms timeout
   proxyConsumerParallel[3]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   // 10000ms timeout
   proxyConsumerParallel[4]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   // 15000ms timeout
   proxyConsumerParallel[5]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   // 20000ms timeout
   proxyConsumerParallel[6]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   // 25000ms timeout
   proxyConsumerParallel[7]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   for (i = NUM_CONSUMER_SEQUENTIAL - 1; i >= 0; i--)
   {
      // Wait until SelectiveEvent is received...
      while (0 == barrierSequential) { usleep(10); }
      barrierSequential = 0;

      proxyConsumerSequential[i]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }
   sleep(1);


   /* NodeState should be NsmNodeState_Shutdown already */
   proxyConsumerSequential[1]->GetNodeState(callStatus, nodeState, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   ASSERT_EQ(nodeState, GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_Shutdown);

   clock_t end = clock();

   /* The only client that registered with 1000ms timeout doesn't respond -> bigger timeouts should not count to shutdown time */
   ASSERT_TRUE((double(end - begin) / CLOCKS_PER_SEC) < 5);

   sleep(1);

   expectedModeParallel = NSM_SHUTDOWNTYPE_RUNUP | NSM_SHUTDOWNTYPE_PARALLEL;
   expectedModeSequential = NSM_SHUTDOWNTYPE_RUNUP;

   // Set NodeState to BaseRunning
   barrierSequential = 0;
   barrierParallel = 0;
   proxyLifecycleControl->SetNodeState(GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_BaseRunning, callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   for (i = 0; i < NUM_CONSUMER_SEQUENTIAL; i++)
   {
      // Wait until SelectiveEvent is received...
      while (0 == barrierSequential) { usleep(10); }
      barrierSequential = 0;

      proxyConsumerSequential[i]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   while (NUM_CONSUMER_PARALLEL - 1 > barrierParallel) // Client 1 had a time out so he will not be informed this time
   {
      usleep(10);
   }

   for (i = 0; i < NUM_CONSUMER_PARALLEL; i++)
   {
      if(i == 1) continue; // Client 1 had a time out so he will not be informed this time
      proxyConsumerParallel[i]->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   for (i = 0; i < NUM_CONSUMER_PARALLEL; i++)
   {
      proxyConsumerParallel[i]->UnRegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL | NSM_SHUTDOWNTYPE_PARALLEL, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   for (i = 0; i < NUM_CONSUMER_SEQUENTIAL; i++)
   {
      proxyConsumerSequential[i]->UnRegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL, callStatus, errorCode);
      ASSERT_TRUE(callStatus == CommonAPI::CallStatus::SUCCESS);
      ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   }

   DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_TIMEOUT_TimeoutTestEarlyTimeout ==> end"));
}

