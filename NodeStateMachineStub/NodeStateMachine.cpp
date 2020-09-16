/**********************************************************************************************************************
*
* Copyright (C) 2012 Continental Automotive Systems, Inc.
*
* Author: Jean-Pierre.Bogler@continental-corporation.com
*
* Header for the NodestateMachine stub.
*
* The header file defines the interfaces offered by the NodeStateMachine stub.
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Date             Author              Reason
* 27.09.2012       Jean-Pierre Bogler  CSP_WZ#1194: Initial creation.
* 24.10.2012       Jean-Pierre Bogler  CSP_WZ#1322: Changed parameter types of interface functions.
*
**********************************************************************************************************************/


/**********************************************************************************************************************
*
* Header includes
*
**********************************************************************************************************************/

#include "NodeStateMachine.hpp" /* own header file            */

#include "NodeStateManager.h"
#include "NodeStateTypes.h"
#include <stdio.h>
#include <CommonAPI/CommonAPI.hpp>

#ifdef ENABLE_TESTS
#include "Test_StubImpl.hpp"
#include "v1/org/genivi/nodestatemachinetest/TestInstanceIds.hpp"
#endif
/**********************************************************************************************************************
*
* Local defines, macros, constants and type definitions.
*
**********************************************************************************************************************/

// CommonAPI connection ID
const char* gConnectionID = "NSMimpl";
const char* gCapiDomain = "local";

/**********************************************************************************************************************
*
* Local variables
*
**********************************************************************************************************************/
#ifdef ENABLE_TESTS
static std::shared_ptr<CommonAPI::Runtime> gRuntime = NULL;
static std::shared_ptr<Test_StubImpl> gTestStub = NULL;
#endif
/**********************************************************************************************************************
*
* Prototypes for file local functions (see implementation for description)
*
**********************************************************************************************************************/

/* There are no file local functions */

/**********************************************************************************************************************
*
* Local (static) functions
*
**********************************************************************************************************************/

/* There are no file local functions */

/**********************************************************************************************************************
*
* Interfaces, exported functions. See header for detailed description.
*
**********************************************************************************************************************/
unsigned char NsmcDeInit(void)
{
  printf("NSMC: NsmcDeInit called.\n");
#ifdef ENABLE_TESTS
  gRuntime->unregisterService(gCapiDomain, Test_StubImpl::StubInterface::getInterface(), v1::org::genivi::nodestatemachinetest::Test_INSTANCES[0]);
  gTestStub = NULL;
#endif

  return 1;
}

unsigned char NsmcInit(void)
{
  printf("NSMC: NsmcInit called.\n");
#ifdef ENABLE_TESTS
  // Get CommonAPI runtime
  gRuntime = CommonAPI::Runtime::get();

  gTestStub = std::make_shared<Test_StubImpl>();
  if (gRuntime->registerService(gCapiDomain, v1::org::genivi::nodestatemachinetest::Test_INSTANCES[0], gTestStub, gConnectionID) == false)
  {
    printf("NSMC: Failed to create NSMTest\n");
  }
#endif
  return 1;
}


unsigned char NsmcLucRequired(void)
{
  printf("NSMC: NsmcLucRequired called.\n");

  return 1;
}


NsmErrorStatus_e NsmcSetData(NsmDataType_e enData, unsigned char *pData, unsigned int u32DataLen)
{
  printf("NSMC: NsmcSetData called. enData: %d. pData: 0x%08X. u32DataLen: %d\n", enData, (int)(size_t) pData, u32DataLen);

  return NsmErrorStatus_Ok;
}


unsigned char NsmcRequestNodeRestart(NsmRestartReason_e enRestartReason, unsigned int u32RestartType)
{
  printf("NSMC: NsmcRequestNodeRestart called. Restart reason: %d. RestartType: 0x%02X\n", enRestartReason, u32RestartType);

  return 1;
}


unsigned int NsmcGetInterfaceVersion(void)
{
  printf("NSMC: NsmcGetInterfaceVersion called.\n");

  return NSMC_INTERFACE_VERSION;
}




