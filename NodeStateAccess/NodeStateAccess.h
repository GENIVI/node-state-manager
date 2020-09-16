/**********************************************************************************************************************
 *
 * Copyright (C) 2013 Continental Automotive Systems, Inc.
 *               2017 BMW AG
 *
 * Author: Jean-Pierre.Bogler@continental-corporation.com
 *
 * Interface between NodeStateManager and IPC
 
 * This source file is a part of the NodeStateAccess library (NSMA).
 * The architecture requires that the NodeStateManager (NSM) is independent from the CommonAPI binding.
 * Therefore, the CommonAPI communication is handled inside of this library.
 * The library offers the NSM an interface to use objects generated via CommonAPI.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 **********************************************************************************************************************/

#ifndef NODESTATEACCESS_H_
#define NODESTATEACCESS_H_

/**********************************************************************************************************************
*
*  HEADER FILE INCLUDES
*
**********************************************************************************************************************/

#include "NodeStateTypes.h" /* NodeStateManagerTypes to communicate with NSM */
#include <gio/gio.h>        /* glib types for easier ICP connection   */
#ifdef __cplusplus
extern "C" {
#endif
/**********************************************************************************************************************
*
*  TYPE
*
**********************************************************************************************************************/

/* The type defines the structure for a lifecycle consumer client                               */
typedef struct
{
  size_t                  clientHash;        /* Identifier of the client base on capi client id */
  guint32                 u32RegisteredMode; /* Bit array of shutdown modes                     */
  guint32                 timeout;           /* Timeout in ms of the client                     */
  gboolean                boShutdown;        /* Only "run up" clients which are shut down       */
  gboolean                boPendingCall;     /* Client has a pending lifecycle call             */
} NSM__tstLifecycleClient;

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
typedef NsmErrorStatus_e (*NSMA_tpfRegisterLifecycleClientCb)  (const size_t                clientHash,
                                                                const guint                 u32ShutdownMode,
                                                                const guint                 u32TimeoutMs);
typedef NsmErrorStatus_e (*NSMA_tpfUnRegisterLifecycleClientCb)(const size_t                clientHash,
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
typedef void (*NSMA_tpfLifecycleReqFinish)(size_t clientID, gboolean timeout, gboolean late);

/* Type definition to wrap all callbacks in a structure */
typedef struct
{
  NSMA_tpfSetBootModeCb               pfSetBootModeCb;
  NSMA_tpfSetNodeStateCb              pfSetNodeStateCb;
  NSMA_tpfRequestNodeRestartCb        pfRequestNodeRestartCb;
  NSMA_tpfSetAppHealthStatusCb        pfSetAppHealthStatusCb;
  NSMA_tpfCheckLucRequiredCb          pfCheckLucRequiredCb;
  NSMA_tpfRegisterSessionCb           pfRegisterSessionCb;
  NSMA_tpfUnRegisterSessionCb         pfUnRegisterSessionCb;
  NSMA_tpfRegisterLifecycleClientCb   pfRegisterLifecycleClientCb;
  NSMA_tpfUnRegisterLifecycleClientCb pfUnRegisterLifecycleClientCb;
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
* The function is used to inform a LifecycleConsumer about a shutdown or runup request
*
* @param hLcClient:       Handle of the client (created with "NSMA_hCreateLcConsumer").
* @param u32ShutdownType: Shutdown type.
*
* @return TRUE:  Successfully called client
*         FALSE: Error calling the client.
*
**********************************************************************************************************************/
gboolean NSMA_boCallLcClientRequest(NSM__tstLifecycleClient *client, guint u32ShutdownType);

/**********************************************************************************************************************
*
* The function is used to inform a LifecycleConsumer about a shutdown or runup request without timeout
* This is used when a late LifecycleConsumer has returned to inform him about the current state when it
* has been changed since the last call to this LifecycleConsumer
*
* @param hLcClient:       Handle of the client (created with "NSMA_hCreateLcConsumer").
* @param u32ShutdownType: Shutdown type.
*
* @return TRUE:  Successfully called client
*         FALSE: Error calling the client.
*
**********************************************************************************************************************/
gboolean NSMA_boCallLcClientRequestWithoutTimeout(NSM__tstLifecycleClient *client, guint u32ShutdownType);


/**********************************************************************************************************************
*
* Returns true if a sequential client has a pending call that has not timed out
*
* @return TRUE:  Pending call active
*         FALSE: No pending call active
*
**********************************************************************************************************************/
gboolean NSMA__SequentialClientHasPendingActiveCall();

/**********************************************************************************************************************
*
* Returns true if parallel client has a pending call that has not timed out
*
* @param clientID:       ClientID to check for
*                        If clientID is 0 check if any parallel client has a pending call
*
* @return TRUE:  Pending call active
*         FALSE: No pending call active
*
**********************************************************************************************************************/
gboolean NSMA__ParallelClientHasPendingActiveCall(size_t clientID);

/**********************************************************************************************************************
*
* The function is used to inform multiple LifecycleConsumers about a shutdown or runup request.
* This is used for parallel shutdown.
*
* @param client:          List of Handle of the client (created with "NSMA_hCreateLcConsumer").
* @param numClients:      Number of clients in client list.
* @param u32ShutdownType: Shutdown type.
*
* @return TRUE:  Successfully called client
*         FALSE: Error calling the client.
*
**********************************************************************************************************************/
gboolean NSMA_boCallParallelLcClientsRequest(NSM__tstLifecycleClient *client, guint numClients, guint u32ShutdownType);


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
gboolean NSMA_boSetLcClientTimeout(NSM__tstLifecycleClient *client, guint u32TimeoutMs);


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
gboolean NSMA_boGetLcClientTimeout(NSM__tstLifecycleClient *client, guint *pu32TimeoutMs);

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
gboolean NSMA_boDeleteLifecycleClient(NSM__tstLifecycleClient *client);


/**********************************************************************************************************************
*
* The function is called to cancel the current LcCient Timeout.
*
**********************************************************************************************************************/
void NSMA_cancelLcClientTimeout();

/**********************************************************************************************************************
*
* The function is called to stop the current LcCient Timeout.
*
**********************************************************************************************************************/
void NSMA_stopLcClientTimeout();

/**********************************************************************************************************************
*
* The function is called to stop the current Parallel LcCient Timeout.
*
**********************************************************************************************************************/
void NSMA_stopParallelLcClientTimeout();

/**********************************************************************************************************************
*
* The function is called to cancel the current shutdown due to collective timeout.
*
**********************************************************************************************************************/
void NSMA_setLcCollectiveTimeout();


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
#ifdef __cplusplus
}
#endif

#endif /* NODESTATEACCESS_H_ */
