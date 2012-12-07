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

#include "NodeStateMachine.h" /* own header file            */
#include "NodeStateManager.h"
#include "NodeStateTypes.h"
#include <stdio.h>


/**********************************************************************************************************************
*
* Local defines, macros, constants and type definitions.
*
**********************************************************************************************************************/

/* There are currently no local defines, macros or types */


/**********************************************************************************************************************
*
* Local variables
*
**********************************************************************************************************************/

/* There are currently no local variables */

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

unsigned char NsmcInit(void)
{
  printf("NSMC: NsmcInit called.\n");

  return 1;
}


unsigned char NsmcLucRequired(void)
{
  printf("NSMC: NsmcLucRequired called.\n");

  return 1;
}


NsmErrorStatus_e NsmcSetData(NsmDataType_e enData, unsigned char *pData, unsigned int u32DataLen)
{
  printf("NSMC: NsmcSetData called. enData: %d. pData: 0x%08X. u32DataLen: %d\n", enData, (unsigned int) pData, u32DataLen);

  return NsmErrorStatus_Ok;
}


unsigned char NsmcRequestNodeRestart(void)
{
  printf("NSMC: NsmcRequestNodeRestart called.\n");

  return 1;
}


unsigned int NsmcGetInterfaceVersion(void)
{
  printf("NSMC: NsmcGetInterfaceVersion called.\n");

  return NSMC_INTERFACE_VERSION;
}




