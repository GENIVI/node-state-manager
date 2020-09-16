/**********************************************************************************************************************
*
* Copyright (C) 2013 Continental Automotive Systems, Inc.
*               2017 BMW AG
*
* Author: Jean-Pierre.Bogler@continental-corporation.com
*
* Implementation of the NodeStateManager
*
* The NodeStateManager (NSM) is a central state manager for the system node. It manages the "NodeState",
* the "ApplicationMode" and many other states of the complete system. In addition, the NSM offers a
* session handling and a shutdown management.
* The NSM communicates with the NodeStateMachine (NSMC) to request and inform it about state changes
* and the NodeStateAccess (NSMA) to connect to CommonAPI.
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
#include "NodeStateAccess.h"                /* Access the IPC (D-Bus)         */
#include "Watchdog.hpp"
#include <string.h>                         /* Memcpy etc.                    */
#include <stdlib.h>
#include <getopt.h>
#include <gio/gio.h>                        /* GLib lists                     */
#include <dlt.h>                            /* DLT Log'n'Trace                */
#include <syslog.h>                         /* Syslog messages                */
#include <pthread.h>
#include <systemd/sd-daemon.h>              /* Systemd wdog                   */
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>
#include <stdio.h>
#include "NodeStateMachine.hpp"               /* Talk to NodeStateMachine       */

/**********************************************************************************************************************
*
* Local defines, macros and type definitions.
*
**********************************************************************************************************************/

#ifdef COVERAGE_ENABLED
extern void __gcov_flush();
#endif

static const char *mark __attribute__((used))= "**WATERMARK**" WATERMARK "**WATERMARK**";

/* Defines to access persistence keys */
#define NSM_PERS_APPLICATION_MODE_DB  0xFF
#define NSM_PERS_APPLICATION_MODE_KEY "ERG_OIP_NSM_NODE_APPMODE"

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
static void NSM__vCallParallelLifecycleClient(gboolean verbose);
static void NSM__vOnLifecycleRequestFinish(size_t clientID, gboolean timeout, gboolean late);


/* Internal functions, to set and get values. Indirectly used by D-Bus and StateMachine */
static NsmErrorStatus_e     NSM__enRegisterSession       (NsmSession_s *session,
		                                                  gboolean      boInformBus,
		                                                  gboolean      boInformMachine);
static NsmErrorStatus_e     NSM__enUnRegisterSession     (NsmSession_s *session,
		                                                  gboolean      boInformBus,
		                                                  gboolean      boInformMachine);
static NsmErrorStatus_e     NSM__enSetNodeState          (NsmNodeState_e       enNodeState,
                                                          gboolean             boInformBus,
                                                          gboolean             boInformMachine,
                                                          gboolean             boExternalOrigin);
static NsmErrorStatus_e     NSM__enSetBootMode           (const gint           i32BootMode,
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


/* Callbacks for D-Bus interfaces of the NodeStateManager */
static NsmErrorStatus_e NSM__enOnHandleSetBootMode              (const gint                  i32BootMode);
static NsmErrorStatus_e NSM__enOnHandleSetNodeState             (const NsmNodeState_e        enNodeState);
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
static NsmErrorStatus_e NSM__enOnHandleRegisterLifecycleClient  (const size_t                clientHash,
                                                                 const guint                 u32ShutdownMode,
                                                                 const guint                 u32TimeoutMs);
static NsmErrorStatus_e NSM__enOnHandleUnRegisterLifecycleClient(const size_t                clientHash,
                                                                 const guint                 u32ShutdownMode);
static NsmErrorStatus_e NSM__enOnHandleGetSessionState          (const gchar                *sSessionName,
                                                                 const NsmSeat_e             enSeatId,
                                                                 NsmSessionState_e          *penSessionState);
static NsmErrorStatus_e NSM__enOnHandleSetSessionState          (const gchar                *sSessionName,
                                                                 const gchar                *sSessionOwner,
                                                                 const NsmSeat_e             enSeatId,
                                                                 const NsmSessionState_e     enSessionState);
static NsmErrorStatus_e NSM__enSetBlockExternalNodeState        (const bool                  boBlock);

static guint NSM__u32OnHandleGetAppHealthCount                  (void);
static guint NSM__u32OnHandleGetInterfaceVersion                (void);

/* Functions to simplify internal work flow */
static void  NSM__vInitializeVariables   (void);
static void  NSM__vCreatePlatformSessions(void);

/* LTPROF helper function */
static void NSM__vLtProf(size_t client, guint32 dwReason, gchar *pszInOut, guint32 dwValue);
static void NSM__vSyslogOpen(void);
static void NSM__vSyslogClose(void);

gboolean                    NSM__boEndByUser = FALSE;

/* Systemd watchdog functions */
static void                 *NSM__boOnHandleTimerWdog(void *pUserData);
static void                 NSM__vConfigureWdogTimer(void);
static pthread_t            NSM__watchdog_thread;
static unsigned long int    NSM__WdogSec = 0;

int                         NSM__bootloader_flag;
static struct option        NSM__options[] =
{
    /* These options set a flag. */
    {"bootloader", no_argument, &NSM__bootloader_flag, 1},
    {0, 0, 0, 0}
};

/**********************************************************************************************************************
*
* Local variables and constants
*
**********************************************************************************************************************/

/* Context for Log'n'Trace */
DLT_DECLARE_CONTEXT(NsmContext);
DLT_DECLARE_CONTEXT(NsmaContext);

/* Variables for "Properties" hosted by the NSM */
static GMutex                     NSM__pSessionMutex;
static GSList                    *NSM__pSessions               = NULL;

static GList                     *NSM__pLifecycleClients       = NULL;

static GMutex                     NSM__pNodeStateMutex;
static NsmNodeState_e             NSM__enNodeState             = NsmNodeState_NotSet;
static guint32                    NSM__uiShutdownType          = 0;
static pthread_t                  NSM__callLCThread;

static GSList                    *NSM__pFailedApplications     = NULL;

static guint                      NSM__collective_sequential_timeout = 0;
static guint                      NSM__max_parallel_timeout = 0;
static GMutex                     NSM__collective_timeout_mutex;
static GCond                      NSM__collective_timeout_condVar;
static gboolean                   NSM__collective_timeout_canceled = false;
static GCond                      NSM__collective_timeout_init_condVar;
static gboolean                   NSM__collective_timeout_initialized = false;
static pthread_t                  NSM__collective_timeout_thread = 0;

static volatile gboolean          NSM__boResetActive = FALSE;
static gboolean                   NSM__boBlockExternalNodeState = FALSE;

/* Constant array of callbacks which are registered at the NodeStateAccess library */
static const NSMA_tstObjectCallbacks NSM__stObjectCallBacks = { &NSM__enOnHandleSetBootMode,
                                                                &NSM__enOnHandleSetNodeState,
                                                                &NSM__enOnHandleRequestNodeRestart,
                                                                &NSM__enOnHandleSetAppHealthStatus,
                                                                &NSM__boOnHandleCheckLucRequired,
                                                                &NSM__enOnHandleRegisterSession,
                                                                &NSM__enOnHandleUnRegisterSession,
                                                                &NSM__enOnHandleRegisterLifecycleClient,
                                                                &NSM__enOnHandleUnRegisterLifecycleClient,
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
static void NSM__startCollectiveTimeoutThread(size_t shutdownType);

/**
*
* This function will be called by the NSM__collective_timeout_thread.
* If the thread is not canceled before timeout occurred it will set the target NodeState
* NsmNodeState_Shutdown or NsmNodeState_FullyOperational
* @param  param: Shutdown type
*
* @return NULL
*
*/
static void *NSM__collectiveTimeoutHandler(void *param)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   pthread_detach(pthread_self());

   guint32 timeoutSec = 0;
   guint32 shutdownType = (uint)(uintptr_t)param;
   gboolean timeout = false;

   switch (shutdownType) {
       case NSM_SHUTDOWNTYPE_FAST | NSM_SHUTDOWNTYPE_PARALLEL:
         timeoutSec = 2;
       break;
       case NSM_SHUTDOWNTYPE_FAST:
         timeoutSec = 3;
         break;
       default:
         timeoutSec = 60;
         break;
   }

   gint64 end_time = g_get_monotonic_time () + timeoutSec * G_TIME_SPAN_SECOND;;

   g_mutex_lock(&NSM__collective_timeout_mutex);
   NSM__collective_timeout_initialized = true;
   g_cond_broadcast(&NSM__collective_timeout_init_condVar);

   NSM__collective_timeout_canceled = false;
   NSMTriggerWatchdog(NsmWatchdogState_Sleep);
   while(!NSM__collective_timeout_canceled)
   {
     if(!g_cond_wait_until (&NSM__collective_timeout_condVar,
         &NSM__collective_timeout_mutex, end_time))
     {
       NSMTriggerWatchdog(NsmWatchdogState_Active);
       timeout = true;
       break;
     }
   }
   NSMTriggerWatchdog(NsmWatchdogState_Active);

   g_mutex_unlock(&NSM__collective_timeout_mutex);

   if(timeout)
   {
     g_mutex_lock(&NSM__pNodeStateMutex);
     if(shutdownType != NSM__uiShutdownType)
     {
       // Probably a different thread has already continued with shutdown/runup
       g_mutex_unlock(&NSM__pNodeStateMutex);
     }
     else
     {
       NsmNodeState_e oldNodeState = NSM__enNodeState;

       switch (shutdownType) {
       case NSM_SHUTDOWNTYPE_FAST | NSM_SHUTDOWNTYPE_PARALLEL:
       case NSM_SHUTDOWNTYPE_NORMAL | NSM_SHUTDOWNTYPE_PARALLEL:
          DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Parallel shutdown took too long. Will continue with sequential now!"));

          size_t shutdownType = (NSM__enNodeState == NsmNodeState_FastShutdown) ? NSM_SHUTDOWNTYPE_FAST : NSM_SHUTDOWNTYPE_NORMAL;
          NSMA_setLcCollectiveTimeout();
          NSM__startCollectiveTimeoutThread(shutdownType);
          g_mutex_unlock(&NSM__pNodeStateMutex);
          NSM__vCallNextLifecycleClient();
          break;

       case NSM_SHUTDOWNTYPE_RUNUP:
          DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Sequential runup took too long. Will continue with parallel now!"));
          NSMA_setLcCollectiveTimeout();
          NSM__startCollectiveTimeoutThread(NSM_SHUTDOWNTYPE_RUNUP | NSM_SHUTDOWNTYPE_PARALLEL);
          g_mutex_unlock(&NSM__pNodeStateMutex);
          NSM__vCallParallelLifecycleClient(TRUE);
          break;

       case NSM_SHUTDOWNTYPE_FAST:
       case NSM_SHUTDOWNTYPE_NORMAL:
          DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Shutdown took too long. Will force shutdown now!"));

          DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Changed NodeState -"),
             DLT_STRING(NODESTATE_STRING[oldNodeState]), DLT_INT((gint) oldNodeState), DLT_STRING("=>"),
             DLT_STRING(NODESTATE_STRING[NsmNodeState_Shutdown]), DLT_INT((gint) NsmNodeState_Shutdown));

          NSMA_setLcCollectiveTimeout();
          NSM__enNodeState = NsmNodeState_Shutdown;
          NsmcSetData(NsmDataType_NodeState, (unsigned char*) &NSM__enNodeState, sizeof(NsmNodeState_e));
          NSMA_boSendNodeStateSignal(NSM__enNodeState);
          g_mutex_unlock(&NSM__pNodeStateMutex);
          break;

       case NSM_SHUTDOWNTYPE_RUNUP | NSM_SHUTDOWNTYPE_PARALLEL:
          DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Runup took too long. Will force fully operational now!"));

          DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Changed NodeState -"),
             DLT_STRING(NODESTATE_STRING[oldNodeState]), DLT_INT((gint) oldNodeState), DLT_STRING("=>"),
             DLT_STRING(NODESTATE_STRING[NsmNodeState_FullyOperational]), DLT_INT((gint) NsmNodeState_FullyOperational));

          NSMA_setLcCollectiveTimeout();
          NSM__enNodeState = NsmNodeState_FullyOperational;
          NsmcSetData(NsmDataType_NodeState, (unsigned char*) &NSM__enNodeState, sizeof(NsmNodeState_e));
          NSMA_boSendNodeStateSignal(NSM__enNodeState);
          g_mutex_unlock(&NSM__pNodeStateMutex);
          break;

       default:
          // This should never happen
          g_mutex_unlock(&NSM__pNodeStateMutex);
          DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Shutdown/Runup took to long. Error unknown state!"));
          break;
       }
     }
   }

   NSMUnregisterWatchdog();
   return NULL;
}

/**
* This functions cancels the collectiveTimeout.
* Will be called when all clients successfully returned or timed out in time
*
*/
static void NSM__cancelCollectiveTimeoutThread()
{
  DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: NSM__cancelCollectiveTimeoutThread"));

  g_mutex_lock (&NSM__collective_timeout_mutex);
  NSM__collective_timeout_canceled = true;
  g_cond_broadcast (&NSM__collective_timeout_condVar);
  g_mutex_unlock(&NSM__collective_timeout_mutex);
}

static void NSM__startCollectiveTimeoutThread(size_t shutdownType)
{
  pthread_create(&NSM__collective_timeout_thread, NULL, &NSM__collectiveTimeoutHandler, (void*)shutdownType);
  g_mutex_lock(&NSM__collective_timeout_mutex);
  while (!NSM__collective_timeout_initialized)
  {
    // Wait until thread has been initialized
    g_cond_wait(&NSM__collective_timeout_init_condVar, &NSM__collective_timeout_mutex);
  }
  NSM__collective_timeout_initialized = false;
  g_mutex_unlock(&NSM__collective_timeout_mutex);
}
/**
*
* This helper function is called from various places to check if a session is a "platform" session.
*
* @param  pstSession: Pointer to the session for which a check should be done, if it is a platform session
*
* @return TRUE:  The session is a "platform" session
*         FALSE: The session is not a "platform" session
*
*/
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
* @param session:         Ptr to NsmSession_s structure containing data to register a session
* @param boInformBus:     Flag whether the a dbus signal should be send to inform about the new session
* @param boInformMachine: Flag whether the NSMC should be informed about the new session
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
	    g_mutex_lock(&NSM__pSessionMutex);

	    pListEntry = g_slist_find_custom(NSM__pSessions, session, &NSM__i32SessionNameSeatCompare);

	    if(pListEntry == NULL)
	    {
	      enRetVal = NsmErrorStatus_Ok;

	      pNewSession  = g_new0(NsmSession_s, 1);
	      memcpy(pNewSession, session, sizeof(NsmSession_s));

	      DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Registered session."                          ),
	                                        DLT_STRING("Name:"         ), DLT_STRING(session->sName      ),
	                                        DLT_STRING("Owner:"        ), DLT_STRING(session->sOwner     ),
	                                        DLT_STRING("Seat:"         ), DLT_STRING(SEAT_STRING[session->enSeat]),
	                                        DLT_STRING("Initial state:"), DLT_STRING(SESSIONSTATE_STRING[session->enState]));

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
	                                        DLT_STRING("Name:"         ), DLT_STRING(session->sName            ),
	                                        DLT_STRING("Owner:"        ), DLT_STRING(session->sOwner           ),
	                                        DLT_STRING("Seat:"         ), DLT_STRING(SEAT_STRING[session->enSeat]),
	                                        DLT_STRING("Initial state:"), DLT_STRING(SESSIONSTATE_STRING[session->enState]));
	    }

	    g_mutex_unlock(&NSM__pSessionMutex);
	  }
	  else
	  {
	    /* Error: It is not allowed to re-register a default session! */
	    enRetVal = NsmErrorStatus_Parameter;
	    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to register session. Re-Registration of default session not allowed."),
	                                       DLT_STRING("Name:"         ), DLT_STRING(session->sName                                    ),
	                                       DLT_STRING("Owner:"        ), DLT_STRING(session->sOwner                                   ),
	                                       DLT_STRING("Seat:"         ), DLT_STRING(SEAT_STRING[session->enSeat]                      ),
	                                       DLT_STRING("Initial state:"), DLT_STRING(SESSIONSTATE_STRING[session->enState]             ));
	  }
  }
  else
  {
    /* Error: A parameter with an invalid value has been passed */
    enRetVal = NsmErrorStatus_Parameter;
    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to register session. Invalid owner or state."),
                                       DLT_STRING("Name:"         ), DLT_STRING(session->sName            ),
                                       DLT_STRING("Owner:"        ), DLT_STRING(session->sOwner           ),
                                       DLT_STRING("Seat:"         ), DLT_STRING(SEAT_STRING[session->enSeat]),
                                       DLT_STRING("Initial state:"), DLT_STRING(SESSIONSTATE_STRING[session->enState]));
  }

  return enRetVal;
}


/**
* NSM__enUnRegisterSession:
* @param session:         Ptr to NsmSession_s structure containing data to unregister a session
* @param boInformBus:     Flag whether the a dbus signal should be send to inform about the lost session
* @param boInformMachine: Flag whether the NSMC should be informed about the lost session
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
    g_mutex_lock(&NSM__pSessionMutex);

    pListEntry = g_slist_find_custom(NSM__pSessions, session, &NSM__i32SessionOwnerNameSeatCompare);

    /* Check if the session exists */
    if(pListEntry != NULL)
    {
      /* Found the session in the list. Now remove it. */
      enRetVal = NsmErrorStatus_Ok;
      pExistingSession = (NsmSession_s*) pListEntry->data;

      DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Unregistered session."                          ),
                                        DLT_STRING("Name:"      ), DLT_STRING(pExistingSession->sName  ),
                                        DLT_STRING("Owner:"     ), DLT_STRING(pExistingSession->sOwner ),
                                        DLT_STRING("Seat:"      ), DLT_STRING(SEAT_STRING[session->enSeat] ),
                                        DLT_STRING(" Last state: "), DLT_STRING(SESSIONSTATE_STRING[session->enState]));

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
                                        DLT_STRING("Name:"      ), DLT_STRING(session->sName          ),
                                        DLT_STRING("Owner:"     ), DLT_STRING(session->sOwner         ),
                                        DLT_STRING("Seat:"      ), DLT_STRING(SEAT_STRING[session->enSeat]));
    }

    g_mutex_unlock(&NSM__pSessionMutex);
  }
  else
  {
    /* Error: Failed to unregister session. The passed session is a "platform" session. */
    enRetVal = NsmErrorStatus_WrongSession;
    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to unregister session. The session is a platform session."),
                                       DLT_STRING("Name:"      ), DLT_STRING(session->sName                            ),
                                       DLT_STRING("Owner:"     ), DLT_STRING(session->sOwner                           ),
                                       DLT_STRING("Seat:"      ), DLT_STRING(SEAT_STRING[session->enSeat]              ));
  }

  return enRetVal;
}


static NsmErrorStatus_e NSM__enSetBlockExternalNodeState(bool boBlock)
{
  NSM__boBlockExternalNodeState = boBlock;
  return NsmErrorStatus_Ok;
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
static NsmErrorStatus_e NSM__enSetNodeState(NsmNodeState_e enNodeState, gboolean boInformBus, gboolean boInformMachine, gboolean boExternalOrigin)
{
  /* Function local variables                                        */
  NsmErrorStatus_e enRetVal = NsmErrorStatus_NotSet; /* Return value */

  /* Check if the passed parameter is valid */
  if((enNodeState > NsmNodeState_NotSet) && (enNodeState < NsmNodeState_Last))
  {
    /* Assert that the Node not already is shut down. Otherwise it will switch of immediately */
    enRetVal = NsmErrorStatus_Ok;

    g_mutex_lock(&NSM__pNodeStateMutex);

    if(!boExternalOrigin || !NSM__boBlockExternalNodeState)
    {
       /* Only store the new value and emit a signal, if the new value is different */
       if(NSM__enNodeState != enNodeState)
       {
         if(!(NSM__enNodeState == NsmNodeState_Shutdown && (enNodeState == NsmNodeState_ShuttingDown || enNodeState == NsmNodeState_FastShutdown)))
         {
            if(!NSM__boResetActive ||
                enNodeState == NsmNodeState_Shutdown ||
                enNodeState == NsmNodeState_ShuttingDown ||
                enNodeState == NsmNodeState_FastShutdown)
            {
               /* Store the last NodeState, before switching to the new one */
               DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Changed NodeState -"),
                                                 DLT_STRING(NODESTATE_STRING[NSM__enNodeState]), DLT_INT((gint) NSM__enNodeState), DLT_STRING("=>"),
                                                 DLT_STRING(NODESTATE_STRING[enNodeState]), DLT_INT((gint) enNodeState));

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
               if (enNodeState == NsmNodeState_FastShutdown || enNodeState == NsmNodeState_ShuttingDown)
               {
                  size_t shutdownType = (enNodeState == NsmNodeState_FastShutdown) ? NSM_SHUTDOWNTYPE_PARALLEL | NSM_SHUTDOWNTYPE_FAST : NSM_SHUTDOWNTYPE_PARALLEL | NSM_SHUTDOWNTYPE_NORMAL;
                  NSM__cancelCollectiveTimeoutThread();
                  NSM__startCollectiveTimeoutThread(shutdownType);
                  g_mutex_unlock(&NSM__pNodeStateMutex);
                  NSM__vCallParallelLifecycleClient(TRUE);
               }
               else
               {
                  NSM__cancelCollectiveTimeoutThread();
                  NSM__startCollectiveTimeoutThread(NSM_SHUTDOWNTYPE_RUNUP);
                  g_mutex_unlock(&NSM__pNodeStateMutex);
                  NSM__vCallNextLifecycleClient();
               }
               DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Finished setting NodeState:"), DLT_STRING(NODESTATE_STRING[NSM__enNodeState]), DLT_INT((gint) NSM__enNodeState));
            }
            else
            {
               DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: A reset is being processed! Will not run up again!"));
               enRetVal = NsmErrorStatus_Error;
               g_mutex_unlock(&NSM__pNodeStateMutex);
            }
         }
         else
         {
            DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Already in Shutdown Mode! Will not shutdown again."));
            g_mutex_unlock(&NSM__pNodeStateMutex);
         }
       }
       else
       {
         /* NodeState stays the same. Just leave the lock. */
         g_mutex_unlock(&NSM__pNodeStateMutex);
       }
    }
    else
    {
       enRetVal = NsmErrorStatus_Error;
       DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Set NodeState not allowed from external anymore!"));
       g_mutex_unlock(&NSM__pNodeStateMutex);
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
    *penNodeState = NSM__enNodeState;
    enRetVal = NsmErrorStatus_Ok;
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
                                        DLT_STRING(SHUTDOWNREASON_STRING[enCurrentShutdownReason]), DLT_INT((gint) enCurrentShutdownReason), DLT_STRING("=>"),
                                        DLT_STRING(SHUTDOWNREASON_STRING[enNewShutdownReason]), DLT_INT((gint) enNewShutdownReason    ));

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
                                       DLT_STRING("Old ShutdownReason:"),     DLT_STRING(SHUTDOWNREASON_STRING[enCurrentShutdownReason]), DLT_INT((gint) enCurrentShutdownReason),
                                       DLT_STRING("Desired ShutdownReason:"), DLT_INT((gint) enNewShutdownReason    ));
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
                                         DLT_STRING("State machine returned:"),      DLT_STRING(ERRORSTATUS_STRING[enStateMachineReturn]),
                                         DLT_STRING("Application:"),                 DLT_STRING(pstChangedSession->sOwner  ),
                                         DLT_STRING("Session:"),                     DLT_STRING(pstChangedSession->sName   ),
                                         DLT_STRING("Seat:"),                        DLT_STRING(SEAT_STRING[pstChangedSession->enSeat]  ),
                                         DLT_STRING("Desired state:"),               DLT_STRING(SESSIONSTATE_STRING[pstChangedSession->enState]));
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

  g_mutex_lock(&NSM__pSessionMutex);

  pListEntry = g_slist_find_custom(NSM__pSessions, pstSession, &NSM__i32SessionOwnerNameSeatCompare);

  if(pListEntry != NULL)
  {
    enRetVal = NsmErrorStatus_Ok;
    pExistingSession = (NsmSession_s*) pListEntry->data;

    if(pExistingSession->enState != pstSession->enState)
    {
      DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Changed product session's state."),
                                        DLT_STRING("Application:"), DLT_STRING(pExistingSession->sOwner ),
                                        DLT_STRING("Session:"),     DLT_STRING(pExistingSession->sName  ),
                                        DLT_STRING("Seat:"),        DLT_STRING(SEAT_STRING[pExistingSession->enSeat]),
                                        DLT_STRING("Old state:"),   DLT_STRING(SESSIONSTATE_STRING[pExistingSession->enState]),
                                        DLT_STRING("New state:"),   DLT_STRING(SESSIONSTATE_STRING[pstSession->enState] ));
      pExistingSession->enState = pstSession->enState;
      NSM__vPublishSessionChange(pExistingSession, boInformBus, boInformMachine);
    }
  }
  else
  {
    enRetVal = NsmErrorStatus_WrongSession;
    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to set session state. Session unknown."),
                                       DLT_STRING("Application:"),   DLT_STRING(pstSession->sOwner  ),
                                       DLT_STRING("Session:"),       DLT_STRING(pstSession->sName   ),
                                       DLT_STRING("Seat:"),          DLT_STRING(SEAT_STRING[pstSession->enSeat]),
                                       DLT_STRING("Desired state:"), DLT_INT(   pstSession->enState));
  }

  g_mutex_unlock(&NSM__pSessionMutex);

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
  g_mutex_lock(&NSM__pSessionMutex);

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
                                          DLT_STRING("Application:"), DLT_STRING(pExistingSession->sOwner ),
                                          DLT_STRING("Session:"),     DLT_STRING(pExistingSession->sName  ),
                                          DLT_STRING("Seat:"),        DLT_STRING(SEAT_STRING[pExistingSession->enSeat]),
                                          DLT_STRING("Old state:"),   DLT_STRING(SESSIONSTATE_STRING[pExistingSession->enState]),
                                          DLT_STRING("New state:"),   DLT_STRING(SESSIONSTATE_STRING[pstSession->enState] ));

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
                                            DLT_STRING("Application:"), DLT_STRING(pExistingSession->sOwner ),
                                            DLT_STRING("Session:"),     DLT_STRING(pExistingSession->sName  ),
                                            DLT_STRING("Seat:"),        DLT_STRING(SEAT_STRING[pExistingSession->enSeat]),
                                            DLT_STRING("Old state:"),   DLT_STRING(SESSIONSTATE_STRING[pExistingSession->enState]),
                                            DLT_STRING("New state:"),   DLT_STRING(SESSIONSTATE_STRING[pstSession->enState] ));

          pExistingSession->enState = pstSession->enState;

          NSM__vPublishSessionChange(pExistingSession, boInformBus, boInformMachine);
        }
        else
        {
          /* The session has no owner, but could not be activated because the passed state is "inactive". */
          enRetVal = NsmErrorStatus_Parameter;

          DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to enable default session. Passed state is 'inactive'. "),
                                             DLT_STRING("Session:"),                DLT_STRING(pstSession->sName           ),
                                             DLT_STRING("Seat:"),                   DLT_STRING(SEAT_STRING[pstSession->enSeat]),
                                             DLT_STRING("Owning application:"), DLT_STRING(pExistingSession->sOwner    ),
                                             DLT_STRING("Requesting application:"), DLT_STRING(pstSession->sOwner          ));
        }
      }
      else
      {
        /* The session owners do not match and the existing session has an owner */
        enRetVal = NsmErrorStatus_Error;

        DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to set default session state. Session has another owner."),
                                           DLT_STRING("Session:"),                DLT_STRING(pstSession->sName            ),
                                           DLT_STRING("Seat:"),                   DLT_STRING(SEAT_STRING[pstSession->enSeat]),
                                           DLT_STRING("Owning application:"), DLT_STRING(pExistingSession->sOwner     ),
                                           DLT_STRING("Requesting application:"), DLT_STRING(pstSession->sOwner           ));
      }
    }
  }
  else
  {
    /* This should never happen, because the function is only called for default sessions! */
    enRetVal = NsmErrorStatus_Internal;
    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Critical error. Default session not found in session list!"),
                                       DLT_STRING("Application:"),   DLT_STRING(pstSession->sOwner               ),
                                       DLT_STRING("Session:"),       DLT_STRING(pstSession->sName                ),
                                       DLT_STRING("Seat:"),          DLT_STRING(SEAT_STRING[pstSession->enSeat]  ),
                                       DLT_STRING("Desired state:"), DLT_STRING(SESSIONSTATE_STRING[pstSession->enState]));
  }

  /* Unlock the sessions again. */
  g_mutex_unlock(&NSM__pSessionMutex);

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
                                       DLT_STRING("Application:"),   DLT_STRING(pstSession->sOwner      ),
                                       DLT_STRING("Session:"),       DLT_STRING(pstSession->sName       ),
                                       DLT_STRING("Seat:"),          DLT_STRING(SEAT_STRING[pstSession->enSeat]      ),
                                       DLT_STRING("Desired state:"), DLT_STRING(SESSIONSTATE_STRING[pstSession->enState]));
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

  g_mutex_lock(&NSM__pSessionMutex);

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
                                      DLT_STRING("Session:"),       DLT_STRING(pstSession->sName        ),
                                      DLT_STRING("Seat:"),          DLT_STRING(SEAT_STRING[pstSession->enSeat]));
  }

  g_mutex_unlock(&NSM__pSessionMutex);

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
  if(pListClient->clientHash == pCompareClient->clientHash)
  {
      i32RetVal = 0;  /* Clients are identical. Return 0.       */
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

/*
 * Helper function to call NSM__vCallParallelLifecycleClient in a thread
 */
static void* callParallelLifecycleClient(void* ignore)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   NSM__vCallParallelLifecycleClient(FALSE);
   NSMUnregisterWatchdog();
   return NULL;
}
/*
 * Helper function to call NSM__vCallNextLifecycleClient in a thread
 */
static void* callLifecycleClient(void* ignore)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   NSM__vCallNextLifecycleClient();
   NSMUnregisterWatchdog();
   return NULL;
}

/**********************************************************************************************************************
*
* The function is called after a lifecycle client was informed about the changed life cycle.
* The return value of the last informed client will be evaluated and the next lifecycle client
* to inform will be determined and called.
* If there is no client left, the lifecycle sequence will be finished.
*
* @param enErrorStatus: Status of the client
* @param clientID:      array of clients that have returned
* @param numClients:    Number of clients that have finished
*                       Only is greater than one when multiple parallel clients have timed out.
*
* @return void
*
**********************************************************************************************************************/
static void NSM__vOnLifecycleRequestFinish(size_t clientID, gboolean timeout, gboolean late)
{
  g_mutex_lock(&NSM__pNodeStateMutex);

  GList *pListEntry = NULL; /* Iterate through list entries */
  NSM__tstLifecycleClient currentClient = {0}; /* Client object from list      */
  NSM__tstLifecycleClient *pCurrentClient = NULL; /* Client object from list      */

  for (pListEntry = g_list_last(NSM__pLifecycleClients);
      (pListEntry != NULL) && (pCurrentClient == NULL);
      pListEntry = g_list_previous(pListEntry))
  {
    /* Check if client is the one that returned */
    if ((((NSM__tstLifecycleClient*) pListEntry->data)->clientHash == clientID))
    {
      /* Found the current client */
      pCurrentClient = (NSM__tstLifecycleClient*) pListEntry->data;
      memcpy(&currentClient, pCurrentClient, sizeof(NSM__tstLifecycleClient));
      /* A timed out client has still a pending call */
      if(!timeout)
      {
        pCurrentClient->boPendingCall = FALSE;
      }

      if (!late)
      {
        /* If client is in time (or has timed out) continue as normal */
        if (NSM_SHUTDOWNTYPE_PARALLEL & NSM__uiShutdownType)
        {
          pthread_create(&NSM__callLCThread, NULL, callParallelLifecycleClient, NULL);
          pthread_detach(NSM__callLCThread);
        }
        else
        {
          pthread_create(&NSM__callLCThread, NULL, callLifecycleClient, NULL);
          pthread_detach(NSM__callLCThread);
        }
      }
      else
      {
        /* Add parallel flag to lifecycle request when client has registered for parallel */
        uint32_t uiShutdownType = pCurrentClient->u32RegisteredMode & NSM_SHUTDOWNTYPE_PARALLEL ? NSM_SHUTDOWNTYPE_PARALLEL : NSM_SHUTDOWNTYPE_NOT;

        /* If client has returned to late inform him about current possible changes */
        if((NSM__uiShutdownType & NSM_SHUTDOWNTYPE_RUNUP) && pCurrentClient->boShutdown)
        {
          pCurrentClient->boShutdown = FALSE;
          g_mutex_unlock(&NSM__pNodeStateMutex);
          NSMA_boCallLcClientRequestWithoutTimeout(pCurrentClient, uiShutdownType | NSM_SHUTDOWNTYPE_RUNUP);
          g_mutex_lock(&NSM__pNodeStateMutex);
        }
        else if((NSM__uiShutdownType & NSM_SHUTDOWNTYPE_FAST) != 0  && (pCurrentClient->u32RegisteredMode & NSM_SHUTDOWNTYPE_FAST) != 0 && !pCurrentClient->boShutdown)
        {
          pCurrentClient->boShutdown = TRUE;
          g_mutex_unlock(&NSM__pNodeStateMutex);
          NSMA_boCallLcClientRequestWithoutTimeout(pCurrentClient, uiShutdownType | NSM_SHUTDOWNTYPE_FAST);
          g_mutex_lock(&NSM__pNodeStateMutex);
        }
        else if((NSM__uiShutdownType & NSM_SHUTDOWNTYPE_NORMAL) != 0  && (pCurrentClient->u32RegisteredMode & NSM_SHUTDOWNTYPE_NORMAL) != 0 && !pCurrentClient->boShutdown)
        {
          pCurrentClient->boShutdown = TRUE;
          g_mutex_unlock(&NSM__pNodeStateMutex);
          NSMA_boCallLcClientRequestWithoutTimeout(pCurrentClient, uiShutdownType | NSM_SHUTDOWNTYPE_NORMAL);
          g_mutex_lock(&NSM__pNodeStateMutex);
        }
        else
        {
          DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: No need to inform late shutdown client as it is in a valid state."),
                                            DLT_STRING("ClientID:"), DLT_UINT64(pCurrentClient->clientHash),
                                            DLT_STRING("Client is shutdown:"), DLT_BOOL((uint8_t)pCurrentClient->boShutdown),
                                            DLT_STRING("Current shutdown type:"), DLT_INT(NSM__uiShutdownType));

        }
      }
    }
  }
  g_mutex_unlock(&NSM__pNodeStateMutex);
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
*              "main_loop", which leads to the the termination of the NSM!
*
* @return void
*
**********************************************************************************************************************/
static void NSM__vCallNextLifecycleClient(void)
{
  /* Function local variables                                                                      */
  GList                   *pListEntry      = NULL;                 /* Iterate through list entries */
  NSM__tstLifecycleClient *pClient         = NULL;                 /* Client object from list      */
  NSM__tstLifecycleClient currentLifecycleClient = {0};

  g_mutex_lock(&NSM__pNodeStateMutex);

  /* When a sequential client is still being informed do nothing for now */
  if(!NSMA__SequentialClientHasPendingActiveCall())
  {
    if(!NSMA__ParallelClientHasPendingActiveCall(0))
    {
      /* Based on NodeState determine if clients have to shutdown or run up. Find a client that has not been informed */
      switch(NSM__enNodeState)
      {
        case NsmNodeState_Shutdown: break;
        /* For "shutdown" search backward in the list, until there is a client that has not been shut down */
        case NsmNodeState_ShuttingDown:
           NSM__uiShutdownType = NSM_SHUTDOWNTYPE_NORMAL;
          for( pListEntry = g_list_last(NSM__pLifecycleClients);
              (pListEntry != NULL) && (currentLifecycleClient.clientHash == 0);
              pListEntry = g_list_previous(pListEntry))
          {
            /* Check if client has not been shut down and is registered for "normal shutdown" */
            pClient = (NSM__tstLifecycleClient*) pListEntry->data;
            if(   (  pClient->boShutdown                           == FALSE)
               && ( (pClient->u32RegisteredMode & NSM__uiShutdownType) != 0    )
               && ( (pClient->u32RegisteredMode & NSM_SHUTDOWNTYPE_PARALLEL) == 0 )) // NSM_SHUTDOWNTYPE_PARALLEL have been notified earlier
            {
              /* Found a "running" previous client, registered for the shutdown mode */
              memcpy(&currentLifecycleClient, pClient, sizeof(NSM__tstLifecycleClient));
            }
          }
        break;

        /* For "fast shutdown" search backward in the list, until there is a client that has not been shut down */
        case NsmNodeState_FastShutdown:
           NSM__uiShutdownType = NSM_SHUTDOWNTYPE_FAST;
          for( pListEntry = g_list_last(NSM__pLifecycleClients);
              (pListEntry != NULL) && (currentLifecycleClient.clientHash == 0);
              pListEntry = g_list_previous(pListEntry))
          {
            /* Check if client has not been shut down and is registered for "fast shutdown" */
            pClient = (NSM__tstLifecycleClient*) pListEntry->data;
            if(   (  pClient->boShutdown                           == FALSE )
               && ( (pClient->u32RegisteredMode & NSM__uiShutdownType) != 0 )
               && ( (pClient->u32RegisteredMode & NSM_SHUTDOWNTYPE_PARALLEL) == 0 )) // NSM_SHUTDOWNTYPE_PARALLEL have been notified earlier
            {
              /* Found a "running" previous client, registered for the shutdown mode */
              memcpy(&currentLifecycleClient, pClient, sizeof(NSM__tstLifecycleClient));
            }
          }
        break;

        /* For a "running" mode search forward in the list (get next), until there is a client that is shut down */
        default:
           NSM__uiShutdownType = NSM_SHUTDOWNTYPE_RUNUP;
          for(pListEntry = g_list_first(NSM__pLifecycleClients);
              (pListEntry != NULL) && (currentLifecycleClient.clientHash == 0);
              pListEntry = g_list_next(pListEntry))
          {
            /* Check if client is shut down */
            pClient = (NSM__tstLifecycleClient*) pListEntry->data;
            if(pClient->boShutdown == TRUE && ((pClient->u32RegisteredMode & NSM_SHUTDOWNTYPE_PARALLEL) == 0 ))
            {
              /* The client was shutdown. It should run up, because we are in a running mode */
              memcpy(&currentLifecycleClient, pClient, sizeof(NSM__tstLifecycleClient));
            }
          }
        break;
      }
    }

    /* Check if a client could be found that needs to be informed */
    if(currentLifecycleClient.clientHash != 0)
    {
      DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Call lifecycle client."                                                  ),
                                        DLT_STRING("ClientID:"),         DLT_UINT64(currentLifecycleClient.clientHash     ),
                                        DLT_STRING("Registered types:"), DLT_INT(currentLifecycleClient.u32RegisteredMode),
                                        DLT_STRING("ShutdownType:"),     DLT_UINT(NSM__uiShutdownType                               ));

      /* Remember that client received a run-up or shutdown call */
      pClient->boShutdown = (NSM__uiShutdownType != NSM_SHUTDOWNTYPE_RUNUP);

      NSM__vLtProf(currentLifecycleClient.clientHash, NSM__uiShutdownType, "enter: ", 0);

      NSMA_boCallLcClientRequest(&currentLifecycleClient, NSM__uiShutdownType);
      g_mutex_unlock(&NSM__pNodeStateMutex);
    }
    else
    {
      /* The last client was called. Depending on the NodeState check if we can end. */
      switch(NSM__enNodeState)
      {
        case NsmNodeState_Shutdown:
          g_mutex_unlock(&NSM__pNodeStateMutex);
          break;
          /* All registered clients have been 'fast shutdown'. Set NodeState to "shutdown" */
        case NsmNodeState_FastShutdown:
          if (!NSMA__ParallelClientHasPendingActiveCall(0))
          {
            DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Informed all registered clients about 'fast shutdown'. Set NodeState to 'shutdown'"));
            /* Store the last NodeState, before switching to the new one */
            DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Changed NodeState -"),
                DLT_STRING(NODESTATE_STRING[NSM__enNodeState]), DLT_INT((gint) NSM__enNodeState), DLT_STRING("=>"),
                DLT_STRING(NODESTATE_STRING[NsmNodeState_Shutdown]), DLT_INT((gint) NsmNodeState_Shutdown));

            NSM__cancelCollectiveTimeoutThread();

            NSM__enNodeState = NsmNodeState_Shutdown;
            NsmcSetData(NsmDataType_NodeState, (unsigned char*) &NSM__enNodeState, sizeof(NsmNodeState_e));
            NSMA_boSendNodeStateSignal(NSM__enNodeState);
          }
          g_mutex_unlock(&NSM__pNodeStateMutex);
        break;

        /* All registered clients have been 'shutdown'. Set NodeState to "shutdown" */
        case NsmNodeState_ShuttingDown:
          if (!NSMA__ParallelClientHasPendingActiveCall(0))
          {
            DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Informed all registered clients about 'shutdown'. Set NodeState to 'shutdown'."));
            /* Store the last NodeState, before switching to the new one */
            DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Changed NodeState -"),
                DLT_STRING(NODESTATE_STRING[NSM__enNodeState]), DLT_INT((gint) NSM__enNodeState), DLT_STRING("=>"),
                DLT_STRING(NODESTATE_STRING[NsmNodeState_Shutdown]), DLT_INT((gint) NsmNodeState_Shutdown));

            NSM__cancelCollectiveTimeoutThread();

            NSM__enNodeState = NsmNodeState_Shutdown;
            NsmcSetData(NsmDataType_NodeState, (unsigned char*) &NSM__enNodeState, sizeof(NsmNodeState_e));
            NSMA_boSendNodeStateSignal(NSM__enNodeState);
          }
          g_mutex_unlock(&NSM__pNodeStateMutex);
        break;

        default:
          NSM__cancelCollectiveTimeoutThread();
          NSM__startCollectiveTimeoutThread(NSM_SHUTDOWNTYPE_PARALLEL | NSM_SHUTDOWNTYPE_RUNUP);
          g_mutex_unlock(&NSM__pNodeStateMutex);
          NSM__vCallParallelLifecycleClient(TRUE);
        break;
      }
    }
  }
  else
  {
    g_mutex_unlock(&NSM__pNodeStateMutex);
  }
}

static void reportPendingCall(size_t clientID, char* reason)
{
  if(NSMA__ParallelClientHasPendingActiveCall(clientID))
    DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSM: Will NOT inform client"), DLT_INT64(clientID),
                                       DLT_STRING("about"), DLT_STRING(reason), DLT_STRING("yet, as there is still a (valid) pending lifecycle call!"));
  else
    DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSM: Will NOT inform client"), DLT_INT64(clientID),
                                       DLT_STRING("about"), DLT_STRING(reason), DLT_STRING("yet, as there is still a (timed out) pending lifecycle call!"));
}

/**********************************************************************************************************************
*
* The function is called when:
*    - A shutdown is active before the "normal/sequential" clients
*    - A runup is active after all "normal/sequential" clients have been notified and finished
*
* If the clients need to "run up" or shut down for the current NodeState, the function
* searches the list until a client is found, which needs to be informed and supports parallel shutdown.
*
* @return void
*
**********************************************************************************************************************/
static void NSM__vCallParallelLifecycleClient(gboolean verbose)
{
  int arrayIndex = 0;
  NSM__tstLifecycleClient *pClient = NULL; /* Client object from list      */
  GList *pListEntry = NULL; /* Iterate through list entries */
  g_mutex_lock(&NSM__pNodeStateMutex);

  /*
   * Allocate a array which can hold all clients.
   * Note:       An array is needed to have a generic C(glib-2.0)/C++11 independent container.
   *             This way it is possible to pass it to NSMA_boCallParallelLcClientsRequest which internally converts it to
   *             a CommonAPI::ClientIdList
   * */
  NSM__tstLifecycleClient *pParallelLifecycleClients = alloca(sizeof(NSM__tstLifecycleClient) * g_list_length(NSM__pLifecycleClients));
  memset(pParallelLifecycleClients, 0, sizeof(NSM__tstLifecycleClient) * g_list_length(NSM__pLifecycleClients));
  if (!NSMA__SequentialClientHasPendingActiveCall())
  {
    switch (NSM__enNodeState)
    {
    case NsmNodeState_Shutdown: break;
    /* For "shutdown" search backward in the list, until there is a client that has not been shut down */
    case NsmNodeState_ShuttingDown:
      NSM__uiShutdownType = NSM_SHUTDOWNTYPE_NORMAL | NSM_SHUTDOWNTYPE_PARALLEL;
      for (pListEntry = g_list_first(NSM__pLifecycleClients); pListEntry != NULL; pListEntry = g_list_next(pListEntry))
      {
        /* Check if client has not shut down and is registered for "normal/parallel shutdown" then add him to the array */
        pClient = (NSM__tstLifecycleClient*) pListEntry->data;
        if ((pClient->boShutdown == FALSE)
            && ((pClient->u32RegisteredMode & NSM_SHUTDOWNTYPE_PARALLEL) != 0)
            && ((pClient->u32RegisteredMode & NSM_SHUTDOWNTYPE_NORMAL) != 0))
        {
          if (!pClient->boPendingCall)
          {
            /* Remember that client received a run-up or shutdown call */
            pClient->boShutdown = TRUE;
            pClient->boPendingCall = TRUE;
            memcpy(&pParallelLifecycleClients[arrayIndex], pClient, sizeof(NSM__tstLifecycleClient));
            arrayIndex++;
          }
          else if(verbose)
          {
            reportPendingCall(pClient->clientHash, "parallel shutdown");
          }
        }
      }
      break;
      /* For "fast shutdown" search backward in the list, until there is a client that has not been shut down */
    case NsmNodeState_FastShutdown:
      NSM__uiShutdownType = NSM_SHUTDOWNTYPE_FAST | NSM_SHUTDOWNTYPE_PARALLEL;
      for (pListEntry = g_list_first(NSM__pLifecycleClients); pListEntry != NULL; pListEntry = g_list_next(pListEntry))
      {
        /* Check if client has not shut down and is registered for "fast/parallel shutdown" then add him to the array */
        pClient = (NSM__tstLifecycleClient*) pListEntry->data;
        if ((pClient->boShutdown == FALSE)
            && ((pClient->u32RegisteredMode & NSM_SHUTDOWNTYPE_PARALLEL) != 0)
            && ((pClient->u32RegisteredMode & NSM_SHUTDOWNTYPE_FAST) != 0))
        {
          if (!pClient->boPendingCall)
          {
            /* Remember that client received a run-up or shutdown call */
            pClient->boShutdown = TRUE;
            pClient->boPendingCall = TRUE;
            memcpy(&pParallelLifecycleClients[arrayIndex], pClient, sizeof(NSM__tstLifecycleClient));
            arrayIndex++;
          }
          else if(verbose)
          {
            reportPendingCall(pClient->clientHash, "parallel fast shutdown");
          }
        }
      }
      break;
    default:
      NSM__uiShutdownType = NSM_SHUTDOWNTYPE_RUNUP | NSM_SHUTDOWNTYPE_PARALLEL;
      for (pListEntry = g_list_first(NSM__pLifecycleClients); pListEntry != NULL; pListEntry = g_list_next(pListEntry))
      {
        /* Check if client has shut down and is registered for "parallel shutdown" then add him to the array */
        pClient = (NSM__tstLifecycleClient*) pListEntry->data;
        if ((pClient->boShutdown == TRUE)
            && ((pClient->u32RegisteredMode & NSM_SHUTDOWNTYPE_PARALLEL) != 0))
        {
          if (!pClient->boPendingCall)
          {
            /* Remember that client received a run-up or shutdown call */
            pClient->boShutdown = FALSE;
            pClient->boPendingCall = TRUE;
            memcpy(&pParallelLifecycleClients[arrayIndex], pClient, sizeof(NSM__tstLifecycleClient));
            arrayIndex++;
          }
          else if(verbose)
          {
            reportPendingCall(pClient->clientHash, "parallel runup");
          }
        }
      }
    }
  }

  /* Check if a client could be found that needs to be informed */
  if (arrayIndex > 0)
  {
    g_mutex_unlock(&NSM__pNodeStateMutex);
    NSMA_boCallParallelLcClientsRequest(pParallelLifecycleClients, arrayIndex, NSM__uiShutdownType);
    DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Informed"), DLT_INT(arrayIndex), DLT_STRING("clients!"),
        DLT_STRING("ShutdownType:"), DLT_UINT(NSM__uiShutdownType));
  }
  else if (!NSMA__SequentialClientHasPendingActiveCall() && !NSMA__ParallelClientHasPendingActiveCall(0))
  {
    /* The last client was called. Depending on the NodeState check if we can end. */
    switch (NSM__enNodeState)
    {
    case NsmNodeState_Shutdown:
      g_mutex_unlock(&NSM__pNodeStateMutex);
      break;
    case NsmNodeState_FastShutdown:
    case NsmNodeState_ShuttingDown:
      DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: No more parallel clients pending (for this lifecycle)"));

      size_t shutdownType = (NSM__enNodeState == NsmNodeState_FastShutdown) ? NSM_SHUTDOWNTYPE_FAST : NSM_SHUTDOWNTYPE_NORMAL;

      NSM__cancelCollectiveTimeoutThread();
      NSM__startCollectiveTimeoutThread(shutdownType);
      g_mutex_unlock(&NSM__pNodeStateMutex);

      NSM__vCallNextLifecycleClient();
      break;
      /* We are in a running state. */
    default:
      NSM__cancelCollectiveTimeoutThread();

      if (NSM__enNodeState == NsmNodeState_Resume)
      {
        DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Informed all registered clients about 'resume'. Set NodeState to 'NsmNodeState_FullyOperational'."));
        DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Changed NodeState -"),
            DLT_STRING(NODESTATE_STRING[NSM__enNodeState]), DLT_INT((gint) NSM__enNodeState), DLT_STRING("=>"),
            DLT_STRING(NODESTATE_STRING[NsmNodeState_FullyOperational]), DLT_INT((gint) NsmNodeState_FullyOperational));
        NSM__enNodeState = NsmNodeState_FullyOperational;
        NsmcSetData(NsmDataType_NodeState, (unsigned char*) &NSM__enNodeState, sizeof(NsmNodeState_e));
        NSMA_boSendNodeStateSignal(NsmNodeState_FullyOperational);
      }
      g_mutex_unlock(&NSM__pNodeStateMutex);
      break;
    }
  }
  else
  {
    g_mutex_unlock(&NSM__pNodeStateMutex);
  }
}


/**********************************************************************************************************************
*
* The callback is called when a check for LUC is required.
* It uses the NodeStateMachine to determine whether LUC is required.
*
* @return Boolean if luc is required according to StateMAchine
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
* @return:   Status of method call. (NsmErrorStatus_e)
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
* @return:   Status of method call. (NsmErrorStatus_e)
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enOnHandleSetNodeState(const NsmNodeState_e  enNodeState)
{
  return NSM__enSetNodeState(enNodeState, TRUE, TRUE, TRUE);
}

/**********************************************************************************************************************
*
* The callback is called when the node reset is requested.
* It passes the request to the NodestateMachine.
*
* @param i32RestartReason:     Restart reason
* @param i32RestartType:       Restart type
* @return:                     Status of method call. (NsmErrorStatus_e)
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enOnHandleRequestNodeRestart(const NsmRestartReason_e enRestartReason,
                                                          const guint              u32RestartType)
{
  NsmErrorStatus_e enRetVal = NsmErrorStatus_NotSet;

  DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Node restart has been requested."));
  g_mutex_lock(&NSM__pNodeStateMutex);
  NSM__boResetActive = TRUE;
  g_mutex_unlock(&NSM__pNodeStateMutex);

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
* @return:               Status of method call. (NsmErrorStatus_e)
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
                                       DLT_STRING("Seat:"         ), DLT_STRING(SEAT_STRING[enSeatId]   ),
                                       DLT_STRING("Initial state:"), DLT_STRING(SESSIONSTATE_STRING[enSessionState]));
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
* @return:              Status of method call. (NsmErrorStatus_e)
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enOnHandleUnRegisterSession(const gchar     *sSessionName,
                                                         const gchar     *sSessionOwner,
                                                         const NsmSeat_e  enSeatId)
{
  /* Function local variables                                                                   */
  glong             u32SessionNameLen  = 0;                   /* Length of passed session owner */
  glong             u32SessionOwnerLen = 0;                   /* Length of passed session name  */
  NsmSession_s      stSearchSession    = {{0}, {0}, 0, 0};    /* To search for existing session */
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
    /* Error: Invalid parameter. The session or owner name is too long. */
    enRetVal = NsmErrorStatus_Parameter;
    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to unregister session. The session or owner name is too long."),
                                       DLT_STRING("Name:"      ), DLT_STRING(sSessionName                                 ),
                                       DLT_STRING("Owner:"     ), DLT_STRING(sSessionOwner                                ),
                                       DLT_STRING("Seat:"      ), DLT_STRING(SEAT_STRING[enSeatId]                        ));
  }

  return enRetVal;
}

static void NSM__adjustMaxParallelTimeout()
{
  GList *clientIter;
  /* Reset max parallel timeout ... */
  NSM__max_parallel_timeout = 0;
  for (clientIter = NSM__pLifecycleClients; clientIter != NULL; clientIter = clientIter->next)
  {
    NSM__tstLifecycleClient *client = (NSM__tstLifecycleClient*) clientIter->data;
    if ((client->u32RegisteredMode & NSM_SHUTDOWNTYPE_PARALLEL) && client->timeout > NSM__max_parallel_timeout)
    {
      NSM__max_parallel_timeout = ((NSM__tstLifecycleClient*) clientIter->data)->timeout;
    }
  }
}

/**********************************************************************************************************************
*
* The callback is called when a lifecycle client should be registered.
* In the list of lifecycle clients it will be checked if the client already exists.
* If it exists, it's settings will be updated. Otherwise a new client will be created.
*
* @param clientHash:      Hash of the lifecycle client. Used for identification.
* @param client:          Object of the lifecycle client
* @param u32ShutdownMode: Shutdown mode for which the client wants to be informed
* @param u32TimeoutMs:    Timeout in ms. If the client does not return after the specified time, the NSM
*                         aborts its shutdown and calls the next client.
* @return:                Status of method call. (NsmErrorStatus_e)
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enOnHandleRegisterLifecycleClient(const size_t clientHash,
                                                               const guint  u32ShutdownMode,
                                                               const guint  u32TimeoutMs)
{
  NSM__tstLifecycleClient     stTestLifecycleClient = {0};
  NSM__tstLifecycleClient    *pstNewClient          = NULL;
  NSM__tstLifecycleClient    *pstExistingClient     = NULL;
  GList                      *pListEntry            = NULL;
  guint                       timeout               = u32TimeoutMs;
  NsmErrorStatus_e            enRetVal              = NsmErrorStatus_NotSet;

  /* The parameters are valid. Create a temporary client to search the list */
  stTestLifecycleClient.clientHash = clientHash;

  /* Check if the lifecycle client already is registered */
  pListEntry = g_list_find_custom(NSM__pLifecycleClients, &stTestLifecycleClient, &NSM__i32LifecycleClientCompare);

  /* Allow a maximal timeout of 60 seconds */
  if (60000 < timeout)
  {
     DLT_LOG(NsmContext, DLT_LOG_WARN, DLT_STRING("NSM: Client specified timeout greater 60 seconds. ClientID:"), DLT_UINT64(clientHash));
     timeout = 60000;
  }

  if (pListEntry == NULL)
  {
     enRetVal = NsmErrorStatus_Ok;

     /* Create client object and copies of the strings. */
     pstNewClient = g_new0(NSM__tstLifecycleClient, 1);
     pstNewClient->u32RegisteredMode = u32ShutdownMode;
     pstNewClient->clientHash        = clientHash;
     pstNewClient->boShutdown        = FALSE;
     pstNewClient->timeout           = timeout;
     pstNewClient->boPendingCall     = FALSE;

     if(!(pstNewClient->u32RegisteredMode & NSM_SHUTDOWNTYPE_PARALLEL))
     {
       NSM__collective_sequential_timeout += timeout;
     }
     else if(timeout > NSM__max_parallel_timeout)
     {
       NSM__max_parallel_timeout = timeout;
     }

     /* Append the new client to the list */
     NSM__pLifecycleClients = g_list_append(NSM__pLifecycleClients, pstNewClient);
     DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Registered new lifecycle consumer."                 ),
                                       DLT_STRING("ClientID:"), DLT_UINT64(pstNewClient->clientHash    ),
                                       DLT_STRING("Timeout:"), DLT_UINT(  timeout                   ),
                                       DLT_STRING("Mode(s):"),  DLT_INT(   pstNewClient->u32RegisteredMode));
  }
  else
  {
    /* The client already exists. Assert to update the values for timeout and mode */
    enRetVal = NsmErrorStatus_Last;

    pstExistingClient = (NSM__tstLifecycleClient*) pListEntry->data;

    guint oldShutdownMode = pstExistingClient->u32RegisteredMode;
    guint oldTimeout = pstExistingClient->timeout;

    pstExistingClient->u32RegisteredMode |= u32ShutdownMode;

    if(timeout != 0)
    {
      pstExistingClient->timeout = timeout;

      if(pstExistingClient->u32RegisteredMode != 0)
      {
        /* If client has been registered for sequential events and is now registered for parallel events */
        if(!(oldShutdownMode & NSM_SHUTDOWNTYPE_PARALLEL) && (pstExistingClient->u32RegisteredMode & NSM_SHUTDOWNTYPE_PARALLEL))
        {
          NSM__collective_sequential_timeout -= oldTimeout;
        }

        if(!(pstExistingClient->u32RegisteredMode & NSM_SHUTDOWNTYPE_PARALLEL))
        {
          NSM__collective_sequential_timeout -= oldShutdownMode;
          NSM__collective_sequential_timeout += timeout;
        }
        /* else if client is parallel one and timeout is the biggest */
        else if(timeout > NSM__max_parallel_timeout)
        {
          NSM__max_parallel_timeout = timeout;
        }
        /* else if client is parallel one and his previous timeout has been biggest and now it is smaller */
        else if(oldTimeout == NSM__max_parallel_timeout && timeout < NSM__max_parallel_timeout)
        {
          /* ... and search which parallel client now has the biggest timeout */
          NSM__adjustMaxParallelTimeout();
        }
      }
    }

    DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Changed lifecycle consumer registration."                        ),
                                      DLT_STRING("ClientID:"),        DLT_UINT64(pstExistingClient->clientHash          ),
                                      DLT_STRING("Timeout:"),           DLT_UINT(pstExistingClient->timeout          ),
                                      DLT_STRING("Registered mode(s):"), DLT_INT(pstExistingClient->u32RegisteredMode ));
  }

  if(120000 < (NSM__collective_sequential_timeout + NSM__max_parallel_timeout) && 0 < timeout)
  {
    DLT_LOG(NsmContext, DLT_LOG_WARN, DLT_STRING("NSM: Collective timeout greater 120 seconds"));
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
* @param clientHash:      Hash of the lifecycle client. Used for identification.
* @param u32ShutdownMode: Shutdown mode for which the client wants to unregister
* @return:                Status of method call. (NsmErrorStatus_e)
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enOnHandleUnRegisterLifecycleClient(const size_t  clientHash,
                                                                 const guint   u32ShutdownMode)
{
  NSM__tstLifecycleClient *pstExistingClient = NULL;
  NSM__tstLifecycleClient  stSearchClient    = {0};
  GList                   *pListEntry        = NULL;
  NsmErrorStatus_e         enRetVal          = NsmErrorStatus_NotSet;

  stSearchClient.clientHash = clientHash;

  /* Check if the lifecycle client already is registered */
  pListEntry = g_list_find_custom(NSM__pLifecycleClients, &stSearchClient, &NSM__i32LifecycleClientCompare);

  /* Check if an existing client could be found */
  if(pListEntry != NULL)
  {
    /* The client could be found in the list. Change the registered shutdown mode */
    enRetVal = NsmErrorStatus_Ok;
    pstExistingClient = (NSM__tstLifecycleClient*) pListEntry->data;
    guint oldShutdownMode = pstExistingClient->u32RegisteredMode;
    pstExistingClient->u32RegisteredMode &= ~(u32ShutdownMode);

    /* If client has been registered for parallel events and is now registered for sequential events */
    if((oldShutdownMode & NSM_SHUTDOWNTYPE_PARALLEL) && !(pstExistingClient->u32RegisteredMode & NSM_SHUTDOWNTYPE_PARALLEL) )
    {
      if(pstExistingClient->timeout == NSM__max_parallel_timeout)
      {
        NSM__adjustMaxParallelTimeout();
      }
      /* If client is still registered for anything */
      if(pstExistingClient->u32RegisteredMode)
      {
        NSM__collective_sequential_timeout += pstExistingClient->timeout;
      }
    }
    /* If client is sequential and still registered for anything */
    else if(!(pstExistingClient->u32RegisteredMode & NSM_SHUTDOWNTYPE_PARALLEL) && !pstExistingClient->u32RegisteredMode)
    {
      NSM__collective_sequential_timeout -= pstExistingClient->timeout;
    }

    DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Unregistered lifecycle consumer for mode(s)."             ),
                                      DLT_STRING("Client hash:"),  DLT_UINT64(pstExistingClient->clientHash),
                                      DLT_STRING("New mode:"),     DLT_INT(pstExistingClient->u32RegisteredMode));
    if(pstExistingClient->u32RegisteredMode == NSM_SHUTDOWNTYPE_NOT)
    {
      NSMA_boDeleteLifecycleClient(pstExistingClient);
      /* The client is not registered for at least one mode. Remove it from the list */
      NSM__pLifecycleClients = g_list_remove(NSM__pLifecycleClients, pstExistingClient);
      NSM__vFreeLifecycleClientObject(pstExistingClient);
    }
  }
  else
  {
    /* Warning: The client could not be found in the list of clients. */
    enRetVal = NsmErrorStatus_Parameter;
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
* @return:                Status of method call. (NsmErrorStatus_e)
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enOnHandleGetSessionState(const gchar       *sSessionName,
                                                       const NsmSeat_e    enSeatId,
                                                       NsmSessionState_e *penSessionState)
{
  /* Function local variables                                                                     */
  NsmErrorStatus_e  enRetVal           = NsmErrorStatus_NotSet;
  glong             u32SessionNameLen  = 0;                     /* Length of passed session owner */
  NsmSession_s      stSearchSession    = {{0}, {0}, 0, 0};      /* To search for existing session */

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
    /* Error: Invalid parameter. The session or owner name is too long. */
    enRetVal = NsmErrorStatus_Parameter;
    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to get session state. The session name is too long."),
                                       DLT_STRING("Name:"      ), DLT_STRING(sSessionName                       ),
                                       DLT_STRING("Seat:"      ), DLT_STRING(SEAT_STRING[enSeatId ]             ));
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
* @return:                Status of method call. (NsmErrorStatus_e)
*
**********************************************************************************************************************/
static NsmErrorStatus_e NSM__enOnHandleSetSessionState(const gchar             *sSessionName,
                                                       const gchar             *sSessionOwner,
                                                       const NsmSeat_e          enSeatId,
                                                       const NsmSessionState_e  enSessionState)
{
  /* Function local variables                                                                       */
  NsmErrorStatus_e enRetVal           = NsmErrorStatus_NotSet;
  glong            u32SessionNameLen  = 0;                /* Length of passed session owner             */
  glong            u32SessionOwnerLen = 0;                /* Length of passed session name              */
  NsmSession_s     stSession          = {{0}, {0}, 0, 0}; /* Session object passed to internal function */

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
    /* Error: Invalid parameter. The session or owner name is too long. */
    enRetVal = NsmErrorStatus_Parameter;
    DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to set session state. Invalid parameter."),
                                       DLT_STRING("Name:"      ), DLT_STRING(sSessionName             ),
                                       DLT_STRING("Owner:"     ), DLT_STRING(sSessionOwner            ),
                                       DLT_STRING("Seat:"      ), DLT_STRING(SEAT_STRING[enSeatId]    ));
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
                                      DLT_STRING("Application:"), DLT_STRING(pstFailedApp->sName));
  }
  else
  {
    /* Error: There was no session registered for the application that failed. */
    enRetVal = NsmErrorStatus_Error;
    DLT_LOG(NsmContext, DLT_LOG_WARN, DLT_STRING("NSM: Failed to set application valid. Application was never invalid."),
                                      DLT_STRING("Application:"), DLT_STRING(pstFailedApp->sName                     ));
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
  NsmSession_s  stSearchSession    = {{0}, {0}, 0, 0};

  /* Only set the "owner" of the session (to the AppName) to search for all sessions of the app. */
  g_strlcpy(stSearchSession.sOwner, pstFailedApp->sName, sizeof(stSearchSession.sOwner));

  g_mutex_lock(&NSM__pSessionMutex);
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
                                        DLT_STRING("Application:"), DLT_STRING(pstExistingSession->sOwner           ),
                                        DLT_STRING("Session:"),     DLT_STRING(pstExistingSession->sName            ),
                                        DLT_STRING("Seat:"),        DLT_STRING(SEAT_STRING[pstExistingSession->enSeat]),
                                        DLT_STRING("State:"),       DLT_STRING(SESSIONSTATE_STRING[pstExistingSession->enState]));

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
                                      DLT_STRING("Application:"), DLT_STRING(pstFailedApp->sName));
  }

  g_mutex_unlock(&NSM__pSessionMutex);
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
                                      DLT_STRING("Application:"), DLT_STRING(pstFailedApp->sName          ));
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
  NSM__tstFailedApplication stSearchApplication = {{0}}; /* Temporary application object for search */
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
                                       DLT_STRING("Owner:"     ), DLT_STRING(sAppName                                            ),
                                       DLT_STRING("State:"     ), DLT_STRING(SESSIONSTATE_STRING[boAppState]                     ));

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
static void *NSM__boOnHandleTimerWdog(void *pUserData)
{
   while(!NSM__boEndByUser && NSMWatchdogIsHappy())
   {
      (void) sd_notify(0, "WATCHDOG=1");
      DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Sent heartbeat to systemd watchdog"));
      usleep((unsigned int)NSM__WdogSec * 1000);
   }

   if(!NSM__boEndByUser)
   {
     DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Calling abort because of watchdog"));
     abort(); // Don't trust on systemd timeout. Abort immediately
   }

  return NULL;
}


/**********************************************************************************************************************
*
* The function checks if the NSM is observed by a systemd wdog and installs a timer if necessary.
*
**********************************************************************************************************************/
static void NSM__vConfigureWdogTimer(void)
{
  const gchar *sWdogSec   = NULL;

  sWdogSec = g_getenv("WATCHDOG_USEC");
  if(sWdogSec != NULL)
  {
    NSM__WdogSec = strtoul(sWdogSec, NULL, 10);

    /* The min. valid value for systemd is 1 s => WATCHDOG_USEC at least needs to contain 1.000.000 us */
    if(NSM__WdogSec >= 1000000)
    {
      /* Convert us timeout in ms and divide by two to trigger wdog every half timeout interval */
      NSM__WdogSec /= 2000;
#ifdef ENABLE_TESTS
      NSM__WdogSec = 1000;
#endif
      if(!pthread_create(&NSM__watchdog_thread, NULL, NSM__boOnHandleTimerWdog, NULL)) {
         DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Started wdog timer."            ),
                                           DLT_STRING("Interval [ms]:"), DLT_UINT64(NSM__WdogSec));
      }
      else
      {
        DLT_LOG(NsmContext, DLT_LOG_ERROR, DLT_STRING("NSM: Failed to create watchdog thread"));
      }

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
  NSM__pSessions               = NULL;
  NSM__pLifecycleClients       = NULL;
  NSM__enNodeState             = NsmNodeState_NotSet;
  NSM__pFailedApplications     = NULL;
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
* The function is called to trace a syslog message for a shutdown client.
*
* @param client:        Hash of the lifecycle client. Used for identification.
* @param u32Reason:     Shutdown reason send to the client.
* @param sInOut:        "enter" or "leave" (including failure reason)
* @param enErrorStatus: Error value
*
**********************************************************************************************************************/
static void NSM__vLtProf(size_t client, guint32 u32Reason, gchar *sInOut, NsmErrorStatus_e enErrorStatus)
{
    gchar pszLtprof[128] = "LTPROF: client:%Iu (0x%08X:%d) ";
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

    syslog(LOG_NOTICE, (char *)pszLtprof, client, u32Reason, enErrorStatus);
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
                 ? NSM__enSetNodeState((NsmNodeState_e) *pData, TRUE, FALSE, FALSE)
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
    case NsmDataType_RunningReason:
      enRetVal =   (u32DataLen == sizeof(NsmRunningReason_e))
                 ? NSMA_boSetRunningReason((NsmRunningReason_e) *pData)
                 : NsmErrorStatus_Parameter;
    break;
    case NsmDataType_RequestNodeRestart:
      enRetVal =   (u32DataLen == sizeof(NsmRestartReason_e))
                 ? NSM__enOnHandleRequestNodeRestart((NsmRestartReason_e) *pData, NSM_SHUTDOWNTYPE_FAST)
                 : NsmErrorStatus_Parameter;
    break;
    case NsmDataType_BlockExternalNodeState:
      enRetVal =   (u32DataLen == sizeof(bool))
                 ? NSM__enSetBlockExternalNodeState((bool) *pData)
                 : NsmErrorStatus_Parameter;
    break;

    /* Error: The type of the data NSMC is trying to set is unknown or the data is read only! */
    case NsmDataType_RestartReason:
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
int main(int argc, char **argv)
{
  NSMTriggerWatchdog(NsmWatchdogState_Active);

  GList                   *pListEntry        = NULL;
  NSM__tstLifecycleClient *pstExistingClient = NULL;

  /* Register NSM for DLT */
  DLT_REGISTER_APP("NSM", "Node State Manager|SysInfra|Lifecycle");

  DLT_REGISTER_CONTEXT(NsmContext,  "NSM",  "Context for NSM");
  DLT_REGISTER_CONTEXT(NsmaContext, "NSMA", "Context for NSMA");

#ifdef ENABLE_TESTS
  DLT_ENABLE_LOCAL_PRINT();
#endif

  int option_index = 0;
  getopt_long (argc, argv, "", NSM__options, &option_index);

  /* Initialize syslog */
  NSM__vSyslogOpen();

  /* Print first msg. to show that NSM is going to start */
  DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: NodeStateManager started."), DLT_STRING("Version:"), DLT_STRING(WATERMARK));

  /* Currently no other resources accessing the NSM. Prepare it now! */
  NSM__vInitializeVariables();     /* Initialize file local variables*/
  NSM__vCreatePlatformSessions();  /* Create platform sessions       */

  /* Initialize the NSMA before the NSMC, because the NSMC can access properties */
  if(NSMA_boInit(&NSM__stObjectCallBacks) == TRUE)
  {
    /* Set the properties to initial values */
    if(0 == NSM__bootloader_flag)
    {
       (void) NSMA_boSetBootMode(1);
    }
    else
    {
       DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Starting in bootloader mode"));
       (void) NSMA_boSetBootMode(2);
    }

    (void) NSMA_boSetRestartReason(NsmRestartReason_NotSet);
    (void) NSMA_boSetShutdownReason(NsmShutdownReason_NotSet);
    (void) NSMA_boSetRunningReason(NsmRunningReason_WakeupCan);

    /* Initialize/start the NSMC */
    if(NsmcInit() == 0x01)
    {
      /* Start timer to satisfy wdog */
      NSM__vConfigureWdogTimer();

      DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM has been initialized successfully"));
      /* Inform systemd that nsm has been successfully initialized */
      sd_notify(0, "READY=1");
      /* The event loop is only canceled if the Node is completely shut down or there is an internal error. */
      NSM__boEndByUser = NSMA_boWaitForEvents();

      /* If there are still clients registered -> delete them */
      for (pListEntry = NSM__pLifecycleClients; pListEntry; pListEntry = pListEntry->next)
      {
         pstExistingClient = pListEntry->data;
         pstExistingClient = (NSM__tstLifecycleClient*) pListEntry->data;

         NSM__vFreeLifecycleClientObject(pstExistingClient);
         NSM__pLifecycleClients = g_list_remove(NSM__pLifecycleClients, pstExistingClient);
      }

      NSM__collective_sequential_timeout = 0;
      NSM__max_parallel_timeout = 0;

      if(NSM__boEndByUser == TRUE)
      {
        DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Successfully canceled event loop."),
                                          DLT_STRING("Shutting down NodeStateManager."        ));
      }
      else
      {
        DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: Error in event loop."     ),
                                          DLT_STRING("Shutting down NodeStateManager."));
        NSM__boEndByUser = TRUE;
      }

      /* The event loop returned. Clean up the NSMA. */
      (void) NSMA_boDeInit();

      /* The event loop returned. Clean up the NSMC. */
      (void) NsmcDeInit();
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
  /* Remove data from all lists */
  g_slist_free_full(NSM__pSessions,           &NSM__vFreeSessionObject);
  g_slist_free_full(NSM__pFailedApplications, &NSM__vFreeFailedApplicationObject);
  g_list_free_full (NSM__pLifecycleClients,   &NSM__vFreeLifecycleClientObject);

  DLT_LOG(NsmContext, DLT_LOG_INFO, DLT_STRING("NSM: NodeStateManager stopped."));

  /* Deinit syslog */
  NSM__vSyslogClose();

  /* Unregister NSM from DLT */
  DLT_UNREGISTER_CONTEXT(NsmContext);
  DLT_UNREGISTER_CONTEXT(NsmaContext);

  DLT_UNREGISTER_APP();

#ifdef COVERAGE_ENABLED
  __gcov_flush();
#endif
  return 0;
}
