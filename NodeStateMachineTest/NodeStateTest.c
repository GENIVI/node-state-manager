/**********************************************************************************************************************
*
* Copyright (C) 2012 Continental Automotive Systems, Inc.
*
* Author: Jean-Pierre.Bogler@continental-corporation.com
*
* Implementation of the NodeStateMachineTest.
*
* The file implements a test executable that communicates with the NodeStateManager and provides "LifecycleClients"
* for shutdown tests. The intention is to keep the test code as simple as possible. Additional test cases should
* only be defined by adding them to the table "NSMTST__astTestCase". If the NSM offers new functionality, it might
* be necessary to extend the test frame by new functions.
*
* The test is controlled by a timer. In configurable time steps, the test cases are called one after the other.
* The table NSMTST__astTestCase contains a function pointer to the test function, a union for possible parameters
* and a union for the expected return value(s).
*
* The test functions return a boolean value and do not take parameters. The parameters to test the NSM and the
* expected return values are read from the table "NSMTST__astTestCase".
*
* For every NSM interface the possible parameters are stored in a structure. The structure is added as a member
* in the "tunParameter" union type definition. The same applies to the expected return values, where the structures
* are declares as members in the tunReturnValues union.
*
* Wrapping the parameters and expected returns in structures should simplify further extensions and adaption to the
* test frame and allow a consistent syntax. Every structure name for parameters and return values has the same naming
* convenitons:
*
* NSMTST__tst<Interface><Function>Param or NSMTST__tst<Interface><Function>Return
*
* Where interface either is Db (D-Bus), Sm (StateMachine) or Test (int. functions, where no NSM interface is accessed).
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Date             Author              Reason
* 24.01.2013       Jean-Pierre Bogler  CSP_WZ#1194:  Initial creation.
* 18.04.2013       Jean-Pierre Bogler  OvipRbt#1153: Implemented test cases 164 - 170.
*
**********************************************************************************************************************/


/**********************************************************************************************************************
*
* Header includes
*
**********************************************************************************************************************/

/* System header files                                                                          */
#include <gio/gio.h>                    /* Use glib to access dbus and communicate to NSM       */

/* Header files offered by NSM                                                                  */
#include "NodeStateTypes.h"             /* Know type definitions of NSM                         */
#include "NodeStateManager.h"           /* Know internal interfaces of NSM                      */

/* Generated header files to access NSM via D-Bus                                               */
#include "NodeStateConsumer.h"          /* Consumer interface with publicly available functions */
#include "NodeStateLifecycleControl.h"  /* Control  interface with safety relevant functions    */
#include "NodeStateLifecycleConsumer.h" /* Consumer interface to offer life cycle clients       */
#include "NodeStateMachineTestApi.h"    /* Access D-Bus interface of test NSMC                  */


/**********************************************************************************************************************
*
* Local defines, macros and type definitions.
*
**********************************************************************************************************************/

/*
 * Interval between the test steps in ms.
 */
#define NSMTST__TIMER_INTERVAL 100

/*
 * Format for the trace output of the test frame.
 * An integer (the test case number) and three strings can be displayed:
 * The test description, the ErrorMessage and a "success"/"failed" string.
 *
 */
#define NSMTST__TESTPRINT  "%03d;%s;%s;%s\n"

/*
 * The define sets up a string that is longer for all text fields used in the NSM.
 * The intend is to test the NSM for correct behavior by passing this string.
 */
#define NSMTST__260CHAR_STRING "012345678901234567890123456789012345678901234567890123456789"\
                               "012345678901234567890123456789012345678901234567890123456789"\
                               "012345678901234567890123456789012345678901234567890123456789"\
                               "012345678901234567890123456789012345678901234567890123456789"\
                               "01234567890123456789"

/* Defines the syntax of a test function call */
typedef gboolean (*NSMTST__tpfTestFunction)(void);

/* Configures dummy parameter for test cases that do not need additional parameters. */
typedef struct
{
  guint8 u8Dummy;
} NSMTST__tstTestDummyParam;

/*
 * Configures parameters for the helper function "NSMTST__boCreateLifecycleClient",
 * which is used to create life cycle clients that are needed for further tests.
 */
typedef struct
{
  gchar* sObjName; /* Object name of the LifecycleClient that should be created */
} NSMTST__tstTestCreateLifecycleClientParam;

/* Configures parameters for calling the (internal) NsmSetData interface of the NSM with invalid data types. */
typedef struct
{
  guint         u32DataLen; /* Length of the data to be set in byte */
  NsmDataType_e enDataType; /* Data type passed to NSM              */
} NSMTST__tstSmSetInvalidDataParam;

/* Configures parameters for getting a SessionState from the NSM by calling its (internal) NsmSetData interface. */
typedef struct
{
  guint        u32DataLen; /* Length of the data to be set in byte */
  NsmSession_s sSession;   /* Defines which session to get         */
} NSMTST__tstSmGetSessionStateParam;

/* Configures parameters for setting an ApplicationMode using the (internal) NsmSetData interface. */
typedef struct
{
  guint                u32DataLen;        /* Length of the data to be set in byte */
  NsmApplicationMode_e enApplicationMode; /* ApplicationMode to be set            */
} NSMTST__tstSmSetApplicationModeParam;

/* Configures parameters for setting a NodeState using the (internal) NsmSetData interface. */
typedef struct
{
  guint          u32DataLen;  /* Length of the data to be set in byte */
  NsmNodeState_e enNodeState; /* NodeState to be set                  */
} NSMTST__tstSmSetNodeStateParam;

/* Configure parameters for setting a SessionState using the (internal) NsmSetData interface. */
typedef struct
{
  guint        u32DataLen;  /* Length of the data to be set in byte         */
  NsmSession_s stSession;   /* Defines session and state that should be set */
} NSMTST__tstSmSetSessionStateParam,
  NSMTST__tstSmRegisterSessionParam,
  NSMTST__tstSmUnRegisterSessionParam;

/* Configure the parameters for setting a ShutdownReason using the (internal) NsmSetData interface. */
typedef struct
{
  guint               u32DataLen;       /* Length of the data to be set in byte */
  NsmShutdownReason_e enShutdownReason; /* ShutdownReason to be set             */
} NSMTST__tstSmSetShutdownReasonParam;

/* Configure parameters for setting a BootMode using the (internal) NsmSetData interface. */
typedef struct
{
  guint u32DataLen;  /* Length of the data to be set in byte */
  gint  i32BootMode; /* BootMode to be set                   */
} NSMTST__tstSmSetBootModeParam;

/* Configure the parameters for getting different values of the NSM via the (internal) NsmGetData interface */
typedef struct
{
  guint u32DataLen;  /* Length of the data to be set in byte */
} NSMTST__tstSmGetRunningReasonParam,
  NSMTST__tstSmGetShutdownReasonParam,
  NSMTST__tstSmGetNodeStateParam,
  NSMTST__tstSmGetApplicationModeParam,
  NSMTST__tstSmGetBootModeParam,
  NSMTST__tstSmGetRestartReasonParam;

/* Configures parameters for trying to get invalid data using the (internal) NsmSetData interface. */
typedef struct
{
  guint         u32DataLen; /* Length of the data to be set in byte */
  NsmDataType_e enDataType; /* DataType, which is trying to be get  */
} NSMTST__tstSmGetInvalidDataParam;

/* Configures parameters for calling the RegisterLifecycleClient D-Bus interface of the NSM.
 * Note that the bus address automatically is defined by the test frame.
 */
typedef struct
{
  gchar *sObjName;   /* Object name                 */
  guint  u32Mode;    /* Registered shutdown mode(s) */
  guint  u32Timeout; /* Timeout for shutdown        */
} NSMTST__tstDbRegisterShutdownClientParam;

/* Configures parameters for calling the UnRegisterLifecycleClient D-Bus interface of the NSM. */
typedef struct
{
  gchar *sObjName; /* Object name                                  */
  guint  u32Mode;  /* Shutdown mode(s) taht should be unregistered */
} NSMTST__tstDbUnRegisterShutdownClientParam;

/* Configures parameters for calling the RequestNodeRestart D-Bus interface of the NSM. */
typedef struct
{
  NsmRestartReason_e enRestartReason; /* RestartReason passed to NSM           */
  guint              u32RestartType;  /* RestartType. E.g. NSM_NORMAL_SHUTDOWN */
} NSMTST__tstDbRequestNodeRestartParam;

/* Configures parameters for calling the SetAppHealthStatus D-Bus interface of the NSM. */
typedef struct
{
  gchar    *sAppName;     /* (Systemd) name of the application, which state changed */
  gboolean  boAppRunning; /* New "RunningState" of the application                  */
} NSMTST__tstDbSetAppHealthStatusParam;

/* Configures parameters for calling the SetBootMode D-Bus interface of the NSM. */
typedef struct
{
  gint i32BootMode; /* BootMode to be set */
} NSMTST__tstDbSetBootModeParam;

/* Configures parameters for setting the NodeState using the D-Bus SetNodeState interface of the NSM. */
typedef struct
{
  NsmNodeState_e enNodeState; /* NodeState to be set */
} NSMTST__tstDbSetNodeStateParam;

/* Configures parameters for setting the ApplicationMode using the D-Bus SetApplicationMode interface of the NSM. */
typedef struct
{
  NsmApplicationMode_e enApplicationMode; /* ApplicationMode to be set */
} NSMTST__tstDbSetApplicationModeParam;

/*
 * Configures parameters for registering sessions and setting session states using the D-Bus
 * RegisterSession and SetSessionState interface of the NSM.
 */
typedef struct
{
  gchar             *sSessionName;  /* Name of the session  */
  gchar             *sSessionOwner; /* Owner of the session */
  NsmSeat_e          enSeat;        /* Seat of the session  */
  NsmSessionState_e  enState;       /* State of the session */
} NSMTST__tstDbRegisterSessionParam,
  NSMTST__tstDbSetSessionStateParam;

/* Configures parameters for getting session states using the D-Bus GetSessionState interface of the NSM. */
typedef struct
{
  gchar     *sSessionName; /* Name of the session */
  NsmSeat_e  enSeat;       /* Seat of the session */
} NSMTST__tstDbGetSessionStateParam;

/* Configures parameters for getting session states using the D-Bus GetSessionState interface of the NSM. */
typedef struct
{
  gchar     *sSessionName;  /* Name of the session  */
  gchar     *sSessionOwner; /* Owner of the session */
  NsmSeat_e  enSeat;        /* Seat of the session  */
} NSMTST__tstDbUnRegisterSessionParam;

/*
 * Configures the parameters for calling the LifecyclRequestComplete D-Bus interface of the NSM to finalize
 * an asynchronous shut down. Furthermore, the same value is passed in ProcessLifecycleRequest to process an
 * initial request.
 */
typedef struct
{
  NsmErrorStatus_e enErrorStatus; /* Error status returned to NSM */
} NSMTST__tstDbLifecycleRequestCompleteParam,
  NSMTST__tstTestProcessLifecycleRequestParam;

/* The union includes all possible parameters needed by different NSM interfaces. */
typedef union
{
  /* Parameters for internal functions that control the test */
  NSMTST__tstTestDummyParam                   stTestDummy;
  NSMTST__tstTestCreateLifecycleClientParam   stTestCreateLcClient;

  /* Parameters to control callback functions, which occur because of NSM signals */
  NSMTST__tstTestProcessLifecycleRequestParam stTestProcessLifecycleRequest;

  /* Parameters for D-Bus interfaces of the NSM */
  NSMTST__tstDbSetNodeStateParam              stDbSetNodeState;
  NSMTST__tstDbSetApplicationModeParam        stDbSetApplicationMode;
  NSMTST__tstDbSetBootModeParam               stDbSetBootMode;
  NSMTST__tstDbGetSessionStateParam           stDbGetSessionState;
  NSMTST__tstDbSetSessionStateParam           stDbSetSessionState;

  NSMTST__tstDbRegisterSessionParam           stDbRegisterSession;
  NSMTST__tstDbUnRegisterSessionParam         stDbUnRegisterSession;
  NSMTST__tstDbSetAppHealthStatusParam        stDbSetAppHealthStatus;
  NSMTST__tstDbRegisterShutdownClientParam    stDbRegisterShutdownClient;
  NSMTST__tstDbUnRegisterShutdownClientParam  stDbUnRegisterShutdownClient;
  NSMTST__tstDbRequestNodeRestartParam        stDbRequestNodeRestart;

  NSMTST__tstDbLifecycleRequestCompleteParam  stDbLifecycleRequestComplete;

  /* Parameters for D-Bus interfaces of the test NSMC */
  NSMTST__tstSmSetInvalidDataParam            stSmSetInvalidData;
  NSMTST__tstSmSetApplicationModeParam        stSmSetApplicationMode;
  NSMTST__tstSmSetNodeStateParam              stSmSetNodeState;
  NSMTST__tstSmSetSessionStateParam           stSmSetSessionState;
  NSMTST__tstSmRegisterSessionParam           stSmRegisterSession;
  NSMTST__tstSmUnRegisterSessionParam         stSmUnRegisterSession;
  NSMTST__tstSmSetShutdownReasonParam         stSmSetShutdownReason;
  NSMTST__tstSmSetBootModeParam               stSmSetBootMode;

  NSMTST__tstSmGetBootModeParam               stSmGetBootMode;
  NSMTST__tstSmGetRestartReasonParam          stSmGetRestartReason;
  NSMTST__tstSmGetRunningReasonParam          stSmGetRunningReason;
  NSMTST__tstSmGetShutdownReasonParam         stSmGetShutdownReason;
  NSMTST__tstSmGetNodeStateParam              stSmGetNodeState;
  NSMTST__tstSmGetApplicationModeParam        stSmGetApplicationMode;
  NSMTST__tstSmGetInvalidDataParam            stSmGetInvalidData;
  NSMTST__tstSmGetSessionStateParam           stSmGetSessionState;
} NSMTST__tunParameters;


/* Configures dummy return value for test cases where no return value is expected. */
typedef struct
{
  guint8 u8Dummy;
} NSMTST__tstTestDummyReturn;

/* Configures expected return values when calling the (internal) NsmGetData interface with invalid values. */
typedef struct
{
  gint i32WrittenBytes; /* Number of bytes returned by NSM */
} NSMTST__tstSmGetInvalidDataReturn;

/* Configure expected return values when getting the ApplicationMode via the internal NsmGetData interface. */
typedef struct
{
  gint                 i32WrittenBytes;   /* Number of bytes returned by NSM */
  NsmApplicationMode_e enApplicationMode; /* ApplicationMode returned by NSM */
} NSMTST__tstSmGetAppModeReturn;

/* Configures expected return values when getting the NodeState via the internal NsmGetData interface. */
typedef struct
{
  gint           i32WrittenBytes; /* Number of bytes returned by NSM */
  NsmNodeState_e enNodeState;     /* NodeState returned by NSM       */
} NSMTST__tstSmGetNodeStateReturn;

/* Configures expected return values when getting the RestartReason via the internal NsmGetData interface. */
typedef struct
{
  gint               i32WrittenBytes; /* Number of bytes returned by NSM */
  NsmRestartReason_e enRestartReason; /* RestartReason returned by NSM   */
} NSMTST__tstSmGetRestartReasonReturn;

/* Configures expected return values when getting a SessionState via the internal NsmGetData interface. */
typedef struct
{
  gint              i32WrittenBytes; /* Number of bytes returned by NSM */
  NsmSessionState_e enSessionState;  /* SessionState returned by NSM    */
} NSMTST__tstSmGetSessionStateReturn;

/* Configures expected return values when getting a ShutdownReason via the internal NsmGetData interface. */
typedef struct
{
  gint                i32WrittenBytes;  /* Number of bytes returned by NSM */
  NsmShutdownReason_e enShutdownReason; /* ShutdownReason returned by NSM  */
} NSMTST__tstSmGetShutdownReasonReturn;

/* Configures expected return values when getting a BootMode via the internal NsmGetData interface. */
typedef struct
{
  gint i32WrittenBytes; /* Number of bytes returned by NSM */
  gint i32BootMode;     /* BootMode returned by NSM        */
} NSMTST__tstSmGetBootModeReturn;

/* Configures expected return values when getting a RunningReason via the internal NsmGetData interface. */
typedef struct
{
  gint               i32WrittenBytes; /* Number of bytes returned by NSM */
  NsmRunningReason_e enRunningReason; /* RunningReason returned by NSM   */
} NSMTST__tstSmGetRunningReasonReturn;

/* Configures expected return values when getting a SessionState via the GetSessionState D-Bus interface of the NSM. */
typedef struct
{
  NsmErrorStatus_e  enErrorStatus;  /* ErrorStatus returned by NSM  */
  NsmSessionState_e enSessionState; /* SessionState returned by NSM */
} NSMTST__tstDbGetSessionStateReturn;

/* Configures expected return values when calling the CheckLucRequired D-Bus interface of the NSM. */
typedef struct
{
  gboolean boLucRequired; /* LUC flag returned by NSM */
} NSMTST__tstDbCheckLucRequired;

/* Configures expected return values when calling the GetBootMode D-Bus interface of the NSM. */
typedef struct
{
  gint i32BootMode; /* BootMdoe returned by NSM */
} NSMTST__tstDbGetBootModeReturn;

/* Configures expected return values when calling the GetRunningReason D-Bus interface of the NSM. */
typedef struct
{
  NsmRunningReason_e enRunningReason; /* RunningReason returned by NSM */
} NSMTST__tstDbGetRunningReasonReturn;

/* Configures the expected return values when calling the GetShutdownReason D-Bus interface of the NSM. */
typedef struct
{
  NsmShutdownReason_e enShutdownReason; /* ShutdownReason returned by NSM */
} NSMTST__tstDbGetShutdownReasonReturn;

/* Configures expected return values when calling the GetRestartReason D-Bus interface of the NSM. */
typedef struct
{
  NsmRestartReason_e enRestartReason; /* RetsartReason returned by NSM */
} NSMTST__tstDbGetRestartReasonReturn;

/* Configures expected return values when calling the GetNodeState D-Bus interface of the NSM. */
typedef struct
{
  NsmErrorStatus_e enErrorStatus; /* ErrorStatus returned by NSM */
  NsmNodeState_e   enNodeState;   /* NodeState returned by NSM   */
} NSMTST__tstDbGetNodeStateReturn;

/* Configures expected return values when calling the GetApplicationMode D-Bus interface of the NSM. */
typedef struct
{
  NsmErrorStatus_e     enErrorStatus;     /* ErrorStatus returned by NSM     */
  NsmApplicationMode_e enApplicationMode; /* ApplicationMode returned by NSM */
} NSMTST__tstDbGetApplicationModeReturn;

/*
 * Configures expected return values when getting the interface version using the internal
 * NsmGetInterfaceVersion interface and the GetInterfaceVersion D-Bus interface of the NSM.
 */
typedef struct
{
  guint u32InterfaceVersion;
} NSMTST__tstSmGetInterfaceVersionReturn,
  NSMTST__tstDbGetInterfaceVersionReturn;

/* Configures expected return values when calling the GetAppHealthCount D-Bus interface of the NSM. */
typedef struct
{
  guint u32AppHealthCount; /* Number fo failed applications returned by NSM */
} NSMTST__tstDbGetAppHealthCountReturn;

/*  Configures expected return value when calling different interface of the NSM. */
typedef struct
{
  NsmErrorStatus_e enErrorStatus; /* ErrorStatus returned by NSM */
} NSMTST__tstDbSetBootModeReturn,
  NSMTST__tstDbSetNodeStateReturn,
  NSMTST__tstDbSetApplicationReturn,
  NSMTST__tstDbSetSessionStateReturn,
  NSMTST__tstDbSetAppHealthStatusReturn,
  NSMTST__tstDbRequestNodeRestartReturn,
  NSMTST__tstDbRegisterShutdownClientReturn,
  NSMTST__tstDbUnRegisterShutdownClientReturn,
  NSMTST__tstDbRegisterSessionReturn,
  NSMTST__tstDbUnRegisterSessionReturn,
  NSMTST__tstSmSetBootModeReturn,
  NSMTST__tstSmSetNodeStateReturn,
  NSMTST__tstSmSetApplicationModeReturn,
  NSMTST__tstSmSetInvalidDataReturn,
  NSMTST__tstSmSetShutdownModeReturn,
  NSMTST__tstSmSetSessionStateReturn,
  NSMTST__tstSmRegisterSessionReturn,
  NSMTST__tstSmUnRegisterSessionReturn,
  NSMTST__tstTestLifecycleRequestCompleteReturn;

/* Configures the expected values for the reception of the SessionState signal send by the NSM. */
typedef struct
{
  gboolean           boReceived; /* Flag if Session signal is expected               */
  gchar             *sName;      /* Name of the session for which signal is expected */
  NsmSeat_e          enSeat;     /* Seat of the session for which signal is expected */
  NsmSessionState_e  enState;    /* Expected session state                           */
} NSMTST__tstCheckSessionSignal;

/* Configures the expected values for the reception of the NodeState signal send by the NSM. */
typedef struct
{
  gboolean       boReceived;  /* Flag if NodeState signal is expected */
  NsmNodeState_e enNodeState; /* NodeState that is expected           */
} NSMTST__tstCheckNodeStateSignal;

/* Configures the expected values for the reception of the ApplicationMode signal send by the NSM. */
typedef struct
{
  gboolean             boReceived;        /* Flag if ApplicationMode signal is expected */
  NsmApplicationMode_e enApplicationMode; /* ApplicationMode that is expected           */
} NSMTST__tstCheckApplicationMode;

/* Configures the expected value that the NSM has passed when issuing a LifecyclRequest. */
typedef struct
{
  guint u32RequestType; /* Expected RestartType (set by NSM) for life cycle client */
} NSMTST__tstTestProcessLifecycleRequestReturn;

/* The union includes all possible expected return values for the interfaces of the NSM. */
typedef union
{
  /* Expected return values for internal functions that control the test */
  NSMTST__tstTestDummyReturn                    stTestDummy;
  NSMTST__tstTestProcessLifecycleRequestReturn  stTestProcessLifecycleRequest;

  /* Expected return values for D-Bus interfaces of the NSM */
  NSMTST__tstDbSetBootModeReturn                stDbSetBootMode;
  NSMTST__tstDbSetNodeStateReturn               stDbSetNodeState;
  NSMTST__tstDbSetApplicationReturn             stDbSetApplicationMode;
  NSMTST__tstDbSetSessionStateReturn            stDbSetSessionState;

  NSMTST__tstDbGetNodeStateReturn               stDbGetNodeState;
  NSMTST__tstDbGetApplicationModeReturn         stDbGetApplicationMode;
  NSMTST__tstDbGetBootModeReturn                stDbGetBootMode;
  NSMTST__tstDbGetShutdownReasonReturn          stDbGetShutdownReason;
  NSMTST__tstDbGetRunningReasonReturn           stDbGetRunningReason;
  NSMTST__tstDbGetRestartReasonReturn           stDbGetRestartReason;
  NSMTST__tstDbGetSessionStateReturn            stDbGetSessionState;

  NSMTST__tstDbSetAppHealthStatusReturn         stDbSetAppHealthStatus;
  NSMTST__tstDbGetAppHealthCountReturn          stDbGetAppHealthCount;
  NSMTST__tstDbCheckLucRequired                 stDbCheckLucRequired;
  NSMTST__tstDbRegisterSessionReturn            stDbRegisterSession;
  NSMTST__tstDbUnRegisterSessionReturn          stDbUnRegisterSession;
  NSMTST__tstDbRegisterShutdownClientReturn     stDbRegisterShutdownClient;
  NSMTST__tstDbUnRegisterShutdownClientReturn   stDbUnRegisterShutdownClient;
  NSMTST__tstDbRequestNodeRestartReturn         stDbRequestNodeRestart;
  NSMTST__tstDbGetInterfaceVersionReturn        stDbGetInterfaceVersion;
  NSMTST__tstTestLifecycleRequestCompleteReturn stDbLifecycleRequestComplete;

  /* Expected return values for NSMC interfaces of the NSM */
  NSMTST__tstSmSetShutdownModeReturn            stSmSetShutdownReason;
  NSMTST__tstSmSetBootModeReturn                stSmSetBootMode;
  NSMTST__tstSmSetNodeStateReturn               stSmSetNodeState;
  NSMTST__tstSmSetApplicationModeReturn         stSmSetApplicationMode;
  NSMTST__tstSmSetInvalidDataReturn             stSmSetInvalidData;
  NSMTST__tstSmSetSessionStateReturn            stSmSetSessionState;
  NSMTST__tstSmRegisterSessionReturn            stSmRegisterSession;
  NSMTST__tstSmUnRegisterSessionReturn          stSmUnRegisterSession;

  NSMTST__tstSmGetAppModeReturn                 stSmGetApplicationMode;
  NSMTST__tstSmGetNodeStateReturn               stSmGetNodeState;
  NSMTST__tstSmGetRestartReasonReturn           stSmGetRestartReason;
  NSMTST__tstSmGetSessionStateReturn            stSmGetSessionState;
  NSMTST__tstSmGetShutdownReasonReturn          stSmGetShutdownReason;
  NSMTST__tstSmGetBootModeReturn                stSmGetBootMode;
  NSMTST__tstSmGetRunningReasonReturn           stSmGetRunningReason;
  NSMTST__tstSmGetInvalidDataReturn             stSmGetInvalidData;

  NSMTST__tstSmGetInterfaceVersionReturn        stSmGetInterfaceVersion;

  /* Expected signals send by NSM */
  NSMTST__tstCheckSessionSignal                 stCheckSessionSignal;
  NSMTST__tstCheckNodeStateSignal               stCheckNodeStateSignal;
  NSMTST__tstCheckApplicationMode               stCheckApplicationModeSignal;
} NSMTST__tunReturnValues;

/* Type definition for a test call. */
typedef struct
{
  NSMTST__tpfTestFunction pfTestCall;     /* Function               */
  NSMTST__tunParameters   unParameter;    /* Parameters             */
  NSMTST__tunReturnValues unReturnValues; /* Expected return values */
} NSMTST__tstTestCase;


/**********************************************************************************************************************
*
* Prototypes for file local functions (see implementation for description)
*
**********************************************************************************************************************/

/* Internal test functions. get connections, create and register objects */
static gboolean NSMTST__boTestGetBusConnection           (void);
static gboolean NSMTST__boTestCreateConsumerProxy        (void);
static gboolean NSMTST__boTestCreateLifecycleControlProxy(void);
static gboolean NSMTST__boTestCreateNodeStateMachineProxy(void);
static gboolean NSMTST__boTestRegisterCallbacks          (void);
static gboolean NSMTST__boTestCreateLcClient             (void);
static gboolean NSMTST__boTestProcessLifecycleRequest    (void);

/* Functions to call D-Bus interfaces of the NSM */
static gboolean NSMTST__boDbSetBootMode                  (void);
static gboolean NSMTST__boDbSetApplicationMode           (void);
static gboolean NSMTST__boDbSetNodeState                 (void);
static gboolean NSMTST__boDbSetSessionState              (void);

static gboolean NSMTST__boDbGetBootMode                  (void);
static gboolean NSMTST__boDbGetApplicationMode           (void);
static gboolean NSMTST__boDbGetNodeState                 (void);
static gboolean NSMTST__boDbGetSessionState              (void);
static gboolean NSMTST__boDbGetRestartReason             (void);
static gboolean NSMTST__boDbGetShutdownReason            (void);
static gboolean NSMTST__boDbGetRunningReason             (void);

static gboolean NSMTST__boDbCheckLucRequired             (void);
static gboolean NSMTST__boDbRegisterSession              (void);
static gboolean NSMTST__boDbUnRegisterSession            (void);
static gboolean NSMTST__boDbRegisterShutdownClient       (void);
static gboolean NSMTST__boDbUnRegisterShutdownClient     (void);
static gboolean NSMTST__boDbGetAppHealthCount            (void);
static gboolean NSMTST__boDbGetInterfaceVersion          (void);
static gboolean NSMTST__boDbRequestNodeRestart           (void);
static gboolean NSMTST__boDbSetAppHealthStatus           (void);
static gboolean NSMTST__boDbLifecycleRequestComplete     (void);

/* Functions to call (internal) NSMC interfaces of the NSM  */
static gboolean NSMTST__boSmSetApplicationMode           (void);
static gboolean NSMTST__boSmSetNodeState                 (void);
static gboolean NSMTST__boSmSetSessionState              (void);
static gboolean NSMTST__boSmRegisterSession              (void);
static gboolean NSMTST__boSmUnRegisterSession            (void);
static gboolean NSMTST__boSmSetShutdownReason            (void);
static gboolean NSMTST__boSmSetBootMode                  (void);
static gboolean NSMTST__boSmSetInvalidData               (void);

static gboolean NSMTST__boSmGetApplicationMode           (void);
static gboolean NSMTST__boSmGetNodeState                 (void);
static gboolean NSMTST__boSmGetRestartReason             (void);
static gboolean NSMTST__boSmGetSessionState              (void);
static gboolean NSMTST__boSmGetShutdownReason            (void);
static gboolean NSMTST__boSmGetBootMode                  (void);
static gboolean NSMTST__boSmGetRunningReason             (void);
static gboolean NSMTST__boSmGetInvalidData               (void);

static gboolean NSMTST__boSmGetInterfaceVersion          (void);

/* Functions to check for signals */
static gboolean NSMTST__boCheckSessionSignal             (void);
static gboolean NSMTST__boCheckNodeStateSignal           (void);
static gboolean NSMTST__boCheckApplicationModeSignal     (void);

/* Internal HelperFunctions */
static GVariant* NSMTST__pPrepareStateMachineData(guchar *pDataArray, const guint32 u32ArraySize);

/* Internal callback functions to process signals */
static gboolean NSMTST__boOnSessionSignal(NodeStateConsumer *pObject,
                                          const gchar       *sSessionName,
                                          const gint         i32SeatId,
                                          const gint         i32SessionState,
                                          gpointer           pUserData);

static gboolean NSMTST__boOnApplicationModeSignal(NodeStateConsumer *pObject,
                                                  const gint         i32ApplicationMode,
                                                  gpointer           pUserData);

static gboolean NSMTST__boOnNodeStateSignal(NodeStateConsumer *pObject,
                                            const gint         i32NodeState,
                                            gpointer           pUserData);

static gboolean NSMTST__boOnLifecycleClientCb(NodeStateLifeCycleConsumer *pConsumer,
                                              GDBusMethodInvocation      *pInvocation,
                                              const guint32               u32LifeCycleRequest,
                                              const guint32               u32RequestId,
                                              gpointer                    pUserData);

/**********************************************************************************************************************
*
* Local variables and constants
*
**********************************************************************************************************************/

/* Basic variables for test frame */
static GMainLoop                       *NSMTST__pMainLoop                 = NULL;
static GDBusConnection                 *NSMTST__pConnection               = NULL;

static guint16                          NSMTST__u16TestIdx                = 0;
static NSMTST__tstTestCase             *NSMTST__pstTestCase               = NULL;
static gchar                           *NSMTST__sErrorDescription         = NULL;
static gchar                           *NSMTST__sTestDescription          = NULL;
static const gchar                     *NSMTST__sBusName                  = NULL;

/* Created proxy objects */
static NodeStateConsumer               *NSMTST__pNodeStateConsumer        = NULL;
static NodeStateLifecycleControl       *NSMTST__pLifecycleControl         = NULL;
static NodeStateTest                   *NSMTST__pNodeStateMachine         = NULL;

/* Store values from signals and LifecycleRequests */
static NSMTST__tstCheckSessionSignal    NSMTST__stReceivedSessionSignal   = {0};
static NSMTST__tstCheckNodeStateSignal  NSMTST__stReceivedNodeStateSignal = {0};
static NSMTST__tstCheckApplicationMode  NSMTST__stApplicationModeSignal   = {0};

static NodeStateLifeCycleConsumer      *NSMTST__pLifecycleConsumer        = NULL;
static GDBusMethodInvocation           *NSMTST__pLifecycleInvocation      = NULL;
static guint32                          NSMTST__u32LifecycleRequest       = 0;
static guint32                          NSMTST__u32LifecycleRequestId     = 0;

/* List to handle created LifecycleConsumers */
static GSList                          *NSMTST__pLifecycleClients         = NULL;

/*
 * This table is the most important part of the test frame. All test cases are defined here.
 * The test frame will perform one test case after the other, triggered by a timer.
 *
 * To define a test frame, the test function, the NSM parameters and the expected return values have to be configured.
 */
static NSMTST__tstTestCase NSMTST__astTestCases[] =
{
  { &NSMTST__boTestGetBusConnection,            .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stTestDummy                   = {0x00}                                                       },
  { &NSMTST__boTestCreateConsumerProxy,         .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stTestDummy                   = {0x00}                                                       },
  { &NSMTST__boTestCreateLifecycleControlProxy, .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stTestDummy                   = {0x00}                                                       },
  { &NSMTST__boTestCreateNodeStateMachineProxy, .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stTestDummy                   = {0x00}                                                       },
  { &NSMTST__boTestRegisterCallbacks,           .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stTestDummy                   = {0x00}                                                       },
  { &NSMTST__boDbSetBootMode,                   .unParameter.stDbSetBootMode               = {0x00},                                                                                                 .unReturnValues.stDbSetBootMode               = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbGetBootMode,                   .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stDbGetBootMode               = {0x00}                                                       },
  { &NSMTST__boDbSetBootMode,                   .unParameter.stDbSetBootMode               = {0x01},                                                                                                 .unReturnValues.stDbSetBootMode               = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbGetBootMode,                   .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stDbGetBootMode               = {0x01}                                                       },
  { &NSMTST__boDbSetBootMode,                   .unParameter.stDbSetBootMode               = {0x01},                                                                                                 .unReturnValues.stDbSetBootMode               = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbGetBootMode,                   .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stDbGetBootMode               = {0x01}                                                       },
  { &NSMTST__boSmSetBootMode,                   .unParameter.stSmSetBootMode               = {sizeof(gint),     0x02},                                                                               .unReturnValues.stSmSetBootMode               = {NsmErrorStatus_Ok       }                                   },
  { &NSMTST__boSmSetBootMode,                   .unParameter.stSmSetBootMode               = {sizeof(gint) + 1, 0x03},                                                                               .unReturnValues.stSmSetBootMode               = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boSmSetBootMode,                   .unParameter.stSmSetBootMode               = {sizeof(gint) - 1, 0x04},                                                                               .unReturnValues.stSmSetBootMode               = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boSmGetBootMode,                   .unParameter.stSmGetBootMode               = {sizeof(gint)},                                                                                         .unReturnValues.stSmGetBootMode               = {sizeof(gint), 0x02}                                         },
  { &NSMTST__boSmGetBootMode,                   .unParameter.stSmGetBootMode               = {sizeof(gint) + 1},                                                                                     .unReturnValues.stSmGetBootMode               = {-1,           0x00}                                         },
  { &NSMTST__boSmGetBootMode,                   .unParameter.stSmGetBootMode               = {sizeof(gint) - 1},                                                                                     .unReturnValues.stSmGetBootMode               = {-1,           0x00}                                         },
  { &NSMTST__boDbGetRunningReason,              .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stDbGetRunningReason          = {NsmRunningReason_WakeupCan}                                 },
  { &NSMTST__boSmGetRunningReason,              .unParameter.stSmGetRunningReason          = {sizeof(NsmRunningReason_e)    },                                                                       .unReturnValues.stSmGetRunningReason          = {sizeof(NsmRunningReason_e), NsmRunningReason_WakeupCan}     },
  { &NSMTST__boSmGetRunningReason,              .unParameter.stSmGetRunningReason          = {sizeof(NsmRunningReason_e) + 1},                                                                       .unReturnValues.stSmGetRunningReason          = {-1, NsmRunningReason_NotSet}                                },
  { &NSMTST__boSmGetRunningReason,              .unParameter.stSmGetRunningReason          = {sizeof(NsmRunningReason_e) - 1},                                                                       .unReturnValues.stSmGetRunningReason          = {-1, NsmRunningReason_NotSet}                                },
  { &NSMTST__boSmSetShutdownReason,             .unParameter.stSmSetShutdownReason         = {sizeof(NsmShutdownReason_e), NsmShutdownReason_NotSet},                                                .unReturnValues.stSmSetShutdownReason         = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boSmSetShutdownReason,             .unParameter.stSmSetShutdownReason         = {sizeof(NsmShutdownReason_e), (NsmShutdownReason_e) 0xFFFFFFFF},                                        .unReturnValues.stSmSetShutdownReason         = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boSmSetShutdownReason,             .unParameter.stSmSetShutdownReason         = {sizeof(NsmShutdownReason_e) - 1, NsmShutdownReason_Normal},                                            .unReturnValues.stSmSetShutdownReason         = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boSmSetShutdownReason,             .unParameter.stSmSetShutdownReason         = {sizeof(NsmShutdownReason_e) + 1, NsmShutdownReason_Normal},                                            .unReturnValues.stSmSetShutdownReason         = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boSmSetShutdownReason,             .unParameter.stSmSetShutdownReason         = {sizeof(NsmShutdownReason_e),     NsmShutdownReason_Normal},                                            .unReturnValues.stSmSetShutdownReason         = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boSmSetShutdownReason,             .unParameter.stSmSetShutdownReason         = {sizeof(NsmShutdownReason_e),     NsmShutdownReason_Normal},                                            .unReturnValues.stSmSetShutdownReason         = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbGetShutdownReason,             .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stDbGetShutdownReason         = {NsmShutdownReason_Normal}                                   },
  { &NSMTST__boSmGetShutdownReason,             .unParameter.stSmGetShutdownReason         = {sizeof(NsmShutdownReason_e)    },                                                                      .unReturnValues.stSmGetShutdownReason         = {sizeof(NsmShutdownReason_e), NsmShutdownReason_Normal}      },
  { &NSMTST__boSmGetShutdownReason,             .unParameter.stSmGetShutdownReason         = {sizeof(NsmShutdownReason_e) - 1},                                                                      .unReturnValues.stSmGetShutdownReason         = {-1, NsmShutdownReason_NotSet}                               },
  { &NSMTST__boSmGetShutdownReason,             .unParameter.stSmGetShutdownReason         = {sizeof(NsmShutdownReason_e) + 1},                                                                      .unReturnValues.stSmGetShutdownReason         = {-1, NsmShutdownReason_NotSet}                               },
  { &NSMTST__boSmSetShutdownReason,             .unParameter.stSmSetShutdownReason         = {sizeof(NsmShutdownReason_e), NsmShutdownReason_SupplyBad},                                             .unReturnValues.stSmSetShutdownReason         = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbGetShutdownReason,             .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stDbGetShutdownReason         = {NsmShutdownReason_SupplyBad}                                },
  { &NSMTST__boSmGetShutdownReason,             .unParameter.stSmGetShutdownReason         = {sizeof(NsmShutdownReason_e)    },                                                                      .unReturnValues.stSmGetShutdownReason         = {sizeof(NsmShutdownReason_e), NsmShutdownReason_SupplyBad}   },
  { &NSMTST__boSmGetShutdownReason,             .unParameter.stSmGetShutdownReason         = {sizeof(NsmShutdownReason_e) - 1},                                                                      .unReturnValues.stSmGetShutdownReason         = {-1,                          NsmShutdownReason_NotSet}      },
  { &NSMTST__boSmGetShutdownReason,             .unParameter.stSmGetShutdownReason         = {sizeof(NsmShutdownReason_e) + 1},                                                                      .unReturnValues.stSmGetShutdownReason         = {-1,                          NsmShutdownReason_NotSet}      },
  { &NSMTST__boDbGetRestartReason,              .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stDbGetRestartReason          = {NsmRestartReason_NotSet}                                    },
  { &NSMTST__boSmGetRestartReason,              .unParameter.stSmGetRestartReason          = {sizeof(NsmRestartReason_e)},                                                                           .unReturnValues.stSmGetRestartReason          = {sizeof(NsmRestartReason_e), NsmRestartReason_NotSet}        },
  { &NSMTST__boSmGetRestartReason,              .unParameter.stSmGetRestartReason          = {sizeof(NsmRestartReason_e) - 1},                                                                       .unReturnValues.stSmGetRestartReason          = {-1, NsmRestartReason_NotSet}                                },
  { &NSMTST__boSmGetRestartReason,              .unParameter.stSmGetRestartReason          = {sizeof(NsmRestartReason_e) + 1},                                                                       .unReturnValues.stSmGetRestartReason          = {-1, NsmRestartReason_NotSet}                                },
  { &NSMTST__boDbSetNodeState,                  .unParameter.stDbSetNodeState              = {NsmNodeState_NotSet        },                                                                          .unReturnValues.stDbSetNodeState              = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boDbSetNodeState,                  .unParameter.stDbSetNodeState              = {(NsmNodeState_e) 0xFFFFFFFF},                                                                          .unReturnValues.stDbSetNodeState              = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boDbSetNodeState,                  .unParameter.stDbSetNodeState              = {NsmNodeState_BaseRunning   },                                                                          .unReturnValues.stDbSetNodeState              = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbSetNodeState,                  .unParameter.stDbSetNodeState              = {NsmNodeState_BaseRunning   },                                                                          .unReturnValues.stDbSetNodeState              = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbSetNodeState,                  .unParameter.stDbSetNodeState              = {NsmNodeState_LucRunning    },                                                                          .unReturnValues.stDbSetNodeState              = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boCheckNodeStateSignal,            .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stCheckNodeStateSignal        = {TRUE, NsmNodeState_LucRunning}                              },
  { &NSMTST__boDbGetNodeState,                  .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stDbGetNodeState              = {NsmErrorStatus_Ok, NsmNodeState_LucRunning}                 },
  { &NSMTST__boSmSetNodeState,                  .unParameter.stSmSetNodeState              = {sizeof(NsmNodeState_e), NsmNodeState_FullyRunning},                                                    .unReturnValues.stSmSetNodeState              = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boCheckNodeStateSignal,            .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stCheckNodeStateSignal        = {TRUE, NsmNodeState_FullyRunning}                            },
  { &NSMTST__boSmSetNodeState,                  .unParameter.stSmSetNodeState              = {3, NsmNodeState_FullyRunning},                                                                         .unReturnValues.stSmSetNodeState              = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boSmSetNodeState,                  .unParameter.stSmSetNodeState              = {5, NsmNodeState_FullyRunning},                                                                         .unReturnValues.stSmSetNodeState              = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boSmGetNodeState,                  .unParameter.stSmGetNodeState              = {sizeof(NsmNodeState_e)    },                                                                           .unReturnValues.stSmGetNodeState              = {sizeof(NsmNodeState_e), NsmNodeState_FullyRunning}          },
  { &NSMTST__boSmGetNodeState,                  .unParameter.stSmGetNodeState              = {sizeof(NsmNodeState_e) - 1},                                                                           .unReturnValues.stSmGetNodeState              = {-1, NsmNodeState_NotSet}                                    },
  { &NSMTST__boDbSetApplicationMode,            .unParameter.stDbSetApplicationMode        = {NsmApplicationMode_NotSet        },                                                                    .unReturnValues.stDbSetApplicationMode        = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boDbSetApplicationMode,            .unParameter.stDbSetApplicationMode        = {(NsmApplicationMode_e) 0xFFFFFFFF},                                                                    .unReturnValues.stDbSetApplicationMode        = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boDbSetApplicationMode,            .unParameter.stDbSetApplicationMode        = {NsmApplicationMode_Parking       },                                                                    .unReturnValues.stDbSetApplicationMode        = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbSetApplicationMode,            .unParameter.stDbSetApplicationMode        = {NsmApplicationMode_Parking       },                                                                    .unReturnValues.stDbSetApplicationMode        = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbSetApplicationMode,            .unParameter.stDbSetApplicationMode        = {NsmApplicationMode_Factory       },                                                                    .unReturnValues.stDbSetApplicationMode        = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boCheckApplicationModeSignal,      .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stCheckApplicationModeSignal  = {TRUE, NsmApplicationMode_Factory}                           },
  { &NSMTST__boDbGetApplicationMode,            .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stDbGetApplicationMode        = {NsmErrorStatus_Ok, NsmApplicationMode_Factory}              },
  { &NSMTST__boSmSetApplicationMode,            .unParameter.stSmSetApplicationMode        = {sizeof(NsmApplicationMode_e), NsmApplicationMode_Transport},                                           .unReturnValues.stSmSetApplicationMode        = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boCheckApplicationModeSignal,      .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stCheckApplicationModeSignal  = {TRUE, NsmApplicationMode_Transport}                         },
  { &NSMTST__boSmSetApplicationMode,            .unParameter.stSmSetApplicationMode        = {3, NsmApplicationMode_Transport},                                                                      .unReturnValues.stSmSetApplicationMode        = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boSmSetApplicationMode,            .unParameter.stSmSetApplicationMode        = {5, NsmApplicationMode_Transport},                                                                      .unReturnValues.stSmSetApplicationMode        = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boSmGetApplicationMode,            .unParameter.stSmGetApplicationMode        = {sizeof(NsmApplicationMode_e) + 1},                                                                     .unReturnValues.stSmGetApplicationMode        = {-1, NsmApplicationMode_NotSet}                              },
  { &NSMTST__boSmGetApplicationMode,            .unParameter.stSmGetApplicationMode        = {sizeof(NsmApplicationMode_e)    },                                                                     .unReturnValues.stSmGetApplicationMode        = {sizeof(NsmApplicationMode_e), NsmApplicationMode_Transport} },
  { &NSMTST__boDbGetInterfaceVersion,           .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stDbGetInterfaceVersion       = {NSM_INTERFACE_VERSION}                                      },
  { &NSMTST__boSmGetInterfaceVersion,           .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stSmGetInterfaceVersion       = {NSM_INTERFACE_VERSION}                                      },
  { &NSMTST__boSmSetInvalidData,                .unParameter.stSmSetInvalidData            = {sizeof(NsmRunningReason_e), NsmDataType_RunningReason},                                                .unReturnValues.stSmSetInvalidData            = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boSmSetInvalidData,                .unParameter.stSmSetInvalidData            = {sizeof(NsmRestartReason_e), NsmDataType_RestartReason},                                                .unReturnValues.stSmSetInvalidData            = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boSmSetInvalidData,                .unParameter.stSmSetInvalidData            = {sizeof(NsmRestartReason_e), 0xFFFFFFFF               },                                                .unReturnValues.stSmSetInvalidData            = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boSmGetInvalidData,                .unParameter.stSmGetInvalidData            = {sizeof(NsmRestartReason_e), 0xFFFFFFFF               },                                                .unReturnValues.stSmGetInvalidData            = {-1}                                                         },
  { &NSMTST__boDbRegisterSession,               .unParameter.stDbRegisterSession           = {"VoiceControl",         "NodeStateManager",     NsmSeat_Driver,         NsmSessionState_Active},       .unReturnValues.stDbRegisterSession           = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boDbRegisterSession,               .unParameter.stDbRegisterSession           = {NSMTST__260CHAR_STRING, "NodeStateTest",        NsmSeat_Driver,         NsmSessionState_Active},       .unReturnValues.stDbRegisterSession           = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boDbRegisterSession,               .unParameter.stDbRegisterSession           = {"VoiceControl",         NSMTST__260CHAR_STRING, NsmSeat_Driver,         NsmSessionState_Active},       .unReturnValues.stDbRegisterSession           = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boDbRegisterSession,               .unParameter.stDbRegisterSession           = {"VoiceControl",         "NodeStateTest",        NsmSeat_NotSet,         NsmSessionState_Active},       .unReturnValues.stDbRegisterSession           = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boDbRegisterSession,               .unParameter.stDbRegisterSession           = {"VoiceControl",         "NodeStateTest",        (NsmSeat_e) 0xFFFFFFFF, NsmSessionState_Active},       .unReturnValues.stDbRegisterSession           = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boDbRegisterSession,               .unParameter.stDbRegisterSession           = {"VoiceControl",         "NodeStateTest",        NsmSeat_Driver,         NsmSessionState_Unregistered}, .unReturnValues.stDbRegisterSession           = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boDbRegisterSession,               .unParameter.stDbRegisterSession           = {"DiagnosisSession",     "NodeStateTest",        NsmSeat_Driver,         NsmSessionState_Active},       .unReturnValues.stDbRegisterSession           = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boDbRegisterSession,               .unParameter.stDbRegisterSession           = {"VoiceControl",         "NodeStateTest",        NsmSeat_Driver,         NsmSessionState_Active},       .unReturnValues.stDbRegisterSession           = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbUnRegisterSession,             .unParameter.stDbUnRegisterSession         = {NSMTST__260CHAR_STRING, "NodeStateTest",        NsmSeat_Driver},                                       .unReturnValues.stDbUnRegisterSession         = {NsmErrorStatus_Parameter   }                                },
  { &NSMTST__boDbUnRegisterSession,             .unParameter.stDbUnRegisterSession         = {"VoiceControl",         NSMTST__260CHAR_STRING, NsmSeat_Driver},                                       .unReturnValues.stDbUnRegisterSession         = {NsmErrorStatus_Parameter   }                                },
  { &NSMTST__boDbUnRegisterSession,             .unParameter.stDbUnRegisterSession         = {"DiagnosisSession",     "NodeStateTest",        NsmSeat_Driver},                                       .unReturnValues.stDbUnRegisterSession         = {NsmErrorStatus_WrongSession}                                },
  { &NSMTST__boDbUnRegisterSession,             .unParameter.stDbUnRegisterSession         = {"Unknown",              "NodeStateTest",        NsmSeat_Driver},                                       .unReturnValues.stDbUnRegisterSession         = {NsmErrorStatus_WrongSession}                                },
  { &NSMTST__boDbUnRegisterSession,             .unParameter.stDbUnRegisterSession         = {"VoiceControl",         "NodeStateTest",        NsmSeat_Driver},                                       .unReturnValues.stDbUnRegisterSession         = {NsmErrorStatus_Ok          }                                },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {NSMTST__260CHAR_STRING, "NodeStateTest",        NsmSeat_Driver,         NsmSessionState_Active},       .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_Parameter   }                                },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {"VoiceControl",         NSMTST__260CHAR_STRING, NsmSeat_Driver,         NsmSessionState_Active},       .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_Parameter   }                                },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {"VoiceControl",         "NodeStateTest",        NsmSeat_Driver,         NsmSessionState_Active},       .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_WrongSession}                                },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {"VoiceControl",         "NodeStateManager",     NsmSeat_Driver,         NsmSessionState_Active},       .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {"VoiceControl",         "NodeStateTest",        NsmSeat_Driver,         NsmSessionState_Unregistered}, .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {"VoiceControl",         "NodeStateTest",        NsmSeat_NotSet,         NsmSessionState_Active},       .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {"VoiceControl",         "NodeStateTest",        (NsmSeat_e) 0xFFFFFFFF, NsmSessionState_Active},       .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {"DiagnosisSession", "NodeStateTest", NsmSeat_Driver, NsmSessionState_Active  },                        .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {"DiagnosisSession", "NodeStateTest", NsmSeat_Driver, (NsmSessionState_e) 0x03},                        .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {"DiagnosisSession", "NodeStateTest", NsmSeat_Driver, NsmSessionState_Inactive},                        .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {"DiagnosisSession", "NodeStateTest",   NsmSeat_Driver, NsmSessionState_Active},                        .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_Ok       }                                   },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {"DiagnosisSession", "NodeStateTest02", NsmSeat_Driver, NsmSessionState_Active},                        .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_Error    }                                   },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {"DiagnosisSession", "NodeStateTest",   NsmSeat_Driver, NsmSessionState_Inactive},                      .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_Ok       }                                   },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {"DiagnosisSession", "NodeStateTest",   NsmSeat_Driver, NsmSessionState_Inactive},                      .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {"ProductSession", "NodeStateTest", NsmSeat_Driver, NsmSessionState_Active},                            .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_WrongSession}                                },
  { &NSMTST__boDbRegisterSession,               .unParameter.stDbRegisterSession           = {"ProductSession", "NodeStateTest", NsmSeat_Driver, NsmSessionState_Active},                            .unReturnValues.stDbRegisterSession           = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {"ProductSession", "NodeStateTest", NsmSeat_Driver, NsmSessionState_Inactive},                          .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {"ProductSession", "NodeStateTest", NsmSeat_Driver, NsmSessionState_Inactive},                          .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbGetSessionState,               .unParameter.stDbGetSessionState           = {NSMTST__260CHAR_STRING, NsmSeat_Driver},                                                               .unReturnValues.stDbGetSessionState           = {NsmErrorStatus_Parameter, NsmSessionState_Unregistered   }  },
  { &NSMTST__boDbGetSessionState,               .unParameter.stDbGetSessionState           = {"ProductSession",       NsmSeat_Driver},                                                               .unReturnValues.stDbGetSessionState           = {NsmErrorStatus_Ok, NsmSessionState_Inactive              }  },
  { &NSMTST__boDbGetSessionState,               .unParameter.stDbGetSessionState           = {"UnknownSession",       NsmSeat_Driver},                                                               .unReturnValues.stDbGetSessionState           = {NsmErrorStatus_WrongSession, NsmSessionState_Unregistered}  },
  { &NSMTST__boSmGetSessionState,               .unParameter.stSmGetSessionState           = {sizeof(NsmSession_s), {"ProductSession", "NodeStateTest", NsmSeat_Driver,   NsmSessionState_Active}},  .unReturnValues.stSmGetSessionState           = {sizeof(NsmSession_s), NsmSessionState_Inactive}             },
  { &NSMTST__boDbUnRegisterSession,             .unParameter.stDbUnRegisterSession         = {"ProductSession", "NodeStateTest", NsmSeat_Driver},                                                    .unReturnValues.stDbUnRegisterSession         = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boSmSetSessionState,               .unParameter.stSmSetSessionState           = {sizeof(NsmSession_s), {"ProductSession", "NodeStateTest", NsmSeat_Driver,   NsmSessionState_Active}},  .unReturnValues.stSmSetSessionState           = {NsmErrorStatus_WrongSession}                                },
  { &NSMTST__boSmSetSessionState,               .unParameter.stSmSetSessionState           = {4, {"ProductSession", "NodeStateTest", NsmSeat_Driver,   NsmSessionState_Active}},                     .unReturnValues.stSmSetSessionState           = {NsmErrorStatus_Parameter   }                                },
  { &NSMTST__boSmGetSessionState,               .unParameter.stSmGetSessionState           = {5, {"ProductSession", "NodeStateTest", NsmSeat_Driver,   NsmSessionState_Active}},                     .unReturnValues.stSmGetSessionState           = {-1, NsmSessionState_Unregistered}                           },
  { &NSMTST__boDbGetAppHealthCount,             .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stDbGetAppHealthCount         = {0x00}                                                       },
  { &NSMTST__boDbSetAppHealthStatus,            .unParameter.stDbSetAppHealthStatus        = {NSMTST__260CHAR_STRING, TRUE },                                                                        .unReturnValues.stDbSetAppHealthStatus        = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boDbSetAppHealthStatus,            .unParameter.stDbSetAppHealthStatus        = {"NodeStateTest",        TRUE },                                                                        .unReturnValues.stDbSetAppHealthStatus        = {NsmErrorStatus_Error}                                       },
  { &NSMTST__boDbSetAppHealthStatus,            .unParameter.stDbSetAppHealthStatus        = {"NodeStateTest",        FALSE},                                                                        .unReturnValues.stDbSetAppHealthStatus        = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbSetAppHealthStatus,            .unParameter.stDbSetAppHealthStatus        = {"NodeStateTest",        TRUE },                                                                        .unReturnValues.stDbSetAppHealthStatus        = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbRegisterSession,               .unParameter.stDbRegisterSession           = {"ProductSession", "NodeStateTest", NsmSeat_Driver,   NsmSessionState_Active},                          .unReturnValues.stDbRegisterSession           = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbSetAppHealthStatus,            .unParameter.stDbSetAppHealthStatus        = {"NodeStateTest",  FALSE},                                                                              .unReturnValues.stDbSetAppHealthStatus        = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbSetAppHealthStatus,            .unParameter.stDbSetAppHealthStatus        = {"NodeStateTest1", FALSE},                                                                              .unReturnValues.stDbSetAppHealthStatus        = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbSetAppHealthStatus,            .unParameter.stDbSetAppHealthStatus        = {"NodeStateTest",  TRUE},                                                                               .unReturnValues.stDbSetAppHealthStatus        = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {"DiagnosisSession", "NodeStateTest", NsmSeat_Driver,   NsmSessionState_Active},                        .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {"HevacSession", "NodeStateTest", NsmSeat_Driver,   NsmSessionState_Active},                            .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbSetAppHealthStatus,            .unParameter.stDbSetAppHealthStatus        = {"NodeStateTest", FALSE},                                                                               .unReturnValues.stDbSetAppHealthStatus        = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbCheckLucRequired,              .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stDbCheckLucRequired          = {TRUE}                                                       },
  { &NSMTST__boDbRequestNodeRestart,            .unParameter.stDbRequestNodeRestart        = {NsmRestartReason_ApplicationFailure, NSM_SHUTDOWNTYPE_NORMAL},                                         .unReturnValues.stDbRequestNodeRestart        = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boTestCreateLcClient,              .unParameter.stTestCreateLcClient          = {"/org/genivi/NodeStateTest/LcClient01"},                                                               .unReturnValues.stTestDummy                   = {0x00}                                                       },
  { &NSMTST__boTestCreateLcClient,              .unParameter.stTestCreateLcClient          = {"/org/genivi/NodeStateTest/LcClient02"},                                                               .unReturnValues.stTestDummy                   = {0x00}                                                       },
  { &NSMTST__boTestCreateLcClient,              .unParameter.stTestCreateLcClient          = {"/org/genivi/NodeStateTest/LcClient03"},                                                               .unReturnValues.stTestDummy                   = {0x00}                                                       },
  { &NSMTST__boTestCreateLcClient,              .unParameter.stTestCreateLcClient          = {"/org/genivi/NodeStateTest/LcClient04"},                                                               .unReturnValues.stTestDummy                   = {0x00}                                                       },
  { &NSMTST__boTestCreateLcClient,              .unParameter.stTestCreateLcClient          = {"/org/genivi/NodeStateTest/LcClient05"},                                                               .unReturnValues.stTestDummy                   = {0x00}                                                       },
  { &NSMTST__boDbRegisterShutdownClient,        .unParameter.stDbRegisterShutdownClient    = {"/org/genivi/NodeStateTest/LcClient01", NSM_SHUTDOWNTYPE_NORMAL, 2000},                                .unReturnValues.stDbRegisterShutdownClient    = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbRegisterShutdownClient,        .unParameter.stDbRegisterShutdownClient    = {"/org/genivi/NodeStateTest/LcClient01", NSM_SHUTDOWNTYPE_FAST,   2000},                                .unReturnValues.stDbRegisterShutdownClient    = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbRegisterShutdownClient,        .unParameter.stDbRegisterShutdownClient    = {"/org/genivi/NodeStateTest/LcClient02", NSM_SHUTDOWNTYPE_NORMAL, 2000},                                .unReturnValues.stDbRegisterShutdownClient    = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbRegisterShutdownClient,        .unParameter.stDbRegisterShutdownClient    = {"/org/genivi/NodeStateTest/LcClient02", NSM_SHUTDOWNTYPE_FAST,   2000},                                .unReturnValues.stDbRegisterShutdownClient    = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbRegisterShutdownClient,        .unParameter.stDbRegisterShutdownClient    = {"/org/genivi/NodeStateTest/LcClient03", NSM_SHUTDOWNTYPE_NORMAL, 2000},                                .unReturnValues.stDbRegisterShutdownClient    = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbRegisterShutdownClient,        .unParameter.stDbRegisterShutdownClient    = {"/org/genivi/NodeStateTest/LcClient04", NSM_SHUTDOWNTYPE_NORMAL, 2000},                                .unReturnValues.stDbRegisterShutdownClient    = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbRegisterShutdownClient,        .unParameter.stDbRegisterShutdownClient    = {"/org/genivi/NodeStateTest/LcClient05", NSM_SHUTDOWNTYPE_NORMAL | NSM_SHUTDOWNTYPE_FAST, 2000},        .unReturnValues.stDbRegisterShutdownClient    = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbRegisterShutdownClient,        .unParameter.stDbRegisterShutdownClient    = {"/org/genivi/NodeStateTest/LcClient06", NSM_SHUTDOWNTYPE_NORMAL, 2000},                                .unReturnValues.stDbRegisterShutdownClient    = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbUnRegisterShutdownClient,      .unParameter.stDbUnRegisterShutdownClient  = {"/org/genivi/NodeStateTest/LcClient01", NSM_SHUTDOWNTYPE_FAST},                                        .unReturnValues.stDbUnRegisterShutdownClient  = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbUnRegisterShutdownClient,      .unParameter.stDbUnRegisterShutdownClient  = {"/org/genivi/NodeStateTest/LcClient06", NSM_SHUTDOWNTYPE_NORMAL},                                      .unReturnValues.stDbUnRegisterShutdownClient  = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbUnRegisterShutdownClient,      .unParameter.stDbUnRegisterShutdownClient  = {"/org/genivi/NodeStateTest/LcClient07", NSM_SHUTDOWNTYPE_NORMAL},                                      .unReturnValues.stDbUnRegisterShutdownClient  = {NsmErrorStatus_Parameter}                                   },
  { &NSMTST__boDbSetNodeState,                  .unParameter.stDbSetNodeState              = {NsmNodeState_ShuttingDown},                                                                            .unReturnValues.stDbSetNodeState              = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boTestProcessLifecycleRequest,     .unParameter.stTestProcessLifecycleRequest = {NsmErrorStatus_Ok             },                                                                       .unReturnValues.stTestProcessLifecycleRequest = {NSM_SHUTDOWNTYPE_NORMAL}                                    },
  { &NSMTST__boTestProcessLifecycleRequest,     .unParameter.stTestProcessLifecycleRequest = {NsmErrorStatus_Ok             },                                                                       .unReturnValues.stTestProcessLifecycleRequest = {NSM_SHUTDOWNTYPE_NORMAL}                                    },
  { &NSMTST__boTestProcessLifecycleRequest,     .unParameter.stTestProcessLifecycleRequest = {NsmErrorStatus_Error          },                                                                       .unReturnValues.stTestProcessLifecycleRequest = {NSM_SHUTDOWNTYPE_NORMAL}                                    },
  { &NSMTST__boTestProcessLifecycleRequest,     .unParameter.stTestProcessLifecycleRequest = {NsmErrorStatus_ResponsePending},                                                                       .unReturnValues.stTestProcessLifecycleRequest = {NSM_SHUTDOWNTYPE_NORMAL}                                    },
  { &NSMTST__boDbLifecycleRequestComplete,      .unParameter.stDbLifecycleRequestComplete  = {NsmErrorStatus_Ok             },                                                                       .unReturnValues.stDbLifecycleRequestComplete  = {NsmErrorStatus_Ok      }                                    },
  { &NSMTST__boDbSetNodeState,                  .unParameter.stDbSetNodeState              = {NsmNodeState_BaseRunning},                                                                             .unReturnValues.stDbSetNodeState              = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boTestProcessLifecycleRequest,     .unParameter.stTestProcessLifecycleRequest = {NsmErrorStatus_Ok},                                                                                    .unReturnValues.stTestProcessLifecycleRequest = {NSM_SHUTDOWNTYPE_NORMAL}                                    },
  { &NSMTST__boTestProcessLifecycleRequest,     .unParameter.stTestProcessLifecycleRequest = {NsmErrorStatus_Ok},                                                                                    .unReturnValues.stTestProcessLifecycleRequest = {NSM_SHUTDOWNTYPE_RUNUP}                                     },
  { &NSMTST__boTestProcessLifecycleRequest,     .unParameter.stTestProcessLifecycleRequest = {NsmErrorStatus_Ok},                                                                                    .unReturnValues.stTestProcessLifecycleRequest = {NSM_SHUTDOWNTYPE_RUNUP}                                     },
  { &NSMTST__boTestProcessLifecycleRequest,     .unParameter.stTestProcessLifecycleRequest = {NsmErrorStatus_Ok},                                                                                    .unReturnValues.stTestProcessLifecycleRequest = {NSM_SHUTDOWNTYPE_RUNUP}                                     },
  { &NSMTST__boTestProcessLifecycleRequest,     .unParameter.stTestProcessLifecycleRequest = {NsmErrorStatus_Ok},                                                                                    .unReturnValues.stTestProcessLifecycleRequest = {NSM_SHUTDOWNTYPE_RUNUP}                                     },
  { &NSMTST__boTestProcessLifecycleRequest,     .unParameter.stTestProcessLifecycleRequest = {NsmErrorStatus_Ok},                                                                                    .unReturnValues.stTestProcessLifecycleRequest = {NSM_SHUTDOWNTYPE_RUNUP}                                     },
  { &NSMTST__boDbSetNodeState,                  .unParameter.stDbSetNodeState              = {NsmNodeState_FastShutdown},                                                                            .unReturnValues.stDbSetNodeState              = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boTestProcessLifecycleRequest,     .unParameter.stTestProcessLifecycleRequest = {NsmErrorStatus_Ok},                                                                                    .unReturnValues.stTestProcessLifecycleRequest = {NSM_SHUTDOWNTYPE_FAST}                                      },
  { &NSMTST__boDbSetNodeState,                  .unParameter.stDbSetNodeState              = {NsmNodeState_BaseRunning},                                                                             .unReturnValues.stDbSetNodeState              = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boTestProcessLifecycleRequest,     .unParameter.stTestProcessLifecycleRequest = {NsmErrorStatus_Ok},                                                                                    .unReturnValues.stTestProcessLifecycleRequest = {NSM_SHUTDOWNTYPE_FAST}                                      },
  { &NSMTST__boTestProcessLifecycleRequest,     .unParameter.stTestProcessLifecycleRequest = {NsmErrorStatus_Ok},                                                                                    .unReturnValues.stTestProcessLifecycleRequest = {NSM_SHUTDOWNTYPE_RUNUP}                                     },
  { &NSMTST__boTestProcessLifecycleRequest,     .unParameter.stTestProcessLifecycleRequest = {NsmErrorStatus_Ok},                                                                                    .unReturnValues.stTestProcessLifecycleRequest = {NSM_SHUTDOWNTYPE_RUNUP}                                     },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {"PlatformSupplySession", "NodeStateTest", NsmSeat_Driver, (NsmSessionState_e) 0x02},                   .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boDbGetSessionState,               .unParameter.stDbGetSessionState           = {"PlatformSupplySession",       NsmSeat_Driver},                                                        .unReturnValues.stDbGetSessionState           = {NsmErrorStatus_Ok, (NsmSessionState_e) 0x02}                },
  { &NSMTST__boDbSetSessionState,               .unParameter.stDbSetSessionState           = {"PlatformSupplySession", "NodeStateTest", NsmSeat_Driver, (NsmSessionState_e) 0x03},                   .unReturnValues.stDbSetSessionState           = {NsmErrorStatus_Ok}                                          },
  { &NSMTST__boCheckSessionSignal,              .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stCheckSessionSignal          = {TRUE, "PlatformSupplySession", NsmSeat_Driver, (NsmSessionState_e) 0x03 } },
  { &NSMTST__boDbGetSessionState,               .unParameter.stDbGetSessionState           = {"PlatformSupplySession",       NsmSeat_Driver},                                                        .unReturnValues.stDbGetSessionState           = {NsmErrorStatus_Ok, (NsmSessionState_e) 0x03}                              },
  { &NSMTST__boSmRegisterSession,               .unParameter.stSmRegisterSession           = {sizeof(NsmSession_s), {"StateMachine", "NodeStateTest", NsmSeat_Driver, NsmSessionState_Active}},      .unReturnValues.stSmRegisterSession           = {NsmErrorStatus_Ok}                                                        },
  { &NSMTST__boCheckSessionSignal,              .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stCheckSessionSignal          = {TRUE, "StateMachine", NsmSeat_Driver, NsmSessionState_Active }            },
  { &NSMTST__boSmRegisterSession,               .unParameter.stSmRegisterSession           = {sizeof(NsmSession_s)-1, {"StateMachine", "NodeStateTest", NsmSeat_Driver, NsmSessionState_Active}},    .unReturnValues.stSmRegisterSession           = {NsmErrorStatus_Parameter}                                                 },
  { &NSMTST__boSmUnRegisterSession,             .unParameter.stSmUnRegisterSession         = {sizeof(NsmSession_s)-1, {"StateMachine", "NodeStateTest", NsmSeat_Driver, NsmSessionState_Active}},    .unReturnValues.stSmRegisterSession           = {NsmErrorStatus_Parameter}                                                 },
  { &NSMTST__boSmUnRegisterSession,             .unParameter.stSmUnRegisterSession         = {sizeof(NsmSession_s),   {"StateMachine", "NodeStateTest", NsmSeat_Driver, NsmSessionState_Unregistered}}, .unReturnValues.stSmRegisterSession        = {NsmErrorStatus_Ok}                                                        },
  { &NSMTST__boCheckSessionSignal,              .unParameter.stTestDummy                   = {0x00},                                                                                                 .unReturnValues.stCheckSessionSignal          = {TRUE, "StateMachine", NsmSeat_Driver, NsmSessionState_Unregistered }      }
};


/**********************************************************************************************************************
*
* Local (static) functions
*
**********************************************************************************************************************/

/**********************************************************************************************************************
*
* Helper function to prepare data for StateMachine. Data is read from the passed array and converted into a GVariant.
*
* @param pDataArray:   Array which should be written to GVariant.
* @param u32ArraySize: Number of bytes that the array at pDataArray contains.
*
* @return GVariant of type "ay" that includes the passed data.
*
**********************************************************************************************************************/
static GVariant* NSMTST__pPrepareStateMachineData(guchar *pDataArray, const guint32 u32ArraySize)
{
  /* Function local variables                                          */
  guint32    u32ArrayIdx    = 0;    /* Index to loop through data      */
  GVariant **aArrayElements = NULL; /* GVariant elements to build "ay" */

  aArrayElements = g_new(GVariant*, u32ArraySize); /* Create new variants */

  /* Store bytes in new variants */
  for(u32ArrayIdx = 0; u32ArrayIdx < u32ArraySize; u32ArrayIdx++)
  {
    aArrayElements[u32ArrayIdx] = g_variant_new_byte(pDataArray[u32ArrayIdx]);
  }

  /* Return a new varaint array */
  return g_variant_new_array(G_VARIANT_TYPE_BYTE, aArrayElements, u32ArraySize);
}

/**********************************************************************************************************************
*
* Helper function to retrieve StateMachine data from received GVariant.
*
* @param pVariantArray: Pointer to the variant that contaions the data.
* @param pDataArray:    Pointer where the bytes from pVariantArray should be stored.
*                       The caller has to assert that there are as many bytes reserved as the variant has children.
*
**********************************************************************************************************************/
static void NSMTST__vGetStateMachineData(GVariant *pVariantArray, guchar *pDataArray)
{
  /* Function local variables                                        */
  guint          u32ArrayIdx   = 0;    /* Index to loop through data */
  GVariant      *pArrayElement = NULL; /* Pointer to variant child   */

  /* Loop through all children iof the source variant */
  for(u32ArrayIdx = 0; u32ArrayIdx < g_variant_n_children(pVariantArray); u32ArrayIdx++)
  {
    /* Get the variant child and get the byte value from it. */
    pArrayElement           = g_variant_get_child_value(pVariantArray, u32ArrayIdx);
    pDataArray[u32ArrayIdx] = g_variant_get_byte(pArrayElement);
  }

  g_variant_unref(pVariantArray); /* Release the variant */
}

/**********************************************************************************************************************
*
* Test function to get the bus connection.
*
* @return TRUE: Test case successful. FALSE: Test case failed.
*
**********************************************************************************************************************/
static gboolean NSMTST__boTestGetBusConnection(void)
{
  /* Function local variables                */
  gboolean  boRetVal = TRUE; /* Return value */
  GError   *pError   = NULL; /* DBus Error   */

  NSMTST__sTestDescription = g_strdup_printf("Get connection to %s bus.",
                                             NSM_BUS_TYPE == G_BUS_TYPE_SYSTEM ? "system" : "session");

  NSMTST__pConnection = g_bus_get_sync(NSM_BUS_TYPE, NULL, &pError);

  if(pError == NULL)
  {
    NSMTST__sBusName = g_dbus_connection_get_unique_name(NSMTST__pConnection);

    if(NSMTST__sBusName != NULL)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Failed to get bus name.");
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to get bus connection. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

/**********************************************************************************************************************
*
* Test function to create NodeStateConsumer proxy.
*
* @return TRUE: Test case successful. FALSE: Test case failed.
*
**********************************************************************************************************************/
static gboolean NSMTST__boTestCreateConsumerProxy(void)
{
  /* Function local variables                */
  gboolean  boRetVal = TRUE; /* Return value */
  GError   *pError   = NULL; /* DBus Error   */

  /* Write test description */
  NSMTST__sTestDescription = g_strdup("Create NodeStateConsumer proxy.");

  NSMTST__pNodeStateConsumer = node_state_consumer_proxy_new_sync(NSMTST__pConnection,
                                                                  G_DBUS_PROXY_FLAGS_NONE,
                                                                  NSM_BUS_NAME,
                                                                  NSM_CONSUMER_OBJECT,
                                                                  NULL,
                                                                  &pError);

  if(pError != NULL)
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create proxy object. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

/**********************************************************************************************************************
*
* Test function to create LifecycleControl proxy.
*
* @return TRUE: Test case successful. FALSE: Test case failed.
*
**********************************************************************************************************************/
static gboolean NSMTST__boTestCreateLifecycleControlProxy(void)
{
  /* Function local variables                */
  gboolean  boRetVal = TRUE; /* Return value */
  GError   *pError   = NULL; /* DBus Error   */

  /* Write test description */
  NSMTST__sTestDescription = g_strdup("Create LifecycleControl proxy.");

  NSMTST__pLifecycleControl = node_state_lifecycle_control_proxy_new_sync(NSMTST__pConnection,
                                                                          G_DBUS_PROXY_FLAGS_NONE,
                                                                          NSM_BUS_NAME,
                                                                          NSM_LIFECYCLE_OBJECT,
                                                                          NULL,
                                                                          &pError);

  if(pError != NULL)
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create proxy object. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

/**********************************************************************************************************************
*
* Test function to create NodeStateMachineTest proxy.
*
* @return TRUE: Test case successful. FALSE: Test case failed.
*
**********************************************************************************************************************/
static gboolean NSMTST__boTestCreateNodeStateMachineProxy(void)
{
  /* Function local variables                */
  gboolean  boRetVal = TRUE; /* Return value */
  GError   *pError   = NULL; /* DBus Error   */

  /* Write test description */
  NSMTST__sTestDescription = g_strdup("Create NodeStateMachine proxy.");

  NSMTST__pNodeStateMachine = node_state_test_proxy_new_sync(NSMTST__pConnection,
                                                             G_DBUS_PROXY_FLAGS_NONE,
                                                             NSM_BUS_NAME,
                                                             "/com/contiautomotive/NodeStateMachineTest",
                                                             NULL,
                                                             &pError);

  if(pError != NULL)
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create proxy object. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

/**********************************************************************************************************************
*
* Test function to register callbacks.
*
* @return TRUE: Test case successful. FALSE: Test case failed.
*
**********************************************************************************************************************/
static gboolean NSMTST__boTestRegisterCallbacks(void)
{
  /* Provide test description */
  NSMTST__sTestDescription = g_strdup("Register callbacks for SessionState, NodeState and ApplicationMode signals.");

  /* Perform test calls */
  g_signal_connect(NSMTST__pNodeStateConsumer, "node-state",            G_CALLBACK(NSMTST__boOnNodeStateSignal),       NULL);
  g_signal_connect(NSMTST__pNodeStateConsumer, "node-application-mode", G_CALLBACK(NSMTST__boOnApplicationModeSignal), NULL);
  g_signal_connect(NSMTST__pNodeStateConsumer, "session-state-changed", G_CALLBACK(NSMTST__boOnSessionSignal),         NULL);

  return TRUE;
}

/**********************************************************************************************************************
*
* Test function for trying to set invalid data via the (internal) NsmSetData of the NSM.
*
* @return TRUE: Test case successful. FALSE: Test case failed.
*
**********************************************************************************************************************/
static gboolean NSMTST__boSmSetInvalidData(void)
{
  /* Function local variables                                                                     */
  gboolean              boRetVal            = TRUE;                      /* Return value          */
  GError               *pError              = NULL;
  GVariant             *pDataIn             = NULL;
  NsmErrorStatus_e      enReceivedNsmReturn = NsmErrorStatus_NotSet;

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Set invalid data. Interface: StateMachine.");

  /* Perform test call */
  pDataIn = g_variant_new_array(G_VARIANT_TYPE_BYTE, NULL, 0);
  (void) node_state_test_call_set_nsm_data_sync(NSMTST__pNodeStateMachine,
                                                NSMTST__pstTestCase->unParameter.stSmSetInvalidData.enDataType,
                                                pDataIn,
                                                NSMTST__pstTestCase->unParameter.stSmSetInvalidData.u32DataLen,
                                                (gint*) &enReceivedNsmReturn,
                                                NULL,
                                                &pError);
  g_variant_unref(pDataIn);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(enReceivedNsmReturn == NSMTST__pstTestCase->unReturnValues.stSmSetInvalidData.enErrorStatus)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  enReceivedNsmReturn, NSMTST__pstTestCase->unReturnValues.stSmSetInvalidData.enErrorStatus);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSMC via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boSmGetInvalidData(void)
{
  /* Function local variables                                                                           */
  gboolean              boRetVal                  = TRUE;                      /* Return value          */
  GError               *pError                    = NULL;
  GVariant             *pDataIn                   = NULL;
  GVariant             *pDataOut                  = NULL;
  gint                  i32ReceivedNsmReturn      = -1;

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Get invalid data. Interface: StateMachine.");

  /* Perform test call */
  pDataIn = g_variant_new_array(G_VARIANT_TYPE_BYTE, NULL, 0);
  (void) node_state_test_call_get_nsm_data_sync(NSMTST__pNodeStateMachine,
                                                NSMTST__pstTestCase->unParameter.stSmGetInvalidData.enDataType,
                                                pDataIn,
                                                NSMTST__pstTestCase->unParameter.stSmGetInvalidData.u32DataLen,
                                                &pDataOut,
                                                &i32ReceivedNsmReturn,
                                                NULL,
                                                &pError);
  g_variant_unref(pDataIn);
  g_variant_unref(pDataOut);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(i32ReceivedNsmReturn == NSMTST__pstTestCase->unReturnValues.stSmGetInvalidData.i32WrittenBytes)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  i32ReceivedNsmReturn, NSMTST__pstTestCase->unReturnValues.stSmGetInvalidData.i32WrittenBytes);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSMC via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boDbGetRestartReason(void)
{
  /* Function local variables                                                                    */
  gboolean           boRetVal                = TRUE;                    /* Return value          */
  NsmRestartReason_e enReceivedRestartReason = NsmRestartReason_NotSet; /* Value returned by NSM */
  NsmRestartReason_e enExpectedRestartReason = NsmRestartReason_NotSet; /* Expected value        */

  enExpectedRestartReason = NSMTST__pstTestCase->unReturnValues.stDbGetRestartReason.enRestartReason;

  /* Provide test description */
  NSMTST__sTestDescription = g_strdup_printf("Get RestartReason. Interface: D-Bus. Expected value: 0x%02X.",
                                             enExpectedRestartReason);

  enReceivedRestartReason = (NsmRestartReason_e) node_state_consumer_get_restart_reason(NSMTST__pNodeStateConsumer);

  if(enReceivedRestartReason != enExpectedRestartReason)
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected value. Received: 0x%02X. Expected: 0x%02X.",
                                                enReceivedRestartReason, enExpectedRestartReason);
  }

  return boRetVal;
}

static gboolean NSMTST__boDbGetShutdownReason(void)
{
  /* Function local variables                                                                       */
  gboolean            boRetVal                 = TRUE;                     /* Return value          */
  NsmShutdownReason_e enReceivedShutdownReason = NsmShutdownReason_NotSet; /* Value returned by NSM */
  NsmShutdownReason_e enExpectedShutdownReason = NsmShutdownReason_NotSet; /* Expected value        */

  enExpectedShutdownReason = NSMTST__pstTestCase->unReturnValues.stDbGetShutdownReason.enShutdownReason;

  NSMTST__sTestDescription = g_strdup_printf("Get ShutdownReason. Interface: D-Bus. Expected value: 0x%02X.",
                                             enExpectedShutdownReason);

  enReceivedShutdownReason = (NsmShutdownReason_e) node_state_consumer_get_shutdown_reason(NSMTST__pNodeStateConsumer);

  if(enReceivedShutdownReason != enExpectedShutdownReason)
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected value. Received: 0x%02X. Expected: 0x%02X.",
                                                enReceivedShutdownReason, enExpectedShutdownReason);
  }

  return boRetVal;
}

static gboolean NSMTST__boDbGetRunningReason(void)
{
  /* Function local variables                                                                     */
  gboolean           boRetVal                = TRUE;                     /* Return value          */
  NsmRunningReason_e enReceivedRunningReason = NsmShutdownReason_NotSet; /* Value returned by NSM */

  /* Provide test description */
  NSMTST__sTestDescription = g_strdup_printf("Get RunningReason. Interface: D-Bus. Expected value: 0x%02X.",
                                             NSMTST__pstTestCase->unReturnValues.stDbGetRunningReason.enRunningReason);

  enReceivedRunningReason = (NsmRunningReason_e) node_state_consumer_get_wake_up_reason(NSMTST__pNodeStateConsumer);

  if(enReceivedRunningReason != NSMTST__pstTestCase->unReturnValues.stDbGetRunningReason.enRunningReason)
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected value. Received: 0x%02X. Expected: 0x%02X.",
                                                enReceivedRunningReason, NSMTST__pstTestCase->unReturnValues.stDbGetRunningReason.enRunningReason);
  }

  return boRetVal;
}

static gboolean NSMTST__boDbGetBootMode(void)
{
  /* Function local variables                                   */
  gboolean boRetVal            = TRUE; /* Return value          */
  gint     i32ReceivedBootMode = 0x00; /* Value returned by NSM */
  gint     i32ExpectedBootMode = 0x00; /* Expected value        */

  i32ExpectedBootMode = NSMTST__pstTestCase->unReturnValues.stDbGetBootMode.i32BootMode;

  /* Provide test description */
  NSMTST__sTestDescription = g_strdup_printf("Get BootMode. Interface: D-Bus. Expected value: 0x%02X.",
                                             i32ExpectedBootMode);

  i32ReceivedBootMode = (gint) node_state_consumer_get_boot_mode(NSMTST__pNodeStateConsumer);

  if(i32ReceivedBootMode != i32ExpectedBootMode)
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected value. Received: 0x%02X. Expected: 0x%02X.",
                                                i32ReceivedBootMode, i32ExpectedBootMode);
  }

  return boRetVal;
}

static gboolean NSMTST__boSmGetApplicationMode(void)
{
  /* Function local variables                                     */
  gboolean  boRetVal             = TRUE; /* Return value          */
  GError   *pError               = NULL;
  GVariant *pDataIn              = NULL;
  GVariant *pDataOut             = NULL;
  gint      i32ReceivedNsmReturn = -1;
  gchar    *sExpectedValue       = NULL;
  guint     u32ReceivedDataLen   = 0x00;

  /* Values read from parameter config */
  const guint u32DataLen = NSMTST__pstTestCase->unParameter.stSmGetApplicationMode.u32DataLen;

  /* Values read from return config */
  const gint                 i32ExpectedNsmReturn      = NSMTST__pstTestCase->unReturnValues.stSmGetApplicationMode.i32WrittenBytes;
  const NsmApplicationMode_e enExpectedApplicationMode = NSMTST__pstTestCase->unReturnValues.stSmGetApplicationMode.enApplicationMode;

  /* Variables need to adapt test case */
  NsmApplicationMode_e  enReceivedApplicationMode = NsmApplicationMode_NotSet; /* Value returned by NSM */
  const gchar                *sNsmValue           = "ApplicationMode";
  const NsmDataType_e         enDataType          = NsmDataType_AppMode;
  const guint                 u32RealDataLen      = sizeof(NsmApplicationMode_e);

  /* Create test case description */
  sExpectedValue =   (u32DataLen == u32RealDataLen)
                   ? g_strdup_printf("0x%02X", enExpectedApplicationMode)
                   : g_strdup_printf("%s", "-");

  NSMTST__sTestDescription = g_strdup_printf("Get %s. Interface: StateMachine. Passed DataLen: %d. Expected value: %s.",
                                             sNsmValue, u32DataLen, sExpectedValue);
  g_free(sExpectedValue);

  /* Perform test call */
  pDataIn = g_variant_new_array(G_VARIANT_TYPE_BYTE, NULL, 0);
  (void) node_state_test_call_get_nsm_data_sync(NSMTST__pNodeStateMachine,
                                                enDataType,
                                                pDataIn,
                                                u32DataLen,
                                                &pDataOut,
                                                &i32ReceivedNsmReturn,
                                                NULL,
                                                &pError);
  g_variant_unref(pDataIn);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(i32ReceivedNsmReturn == i32ExpectedNsmReturn)
    {
      /* The NSM returned with the expected value. Check if the value should be valid. */
      if(u32DataLen == u32RealDataLen)
      {
        /* We called the NSM with the correct DataLen. Check the received value. */
        u32ReceivedDataLen = g_variant_n_children(pDataOut);
        if(u32RealDataLen == u32ReceivedDataLen)
        {
          NSMTST__vGetStateMachineData(pDataOut, (guchar*) &enReceivedApplicationMode);
          if(enReceivedApplicationMode == enExpectedApplicationMode)
          {

          }
          else
          {
            boRetVal = FALSE;
            NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected %s. Received: 0x%02X. Expected: 0x%02X.",
                                                        sNsmValue, enReceivedApplicationMode, enExpectedApplicationMode);
          }
        }
        else
        {
          boRetVal = FALSE;
          NSMTST__sErrorDescription = g_strdup_printf("Did not receive data of expected length. Received: %d Byte. Expected: %d Byte.",
                                                      u32ReceivedDataLen, u32RealDataLen);
        }
      }
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  i32ReceivedNsmReturn, i32ExpectedNsmReturn);
    }

    g_variant_unref(pDataOut);
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSMC via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boSmGetNodeState(void)
{
  /* Function local variables                            */
  gboolean   boRetVal            = TRUE; /* Return value */
  GError   *pError               = NULL;
  GVariant *pDataIn              = NULL;
  GVariant *pDataOut             = NULL;
  gint      i32ReceivedNsmReturn = -1;
  gchar    *sExpectedValue       = NULL;
  guint     u32ReceivedDataLen   = 0x00;

  /* Values read from parameter config */
  const guint u32DataLen = NSMTST__pstTestCase->unParameter.stSmGetNodeState.u32DataLen;

  /* Values read from return config */
  const gint           i32ExpectedNsmReturn = NSMTST__pstTestCase->unReturnValues.stSmGetNodeState.i32WrittenBytes;
  const NsmNodeState_e enExpectedNodeState  = NSMTST__pstTestCase->unReturnValues.stSmGetNodeState.enNodeState;

  /* Variables need to adapt test case */
  NsmNodeState_e       enReceivedNodeState = NsmNodeState_NotSet; /* Value returned by NSM */
  const gchar         *sNsmValue           = "NodeState";
  const NsmDataType_e  enDataType          = NsmDataType_NodeState;
  const guint          u32RealDataLen      = sizeof(NsmNodeState_e);

  /* Create test case description */
  sExpectedValue =   (u32DataLen == sizeof(NsmNodeState_e))
                   ? g_strdup_printf("0x%02X", enExpectedNodeState)
                   : g_strdup_printf("%s", "-");

  NSMTST__sTestDescription = g_strdup_printf("Get %s. Interface: StateMachine. Passed DataLen: %d. Expected value: %s.",
                                             sNsmValue, u32DataLen, sExpectedValue);
  g_free(sExpectedValue);

  /* Perform test call */
  pDataIn = g_variant_new_array(G_VARIANT_TYPE_BYTE, NULL, 0);
  (void) node_state_test_call_get_nsm_data_sync(NSMTST__pNodeStateMachine,
                                                enDataType,
                                                pDataIn,
                                                u32DataLen,
                                                &pDataOut,
                                                &i32ReceivedNsmReturn,
                                                NULL,
                                                &pError);
  g_variant_unref(pDataIn);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(i32ReceivedNsmReturn == i32ExpectedNsmReturn)
    {
      /* The NSM returned with the expected value. Check if the value should be valid. */
      if(u32DataLen == u32RealDataLen)
      {
        /* We called the NSM with the correct DataLen. Check the received value. */
        u32ReceivedDataLen = g_variant_n_children(pDataOut);
        if(u32RealDataLen == u32ReceivedDataLen)
        {
          NSMTST__vGetStateMachineData(pDataOut, (guchar*) &enReceivedNodeState);
          if(enReceivedNodeState == enExpectedNodeState)
          {

          }
          else
          {
            boRetVal = FALSE;
            NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected %s. Received: 0x%02X. Expected: 0x%02X.",
                                                        sNsmValue, enReceivedNodeState, enExpectedNodeState);
          }
        }
        else
        {
          boRetVal = FALSE;
          NSMTST__sErrorDescription = g_strdup_printf("Did not receive data of expected length. Received: %d Byte. Expected: %d Byte.",
                                                      u32ReceivedDataLen, u32RealDataLen);
        }
      }
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  i32ReceivedNsmReturn, i32ExpectedNsmReturn);
    }

    g_variant_unref(pDataOut);
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSMC via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boSmGetRestartReason(void)
{
  /* Function local variables                                                                           */
  gboolean              boRetVal                  = TRUE;                      /* Return value          */
  GError               *pError                    = NULL;
  GVariant             *pDataIn                   = NULL;
  GVariant             *pDataOut                  = NULL;
  gint                  i32ReceivedNsmReturn      = 0x00;
  gchar                *sExpectedValue            = NULL;
  guint                 u32ReceivedDataLen        = 0x00;

  /* Values read from parameter config */
  const guint u32DataLen = NSMTST__pstTestCase->unParameter.stSmGetRestartReason.u32DataLen;

  /* Values read from return config */
  const NsmRestartReason_e enExpectedRestartReason = NSMTST__pstTestCase->unReturnValues.stSmGetRestartReason.enRestartReason;

  /* Variables need to adapt test case */
  NsmRestartReason_e   enReceivedRestartReason = NsmRestartReason_NotSet; /* Value returned by NSM */
  const gchar         *sNsmValue               = "RestartReason";
  const NsmDataType_e  enDataType              = NsmDataType_RestartReason;
  const guint          u32RealDataLen          = sizeof(NsmRestartReason_e);

  /* Create test case description */
  sExpectedValue =   (u32DataLen == u32RealDataLen)
                   ? g_strdup_printf("0x%02X", enExpectedRestartReason)
                   : g_strdup_printf("%s", "-");

  NSMTST__sTestDescription = g_strdup_printf("Get %s. Interface: StateMachine. Passed DataLen: %d. Expected value: %s.",
                                             sNsmValue, u32DataLen, sExpectedValue);
  g_free(sExpectedValue);

  /* Perform test call */
  pDataIn = g_variant_new_array(G_VARIANT_TYPE_BYTE, NULL, 0);
  (void) node_state_test_call_get_nsm_data_sync(NSMTST__pNodeStateMachine,
                                                enDataType,
                                                pDataIn,
                                                u32DataLen,
                                                &pDataOut,
                                                &i32ReceivedNsmReturn,
                                                NULL,
                                                &pError);
  g_variant_unref(pDataIn);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(i32ReceivedNsmReturn == NSMTST__pstTestCase->unReturnValues.stSmGetRestartReason.i32WrittenBytes)
    {
      /* The NSM returned with the expected value. Check if the value should be valid. */
      if(u32DataLen == u32RealDataLen)
      {
        /* We called the NSM with the correct DataLen. Check the received value. */
        u32ReceivedDataLen = g_variant_n_children(pDataOut);
        if(u32RealDataLen == u32ReceivedDataLen)
        {
          NSMTST__vGetStateMachineData(pDataOut, (guchar*) &enReceivedRestartReason);
          if(enReceivedRestartReason == enExpectedRestartReason)
          {
            boRetVal = TRUE;
          }
          else
          {
            boRetVal = FALSE;
            NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected %s. Received: 0x%02X. Expected: 0x%02X.",
                                                        sNsmValue, enReceivedRestartReason, enExpectedRestartReason);
          }
        }
        else
        {
          boRetVal = FALSE;
          NSMTST__sErrorDescription = g_strdup_printf("Did not receive data of expected length. Received: %d Byte. Expected: %d Byte.",
                                                      u32ReceivedDataLen, u32RealDataLen);
        }
      }
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  i32ReceivedNsmReturn, NSMTST__pstTestCase->unReturnValues.stSmGetRestartReason.i32WrittenBytes);
    }

    g_variant_unref(pDataOut);
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSMC via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boSmGetSessionState(void)
{
  /* Function local variables                                        */
  gboolean              boRetVal             = TRUE; /* Return value */
  GError               *pError               = NULL;
  GVariant             *pDataIn              = NULL;
  GVariant             *pDataOut             = NULL;
  gint                  i32ReceivedNsmReturn = -1;
  gchar                *sExpectedValue       = NULL;
  guint                 u32ReceivedDataLen   = 0x00;

  /* Values read from parameter config */
  const guint u32DataLen = NSMTST__pstTestCase->unParameter.stSmGetSessionState.u32DataLen;

  /* Values read from return config */
  const gint              i32ExpectedNsmReturn   = NSMTST__pstTestCase->unReturnValues.stSmGetSessionState.i32WrittenBytes;
  const NsmSessionState_e enExpectedSessionState = NSMTST__pstTestCase->unReturnValues.stSmGetSessionState.enSessionState;

  /* Variables need to adapt test case */
  NsmSession_s         stReceivedSession;                          /* Value returned by NSM */
  const gchar         *sNsmValue      = "SessionState";
  const NsmDataType_e  enDataType     = NsmDataType_SessionState;
  const guint          u32RealDataLen = sizeof(NsmSession_s);

  /* Create test case description */
  sExpectedValue =   (u32DataLen == u32RealDataLen)
                   ? g_strdup_printf("0x%02X", NSMTST__pstTestCase->unParameter.stSmGetSessionState.sSession.enState)
                   : g_strdup_printf("%s", "-");

  NSMTST__sTestDescription = g_strdup_printf("Get %s. Interface: StateMachine. Passed DataLen: %d. Expected value: %s.",
                                             sNsmValue, u32DataLen, sExpectedValue);
  g_free(sExpectedValue);

  /* Perform test call */
  pDataIn = NSMTST__pPrepareStateMachineData((guchar*) &(NSMTST__pstTestCase->unParameter.stSmGetSessionState.sSession), u32RealDataLen);
  (void) node_state_test_call_get_nsm_data_sync(NSMTST__pNodeStateMachine,
                                                enDataType,
                                                pDataIn,
                                                u32DataLen,
                                                &pDataOut,
                                                &i32ReceivedNsmReturn,
                                                NULL,
                                                &pError);
  g_variant_unref(pDataIn);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(i32ExpectedNsmReturn == i32ReceivedNsmReturn)
    {
      /* The NSM returned with the expected value. Check if the value should be valid. */
      if(u32DataLen == u32RealDataLen)
      {
        /* We called the NSM with the correct DataLen. Check the received value. */
        u32ReceivedDataLen = g_variant_n_children(pDataOut);
        if(u32RealDataLen == u32ReceivedDataLen)
        {
          NSMTST__vGetStateMachineData(pDataOut, (guchar*) &stReceivedSession);
          if(enExpectedSessionState == stReceivedSession.enState)
          {
            boRetVal = TRUE;
          }
          else
          {
            boRetVal = FALSE;
            NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected %s. Received: 0x%02X. Expected: 0x%02X.",
                                                        sNsmValue, stReceivedSession.enState, enExpectedSessionState);
          }
        }
        else
        {
          boRetVal = FALSE;
          NSMTST__sErrorDescription = g_strdup_printf("Did not receive data of expected length. Received: %d Byte. Expected: %d Byte.",
                                                      u32ReceivedDataLen, u32RealDataLen);
        }
      }
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: %d. Expected: %d.",
                                                  i32ReceivedNsmReturn, i32ExpectedNsmReturn);
    }

    g_variant_unref(pDataOut);
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSMC via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}


static gboolean NSMTST__boSmGetShutdownReason(void)
{
  /* Function local variables                            */
  gboolean    boRetVal           = TRUE; /* Return value */
  GError    *pError              = NULL;
  GVariant  *pDataIn             = NULL;
  GVariant  *pDataOut            = NULL;
  gint      i32ReceivedNsmReturn = -1;
  gchar    *sExpectedValue       = NULL;
  guint     u32ReceivedDataLen   = 0x00;

  /* Values read from parameter config */
  const guint u32DataLen = NSMTST__pstTestCase->unParameter.stSmGetShutdownReason.u32DataLen;

  /* Values read from return config */
  const gint                i32ExpectedNsmReturn     = NSMTST__pstTestCase->unReturnValues.stSmGetShutdownReason.i32WrittenBytes;
  const NsmShutdownReason_e enExpectedShutdownReason = NSMTST__pstTestCase->unReturnValues.stSmGetShutdownReason.enShutdownReason;

  /* Variables need to adapt test case */
  NsmShutdownReason_e  enReceivedShutdownReason = NsmShutdownReason_NotSet; /* Value returned by NSM */
  const gchar         *sNsmValue                = "ShutdownReason";
  const NsmDataType_e  enDataType               = NsmDataType_ShutdownReason;
  const guint          u32RealDataLen           = sizeof(NsmShutdownReason_e);

  /* Create test case description */
  sExpectedValue =   (u32DataLen == u32RealDataLen)
                   ? g_strdup_printf("0x%02X", enExpectedShutdownReason)
                   : g_strdup_printf("%s", "-");

  NSMTST__sTestDescription = g_strdup_printf("Get %s. Interface: StateMachine. Passed DataLen: %d. Expected value: %s.",
                                             sNsmValue, u32DataLen, sExpectedValue);
  g_free(sExpectedValue);

  /* Perform test call */
  pDataIn = g_variant_new_array(G_VARIANT_TYPE_BYTE, NULL, 0);
  (void) node_state_test_call_get_nsm_data_sync(NSMTST__pNodeStateMachine,
                                                enDataType,
                                                pDataIn,
                                                u32DataLen,
                                                &pDataOut,
                                                &i32ReceivedNsmReturn,
                                                NULL,
                                                &pError);
  g_variant_unref(pDataIn);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(i32ReceivedNsmReturn == i32ExpectedNsmReturn)
    {
      /* The NSM returned with the expected value. Check if the value should be valid. */
      if(u32DataLen == u32RealDataLen)
      {
        /* We called the NSM with the correct DataLen. Check the received value. */
        u32ReceivedDataLen = g_variant_n_children(pDataOut);
        if(u32RealDataLen == u32ReceivedDataLen)
        {
          NSMTST__vGetStateMachineData(pDataOut, (guchar*) &enReceivedShutdownReason);
          if(enReceivedShutdownReason == enExpectedShutdownReason)
          {
            boRetVal = TRUE;
          }
          else
          {
            boRetVal = FALSE;
            NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected %s. Received: 0x%02X. Expected: 0x%02X.",
                                                        sNsmValue, enReceivedShutdownReason, enExpectedShutdownReason);
          }
        }
        else
        {
          boRetVal = FALSE;
          NSMTST__sErrorDescription = g_strdup_printf("Did not receive data of expected length. Received: %d Byte. Expected: %d Byte.",
                                                      u32ReceivedDataLen, u32RealDataLen);
        }
      }
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  i32ReceivedNsmReturn, i32ExpectedNsmReturn);
    }

    g_variant_unref(pDataOut);
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSMC via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boSmGetBootMode(void)
{
  /* Function local variables                                                                           */
  gboolean              boRetVal                  = TRUE;                      /* Return value          */
  GError               *pError                    = NULL;
  GVariant             *pDataIn, *pDataOut        = NULL;
  gint                 i32ReceivedNsmReturn       = -1;
  gchar                *sExpectedValue            = NULL;
  guint                 u32ReceivedDataLen        = 0x00;

  /* Values read from parameter config */
  const guint u32DataLen = NSMTST__pstTestCase->unParameter.stSmGetBootMode.u32DataLen;

  /* Values read from return config */
  const gint i32ExpectedBootMode = NSMTST__pstTestCase->unReturnValues.stSmGetBootMode.i32BootMode;

  /* Variables need to adapt test case */
  gint                 i32ReceivedBootMode = 0x00; /* Value returned by NSM */
  const gchar         *sNsmValue           = "BootMode";
  const NsmDataType_e  enDataType          = NsmDataType_BootMode;
  const guint          u32RealDataLen      = sizeof(gint);

  /* Create test case description */
  sExpectedValue =   (u32DataLen == u32RealDataLen)
                   ? g_strdup_printf("0x%02X", i32ExpectedBootMode)
                   : g_strdup_printf("%s", "-");

  NSMTST__sTestDescription = g_strdup_printf("Get %s. Interface: StateMachine. Passed DataLen: %d. Expected value: %s.",
                                             sNsmValue, u32DataLen, sExpectedValue);
  g_free(sExpectedValue);

  /* Perform test call */
  pDataIn = g_variant_new_array(G_VARIANT_TYPE_BYTE, NULL, 0);
  (void) node_state_test_call_get_nsm_data_sync(NSMTST__pNodeStateMachine,
                                                enDataType,
                                                pDataIn,
                                                u32DataLen,
                                                &pDataOut,
                                                &i32ReceivedNsmReturn,
                                                NULL,
                                                &pError);
  g_variant_unref(pDataIn);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(i32ReceivedNsmReturn == NSMTST__pstTestCase->unReturnValues.stSmGetBootMode.i32WrittenBytes)
    {
      /* The NSM returned with the expected value. Check if the value should be valid. */
      if(u32DataLen == u32RealDataLen)
      {
        /* We called the NSM with the correct DataLen. Check the received value. */
        u32ReceivedDataLen = g_variant_n_children(pDataOut);
        if(u32RealDataLen == u32ReceivedDataLen)
        {
          NSMTST__vGetStateMachineData(pDataOut, (guchar*) &i32ReceivedBootMode);
          if(i32ReceivedBootMode == i32ExpectedBootMode)
          {
            boRetVal = TRUE;
          }
          else
          {
            boRetVal = FALSE;
            NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected %s. Received: 0x%02X. Expected: 0x%02X.",
                                                        sNsmValue, i32ReceivedBootMode, i32ExpectedBootMode);
          }
        }
        else
        {
          boRetVal = FALSE;
          NSMTST__sErrorDescription = g_strdup_printf("Did not receive data of expected length. Received: %d Byte. Expected: %d Byte.",
                                                      u32ReceivedDataLen, u32RealDataLen);
        }
      }
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  i32ReceivedNsmReturn, NSMTST__pstTestCase->unReturnValues.stSmGetBootMode.i32WrittenBytes);
    }

    g_variant_unref(pDataOut);
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSMC via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boSmGetRunningReason(void)
{
  /* Function local variables                             */
  gboolean   boRetVal             = TRUE; /* Return value */
  GError    *pError               = NULL;
  GVariant  *pDataIn              = NULL;
  GVariant  *pDataOut             = NULL;
  gint       i32ReceivedNsmReturn = -1;
  gchar     *sExpectedValue       = NULL;
  guint      u32ReceivedDataLen   = 0x00;

  /* Values read from parameter config */
  const guint u32DataLen = NSMTST__pstTestCase->unParameter.stSmGetRunningReason.u32DataLen;

  /* Values read from return config */
  const NsmRunningReason_e enExpectedRunningReason = NSMTST__pstTestCase->unReturnValues.stSmGetRunningReason.enRunningReason;

  /* Variables need to adapt test case */
  NsmRunningReason_e   enReceivedRunningReason = NsmRunningReason_NotSet; /* Value returned by NSM */
  const gchar         *sNsmValue               = "RunningReason";
  const NsmDataType_e  enDataType              = NsmDataType_RunningReason;
  const guint          u32RealDataLen          = sizeof(NsmRunningReason_e);

  /* Create test case description */
  sExpectedValue =   (u32DataLen == u32RealDataLen)
                   ? g_strdup_printf("0x%02X", enExpectedRunningReason)
                   : g_strdup_printf("%s", "-");

  NSMTST__sTestDescription = g_strdup_printf("Get %s. Interface: StateMachine. Passed DataLen: %d. Expected value: %s.",
                                             sNsmValue, u32DataLen, sExpectedValue);
  g_free(sExpectedValue);

  /* Perform test call */
  pDataIn = g_variant_new_array(G_VARIANT_TYPE_BYTE, NULL, 0);
  (void) node_state_test_call_get_nsm_data_sync(NSMTST__pNodeStateMachine,
                                                enDataType,
                                                pDataIn,
                                                u32DataLen,
                                                &pDataOut,
                                                &i32ReceivedNsmReturn,
                                                NULL,
                                                &pError);
  g_variant_unref(pDataIn);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(i32ReceivedNsmReturn == NSMTST__pstTestCase->unReturnValues.stSmGetRunningReason.i32WrittenBytes)
    {
      /* The NSM returned with the expected value. Check if the value should be valid. */
      if(u32DataLen == u32RealDataLen)
      {
        /* We called the NSM with the correct DataLen. Check the received value. */
        u32ReceivedDataLen = g_variant_n_children(pDataOut);
        if(u32RealDataLen == u32ReceivedDataLen)
        {
          NSMTST__vGetStateMachineData(pDataOut, (guchar*) &enReceivedRunningReason);
          if(enReceivedRunningReason == enExpectedRunningReason)
          {
            boRetVal = TRUE;
          }
          else
          {
            boRetVal = FALSE;
            NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected %s. Received: 0x%02X. Expected: 0x%02X.",
                                                        sNsmValue, enReceivedRunningReason, enExpectedRunningReason);
          }
        }
        else
        {
          boRetVal = FALSE;
          NSMTST__sErrorDescription = g_strdup_printf("Did not receive data of expected length. Received: %d Byte. Expected: %d Byte.",
                                                      u32ReceivedDataLen, u32RealDataLen);
        }
      }
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: %d. Expected: %d.",
                                                  i32ReceivedNsmReturn, NSMTST__pstTestCase->unReturnValues.stSmGetRunningReason.i32WrittenBytes);
    }

    g_variant_unref(pDataOut);
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSMC via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}



static gboolean NSMTST__boSmSetApplicationMode(void)
{
  /* Function local variables                                                                           */
  gboolean              boRetVal                  = TRUE;                      /* Return value          */
  GError               *pError                    = NULL;
  GVariant             *pDataIn                   = NULL;
  NsmErrorStatus_e      enReceivedNsmReturn       = NsmErrorStatus_NotSet;

  /* Values read from parameter config */
  const guint                u32DataLen        = NSMTST__pstTestCase->unParameter.stSmSetApplicationMode.u32DataLen;
  const NsmApplicationMode_e enApplicationMode = NSMTST__pstTestCase->unParameter.stSmSetApplicationMode.enApplicationMode;

  /* Values read from return config */
  const NsmErrorStatus_e enExpectedNsmReturn = NSMTST__pstTestCase->unReturnValues.stSmSetApplicationMode.enErrorStatus;

  /* Variables need to adapt test case */
  const gchar         *sNsmValue      = "ApplicationMode";
  const NsmDataType_e  enDataType     = NsmDataType_AppMode;
  const guint          u32RealDataLen = sizeof(NsmApplicationMode_e);

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Set %s. Interface: StateMachine. Passed DataLen: %d.",
                                             sNsmValue, u32DataLen);

  /* Perform test call */
  pDataIn = NSMTST__pPrepareStateMachineData((guchar*) &enApplicationMode, u32RealDataLen);
  (void) node_state_test_call_set_nsm_data_sync(NSMTST__pNodeStateMachine,
                                                enDataType,
                                                pDataIn,
                                                u32DataLen,
                                                (gint*) &enReceivedNsmReturn,
                                                NULL,
                                                &pError);

  g_variant_unref(pDataIn);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(enReceivedNsmReturn == enExpectedNsmReturn)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  enReceivedNsmReturn, enExpectedNsmReturn);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSMC via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boSmSetNodeState(void)
{
  /* Function local variables                                   */
  gboolean          boRetVal            = TRUE; /* Return value */
  GError           *pError              = NULL;
  GVariant         *pDataIn             = NULL;
  NsmErrorStatus_e  enReceivedNsmReturn = NsmErrorStatus_NotSet;

  /* Values read from parameter config */
  const guint          u32DataLen  = NSMTST__pstTestCase->unParameter.stSmSetNodeState.u32DataLen;
  const NsmNodeState_e enNodeState = NSMTST__pstTestCase->unParameter.stSmSetNodeState.enNodeState;

  /* Values read from return config */
  const NsmErrorStatus_e enExpectedNsmReturn = NSMTST__pstTestCase->unReturnValues.stSmSetNodeState.enErrorStatus;

  /* Variables need to adapt test case */
  const gchar         *sNsmValue      = "NodeState";
  const NsmDataType_e  enDataType     = NsmDataType_NodeState;
  const guint          u32RealDataLen = sizeof(NsmNodeState_e);

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Set %s. Interface: StateMachine. Passed DataLen: %d.",
                                             sNsmValue, u32DataLen);

  /* Perform test call */
  pDataIn = NSMTST__pPrepareStateMachineData((guchar*) &enNodeState, u32RealDataLen);
  (void) node_state_test_call_set_nsm_data_sync(NSMTST__pNodeStateMachine,
                                                enDataType,
                                                pDataIn,
                                                u32DataLen,
                                                (gint*) &enReceivedNsmReturn,
                                                NULL,
                                                &pError);
  g_variant_unref(pDataIn);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(enReceivedNsmReturn == enExpectedNsmReturn)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  enReceivedNsmReturn, enExpectedNsmReturn);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSMC via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}


static gboolean NSMTST__boSmSetSessionState(void)
{
  /* Function local variables                                   */
  gboolean          boRetVal            = TRUE; /* Return value */
  GError           *pError              = NULL;
  GVariant         *pDataIn             = NULL;
  NsmErrorStatus_e  enReceivedNsmReturn = NsmErrorStatus_NotSet;

  /* Values read from parameter config */
  const guint        u32DataLen = NSMTST__pstTestCase->unParameter.stSmSetSessionState.u32DataLen;
  const NsmSession_s stSession  = NSMTST__pstTestCase->unParameter.stSmSetSessionState.stSession;

  /* Values read from return config */
  const NsmErrorStatus_e enExpectedNsmReturn = NSMTST__pstTestCase->unReturnValues.stSmSetSessionState.enErrorStatus;

  /* Variables need to adapt test case */
  const gchar         *sNsmValue      = "SessionState";
  const NsmDataType_e  enDataType     = NsmDataType_SessionState;
  const guint          u32RealDataLen = sizeof(NsmSession_s);

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Set %s. Interface: StateMachine. Passed DataLen: %d.",
                                             sNsmValue, u32DataLen);
  /* Perform test call */
  pDataIn = NSMTST__pPrepareStateMachineData((guchar*) &stSession, u32RealDataLen);
  (void) node_state_test_call_set_nsm_data_sync(NSMTST__pNodeStateMachine,
                                                enDataType,
                                                pDataIn,
                                                u32DataLen,
                                                (gint*) &enReceivedNsmReturn,
                                                NULL,
                                                &pError);
  g_variant_unref(pDataIn);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(enReceivedNsmReturn == enExpectedNsmReturn)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  enReceivedNsmReturn, enExpectedNsmReturn);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSMC via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}


static gboolean NSMTST__boSmRegisterSession(void)
{
  /* Function local variables                                   */
  gboolean          boRetVal            = TRUE; /* Return value */
  GError           *pError              = NULL;
  GVariant         *pDataIn             = NULL;
  NsmErrorStatus_e  enReceivedNsmReturn = NsmErrorStatus_NotSet;

  /* Values read from parameter config */
  const guint        u32DataLen = NSMTST__pstTestCase->unParameter.stSmRegisterSession.u32DataLen;
  const NsmSession_s stSession  = NSMTST__pstTestCase->unParameter.stSmRegisterSession.stSession;

  /* Values read from return config */
  const NsmErrorStatus_e enExpectedNsmReturn = NSMTST__pstTestCase->unReturnValues.stSmRegisterSession.enErrorStatus;

  /* Variables need to adapt test case */
  const gchar         *sNsmValue      = "RegisterSession";
  const NsmDataType_e  enDataType     = NsmDataType_RegisterSession;
  const guint          u32RealDataLen = sizeof(NsmSession_s);

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Set %s. Interface: StateMachine. Passed DataLen: %d.",
											 sNsmValue, u32DataLen);
  /* Perform test call */
  pDataIn = NSMTST__pPrepareStateMachineData((guchar*) &stSession, u32RealDataLen);
  (void) node_state_test_call_set_nsm_data_sync(NSMTST__pNodeStateMachine,
												enDataType,
												pDataIn,
												u32DataLen,
												(gint*) &enReceivedNsmReturn,
												NULL,
												&pError);
  g_variant_unref(pDataIn);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
	/* D-Bus communication successful. Check if NSM returned with the expected value. */
	if(enReceivedNsmReturn == enExpectedNsmReturn)
	{
	  boRetVal = TRUE;
	}
	else
	{
	  boRetVal = FALSE;
	  NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
												  enReceivedNsmReturn, enExpectedNsmReturn);
	}
  }
  else
  {
	boRetVal = FALSE;
	NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSMC via D-Bus. Error msg.: %s.",
												pError->message);
	g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boSmUnRegisterSession(void)
{
  /* Function local variables                                   */
  gboolean          boRetVal            = TRUE; /* Return value */
  GError           *pError              = NULL;
  GVariant         *pDataIn             = NULL;
  NsmErrorStatus_e  enReceivedNsmReturn = NsmErrorStatus_NotSet;

  /* Values read from parameter config */
  const guint        u32DataLen = NSMTST__pstTestCase->unParameter.stSmUnRegisterSession.u32DataLen;
  const NsmSession_s stSession  = NSMTST__pstTestCase->unParameter.stSmUnRegisterSession.stSession;

  /* Values read from return config */
  const NsmErrorStatus_e enExpectedNsmReturn = NSMTST__pstTestCase->unReturnValues.stSmUnRegisterSession.enErrorStatus;

  /* Variables need to adapt test case */
  const gchar         *sNsmValue      = "UnRegisterSession";
  const NsmDataType_e  enDataType     = NsmDataType_UnRegisterSession;
  const guint          u32RealDataLen = sizeof(NsmSession_s);

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Set %s. Interface: StateMachine. Passed DataLen: %d.",
											 sNsmValue, u32DataLen);
  /* Perform test call */
  pDataIn = NSMTST__pPrepareStateMachineData((guchar*) &stSession, u32RealDataLen);
  (void) node_state_test_call_set_nsm_data_sync(NSMTST__pNodeStateMachine,
												enDataType,
												pDataIn,
												u32DataLen,
												(gint*) &enReceivedNsmReturn,
												NULL,
												&pError);
  g_variant_unref(pDataIn);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
	/* D-Bus communication successful. Check if NSM returned with the expected value. */
	if(enReceivedNsmReturn == enExpectedNsmReturn)
	{
	  boRetVal = TRUE;
	}
	else
	{
	  boRetVal = FALSE;
	  NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
												  enReceivedNsmReturn, enExpectedNsmReturn);
	}
  }
  else
  {
	boRetVal = FALSE;
	NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSMC via D-Bus. Error msg.: %s.",
												pError->message);
	g_error_free(pError);
  }

  return boRetVal;
}


static gboolean NSMTST__boSmSetShutdownReason(void)
{
  /* Function local variables                                   */
  gboolean          boRetVal            = TRUE; /* Return value */
  GError           *pError              = NULL;
  GVariant         *pDataIn             = NULL;
  NsmErrorStatus_e  enReceivedNsmReturn = NsmErrorStatus_NotSet;

  /* Values read from parameter config */
  const guint               u32DataLen       = NSMTST__pstTestCase->unParameter.stSmSetShutdownReason.u32DataLen;
  const NsmShutdownReason_e enShutdownReason = NSMTST__pstTestCase->unParameter.stSmSetShutdownReason.enShutdownReason;

  /* Values read from return config */
  const NsmErrorStatus_e enExpectedNsmReturn = NSMTST__pstTestCase->unReturnValues.stSmSetShutdownReason.enErrorStatus;

  /* Variables need to adapt test case */
  const gchar         *sNsmValue      = "ShutdownReason";
  const NsmDataType_e  enDataType     = NsmDataType_ShutdownReason;
  const guint          u32RealDataLen = sizeof(NsmShutdownReason_e);

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Set %s. Interface: StateMachine. Passed DataLen: %d.",
                                             sNsmValue, u32DataLen);

  /* Perform test call */
  pDataIn = NSMTST__pPrepareStateMachineData((guchar*) &enShutdownReason, u32RealDataLen);
  (void) node_state_test_call_set_nsm_data_sync(NSMTST__pNodeStateMachine,
                                                enDataType,
                                                pDataIn,
                                                u32DataLen,
                                                (gint*) &enReceivedNsmReturn,
                                                NULL,
                                                &pError);
  g_variant_unref(pDataIn);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(enReceivedNsmReturn == enExpectedNsmReturn)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  enReceivedNsmReturn, enExpectedNsmReturn);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSMC via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boSmSetBootMode(void)
{
  /* Function local variables                                   */
  gboolean          boRetVal            = TRUE; /* Return value */
  GError           *pError              = NULL;
  GVariant         *pDataIn             = NULL;
  NsmErrorStatus_e  enReceivedNsmReturn = NsmErrorStatus_NotSet;

  /* Values read from parameter config */
  const guint u32DataLen  = NSMTST__pstTestCase->unParameter.stSmSetBootMode.u32DataLen;
  const gint  i32BootMode = NSMTST__pstTestCase->unParameter.stSmSetBootMode.i32BootMode;

  /* Values read from return config */
  const NsmErrorStatus_e enExpectedNsmReturn = NSMTST__pstTestCase->unReturnValues.stSmSetBootMode.enErrorStatus;

  /* Variables need to adapt test case */
  const gchar         *sNsmValue      = "BootMode";
  const NsmDataType_e  enDataType     = NsmDataType_BootMode;
  const guint          u32RealDataLen = sizeof(NsmDataType_BootMode);

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Set %s. Interface: StateMachine. Passed DataLen: %d.",
                                             sNsmValue, u32DataLen);

  /* Perform test call */
  pDataIn = NSMTST__pPrepareStateMachineData((guchar*) &i32BootMode, u32RealDataLen);
  (void) node_state_test_call_set_nsm_data_sync(NSMTST__pNodeStateMachine,
                                                enDataType,
                                                pDataIn,
                                                u32DataLen,
                                                (gint*) &enReceivedNsmReturn,
                                                NULL,
                                                &pError);
  g_variant_unref(pDataIn);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(enReceivedNsmReturn == enExpectedNsmReturn)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  enReceivedNsmReturn, enExpectedNsmReturn);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSMC via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boDbSetApplicationMode(void)
{
  /* Function local variables                                   */
  gboolean          boRetVal            = TRUE; /* Return value */
  GError           *pError              = NULL;
  NsmErrorStatus_e  enReceivedNsmReturn = NsmErrorStatus_NotSet;

  /* Values read from config */
  const NsmApplicationMode_e enApplicationMode   = NSMTST__pstTestCase->unParameter.stDbSetApplicationMode.enApplicationMode;
  const NsmErrorStatus_e     enExpectedNsmReturn = NSMTST__pstTestCase->unReturnValues.stDbSetApplicationMode.enErrorStatus;

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Set ApplicationMode. Interface: D-Bus. Value: 0x%02X.", enApplicationMode);

  /* Perform test call */
  (void) node_state_lifecycle_control_call_set_application_mode_sync(NSMTST__pLifecycleControl,
                                                                     (gint) enApplicationMode,
                                                                     (gint*) &enReceivedNsmReturn,
                                                                     NULL,
                                                                     &pError);
  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(enReceivedNsmReturn == enExpectedNsmReturn)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  enReceivedNsmReturn, enExpectedNsmReturn);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSM via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boDbSetBootMode(void)
{
  /* Function local variables                                   */
  gboolean          boRetVal            = TRUE; /* Return value */
  GError           *pError              = NULL;
  NsmErrorStatus_e  enReceivedNsmReturn = NsmErrorStatus_NotSet;

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Set BootMode. Interface: D-Bus. Value: 0x%02X.",
                                             NSMTST__pstTestCase->unParameter.stDbSetBootMode.i32BootMode);

  /* Perform test call */
  (void) node_state_lifecycle_control_call_set_boot_mode_sync(NSMTST__pLifecycleControl,
                                                              (gint)  NSMTST__pstTestCase->unParameter.stDbSetBootMode.i32BootMode,
                                                              (gint*) &enReceivedNsmReturn,
                                                              NULL,
                                                              &pError);
  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(enReceivedNsmReturn == NSMTST__pstTestCase->unReturnValues.stDbSetBootMode.enErrorStatus)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  enReceivedNsmReturn, NSMTST__pstTestCase->unReturnValues.stDbSetBootMode.enErrorStatus);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSM via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}



static gboolean NSMTST__boDbSetNodeState(void)
{
  /* Function local variables                                   */
  gboolean          boRetVal            = TRUE; /* Return value */
  GError           *pError              = NULL;
  NsmErrorStatus_e  enReceivedNsmReturn = NsmErrorStatus_NotSet;

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Set NodeState. Interface: D-Bus. Value: 0x%02X.",
                                             NSMTST__pstTestCase->unParameter.stDbSetNodeState.enNodeState);

  /* Perform test call */
  (void) node_state_lifecycle_control_call_set_node_state_sync(NSMTST__pLifecycleControl,
                                                               (gint) NSMTST__pstTestCase->unParameter.stDbSetNodeState.enNodeState,
                                                               (gint*) &enReceivedNsmReturn,
                                                               NULL,
                                                               &pError);
  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(enReceivedNsmReturn == NSMTST__pstTestCase->unReturnValues.stDbSetNodeState.enErrorStatus)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  enReceivedNsmReturn, NSMTST__pstTestCase->unReturnValues.stDbSetNodeState.enErrorStatus);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSM via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}


static gboolean NSMTST__boDbSetSessionState(void)
{
  /* Function local variables                                   */
  gboolean          boRetVal            = TRUE; /* Return value */
  GError           *pError              = NULL;
  NsmErrorStatus_e  enReceivedNsmReturn = NsmErrorStatus_NotSet;

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Set SessionState. Interface: D-Bus. Value: (SessionName: %s. SessionOwner: %s. Seat: 0x%02X. State: 0x%02X.).",
                                             NSMTST__pstTestCase->unParameter.stDbSetSessionState.sSessionName,
                                             NSMTST__pstTestCase->unParameter.stDbSetSessionState.sSessionOwner,
                                             NSMTST__pstTestCase->unParameter.stDbSetSessionState.enSeat,
                                             NSMTST__pstTestCase->unParameter.stDbSetSessionState.enState);

  /* Perform test call */
  (void) node_state_consumer_call_set_session_state_sync(NSMTST__pNodeStateConsumer,
                                                         (gchar*) NSMTST__pstTestCase->unParameter.stDbSetSessionState.sSessionName,
                                                         (gchar*) NSMTST__pstTestCase->unParameter.stDbSetSessionState.sSessionOwner,
                                                         (gint)   NSMTST__pstTestCase->unParameter.stDbSetSessionState.enSeat,
                                                         (gint)   NSMTST__pstTestCase->unParameter.stDbSetSessionState.enState,
                                                         (gint*) &enReceivedNsmReturn,
                                                         NULL,
                                                         &pError);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(enReceivedNsmReturn == NSMTST__pstTestCase->unReturnValues.stDbSetSessionState.enErrorStatus)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  enReceivedNsmReturn, NSMTST__pstTestCase->unReturnValues.stDbSetSessionState.enErrorStatus);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSM via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}



/*********************************************** Get simple value via D-Bus ******************************************/

static gboolean NSMTST__boDbGetApplicationMode(void)
{
  /* Function local variables                                             */
  gboolean              boRetVal                  = TRUE; /* Return value */
  GError               *pError                    = NULL;
  NsmErrorStatus_e      enReceivedNsmReturn       = NsmErrorStatus_NotSet;
  NsmApplicationMode_e  enReceivedApplicationMode = NsmApplicationMode_NotSet;


  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Get ApplicationMode. Interface: D-Bus. Expected value: 0x%02X.",
                                             NSMTST__pstTestCase->unReturnValues.stDbGetApplicationMode.enApplicationMode);

  /* Perform test call */
  (void) node_state_consumer_call_get_application_mode_sync(NSMTST__pNodeStateConsumer,
                                                            (gint*) &enReceivedApplicationMode,
                                                            (gint*) &enReceivedNsmReturn,
                                                            NULL,
                                                            &pError);
  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(enReceivedNsmReturn == NSMTST__pstTestCase->unReturnValues.stDbGetApplicationMode.enErrorStatus)
    {
      if(enReceivedApplicationMode == NSMTST__pstTestCase->unReturnValues.stDbGetApplicationMode.enApplicationMode)
      {
        boRetVal = TRUE;
      }
      else
      {
        boRetVal = FALSE;
        NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected ApplicationMode. Received: 0x%02X. Expected: 0x%02X.",
                                                    enReceivedApplicationMode, NSMTST__pstTestCase->unReturnValues.stDbGetApplicationMode.enApplicationMode);
      }
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  enReceivedNsmReturn, NSMTST__pstTestCase->unReturnValues.stDbGetApplicationMode.enErrorStatus);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSM via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boDbGetNodeState(void)
{
  /* Function local variables                                   */
  gboolean          boRetVal            = TRUE; /* Return value */
  GError           *pError              = NULL;
  NsmErrorStatus_e  enReceivedNsmReturn = NsmErrorStatus_NotSet;
  NsmNodeState_e    enReceivedNodeState = NsmApplicationMode_NotSet;

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Get NodeState. Interface: D-Bus. Expected value: 0x%02X.",
                                             NSMTST__pstTestCase->unReturnValues.stDbGetNodeState.enNodeState);

  /* Perform test call */
  (void) node_state_consumer_call_get_node_state_sync(NSMTST__pNodeStateConsumer,
                                                      (gint*) &enReceivedNodeState,
                                                      (gint*) &enReceivedNsmReturn,
                                                      NULL,
                                                      &pError);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(enReceivedNsmReturn == NSMTST__pstTestCase->unReturnValues.stDbGetNodeState.enErrorStatus)
    {
      if(enReceivedNodeState == NSMTST__pstTestCase->unReturnValues.stDbGetNodeState.enNodeState)
      {
        boRetVal = TRUE;
      }
      else
      {
        boRetVal = FALSE;
        NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NodeState. Received: 0x%02X. Expected: 0x%02X.",
                                                    enReceivedNodeState, NSMTST__pstTestCase->unReturnValues.stDbGetNodeState.enNodeState);
      }
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  enReceivedNsmReturn, NSMTST__pstTestCase->unReturnValues.stDbGetNodeState.enNodeState);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSM via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boDbGetSessionState(void)
{
  /* Function local variables                                                                     */
  gboolean           boRetVal               = TRUE;                      /* Return value          */
  GError            *pError                 = NULL;
  NsmErrorStatus_e   enReceivedNsmReturn    = NsmErrorStatus_NotSet;
  NsmSessionState_e  enReceivedSessionState = NsmSessionState_Unregistered;

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Get SessionState. Interface: D-Bus.");

  /* Perform test call */
  (void) node_state_consumer_call_get_session_state_sync(NSMTST__pNodeStateConsumer,
                                                         (gchar*) NSMTST__pstTestCase->unParameter.stDbGetSessionState.sSessionName,
                                                         (gint)   NSMTST__pstTestCase->unParameter.stDbGetSessionState.enSeat,
                                                         (gint*) &enReceivedSessionState,
                                                         (gint*) &enReceivedNsmReturn,
                                                         NULL,
                                                         &pError);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(enReceivedNsmReturn == NSMTST__pstTestCase->unReturnValues.stDbGetSessionState.enErrorStatus)
    {
      if(enReceivedSessionState == NSMTST__pstTestCase->unReturnValues.stDbGetSessionState.enSessionState)
      {
        boRetVal = TRUE;
      }
      else
      {
        boRetVal = FALSE;
        NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NodeState. Received: 0x%02X. Expected: 0x%02X.",
                                                    enReceivedSessionState, NSMTST__pstTestCase->unReturnValues.stDbGetSessionState.enSessionState);
      }
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  enReceivedNsmReturn, NSMTST__pstTestCase->unReturnValues.stDbGetSessionState.enErrorStatus);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSM via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boDbGetAppHealthCount(void)
{
  /* Function local variables                                          */
  gboolean           boRetVal                  = TRUE; /* Return value */
  GError            *pError                    = NULL;
  guint              u32ReceivedAppHealthCount = 0x00;

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Get AppHealthCount. Interface: D-Bus.");

  /* Perform test call */
  (void) node_state_consumer_call_get_app_health_count_sync(NSMTST__pNodeStateConsumer,
                                                            &u32ReceivedAppHealthCount,
                                                            NULL,
                                                            &pError);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(u32ReceivedAppHealthCount == NSMTST__pstTestCase->unReturnValues.stDbGetAppHealthCount.u32AppHealthCount)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected AppHealthCount. Received: %d. Expected: %d.",
                                                  u32ReceivedAppHealthCount, NSMTST__pstTestCase->unReturnValues.stDbGetAppHealthCount.u32AppHealthCount);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to access NSM via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boDbGetInterfaceVersion(void)
{
  /* Function local variables                                            */
  gboolean           boRetVal                    = TRUE; /* Return value */
  GError            *pError                      = NULL;
  guint              u32ReceivedInterfaceVersion = 0x00;

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Get InterfaceVersion. Interface: D-Bus.");

  /* Perform test call */
  (void) node_state_consumer_call_get_interface_version_sync(NSMTST__pNodeStateConsumer,
                                                             &u32ReceivedInterfaceVersion,
                                                             NULL,
                                                             &pError);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(u32ReceivedInterfaceVersion == NSMTST__pstTestCase->unReturnValues.stDbGetInterfaceVersion.u32InterfaceVersion)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected InterfaceVersion. Received: %i. Expected: %i.",
                                                  u32ReceivedInterfaceVersion,
                                                  NSMTST__pstTestCase->unReturnValues.stDbGetInterfaceVersion.u32InterfaceVersion);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to access NSM via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}


/*********************************************** Call methods via D-Bus *********************************************/

static gboolean NSMTST__boDbRegisterSession(void)
{
  /* Function local variables                                   */
  gboolean          boRetVal            = TRUE; /* Return value */
  GError           *pError              = NULL;
  NsmErrorStatus_e  enReceivedNsmReturn = NsmErrorStatus_NotSet;

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Register Session. Interface: D-Bus. Value: (SessionName: %s. SessionOwner: %s. Seat: 0x%02X. State: 0x%02X.).",
                                             NSMTST__pstTestCase->unParameter.stDbRegisterSession.sSessionName,
                                             NSMTST__pstTestCase->unParameter.stDbRegisterSession.sSessionOwner,
                                             NSMTST__pstTestCase->unParameter.stDbRegisterSession.enSeat,
                                             NSMTST__pstTestCase->unParameter.stDbRegisterSession.enState);

  /* Perform test call */
  (void) node_state_consumer_call_register_session_sync(NSMTST__pNodeStateConsumer,
                                                        (gchar*) NSMTST__pstTestCase->unParameter.stDbRegisterSession.sSessionName,
                                                        (gchar*) NSMTST__pstTestCase->unParameter.stDbRegisterSession.sSessionOwner,
                                                        (gint)   NSMTST__pstTestCase->unParameter.stDbRegisterSession.enSeat,
                                                        (gint)   NSMTST__pstTestCase->unParameter.stDbRegisterSession.enState,
                                                        (gint*) &enReceivedNsmReturn,
                                                        NULL,
                                                        &pError);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(enReceivedNsmReturn == NSMTST__pstTestCase->unReturnValues.stDbRegisterSession.enErrorStatus)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  enReceivedNsmReturn, NSMTST__pstTestCase->unReturnValues.stDbRegisterSession.enErrorStatus);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSM via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boDbUnRegisterSession(void)
{
  /* Function local variables                                       */
  gboolean              boRetVal            = TRUE; /* Return value */
  GError               *pError              = NULL;
  NsmErrorStatus_e      enReceivedNsmReturn = NsmErrorStatus_NotSet;

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Unregister Session. Interface: D-Bus. Value: (SessionName: %s. SessionOwner: %s. Seat: 0x%02X.).",
                                             NSMTST__pstTestCase->unParameter.stDbUnRegisterSession.sSessionName,
                                             NSMTST__pstTestCase->unParameter.stDbUnRegisterSession.sSessionOwner,
                                             NSMTST__pstTestCase->unParameter.stDbUnRegisterSession.enSeat);

  /* Perform test call */
  (void) node_state_consumer_call_un_register_session_sync(NSMTST__pNodeStateConsumer,
                                                           (gchar*) NSMTST__pstTestCase->unParameter.stDbUnRegisterSession.sSessionName,
                                                           (gchar*) NSMTST__pstTestCase->unParameter.stDbUnRegisterSession.sSessionOwner,
                                                           (gint)   NSMTST__pstTestCase->unParameter.stDbUnRegisterSession.enSeat,
                                                           (gint*) &enReceivedNsmReturn,
                                                           NULL,
                                                           &pError);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(enReceivedNsmReturn == NSMTST__pstTestCase->unReturnValues.stDbUnRegisterSession.enErrorStatus)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  enReceivedNsmReturn, NSMTST__pstTestCase->unReturnValues.stDbUnRegisterSession.enErrorStatus);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSM via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boDbRegisterShutdownClient(void)
{
  /* Function local variables                                       */
  gboolean              boRetVal            = TRUE; /* Return value */
  GError               *pError              = NULL;
  NsmErrorStatus_e      enReceivedNsmReturn = NsmErrorStatus_NotSet;

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Register shutdown client. Interface: D-Bus. Value: (BusName: %s. ObjName: %s. Mode: 0x%04X. Timeout: %d.).",
                                             NSMTST__sBusName,
                                             NSMTST__pstTestCase->unParameter.stDbRegisterShutdownClient.sObjName,
                                             NSMTST__pstTestCase->unParameter.stDbRegisterShutdownClient.u32Mode,
                                             NSMTST__pstTestCase->unParameter.stDbRegisterShutdownClient.u32Timeout);

  /* Perform test call */
  (void) node_state_consumer_call_register_shutdown_client_sync(NSMTST__pNodeStateConsumer,
                                                                NSMTST__sBusName,
                                                                NSMTST__pstTestCase->unParameter.stDbRegisterShutdownClient.sObjName,
                                                                NSMTST__pstTestCase->unParameter.stDbRegisterShutdownClient.u32Mode,
                                                                NSMTST__pstTestCase->unParameter.stDbRegisterShutdownClient.u32Timeout,
                                                                (gint*) &enReceivedNsmReturn,
                                                                NULL,
                                                                &pError);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(enReceivedNsmReturn == NSMTST__pstTestCase->unReturnValues.stDbRegisterShutdownClient.enErrorStatus)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  enReceivedNsmReturn, NSMTST__pstTestCase->unReturnValues.stDbRegisterShutdownClient.enErrorStatus);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSM via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boDbUnRegisterShutdownClient(void)
{
  /* Function local variables                                       */
  gboolean              boRetVal            = TRUE; /* Return value */
  GError               *pError              = NULL;
  NsmErrorStatus_e      enReceivedNsmReturn = NsmErrorStatus_NotSet;

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Unregister shutdown client. Interface: D-Bus. Value: (BusName: %s. ObjName: %s. Mode: 0x%04X.).",
                                             NSMTST__sBusName,
                                             NSMTST__pstTestCase->unParameter.stDbUnRegisterShutdownClient.sObjName,
                                             NSMTST__pstTestCase->unParameter.stDbUnRegisterShutdownClient.u32Mode);

  /* Perform test call */
  (void) node_state_consumer_call_un_register_shutdown_client_sync(NSMTST__pNodeStateConsumer,
                                                                   NSMTST__sBusName,
                                                                   NSMTST__pstTestCase->unParameter.stDbUnRegisterShutdownClient.sObjName,
                                                                   NSMTST__pstTestCase->unParameter.stDbUnRegisterShutdownClient.u32Mode,
                                                                   (gint*) &enReceivedNsmReturn,
                                                                   NULL,
                                                                   &pError);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(enReceivedNsmReturn == NSMTST__pstTestCase->unReturnValues.stDbUnRegisterShutdownClient.enErrorStatus)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  enReceivedNsmReturn, NSMTST__pstTestCase->unReturnValues.stDbUnRegisterShutdownClient.enErrorStatus);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSM via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}



static gboolean NSMTST__boDbRequestNodeRestart(void)
{
  /* Function local variables                                       */
  gboolean              boRetVal            = TRUE; /* Return value */
  GError               *pError              = NULL;
  NsmErrorStatus_e      enReceivedNsmReturn = NsmErrorStatus_NotSet;

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Request Node Restart. Interface: D-Bus. Value: (RestartReason: 0x%02X. RestartType: 0x%04X.).",
                                             NSMTST__pstTestCase->unParameter.stDbRequestNodeRestart.enRestartReason,
                                             NSMTST__pstTestCase->unParameter.stDbRequestNodeRestart.u32RestartType);

  /* Perform test call */
  (void) node_state_lifecycle_control_call_request_node_restart_sync(NSMTST__pLifecycleControl,
                                                                   NSMTST__pstTestCase->unParameter.stDbRequestNodeRestart.enRestartReason,
                                                                   NSMTST__pstTestCase->unParameter.stDbRequestNodeRestart.u32RestartType,
                                                                   (gint*) &enReceivedNsmReturn,
                                                                   NULL,
                                                                   &pError);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(enReceivedNsmReturn == NSMTST__pstTestCase->unReturnValues.stDbRequestNodeRestart.enErrorStatus)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  enReceivedNsmReturn, NSMTST__pstTestCase->unReturnValues.stDbRequestNodeRestart.enErrorStatus);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSM via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boDbSetAppHealthStatus(void)
{
  /* Function local variables                                       */
  gboolean              boRetVal            = TRUE; /* Return value */
  GError               *pError              = NULL;
  NsmErrorStatus_e      enReceivedNsmReturn = NsmErrorStatus_NotSet;

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Set AppHealthStatus. Interface: D-Bus. Value: (AppName: %s. AppRunning: %s.).",
                                             NSMTST__pstTestCase->unParameter.stDbSetAppHealthStatus.sAppName,
                                             (NSMTST__pstTestCase->unParameter.stDbSetAppHealthStatus.boAppRunning == TRUE) ? "true" : "false");

  /* Perform test call */
  (void) node_state_lifecycle_control_call_set_app_health_status_sync(NSMTST__pLifecycleControl,
                                                                      NSMTST__pstTestCase->unParameter.stDbSetAppHealthStatus.sAppName,
                                                                      NSMTST__pstTestCase->unParameter.stDbSetAppHealthStatus.boAppRunning,
                                                                      (gint*) &enReceivedNsmReturn,
                                                                      NULL,
                                                                      &pError);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(enReceivedNsmReturn == NSMTST__pstTestCase->unReturnValues.stDbSetAppHealthStatus.enErrorStatus)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return value. Received: 0x%02X. Expected: 0x%02X.",
                                                  enReceivedNsmReturn, NSMTST__pstTestCase->unReturnValues.stDbSetAppHealthStatus.enErrorStatus);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSM via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boSmGetInterfaceVersion(void)
{
  /* Function local variables                                   */
  gboolean  boRetVal                    = TRUE; /* Return value */
  GError   *pError                      = NULL;
  guint     u32ReceivedInterfaceVersion = 0;

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Get InterfaceVersion. Interface: StateMachine.");

  /* Perform test call */
  (void) node_state_test_call_get_nsm_interface_version_sync(NSMTST__pNodeStateMachine,
                                                             &u32ReceivedInterfaceVersion,
                                                             NULL,
                                                             &pError);

  /* Evaluate result. Check if a D-Bus error occurred. */
  if(pError == NULL)
  {
    /* D-Bus communication successful. Check if NSM returned with the expected value. */
    if(u32ReceivedInterfaceVersion == NSMTST__pstTestCase->unReturnValues.stSmGetInterfaceVersion.u32InterfaceVersion)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected InterfaceVersion. Received: 0x%02X. Expected: 0x%02X.",
                                                  u32ReceivedInterfaceVersion, NSMTST__pstTestCase->unReturnValues.stSmGetInterfaceVersion.u32InterfaceVersion);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to create access NSM via D-Bus. Error msg.: %s.",
                                                pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}

static gboolean NSMTST__boCheckSessionSignal(void)
{
  /* Function local variables                */
  gboolean boRetVal = FALSE; /* Return value */

  /* Create test case description */
  NSMTST__sTestDescription = g_strdup_printf("Check for Session signal.");

  /* Compare the received with the expected values  */
  if(   (NSMTST__pstTestCase->unReturnValues.stCheckSessionSignal.boReceived == NSMTST__stReceivedSessionSignal.boReceived     )
     && (NSMTST__pstTestCase->unReturnValues.stCheckSessionSignal.enSeat     == NSMTST__stReceivedSessionSignal.enSeat         )
     && (NSMTST__pstTestCase->unReturnValues.stCheckSessionSignal.enState    == NSMTST__stReceivedSessionSignal.enState        )
     && (g_strcmp0(NSMTST__pstTestCase->unReturnValues.stCheckSessionSignal.sName, NSMTST__stReceivedSessionSignal.sName) == 0 ))
  {
    /* We found what we expected */
    boRetVal = TRUE;
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Check for SessionState signal reception failed.");
  }

  NSMTST__stReceivedSessionSignal.boReceived = FALSE;

  if(NSMTST__stReceivedSessionSignal.sName != NULL)
  {
    g_free(NSMTST__stReceivedSessionSignal.sName);
    NSMTST__stReceivedSessionSignal.sName = NULL;
  }

  NSMTST__stReceivedSessionSignal.enSeat  = NsmSeat_NotSet;
  NSMTST__stReceivedSessionSignal.enState = NsmSessionState_Unregistered;

  return boRetVal;
}


static gboolean NSMTST__boCheckNodeStateSignal(void)
{
  /* Function local variables                */
  gboolean boRetVal = FALSE; /* Return value */

  NSMTST__sTestDescription = g_strdup("Check for NodeState signal.");

  /* Compare the received with the expected values  */
  if(   (NSMTST__pstTestCase->unReturnValues.stCheckNodeStateSignal.boReceived  == NSMTST__stReceivedNodeStateSignal.boReceived )
     && (NSMTST__pstTestCase->unReturnValues.stCheckNodeStateSignal.enNodeState == NSMTST__stReceivedNodeStateSignal.enNodeState))
  {
    /* We found what we expected */
    boRetVal = TRUE;
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup("Check for NodeState reception failed.");
  }

  NSMTST__stReceivedNodeStateSignal.boReceived  = FALSE;
  NSMTST__stReceivedNodeStateSignal.enNodeState = NsmNodeState_NotSet;

  return boRetVal;
}

static gboolean NSMTST__boCheckApplicationModeSignal(void)
{
  /* Function local variables                */
  gboolean boRetVal = FALSE; /* Return value */

  NSMTST__sTestDescription = g_strdup("Check for Application mode signal.");

  /* Compare the received with the expected values  */
  if(   (NSMTST__pstTestCase->unReturnValues.stCheckApplicationModeSignal.boReceived        == NSMTST__stApplicationModeSignal.boReceived       )
     && (NSMTST__pstTestCase->unReturnValues.stCheckApplicationModeSignal.enApplicationMode == NSMTST__stApplicationModeSignal.enApplicationMode))
  {
    /* We found what we expected */
    boRetVal = TRUE;
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Check for ApplicationMode signal reception failed.");
  }

  NSMTST__stApplicationModeSignal.boReceived        = FALSE;
  NSMTST__stApplicationModeSignal.enApplicationMode = NsmApplicationMode_NotSet;

  return boRetVal;




  return boRetVal;
}

static gboolean NSMTST__boTestCreateLcClient(void)
{
  NodeStateLifeCycleConsumer *pLifeCycleConsumer = NULL;
  gboolean                    boRetVal           = FALSE;

  NSMTST__sTestDescription = g_strdup_printf("Create LifecycleConsumer: %s.", NSMTST__pstTestCase->unParameter.stTestCreateLcClient.sObjName);

  pLifeCycleConsumer = node_state_life_cycle_consumer_skeleton_new();
  NSMTST__pLifecycleClients = g_slist_append(NSMTST__pLifecycleClients, (gpointer) pLifeCycleConsumer);

  g_signal_connect(pLifeCycleConsumer, "handle-lifecycle-request", G_CALLBACK(NSMTST__boOnLifecycleClientCb), NULL);

  if(g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(pLifeCycleConsumer),
                                      NSMTST__pConnection,
                                      NSMTST__pstTestCase->unParameter.stTestCreateLcClient.sObjName,
                                      NULL) == TRUE)
  {
    boRetVal = TRUE;
  }
  else
  {
    boRetVal = FALSE;
  }

  return boRetVal;
}


static gboolean NSMTST__boTestProcessLifecycleRequest(void)
{
  gboolean boRetVal = FALSE;

  NSMTST__sTestDescription = g_strdup_printf("Processing Lifecycle request. Return 0x%02X to NSM.",
                                             NSMTST__pstTestCase->unParameter.stTestProcessLifecycleRequest.enErrorStatus);

  if(   (NSMTST__pLifecycleConsumer   != NULL)
     && (NSMTST__pLifecycleInvocation != NULL))
  {
    if(NSMTST__u32LifecycleRequest == NSMTST__pstTestCase->unReturnValues.stTestProcessLifecycleRequest.u32RequestType)
    {
      boRetVal = TRUE;

      node_state_life_cycle_consumer_complete_lifecycle_request(NSMTST__pLifecycleConsumer,
                                                                NSMTST__pLifecycleInvocation,
                                                                (gint) NSMTST__pstTestCase->unParameter.stTestProcessLifecycleRequest.enErrorStatus);

      if(   (NSMTST__pstTestCase->unParameter.stTestProcessLifecycleRequest.enErrorStatus == NsmErrorStatus_Ok)
         || (NSMTST__pstTestCase->unParameter.stTestProcessLifecycleRequest.enErrorStatus == NsmErrorStatus_Error))
      {
        NSMTST__pLifecycleConsumer    = NULL;
        NSMTST__pLifecycleInvocation  = NULL;
      }
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected shutdown type.");
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected shutdown request.");
  }

  return boRetVal;
}

static gboolean NSMTST__boDbLifecycleRequestComplete(void)
{
  gboolean          boRetVal            = FALSE;
  GError           *pError              = NULL;
  NsmErrorStatus_e  enReceivedNsmReturn = NsmErrorStatus_NotSet;

  NSMTST__sTestDescription = g_strdup("Sending Lifecycle request finished.");

  (void) node_state_consumer_call_lifecycle_request_complete_sync(NSMTST__pNodeStateConsumer,
                                                                  NSMTST__u32LifecycleRequestId,
                                                                  NSMTST__pstTestCase->unParameter.stDbLifecycleRequestComplete.enErrorStatus,
                                                                  (gint*) &enReceivedNsmReturn,
                                                                  NULL,
                                                                  &pError);

  if(pError == NULL)
  {
    if(enReceivedNsmReturn == NSMTST__pstTestCase->unReturnValues.stDbLifecycleRequestComplete.enErrorStatus)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected NSM return. Received: 0x%02X. Expected: 0x%02X.",
                                                  enReceivedNsmReturn, NSMTST__pstTestCase->unReturnValues.stDbLifecycleRequestComplete.enErrorStatus);
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to access NSM via D-Bus. Error msg.: %s.", pError->message);
    g_error_free(pError);
  }

  return boRetVal;
}



static gboolean NSMTST__boDbCheckLucRequired(void)
{
  gboolean  boReceivedLucRequired = FALSE;
  GError   *pError                = NULL;
  gboolean  boRetVal              = FALSE;

  NSMTST__sTestDescription = g_strdup("Check LUC required.");

  (void) node_state_lifecycle_control_call_check_luc_required_sync(NSMTST__pLifecycleControl,
                                                                   &boReceivedLucRequired,
                                                                   NULL,
                                                                   &pError);

  if(pError == NULL)
  {
    if(boReceivedLucRequired == NSMTST__pstTestCase->unReturnValues.stDbCheckLucRequired.boLucRequired)
    {
      boRetVal = TRUE;
    }
    else
    {
      boRetVal = FALSE;
      NSMTST__sErrorDescription = g_strdup_printf("Did not receive expected value. Received: %s. Expected: %s.",
                                                  boReceivedLucRequired == TRUE ? "true" : "false",
                                                  NSMTST__pstTestCase->unReturnValues.stDbCheckLucRequired.boLucRequired == TRUE ? "true" : "false");
    }
  }
  else
  {
    boRetVal = FALSE;
    NSMTST__sErrorDescription = g_strdup_printf("Failed to access NSM via D-Bus. Error msg.: %s.", pError->message);
  }

  return boRetVal;
}


gboolean NSMTST__boOnApplicationModeSignal(NodeStateConsumer *pObject,
                                           const gint        i32ApplicationMode,
                                           gpointer          pUserData)
{
  NSMTST__stApplicationModeSignal.boReceived        = TRUE;
  NSMTST__stApplicationModeSignal.enApplicationMode = (NsmApplicationMode_e) i32ApplicationMode;

  return TRUE;
}


gboolean NSMTST__boOnNodeStateSignal(NodeStateConsumer *pObject,
                                     const gint         i32NodeState,
                                     gpointer           pUserData)
{
  NSMTST__stReceivedNodeStateSignal.boReceived  = TRUE;
  NSMTST__stReceivedNodeStateSignal.enNodeState = (NsmNodeState_e) i32NodeState;

  return TRUE;
}


/**********************************************************************************************************************
*
* Callback for the SessionState signal. Store the parameters
*
* @param pConsumer:           Lifecycle consumer object
* @param pInvocation:         D-Bus invocation object
* @param u32LifeCycleRequest: Request (shutdown type)
* @param u32RequestId:        Unique ID, which a client needs to pass to "LifecycleRequestComplete" in case of an
*                             asynchronous shut down handling.
* @param pUserData:           Optional user data (not used).
*
* @return:  TRUE: Indicate D-Bus that the method has been processed,
*
**********************************************************************************************************************/
static gboolean NSMTST__boOnSessionSignal(NodeStateConsumer *pObject,
                                          const gchar       *sSessionName,
                                          const gint         i32SeatId,
                                          const gint         i32SessionState,
                                          gpointer           pUserData)
{
  /* Store values passed by the NSM */
  NSMTST__stReceivedSessionSignal.boReceived = TRUE;

  if(NSMTST__stReceivedSessionSignal.sName != NULL)
  {
    g_free(NSMTST__stReceivedSessionSignal.sName);
    NSMTST__stReceivedSessionSignal.sName = NULL;
  }

  NSMTST__stReceivedSessionSignal.sName      = g_strdup(sSessionName);
  NSMTST__stReceivedSessionSignal.enSeat     = (NsmSeat_e)         i32SeatId;
  NSMTST__stReceivedSessionSignal.enState    = (NsmSessionState_e) i32SessionState;

  return TRUE;
}


/**********************************************************************************************************************
*
* Callback for the life cycle request call used for ALL life cycle clients.  The "completion" function to return
* values to the NSM is NOT called here. Instead the parameters of the callback are saved and during the test
* "NSMTST__boProcessLifecycleRequest" can be used to return different values.
*
* @param pConsumer:           Lifecycle consumer object
* @param pInvocation:         D-Bus invocation object
* @param u32LifeCycleRequest: Request (shutdown type)
* @param u32RequestId:        Unique ID, which a client needs to pass to "LifecycleRequestComplete" in case of an
*                             asynchronous shut down handling.
* @param pUserData:           Optional user data (not used).
*
* @return:  TRUE: Indicate D-Bus that the method has been processed,
*
**********************************************************************************************************************/
static gboolean NSMTST__boOnLifecycleClientCb(NodeStateLifeCycleConsumer *pConsumer,
                                              GDBusMethodInvocation      *pInvocation,
                                              const guint32               u32LifeCycleRequest,
                                              const guint32               u32RequestId,
                                              gpointer                    pUserData)
{
  /* Store values passed by the NSM */
  NSMTST__pLifecycleConsumer    = pConsumer;
  NSMTST__pLifecycleInvocation  = pInvocation;
  NSMTST__u32LifecycleRequest   = u32LifeCycleRequest;
  NSMTST__u32LifecycleRequestId = u32RequestId;

  return TRUE;
}


/**********************************************************************************************************************
*
* Timer callback in which the test cases are performed.
*
* @param pUserData: Data passed to callback by user.
*
* @return:  TRUE:  Keep the cyclic timer callback alive.
*           FALSE: Remove the cyclic timer event from MainLoop.
*
**********************************************************************************************************************/
static gboolean NSMTST__boTestCaseTimerCb(gpointer pUserData)
{
  /* Function local variables                                                */
  gboolean boTestSuccess    = FALSE; /* Flag to recognize error in test case */
  gboolean boKeepTimerAlive = FALSE; /* Flag if timer should stay alive      */

  /* Perform the test call. Store return value for further evaluation */
  NSMTST__pstTestCase = &NSMTST__astTestCases[NSMTST__u16TestIdx];
  boTestSuccess = NSMTST__pstTestCase->pfTestCall();

  /* Print the test result */
  g_print(NSMTST__TESTPRINT, NSMTST__u16TestIdx,
                             NSMTST__sTestDescription  == NULL ? "-"       : NSMTST__sTestDescription,
                             NSMTST__sErrorDescription == NULL ? "-"       : NSMTST__sErrorDescription,
                             boTestSuccess             == TRUE ? "success" : "failed");

  /* Free description string allocated by the test */
  if(NSMTST__sTestDescription != NULL)
  {
    g_free(NSMTST__sTestDescription);
    NSMTST__sTestDescription = NULL;
  }

  /* Free error string allocated by the test */
  if(NSMTST__sErrorDescription != NULL)
  {
    g_free(NSMTST__sErrorDescription);
    NSMTST__sErrorDescription = NULL;
  }

  NSMTST__u16TestIdx++; /* prepare system for next test */

  /* The tests end if there was an error or there are no test cases left */
  if((NSMTST__u16TestIdx < sizeof(NSMTST__astTestCases)/sizeof(NSMTST__tstTestCase)))
  {
    boKeepTimerAlive = TRUE;
  }
  else
  {
    boKeepTimerAlive = FALSE;
    g_main_loop_quit(pUserData);
  }

  return boKeepTimerAlive;
}


/**********************************************************************************************************************
*
* Main function of the test client executable.
*
* @return:  0: All tests ended successful
*          -1: At least one test was not successful
*
**********************************************************************************************************************/
int main(void)
{
  int iRetVal = 0;

  /* Initialize types in order to use glib */
  g_type_init();

  NSMTST__pLifecycleConsumer   = NULL;
  NSMTST__pLifecycleInvocation = NULL;

  /* Create main loop. Function can not fail. */
  NSMTST__pMainLoop = g_main_loop_new(NULL, FALSE);

  /* Add a timeout to the loop in which's callback the test cases are performed */
  g_timeout_add_full(G_PRIORITY_DEFAULT, NSMTST__TIMER_INTERVAL, &NSMTST__boTestCaseTimerCb, NSMTST__pMainLoop, NULL);

  /* Blocking call: Run the main loop, wait for callbacks */
  g_main_loop_run(NSMTST__pMainLoop);

  iRetVal = (NSMTST__u16TestIdx == sizeof(NSMTST__astTestCases)/sizeof(NSMTST__tstTestCase)) ? 0 : -1;

  if(NSMTST__pLifecycleControl  != NULL) g_object_unref(NSMTST__pLifecycleControl);
  if(NSMTST__pNodeStateConsumer != NULL) g_object_unref(NSMTST__pNodeStateConsumer);
  if(NSMTST__pNodeStateMachine  != NULL) g_object_unref(NSMTST__pNodeStateMachine);

  g_slist_free_full(NSMTST__pLifecycleClients, &g_object_unref);

  /* Free the main loop, when it was left */
  g_main_loop_unref(NSMTST__pMainLoop);

  return iRetVal;
}
