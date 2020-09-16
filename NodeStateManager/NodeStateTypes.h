#ifndef NODESTATETYPES_H
#define NODESTATETYPES_H

/**********************************************************************************************************************
*
* Copyright (C) 2012 Continental Automotive Systems, Inc.
*               2017 BMW AG
*
* Author: Jean-Pierre.Bogler@continental-corporation.com
*
* Type and constant definitions to communicate with the NSM.
*
* The file defines types and constants to be able to communicate with the NSM.
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Date       Author             Reason
* 2012.09.27 uidu5846  1.0.0.0  CSP_WZ#1194: Introduced 'NodeStateManagerTypes.h' to avoid circle includes
*                                            and encapsulate type definitions.
* 2012.10.24 uidu5846  1.0.0.1  CSP_WZ#1322: Removed "ssw_types" redefinition from header.
*                                            Since the same native types are used, no interface change.
* 2013.04.18 uidu5846  1.2.1    OvipRbt#1153 Added possibility to register sessions via the NSMC.
*
**********************************************************************************************************************/

/** \ingroup SSW_LCS */
/** \defgroup SSW_NSM_TEMPLATE Node State Manager
 *  \{
 */
/** \defgroup SSW_NSM_INTERFACE API document
 *  \{
 */

#ifdef __cplusplus
extern "C"
{
#endif

/**********************************************************************************************************************
*
*  CONSTANTS
*
**********************************************************************************************************************/

/* Defines for session handling */
#define NSM_DEFAULT_SESSION_OWNER "NodeStateManager"           /**< "Owner" of the default sessions                  */

/* Defines for internal settings like max. string lenghts */
#define NSM_MAX_SESSION_NAME_LENGTH  256                       /**< Max. number of chars a session name can have     */
#define NSM_MAX_SESSION_OWNER_LENGTH 256                       /**< Max. number of chars for name of session owner   */

/*
 * Defines for shutdown handling as bit masks. Used to register for multiple shutdown types and as parameter to
 * inform clients about the shutdown type via the LifecycleConsumer interface.
 */
#define NSM_SHUTDOWNTYPE_NOT      0x00000000U                  /**< Client not registered for any shutdown           */
#define NSM_SHUTDOWNTYPE_NORMAL   0x00000001U                  /**< Client registered for normal shutdown            */
#define NSM_SHUTDOWNTYPE_FAST     0x00000002U                  /**< Client registered for fast shutdown              */
#define NSM_SHUTDOWNTYPE_PARALLEL 0x00000004U                  /**< Client registered for parallel shutdown          */
#define NSM_SHUTDOWNTYPE_RUNUP    0x00000008U                  /**< The shutdown type "run up" can not be used for
                                                                    registration. Clients which are registered and
                                                                    have been shut down, will automatically be
                                                                    informed about the "run up", when the shut down
                                                                    is canceled.                                    */

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

/**********************************************************************************************************************
*
*  TYPE
*
**********************************************************************************************************************/

/**
 * The enumeration defines the different types of data that can be exchanged between the NodeStateManager (NSM)
 * and the NodeStateMachine (NSMC). Based on this value, the setter and getter functions of the NSM and NSMC will
 * interpret data behind the passed byte pointer.
 */
#define FOREACH_DATATYPE(DATATYPE) \
          DATATYPE(NsmDataType_AppMode)                 /**< An ApplicationMode should be set or get */   \
          DATATYPE(NsmDataType_NodeState)               /**< A NodeState should be set or get        */   \
          DATATYPE(NsmDataType_RestartReason)           /**< A RestartReason should be set or get    */   \
          DATATYPE(NsmDataType_SessionState)            /**< A SessionState should be set or get     */   \
          DATATYPE(NsmDataType_ShutdownReason)          /**< A ShutdownReason should be set or get   */   \
          DATATYPE(NsmDataType_BootMode)                /**< A BootMode should be set or get         */   \
          DATATYPE(NsmDataType_RunningReason)           /**< A RunningReason should be set or get    */   \
          DATATYPE(NsmDataType_RegisterSession)         /**< A Session should be registered          */   \
          DATATYPE(NsmDataType_UnRegisterSession)       /**< A Session should be unregistered        */   \
          DATATYPE(NsmDataType_RequestNodeRestart)      /**< A NodeRestart should be triggered       */   \
          DATATYPE(NsmDataType_BlockExternalNodeState)
typedef enum _NsmDataType_e {
      FOREACH_DATATYPE(GENERATE_ENUM)
}NsmDataType_e;

static const char *DATATYPE_STRING[] __attribute__((unused)) = {
      FOREACH_DATATYPE(GENERATE_STRING)
};

/**
 * The enumeration defines the different wake up reasons.
 */
#define FOREACH_ERRORSTATUS(ERRORSTATUS)                                                                                           \
         ERRORSTATUS(NsmErrorStatus_NotSet)               /**< Initial value when error type is not set                         */ \
         ERRORSTATUS(NsmErrorStatus_Ok)                   /**< Value when no error occurred                                     */ \
         ERRORSTATUS(NsmErrorStatus_Error)                /**< A general, non-specific error occurred                           */ \
         ERRORSTATUS(NsmErrorStatus_Dbus)                 /**< Error in D-Bus communication                                     */ \
         ERRORSTATUS(NsmErrorStatus_Internal)             /**< Internal error (memory alloc. failed, etc.)                      */ \
         ERRORSTATUS(NsmErrorStatus_Parameter)            /**< A passed parameter was incorrect                                 */ \
         ERRORSTATUS(NsmErrorStatus_WrongSession)         /**< The requested session is unknown.                                */ \
         ERRORSTATUS(NsmErrorStatus_ResponsePending)      /**< Command accepted, return value delivered asynch.                 */ \
         ERRORSTATUS(NsmErrorStatus_WrongClient)          /**< A client has returned with wrong id                              */ \
         ERRORSTATUS(NsmErrorStatus_Last)                 /**< Last error value to identify valid errors                        */ \

typedef enum _NsmErrorStatus_e {
      FOREACH_ERRORSTATUS(GENERATE_ENUM)
}NsmErrorStatus_e;

static const char *ERRORSTATUS_STRING[] __attribute__((unused)) = {
      FOREACH_ERRORSTATUS(GENERATE_STRING)
};

/**
 * Possible application modes of the node.
 */
#define FOREACH_APPLICATIONMODE(APPLICATIONMODE) \
         APPLICATIONMODE(NsmApplicationMode_NotSet)           /**< Initial state                                        */\
         APPLICATIONMODE(NsmApplicationMode_Parking)          /**< Parking          mode                                */\
         APPLICATIONMODE(NsmApplicationMode_Factory)          /**< Factory          mode                                */\
         APPLICATIONMODE(NsmApplicationMode_Transport)        /**< Transport        mode                                */\
         APPLICATIONMODE(NsmApplicationMode_Normal)           /**< Normal           mode                                */\
         APPLICATIONMODE(NsmApplicationMode_Swl)              /**< Software loading mode                                */\
         APPLICATIONMODE(NsmApplicationMode_Last)             /**< Last value to identify valid values                  */\

typedef enum _NsmApplicationMode_e {
      FOREACH_APPLICATIONMODE(GENERATE_ENUM)
}NsmApplicationMode_e;

static const char *APPLICATIONMODE_STRING[] __attribute__((unused)) = {
      FOREACH_APPLICATIONMODE(GENERATE_STRING)
};


/**
 * The enumeration defines the different restart reasons.
 */
#define FOREACH_RESTARTREASON(RESTARTREASON) \
         RESTARTREASON(NsmRestartReason_NotSet)             /**< Initial value when reset reason is not set           */\
         RESTARTREASON(NsmRestartReason_ApplicationFailure) /**< Reset was requested by System Health Mon.            */\
         RESTARTREASON(NsmRestartReason_Diagnosis)          /**< Reset was requested by diagnosis                     */\
         RESTARTREASON(NsmRestartReason_Swl)                /**< Reset was requested by the SWL application           */\
         RESTARTREASON(NsmRestartReason_User)               /**< Reset was requested by an user application           */\
         RESTARTREASON(NsmRestartReason_Last)               /**< Last value to identify valid reset reasons           */\

typedef enum _NsmRestartReason_e {
      FOREACH_RESTARTREASON(GENERATE_ENUM)
}NsmRestartReason_e;

static const char *RESTARTREASON_STRING[] __attribute__((unused)) = {
      FOREACH_RESTARTREASON(GENERATE_STRING)
};

/**
 * Session can be enabled seat depended.
 */
#define FOREACH_SEAT(SEAT) \
         SEAT(NsmSeat_NotSet)                      /**< Initial state                                        */   \
         SEAT(NsmSeat_Driver)                      /**< Driver seat                                          */   \
         SEAT(NsmSeat_CoDriver)                    /**< CoDriver seat                                        */   \
         SEAT(NsmSeat_Rear1)                       /**< Rear 1                                               */   \
         SEAT(NsmSeat_Rear2)                       /**< Rear 2                                               */   \
         SEAT(NsmSeat_Rear3)                       /**< Rear 3                                               */   \
         SEAT(NsmSeat_Last)                        /**< Last valid state                                     */   \

typedef enum _NsmSeat_e {
      FOREACH_SEAT(GENERATE_ENUM)
}NsmSeat_e;

static const char *SEAT_STRING[] __attribute__((unused)) = {
      FOREACH_SEAT(GENERATE_STRING)
};

/**
 * The enumeration defines the different wake up reasons.
 */
#define FOREACH_SESSIONSTATE(SESSIONSTATE) \
        SESSIONSTATE(NsmSessionState_Unregistered)        /**< Initial state, equals "not set"                      */ \
        SESSIONSTATE(NsmSessionState_Inactive)            /**< Session is inactive                                  */ \
        SESSIONSTATE(NsmSessionState_Active)               /**< Session is active                                    */\

typedef enum _NsmSessionState_e {
      FOREACH_SESSIONSTATE(GENERATE_ENUM)
}NsmSessionState_e;

static const char *SESSIONSTATE_STRING[] __attribute__((unused)) = {
      FOREACH_SESSIONSTATE(GENERATE_STRING)
};

/**
 * The enumeration defines the different shutdown reasons.
 */
#define FOREACH_SHUTDOWNREASON(SHUTDOWNREASON) \
        SHUTDOWNREASON(NsmShutdownReason_NotSet)            /**< Initial value when ShutdownReason not set            */ \
        SHUTDOWNREASON(NsmShutdownReason_Normal)            /**< A normal shutdown has been performed                 */ \
        SHUTDOWNREASON(NsmShutdownReason_SupplyBad)         /**< Shutdown because of bad supply                       */ \
        SHUTDOWNREASON(NsmShutdownReason_SupplyPoor)        /**< Shutdown because of poor supply                      */ \
        SHUTDOWNREASON(NsmShutdownReason_ThermalBad)        /**< Shutdown because of bad thermal state                */ \
        SHUTDOWNREASON(NsmShutdownReason_ThermalPoor)       /**< Shutdown because of poor thermal state               */ \
        SHUTDOWNREASON(NsmShutdownReason_SwlNotActive)      /**< Shutdown after software loading                      */ \
        SHUTDOWNREASON(NsmShutdownReason_Last)              /**< Last value. Identify valid ShutdownReasons           */ \

typedef enum _NsmShutdownReason_e {
      FOREACH_SHUTDOWNREASON(GENERATE_ENUM)
}NsmShutdownReason_e;

static const char *SHUTDOWNREASON_STRING[] __attribute__((unused)) = {
      FOREACH_SHUTDOWNREASON(GENERATE_STRING)
};

/**
 * The enumeration defines the different start or wake up reasons.
 */

#define FOREACH_RUNNINGREASON(RUNNINGREASON) \
        RUNNINGREASON(NsmRunningReason_NotSet)                       /**< Initial value when reason is not set.                          */\
        RUNNINGREASON(NsmRunningReason_WakeupCan)                    /**< Wake up because of CAN activity                                */\
        RUNNINGREASON(NsmRunningReason_WakeupMediaEject)             /**< Wake up because of 'Eject' button                              */\
        RUNNINGREASON(NsmRunningReason_WakeupMediaInsertion)         /**< Wake up because of media insertion                             */\
        RUNNINGREASON(NsmRunningReason_WakeupHevac)                  /**< Wake up because of user uses the HEVAC unit in the car.
                                                                          Even if the HEVAC actually causes activity on the CAN bus a
                                                                          different wakeup reason is required as it could result in a
                                                                          different level of functionality being started                 */\
        RUNNINGREASON(NsmRunningReason_WakeupPhone)                  /**< Wake up because of a phone call being received.
                                                                          Even if this is passed as a CAN event a different wakeup reason
                                                                          is required as it could result in a different level of
                                                                          functionality being started                                    */\
        RUNNINGREASON(NsmRunningReason_WakeupPowerOnButton)          /**< Startup because user presses the "Power ON" button in the car.
                                                                          Even if this is passed as a CAN event a different wakeup reason
                                                                          is required as it could result in a different level of
                                                                          functionality being started                                    */\
        RUNNINGREASON(NsmRunningReason_StartupFstp)                  /**< System was started due to a first switch to power              */\
        RUNNINGREASON(NsmRunningReason_StartupSwitchToPower)         /**< System was switched to power                                   */\
        RUNNINGREASON(NsmRunningReason_RestartSwRequest)             /**< System was restarted due to an internal SW Request
                                                                          (i.e. SWL or Diagnosis)                                        */\
        RUNNINGREASON(NsmRunningReason_RestartInternalHealth)        /**< System was restarted due to an internal health problem         */\
        RUNNINGREASON(NsmRunningReason_RestartExternalHealth)        /**< System was restarted due to an external health problem
                                                                          (i.e. external wdog believed node was in failure)              */\
        RUNNINGREASON(NsmRunningReason_RestartUnexpected)            /**< System was restarted due to an unexpected kernel restart.
                                                                          This will be the default catch when no other reason is known   */\
        RUNNINGREASON(NsmRunningReason_RestartUser)                  /**< Target was reset due to user action (i.e user 3 finger press)  */\
        RUNNINGREASON(NsmRunningReason_PlatformEnd)\

typedef enum _NsmRunningReason_e {
      FOREACH_RUNNINGREASON(GENERATE_ENUM)
}NsmRunningReason_e;

static const char *RUNNINGREASON_STRING[] __attribute__((unused)) = {
      FOREACH_RUNNINGREASON(GENERATE_STRING)
};


/**
 * The enumeration defines the different node states
 */

#define FOREACH_NODESTATE(NODESTATE) \
        NODESTATE(NsmNodeState_NotSet)   \
        NODESTATE(NsmNodeState_StartUp)   \
        NODESTATE(NsmNodeState_BaseRunning)   \
        NODESTATE(NsmNodeState_LucRunning)   \
        NODESTATE(NsmNodeState_FullyRunning)   \
        NODESTATE(NsmNodeState_FullyOperational)   \
        NODESTATE(NsmNodeState_ShuttingDown)   \
        NODESTATE(NsmNodeState_ShutdownDelay)   \
        NODESTATE(NsmNodeState_FastShutdown)   \
        NODESTATE(NsmNodeState_DegradedPower)   \
        NODESTATE(NsmNodeState_Shutdown)   \
        NODESTATE(NsmNodeState_Resume)   \
        NODESTATE(NsmNodeState_Last)   \

typedef enum _NsmNodeState_e {
   FOREACH_NODESTATE(GENERATE_ENUM)
}NsmNodeState_e;

static const char *NODESTATE_STRING[] __attribute__((unused)) = {
      FOREACH_NODESTATE(GENERATE_STRING)
};

/** The type defines the structure for a session.                                                */
typedef struct _NsmSession_s
{
  char               sName[NSM_MAX_SESSION_NAME_LENGTH];   /**< Name  of the session             */
  char               sOwner[NSM_MAX_SESSION_OWNER_LENGTH]; /**< Owner of the session             */
  NsmSeat_e          enSeat;                               /**< Seat  of the session             */
  NsmSessionState_e  enState;                              /**< State of the session             */
} NsmSession_s, *pNsmSession_s;

#ifdef __cplusplus
}
#endif
/** \} */ /* End of SSW_NSM_INTERFACE */
/** \} */ /* End of SSW_NSM_TEMPLATE  */
#endif /* NODESTATETYPES_H */
