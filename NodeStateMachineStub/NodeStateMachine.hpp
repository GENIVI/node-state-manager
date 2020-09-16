#ifndef NSM_NODESTATEMACHINE_H
#define NSM_NODESTATEMACHINE_H


/**********************************************************************************************************************
*
* Copyright (C) 2012 Continental Automotive Systems, Inc.
*               2017 BMW AG
*
* Author: Jean-Pierre.Bogler@continental-corporation.com
*
* Header for the NodestateMachine stub.
*
* The header file defines the interfaces offred by the NodeStateMachine stub.
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
**********************************************************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/**********************************************************************************************************************
*
*  HEADER FILE INCLUDES
*
**********************************************************************************************************************/

#include "NodeStateTypes.h"

/**********************************************************************************************************************
*
*  CONSTANTS
*
**********************************************************************************************************************/

/**
 *  Module version, use SswVersion to interpret the value.
 *  The lower significant byte is equal 0 for released version only
 */

#define NSMC_INTERFACE_VERSION    0x01010000U


/**********************************************************************************************************************
*
*  TYPE
*
**********************************************************************************************************************/

/* There are no own types defined */


/**********************************************************************************************************************
*
*  GLOBAL VARIABLES
*
**********************************************************************************************************************/

/* There are no exported global variables */


/**********************************************************************************************************************
*
*  FUNCTION PROTOTYPE
*
**********************************************************************************************************************/

/** \brief Initialize the NodeStateMachine
\retval true:  The NodeStateMachine could be initialized and is running. false: An error occurred. NodeStateMachine not started.

This function will be used to initialize the Node State Machine, it will be called by the Node State Manager.
At the point where this function returns the machine is available to accept events via its interfaces from
the NSM. It is envisaged that in this call the NSMC will create and transfer control of the NSMC to its own
thread and will return in the original thread.*/
unsigned char NsmcInit(void);

unsigned char NsmcDeInit(void);


/** \brief Check for Last User Context
\retval true:  Last User Context (LUC) is required. false: No LUC required.

This will be used by the NSM to check whether in the current Lifecycle the Last User Context (LUC) should
be started. This allows the product to define its own handling for specific Application modes. */
unsigned char NsmcLucRequired(void);


/** \brief Set data in the NodeStateMachine.
\param[in]  enData     Type of the data to set (see NsmDataType_e).
\param[in]  pData      Pointer to the memory location containing the data.
\param[in]  u32DataLen Length of the data that should be set (in byte).
\retval see NsmErrorStatus_e

This is a generic interface that can be used by the NSM to inform the NSMC about changes
to data items (i.e. events that have occurred in the system) */
NsmErrorStatus_e NsmcSetData(NsmDataType_e enData, unsigned char *pData, unsigned int u32DataLen);


/** \brief Request a NodeRestart.
\retval true:  The request for the restart could be processed. false: Error processing the restart request.

This will be used by the NSM to request a node restart when requested by one of its clients.*/
unsigned char NsmcRequestNodeRestart(NsmRestartReason_e enRestartReason, unsigned int u32RestartType);

/**********************************************************************************************************************
*
*  MACROS
*
**********************************************************************************************************************/

/* There are no macros defined */


#ifdef __cplusplus
}
#endif
/** \} */ /* End of SSW_NSMC_INTERFACE */
/** \} */ /* End of SSW_NSMC_TEMPLATE  */
#endif /* NSM_NODESTATEMACHINE_H */
