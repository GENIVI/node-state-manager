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

DLT_DECLARE_CONTEXT(gNSMTestContext);

class NSMTest_GENIVI: public testing::Test
{
protected:
   virtual void SetUp()
   {
      //Define application ID
      CommonAPI::Runtime::setProperty("LogContext", "CAPI");

      GENIVI::NodeStateManagerTypes::NsmErrorStatus_e eCode = GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_NotSet;
      GENIVI::NodeStateManagerTypes::NsmApplicationMode_e appMode;

      runtime = CommonAPI::Runtime::get();
      proxyConsumer = runtime->buildProxy<ConsumerProxy>("local", "NSMConsumer", "Consumer");
      proxyConsumer_1 = runtime->buildProxy<ConsumerProxy>("local", "NSMConsumer", "Consumer_1");
      proxyConsumer_2 = runtime->buildProxy<ConsumerProxy>("local", "NSMConsumer", "Consumer_2");
      proxyConsumer_3 = runtime->buildProxy<ConsumerProxy>("local", "NSMConsumer", "Consumer_3");
      proxyConsumer_4 = runtime->buildProxy<ConsumerProxy>("local", "NSMConsumer", "Consumer_4");
      proxyConsumer_5 = runtime->buildProxy<ConsumerProxy>("local", "NSMConsumer", "Consumer_5");
      proxyLifecycleControl = runtime->buildProxy<LifecycleControlProxy>("local", "NSMLifecycleControl", "LifecycleControl");
      proxyTest = runtime->buildProxy<NodeStateMachineTest::TestProxy>("local", "NSMTest", "Test");

      ASSERT_FALSE(proxyConsumer == NULL);
      ASSERT_FALSE(proxyConsumer_1 == NULL);
      ASSERT_FALSE(proxyConsumer_2 == NULL);
      ASSERT_FALSE(proxyConsumer_3 == NULL);
      ASSERT_FALSE(proxyConsumer_4 == NULL);
      ASSERT_FALSE(proxyConsumer_5 == NULL);
      ASSERT_FALSE(proxyLifecycleControl == NULL);

      ASSERT_FALSE(proxyTest == NULL);

      ASSERT_TRUE(proxyConsumer->isAvailableBlocking());
      ASSERT_TRUE(proxyConsumer_1->isAvailableBlocking());
      ASSERT_TRUE(proxyConsumer_2->isAvailableBlocking());
      ASSERT_TRUE(proxyConsumer_3->isAvailableBlocking());
      ASSERT_TRUE(proxyConsumer_4->isAvailableBlocking());
      ASSERT_TRUE(proxyConsumer_5->isAvailableBlocking());

      ASSERT_TRUE(proxyLifecycleControl->isAvailableBlocking());
      ASSERT_TRUE(proxyTest->isAvailableBlocking());


      proxyConsumer->GetInterfaceVersion(callStatus, var_ui32);
      ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);

      proxyConsumer->GetApplicationMode(callStatus, appMode, eCode);
      ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   }

   virtual void TearDown()
   {
      // nothing to do
   }

   CommonAPI::CallStatus callStatus;
   int32_t var_i32 = 0;
   uint32_t var_ui32 = 0;
   int nsm_test_errorCode = NsmErrorStatus_NotSet;
   GENIVI::NodeStateManagerTypes::NsmErrorStatus_e errorCode = GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_NotSet;

   std::mutex mMutex;
   std::condition_variable mCondVar;
   std::shared_ptr<CommonAPI::Runtime> runtime;
   std::shared_ptr<ConsumerProxyDefault> proxyConsumer;
   std::shared_ptr<ConsumerProxyDefault> proxyConsumer_1;
   std::shared_ptr<ConsumerProxyDefault> proxyConsumer_2;
   std::shared_ptr<ConsumerProxyDefault> proxyConsumer_3;
   std::shared_ptr<ConsumerProxyDefault> proxyConsumer_4;
   std::shared_ptr<ConsumerProxyDefault> proxyConsumer_5;
   std::shared_ptr<LifecycleControlProxyDefault> proxyLifecycleControl;
   std::shared_ptr<NodeStateMachineTest::TestProxyDefault> proxyTest;

public:

};

/*
 * The define sets up a string that is longer for all text fields used in the NSM.
 * The intend is to test the NSM for correct behavior by passing this string.
 */
#define NSMTST__260CHAR_STRING "012345678901234567890123456789012345678901234567890123456789"\
                               "012345678901234567890123456789012345678901234567890123456789"\
                               "012345678901234567890123456789012345678901234567890123456789"\
                               "012345678901234567890123456789012345678901234567890123456789"\
                               "01234567890123456789"

/********************************************************************************************
 *    main()
 ********************************************************************************************/

int main(int argc, char **argv)
{
  int retVal = 1;
  DLT_REGISTER_APP("NSMT", "Unit tests for NSM");
  DLT_REGISTER_CONTEXT(gNSMTestContext, "GEN", "Context for Genivi TestLogging");

  testing::InitGoogleTest(&argc, argv);
  retVal = RUN_ALL_TESTS();

  // unregister debug log and trace
  DLT_UNREGISTER_CONTEXT(gNSMTestContext);
  DLT_UNREGISTER_APP();
  return retVal;
}

int registeredShutdownClientCb = 0;
std::mutex mMutexRegisterdClientCb;
std::condition_variable mCondVarRegisterdClientCb;

void async_callback(const CommonAPI::CallStatus& callStatus, const ::v1::org::genivi::NodeStateManagerTypes::NsmErrorStatus_e& errorStatus)
{
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorStatus, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   std::unique_lock<std::mutex> lock(mMutexRegisterdClientCb);
   registeredShutdownClientCb++;
   lock.unlock();
   mCondVarRegisterdClientCb.notify_one();
}

/********************************************************************************************
 *    BootMode Tests
 ********************************************************************************************/

TEST_F(NSMTest_GENIVI, BootMode)
{
   DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_BootMode ==> start"));

   RecordProperty("TestCaseID", "GENIVI_BootMode");
   RecordProperty("TestCaseDescription", "");

   int bootMode = 0x0;
   std::vector<uint8_t> dataIn;
   std::vector<uint8_t> dataOut;

   proxyLifecycleControl->SetBootMode(0, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer->getBootModeAttribute().getValue(callStatus, bootMode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(bootMode == 0);

   proxyLifecycleControl->SetBootMode(1, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer->getBootModeAttribute().getValue(callStatus, bootMode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(bootMode == 1);

   proxyLifecycleControl->SetBootMode(1, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer->getBootModeAttribute().getValue(callStatus, bootMode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(bootMode == 1);

   bootMode = 0x02;
   dataIn.insert(dataIn.begin(), (uint8_t*) &bootMode, ((uint8_t*) &bootMode) + sizeof(bootMode));
   proxyTest->SetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_BootMode, dataIn, sizeof(bootMode), callStatus, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(nsm_test_errorCode, NsmErrorStatus_Ok);

   bootMode = 0x03;
   dataIn.insert(dataIn.begin(), (uint8_t*) &bootMode, ((uint8_t*) &bootMode) + sizeof(bootMode));
   proxyTest->SetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_BootMode, dataIn, sizeof(bootMode) + 1, callStatus, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == NsmErrorStatus_Parameter);

   bootMode = 0x04;
   dataIn.insert(dataIn.begin(), (uint8_t*) &bootMode, ((uint8_t*) &bootMode) + sizeof(bootMode));
   proxyTest->SetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_BootMode, dataIn, sizeof(bootMode) - 1, callStatus, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == NsmErrorStatus_Parameter);

   dataOut.clear();
   proxyTest->GetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_BootMode, dataIn, sizeof(bootMode), callStatus, dataOut, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == sizeof(bootMode));
   ASSERT_TRUE(*((int*)dataOut.data()) == 0x02);

   proxyTest->GetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_BootMode, dataIn, sizeof(bootMode) + 1, callStatus, dataOut, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == -1);

   proxyTest->GetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_BootMode, dataIn, sizeof(bootMode) - 1, callStatus, dataOut, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == -1);

   DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_BootMode ==> end"));
}

/********************************************************************************************
 *    RunningReason Tests
 ********************************************************************************************/

TEST_F(NSMTest_GENIVI, RunningReason)
{
	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_RunningReason ==> start"));

	 RecordProperty("TestCaseID", "GENIVI_RunningReason");
	 RecordProperty("TestCaseDescription", "");


   GENIVI::NodeStateManagerTypes::NsmRunningReason_e runningReason;
   std::vector<uint8_t> dataIn;
   std::vector<uint8_t> dataOut;

   proxyConsumer->getWakeUpReasonAttribute().getValue(callStatus, runningReason);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(runningReason, GENIVI::NodeStateManagerTypes::NsmRunningReason_e::NsmRunningReason_WakeupCan);

   dataOut.clear();
   proxyTest->GetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_RunningReason, dataIn, sizeof(NsmRunningReason_e), callStatus, dataOut, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(nsm_test_errorCode, sizeof(NsmRunningReason_e));
   ASSERT_TRUE(*((int*)dataOut.data()) == NsmRunningReason_WakeupCan);

   proxyTest->GetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_RunningReason, dataIn, sizeof(NsmRunningReason_e) + 1, callStatus, dataOut, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == -1);

   proxyTest->GetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_RunningReason, dataIn, sizeof(NsmRunningReason_e) - 1, callStatus, dataOut, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == -1);

	DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_RunningReason ==> end"));

}

/********************************************************************************************
 *    ShutdownReason Tests
 ********************************************************************************************/

TEST_F(NSMTest_GENIVI, ShutdownReason)
{
	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_ShutdownReason ==> start"));

	 RecordProperty("TestCaseID", "GENIVI_ShutdownReason");
	 RecordProperty("TestCaseDescription", "");

   NsmShutdownReason_e enShutdownReason = NsmShutdownReason_NotSet;
   GENIVI::NodeStateManagerTypes::NsmShutdownReason_e shutdownReason;
   std::vector<uint8_t> dataIn;
   std::vector<uint8_t> dataOut;

   enShutdownReason = NsmShutdownReason_NotSet;
   dataIn.insert(dataIn.begin(), (uint8_t*) &enShutdownReason, ((uint8_t*) &enShutdownReason) + sizeof(enShutdownReason));
   proxyTest->SetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_ShutdownReason, dataIn, sizeof(NsmShutdownReason_e), callStatus, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == NsmErrorStatus_Parameter);

   // Explicitly pass wrong value
#pragma GCC diagnostic ignored "-Wconversion"
   enShutdownReason = (NsmShutdownReason_e)0xFFFFFFFF;
#pragma GCC diagnostic pop
   dataIn.insert(dataIn.begin(), (uint8_t*) &enShutdownReason, ((uint8_t*) &enShutdownReason) + sizeof(enShutdownReason));
   proxyTest->SetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_ShutdownReason, dataIn, sizeof(NsmShutdownReason_e), callStatus, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == NsmErrorStatus_Parameter);

   enShutdownReason = NsmShutdownReason_Normal;
   dataIn.insert(dataIn.begin(), (uint8_t*) &enShutdownReason, ((uint8_t*) &enShutdownReason) + sizeof(enShutdownReason));
   proxyTest->SetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_ShutdownReason, dataIn, sizeof(NsmShutdownReason_e) - 1, callStatus, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == NsmErrorStatus_Parameter);

   enShutdownReason = NsmShutdownReason_Normal;
   dataIn.insert(dataIn.begin(), (uint8_t*) &enShutdownReason, ((uint8_t*) &enShutdownReason) + sizeof(enShutdownReason));
   proxyTest->SetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_ShutdownReason, dataIn, sizeof(NsmShutdownReason_e) + 1, callStatus, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == NsmErrorStatus_Parameter);

   enShutdownReason = NsmShutdownReason_Normal;
   dataIn.insert(dataIn.begin(), (uint8_t*) &enShutdownReason, ((uint8_t*) &enShutdownReason) + sizeof(enShutdownReason));
   proxyTest->SetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_ShutdownReason, dataIn, sizeof(NsmShutdownReason_e), callStatus, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(nsm_test_errorCode, NsmErrorStatus_Ok);

   enShutdownReason = NsmShutdownReason_Normal;
   dataIn.insert(dataIn.begin(), (uint8_t*) &enShutdownReason, ((uint8_t*) &enShutdownReason) + sizeof(enShutdownReason));
   proxyTest->SetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_ShutdownReason, dataIn, sizeof(NsmShutdownReason_e), callStatus, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(nsm_test_errorCode, NsmErrorStatus_Ok);

   proxyConsumer->getShutdownReasonAttribute().getValue(callStatus, shutdownReason);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(shutdownReason == GENIVI::NodeStateManagerTypes::NsmShutdownReason_e::NsmShutdownReason_Normal);

   dataOut.clear();
   proxyTest->GetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_ShutdownReason, dataIn, sizeof(NsmShutdownReason_e), callStatus, dataOut, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == sizeof(NsmShutdownReason_e));
   ASSERT_TRUE( *((NsmShutdownReason_e*)dataOut.data()) == NsmShutdownReason_Normal);

   proxyTest->GetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_ShutdownReason, dataIn, sizeof(NsmShutdownReason_e) - 1, callStatus, dataOut, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == -1);

   proxyTest->GetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_ShutdownReason, dataIn, sizeof(NsmShutdownReason_e) + 1, callStatus, dataOut, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == -1);

   enShutdownReason = NsmShutdownReason_SupplyBad;
   dataIn.insert(dataIn.begin(), (uint8_t*) &enShutdownReason, ((uint8_t*) &enShutdownReason) + sizeof(enShutdownReason));
   proxyTest->SetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_ShutdownReason, dataIn, sizeof(NsmShutdownReason_e), callStatus, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(nsm_test_errorCode, NsmErrorStatus_Ok);

   proxyConsumer->getShutdownReasonAttribute().getValue(callStatus, shutdownReason);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(shutdownReason == GENIVI::NodeStateManagerTypes::NsmShutdownReason_e::NsmShutdownReason_SupplyBad);

   dataOut.clear();
   proxyTest->GetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_ShutdownReason, dataIn, sizeof(NsmShutdownReason_e), callStatus, dataOut, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == sizeof(NsmShutdownReason_e));
   ASSERT_TRUE( *((NsmShutdownReason_e*)dataOut.data()) == NsmShutdownReason_SupplyBad);

   proxyTest->GetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_ShutdownReason, dataIn, sizeof(NsmShutdownReason_e) - 1, callStatus, dataOut, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == -1);

   proxyTest->GetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_ShutdownReason, dataIn, sizeof(NsmShutdownReason_e) + 1, callStatus, dataOut, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == -1);

	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_ShutdownReason ==> end"));
}

/********************************************************************************************
 *    RestartReason Tests
 ********************************************************************************************/

TEST_F(NSMTest_GENIVI, RestartReason)
{
	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_RestartReason ==> start"));

	 RecordProperty("TestCaseID", "GENIVI_RestartReason");
	 RecordProperty("TestCaseDescription", "");

   GENIVI::NodeStateManagerTypes::NsmRestartReason_e restartReason;
   std::vector<uint8_t> dataIn;
   std::vector<uint8_t> dataOut;
   proxyConsumer->getRestartReasonAttribute().getValue(callStatus, restartReason);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(restartReason == GENIVI::NodeStateManagerTypes::NsmRestartReason_e::NsmRestartReason_NotSet);

   dataOut.clear();
   proxyTest->GetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_RestartReason, dataIn, sizeof(NsmRestartReason_e), callStatus, dataOut, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == sizeof(NsmRestartReason_e));
   ASSERT_TRUE( *((NsmRestartReason_e*)dataOut.data()) == NsmRestartReason_NotSet);

   proxyTest->GetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_RestartReason, dataIn, sizeof(NsmRestartReason_e) - 1, callStatus, dataOut, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == -1);

   proxyTest->GetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_RestartReason, dataIn, sizeof(NsmRestartReason_e) + 1, callStatus, dataOut, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == -1);

	DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_RestartReason ==> end"));
}

/********************************************************************************************
 *    NodeState Tests
 ********************************************************************************************/

TEST_F(NSMTest_GENIVI, NodeState)
{
	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_NodeState ==> start"));

	 RecordProperty("TestCaseID", "GENIVI_NodeState");
	 RecordProperty("TestCaseDescription", "");

   GENIVI::NodeStateManagerTypes::NsmNodeState_e nsmNodeState;
   int32_t nodeState = -1;
   int32_t nodeStateToSet = -1;
   std::vector<uint8_t> dataIn;
   std::vector<uint8_t> dataOut;

   proxyConsumer->getNodeStateEvent().subscribe([&](const int32_t& val) {
           nodeState = val;
       });

   proxyLifecycleControl->SetNodeState(GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_NotSet, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Parameter);

   proxyLifecycleControl->SetNodeState(GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_BaseRunning, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyLifecycleControl->SetNodeState(GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_BaseRunning, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   nodeState = -1;

   proxyLifecycleControl->SetNodeState(GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_LucRunning, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   while (nodeState == -1)
      usleep(10);
   ASSERT_TRUE(nodeState == NsmNodeState_LucRunning);

   proxyConsumer->GetNodeState(callStatus, nsmNodeState, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   ASSERT_TRUE(nsmNodeState == GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_LucRunning);

   nodeState = -1;
   nodeStateToSet = NsmNodeState_FullyRunning;
   dataIn.insert(dataIn.begin(), (uint8_t*) &nodeStateToSet, ((uint8_t*) &nodeStateToSet) + sizeof(nodeStateToSet));
   proxyTest->SetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_NodeState, dataIn, sizeof(nodeStateToSet), callStatus, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(nsm_test_errorCode, NsmErrorStatus_Ok);

   while (nodeState == -1)
      usleep(10);
   ASSERT_TRUE(nodeState == NsmNodeState_FullyRunning);

   nodeState = NsmNodeState_FullyRunning;
   dataIn.insert(dataIn.begin(), (uint8_t*) &nodeState, ((uint8_t*) &nodeState) + sizeof(nodeState));
   proxyTest->SetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_NodeState, dataIn, 3, callStatus, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == NsmErrorStatus_Parameter);

   nodeState = NsmNodeState_FullyRunning;
   dataIn.insert(dataIn.begin(), (uint8_t*) &nodeState, ((uint8_t*) &nodeState) + sizeof(nodeState));
   proxyTest->SetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_NodeState, dataIn, 5, callStatus, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == NsmErrorStatus_Parameter);

   dataOut.clear();
   proxyTest->GetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_NodeState, dataIn, sizeof(nodeState), callStatus, dataOut, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == sizeof(nodeState));
   ASSERT_TRUE(*((NsmNodeState_e* )dataOut.data()) == NsmNodeState_FullyRunning);

   dataOut.clear();
   proxyTest->GetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_NodeState, dataIn, sizeof(nodeState) - 1, callStatus, dataOut, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == -1);

	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_NodeState ==> end"));
}

/********************************************************************************************
 *    InvalidData Tests
 ********************************************************************************************/

TEST_F(NSMTest_GENIVI, InvalidData)
{
	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_InvalidData ==> start"));

	 RecordProperty("TestCaseID", "GENIVI_InvalidData");
   RecordProperty("TestCaseDescription", "");

   std::vector<uint8_t> dataIn;
   NsmRestartReason_e restartReason;

   dataIn.insert(dataIn.begin(), (uint8_t*) &restartReason, ((uint8_t*) &restartReason) + sizeof(restartReason));
   proxyTest->SetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_RestartReason, dataIn, sizeof(restartReason), callStatus, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == NsmErrorStatus_Parameter);

   dataIn.insert(dataIn.begin(), (uint8_t*) &restartReason, ((uint8_t*) &restartReason) + sizeof(restartReason));
   // Explicitly pass wrong value
#pragma GCC diagnostic ignored "-Wconversion"
   proxyTest->SetNsmData(
         GENIVI::NodeStateManagerTypes::NsmDataType_e((GENIVI::NodeStateManagerTypes::NsmDataType_e::Literal) 0xFFFFFFFF),
         dataIn, sizeof(restartReason), callStatus, nsm_test_errorCode);
#pragma GCC diagnostic pop
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::INVALID_VALUE);
   ASSERT_TRUE(nsm_test_errorCode == NsmErrorStatus_Parameter);

   dataIn.insert(dataIn.begin(), (uint8_t*) &restartReason, ((uint8_t*) &restartReason) + sizeof(restartReason));
   // Explicitly pass wrong value
#pragma GCC diagnostic ignored "-Wconversion"
   proxyTest->SetNsmData(
         GENIVI::NodeStateManagerTypes::NsmDataType_e((GENIVI::NodeStateManagerTypes::NsmDataType_e::Literal) 0xFFFFFFFF),
         dataIn, sizeof(restartReason), callStatus, nsm_test_errorCode);
#pragma GCC diagnostic pop
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::INVALID_VALUE);
   ASSERT_TRUE(nsm_test_errorCode == NsmErrorStatus_Parameter);

	DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_InvalidData ==> end"));
}

/********************************************************************************************
 *    RegisterSession Tests
 ********************************************************************************************/

TEST_F(NSMTest_GENIVI, RegisterSession)
{
	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_RegisterSession ==> start"));

   RecordProperty("TestCaseID", "GENIVI_RegisterSession");
   RecordProperty("TestCaseDescription", "");

   proxyConsumer->RegisterSession("VoiceControl", "NodeStateManager", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Parameter);

   proxyConsumer->RegisterSession(NSMTST__260CHAR_STRING, "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Parameter);

   proxyConsumer->RegisterSession("VoiceControl", NSMTST__260CHAR_STRING, GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Parameter);

   proxyConsumer->RegisterSession("VoiceControl", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_NotSet, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Parameter);

   // Explicitly pass wrong value
#pragma GCC diagnostic ignored "-Wconversion"
   proxyConsumer->RegisterSession("VoiceControl", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e((GENIVI::NodeStateManagerTypes::NsmSeat_e::Literal)0xFFFFFFFF), GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
#pragma GCC diagnostic pop
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::INVALID_VALUE);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Parameter);

   proxyConsumer->RegisterSession("VoiceControl", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Unregistered, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Parameter);

   proxyConsumer->RegisterSession("DiagnosisSession", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Parameter);

   proxyConsumer->RegisterSession("VoiceControl", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_RegisterSession ==> end"));
}

/********************************************************************************************
 *    UnRegisterSession Tests
 ********************************************************************************************/

TEST_F(NSMTest_GENIVI, UnregisterSession)
{
	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_UnregisterSession ==> start"));

   RecordProperty("TestCaseID", "GENIVI_UnregisterSession");
   RecordProperty("TestCaseDescription", "");

   proxyConsumer->UnRegisterSession(NSMTST__260CHAR_STRING, "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Parameter);

   proxyConsumer->UnRegisterSession("VoiceControl", NSMTST__260CHAR_STRING, GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Parameter);

   proxyConsumer->UnRegisterSession("DiagnosisSession", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_WrongSession);

   proxyConsumer->UnRegisterSession("Unknown", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_WrongSession);

   proxyConsumer->UnRegisterSession("VoiceControl", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_UnregisterSession ==> end"));
}

/********************************************************************************************
 *    SessionState Tests
 ********************************************************************************************/

TEST_F(NSMTest_GENIVI, SetSessionState)
{
	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_SetSessionState ==> start"));

   RecordProperty("TestCaseID", "GENIVI_SetSessionState");
   RecordProperty("TestCaseDescription", "");

   std::vector<uint8_t> dataIn;
   std::vector<uint8_t> dataOut;
   NsmSession_s session;

   GENIVI::NodeStateManagerTypes::NsmSessionState_e sessionState;

   proxyConsumer->SetSessionState(NSMTST__260CHAR_STRING, "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Parameter);

   proxyConsumer->SetSessionState("VoiceControl", NSMTST__260CHAR_STRING, GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Parameter);

   proxyConsumer->SetSessionState("VoiceControl", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_WrongSession);

   proxyConsumer->SetSessionState("VoiceControl", "NodeStateManager", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Parameter);

   proxyConsumer->SetSessionState("VoiceControl", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Unregistered, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Parameter);

   proxyConsumer->SetSessionState("VoiceControl", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_NotSet, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Parameter);

   // Explicitly pass wrong value
#pragma GCC diagnostic ignored "-Wconversion"
   proxyConsumer->SetSessionState("VoiceControl", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e((GENIVI::NodeStateManagerTypes::NsmSeat_e::Literal)0xFFFFFFFF), GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
#pragma GCC diagnostic pop
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::INVALID_VALUE);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Parameter);

   proxyConsumer->SetSessionState("DiagnosisSession", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer->SetSessionState("DiagnosisSession", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e((GENIVI::NodeStateManagerTypes::NsmSessionState_e::Literal)0x03), callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::INVALID_VALUE);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer->SetSessionState("DiagnosisSession", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Inactive, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer->SetSessionState("DiagnosisSession", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer->SetSessionState("DiagnosisSession", "NodeStateTest02", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Error);

   proxyConsumer->SetSessionState("DiagnosisSession", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Inactive, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer->SetSessionState("DiagnosisSession", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Inactive, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Parameter);

   proxyConsumer->SetSessionState("ProductSession", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_WrongSession);

   proxyConsumer->RegisterSession("ProductSession", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer->SetSessionState("ProductSession", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Inactive, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer->SetSessionState("ProductSession", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Inactive, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer->GetSessionState(NSMTST__260CHAR_STRING, GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, callStatus, sessionState, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Parameter);
   ASSERT_TRUE(sessionState == GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Unregistered);

   proxyConsumer->GetSessionState("ProductSession", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, callStatus, sessionState, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   ASSERT_TRUE(sessionState == GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Inactive);

   proxyConsumer->GetSessionState("UnknownSession", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, callStatus, sessionState, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_WrongSession);
   ASSERT_TRUE(sessionState == GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Unregistered);

   proxyConsumer->UnRegisterSession("ProductSession", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   strcpy(session.sName, "ProductSession");
   strcpy(session.sOwner, "NodeStateTest");

   session.enSeat = NsmSeat_Driver;
   session.enState = NsmSessionState_Active;

   dataOut.clear();
   dataIn.insert(dataIn.begin(), (uint8_t*) &session, ((uint8_t*) &session) + sizeof(session));
   proxyTest->SetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_SessionState, dataIn, sizeof(session), callStatus, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == NsmErrorStatus_WrongSession);

   dataOut.clear();
   dataIn.insert(dataIn.begin(), (uint8_t*) &session, ((uint8_t*) &session) + sizeof(session));
   proxyTest->SetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_SessionState, dataIn, 4, callStatus, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == NsmErrorStatus_Parameter);

   dataOut.clear();
   dataOut.reserve(sizeof(session));
   dataIn.clear();
   dataIn.insert(dataIn.begin(), (uint8_t*) &session, ((uint8_t*) &session) + sizeof(session));
   proxyTest->GetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_SessionState, dataIn, 5, callStatus, dataOut, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == -1);
   memcpy(&session, dataOut.data(), sizeof(session));
   ASSERT_TRUE(session.enState == (int)GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Unregistered);

	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_SetSessionState ==> end"));
}

/********************************************************************************************
 *    AppHealth Tests
 ********************************************************************************************/

TEST_F(NSMTest_GENIVI, GetAppHealth)
{
	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_GetAppHealth ==> start"));

   RecordProperty("TestCaseID", "GENIVI_SGetAppHealth");
   RecordProperty("TestCaseDescription", "");

   proxyConsumer->GetAppHealthCount(callStatus, var_ui32);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(var_ui32 == 0);

   proxyLifecycleControl->SetAppHealthStatus(NSMTST__260CHAR_STRING, true, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Parameter);

   proxyLifecycleControl->SetAppHealthStatus("NodeStateTest", true, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Error);

   proxyLifecycleControl->SetAppHealthStatus("NodeStateTest", false, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyLifecycleControl->SetAppHealthStatus("NodeStateTest", true, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer->RegisterSession("ProductSession", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyLifecycleControl->SetAppHealthStatus("NodeStateTest", false, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyLifecycleControl->SetAppHealthStatus("NodeStateTest1", false, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyLifecycleControl->SetAppHealthStatus("NodeStateTest", true, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer->SetSessionState("DiagnosisSession", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer->SetSessionState("HevacSession", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyLifecycleControl->SetAppHealthStatus("NodeStateTest", false, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_GetAppHealth ==> end"));
}

/********************************************************************************************
 *    Luc Tests
 ********************************************************************************************/

TEST_F(NSMTest_GENIVI, CheckLuc)
{
	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_CheckLuc ==> start"));

   RecordProperty("TestCaseID", "GENIVI_CheckLuc");
   RecordProperty("TestCaseDescription", "");

   int barrier1 = 0;
   int barrier2 = 0;
   int barrier3 = 0;
   int barrier4 = 0;
   int barrier5 = 0;

   uint32_t expected_mode_1 = NSM_SHUTDOWNTYPE_NOT;
   uint32_t expected_mode_2 = NSM_SHUTDOWNTYPE_NOT;
   uint32_t expected_mode_3 = NSM_SHUTDOWNTYPE_NOT;
   uint32_t expected_mode_4 = NSM_SHUTDOWNTYPE_NOT;
   uint32_t expected_mode_5 = NSM_SHUTDOWNTYPE_NOT;


   bool lucWanted;
   proxyLifecycleControl->CheckLucRequired(callStatus, lucWanted);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(lucWanted);

   registeredShutdownClientCb = 0;

   proxyConsumer_1->getShutdownEventsSelectiveEvent().subscribe([&](const uint32_t mode)
   {
      ASSERT_EQ(mode, expected_mode_1);
      barrier1 = 1;
   }, [&](CommonAPI::CallStatus callStatus)
   {
      if(callStatus == CommonAPI::CallStatus::SUCCESS)
      {
        proxyConsumer_1->RegisterShutdownClientAsync(NSM_SHUTDOWNTYPE_NORMAL, 2000, async_callback);
        proxyConsumer_1->RegisterShutdownClientAsync(NSM_SHUTDOWNTYPE_FAST, 2000, async_callback);
      }
   });

   std::unique_lock<std::mutex> lock(mMutexRegisterdClientCb);
   while (registeredShutdownClientCb < 2)
   {
      mCondVarRegisterdClientCb.wait(lock);
   }
   registeredShutdownClientCb = 0;
   lock.unlock();

   proxyConsumer_2->getShutdownEventsSelectiveEvent().subscribe([&](const uint32_t mode)
   {
      ASSERT_EQ(mode, expected_mode_2);
      barrier2 = 1;
   }, [&](CommonAPI::CallStatus callStatus)
   {
      if(callStatus == CommonAPI::CallStatus::SUCCESS)
      {
        proxyConsumer_2->RegisterShutdownClientAsync(NSM_SHUTDOWNTYPE_NORMAL, 2000, async_callback);
        proxyConsumer_2->RegisterShutdownClientAsync(NSM_SHUTDOWNTYPE_FAST, 2000, async_callback);
      }
   });

   lock.lock();
   while (registeredShutdownClientCb < 2)
   {
      mCondVarRegisterdClientCb.wait(lock);
   }
   registeredShutdownClientCb = 0;
   lock.unlock();

   proxyConsumer_3->getShutdownEventsSelectiveEvent().subscribe([&](const uint32_t mode)
   {
      ASSERT_EQ(mode, expected_mode_3);
      barrier3 = 1;
   }, [&](CommonAPI::CallStatus callStatus)
   {
      if(callStatus == CommonAPI::CallStatus::SUCCESS)
      {
        proxyConsumer_3->RegisterShutdownClientAsync(NSM_SHUTDOWNTYPE_NORMAL, 2000, async_callback);
      }
   });

   lock.lock();
   while (registeredShutdownClientCb < 1)
   {
      mCondVarRegisterdClientCb.wait(lock);
   }
   registeredShutdownClientCb = 0;
   lock.unlock();

   proxyConsumer_4->getShutdownEventsSelectiveEvent().subscribe([&](const uint32_t mode)
   {
      ASSERT_EQ(mode, expected_mode_4);
      barrier4 = 1;
   }, [&](CommonAPI::CallStatus callStatus)
   {
      if(callStatus == CommonAPI::CallStatus::SUCCESS)
      {
        proxyConsumer_4->RegisterShutdownClientAsync(NSM_SHUTDOWNTYPE_NORMAL, 2000, async_callback);
      }
   });

   lock.lock();
   while (registeredShutdownClientCb < 1)
   {
      mCondVarRegisterdClientCb.wait(lock);
   }
   registeredShutdownClientCb = 0;
   lock.unlock();

   proxyConsumer_5->getShutdownEventsSelectiveEvent().subscribe([&](const uint32_t mode)
   {
      ASSERT_EQ(mode, expected_mode_5);
      barrier5 = 1;
   }, [&](CommonAPI::CallStatus callStatus)
   {
      if(callStatus == CommonAPI::CallStatus::SUCCESS)
      {
        proxyConsumer_5->RegisterShutdownClientAsync(NSM_SHUTDOWNTYPE_NORMAL | NSM_SHUTDOWNTYPE_FAST, 2000, async_callback);
      }
   });

   lock.lock();
   while (registeredShutdownClientCb < 1)
   {
      mCondVarRegisterdClientCb.wait(lock);
   }
   registeredShutdownClientCb = 0;
   lock.unlock();

   proxyConsumer_1->UnRegisterShutdownClient(NSM_SHUTDOWNTYPE_FAST, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer->UnRegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Parameter);

   expected_mode_1 = NSM_SHUTDOWNTYPE_NORMAL;
   expected_mode_2 = NSM_SHUTDOWNTYPE_NORMAL;
   expected_mode_3 = NSM_SHUTDOWNTYPE_NORMAL;
   expected_mode_4 = NSM_SHUTDOWNTYPE_NORMAL;
   expected_mode_5 = NSM_SHUTDOWNTYPE_NORMAL;

   // Set NodeState to BaseRunning because NSM already is in Shutdown Mode -> it will not shutdown again
   proxyLifecycleControl->SetNodeState(GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_BaseRunning, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   // Set NodeState to ShuttingDown
   proxyLifecycleControl->SetNodeState(GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_ShuttingDown, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   // Wait until LifecycleConsumer is notified
   while(0 == barrier5) { usleep(10); } barrier5 = 0;
   proxyConsumer_5->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(NsmErrorStatus_Ok, errorCode);

   while(0 == barrier4) { usleep(10); } barrier4 = 0;
   proxyConsumer_4->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(NsmErrorStatus_Ok, errorCode);

   while(0 == barrier3) { usleep(10); } barrier3 = 0;
   proxyConsumer_3->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Error, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   while(0 == barrier2) { usleep(10); } barrier2 = 0;
   proxyConsumer_2->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   while(0 == barrier1) { usleep(10); } barrier1 = 0;
   proxyConsumer_1->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   expected_mode_1 = NSM_SHUTDOWNTYPE_RUNUP;
   expected_mode_2 = NSM_SHUTDOWNTYPE_RUNUP;
   expected_mode_3 = NSM_SHUTDOWNTYPE_RUNUP;
   expected_mode_4 = NSM_SHUTDOWNTYPE_RUNUP;
   expected_mode_5 = NSM_SHUTDOWNTYPE_RUNUP;

   // Set NodeState to BaseRunning
   proxyLifecycleControl->SetNodeState(GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_BaseRunning, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   while(0 == barrier1) { usleep(10); }  barrier1 = 0;
   proxyConsumer_1->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   while(0 == barrier2) { usleep(10); }  barrier2 = 0;
   proxyConsumer_2->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   while(0 == barrier3) { usleep(10); }  barrier3 = 0;
   proxyConsumer_3->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   while(0 == barrier4) { usleep(10); }  barrier4 = 0;
   proxyConsumer_4->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   while(0 == barrier5) { usleep(10); }  barrier5 = 0;
   proxyConsumer_5->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   expected_mode_2 = NSM_SHUTDOWNTYPE_FAST;
   expected_mode_5 = NSM_SHUTDOWNTYPE_FAST;

   proxyLifecycleControl->SetNodeState(GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_FastShutdown, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   while(0 == barrier5) { usleep(10); }  barrier5 = 0;
   proxyConsumer_5->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   while(0 == barrier2) { usleep(10); }  barrier2 = 0;
   proxyConsumer_2->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   expected_mode_2 = NSM_SHUTDOWNTYPE_RUNUP;
   expected_mode_5 = NSM_SHUTDOWNTYPE_RUNUP;

   // Set NodeState to BaseRunning
   proxyLifecycleControl->SetNodeState(GENIVI::NodeStateManagerTypes::NsmNodeState_e::NsmNodeState_BaseRunning, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   while(0 == barrier2) { usleep(10); }  barrier2 = 0;
   proxyConsumer_2->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   while(0 == barrier5) { usleep(10); }  barrier5 = 0;
   proxyConsumer_5->LifecycleRequestComplete(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok, callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer_1->UnRegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL | NSM_SHUTDOWNTYPE_NORMAL , callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer_2->UnRegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL | NSM_SHUTDOWNTYPE_NORMAL , callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer_3->UnRegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL | NSM_SHUTDOWNTYPE_NORMAL , callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer_4->UnRegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL | NSM_SHUTDOWNTYPE_NORMAL , callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer_5->UnRegisterShutdownClient(NSM_SHUTDOWNTYPE_NORMAL | NSM_SHUTDOWNTYPE_NORMAL , callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(errorCode == GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_CheckLuc ==> end"));
}

/********************************************************************************************
 *    SessionState Tests
 ********************************************************************************************/

TEST_F(NSMTest_GENIVI, SessionState)
{
	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_SessionState ==> start"));

   RecordProperty("TestCaseID", "GENIVI_SessionState");
   RecordProperty("TestCaseDescription", "");

   NsmSession_s session;
   GENIVI::NodeStateManagerTypes::NsmSessionState_e sessionState;
   GENIVI::NodeStateManagerTypes::NsmSeat_e seatID;
   std::string sessionStateName;

   std::vector<uint8_t> dataIn;
   std::vector<uint8_t> dataOut;

   proxyConsumer->SetSessionState("PlatformSupplySession", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e((GENIVI::NodeStateManagerTypes::NsmSessionState_e::Literal) 0x02), callStatus, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   proxyConsumer->GetSessionState("PlatformSupplySession", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, callStatus, sessionState, errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);
   ASSERT_TRUE(sessionState == GENIVI::NodeStateManagerTypes::NsmSessionState_e((GENIVI::NodeStateManagerTypes::NsmSessionState_e::Literal) 0x02));

   sessionState = GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Unregistered;
   proxyConsumer->getSessionStateChangedEvent().subscribe([&](std::string SessionStateName, GENIVI::NodeStateManagerTypes::NsmSeat_e SeatID, GENIVI::NodeStateManagerTypes::NsmSessionState_e SessionState)
   {
      sessionStateName.assign(SessionStateName);
      seatID = SeatID;
      sessionState = SessionState;
   });

   usleep(10000);

   proxyConsumer->SetSessionState("PlatformSupplySession", "NodeStateTest", GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver, GENIVI::NodeStateManagerTypes::NsmSessionState_e((GENIVI::NodeStateManagerTypes::NsmSessionState_e::Literal) 0x03), callStatus, errorCode);
   ASSERT_TRUE(callStatus == CommonAPI::CallStatus::INVALID_VALUE);
   ASSERT_EQ(errorCode, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::NsmErrorStatus_Ok);

   strcpy(session.sName, "StateMachine");
   strcpy(session.sOwner, "NodeStateTest");

   session.enSeat = NsmSeat_Driver;
   session.enState = NsmSessionState_Active;

   sessionState = GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Unregistered;
   proxyConsumer->getSessionStateChangedEvent().subscribe([&](std::string SessionStateName, GENIVI::NodeStateManagerTypes::NsmSeat_e SeatID, GENIVI::NodeStateManagerTypes::NsmSessionState_e SessionState)
   {
      sessionStateName.assign(SessionStateName);
      seatID = SeatID;
      sessionState = SessionState;
   });

   usleep(10000);

   dataOut.clear();
   dataIn.insert(dataIn.begin(), (uint8_t*) &session, ((uint8_t*) &session) + sizeof(session));
   proxyTest->SetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_RegisterSession, dataIn, sizeof(session), callStatus, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(nsm_test_errorCode, NsmErrorStatus_Ok);

   while (sessionState == GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Unregistered)
      usleep(10);

   ASSERT_TRUE(0 == sessionStateName.compare("StateMachine"));
   ASSERT_TRUE(seatID == GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver);
   ASSERT_TRUE(sessionState == GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Active);

   dataOut.clear();
   dataIn.insert(dataIn.begin(), (uint8_t*) &session, ((uint8_t*) &session) + sizeof(session));
   proxyTest->SetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_RegisterSession, dataIn, sizeof(session) - 1, callStatus, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == NsmErrorStatus_Parameter);

   dataOut.clear();
   dataIn.insert(dataIn.begin(), (uint8_t*) &session, ((uint8_t*) &session) + sizeof(session));
   proxyTest->SetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_UnRegisterSession, dataIn, sizeof(session) - 1, callStatus, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_TRUE(nsm_test_errorCode == NsmErrorStatus_Parameter);

   sessionState = GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Inactive;
   proxyConsumer->getSessionStateChangedEvent().subscribe([&](std::string SessionStateName, GENIVI::NodeStateManagerTypes::NsmSeat_e SeatID, GENIVI::NodeStateManagerTypes::NsmSessionState_e SessionState)
   {
      sessionStateName.assign(SessionStateName);
      seatID = SeatID;
      sessionState = SessionState;
   });

   usleep(10000);

   session.enState = NsmSessionState_Unregistered;
   dataOut.clear();
   dataIn.insert(dataIn.begin(), (uint8_t*) &session, ((uint8_t*) &session) + sizeof(session));
   proxyTest->SetNsmData(GENIVI::NodeStateManagerTypes::NsmDataType_e::NsmDataType_UnRegisterSession, dataIn, sizeof(session), callStatus, nsm_test_errorCode);
   ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
   ASSERT_EQ(nsm_test_errorCode, NsmErrorStatus_Ok);

   while (sessionState == GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Inactive)
      usleep(10);

   ASSERT_TRUE(0 == sessionStateName.compare("StateMachine"));
   ASSERT_TRUE(seatID == GENIVI::NodeStateManagerTypes::NsmSeat_e::NsmSeat_Driver);
   ASSERT_TRUE(sessionState == GENIVI::NodeStateManagerTypes::NsmSessionState_e::NsmSessionState_Unregistered);

	 DLT_LOG(gNSMTestContext, DLT_LOG_INFO, DLT_STRING("run_test_GENIVI_SessionState ==> end"));
}
