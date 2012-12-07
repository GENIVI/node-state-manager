#ifndef NODESTATEACCESS_H_
#define NODESTATEACCESS_H_

/**********************************************************************************************************************
*
* Copyright (C) 2012 Continental Automotive Systems, Inc.
*
* Author: Jean-Pierre.Bogler@continental-corporation.com
*
* Interface between NodeStateManager and IPC
*
* This header file is a part of the NodeStateAccess library (NSMA) stub.
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
*  HEADER FILE INCLUDES
*
**********************************************************************************************************************/

#include "NodeStateTypes.h" /* NodeStateTypes to communicate with NSM */
#include "gio/gio.h"        /* glib types for easier ICP connection   */

/**********************************************************************************************************************
*
*  TYPE
*
**********************************************************************************************************************/

/* Type definitions of callbacks that the NSM registers for the object interfaces */

typedef NsmErrorStatus_e (*NSMA_tpfSetBootModeCb)              (const gint                  i32BootMode);
typedef NsmErrorStatus_e (*NSMA_tpfSetNodeStateCb)             (const NsmNodeState_e        enNodeState);
typedef NsmErrorStatus_e (*NSMA_tpfSetAppModeCb)               (const NsmApplicationMode_e  enApplMode);
typedef NsmErrorStatus_e (*NSMA_tpfRequestNodeRestartCb)       (const NsmRestartReason_e    enRestartReason,
                                                                const guint                 u32RestartType);
typedef NsmErrorStatus_e (*NSMA_tpfSetAppHealthStatusCb)       (const gchar                *sAppName,
                                                                const gboolean              boAppState);
typedef gboolean         (*NSMA_tpfCheckLucRequiredCb)         (void);
typedef NsmErrorStatus_e (*NSMA_tpfRegisterSessionCb)          (const gchar                *sSessionName,
                                                                const gchar                *sSessionOwner,
                                                                const NsmSeat_e             enSeatId,
                                                                const NsmSessionState_e     ensessionState);
typedef NsmErrorStatus_e (*NSMA_tpfUnRegisterSessionCb)        (const gchar                *sSessionName,
                                                                const gchar                *sSessionOwner,
                                                                const NsmSeat_e             enSeatId);
typedef NsmErrorStatus_e (*NSMA_tpfRegisterLifecycleClientCb)  (const gchar                *sBusName,
                                                                const gchar                *sObjName,
                                                                const guint                 u32ShutdownMode,
                                                                const guint                 u32TimeoutMs);
typedef NsmErrorStatus_e (*NSMA_tpfUnRegisterLifecycleClientCb)(const gchar                *sBusName,
                                                                const gchar                *sObjName,
                                                                const guint                 u32ShutdownMode);
typedef NsmErrorStatus_e (*NSMA_tpfGetAppModeCb)               (NsmApplicationMode_e       *penAppMode);
typedef NsmErrorStatus_e (*NSMA_tpfGetSessionStateCb)          (const gchar                *sSessionName,
                                                                const NsmSeat_e             enSeatId,
                                                                NsmSessionState_e          *penSessionState);
typedef NsmErrorStatus_e (*NSMA_tpfGetNodeStateCb)             (NsmNodeState_e             *penNodeState);
typedef NsmErrorStatus_e (*NSMA_tpfSetSessionStateCb)          (const gchar                *sSessionName,
                                                                const gchar                *sSessionOwner,
                                                                const NsmSeat_e             enSeatId,
                                                                const NsmSessionState_e     enSessionState);
typedef guint (*NSMA_tpfGetAppHealthCountCb)                   (void);
typedef guint (*NSMA_tpfGetInterfaceVersionCb)                 (void);


/* Type definition for the management of Lifecycle clients */
typedef gpointer NSMA_tLcConsumerHandle;
typedef void (*NSMA_tpfLifecycleReqFinish)(const NsmErrorStatus_e enErrorStatus);

/* Type definition to wrap all callbacks in a structure */
typedef struct
{
  NSMA_tpfSetBootModeCb               pfSetBootModeCb;
  NSMA_tpfSetNodeStateCb              pfSetNodeStateCb;
  NSMA_tpfSetAppModeCb                pfSetAppModeCb;
  NSMA_tpfRequestNodeRestartCb        pfRequestNodeRestartCb;
  NSMA_tpfSetAppHealthStatusCb        pfSetAppHealthStatusCb;
  NSMA_tpfCheckLucRequiredCb          pfCheckLucRequiredCb;
  NSMA_tpfRegisterSessionCb           pfRegisterSessionCb;
  NSMA_tpfUnRegisterSessionCb         pfUnRegisterSessionCb;
  NSMA_tpfRegisterLifecycleClientCb   pfRegisterLifecycleClientCb;
  NSMA_tpfUnRegisterLifecycleClientCb pfUnRegisterLifecycleClientCb;
  NSMA_tpfGetAppModeCb                pfGetAppModeCb;
  NSMA_tpfGetSessionStateCb           pfGetSessionStateCb;
  NSMA_tpfGetNodeStateCb              pfGetNodeStateCb;
  NSMA_tpfSetSessionStateCb           pfSetSessionStateCb;
  NSMA_tpfGetAppHealthCountCb         pfGetAppHealthCountCb;
  NSMA_tpfGetInterfaceVersionCb       pfGetInterfaceVersionCb;
  NSMA_tpfLifecycleReqFinish          pfLcClientRequestFinish;
} NSMA_tstObjectCallbacks;


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

/**********************************************************************************************************************
*
* The function is called to initialize the NodeStateAccess library.
* It initializes the internal variables and creates a new GMainLoop.
*
* @return TRUE:  The NodeStateAccess library could be initialized.
*         FALSE: Error initializing the NodeStateAccess library.
*
**********************************************************************************************************************/
gboolean NSMA_boInit(const NSMA_tstObjectCallbacks *pstCallbacks);


/**********************************************************************************************************************
*
* The function is used to send the "NodeState" signal via the IPC.
*
* @param enNodeState: NodeState to be send.
*
* @return TRUE:  Signal has been send successfully.
*         FALSE: Error. Signal could not be send.
*
**********************************************************************************************************************/
gboolean NSMA_boSendNodeStateSignal(const NsmNodeState_e enNodeState);


/**********************************************************************************************************************
*
* The function is used to send the "SessionChanged" signal via the IPC.
*
* @param pstSession: Pointer to session structure that should be send.
*
* @return TRUE:  Signal has been send successfully.
*         FALSE: Error. Signal could not be send.
*
**********************************************************************************************************************/
gboolean NSMA_boSendSessionSignal(const NsmSession_s *pstSession);


/**********************************************************************************************************************
*
* The function is used to send the "ApplicationMode" signal via the IPC.
*
* @param enApplicationMode: ApplicationMode to be send.
*
* @return TRUE:  Signal has been send successfully.
*         FALSE: Error. Signal could not be send.
*
**********************************************************************************************************************/
gboolean NSMA_boSendApplicationModeSignal(const NsmApplicationMode_e enApplicationMode);


/**********************************************************************************************************************
*
* The function is used to set the value of the BootMode property.
*
* @param i32BootMode: New value of BootMode property.
*
* @return TRUE:  Successfully set the properties value.
*         FALSE: Error setting the properties value.
*
**********************************************************************************************************************/
gboolean NSMA_boSetBootMode(gint i32BootMode);


/**********************************************************************************************************************
*
* The function is used to get the value of the BootMode property.
*
* @param pi32BootMode: Pointer where to store the BootMode.
*
* @return TRUE:  Successfully got the properties value.
*         FALSE: Error getting the properties value.
*
**********************************************************************************************************************/
gboolean NSMA_boGetBootMode(gint *pi32BootMode);


/**********************************************************************************************************************
*
* The function is used to set the value of the RestartReason property.
*
* @param enRestartReason: New value of RestartReason property.
*
* @return TRUE:  Successfully set the properties value.
*         FALSE: Error setting the properties value.
*
**********************************************************************************************************************/
gboolean NSMA_boSetRestartReason(const NsmRestartReason_e enRestartReason);


/**********************************************************************************************************************
*
* The function is used to get the value of the RestartReason property.
*
* @param penRestartReason: Pointer where to store the RestartReason.
*
* @return TRUE:  Successfully got the properties value.
*         FALSE: Error getting the properties value.
*
**********************************************************************************************************************/
gboolean NSMA_boGetRestartReason(NsmRestartReason_e *penRestartReason);


/**********************************************************************************************************************
*
* The function is used to set the value of the WakeUpReason property.
*
* @param enRunningReason: New value of WakeUpReason property.
*
* @return TRUE:  Successfully set the properties value.
*         FALSE: Error setting the properties value.
*
**********************************************************************************************************************/
gboolean NSMA_boSetRunningReason(const NsmRunningReason_e enRunningReason);


/**********************************************************************************************************************
*
* The function is used to get the value of the RunningReason property.
*
* @param penRunningReason: Pointer where to store the RunningReason.
*
* @return TRUE:  Successfully got the properties value.
*         FALSE: Error getting the properties value.
*
**********************************************************************************************************************/
gboolean NSMA_boGetRunningReason(NsmRunningReason_e *penRunningReason);


/**********************************************************************************************************************
*
* The function is used to set the value of the ShutdownReason property.
*
* @param enShutdownReason: New value of ShutdownReason property.
*
* @return TRUE:  Successfully set the properties value.
*         FALSE: Error setting the properties value.
*
**********************************************************************************************************************/
gboolean NSMA_boSetShutdownReason(const NsmShutdownReason_e enShutdownReason);


/**********************************************************************************************************************
*
* The function is used to get the value of the ShutdownReason property.
*
* @param penShutdownReason: Pointer where to store the ShutdownReason.
*
* @return TRUE:  Successfully got the properties value.
*         FALSE: Error getting the properties value.
*
**********************************************************************************************************************/
gboolean NSMA_boGetShutdownReason(NsmShutdownReason_e *penShutdownReason);


/**********************************************************************************************************************
*
* The function is used to create a LifecycleConsumer client.
*
* @param sBusName:     Bus name of the client object.
* @param sObjName:     Object name of the client object.
* @param u32TimeoutMs: Timeout for client calls in ms.
*
* @return Handle to the new life cycle consumer or NULL, if there was an error.
*
**********************************************************************************************************************/
NSMA_tLcConsumerHandle NSMA_hCreateLcConsumer(const gchar* sBusName, const gchar* sObjName, const guint  u32TimeoutMs);


/**********************************************************************************************************************
*
* The function is used to call the "LifecycleRequest" method of a client.
*
* @param hLcClient:       Handle of the client (created with "NSMA_hCreateLcConsumer").
* @param u32ShutdownType: Shutdown type.
*
* @return TRUE:  Successfully called client
*         FALSE: Error calling the client.
*
**********************************************************************************************************************/
gboolean NSMA_boCallLcClientRequest(NSMA_tLcConsumerHandle hLcClient, guint u32ShutdownType);


/**********************************************************************************************************************
*
* The function is called to set the default timeout for calls to the life cycle client.
*
* @param hLcClient:    Handle of the life cycle client.
* @param u32TimeoutMs: Timeout value in ms.
*
* @return TRUE:  Successfully set timeout time for client.
*         FALSE: Error setting the clients timeout time.
*
**********************************************************************************************************************/
gboolean NSMA_boSetLcClientTimeout(NSMA_tLcConsumerHandle hClient, guint u32TimeoutMs);


/**********************************************************************************************************************
*
* The function is called to get the default timeout for calls to the life cycle client.
*
* @param hLcClient:     Handle of the life cycle client.
* @param pu32TimeoutMs: Pointer where to store the timeout value in ms.
*
* @return TRUE:  Successfully got timeout time for client.
*         FALSE: Error getting the clients timeout time.
*
**********************************************************************************************************************/
gboolean NSMA_boGetLcClientTimeout(NSMA_tLcConsumerHandle hClient, guint *pu32TimeoutMs);


/**********************************************************************************************************************
*
* The function is used to delete a "LifecycleRequest".
*
* @param hLcClient: Handle of the client (created with "NSMA_hCreateLcConsumer").
*
* @return TRUE:  Successfully freed clients memory.
*         FALSE: Error freeing the clients memory.
*
**********************************************************************************************************************/
gboolean NSMA_boFreeLcConsumerProxy(NSMA_tLcConsumerHandle hLcConsumer);


/**********************************************************************************************************************
*
* The function is blocking. It waits in a loop for events and forwards them to the related callback functions.
*
* @return TRUE:  Returned because of user call.
*         FALSE: Returned because of an internal error.
*
**********************************************************************************************************************/
gboolean NSMA_boWaitForEvents(void);


/**********************************************************************************************************************
*
* The function is used to force the return of "NSMA_boWaitForEvents".
*
* @return TRUE:  Accepted return request.
*         FALSE: Error. Return request not accepted.
*
**********************************************************************************************************************/
gboolean NSMA_boQuitEventLoop(void);


/**********************************************************************************************************************
*
* The function is de-initialize the NodeStateAccess library and release all memory used by it.
*
* @return TRUE:  Successfully de-initialized access library.
*         FALSE: Error de-initializing the library.
*
**********************************************************************************************************************/
gboolean NSMA_boDeInit(void);


#endif /* NODESTATEACCESS_H_ */
