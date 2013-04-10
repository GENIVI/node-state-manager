/**********************************************************************************************************************
*
* Copyright (C) 2012 Continental Automotive Systems, Inc.
*
* Author: Jean-Pierre.Bogler@continental-corporation.com
*
* Implementation of the NodeStateMachineTest.
*
* The NodeStateMachineTest compiles to a shared object that is loaded by the NodeStateManager.
* This special test version of the NodeStateMachine implements additonal functionality.
* Beside of the internal interfaces that are directly called by the NSM, who links with the lib,
* the test NodeStateMachine offers an own dbus interface!
*
* A test frame can use this dbus interface to stimulate the NSMC to make calls to the NSM.
* The return values that the NSMC receives are passed back to the test frame, where it can be
* checked if they are valid.
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Date             Author              Reason
* 24.01.2013       Jean-Pierre Bogler  CSP_WZ#1194: Initial creation.
*
**********************************************************************************************************************/


/**********************************************************************************************************************
*
* Header includes
*
**********************************************************************************************************************/

#include <gio/gio.h>                 /* Access dbus using glib          */

#include "NodeStateMachineTest.h"    /* Own header file                 */
#include "NodeStateTypes.h"          /* Know the types of the NSM       */
#include "NodeStateManager.h"	     /* Access inhternal NSM interfaces */

#include "NodeStateMachineTestApi.h" /* Dbus interface offered by NSMC  */


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

static NodeStateTest   *TSTMSC__pTestMachine = NULL;
static GDBusConnection *TSTMSC__pConnection  = NULL;


/**********************************************************************************************************************
*
* Prototypes for file local functions (see implementation for description)
*
**********************************************************************************************************************/

static gboolean NSM__boOnHandleSetNsmData(NodeStateTest         *pTestMachine,
                                          GDBusMethodInvocation *pInvocation,
                                          const gint             i32DataType,
                                          GVariant              *pData,
                                          const guint            u32DataLen,
                                          gpointer               pUserData);

static gboolean NSM__boOnHandleGetNsmData(NodeStateTest         *pTestMachine,
                                          GDBusMethodInvocation *pInvocation,
                                          const gint             i32DataType,
                                          GVariant              *DataIn,
                                          const guint            u32DataLen,
                                          gpointer               pUserData);

static gboolean NSM__boOnHandleGetNsmInterfaceVersion(NodeStateTest         *pTestMachine,
                                                      GDBusMethodInvocation *pInvocation,
                                                      gpointer               pUserData);

/**********************************************************************************************************************
*
* Local (static) functions
*
**********************************************************************************************************************/

/**********************************************************************************************************************
*
* The function is called when a test frame wants to set data to the NSM via the NSMC.
*
* @param pTestMachine: NodeStateMachineTest object.
* @param pInvocation:  Invocation for this call.
* @param i32DataType:  DataType (possible valid values see NsmDataType_e)
* @param pData:        GVariant of type "ay". Includes an array of byte containing the data to be set.
* @param u32DataLen:   Length of the data, which is directly passed to the NSM. Can contain invalid values,
*                      because it is not usef for the serialization of the content of pData.
* @param pUserData:    Opzional user data (not used).
*
* @return TRUE: Dbus message was handled.
*
**********************************************************************************************************************/
static gboolean NSM__boOnHandleSetNsmData(NodeStateTest         *pTestMachine,
                                          GDBusMethodInvocation *pInvocation,
                                          const gint             i32DataType,
                                          GVariant              *pData,
                                          const guint            u32DataLen,
                                          gpointer               pUserData)
{
  /* Function local variables                                           */
  guint             u32ArraySize  = 0;    /* Children of pData          */
  guint8           *au8Data       = NULL; /* Pointer to byte array      */
  guint             u32ArrayIdx   = 0;    /* Index to loop through data */
  GVariant         *pArrayElement = NULL; /* Pointer to child of pData  */
  NsmErrorStatus_e  enRetVal      = NsmErrorStatus_NotSet;
  NsmDataType_e     enDateType    = (NsmDataType_e) i32DataType;

  /* Create a new byte array based on the length of pData and fill it with the values */
  u32ArraySize = g_variant_n_children(pData);
  au8Data = g_new(guint8, u32ArraySize);

  for(u32ArrayIdx = 0; u32ArrayIdx < u32ArraySize; u32ArrayIdx++)
  {
    pArrayElement = g_variant_get_child_value(pData, u32ArrayIdx);
    au8Data[u32ArrayIdx] = g_variant_get_byte(pArrayElement);
    g_variant_unref(pArrayElement);
  }

  g_variant_unref(pData); /* release the variant. We read it */

  /* Call the NSM. Pass extracted data and length, originally passed by test frame */
  enRetVal = NsmSetData(enDateType, (unsigned char*) au8Data, (unsigned int) u32DataLen);

  /* Send NSMs return value to the test frame */
  node_state_test_complete_set_nsm_data(pTestMachine, pInvocation, (gint) enRetVal);

  return TRUE;
}

/**********************************************************************************************************************
*
* The function is called when a test frame wants to get data from the NSM via the NSMC.
*
* @param pTestMachine: NodeStateMachineTest object.
* @param pInvocation:  Invocation for this call.
* @param i32DataType:  DataType (possible valid values see NsmDataType_e)
* @param pDataIn:      Parameters passed by test frame fopr getting data (currently only used for sesion states)
* @param u32DataLen:   Length of the expected data, which is directly passed to the NSM. Can contain invalid values,
*                      because it is not used for the serialization of pDataOut.
* @param pUserData:    Opzional user data (not used).
*
* @return TRUE: Dbus message was handled.
*
**********************************************************************************************************************/
static gboolean NSM__boOnHandleGetNsmData(NodeStateTest         *pTestMachine,
                                          GDBusMethodInvocation *pInvocation,
                                          const gint             i32DataType,
                                          GVariant              *DataIn,
                                          const guint            u32DataLen,
                                          gpointer               pUserData)
{
  /* Function local variables                                                                        */
  NsmSession_s       pData;                 /* Contains data from pDataIn and is popassed to the NSM */
  int                i32RetVal      = 0;    /* Return value of the NSM                               */
  guint              u32ArrayIdx    = 0;    /* Index to loop through data                            */
  GVariant         **pArrayElements = NULL; /* Helper to serailize data                              */
  GVariant          *pRetArray      = NULL; /* Outgoing data returned to NSM                         */
  GVariant          *pDataInElement = NULL; /* Pointer to child of pDataIn                           */

  /* 
   * The NSM has a read write interface for getting data. The largest data frame that can be
   * exchanged is a NsmSession_s. Therefore, pDataIn is translated into this kind of variable.
   */
  for(u32ArrayIdx = 0; u32ArrayIdx < g_variant_n_children(DataIn); u32ArrayIdx++)
  {
    pDataInElement = g_variant_get_child_value(DataIn, u32ArrayIdx);
    ((guchar*) &pData)[u32ArrayIdx] = g_variant_get_byte(pDataInElement);
  }

  g_variant_unref(DataIn); /* Release pDataIn. we read the data */

  /* Call the NSM */
  i32RetVal = NsmGetData((NsmDataType_e) i32DataType, (unsigned char*) &pData, u32DataLen);

  /* Serialize ougoing data retuurned by NSM, if there is a positive NSM return */
  if(i32RetVal > 0)
  {
    pArrayElements = g_new(GVariant*, i32RetVal);

    for(u32ArrayIdx = 0; u32ArrayIdx < (guint) i32RetVal; u32ArrayIdx++)
    {
      pArrayElements[u32ArrayIdx] = g_variant_new_byte( ((guchar*) &pData)[u32ArrayIdx] );
    }

    pRetArray = g_variant_new_array(G_VARIANT_TYPE_BYTE, pArrayElements, (gsize) i32RetVal);
  }
  else
  {
    pArrayElements    = g_new(GVariant*, 1);
    pArrayElements[0] = g_variant_new_byte(0);
    pRetArray = g_variant_new_array(G_VARIANT_TYPE_BYTE, pArrayElements, 1);
  }

  /* Send NSM return to test frame */
  node_state_test_complete_get_nsm_data(pTestMachine, pInvocation, pRetArray, i32RetVal);

  g_variant_unref(pRetArray); /* Release the returned data, because we send it. */

  return TRUE;
}

/**********************************************************************************************************************
*
* The function is called when a test frame wants to get the interface version of the NSM via the NSMC.
*
* @param pTestMachine: NodeStateMachineTest object.
* @param pInvocation:  Invocation for this call.
* @param pUserData:    Opzional user data (not used).
*
* @return TRUE: Dbus message was handled.
*
**********************************************************************************************************************/
static gboolean NSM__boOnHandleGetNsmInterfaceVersion(NodeStateTest         *pTestMachine,
                                                      GDBusMethodInvocation *pInvocation,
                                                      gpointer               pUserData)
{
  node_state_test_complete_get_nsm_interface_version(pTestMachine, pInvocation, (guint) NsmGetInterfaceVersion());

  return TRUE;
}


/**********************************************************************************************************************
*
* Interfaces, exported functions. See header for detailed description.
*
**********************************************************************************************************************/

unsigned char NsmcInit(void)
{
  unsigned char u8RetVal = 0;

  TSTMSC__pTestMachine = node_state_test_skeleton_new();
  TSTMSC__pConnection  = g_bus_get_sync(NSM_BUS_TYPE, NULL, NULL);

  if(g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(TSTMSC__pTestMachine),
		                      TSTMSC__pConnection,
                                      "/com/contiautomotive/NodeStateMachineTest",
                                      NULL) == TRUE)
  {
    u8RetVal = 1;
    (void) g_signal_connect(TSTMSC__pTestMachine, "handle-set-nsm-data",              G_CALLBACK(NSM__boOnHandleSetNsmData),             NULL);
    (void) g_signal_connect(TSTMSC__pTestMachine, "handle-get-nsm-data",              G_CALLBACK(NSM__boOnHandleGetNsmData),             NULL);
    (void) g_signal_connect(TSTMSC__pTestMachine, "handle-get-nsm-interface-version", G_CALLBACK(NSM__boOnHandleGetNsmInterfaceVersion), NULL);
  }
  else
  {
    u8RetVal = 0;
  }

  return u8RetVal;
}


unsigned char NsmcLucRequired(void)
{
  return 1;
}


NsmErrorStatus_e NsmcSetData(NsmDataType_e enData, unsigned char *pData, unsigned int u32DataLen)
{
  return NsmErrorStatus_Ok;
}


unsigned char NsmcRequestNodeRestart(void)
{
  return 1;
}


unsigned int NsmcGetInterfaceVersion(void)
{
  return (unsigned int) NSMC_INTERFACE_VERSION;
}


NsmErrorStatus_e NsmcSetTestData(NsmDataType_e enData, unsigned char *pData, unsigned int u32DataLen)
{
  if(   (enData                   == NsmDataType_NodeState)
     && (u32DataLen               == sizeof(NsmNodeState_e)
     && ((NsmNodeState_e) *pData) == NsmNodeState_Shutdown))
  {
    g_object_unref(TSTMSC__pConnection);
    g_object_unref(TSTMSC__pTestMachine);
  }

  return NsmErrorStatus_Ok;
}

