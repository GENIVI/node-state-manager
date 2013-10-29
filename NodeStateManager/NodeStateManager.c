/**********************************************************************************************************************
*
* Copyright (C) 2013 Continental Automotive Systems, Inc.
*
* Author: Jean-Pierre.Bogler@continental-corporation.com
*
* Implementation of the NodeStateManager
*
* The NodeStateManager (NSM) is a central state manager for the system node. It manages the "NodeState",
* the "ApplicationMode" and many other states of the complete system. In addition, the NSM offers a
* session handling and a shutdown management.
* The NSM communicates with the NodeStateMachine (NSMC) to request and inform it about state changes
* and the NodeStateAccess (NSMA) to connect to the D-Bus.
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
**********************************************************************************************************************/


/**********************************************************************************************************************
*
* Header includes
*
**********************************************************************************************************************/
#include "NodeStateManager.h"               /* Own Header file                */
#include "NodeStateTypes.h"                 /* Typedefinitions to use the NSM */
#include "string.h"                         /* Memcpy etc.                    */
#include "gio/gio.h"                        /* GLib lists                     */
#include "dlt/dlt.h"                        /* DLT Log'n'Trace                */
#include "NodeStateMachine.h"               /* Talk to NodeStateMachine       */
#include "NodeStateAccess.h"                /* Access the IPC (D-Bus)         */
#include "syslog.h"                         /* Syslog messages                */
#include <systemd/sd-daemon.h>              /* Systemd wdog                   */
#include <persistence_client_library.h>     /* Init/DeInit PCL                */
#include <persistence_client_library_key.h> /* Access persistent data         */


/**********************************************************************************************************************
*
* Local defines, macros and type definitions.
*
**********************************************************************************************************************/

/* Defines to access persistence keys */
#define NSM_PERS_APPLICATION_MODE_DB  0xFF
#define NSM_PERS_APPLICATION_MODE_KEY "ERG_OIP_NSM_NODE_APPMODE"

/* The type defines the structure for a lifecycle consumer client                             */
typedef struct
{
  gchar                  *sBusName;          /* Bus name of the lifecycle client              */
  gchar                  *sObjName;          /* Object path of the client                     */
  guint32                 u32RegisteredMode; /* Bit array of shutdown modes                   */
  NSMA_tLcConsumerHandle  hClient;           /* Handle for proxy object for lifecycle client  */
  gboolean                boShutdown;        /* Only "run up" clients which are shut down     */
} NSM__tstLifecycleClient;


/* The type is used to store failed applications. A struct is used to allow extsions in future */
typedef struct
{
  gchar sName[NSM_MAX_SESSION_OWNER_LENGTH];
} NSM__tstFailedApplication;


/* List of names for the available default sessions, will are automatically provided by NSM    */
static const gchar* NSM__asDefaultSessions[] = { "DiagnosisSession",
                                                 "HevacSession",
                                                 "HmiActiveSession",
                                                 "NetworkActiveSession",
                                                 "NetworkPassiveSession",
                                                 "PdcSession",
                                                 "PermanentModeSession",
                                                 "PhoneSession",
                                                 "RvcSession",
                                                 "SwlSession",
                                                 "ProductLcSession",
                                                 "PlatformThermalSession",
                                                 "PlatformSupplySession",
                                                 "PersistencySession"
                                               };

/**********************************************************************************************************************
*
* Prototypes for file local functions (see implementation for description)
*
**********************************************************************************************************************/

/* Helper functions to destruct objects */
static void NSM__vFreeFailedApplicationObject(gpointer pFailedApplication);
static void NSM__vFreeSessionObject          (gpointer pSession          );
static void NSM__vFreeLifecycleClientObject  (gpointer pLifecycleClient  );


/* Helper functions to compare objects in lists */
static gboolean NSM__boIsPlatformSession           (NsmSession_s *pstSession);
static gint     NSM__i32LifecycleClientCompare     (gconstpointer pL1, gconstpointer pL2);
static gint     NSM__i32SessionOwnerNameSeatCompare(gconstpointer pS1, gconstpointer pS2);
static gint     NSM__i32SessionNameSeatCompare     (gconstpointer pS1, gconstpointer pS2);
static gint     NSM__i32SessionOwnerCompare        (gconstpointer pS1, gconstpointer pS2);
static gint     NSM__i32ApplicationCompare         (gconstpointer pA1, gconstpointer pA2);


/* Helper functions to recognize failed applications and disable their sessions */
static void             NSM__vDisableSessionsForApp(NSM__tstFailedApplication* pstFailedApp);
static NsmErrorStatus_e NSM__enSetAppStateFailed   (NSM__tstFailedApplication* pstFailedApp);
static NsmErrorStatus_e NSM__enSetAppStateValid    (NSM__tstFailedApplication* pstFailedApp);


/* Helper functions to control and start the "lifecycle request" sequence */
static void NSM__vCallNextLifecycleClient(void);
static void NSM__vOnLifecycleRequestFinish(const NsmErrorStatus_e enErrorStatus);


/* Internal functions, to set and get values. Indirectly used by D-Bus and StateMachine */
static NsmErrorStatus_e     NSM__enRegisterSession       (NsmSession_s *session,
		                                                  gboolean      boInformBus,
		                                                  gboolean      boInformMachine);
static NsmErrorStatus_e     NSM__enUnRegisterSession     (NsmSession_s *session,
		                                                  gboolean      boInformBus,
		                                                  gboolean      boInformMachine);
static NsmErrorStatus_e     NSM__enSetNodeState          (NsmNodeState_e       enNodeState,
                                                          gboolean             boInformBus,
                                                          gboolean             boInformMachine);
static NsmErrorStatus_e     NSM__enSetBootMode           (const gint           i32BootMode,
                                                          gboolean             boInformMachine);
static NsmErrorStatus_e     NSM__enSetApplicationMode    (NsmApplicationMode_e enApplicationMode,
                                                          gboolean             boInformBus,
                                                          gboolean             boInformMachine);
static NsmErrorStatus_e     NSM__enSetShutdownReason     (NsmShutdownReason_e  enNewShutdownReason,
                                                          gboolean             boInformMachine);

static void                 NSM__vPublishSessionChange   (NsmSession_s        *pstChangedSession,
                                                          gboolean             boInformBus,
                                                          gboolean             boInformMachine);
static NsmErrorStatus_e     NSM__enSetDefaultSessionState(NsmSession_s        *pstSession,
                                                          gboolean             boInformBus,
                                                          gboolean             boInformMachine);
static NsmErrorStatus_e     NSM__enSetProductSessionState(NsmSession_s        *pstSession,
                                                          gboolean             boInformBus,
                                                          gboolean             boInformMachine);
static NsmErrorStatus_e     NSM__enSetSessionState       (NsmSession_s        *pstSession,
                                                          gboolean             boInformBus,
                                                          gboolean             boInformMachine);
static NsmErrorStatus_e     NSM__enGetSessionState       (NsmSession_s        *pstSession);


/* Internal functions that are directly used from D-Bus and StateMachine */
static NsmErrorStatus_e     NSM__enGetNodeState      (NsmNodeState_e *penNodeState);
static NsmErrorStatus_e     NSM__enGetApplicationMode(NsmApplicationMode_e *penApplicationMode);


/* Callbacks for D-Bus interfaces of the NodeStateManager */
static NsmErrorStatus_e NSM__enOnHandleSetBootMode              (const gint                  i32BootMode);
static NsmErrorStatus_e NSM__enOnHandleSetNodeState             (const NsmNodeState_e        enNodeState);
static NsmErrorStatus_e NSM__enOnHandleSetApplicationMode       (const NsmApplicationMode_e  enApplMode);
static NsmErrorStatus_e NSM__enOnHandleRequestNodeRestart       (const NsmRestartReason_e    enRestartReason,
                                                                 const guint                 u32RestartType);
static NsmErrorStatus_e NSM__enOnHandleSetAppHealthStatus       (const gchar                *sAppName,
                                                                 const gboolean              boAppState);
static gboolean         NSM__boOnHandleCheckLucRequired         (void);
static NsmErrorStatus_e NSM__enOnHandleRegisterSession          (const gchar                *sSessionName,
                                                                 const gchar                *sSessionOwner,
                                                                 const NsmSeat_e             enSeatId,
                                                                 const NsmSessionState_e     enSessionState);
static NsmErrorStatus_e NSM__enOnHandleUnRegisterSession        (const gchar                *sSessionName,
                                                                 const gchar                *sSessionOwner,
                                                                 const NsmSeat_e             enSeatId);
static NsmErrorStatus_e NSM__enOnHandleRegisterLifecycleClient  (const gchar                *sBusName,
                                                                 const gchar                *sObjName,
                                                                 const guint                 u32ShutdownMode,
                                                                 const guint                 u32TimeoutMs);
static NsmErrorStatus_e NSM__enOnHandleUnRegisterLifecycleClient(const gchar                *sBusName,
                                                                 const gchar                *sObjName,
                                                                 const guint                 u32ShutdownMode);
static NsmErrorStatus_e NSM__enOnHandleGetSessionState          (const gchar                *sSessionName,
                                                                 const NsmSeat_e             enSeatId,
                                                                 NsmSessionState_e          *penSessionState);
static NsmErrorStatus_e NSM__enOnHandleSetSessionState          (const gchar                *sSessionName,
                                                                 const gchar                *sSessionOwner,
                                                                 const NsmSeat_e             enSeatId,
                                                                 const NsmSessionState_e     enSessionState);
static guint NSM__u32OnHandleGetAppHealthCount                  (void);
static guint NSM__u32OnHandleGetInterfaceVersion                (void);

/* Functions to simplify internal work flow */
static void  NSM__vInitializeVariables   (void);
static void  NSM__vCreatePlatformSessions(void);
static void  NSM__vCreateMutexes         (void);
static void  NSM__vDeleteMutexes         (void);

/* LTPROF helper function */
static void NSM__vLtProf(gchar *pszBus, gchar *pszObj, guint32 dwReason, gchar *pszInOut, guint32 dwValue);
static void NSM__vSyslogOpen(void);
static void NSM__vSyslogClose(void);

/* Systemd watchdog functions */
static gboolean NSM__boOnHandleTimerWdog(gpointer pUserData);
static void     NSM__vConfigureWdogTimer(void);

/**********************************************************************************************************************
*
* Local variables and constants
*
**********************************************************************************************************************/

/* Context for Log'n'Trace */
DLT_DECLARE_CONTEXT(NsmContext);

/* Variables for "Properties" hosted by the NSM */
static GMutex                    *NSM__pSessionMutex           = NULL;
static GSList                    *NSM__pSessions               = NULL;

static GList                     *NSM__pLifecycleClients       = NULL;

static GMutex                    *NSM__pNodeStateMutex         = NULL;
static NsmNodeState_e             NSM__enNodeState             = NsmNodeState_NotSet;

static GMutex                    *NSM__pNextApplicationModeMutex = NULL;
static GMutex                    *NSM__pThisApplicationModeMutex = NULL;
static NsmApplicationMode_e       NSM__enNextApplicationMode     = NsmApplicationMode_NotSet;
static NsmApplicationMode_e       NSM__enThisApplicationMode     = NsmApplicationMode_NotSet;
static gboolean                   NSM__boThisApplicationModeRead = FALSE;

static GSList                    *NSM__pFailedApplications     = NULL;

/* Variables for internal state management (of lifecycle requests) */
static NSM__tstLifecycleClient   *NSM__pCurrentLifecycleClient = NULL;

/* Constant array of callbacks which are registered at the NodeStateAccess library */
static const NSMA_tstObjectCallbacks NSM__stObjectCallBacks = { &NSM__enOnHandleSetBootMode,
                                                                &NSM__enOnHandleSetNodeState,
                                                                &NSM__enOnHandleSetApplicationMode,
                                                                &NSM__enOnHandleRequestNodeRestart,
                                                                &NSM__enOnHandleSetAppHealthStatus,
                                                                &NSM__boOnHandleCheckLucRequired,
                                                                &NSM__enOnHandleRegisterSession,
                                                                &NSM__enOnHandleUnRegisterSession,
                                                                &NSM__enOnHandleRegisterLifecycleClient,
                                                                &NSM__enOnHandleUnRegisterLifecycleClient,
                                                                &NSM__enGetApplicationMode,
                                                                &NSM__enOnHandleGetSessionState,
                                                                &NSM__enGetNodeState,
                                                                &NSM__enOnHandleSetSessionState,
                                                                &NSM__u32OnHandleGetAppHealthCount,
                                                                &NSM__u32OnHandleGetInterfaceVersion,
                                                                &NSM__vOnLifecycleRequestFinish
                                                              };

/**********************************************************************************************************************
*
* Local (static) functions
*
**********************************************************************************************************************/


/**********************************************************************************************************************
*
* This helper function is called from various places to check if a session is a "platform" session.
*
* @param  pstSession: Pointer to the session for which a check should be done, if it is a platform session
*
* @return TRUE:  The session is a "platform" session
*         FALSE: The session is not a "platform" session
*
**********************************************************************************************************************/
static gboolean NSM__boIsPlatformSession(NsmSession_s *pstSession)
{
  /* Function local variables */
  gboolean boIsPlatformSession = FALSE;
  guint16  u16SessionIdx       = 0;

  for(u16SessionIdx = 0;
         (u16SessionIdx       <  sizeof(NSM__asDefaultSessions)/sizeof(gchar*))
      && (boIsPlatformSession == FALSE);
      u16SessionIdx++)
  {
    boIsPlatformSession = (g_strcmp0(pstSession->sName, NSM__asDefaultSessions[u16SessionIdx]) == 0);
  }

  return boIsPlatformSession;
}


/**
* NSM__enRegisterSession:
* @session:         Ptr to NsmSession_s structure containing data to register a session
* @boInformBus:     Flag whether the a dbus signal should be send to inform about the new session
* @boInformMachine: Flag whether the NSMC should be informed about the new session
*
* The internal function is used to register a session. It is either called from the dbus callback
* or it is called via the internal context of the NSMC.
*/
static NsmErrorStatus_e NSM__enRegisterSession(NsmSession_s *session, gboolean boInformBus, gboolean boInformMachine)
{
  /* Function local variables                                              */
  NsmErrorStatus_e enRetVal     = NsmErrorStatus_NotSet; /* Return value   */
  NsmSession_s     *pNewSession = NULL;  /* Pointer to new created session */
  GSList           *pListEntry  = NULL;  /* Pointer to list entry          */

  if(    (g_strcmp0(session->sOwner, NSM_DEFAULT_SESSION_OWNER) != 0)
      && (session->enState                                      > NsmSessionState_Unregistered))
  {
	  if(NSM__boIsPlatformSession(session) == FALSE)
	  {
	    g_mutex_lock(NSM__pSessionMutex);

	    pListEntry = g_slist_find_custom(NSM__pSessions, session, &NSM__i32SessionNameSeatCompare);

	    if(pListEntry == NULL)
	    {
	      enRetVal = NsmErrorStatus_Ok;

	      pNewSession  = g_new0(NsmSession_s, 1);
	      memcpy(pNewSession, session, sizeof(NsmSession_s));

	      DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Registered session."                          ),
	                                        DLT_STRING(" Name: "         ), DLT_STRING(session->sName      ),
	                                        DLT_STRING(" Owner: "        ), DLT_STRING(session->sOwner     ),
	                                        DLT_STRING(" Seat: "         ), DLT_INT((gint) session->enSeat ),
	                                        DLT_STRING(" Initial state: "), DLT_INT((gint) session->enState));

	      /* Return OK and append new object */
	      NSM__pSessions = g_slist_append(NSM__pSessions, pNewSession);

	      /* Inform D-Bus and StateMachine about the new session. */
	      NSM__vPublishSessionChange(pNewSession, boInformBus, boInformMachine);
	    }
	    else
	    {
	      /* Error: The session already exists. Don't store passed state. */
	      enRetVal = NsmErrorStatus_WrongSession;
	      DLT_LOG(NsmContext, DLT_LOG_WARN, DLT_STRING("NSM: Failed to register session. Session already exists."),
	                                        DLT_STRING(" Name: "         ), DLT_STRING(session->sName            ),
	                                        DLT_STRING(" Owner: "        ), DLT_STRING(session->sOwner           ),
	                                        DLT_STRING(" Seat: "         ), DLT_INT((gint) session->enSeat       ),
	                                        DLT_STRING(" Initial state: "), DLT_INT((gint) session->enState      ));
	    }

	    g_mutex_unlock(NSM__pSessionMutex);
	  }
	  else
	  {
	    /* Error: It is not allowed to re-register a default session! */
	    enRetVal = NsmErrorStatus_Parameter;
	    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to register session. Re-Registration of default session not allowed."),
	                                       DLT_STRING(" Name: "         ), DLT_STRING(session->sName                                    ),
	                                       DLT_STRING(" Owner: "        ), DLT_STRING(session->sOwner                                   ),
	                                       DLT_STRING(" Seat: "         ), DLT_INT((gint) session->enSeat                               ),
	                                       DLT_STRING(" Initial state: "), DLT_INT((gint) session->enState                              ));
	  }
  }
  else
  {
    /* Error: A parameter with an invalid value has been passed */
    enRetVal = NsmErrorStatus_Parameter;
    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to register session. Invalid owner or state."),
                                       DLT_STRING(" Name: "         ), DLT_STRING(session->sName            ),
                                       DLT_STRING(" Owner: "        ), DLT_STRING(session->sOwner           ),
                                       DLT_STRING(" Seat: "         ), DLT_INT((gint) session->enSeat       ),
                                       DLT_STRING(" Initial state: "), DLT_INT((gint) session->enState      ));
  }

  return enRetVal;
}


/**
* NSM__enUnRegisterSession:
* @session:         Ptr to NsmSession_s structure containing data to unregister a session
* @boInformBus:     Flag whether the a dbus signal should be send to inform about the lost session
* @boInformMachine: Flag whether the NSMC should be informed about the lost session
*
* The internal function is used to unregister a session. It is either called from the dbus callback
* or it is called via the internal context of the NSMC.
*/
static NsmErrorStatus_e NSM__enUnRegisterSession(NsmSession_s *session, gboolean boInformBus, gboolean boInformMachine)
{
  /* Function local variables                                                                */
  NsmErrorStatus_e  enRetVal         = NsmErrorStatus_NotSet; /* Return value                */
  NsmSession_s     *pExistingSession = NULL;                  /* Pointer to existing session */
  GSList           *pListEntry       = NULL;                  /* Pointer to list entry       */

  if(NSM__boIsPlatformSession(session) == FALSE)
  {
    g_mutex_lock(NSM__pSessionMutex);

    pListEntry = g_slist_find_custom(NSM__pSessions, session, &NSM__i32SessionOwnerNameSeatCompare);

    /* Check if the session exists */
    if(pListEntry != NULL)
    {
      /* Found the session in the list. Now remove it. */
      enRetVal = NsmErrorStatus_Ok;
      pExistingSession = (NsmSession_s*) pListEntry->data;

      DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Unregistered session."                          ),
                                        DLT_STRING(" Name: "      ), DLT_STRING(pExistingSession->sName  ),
                                        DLT_STRING(" Owner: "     ), DLT_STRING(pExistingSession->sOwner ),
                                        DLT_STRING(" Seat: "      ), DLT_INT(   pExistingSession->enSeat ),
                                        DLT_STRING(" Last state: "), DLT_INT(   pExistingSession->enState));

      pExistingSession->enState = NsmSessionState_Unregistered;

      /* Inform D-Bus and StateMachine about the unregistered session */
      NSM__vPublishSessionChange(pExistingSession, boInformBus, boInformMachine);

      NSM__vFreeSessionObject(pExistingSession);
      NSM__pSessions = g_slist_remove(NSM__pSessions, pExistingSession);
    }
    else
    {
      /* Error: The session is unknown. */
      enRetVal = NsmErrorStatus_WrongSession;
      DLT_LOG(NsmContext, DLT_LOG_WARN, DLT_STRING("NSM: Failed to unregister session. Session unknown."),
                                        DLT_STRING(" Name: "      ), DLT_STRING(session->sName          ),
                                        DLT_STRING(" Owner: "     ), DLT_STRING(session->sOwner         ),
                                        DLT_STRING(" Seat: "      ), DLT_INT((gint) session->enSeat     ));
    }

    g_mutex_unlock(NSM__pSessionMutex);
  }
  else
  {
    /* Error: Failed to unregister session. The passed session is a "platform" session. */
    enRetVal = NsmErrorStatus_WrongSession;
    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to unregister session. The session is a platform session."),
                                       DLT_STRING(" Name: "      ), DLT_STRING(session->sName                            ),
                                       DLT_STRING(" Owner: "     ), DLT_STRING(session->sOwner                           ),
                                       DLT_STRING(" Seat: "      ), DLT_INT((gint) session->enSeat                       ));
  }

  return enRetVal;
}


/**********************************************************************************************************************
*
* The function is called from IPC and StateMachine to set the NodeState.
*
* @param enNodeState:     New NodeState that should be stored.
* @param boInformBus:     Defines whether a D-Bus signal should be send when the NodeState could be changed.
* @param boInformMachine: Defines whether the StateMachine should be informed about the new NodeState.
*
* @return see NsmErrorStatus_e
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enSetNodeState(NsmNodeState_e enNodeState, gboolean boInformBus, gboolean boInformMachine)
{
  /* Function local variables                                        */
  NsmErrorStatus_e enRetVal = NsmErrorStatus_NotSet; /* Return value */

  /* Check if the passed parameter is valid */
  if((enNodeState > NsmNodeState_NotSet) && (enNodeState < NsmNodeState_Last))
  {
    /* Assert that the Node not already is shut down. Otherwise it will switch of immediately */
    enRetVal = NsmErrorStatus_Ok;

    g_mutex_lock(NSM__pNodeStateMutex);

    /* Only store the new value and emit a signal, if the new value is different */
    if(NSM__enNodeState != enNodeState)
    {
      /* Store the last NodeState, before switching to the new one */
      DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Changed NodeState."                           ),
                                        DLT_STRING(" Old NodeState: "), DLT_INT((gint) NSM__enNodeState),
                                        DLT_STRING(" New NodeState: "), DLT_INT((gint) enNodeState     ));


      /* Store the passed NodeState and emit a signal to inform system that the NodeState changed */
      NSM__enNodeState = enNodeState;

      /* If required, inform the D-Bus about the change (send signal) */
      if(boInformBus == TRUE)
      {
        (void) NSMA_boSendNodeStateSignal(NSM__enNodeState);
      }

      /* If required, inform the StateMachine about the change */
      if(boInformMachine == TRUE)
      {
        NsmcSetData(NsmDataType_NodeState, (unsigned char*) &NSM__enNodeState,  sizeof(NsmDataType_NodeState));
      }

      /* Leave the lock now, because its not recursive. 'NSM__vCallNextLifecycleClient' may need it. */
      g_mutex_unlock(NSM__pNodeStateMutex);

      /* Check if a new life cycle request needs to be started based on the new ShutdownType */
      if(NSM__pCurrentLifecycleClient == NULL)
      {
        NSM__vCallNextLifecycleClient();
      }
    }
    else
    {
      /* NodeState stays the same. Just leave the lock. */
      g_mutex_unlock(NSM__pNodeStateMutex);
    }
  }
  else
  {
    /* Error: The passed boot mode is invalid. Return an error. */
    enRetVal = NsmErrorStatus_Parameter;
    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to change NodeState. Invalid parameter."),
                                       DLT_STRING(" Old NodeState: "),     DLT_INT(NSM__enNodeState    ),
                                       DLT_STRING(" Desired NodeState: "), DLT_INT((gint) enNodeState) );
  }

  return enRetVal;
}


/**********************************************************************************************************************
*
* The function is called from IPC and StateMachine to get the NodeState.
*
* @return see NsmNodeState_e
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enGetNodeState(NsmNodeState_e *penNodeState)
{
  NsmErrorStatus_e enRetVal = NsmErrorStatus_NotSet;

  if(penNodeState != NULL)
  {
    enRetVal = NsmErrorStatus_Ok;

    g_mutex_lock(NSM__pNodeStateMutex);
    *penNodeState = NSM__enNodeState;
    g_mutex_unlock(NSM__pNodeStateMutex);
  }
  else
  {
    enRetVal = NsmErrorStatus_Parameter;
  }

  return enRetVal;
}


/**********************************************************************************************************************
*
* The function is called from IPC and StateMachine to set the BootMode.
*
* @param i32BootMode:     New BootMode that should be stored.
* @param boInformBus:     Defines whether a D-Bus signal should be send when the BootMode could be changed.
* @param boInformMachine: Defines whether the StateMachine should be informed about the new BootMode.
*
* @return see NsmErrorStatus_e
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enSetBootMode(const gint i32BootMode, gboolean boInformMachine)
{
  /* Function local variables */
  gint             i32CurrentBootMode = 0;
  NsmErrorStatus_e enRetVal           = NsmErrorStatus_NotSet;

  /* The BootMode property should be thread safe by D-Bus. No critical section need.  */
  (void) NSMA_boGetBootMode(&i32CurrentBootMode);
  enRetVal           = NsmErrorStatus_Ok;

  if(i32CurrentBootMode != i32BootMode)
  {
    (void) NSMA_boSetBootMode(i32BootMode);

    DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Changed BootMode."                      ),
                                      DLT_STRING(" Old BootMode: "), DLT_INT(i32CurrentBootMode),
                                      DLT_STRING(" New BootMode: "), DLT_INT(i32BootMode       ));

    /* Inform the machine if desired. The D-Bus will auto. update, because this is property */
    if(boInformMachine == TRUE)
    {
       NsmcSetData(NsmDataType_BootMode, (unsigned char*) &i32BootMode,  sizeof(gint));
    }
  }

  /* Return ok. There is no limitation for this value. */
  return enRetVal;
}

/**********************************************************************************************************************
*
* The function is called from IPC and StateMachine to set the ApplicationMode.
*
* @param enApplicationMode: New application mode that should be stored.
* @param boInformBus:       Defines whether a D-Bus signal should be send when the ApplicationMode could be changed.
* @param boInformMachine:   Defines whether the StateMachine should be informed about the new ApplicationMode.
*
* @return see NsmErrorStatus_e
*
**********************************************************************************************************************/
static NsmErrorStatus_e
NSM__enSetApplicationMode(NsmApplicationMode_e enApplicationMode,
                          gboolean             boInformBus,
                          gboolean             boInformMachine)
{
  /* Function local variables                                          */
  NsmErrorStatus_e enRetVal   = NsmErrorStatus_NotSet; /* Return value */
  int              pcl_return = 0;

  /* Check if the passed parameter is valid */
  if(    (enApplicationMode > NsmApplicationMode_NotSet)
      && (enApplicationMode < NsmApplicationMode_Last  ))
  {
    /* The passed parameter is valid. Return OK */
    enRetVal = NsmErrorStatus_Ok;

    g_mutex_lock(NSM__pNextApplicationModeMutex);

    /* Only store new value and emit signal, if new value is different */
    if(NSM__enNextApplicationMode != enApplicationMode)
    {
      /* Store new value and emit signal with new application mode */
      DLT_LOG(NsmContext,
              DLT_LOG_INFO,
              DLT_STRING("NSM: Changed ApplicationMode.");
              DLT_STRING("Old AppMode:"); DLT_INT((int) NSM__enNextApplicationMode);
              DLT_STRING("New AppMode:"); DLT_INT((int) enApplicationMode));

      NSM__enNextApplicationMode = enApplicationMode;

      /* If original persistent value has not been read before, get it now! */
      g_mutex_lock(NSM__pThisApplicationModeMutex);

      if(NSM__boThisApplicationModeRead == FALSE)
      {
        /* Get data from persistence */
        pcl_return = pclKeyReadData(NSM_PERS_APPLICATION_MODE_DB,
                                    NSM_PERS_APPLICATION_MODE_KEY,
                                    0,
                                    0,
                                    (unsigned char*) &NSM__enThisApplicationMode,
                                    sizeof(NSM__enThisApplicationMode));

        if(pcl_return != sizeof(NSM__enThisApplicationMode))
        {
          NSM__enThisApplicationMode = NsmApplicationMode_NotSet;
          DLT_LOG(NsmContext,
                  DLT_LOG_WARN,
                  DLT_STRING("NSM: Failed to read ApplicationMode.");
                  DLT_STRING("Error: Unexpected PCL return.");
                  DLT_STRING("Return:"); DLT_INT(pcl_return));
        }

        NSM__boThisApplicationModeRead = TRUE;
      }

      g_mutex_unlock(NSM__pThisApplicationModeMutex);

      /* Write the new application mode to persistence */
      pcl_return = pclKeyWriteData(NSM_PERS_APPLICATION_MODE_DB,
                                   NSM_PERS_APPLICATION_MODE_KEY,
                                   0,
                                   0,
                                   (unsigned char*) &NSM__enNextApplicationMode,
                                   sizeof(NSM__enNextApplicationMode));

      if(pcl_return != sizeof(NSM__enNextApplicationMode))
      {
        DLT_LOG(NsmContext,
                DLT_LOG_ERROR,
                DLT_STRING("NSM: Failed to persist ApplicationMode.");
                DLT_STRING("Error: Unexpected PCL return.");
                DLT_STRING("Return:"); DLT_INT(pcl_return));
      }

      if(boInformBus == TRUE)
      {
        NSMA_boSendApplicationModeSignal(NSM__enNextApplicationMode);
      }

      if(boInformMachine == TRUE)
      {
         NsmcSetData(NsmDataType_AppMode,
                     (unsigned char*) &NSM__enNextApplicationMode,
                     sizeof(NsmApplicationMode_e));
      }
    }

    g_mutex_unlock(NSM__pNextApplicationModeMutex);
  }
  else
  {
    /* Error: The passed application mode is invalid. Return an error. */
    enRetVal = NsmErrorStatus_Parameter;
    DLT_LOG(NsmContext,
            DLT_LOG_ERROR,
            DLT_STRING("NSM: Failed to change ApplicationMode.");
            DLT_STRING("Error:"); DLT_STRING("Invalid parameter.");
            DLT_STRING("Old AppMode:"); DLT_INT((int) NSM__enNextApplicationMode);
            DLT_STRING("New AppMode:"); DLT_INT((int) enApplicationMode));
  }

  return enRetVal;
}


/**********************************************************************************************************************
*
* The function is called from IPC and StateMachine to get the ApplicationMode.
*
* @return see NsmApplicationMode_e
*
**********************************************************************************************************************/
static NsmErrorStatus_e
NSM__enGetApplicationMode(NsmApplicationMode_e *penApplicationMode)
{
  NsmErrorStatus_e enRetVal   = NsmErrorStatus_NotSet;
  int              pcl_return = 0;

  if(penApplicationMode != NULL)
  {
    g_mutex_lock(NSM__pThisApplicationModeMutex);

    /* Check if value already was obtained from persistence */
    if(NSM__boThisApplicationModeRead == FALSE)
    {
      /* There was no read attempt before. Read from persistence */
      pcl_return = pclKeyReadData(NSM_PERS_APPLICATION_MODE_DB,
                                  NSM_PERS_APPLICATION_MODE_KEY,
                                  0,
                                  0,
                                  (unsigned char*) &NSM__enThisApplicationMode,
                                  sizeof(NSM__enThisApplicationMode));

      /* Check the PCL return */
      if(pcl_return != sizeof(NSM__enThisApplicationMode))
      {
        /* Read failed. From now on always return 'NsmApplicationMode_NotSet' */
        NSM__enThisApplicationMode = NsmApplicationMode_NotSet;
        DLT_LOG(NsmContext,
                DLT_LOG_WARN,
                DLT_STRING("NSM: Failed to read ApplicationMode.");
                DLT_STRING("Error: Unexpected PCL return.");
                DLT_STRING("Return:"); DLT_INT(pcl_return));
      }

      /* There was a first read attempt from persistence */
      NSM__boThisApplicationModeRead = TRUE;
    }

    enRetVal = NsmErrorStatus_Ok;
    *penApplicationMode = NSM__enThisApplicationMode;

    g_mutex_unlock(NSM__pThisApplicationModeMutex);
  }
  else
  {
    enRetVal = NsmErrorStatus_Parameter;
  }

  return enRetVal;
}


/**********************************************************************************************************************
*
* The function is called from the StateMachine. There is no D-Bus interface to set the ShutdownReason,
* because it is a property.
*
* @param enNewShutdownReason: New ShutdownReason that should be stored.
* @param boInformMachine:     Determines if StateMachine needs to be called on a successful change.
*                             Most of the time this should be false, because the machine sets the
*                             value and can check the return value for errors.
*
* @return see NsmErrorStatus_e
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enSetShutdownReason(NsmShutdownReason_e enNewShutdownReason, gboolean boInformMachine)
{
  /* Function local variables                                                          */
  NsmErrorStatus_e    enRetVal                = NsmErrorStatus_NotSet; /* Return value */
  NsmShutdownReason_e enCurrentShutdownReason = NsmShutdownReason_NotSet;

  /* Check if the passed parameter is valid */
  if((enNewShutdownReason > NsmShutdownReason_NotSet) && (enNewShutdownReason < NsmShutdownReason_Last))
  {
    /* The passed parameter is valid. Return OK */
    enRetVal = NsmErrorStatus_Ok;
    (void) NSMA_boGetShutdownReason(&enCurrentShutdownReason);

    /* Only store the new value and emit a signal, if the new value is different */
    if(enNewShutdownReason != enCurrentShutdownReason)
    {
      /* Store new value and emit signal with new application mode */
      DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Changed ShutdownReason."),
                                        DLT_STRING(" Old ShutdownReason: "), DLT_INT((gint) enCurrentShutdownReason),
                                        DLT_STRING(" New ShutdownReason: "), DLT_INT((gint) enNewShutdownReason    ));

      (void) NSMA_boSetShutdownReason(enNewShutdownReason);

      if(boInformMachine == TRUE)
      {
         NsmcSetData(NsmDataType_ShutdownReason, (unsigned char*) &enNewShutdownReason, sizeof(NsmShutdownReason_e));
      }
    }
  }
  else
  {
    /* Error: The passed application mode is invalid. Return an error. */
    enRetVal = NsmErrorStatus_Parameter;
    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to change ShutdownReason. Invalid parameter."          ),
                                       DLT_STRING(" Old ShutdownReason: "),     DLT_INT((gint) enCurrentShutdownReason),
                                       DLT_STRING(" Desired ShutdownReason: "), DLT_INT((gint) enNewShutdownReason    ));
  }

  return enRetVal;
}

/**********************************************************************************************************************
*
* The function is called when a session state changed. It informs the system (IPC and StateMachine) about
* the changed session state.
*
* @param pstSession:      Pointer to structure with updated session information.
* @param boInformBus:     Defines whether a D-Bus signal should be send on session change.
* @param boInformMachine: Defines whether the StateMachine should be informed about session change.
*
**********************************************************************************************************************/
static void NSM__vPublishSessionChange(NsmSession_s *pstChangedSession, gboolean boInformBus, gboolean boInformMachine)
{
  NsmErrorStatus_e enStateMachineReturn = NsmErrorStatus_NotSet;

  if(boInformBus == TRUE)
  {
    NSMA_boSendSessionSignal(pstChangedSession);
  }

  if(boInformMachine == TRUE)
  {
    enStateMachineReturn = NsmcSetData(NsmDataType_SessionState, (unsigned char*) pstChangedSession, sizeof(NsmSession_s));

    if(enStateMachineReturn != NsmErrorStatus_Ok)
    {
      DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to inform state machine about changed session state."       ),
                                         DLT_STRING(" State machine returned: "),      DLT_INT(   enStateMachineReturn       ),
                                         DLT_STRING(" Application: "),                 DLT_STRING(pstChangedSession->sOwner  ),
                                         DLT_STRING(" Session: "),                     DLT_STRING(pstChangedSession->sName   ),
                                         DLT_STRING(" Seat: "),                        DLT_INT(   pstChangedSession->enSeat  ),
                                         DLT_STRING(" Desired state: "),               DLT_INT(   pstChangedSession->enState));
    }
  }
}


/**********************************************************************************************************************
*
* The function is called when the state of a product session should be changed.
*
* @param pstSession:      Pointer to structure where session name, owner, seat and desired SessionState are defined.
* @param boInformBus:     Defines whether a D-Bus signal should be send on session change.
* @param boInformMachine: Defines whether the StateMachine should be informed about session change.
*
* @return see NsmErrorStatus_e
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enSetProductSessionState(NsmSession_s *pstSession, gboolean boInformBus, gboolean boInformMachine)
{
  /* Function local variables                                         */
  NsmErrorStatus_e  enRetVal = NsmErrorStatus_NotSet; /* Return value */
  GSList           *pListEntry                   = NULL;
  NsmSession_s     *pExistingSession             = NULL;

  g_mutex_lock(NSM__pSessionMutex);

  pListEntry = g_slist_find_custom(NSM__pSessions, pstSession, &NSM__i32SessionOwnerNameSeatCompare);

  if(pListEntry != NULL)
  {
    enRetVal = NsmErrorStatus_Ok;
    pExistingSession = (NsmSession_s*) pListEntry->data;

    if(pExistingSession->enState != pstSession->enState)
    {
      pExistingSession->enState = pstSession->enState;
      NSM__vPublishSessionChange(pExistingSession, boInformBus, boInformMachine);
    }
  }
  else
  {
    enRetVal = NsmErrorStatus_WrongSession;
    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to set session state. Session unknown."),
                                       DLT_STRING(" Application: "),   DLT_STRING(pstSession->sOwner  ),
                                       DLT_STRING(" Session: "),       DLT_STRING(pstSession->sName   ),
                                       DLT_STRING(" Seat: "),          DLT_INT(   pstSession->enSeat  ),
                                       DLT_STRING(" Desired state: "), DLT_INT(   pstSession->enState));
  }

  g_mutex_unlock(NSM__pSessionMutex);

  return enRetVal;
}


/**********************************************************************************************************************
*
* The function is called when the state of a default session should be changed.
*
* @param pstSession:      Pointer to structure where session name, owner, seat and desired SessionState are defined.
* @param boInformBus:     Defines whether a D-Bus signal should be send on session change.
* @param boInformMachine: Defines whether the StateMachine should be informed about session change.
*
* @return see NsmErrorStatus_e
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enSetDefaultSessionState(NsmSession_s *pstSession, gboolean boInformBus, gboolean boInformMachine)
{
  /* Function local variables                                                  */
  NsmErrorStatus_e  enRetVal          = NsmErrorStatus_NotSet; /* Return value */
  GSList           *pListEntry        = NULL;
  NsmSession_s     *pExistingSession  = NULL;

  /* Lock the sessions to be able to change them! */
  g_mutex_lock(NSM__pSessionMutex);

  pListEntry = g_slist_find_custom(NSM__pSessions, pstSession, &NSM__i32SessionNameSeatCompare);

  if(pListEntry != NULL)
  {
    pExistingSession = (NsmSession_s*) pListEntry->data;

    /* Check that the caller owns the session */
    if(g_strcmp0(pExistingSession->sOwner, pstSession->sOwner) == 0)
    {
      enRetVal = NsmErrorStatus_Ok;

      if(pExistingSession->enState != pstSession->enState)
      {
        DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Changed default session's state."),
                                          DLT_STRING(" Application: "), DLT_STRING(pExistingSession->sOwner ),
                                          DLT_STRING(" Session: "),     DLT_STRING(pExistingSession->sName  ),
                                          DLT_STRING(" Seat: "),        DLT_INT(   pExistingSession->enSeat ),
                                          DLT_STRING(" Old state: "),   DLT_INT(   pExistingSession->enState),
                                          DLT_STRING(" New state: "),   DLT_INT(   pstSession->enState      ));

        pExistingSession->enState = pstSession->enState;

        NSM__vPublishSessionChange(pExistingSession, boInformBus, boInformMachine);

        if(pstSession->enState == NsmSessionState_Inactive)
        {
          g_strlcpy(pExistingSession->sOwner, NSM_DEFAULT_SESSION_OWNER, sizeof(pExistingSession->sOwner));
        }
      }
    }
    else
    {
      /* The caller does not own the session. Check if he can become the owner. */
      if(g_strcmp0(pExistingSession->sOwner, NSM_DEFAULT_SESSION_OWNER) == 0)
      {
        /* The session has no owner. The new owner can obtain the session by setting it to an "active" state */
        if(pstSession->enState != NsmSessionState_Inactive)
        {
          /* The session has been activated. Overtake the owner. Broadcast new state. */
          enRetVal = NsmErrorStatus_Ok;
          g_strlcpy(pExistingSession->sOwner, pstSession->sOwner, sizeof(pExistingSession->sOwner));

          DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Changed default session's state."),
                                            DLT_STRING(" Application: "), DLT_STRING(pExistingSession->sOwner ),
                                            DLT_STRING(" Session: "),     DLT_STRING(pExistingSession->sName  ),
                                            DLT_STRING(" Seat: "),        DLT_INT(   pExistingSession->enSeat ),
                                            DLT_STRING(" Old state: "),   DLT_INT(   pExistingSession->enState),
                                            DLT_STRING(" New state: "),   DLT_INT(   pstSession->enState      ));

          pExistingSession->enState = pstSession->enState;

          NSM__vPublishSessionChange(pExistingSession, boInformBus, boInformMachine);
        }
        else
        {
          /* The session has no owner, but could not be activated because the passed state is "inactive". */
          enRetVal = NsmErrorStatus_Parameter;

          DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to enable default session. Passed state is 'inactive'. "),
                                             DLT_STRING(" Session: "),                DLT_STRING(pstSession->sName           ),
                                             DLT_STRING(" Seat: "),                   DLT_INT(   pstSession->enSeat          ),
                                             DLT_STRING(" Owning     application: "), DLT_STRING(pExistingSession->sOwner    ),
                                             DLT_STRING(" Requesting application: "), DLT_STRING(pstSession->sOwner          ));
        }
      }
      else
      {
        /* The session owners do not match and the existing session has an owner */
        enRetVal = NsmErrorStatus_Error;

        DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to set default session state. Session has another owner."),
                                           DLT_STRING(" Session: "),                DLT_STRING(pstSession->sName            ),
                                           DLT_STRING(" Seat: "),                   DLT_INT(   pstSession->enSeat           ),
                                           DLT_STRING(" Owning     application: "), DLT_STRING(pExistingSession->sOwner     ),
                                           DLT_STRING(" Requesting application: "), DLT_STRING(pstSession->sOwner           ));
      }
    }
  }
  else
  {
    /* This should never happen, because the function is only called for default sessions! */
    enRetVal = NsmErrorStatus_Internal;
    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Critical error. Default session not found in session list!"),
                                       DLT_STRING(" Application: "),   DLT_STRING(pstSession->sOwner               ),
                                       DLT_STRING(" Session: "),       DLT_STRING(pstSession->sName                ),
                                       DLT_STRING(" Seat: "),          DLT_INT(   pstSession->enSeat               ),
                                       DLT_STRING(" Desired state: "), DLT_INT(   pstSession->enState              ));
  }

  /* Unlock the sessions again. */
  g_mutex_unlock(NSM__pSessionMutex);

  return enRetVal;
}


/**********************************************************************************************************************
*
* The function is called from IPC and StateMachine to set a session state.
*
* @param pstSession:      Pointer to structure where session name, owner, seat and desired SessionState are defined.
* @param boInformBus:     Defines whether a D-Bus signal should be send on session change.
* @param boInformMachine: Defines whether the StateMachine should be informed about session change.
*
* @return see NsmErrorStatus_e
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enSetSessionState(NsmSession_s *pstSession, gboolean boInformBus, gboolean boInformMachine)
{
  /* Function local variables                                           */
  NsmErrorStatus_e    enRetVal = NsmErrorStatus_NotSet; /* Return value */

  /* Check if the passed parameters are valid. */
  if(   (g_strcmp0(pstSession->sOwner, NSM_DEFAULT_SESSION_OWNER) != 0)
     && (pstSession->enState > NsmSessionState_Unregistered           )
     && (pstSession->enSeat  >  NsmSeat_NotSet                        )
     && (pstSession->enSeat  <  NsmSeat_Last                          ))
  {
    /* Parameters are valid. Check if a platform session state is set */
    if(NSM__boIsPlatformSession(pstSession) == TRUE)
    {
      enRetVal = NSM__enSetDefaultSessionState(pstSession, boInformBus, boInformMachine);
    }
    else
    {
      enRetVal = NSM__enSetProductSessionState(pstSession, boInformBus, boInformMachine);
    }
  }
  else
  {
    /* Error: An invalid parameter has been passed. */
    enRetVal = NsmErrorStatus_Parameter;
    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to change session state. Invalid paramter."),
                                       DLT_STRING(" Application: "),   DLT_STRING(pstSession->sOwner      ),
                                       DLT_STRING(" Session: "),       DLT_STRING(pstSession->sName       ),
                                       DLT_STRING(" Seat: "),          DLT_INT(   pstSession->enSeat      ),
                                       DLT_STRING(" Desired state: "), DLT_INT(   pstSession->enState     ));
  }

  return enRetVal;
}


/**********************************************************************************************************************
*
* The function is called from IPC and StateMachine to get the session state.
*
* @param pstSession: Pointer to structure where session name, owner and seat are defined and SessionState will be set.
*
* @return see NsmErrorStatus_e
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enGetSessionState(NsmSession_s *pstSession)
{
  /* Function local variables                                                                  */
  NsmErrorStatus_e    enRetVal         = NsmErrorStatus_NotSet; /* Return value                */
  NsmSession_s       *pExistingSession = NULL;                  /* Pointer to existing session */
  GSList             *pListEntry       = NULL;

  g_mutex_lock(NSM__pSessionMutex);

  /* Search for session with name, seat and owner. */
  pListEntry = g_slist_find_custom(NSM__pSessions, pstSession, &NSM__i32SessionNameSeatCompare);

  if(pListEntry != NULL)
  {
    /* Found the session in the list. Return its state. */
    enRetVal            = NsmErrorStatus_Ok;
    pExistingSession    = (NsmSession_s*) pListEntry->data;
    pstSession->enState = pExistingSession->enState;
  }
  else
  {
    /* Error: The session is unknown. */
    enRetVal = NsmErrorStatus_WrongSession;
    DLT_LOG(NsmContext, DLT_LOG_WARN, DLT_STRING("NSM: Failed to retrieve session state. Unknown session."),
                                      DLT_STRING(" Session: "),       DLT_STRING(pstSession->sName        ),
                                      DLT_STRING(" Seat: "),          DLT_INT(   pstSession->enSeat       ));
  }

  g_mutex_unlock(NSM__pSessionMutex);

  return enRetVal;
}


static void NSM__vFreeFailedApplicationObject(gpointer pFailedApplication)
{
  /* Function local variables. Cast the passed object */
  NSM__tstFailedApplication *pstFailedApplication = (NSM__tstFailedApplication*) pFailedApplication;

  g_free(pstFailedApplication);
}


/**********************************************************************************************************************
*
* The function is called either manually for one object or for every "session object", when the list of registered
* sessions is destroyed with "g_slist_free_full". All memory occupied by the "session object" is released.
*
* @param pSession: Pointer to the session object
*
* @return void
*
**********************************************************************************************************************/
static void NSM__vFreeSessionObject(gpointer pSession)
{
  /* Function local variables. Cast the passed object */
  NsmSession_s *pstSession = (NsmSession_s*) pSession;

  /* Free the session object */
  g_free(pstSession);
}


/**********************************************************************************************************************
*
* The function is called either manually for one object or for every "lifecycle client object", when the list of
* registered lifecycle clients is destroyed with "g_slist_free_full".
* All memory occupied by the "lifecycle client object" is released.
*
* @param pLifecycleClient: Pointer to the lifecycle client object
*
* @return void
*
**********************************************************************************************************************/
static void NSM__vFreeLifecycleClientObject(gpointer pLifecycleClient)
{
  /* Function local variables. Cast the passed object */
  NSM__tstLifecycleClient *pstLifecycleClient = (NSM__tstLifecycleClient*) pLifecycleClient;

  /* Free internal strings and objects */
  g_free(pstLifecycleClient->sBusName);
  g_free(pstLifecycleClient->sObjName);

  /* No need to check for NULL. Only valid clients come here */
  NSMA_boFreeLcConsumerProxy(pstLifecycleClient->hClient);

  /* Free the shutdown client object */
  g_free(pstLifecycleClient);
}


/**********************************************************************************************************************
*
* The function is used to "custom compare" and identify a lifecycle client in the list of clients.
* Because the function is not used for sorting, the return value 1 is not used.
*
* @param pS1: Lifecycle client from list
* @param pS2: Lifecycle client to compare
*
* @return -1: pL1 < pL2
*          0: pL1 = pL2
*          1: pL1 > pL2 (unused, because function not used for sorting)
*
**********************************************************************************************************************/
static gint NSM__i32LifecycleClientCompare(gconstpointer pL1, gconstpointer pL2)
{
  /* Function local variables. Cast the passed objects */
  NSM__tstLifecycleClient *pListClient    = NULL;
  NSM__tstLifecycleClient *pCompareClient = NULL;
  gint                     i32RetVal      = 1;

  pListClient    = (NSM__tstLifecycleClient*) pL1;
  pCompareClient = (NSM__tstLifecycleClient*) pL2;

  /* Compare the bus name of the client */
  if(g_strcmp0(pListClient->sBusName, pCompareClient->sBusName) == 0)
  {
    /* Bus names are equal. Now compare object name */
    if(g_strcmp0(pListClient->sObjName, pCompareClient->sObjName) == 0)
    {
      i32RetVal = 0;  /* Clients are identical. Return 0.       */
    }
    else
    {
      i32RetVal = -1; /* Object names are different. Return -1. */
    }
  }
  else
  {
    i32RetVal = -1;   /* Bus names are different. Return -1.    */
  }

  return i32RetVal;   /* Return result of comparison.           */
}


/**********************************************************************************************************************
*
* The function is used to "custom compare" and identify a session in the list of sessions.
* It compares the "session name", the "session owner" and "seat".
* Because the function is not used for sorting, the return value 1 is not used.
*
* @param pS1: Session from list
* @param pS2: Session to compare
*
* @return -1: pS1 < pS2
*          0: pS1 = pS2
*          1: pS1 > pS2 (unused, because function not used for sorting)
*
**********************************************************************************************************************/
static gint NSM__i32SessionOwnerNameSeatCompare(gconstpointer pS1, gconstpointer pS2)
{
  /* Function local variables. Cast the passed objects */
  NsmSession_s *pListSession   = NULL;
  NsmSession_s *pSearchSession = NULL;
  gint          i32RetVal      = 1;

  pListSession   = (NsmSession_s*) pS1;
  pSearchSession = (NsmSession_s*) pS2;

  if(g_strcmp0(pListSession->sOwner,  pSearchSession->sOwner) == 0)
  {
    i32RetVal = NSM__i32SessionNameSeatCompare(pS1, pS2);
  }
  else
  {
    i32RetVal = -1;    /* Session owners differ. Return -1.  */
  }

  return i32RetVal;    /* Return result of comparison        */
}


/**********************************************************************************************************************
*
* The function is used to "custom compare" and identify a session in the list of sessions.
* It compares the "session name" and "seat".
* Because the function is not used for sorting, the return value 1 is not used.
*
* @param pS1: Session from list
* @param pS2: Session to compare
*
* @return -1: pS1 < pS2
*          0: pS1 = pS2
*          1: pS1 > pS2 (unused, because function not used for sorting)
*
**********************************************************************************************************************/
static gint NSM__i32SessionNameSeatCompare(gconstpointer pS1, gconstpointer pS2)
{
  /* Function local variables. Cast the passed objects */
  NsmSession_s *pListSession   = NULL;
  NsmSession_s *pSearchSession = NULL;
  gint          i32RetVal      = 1;

  pListSession   = (NsmSession_s*) pS1;
  pSearchSession = (NsmSession_s*) pS2;

  /* Compare seats of the sessions. */
  if(pListSession->enSeat == pSearchSession->enSeat)
  {
    /* Seats are equal. Compare session names. */
    if(g_strcmp0(pListSession->sName,  pSearchSession->sName)  == 0)
    {
      i32RetVal = 0;  /* Session are equal. Return 0.      */
    }
    else
    {
      i32RetVal = -1; /* Session names differ. Return -1.  */
    }
  }
  else
  {
    i32RetVal = -1;   /* Session seats differ. Return -1.  */
  }

  return i32RetVal;
}


/**********************************************************************************************************************
*
* The function is used to "custom compare" and identify an application name.
* Because the function is not used for sorting, the return value 1 is not used.
*
* @param pA1: Application object from list
* @param pA2: Application object to compare
*
* @return -1: pA1 < pA2
*          0: pA1 = pA2
*          1: pA1 > pA2 (unused, because function not used for sorting)
*
**********************************************************************************************************************/
static gint NSM__i32ApplicationCompare(gconstpointer pA1, gconstpointer pA2)
{
  /* Function local variables. Cast the passed objects */
  NSM__tstFailedApplication *pListApp   = NULL;
  NSM__tstFailedApplication *pSearchApp = NULL;
  gint          i32RetVal      = 1;

  pListApp   = (NSM__tstFailedApplication*) pA1;
  pSearchApp = (NSM__tstFailedApplication*) pA2;

  /* Compare names of the applications */
  if(g_strcmp0(pListApp->sName, pSearchApp->sName) == 0)
  {
    i32RetVal = 0;  /* Names are equal. Return 0.      */
  }
  else
  {
    i32RetVal = -1; /* Names are different. Return -1. */
  }

  return i32RetVal; /* Return result of comparison     */
}


/**********************************************************************************************************************
*
* The function is used to "custom compare" and identify a session with a special owner.
* Because the function is not used for sorting, the return value 1 is not used.
*
* @param pS1: Session from list
* @param pS2: Session to compare
*
* @return -1: pS1 < pS2
*          0: pS1 = pS2
*          1: pS1 > pS2 (unused, because function not used for sorting)
*
**********************************************************************************************************************/
static gint NSM__i32SessionOwnerCompare(gconstpointer pS1, gconstpointer pS2)
{
  /* Function local variables. Cast the passed objects */
  NsmSession_s *pListSession   = NULL;
  NsmSession_s *pSearchSession = NULL;
  gint          i32RetVal      = 1;

  pListSession   = (NsmSession_s*) pS1;
  pSearchSession = (NsmSession_s*) pS2;

  /* Compare owners of the sessions */
  if(g_strcmp0(pListSession->sOwner, pSearchSession->sOwner) == 0)
  {
    i32RetVal = 0;  /* Owners are equal. Return 0.      */
  }
  else
  {
    i32RetVal = -1; /* Owners are different. Return -1. */
  }

  return i32RetVal; /* Return result of comparison      */
}


/**********************************************************************************************************************
*
* The function is called after a lifecycle client was informed about the changed life cycle.
* The return value of the last informed client will be evaluated and the next lifecycle client
* to inform will be determined and called.
* If there is no client left, the lifecycle sequence will be finished.
*
* @param pSrcObject: Source object (lifecycle client proxy)
* @param pRes:       Result of asynchronous call
* @param pUserData:  Pointer to the current lifecycle client object
*
* @return void
*
**********************************************************************************************************************/
static void NSM__vOnLifecycleRequestFinish(const NsmErrorStatus_e enErrorStatus)
{
  if(enErrorStatus == NsmErrorStatus_Ok)
  {
    /* The clients "LifecycleRequest" has been successfully processed. */
	NSM__vLtProf(NSM__pCurrentLifecycleClient->sBusName, NSM__pCurrentLifecycleClient->sObjName, 0, "leave: ", 0);
    DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Successfully called lifecycle client."));
  }
  else
  {
    /* Error: The method of the lifecycle client returned an error */
    NSM__vLtProf(NSM__pCurrentLifecycleClient->sBusName, NSM__pCurrentLifecycleClient->sObjName, 0, "leave: error: ", enErrorStatus);
    DLT_LOG(NsmContext, DLT_LOG_WARN, DLT_STRING("NSM: Failed to call life cycle client."       ),
                                      DLT_STRING(" Return Value: "), DLT_INT((gint) enErrorStatus));
  }

  NSM__vCallNextLifecycleClient();
}


/**********************************************************************************************************************
*
* The function is called when:
*    - The NodeState changes (NSM__boHandleSetNodeState), to initiate a lifecycle sequence
*    - A client returned and the next client has to be called (NSM__vOnLifecycleRequestFinish)
*
* If the clients need to "run up" or shut down for the current NodeState, the function
* searches the list forward or backward until a client is found, which needs to be informed.
*
* PLEASE NOTE: If all clients have been informed about a "shut down", this function will quit the
*              "g_main_loop", which leads to the the termination of the NSM!
*
* @return void
*
**********************************************************************************************************************/
static void NSM__vCallNextLifecycleClient(void)
{
  /* Function local variables                                                                      */
  GList                   *pListEntry      = NULL;                 /* Iterate through list entries */
  NSM__tstLifecycleClient *pClient         = NULL;                 /* Client object from list      */
  guint32                  u32ShutdownType = NSM_SHUTDOWNTYPE_NOT; /* Return value                 */
  gboolean                 boShutdown      = FALSE;

  NSM__pCurrentLifecycleClient = NULL;

  g_mutex_lock(NSM__pNodeStateMutex);

  /* Based on NodeState determine if clients have to shutdown or run up. Find a client that has not been informed */
  switch(NSM__enNodeState)
  {
    /* For "shutdown" search backward in the list, until there is a client that has not been shut down */
    case NsmNodeState_ShuttingDown:
      u32ShutdownType = NSM_SHUTDOWNTYPE_NORMAL;
      for( pListEntry = g_list_last(NSM__pLifecycleClients);
          (pListEntry != NULL) && (NSM__pCurrentLifecycleClient == NULL);
          pListEntry = g_list_previous(pListEntry))
      {
        /* Check if client has not been shut down and is registered for "normal shutdown" */
        pClient = (NSM__tstLifecycleClient*) pListEntry->data;
        if(   (  pClient->boShutdown                           == FALSE)
           && ( (pClient->u32RegisteredMode & u32ShutdownType) != 0    ))
        {
          /* Found a "running" previous client, registered for the shutdown mode */
          NSM__pCurrentLifecycleClient = (NSM__tstLifecycleClient*) pListEntry->data;
        }
      }
    break;

    /* For "fast shutdown" search backward in the list, until there is a client that has not been shut down */
    case NsmNodeState_FastShutdown:
      u32ShutdownType = NSM_SHUTDOWNTYPE_FAST;
      for( pListEntry = g_list_last(NSM__pLifecycleClients);
          (pListEntry != NULL) && (NSM__pCurrentLifecycleClient == NULL);
          pListEntry = g_list_previous(pListEntry))
      {
        /* Check if client has not been shut down and is registered for "fast shutdown" */
        pClient = (NSM__tstLifecycleClient*) pListEntry->data;
        if(   (  pClient->boShutdown                           == FALSE)
           && ( (pClient->u32RegisteredMode & u32ShutdownType) != 0    ))
        {
          /* Found a "running" previous client, registered for the shutdown mode */
          NSM__pCurrentLifecycleClient = (NSM__tstLifecycleClient*) pListEntry->data;
        }
      }
    break;

    /* For a "running" mode search forward in the list (get next), until there is a client that is shut down */
    default:
      u32ShutdownType = NSM_SHUTDOWNTYPE_RUNUP;
      for(pListEntry = g_list_first(NSM__pLifecycleClients);
          (pListEntry != NULL) && (NSM__pCurrentLifecycleClient == NULL);
          pListEntry = g_list_next(pListEntry))
      {
        /* Check if client is shut down */
        pClient = (NSM__tstLifecycleClient*) pListEntry->data;
        if(pClient->boShutdown == TRUE)
        {
          /* The client was shutdown. It should run up, because we are in a running mode */
          NSM__pCurrentLifecycleClient = (NSM__tstLifecycleClient*) pListEntry->data;
        }
      }
    break;
  }

  /* Check if a client could be found that needs to be informed */
  if(NSM__pCurrentLifecycleClient != NULL)
  {
    DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Call lifecycle client."                                                  ),
                                      DLT_STRING(" Bus name: "),         DLT_STRING(NSM__pCurrentLifecycleClient->sBusName      ),
                                      DLT_STRING(" Obj name: "),         DLT_STRING(NSM__pCurrentLifecycleClient->sObjName      ),
                                      DLT_STRING(" Registered types: "), DLT_INT(NSM__pCurrentLifecycleClient->u32RegisteredMode),
                                      DLT_STRING(" Client: "),           DLT_INT( (guint) NSM__pCurrentLifecycleClient->hClient ),
                                      DLT_STRING(" ShutdownType: "),     DLT_UINT(u32ShutdownType                               ));

    /* Remember that client received a run-up or shutdown call */
    pClient->boShutdown = (u32ShutdownType != NSM_SHUTDOWNTYPE_RUNUP);

    NSM__vLtProf(NSM__pCurrentLifecycleClient->sBusName, NSM__pCurrentLifecycleClient->sObjName, u32ShutdownType, "enter: ", 0);

    NSMA_boCallLcClientRequest(NSM__pCurrentLifecycleClient->hClient, u32ShutdownType);
    boShutdown = FALSE;
  }
  else
  {
    /* The last client was called. Depending on the NodeState check if we can end. */
    switch(NSM__enNodeState)
    {
      /* All registered clients have been 'fast shutdown'. Set NodeState to "shutdown" */
      case NsmNodeState_FastShutdown:
        DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Informed all registered clients about 'fast shutdown'. Set NodeState to 'shutdown'"));

        NSM__enNodeState = NsmNodeState_Shutdown;
        NsmcSetData(NsmDataType_NodeState, (unsigned char*) &NSM__enNodeState, sizeof(NsmNodeState_e));
        NSMA_boSendNodeStateSignal(NSM__enNodeState);
        boShutdown = TRUE;
      break;

      /* All registered clients have been 'shutdown'. Set NodeState to "shutdown" */
      case NsmNodeState_ShuttingDown:
        DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Informed all registered clients about 'shutdown'. Set NodeState to 'shutdown'."));

        NSM__enNodeState = NsmNodeState_Shutdown;
        NsmcSetData(NsmDataType_NodeState, (unsigned char*) &NSM__enNodeState, sizeof(NsmNodeState_e));
        NSMA_boSendNodeStateSignal(NSM__enNodeState);
        boShutdown = TRUE;
      break;

      /* We are in a running state. Nothing to do */
      default:
        boShutdown = FALSE;
      break;
    }
  }

  g_mutex_unlock(NSM__pNodeStateMutex);

  if(boShutdown == TRUE)
  {
    NSMA_boQuitEventLoop();
  }
}


/**********************************************************************************************************************
*
* The callback is called when a check for LUC is required.
* It uses the NodeStateMachine to determine whether LUC is required.
*
* @param pboRetVal: Pointer, where to store the StateMAchine's return value
*
**********************************************************************************************************************/
static gboolean NSM__boOnHandleCheckLucRequired(void)
{
  /* Determine if LUC is required by asking the NodeStateMachine */
  return (NsmcLucRequired() == 0x01) ? TRUE : FALSE;
}


/**********************************************************************************************************************
*
* The callback is called when the "boot mode" should be set.
* It sets the BootMode using an internal function.
*
* @param i32BootMode: New boot mode
* @param penRetVal:   Pointer, where to store the return value
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enOnHandleSetBootMode(const gint i32BootMode)
{
  /* Use internal setter to set the BootMode and inform the StateMachine */
  return NSM__enSetBootMode(i32BootMode, TRUE);
}


/**********************************************************************************************************************
*
* The callback is called when the "node state" should be set.
* It sets the NodeState using an internal function.
*
* @param enNodeStateId: New node state
* @param penRetVal:     Pointer, where to store the return value
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enOnHandleSetNodeState(const NsmNodeState_e  enNodeState)
{
  return NSM__enSetNodeState(enNodeState, TRUE, TRUE);
}


/**********************************************************************************************************************
*
* The callback is called when the "application mode" should be set.
* It sets the ApplicationMode using an internal function.
*
* @param enApplicationModeId: New application mode
* @param penRetVal:           Pointer, where to store the return value
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enOnHandleSetApplicationMode(const NsmApplicationMode_e enApplMode)
{
  return NSM__enSetApplicationMode(enApplMode, TRUE, TRUE);
}


/**********************************************************************************************************************
*
* The callback is called when the node reset is requested.
* It passes the request to the NodestateMachine.
*
* @param i32RestartReason:     Restart reason
* @param i32RestartType:       Restart type
* @param penRetVal:            Pointer, where to store the return value
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enOnHandleRequestNodeRestart(const NsmRestartReason_e enRestartReason,
                                                          const guint              u32RestartType)
{
  NsmErrorStatus_e enRetVal = NsmErrorStatus_NotSet;

  DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Node restart has been requested."));

  if(NsmcRequestNodeRestart(enRestartReason, u32RestartType) == 0x01)
  {
    enRetVal = NsmErrorStatus_Ok;
    (void) NSMA_boSetRestartReason(enRestartReason);
  }
  else
  {
    enRetVal = NsmErrorStatus_Error;
  }

  return enRetVal;
}


/**********************************************************************************************************************
*
* The called is called when a new session should be registered.
* It checks the passed parameters and creates a NsmSession_s structure of them.
* If everything is ok, the new session will be created and the system and StateMachine will be informed.
*
* @param sSessionName:   Name of the new session
* @param sSessionOwner:  Owner of the new session
* @param enSeatId:       Seat which belongs to the new session
* @param enSessionState: Initial state of the new session
* @param penRetVal:      Pointer, where to store the return value
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enOnHandleRegisterSession(const gchar             *sSessionName,
                                                       const gchar             *sSessionOwner,
                                                       const NsmSeat_e          enSeatId,
                                                       const NsmSessionState_e  enSessionState)
{
  /* Function local variables                                                     */
  glong             u32SessionNameLen  = 0;     /* Length of passed session owner */
  glong             u32SessionOwnerLen = 0;     /* Length of passed session name  */
  NsmSession_s      stSession;                  /* To search for existing session */
  NsmErrorStatus_e  enRetVal           = NsmErrorStatus_NotSet;

  /* Check if the passed parameters are valid */
  u32SessionNameLen  = g_utf8_strlen(sSessionName,  -1);
  u32SessionOwnerLen = g_utf8_strlen(sSessionOwner, -1);

  if(   (u32SessionNameLen  <  NSM_MAX_SESSION_NAME_LENGTH )
     && (u32SessionOwnerLen <  NSM_MAX_SESSION_OWNER_LENGTH)
     && (enSeatId           >  NsmSeat_NotSet              )
     && (enSeatId           <  NsmSeat_Last                ))
  {
    /* Initialize temporary session object to check if session already exists */
    g_strlcpy((gchar*) stSession.sName,  sSessionName,  sizeof(stSession.sName) );
    g_strlcpy((gchar*) stSession.sOwner, sSessionOwner, sizeof(stSession.sOwner));
    stSession.enSeat  = enSeatId;
    stSession.enState = enSessionState;

    enRetVal = NSM__enRegisterSession(&stSession, TRUE, TRUE);
  }
  else
  {
    /* Error: A parameter with an invalid value has been passed */
    enRetVal = NsmErrorStatus_Parameter;
    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to register session. Invalid parameter."),
                                       DLT_STRING("Name:"         ), DLT_STRING(sSessionName           ),
                                       DLT_STRING("Owner:"        ), DLT_STRING(sSessionOwner          ),
                                       DLT_STRING("Seat:"         ), DLT_INT((gint) enSeatId           ),
                                       DLT_STRING("Initial state:"), DLT_INT((gint) enSessionState     ));
  }

  return enRetVal;
}


/**********************************************************************************************************************
*
* The callback is called when a session should be unregistered.
* It checks the passed parameters and creates a NsmSession_s structure of them.
* If everything is ok, the new session will be removed and the system and StateMachine will be informed.
*
* @param sSessionName:  Name of the new session that should be unregistered.
* @param sSessionOwner: Current owner of the session that should be unregistered.
* @param enSeat:        Seat for which the session should be unregistered.
* @param penRetVal:     Pointer, where to store the return value
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enOnHandleUnRegisterSession(const gchar     *sSessionName,
                                                         const gchar     *sSessionOwner,
                                                         const NsmSeat_e  enSeatId)
{
  /* Function local variables                                                                   */
  glong             u32SessionNameLen  = 0;                   /* Length of passed session owner */
  glong             u32SessionOwnerLen = 0;                   /* Length of passed session name  */
  NsmSession_s      stSearchSession    = {0};                 /* To search for existing session */
  NsmErrorStatus_e  enRetVal           = NsmErrorStatus_NotSet;

  /* Check if the passed parameters are valid */
  u32SessionNameLen  = g_utf8_strlen(sSessionName,  -1);
  u32SessionOwnerLen = g_utf8_strlen(sSessionOwner, -1);

  if(   (u32SessionNameLen   < NSM_MAX_SESSION_NAME_LENGTH )
     && (u32SessionOwnerLen  < NSM_MAX_SESSION_OWNER_LENGTH))
  {
    /* Assign seat, session name and owner to search for session */
    stSearchSession.enSeat = enSeatId;
    g_strlcpy((gchar*) stSearchSession.sName,  sSessionName,  sizeof(stSearchSession.sName) );
    g_strlcpy((gchar*) stSearchSession.sOwner, sSessionOwner, sizeof(stSearchSession.sOwner));

    enRetVal = NSM__enUnRegisterSession(&stSearchSession, TRUE, TRUE);
  }
  else
  {
    /* Error: Invalid parameter. The session or owner name is to long. */
    enRetVal = NsmErrorStatus_Parameter;
    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to unregister session. The session or owner name is to long."),
                                       DLT_STRING(" Name: "      ), DLT_STRING(sSessionName                                 ),
                                       DLT_STRING(" Owner: "     ), DLT_STRING(sSessionOwner                                ),
                                       DLT_STRING(" Seat: "      ), DLT_INT((gint) enSeatId                                 ));
  }

  return enRetVal;
}


/**********************************************************************************************************************
*
* The callback is called when a lifecycle client should be registered.
* In the list of lifecycle clients it will be checked if the client already exists.
* If it exists, it's settings will be updated. Otherwise a new client will be created.
*
* @param sBusName:        Bus name of the remote application that hosts the lifecycle client interface
* @param sObjName:        Object name of the lifecycle client
* @param u32ShutdownMode: Shutdown mode for which the client wants to be informed
* @param u32TimeoutMs:    Timeout in ms. If the client does not return after the specified time, the NSM
*                         aborts its shutdown and calls the next client.
* @param penRetVal:       Pointer, where to store the return value
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enOnHandleRegisterLifecycleClient(const gchar *sBusName,
                                                               const gchar *sObjName,
                                                               const guint  u32ShutdownMode,
                                                               const guint  u32TimeoutMs)
{
  NSM__tstLifecycleClient     stTestLifecycleClient = {0};
  NSM__tstLifecycleClient    *pstNewClient          = NULL;
  NSM__tstLifecycleClient    *pstExistingClient     = NULL;
  GList                      *pListEntry            = NULL;
  NSMA_tLcConsumerHandle     *hConsumer             = NULL;
  GError                     *pError                = NULL;
  NsmErrorStatus_e            enRetVal              = NsmErrorStatus_NotSet;

  /* The parameters are valid. Create a temporary client to search the list */
  stTestLifecycleClient.sBusName = (gchar*) sBusName;
  stTestLifecycleClient.sObjName = (gchar*) sObjName;

  /* Check if the lifecycle client already is registered */
  pListEntry = g_list_find_custom(NSM__pLifecycleClients, &stTestLifecycleClient, &NSM__i32LifecycleClientCompare);

  if(pListEntry == NULL)
  {
    /* The client does not exist. Try to create a new proxy */
    hConsumer = NSMA_hCreateLcConsumer(sBusName, sObjName, u32TimeoutMs);

    /* The new proxy could be created. Create and store new client */
    if(hConsumer != NULL)
    {
      enRetVal = NsmErrorStatus_Ok;

      /* Create client object and copies of the strings. */
      pstNewClient = g_new0(NSM__tstLifecycleClient, 1);
      pstNewClient->u32RegisteredMode = u32ShutdownMode;
      pstNewClient->sBusName          = g_strdup(sBusName);
      pstNewClient->sObjName          = g_strdup(sObjName);
      pstNewClient->boShutdown        = FALSE;
      pstNewClient->hClient           = hConsumer;


      /* Append the new client to the list */
      NSM__pLifecycleClients = g_list_append(NSM__pLifecycleClients, pstNewClient);

      DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Registered new lifecycle consumer."                 ),
                                        DLT_STRING(" Bus name: "), DLT_STRING(pstNewClient->sBusName         ),
                                        DLT_STRING(" Obj name: "), DLT_STRING(pstNewClient->sObjName         ),
                                        DLT_STRING(" Timeout: " ), DLT_UINT(  u32TimeoutMs                   ),
                                        DLT_STRING(" Mode(s): "),  DLT_INT(   pstNewClient->u32RegisteredMode),
                                        DLT_STRING(" Client: "),   DLT_UINT((guint) pstNewClient->hClient    ));
    }
    else
    {
      enRetVal = NsmErrorStatus_Dbus;
      DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Failed to register new lifecycle consumer. D-Bus error."),
                                        DLT_STRING(" Bus name: "),           DLT_STRING(sBusName                 ),
                                        DLT_STRING(" Obj name: "),           DLT_STRING(sObjName                 ),
                                        DLT_STRING(" Timeout: " ),           DLT_UINT(  u32TimeoutMs             ),
                                        DLT_STRING(" Registered mode(s): "), DLT_INT(   u32ShutdownMode          ),
                                        DLT_STRING(" Error: "),              DLT_STRING(pError->message          ));

      g_error_free(pError);
    }
  }
  else
  {
    /* The client already exists. Assert to update the values for timeout and mode */
    enRetVal = NsmErrorStatus_Ok;
    pstExistingClient = (NSM__tstLifecycleClient*) pListEntry->data;
    pstExistingClient->u32RegisteredMode |= u32ShutdownMode;
    NSMA_boSetLcClientTimeout(pstExistingClient->hClient, u32TimeoutMs);

    DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Changed lifecycle consumer registration."                          ),
                                      DLT_STRING(" Bus name: "),           DLT_STRING(pstExistingClient->sBusName         ),
                                      DLT_STRING(" Obj name: "),           DLT_STRING(pstExistingClient->sObjName         ),
                                      DLT_STRING(" Timeout: " ),           DLT_UINT(  u32TimeoutMs                        ),
                                      DLT_STRING(" Registered mode(s): "), DLT_INT(   pstExistingClient->u32RegisteredMode));
  }

  return enRetVal;
}


/**********************************************************************************************************************
*
* The callback is called when a lifecycle client should be unregistered or a shutdown
* mode should be removed. In the list of lifecycle clients will be checked if the client exists. If the
* client is found, the registration for the passed shutdown modes will be removed. If the client finally
* is not registered for any shutdown mode, its entry will be removed from the list.
*
* @param sBusName:        Bus name of the remote application that hosts the lifecycle client interface
* @param sObjName:        Object name of the lifecycle client
* @param u32ShutdownMode: Shutdown mode for which the client wants to unregister
* @param penRetVal:       Pointer, where to store the return value
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enOnHandleUnRegisterLifecycleClient(const gchar *sBusName,
                                                                 const gchar *sObjName,
                                                                 const guint  u32ShutdownMode)
{
  NSM__tstLifecycleClient *pstExistingClient = NULL;
  NSM__tstLifecycleClient  stSearchClient    = {0};
  GList                   *pListEntry        = NULL;
  NsmErrorStatus_e         enRetVal          = NsmErrorStatus_NotSet;

  stSearchClient.sBusName = (gchar*) sBusName;
  stSearchClient.sObjName = (gchar*) sObjName;

  /* Check if the lifecycle client already is registered */
  pListEntry = g_list_find_custom(NSM__pLifecycleClients, &stSearchClient, &NSM__i32LifecycleClientCompare);

  /* Check if an existing client could be found */
  if(pListEntry != NULL)
  {
    /* The client could be found in the list. Change the registered shutdown mode */
    enRetVal = NsmErrorStatus_Ok;
    pstExistingClient = (NSM__tstLifecycleClient*) pListEntry->data;
    pstExistingClient->u32RegisteredMode &= ~(u32ShutdownMode);

    DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Unregistered lifecycle consumer for mode(s)."                ),
                                      DLT_STRING(" Bus name: "),     DLT_STRING(pstExistingClient->sBusName         ),
                                      DLT_STRING(" Obj name: "),     DLT_STRING(pstExistingClient->sObjName         ),
                                      DLT_STRING(" New mode: "),     DLT_INT(   pstExistingClient->u32RegisteredMode),
                                      DLT_STRING(" Client: "  ),     DLT_UINT((guint) pstExistingClient->hClient)   );

    if(pstExistingClient->u32RegisteredMode == NSM_SHUTDOWNTYPE_NOT)
    {
      /* The client is not registered for at least one mode. Remove it from the list */
      NSM__vFreeLifecycleClientObject(pstExistingClient);
      NSM__pLifecycleClients = g_list_remove(NSM__pLifecycleClients, pstExistingClient);
    }
  }
  else
  {
    /* Warning: The client could not be found in the list of clients. */
    enRetVal = NsmErrorStatus_Parameter;
    DLT_LOG(NsmContext, DLT_LOG_WARN, DLT_STRING("NSM: Failed to unregister lifecycle consumer."),
                                      DLT_STRING(" Bus name: "),             DLT_STRING(sBusName),
                                      DLT_STRING(" Obj name: "),             DLT_STRING(sObjName),
                                      DLT_STRING(" Unregistered mode(s): "), DLT_INT(   u32ShutdownMode));
  }

  return enRetVal;
}


/**********************************************************************************************************************
*
* The function is used to get the state of the passed session.
* It checks the passed parameters and creates a NsmSession_s structure of them.
* If everything is ok, the state of the session will be determined and written to penSessionState.
*
* @param sSessionName:    Name  of the session whose state just be returned
* @param sSessionName:    Owner of the session whose state just be returned
* @param enSeatId:        Seat of the session
* @param penSessionState: Pointer where to store the session state
* @param penRetVal:       Pointer where to store the return value
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enOnHandleGetSessionState(const gchar       *sSessionName,
                                                       const NsmSeat_e    enSeatId,
                                                       NsmSessionState_e *penSessionState)
{
  /* Function local variables                                                                     */
  NsmErrorStatus_e  enRetVal           = NsmErrorStatus_NotSet;
  glong             u32SessionNameLen  = 0;                     /* Length of passed session owner */
  NsmSession_s      stSearchSession    = {0};                   /* To search for existing session */

  /* Check if the passed parameters are valid */
  u32SessionNameLen = g_utf8_strlen(sSessionName, -1);

  if(u32SessionNameLen < NSM_MAX_SESSION_OWNER_LENGTH)
  {
    /* Search for session with name, seat and owner. */
    stSearchSession.enSeat = enSeatId;
    g_strlcpy((gchar*) stSearchSession.sName,  sSessionName,  sizeof(stSearchSession.sName) );

    enRetVal = NSM__enGetSessionState(&stSearchSession);
    *penSessionState = stSearchSession.enState;
  }
  else
  {
    /* Error: Invalid parameter. The session or owner name is to long. */
    enRetVal = NsmErrorStatus_Parameter;
    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to get session state. The session name is to long."),
                                       DLT_STRING(" Name: "      ), DLT_STRING(sSessionName                       ),
                                       DLT_STRING(" Seat: "      ), DLT_INT((gint) enSeatId                       ));
  }

  return enRetVal;
}


/**********************************************************************************************************************
*
* The function sets the state of a session to a passed value.
* It checks the passed parameters and creates a NsmSession_s structure of them.
* If everything is ok, the state of the session will be set accordingly.
*
* @param sSessionName:   Name of the session whose state just be set
* @param sSessionOwner:  Owner of the session
* @param enSeatId:       Seat of the session
* @param enSessionState: New state of the session
* @param penRetVal:      Pointer where to store the return value
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enOnHandleSetSessionState(const gchar             *sSessionName,
                                                       const gchar             *sSessionOwner,
                                                       const NsmSeat_e          enSeatId,
                                                       const NsmSessionState_e  enSessionState)
{
  /* Function local variables                                                                       */
  NsmErrorStatus_e enRetVal           = NsmErrorStatus_NotSet;
  glong            u32SessionNameLen  = 0;            /* Length of passed session owner             */
  glong            u32SessionOwnerLen = 0;            /* Length of passed session name              */
  NsmSession_s     stSession          = {0};          /* Session object passed to internal function */

  /* Check if the passed parameters are valid */
  u32SessionNameLen  = g_utf8_strlen(sSessionName,  -1);
  u32SessionOwnerLen = g_utf8_strlen(sSessionOwner, -1);

  if(   (u32SessionNameLen  < NSM_MAX_SESSION_NAME_LENGTH )
     && (u32SessionOwnerLen < NSM_MAX_SESSION_OWNER_LENGTH))
  {
    /* Build session object to pass it internally */
    g_strlcpy((gchar*) stSession.sName,  sSessionName,  sizeof(stSession.sName) );
    g_strlcpy((gchar*) stSession.sOwner, sSessionOwner, sizeof(stSession.sOwner));

    stSession.enSeat  = enSeatId;
    stSession.enState = enSessionState;

    enRetVal = NSM__enSetSessionState(&stSession, TRUE, TRUE);
  }
  else
  {
    /* Error: Invalid parameter. The session or owner name is to long. */
    enRetVal = NsmErrorStatus_Parameter;
    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to set session state. Invalid parameter."),
                                       DLT_STRING(" Name: "      ), DLT_STRING(sSessionName             ),
                                       DLT_STRING(" Owner: "     ), DLT_STRING(sSessionOwner            ),
                                       DLT_STRING(" Seat: "      ), DLT_INT((gint) enSeatId             ));
  }

  return enRetVal;
}


/**********************************************************************************************************************
*
* The helper function is called by 'NSM__boOnHandleSetAppHealthStatus', when an application became valid again.
* It removes the application from the list of invalid apps.
*
* @param pstFailedApp: Pointer to structure with information about the failed application.
*
* @return NsmErrorStatus_Ok:           The application has been removed from the list of failed apps.
*         NsmErrorStatus_WrongSession: The application has never been on the list of failed apps.
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enSetAppStateValid(NSM__tstFailedApplication* pstFailedApp)
{
  /* Function local variables                                                                             */
  GSList                    *pAppListEntry          = NULL;                  /* List entry of application */
  NsmErrorStatus_e           enRetVal               = NsmErrorStatus_NotSet; /* Return value              */
  NSM__tstFailedApplication *pstExistingApplication = NULL;

  /* An application has become valid again. Check if it really was invalid before. */
  pAppListEntry = g_slist_find_custom(NSM__pFailedApplications, pstFailedApp, &NSM__i32ApplicationCompare);

  if(pAppListEntry != NULL)
  {
    /* We found at least one entry for the application. Remove it from the list */
    enRetVal = NsmErrorStatus_Ok;
    pstExistingApplication = (NSM__tstFailedApplication*) pAppListEntry->data;
    NSM__pFailedApplications = g_slist_remove(NSM__pFailedApplications, pstExistingApplication);
    NSM__vFreeFailedApplicationObject(pstExistingApplication);

    DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: An application has become valid again."    ),
                                      DLT_STRING(" Application: "), DLT_STRING(pstFailedApp->sName));
  }
  else
  {
    /* Error: There was no session registered for the application that failed. */
    enRetVal = NsmErrorStatus_Error;
    DLT_LOG(NsmContext, DLT_LOG_WARN, DLT_STRING("NSM: Failed to set application valid. Application was never invalid."),
                                      DLT_STRING(" Application: "), DLT_STRING(pstFailedApp->sName                     ));
  }

  return enRetVal;
}


/**********************************************************************************************************************
*
* The helper function is called by 'NSM__enSetAppStateFailed', when an application failed.
* It looks for sessions that have been registered by the app.
*
* @param pstFailedApp: Pointer to structure with information about the failed application.
*
**********************************************************************************************************************/
static void NSM__vDisableSessionsForApp(NSM__tstFailedApplication* pstFailedApp)
{
  /* Function local variables */
  GSList       *pSessionListEntry  = NULL;
  NsmSession_s *pstExistingSession = NULL;
  NsmSession_s  stSearchSession    = {0};

  /* Only set the "owner" of the session (to the AppName) to search for all sessions of the app. */
  g_strlcpy(stSearchSession.sOwner, pstFailedApp->sName, sizeof(stSearchSession.sOwner));

  g_mutex_lock(NSM__pSessionMutex);
  pSessionListEntry = g_slist_find_custom(NSM__pSessions, &stSearchSession, &NSM__i32SessionOwnerCompare);

  if(pSessionListEntry != NULL)
  {
    /* Found at least one session. */
    do
    {
      /* Get the session object for the list entry */
      pstExistingSession = (NsmSession_s*) pSessionListEntry->data;
      pstExistingSession->enState = NsmSessionState_Unregistered;

      /* Inform D-Bus and StateMachine that a session became invalid */
      NSM__vPublishSessionChange(pstExistingSession, TRUE, TRUE);

      DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: A session has become invalid, because an application failed."),
                                        DLT_STRING(" Application: "), DLT_STRING(pstExistingSession->sOwner           ),
                                        DLT_STRING(" Session: "),     DLT_STRING(pstExistingSession->sName            ),
                                        DLT_STRING(" Seat: "),        DLT_INT(   pstExistingSession->enSeat           ),
                                        DLT_STRING(" State: "),       DLT_INT(   pstExistingSession->enState          ));

      /* Remove or "reset" session */
      if(NSM__boIsPlatformSession(pstExistingSession) == TRUE)
      {
        /* It is a default session. Don't remove it. Set owner to NSM again. */
        g_strlcpy(pstExistingSession->sOwner, NSM_DEFAULT_SESSION_OWNER, sizeof(pstExistingSession->sOwner));
      }
      else
      {
        /* The session has been registered by a failed app. Remove it. */
        NSM__pSessions = g_slist_remove(NSM__pSessions, pstExistingSession);
        NSM__vFreeSessionObject(pstExistingSession);
      }

      /* Try to find the next session that had been registered for the app. */
      pSessionListEntry = g_slist_find_custom(NSM__pSessions, &stSearchSession, &NSM__i32SessionOwnerCompare);

    } while(pSessionListEntry != NULL);
  }
  else
  {
    /* There have been no session registered for this application. */
    DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: There had been no registered sessions."    ),
                                      DLT_STRING(" Application: "), DLT_STRING(pstFailedApp->sName));
  }

  g_mutex_unlock(NSM__pSessionMutex);
}


/**********************************************************************************************************************
*
* The helper function is called by 'NSM__boOnHandleSetAppHealthStatus', when an application failed.
*
* @param pstFailedApp: Pointer to structure with information about the failed application.
*
* @return always "NsmErrorStatus_Ok"
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enSetAppStateFailed(NSM__tstFailedApplication* pstFailedApp)
{
  /* Function local variables                                                                            */
  GSList                    *pFailedAppListEntry   = NULL;                  /* List entry of application */
  NsmErrorStatus_e           enRetVal              = NsmErrorStatus_NotSet; /* Return value              */
  NSM__tstFailedApplication *pstFailedApplication  = NULL;

  /* An application failed. Check if the application already is known as 'failed'. */
  pFailedAppListEntry = g_slist_find_custom(NSM__pFailedApplications, pstFailedApp, &NSM__i32ApplicationCompare);

  if(pFailedAppListEntry == NULL)
  {
    /* The application is not on the list yet. Create it. */
    enRetVal = NsmErrorStatus_Ok;

    pstFailedApplication  = g_new(NSM__tstFailedApplication, 1);
    g_strlcpy(pstFailedApplication->sName, pstFailedApp->sName, sizeof(pstFailedApplication->sName));
    NSM__pFailedApplications = g_slist_append(NSM__pFailedApplications, pstFailedApplication);

    /* Disable all session that have been registered by the application */
    NSM__vDisableSessionsForApp(pstFailedApplication);
  }
  else
  {
    /* Warning: The application is already in the list of failed session. */
    enRetVal = NsmErrorStatus_Ok;
    DLT_LOG(NsmContext, DLT_LOG_WARN, DLT_STRING("NSM: The application has already been marked as 'failed'."),
                                      DLT_STRING(" Application: "), DLT_STRING(pstFailedApp->sName          ));
  }

  return enRetVal;
}


/**********************************************************************************************************************
*
* The function is called when an application has become invalid or valid again.
* If an application became inactive, it will be added to the list of failed applications
* and signals for the session registered by the application will be emitted.
* If an application became valid again, it will only be removed from the list of failed sessions.
*
* @param sAppName:   Application which changed its state.
* @param boAppState: Indicates if the application became invalid or valid again.
* @param penRetVal:  Pointer where to store the return value
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enOnHandleSetAppHealthStatus(const gchar    *sAppName,
                                                          const gboolean  boAppState)
{
  /* Function local variables                                                                     */
  NSM__tstFailedApplication stSearchApplication = {0}; /* Temporary application object for search */
  NsmErrorStatus_e          enRetVal            = NsmErrorStatus_NotSet;

  /* Check if passed parameters are valid */
  if(strlen(sAppName) < NSM_MAX_SESSION_OWNER_LENGTH)
  {
    /* The application name is valid. Copy it for further checks. */
    g_strlcpy((gchar*) stSearchApplication.sName, sAppName, sizeof(stSearchApplication.sName));

    if(boAppState == TRUE)
    {
      enRetVal = NSM__enSetAppStateValid(&stSearchApplication);
    }
    else
    {
      enRetVal = NSM__enSetAppStateFailed(&stSearchApplication);
    }
  }
  else
  {
    /* Error: The passed application name is too long. */
    enRetVal = NsmErrorStatus_Parameter;
    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to set application health status. The application name is too long."),
                                       DLT_STRING(" Owner: "     ), DLT_STRING(sAppName                                            ),
                                       DLT_STRING(" State: "     ), DLT_INT(boAppState                                             ));

  }

  return enRetVal;
}


/**********************************************************************************************************************
*
* The function returns the current AppHealthCount, which is stored in local variable.
*
* @param pu32AppHealthCount: Pointer where to store the AppHealthCount (number of failed applications).
*
**********************************************************************************************************************/
static guint NSM__u32OnHandleGetAppHealthCount(void)
{
  return g_slist_length(NSM__pFailedApplications);
}


/**********************************************************************************************************************
*
* The function returns the current interface version of the NodeStateManager.
*
* @param pu32InterfaceVersion: Pointer where to store the interface version.
*
**********************************************************************************************************************/
static guint NSM__u32OnHandleGetInterfaceVersion(void)
{
  /* Return interface version to caller. */
  return NSM_INTERFACE_VERSION;
}


/**********************************************************************************************************************
*
* The function is called cyclically and triggers the systemd wdog.
*
* @param pUserData: Pointer to optional user data
*
* @return Always TRUE to keep timer callback alive.
*
**********************************************************************************************************************/
static gboolean NSM__boOnHandleTimerWdog(gpointer pUserData)
{
  (void) sd_notify(0, "WATCHDOG=1");
  DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Triggered systemd WDOG."));

  return TRUE;
}


/**********************************************************************************************************************
*
* The function checks if the NSM is observed by a systemd wdog and installs a timer if necessary.
*
**********************************************************************************************************************/
static void NSM__vConfigureWdogTimer(void)
{
  const gchar *sWdogSec   = NULL;
  guint        u32WdogSec = 0;

  sWdogSec = g_getenv("WATCHDOG_USEC");

  if(sWdogSec != NULL)
  {
    u32WdogSec = strtoul(sWdogSec, NULL, 10);

    /* The min. valid value for systemd is 1 s => WATCHDOG_USEC at least needs to contain 1.000.000 us */
    if(u32WdogSec >= 1000000)
    {
      /* Convert us timeout in ms and divide by two to trigger wdog every half timeout interval */
      u32WdogSec /= 2000;
      (void) g_timeout_add_full(G_PRIORITY_DEFAULT,
                                u32WdogSec,
                                &NSM__boOnHandleTimerWdog,
                                NULL,
                                NULL);
      DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Started wdog timer."            ),
                                        DLT_STRING("Interval [ms]:"), DLT_UINT(u32WdogSec));
    }
    else
    {
      DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Error. Invalid wdog config."    ),
                                         DLT_STRING("WATCHDOG_USEC:"), DLT_STRING(sWdogSec));
    }
  }
  else
  {
    DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Daemon not observed by wdog"));
  }
}


/**********************************************************************************************************************
*
* The function initializes all file local variables
*
**********************************************************************************************************************/
static void  NSM__vInitializeVariables(void)
{
  /* Initialize file local variables */
  NSM__pSessionMutex           = NULL;
  NSM__pSessions               = NULL;
  NSM__pLifecycleClients       = NULL;
  NSM__pNodeStateMutex         = NULL;
  NSM__enNodeState             = NsmNodeState_NotSet;
  NSM__pNextApplicationModeMutex = NULL;
  NSM__pThisApplicationModeMutex = NULL;
  NSM__pFailedApplications     = NULL;
  NSM__pCurrentLifecycleClient = NULL;
  NSM__enNextApplicationMode   = NsmApplicationMode_NotSet;
  NSM__enThisApplicationMode   = NsmApplicationMode_NotSet;
  NSM__boThisApplicationModeRead = FALSE;
}


/**********************************************************************************************************************
*
* The function creates the platform sessions, configured in "NSM__asDefaultSessions".
*
**********************************************************************************************************************/
static void  NSM__vCreatePlatformSessions(void)
{
  NsmSession_s *pNewDefaultSession   = NULL;
  guint         u32DefaultSessionIdx = 0;
  NsmSeat_e     enSeatIdx            = NsmSeat_NotSet;

  /* Configure the default sessions, which are always available */
  for(u32DefaultSessionIdx = 0;
      u32DefaultSessionIdx < sizeof(NSM__asDefaultSessions)/sizeof(gchar*);
      u32DefaultSessionIdx++)
  {
    /* Create a session for every session name and seat */
    for(enSeatIdx = NsmSeat_NotSet + 1; enSeatIdx < NsmSeat_Last; enSeatIdx++)
    {
      pNewDefaultSession          = g_new0(NsmSession_s, 1);
      g_strlcpy((gchar*) pNewDefaultSession->sName,  NSM__asDefaultSessions[u32DefaultSessionIdx], sizeof(pNewDefaultSession->sName));
      g_strlcpy((gchar*) pNewDefaultSession->sOwner, NSM_DEFAULT_SESSION_OWNER, sizeof(pNewDefaultSession->sOwner));
      pNewDefaultSession->enSeat  = enSeatIdx;
      pNewDefaultSession->enState = NsmSessionState_Inactive;

      NSM__pSessions = g_slist_append(NSM__pSessions, pNewDefaultSession);
    }
  }
}


/**********************************************************************************************************************
*
* The function creates the mutexes used in the NSM.
*
**********************************************************************************************************************/
static void NSM__vCreateMutexes(void)
{
  /* Initialize the local mutexes */
  NSM__pNodeStateMutex       = g_mutex_new();
  NSM__pThisApplicationModeMutex = g_mutex_new();
  NSM__pNextApplicationModeMutex = g_mutex_new();
  NSM__pSessionMutex         = g_mutex_new();
}


/**********************************************************************************************************************
*
* The function deletes the mutexes used in the NSM.
*
**********************************************************************************************************************/
static void NSM__vDeleteMutexes(void)
{
  /* Delete the local mutexes */
  g_mutex_free(NSM__pNodeStateMutex);
  g_mutex_free(NSM__pNextApplicationModeMutex);
  g_mutex_free(NSM__pThisApplicationModeMutex);
  g_mutex_free(NSM__pSessionMutex);
}


/**********************************************************************************************************************
*
* The function is called to trace a syslog message for a shutdown client.
*
* @param sBus:          Bus name of the shutdown client.
* @param sObj:          Object name of the lifecycle client.
* @param u32Reason:     Shutdown reason send to the client.
* @param sInOut:        "enter" or "leave" (including failure reason)
* @param enErrorStatus: Error value
*
**********************************************************************************************************************/
static void NSM__vLtProf(gchar *sBus, gchar *sObj, guint32 u32Reason, gchar *sInOut, NsmErrorStatus_e enErrorStatus)
{
    gchar pszLtprof[128] = "LTPROF: bus:%s obj:%s (0x%08X:%d) ";
    guint32 dwLength = 128;

    g_strlcat(pszLtprof, sInOut, dwLength);

    if(u32Reason != 0)
    {
        if(u32Reason == NSM_SHUTDOWNTYPE_RUNUP)
        {
          g_strlcat(pszLtprof, "runup", dwLength);
        }
        else
        {
          g_strlcat(pszLtprof, "shutdown", dwLength);
        }
    }

    syslog(LOG_NOTICE, (char *)pszLtprof, sBus, sObj, u32Reason, enErrorStatus);
}


/**********************************************************************************************************************
*
* The function is used to initialize syslog
*
**********************************************************************************************************************/
static void NSM__vSyslogOpen(void)
{
  openlog("NSM", LOG_PID, LOG_USER);
}


/**********************************************************************************************************************
*
* The function is used to deinitialize syslog
*
**********************************************************************************************************************/
static void NSM__vSyslogClose(void)
{
  closelog();
}


/**********************************************************************************************************************
*
* Interfaces. Exported functions. See Header for detailed description.
*
**********************************************************************************************************************/


/* The function is called by the NodeStateMachine to set a "property" of the NSM. */
NsmErrorStatus_e NsmSetData(NsmDataType_e enData, unsigned char *pData, unsigned int u32DataLen)
{
  /* Function local variables                                        */
  NsmErrorStatus_e enRetVal = NsmErrorStatus_NotSet; /* Return value */

  /* Check which data the NSMC wants to set */
  switch(enData)
  {
    /* NSMC wants to set the NodeState */
    case NsmDataType_NodeState:
      enRetVal =   (u32DataLen == sizeof(NsmNodeState_e))
                 ? NSM__enSetNodeState((NsmNodeState_e) *pData, TRUE, FALSE)
                 : NsmErrorStatus_Parameter;
    break;

    /* NSMC wants to set the AppMode */
    case NsmDataType_AppMode:
      enRetVal =   (u32DataLen == sizeof(NsmApplicationMode_e))
                 ? NSM__enSetApplicationMode((NsmApplicationMode_e) *pData, TRUE, FALSE)
                 : NsmErrorStatus_Parameter;
    break;

    /* NSMC wants to set the BootMode */
    case NsmDataType_BootMode:
      enRetVal =   (u32DataLen == sizeof(gint))
                 ? NSM__enSetBootMode((gint) *pData, FALSE)
                 : NsmErrorStatus_Parameter;
    break;

    /* NSMC wants to set the ShutdownReason */
    case NsmDataType_ShutdownReason:
      enRetVal =   (u32DataLen == sizeof(NsmShutdownReason_e))
                 ? NSM__enSetShutdownReason((NsmShutdownReason_e) *pData, FALSE)
                 : NsmErrorStatus_Parameter;
    break;

    /* NSMC wants to set a SessionState */
    case NsmDataType_SessionState:
      enRetVal =   (u32DataLen == sizeof(NsmSession_s))
                 ? NSM__enSetSessionState((NsmSession_s*) pData, TRUE, FALSE)
                 : NsmErrorStatus_Parameter;
    break;

    /* NSMC wants to register a session */
    case NsmDataType_RegisterSession:
      enRetVal =   (u32DataLen == sizeof(NsmSession_s))
                 ? NSM__enRegisterSession((NsmSession_s*) pData, TRUE, FALSE)
                 : NsmErrorStatus_Parameter;
    break;

    /* NSMC wants to unregister a session */
    case NsmDataType_UnRegisterSession:
      enRetVal =   (u32DataLen == sizeof(NsmSession_s))
                 ? NSM__enUnRegisterSession((NsmSession_s*) pData, TRUE, FALSE)
                 : NsmErrorStatus_Parameter;
    break;

    /* Error: The type of the data NSMC is trying to set is unknown or the data is read only! */
    case NsmDataType_RestartReason:
    case NsmDataType_RunningReason:
    default:
      enRetVal = NsmErrorStatus_Parameter;
    break;
  }

  return enRetVal;
}


/* The function is called by the NodeStateMachine to get a "property" of the NSM. */
int NsmGetData(NsmDataType_e enData, unsigned char *pData, unsigned int u32DataLen)
{
  /* Function local variables                                             */
  int i32RetVal = -1; /* Return value. Positive: Amount of written bytes.
                                       Negative: An error occurred.       */

  /* Check which data the NSMC wants to get */
  switch(enData)
  {
    /* NSMC wants to get the NodeState */
    case NsmDataType_NodeState:
      if(u32DataLen == sizeof(NsmNodeState_e))
      {
        if(NSM__enGetNodeState((NsmNodeState_e*) pData) == NsmErrorStatus_Ok)
        {
          i32RetVal = sizeof(NsmNodeState_e);
        }
      }
    break;

    /* NSMC wants to get the ApplicationMode */
    case NsmDataType_AppMode:
      if(u32DataLen == sizeof(NsmApplicationMode_e))
      {
        if(NSM__enGetApplicationMode((NsmApplicationMode_e*) pData) == NsmErrorStatus_Ok)
        {
          i32RetVal = sizeof(NsmApplicationMode_e);
        }
      }
    break;

    /* NSMC wants to get the BootMode */
    case NsmDataType_BootMode:
      if(u32DataLen == sizeof(gint))
      {
        if(NSMA_boGetBootMode((gint*) pData) == TRUE)
        {
          i32RetVal = sizeof(gint);
        }
      }
    break;

    /* NSMC wants to get the RunningReason */
    case NsmDataType_RunningReason:
      if(u32DataLen == sizeof(NsmRunningReason_e))
      {
        if(NSMA_boGetRunningReason((NsmRunningReason_e*) pData) == TRUE)
        {
          i32RetVal = sizeof(NsmRunningReason_e);
        }
      }
    break;

    /* NSMC wants to get the ShutdownReason */
    case NsmDataType_ShutdownReason:
      if(u32DataLen == sizeof(NsmShutdownReason_e))
      {
        if(NSMA_boGetShutdownReason((NsmShutdownReason_e*) pData) == TRUE)
        {
          i32RetVal = sizeof(NsmShutdownReason_e);
        }
      }
    break;

    /* NSMC wants to get the RestartReason */
    case NsmDataType_RestartReason:
      if(u32DataLen == sizeof(NsmRestartReason_e))
      {
        if(NSMA_boGetRestartReason((NsmRestartReason_e*) pData) == TRUE)
        {
          i32RetVal = sizeof(NsmRestartReason_e);
        }
      }
    break;

    /* NSMC wants to get the SessionState */
    case NsmDataType_SessionState:
      if(u32DataLen == sizeof(NsmSession_s))
      {
        if(NSM__enGetSessionState((NsmSession_s*) pData) == NsmErrorStatus_Ok)
        {
          i32RetVal = sizeof(NsmSession_s);
        }
      }
    break;

    /* Error: The type of the data NSMC is trying to set is unknown. */
    default:
      i32RetVal = -1;
    break;
  }

  return i32RetVal;
}


unsigned int NsmGetInterfaceVersion(void)
{
	return NSM_INTERFACE_VERSION;
}


/* The main function of the NodeStateManager */
int main(void)
{
  gboolean  boEndByUser = FALSE;
  int       pcl_return  = 0;

  /* Initialize glib for using "g" types */
  g_type_init();

  /* Register NSM for DLT */
  DLT_REGISTER_APP("NSM", "Node State Manager");
  DLT_REGISTER_CONTEXT(NsmContext, "005", "Context for the NSM");
  DLT_ENABLE_LOCAL_PRINT();

  /* Initialize syslog */
  NSM__vSyslogOpen();

  /* Print first msg. to show that NSM is going to start */
  DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: NodeStateManager started."), DLT_STRING("Version:"), DLT_STRING(VERSION));

  /* Initialize PCL before initializing variables */
  pcl_return = pclInitLibrary("NodeStateManager",   PCL_SHUTDOWN_TYPE_NORMAL
                                                  | PCL_SHUTDOWN_TYPE_FAST);
  if(pcl_return < 0)
  {
    DLT_LOG(NsmContext,
            DLT_LOG_WARN,
            DLT_STRING("NSM: Failed to initialize PCL.");
            DLT_STRING("Error: Unexpected PCL return.");
            DLT_STRING("Return:"); DLT_INT(pcl_return));
  }

  /* Currently no other resources accessing the NSM. Prepare it now! */
  NSM__vInitializeVariables();     /* Initialize file local variables*/
  NSM__vCreatePlatformSessions();  /* Create platform sessions       */
  NSM__vCreateMutexes();           /* Create mutexes                 */

  /* Initialize the NSMA before the NSMC, because the NSMC can access properties */
  if(NSMA_boInit(&NSM__stObjectCallBacks) == TRUE)
  {
    /* Set the properties to initial values */
    (void) NSMA_boSetBootMode(0);
    (void) NSMA_boSetRestartReason(NsmRestartReason_NotSet);
    (void) NSMA_boSetShutdownReason(NsmShutdownReason_NotSet);
    (void) NSMA_boSetRunningReason(NsmRunningReason_WakeupCan);

    /* Initialize/start the NSMC */
    if(NsmcInit() == 0x01)
    {
      /* Start timer to satisfy wdog */
      NSM__vConfigureWdogTimer();
      
      /* The event loop is only canceled if the Node is completely shut down or there is an internal error. */
      boEndByUser = NSMA_boWaitForEvents();

      if(boEndByUser == TRUE)
      {
        DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Successfully canceled event loop. "),
                                          DLT_STRING("Shutting down NodeStateManager."        ));
      }
      else
      {
        DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Error in event loop. "     ),
                                          DLT_STRING("Shutting down NodeStateManager."));
      }

      /* The event loop returned. Clean up the NSMA. */
      (void) NSMA_boDeInit();
    }
    else
    {
      /* Error: Failed to initialize the NSMC. Clean up NSMA, because it is not needed anymore. */
      (void) NSMA_boDeInit();
      DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Error. Failed to initialize the NSMC."));
    }
  }
  else
  {
    /* Error: Failed to initialize the NSMA. */
    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Error. Failed to initialize the NSMA."));
  }

  /* Free the mutexes */
  NSM__vDeleteMutexes();

  /* Remove data from all lists */
  g_slist_free_full(NSM__pSessions,           &NSM__vFreeSessionObject);
  g_slist_free_full(NSM__pFailedApplications, &NSM__vFreeFailedApplicationObject);
  g_list_free_full (NSM__pLifecycleClients,   &NSM__vFreeLifecycleClientObject);

  /* Deinitialize the PCL */
  pcl_return = pclDeinitLibrary();

  if(pcl_return < 0)
  {
    DLT_LOG(NsmContext,
            DLT_LOG_WARN,
            DLT_STRING("NSM: Failed to deinitialize PCL.");
            DLT_STRING("Error: Unexpected PCL return.");
            DLT_STRING("Return:"); DLT_INT(pcl_return));
  }

  DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: NodeStateManager stopped."));

  /* Deinit syslog */
  NSM__vSyslogClose();

  /* Unregister NSM from DLT */
  DLT_UNREGISTER_CONTEXT(NsmContext);
  DLT_UNREGISTER_APP();

  return 0;
}
