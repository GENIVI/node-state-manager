/**********************************************************************************************************************
*
* Copyright (C) 2012 Continental Automotive Systems, Inc.
*
* Author: Jean-Pierre.Bogler@continental-corporation.com
*
* Interface between NodeStateManager and IPC
*
* This source file is a part of the NodeStateAccess library (NSMA) stub.
* The architecture requires that the NodeStateManager (NSM) is independent
* from the D-Bus binding and generated code. The interface functions of the
* library have to be implemented according to their description.
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Date       Author             Reason
* 24.10.2012 Jean-Pierre Bogler CSP_WZ#1322: Initial creation
*
**********************************************************************************************************************/


/**********************************************************************************************************************
*
* Header includes
*
**********************************************************************************************************************/

/* generic includes for the NodeStateAccess library        */
#include "gio/gio.h"         /* glib types                 */
#include "NodeStateAccess.h" /* own header                 */
#include "NodeStateTypes.h"  /* Type defintions of the NSM */

/* additional includes to use D-Bus                        */



/**********************************************************************************************************************
*
* Local variables
*
**********************************************************************************************************************/

/* Variables to handle main loop and bus connection */


/**********************************************************************************************************************
*
* Prototypes for file local functions (see implementation for description)
*
**********************************************************************************************************************/


/**********************************************************************************************************************
*
* Local (static) functions
*
**********************************************************************************************************************/


/**********************************************************************************************************************
*
* Interfaces. Exported functions. See Header for detailed description.
*
**********************************************************************************************************************/

gboolean NSMA_boInit(const NSMA_tstObjectCallbacks *pstCallbacks)
{
	return FALSE;
}


gboolean NSMA_boWaitForEvents(void)
{
  return FALSE;
}


gboolean NSMA_boSendNodeStateSignal(const NsmNodeState_e enNodeState)
{
  return FALSE;
}


gboolean NSMA_boSendSessionSignal(const NsmSession_s *pstSession)
{
  return FALSE;
}


gboolean NSMA_boSendApplicationModeSignal(const NsmApplicationMode_e enApplicationMode)
{
  return FALSE;
}


gboolean NSMA_boSetBootMode(gint i32BootMode)
{
  return FALSE;
}


gboolean NSMA_boGetBootMode(gint *pi32BootMode)
{
  return FALSE;
}


gboolean NSMA_boSetRunningReason(const NsmRunningReason_e enRunningReason)
{
  return FALSE;
}


gboolean NSMA_boGetRunningReason(NsmRunningReason_e *penRunningReason)
{
  return FALSE;
}


gboolean NSMA_boSetShutdownReason(const NsmShutdownReason_e enShutdownReason)
{
  return FALSE;
}


gboolean NSMA_boGetShutdownReason(NsmShutdownReason_e *penShutdownReason)
{
  return FALSE;
}


gboolean NSMA_boSetRestartReason(const NsmRestartReason_e enRestartReason)
{
  return FALSE;
}


gboolean NSMA_boGetRestartReason(NsmRestartReason_e *penRestartReason)
{
  return FALSE;
}


gboolean NSMA_boQuitEventLoop(void)
{
  return FALSE;
}


gboolean NSMA_boFreeLcConsumerProxy(NSMA_tLcConsumerHandle hLcConsumer)
{
 return FALSE;
}


NSMA_tLcConsumerHandle NSMA_hCreateLcConsumer(const gchar* sBusName,
                                              const gchar* sObjName,
                                              const guint  u32TimeoutMs)
{
  return NULL;
}



gboolean NSMA_boCallLcClientRequest(NSMA_tLcConsumerHandle hLcClient,
                                    guint                  u32ShutdownType)
{
  return FALSE;
}


gboolean NSMA_boSetLcClientTimeout(NSMA_tLcConsumerHandle hClient, guint u32TimeoutMs)
{
  return FALSE;
}


gboolean NSMA_boGetLcClientTimeout(NSMA_tLcConsumerHandle hClient, guint *pu32TimeoutMs)
{
  return FALSE;
}


gboolean NSMA_boDeInit(void)
{
  return FALSE;
}
