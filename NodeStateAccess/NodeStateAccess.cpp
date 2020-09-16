/**********************************************************************************************************************
 *
 * Copyright (C) 2017 BMW AG
 *
 * Interface between NodeStateManager and IPC
 *
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

/**********************************************************************************************************************
 *
 * Header includes
 *
 **********************************************************************************************************************/

/* generic includes for the NodeStateAccess library        */
#include "NodeStateAccess.h" /* own header                 */
#include "NodeStateAccess.hpp"
#include "NodeStateTypes.h"  /* Type defintions of the NSM */

#include <algorithm>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <dlt.h>
#include <string.h>

#ifdef COVERAGE_ENABLED
extern "C" void __gcov_flush();
#endif

#include "NodeStateConsumer_StubImpl.h"
#include "NodeStateLifecycleControl_StubImpl.h"
#include "SimpleTimer.h"

#include <v1/org/genivi/nodestatemanager/ConsumerInstanceIds.hpp>
#include <v1/org/genivi/nodestatemanager/LifecycleControlInstanceIds.hpp>

namespace GENIVI = v1::org::genivi;
using namespace GENIVI::nodestatemanager;

// CommonAPI connection ID
const char* gConnectionID = "NSMimpl";
const char* gCapiDomain = "local";

DLT_IMPORT_CONTEXT(NsmaContext);

/**********************************************************************************************************************
 *
 * Local variables
 *
 **********************************************************************************************************************/

static gboolean NSMA__boLoopEndByUser = FALSE;
static gboolean NSMA__boInitialized = FALSE;

/* Structure with callback functions to the NSM */
static NSMA_tstObjectCallbacks NSMA__stObjectCallbacks = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

static std::shared_ptr<NodeStateConsumer_StubImpl> NSMA_consumerService = NULL;
static std::shared_ptr<NodeStateLifecycleControl_StubImpl> NSMA_lifecycleControlService = NULL;

static std::unordered_map<std::size_t, std::shared_ptr<SimpleTimer>> NSMA_clParallelShutdownPendingReceivers;

static std::shared_ptr<CommonAPI::Runtime> NSMA_runtime = NULL;

static std::shared_ptr<SimpleTimer> NSMA_simpleTimer = NULL;

static size_t NSMA_currentConsumer = 0;

static std::mutex NSMA__mutexParallelShutdown;
static std::mutex NSMA__mutexWaitForEvents;
static std::mutex NSMA__mutex;
static std::condition_variable NSMA__condVarShutdown;

/**********************************************************************************************************************
 *
 * Local (static) functions
 *
 **********************************************************************************************************************/

/**********************************************************************************************************************
*
* The function is called when the async. call to a life cycle clients "LifecycleRequest" method timed out.
*
**********************************************************************************************************************/
static void NSMA__boHandleSequentialRequestTimeout()
{
   std::unique_lock<std::mutex> lock(NSMA__mutexParallelShutdown);
   if (NULL != NSMA_consumerService)
   {
      DLT_LOG(NsmaContext, DLT_LOG_ERROR, DLT_STRING("NSMA: Error! Sequential client has timed out! Client ID:"), DLT_UINT64(NSMA_currentConsumer));

      size_t oldConsumer = NSMA_currentConsumer;
      NSMA_currentConsumer = 0;

      lock.unlock();
      NSMA__stObjectCallbacks.pfLcClientRequestFinish(oldConsumer, TRUE, FALSE);
   }
}

/**********************************************************************************************************************
*
* The function is called when the async. call to a parallel life cycle clients "LifecycleRequest" method timed out.
*
**********************************************************************************************************************/
static void NSMA__boHandleParallelRequestTimeout(std::size_t client)
{
   std::unique_lock<std::mutex> lock(NSMA__mutexParallelShutdown);

   /* Find timed out client in parallel shutdown receivers list */
   auto search = NSMA_clParallelShutdownPendingReceivers.find(client);
   if(NULL != NSMA_consumerService && search != NSMA_clParallelShutdownPendingReceivers.end())
   {
      DLT_LOG(NsmaContext, DLT_LOG_ERROR, DLT_STRING("NSMA: Error! Parallel client has timed out! Client ID:"), DLT_UINT64(client));

      /* Remove this client from the list of pending clients */
      NSMA_clParallelShutdownPendingReceivers.erase(search);

      lock.unlock();
      NSMA__stObjectCallbacks.pfLcClientRequestFinish(client, TRUE, FALSE);
   }
}

/**********************************************************************************************************************
 *
 * Interfaces. Exported functions. See Header for detailed description.
 *
 **********************************************************************************************************************/
gboolean NSMA_boInit(const NSMA_tstObjectCallbacks *pstCallbacks)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   /* Initialize file local variables */
   NSMA__boInitialized = FALSE;

   memset(&NSMA__stObjectCallbacks, 0, sizeof(NSMA_tstObjectCallbacks));
   std::lock_guard<std::mutex> lock(NSMA__mutex);

   /* Check if all callbacks for the NSM have been configured. */
   if(   pstCallbacks != NULL
     && (pstCallbacks->pfSetBootModeCb               != NULL)
     && (pstCallbacks->pfRequestNodeRestartCb        != NULL)
     && (pstCallbacks->pfSetAppHealthStatusCb        != NULL)
     && (pstCallbacks->pfCheckLucRequiredCb          != NULL)
     && (pstCallbacks->pfRegisterSessionCb           != NULL)
     && (pstCallbacks->pfUnRegisterSessionCb         != NULL)
     && (pstCallbacks->pfRegisterLifecycleClientCb   != NULL)
     && (pstCallbacks->pfUnRegisterLifecycleClientCb != NULL)
     && (pstCallbacks->pfGetSessionStateCb           != NULL)
     && (pstCallbacks->pfGetNodeStateCb              != NULL)
     && (pstCallbacks->pfSetNodeStateCb              != NULL)
     && (pstCallbacks->pfSetSessionStateCb           != NULL)
     && (pstCallbacks->pfGetAppHealthCountCb         != NULL)
     && (pstCallbacks->pfGetInterfaceVersionCb       != NULL)
     && (pstCallbacks->pfLcClientRequestFinish       != NULL))
   {
      /* Store the passed callbacks. */
      memcpy(&NSMA__stObjectCallbacks, pstCallbacks, sizeof(NSMA_tstObjectCallbacks));

      if (NULL == (NSMA_runtime = CommonAPI::Runtime::get()))
      {
         DLT_LOG(NsmaContext, DLT_LOG_ERROR, DLT_STRING("NSMA: Failed to get CAPI Runtime!"));
      }
      else if(NULL == (NSMA_consumerService = std::make_shared<NodeStateConsumer_StubImpl>(&NSMA__stObjectCallbacks)))
      {
         DLT_LOG(NsmaContext, DLT_LOG_ERROR, DLT_STRING("NSMA: Failed to create NSMA_consumerService"));
      }
      else if (NULL == (NSMA_lifecycleControlService = std::make_shared<NodeStateLifecycleControl_StubImpl>(&NSMA__stObjectCallbacks)))
      {
         DLT_LOG(NsmaContext, DLT_LOG_ERROR, DLT_STRING("NSMA: Failed to create NSMA_lifecycleControlService"));
      }
      else if (!NSMA_runtime->registerService(gCapiDomain, v1::org::genivi::nodestatemanager::Consumer_INSTANCES[0], NSMA_consumerService, gConnectionID))
      {
         DLT_LOG(NsmaContext, DLT_LOG_ERROR, DLT_STRING("NSMA: Failed to register NSMConsumer"));
      }
      else if (!NSMA_runtime->registerService(gCapiDomain, v1::org::genivi::nodestatemanager::LifecycleControl_INSTANCES[0], NSMA_lifecycleControlService, "LifecycleControl"))
      {
         DLT_LOG(NsmaContext, DLT_LOG_ERROR, DLT_STRING("NSMA: Failed to register NSMLifecycleControl"));
      }
      else
      {
         DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: Successfully registered NSMA"));
         /* All callbacks are configured. */
         NSMA__boInitialized = TRUE;
      }
   }
   else
   {
      /* Error: Callbacks are configured incorrectly. */
      NSMA__boInitialized = FALSE;
   }

   return NSMA__boInitialized;
}

void signal_handler(int signal)
{
   DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: Received signal"), DLT_INT(signal));
   std::unique_lock<std::mutex> lock(NSMA__mutexWaitForEvents);
   NSMA__boLoopEndByUser = TRUE;
   lock.unlock();
   NSMA__condVarShutdown.notify_all();
}

gboolean NSMA_boWaitForEvents(void)
{
   std::signal(SIGTERM, signal_handler);
   std::signal(SIGINT, signal_handler);

   std::unique_lock<std::mutex> lock(NSMA__mutexWaitForEvents);
   while (FALSE == NSMA__boLoopEndByUser)
   {
      NSMTriggerWatchdog(NsmWatchdogState_Sleep);
      NSMA__condVarShutdown.wait(lock);
      NSMTriggerWatchdog(NsmWatchdogState_Active);
   }

   /* Delete signal handlers */
   signal(SIGTERM, SIG_DFL);
   signal(SIGINT, SIG_DFL);
   lock.unlock();
   std::fflush(stdout);
   std::fflush(stderr);

#ifdef COVERAGE_ENABLED
   __gcov_flush();
#endif

   return TRUE;
}

gboolean NSMA_boSendNodeStateSignal(const NsmNodeState_e enNodeState)
{
   std::lock_guard<std::mutex> lock(NSMA__mutex);
   gboolean boRetVal = FALSE;
   /* Check if library has been initialized (objects are available) */
   if (NSMA__boInitialized == TRUE)
   {
      boRetVal = TRUE; /* Send the signal */
      NSMA_consumerService->fireNodeStateEvent(GENIVI::NodeStateManagerTypes::NsmNodeState_e((GENIVI::NodeStateManagerTypes::NsmNodeState_e::Literal) enNodeState));
   }
   return boRetVal;
}

gboolean NSMA_boSendSessionSignal(const NsmSession_s *pstSession)
{
   std::lock_guard<std::mutex> lock(NSMA__mutex);
   gboolean boRetVal = FALSE;
   /* Check if library has been initialized (objects are available) */
   if (NSMA__boInitialized == TRUE)
   {
      boRetVal = TRUE; /* Send the signal */
      NSMA_consumerService->fireSessionStateChangedEvent(pstSession->sName, GENIVI::NodeStateManagerTypes::NsmSeat_e((GENIVI::NodeStateManagerTypes::NsmSeat_e::Literal)pstSession->enSeat), GENIVI::NodeStateManagerTypes::NsmSessionState_e((GENIVI::NodeStateManagerTypes::NsmSessionState_e::Literal)pstSession->enState));
   }
   return boRetVal;
}

gboolean NSMA_boSendApplicationModeSignal(const NsmApplicationMode_e enApplicationMode)
{
   std::lock_guard<std::mutex> lock(NSMA__mutex);
   gboolean boRetVal = FALSE;
   /* Check if library has been initialized (objects are available) */
   if (NSMA__boInitialized == TRUE)
   {
      boRetVal = TRUE; /* Send the signal */
      NSMA_consumerService->fireNodeApplicationModeEvent(GENIVI::NodeStateManagerTypes::NsmApplicationMode_e((GENIVI::NodeStateManagerTypes::NsmApplicationMode_e::Literal) enApplicationMode));
   }
   return boRetVal;
}

gboolean NSMA_boSetBootMode(gint i32BootMode)
{
   std::lock_guard<std::mutex> lock(NSMA__mutex);
   gboolean boRetVal = FALSE;
   /* Check if library has been initialized (objects are available) */
   if (NSMA__boInitialized == TRUE)
   {
      boRetVal = TRUE; /* Send the signal */
      NSMA_consumerService->setBootModeAttribute(i32BootMode);
   }
   return boRetVal;
}

gboolean NSMA_boGetBootMode(gint *pi32BootMode)
{
   std::lock_guard<std::mutex> lock(NSMA__mutex);
   gboolean boRetVal = FALSE;
   /* Check if library has been initialized (objects are available) */
   if (NSMA__boInitialized == TRUE)
   {
      boRetVal = TRUE; /* Send the signal */
      *pi32BootMode = NSMA_consumerService->getBootModeAttribute();
   }
   return boRetVal;
}

gboolean NSMA_boSetRunningReason(const NsmRunningReason_e enRunningReason)
{
   std::lock_guard<std::mutex> lock(NSMA__mutex);
   gboolean boRetVal = FALSE;
   /* Check if library has been initialized (objects are available) */
   if (NSMA__boInitialized == TRUE)
   {
      boRetVal = TRUE; /* Send the signal */
      NSMA_consumerService->setWakeUpReasonAttribute(GENIVI::NodeStateManagerTypes::NsmRunningReason_e((GENIVI::NodeStateManagerTypes::NsmRunningReason_e::Literal) enRunningReason));
   }
   return boRetVal;
}

gboolean NSMA_boGetRunningReason(NsmRunningReason_e *penRunningReason)
{
   std::lock_guard<std::mutex> lock(NSMA__mutex);
   gboolean boRetVal = FALSE;
   /* Check if library has been initialized (objects are available) */
   if (NSMA__boInitialized == TRUE)
   {
      boRetVal = TRUE; /* Send the signal */
      *penRunningReason = (NsmRunningReason_e)(int) NSMA_consumerService->getWakeUpReasonAttribute();
   }
   return boRetVal;
}

gboolean NSMA_boSetShutdownReason(const NsmShutdownReason_e enShutdownReason)
{
   std::lock_guard<std::mutex> lock(NSMA__mutex);
   gboolean boRetVal = FALSE;
   /* Check if library has been initialized (objects are available) */
   if (NSMA__boInitialized == TRUE)
   {
      boRetVal = TRUE; /* Send the signal */
      NSMA_consumerService->setShutdownReasonAttribute(GENIVI::NodeStateManagerTypes::NsmShutdownReason_e((GENIVI::NodeStateManagerTypes::NsmShutdownReason_e::Literal) enShutdownReason));
   }
   return boRetVal;
}

gboolean NSMA_boGetShutdownReason(NsmShutdownReason_e *penShutdownReason)
{
   std::lock_guard<std::mutex> lock(NSMA__mutex);
   gboolean boRetVal = FALSE;
   /* Check if library has been initialized (objects are available) */
   if (NSMA__boInitialized == TRUE)
   {
      boRetVal = TRUE; /* Send the signal */
      *penShutdownReason = (NsmShutdownReason_e)(int) NSMA_consumerService->getShutdownReasonAttribute();
   }
   return boRetVal;
}

gboolean NSMA_boSetRestartReason(const NsmRestartReason_e enRestartReason)
{
   std::lock_guard<std::mutex> lock(NSMA__mutex);
   gboolean boRetVal = FALSE;
   /* Check if library has been initialized (objects are available) */
   if (NSMA__boInitialized == TRUE)
   {
      boRetVal = TRUE; /* Send the signal */
      NSMA_consumerService->setRestartReasonAttribute(GENIVI::NodeStateManagerTypes::NsmRestartReason_e((GENIVI::NodeStateManagerTypes::NsmRestartReason_e::Literal) enRestartReason));
   }
   return boRetVal;
}

gboolean NSMA_boGetRestartReason(NsmRestartReason_e *penRestartReason)
{
   std::lock_guard<std::mutex> lock(NSMA__mutex);
   gboolean boRetVal = FALSE;
   /* Check if library has been initialized (objects are available) */
   if (NSMA__boInitialized == TRUE)
   {
      boRetVal = TRUE; /* Send the signal */
      *penRestartReason = (NsmRestartReason_e)(int) NSMA_consumerService->getRestartReasonAttribute();
   }
   return boRetVal;
}

gboolean NSMA_boCallParallelLcClientsRequest(NSM__tstLifecycleClient *client, guint numClients, guint u32ShutdownType)
{
   gboolean retVal = FALSE;
   guint i = 0;

   if (NULL != client && 0 < numClients)
   {
      std::lock_guard<std::mutex> lock(NSMA__mutexParallelShutdown);
      std::shared_ptr<CommonAPI::ClientIdList> receiver = std::make_shared<CommonAPI::ClientIdList>();
      std::shared_ptr<CommonAPI::ClientIdList> availableClients(NSMA_consumerService->getSubscribersForShutdownEventsSelective());
      for (i = 0; i < numClients; i++)
      {
         for (const auto &clientIdIterator : *availableClients)
         {
            if (clientIdIterator->hashCode() == client[i].clientHash)
            {
               NSMA_clParallelShutdownPendingReceivers.insert({clientIdIterator->hashCode(), SimpleTimer::CreateTimer((int)client[i].timeout, 1, &NSMA__boHandleParallelRequestTimeout, clientIdIterator->hashCode())});
                receiver->insert(clientIdIterator);
               if(u32ShutdownType == NSM_SHUTDOWNTYPE_RUNUP || u32ShutdownType == (NSM_SHUTDOWNTYPE_RUNUP | NSM_SHUTDOWNTYPE_PARALLEL))
                  DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: Will inform client"), DLT_INT64(client[i].clientHash), DLT_STRING("about parallel run up ("), DLT_UINT(u32ShutdownType), DLT_STRING(")!"));
               else
                  DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: Will inform client"), DLT_INT64(client[i].clientHash), DLT_STRING("about parallel shutdown ("), DLT_UINT(u32ShutdownType), DLT_STRING(")!"));
               break;
            }
         }
      }
      NSMA_consumerService->fireShutdownEventsSelective(u32ShutdownType, receiver);

      retVal = TRUE;
   }
   return retVal;
}

gboolean NSMA_boCallLcClientRequest(NSM__tstLifecycleClient *client, guint u32ShutdownType)
{
   gboolean retVal = FALSE;

   if (NULL != client)
   {
      std::lock_guard<std::mutex> lock(NSMA__mutexParallelShutdown);
      std::shared_ptr<CommonAPI::ClientIdList> receivers = std::make_shared<CommonAPI::ClientIdList>();
      std::shared_ptr<CommonAPI::ClientIdList> availableClients = NSMA_consumerService->getSubscribersForShutdownEventsSelective();

      for (const auto &clientIdIterator : *availableClients)
      {
         if (clientIdIterator->hashCode() == client->clientHash)
         {
            receivers->insert(clientIdIterator);
            if(u32ShutdownType == NSM_SHUTDOWNTYPE_RUNUP)
               DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: Will inform client"), DLT_INT64(client->clientHash), DLT_STRING("about run up ("), DLT_UINT(u32ShutdownType), DLT_STRING(")!"));
            else
               DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: Will inform client"), DLT_INT64(client->clientHash), DLT_STRING("about shutdown ("), DLT_UINT(u32ShutdownType), DLT_STRING(")!"));
            break;
         }
      }

      /* Set current consumer. This way nsm can check if the correct consumer responds */
      NSMA_currentConsumer = client->clientHash;

      NSMA_stopLcClientTimeout();
      /* Start timer so HandleRequestTimeout will be called when this timer is not canceled within timeout ms */
      NSMA_simpleTimer = SimpleTimer::CreateTimer((int) client->timeout, 1, &NSMA__boHandleSequentialRequestTimeout);

      NSMA_consumerService->fireShutdownEventsSelective(u32ShutdownType, receivers);

      retVal = TRUE;
   }
   return retVal;
}

gboolean NSMA_boCallLcClientRequestWithoutTimeout(NSM__tstLifecycleClient *client, guint u32ShutdownType)
{
   gboolean retVal = FALSE;
   if (NULL != client)
   {
      std::lock_guard<std::mutex> lock(NSMA__mutexParallelShutdown);
      std::shared_ptr<CommonAPI::ClientIdList> receivers = std::make_shared<CommonAPI::ClientIdList>();
      std::shared_ptr<CommonAPI::ClientIdList> availableClients = NSMA_consumerService->getSubscribersForShutdownEventsSelective();

      for (const auto &clientIdIterator : *availableClients)
      {
         if (clientIdIterator->hashCode() == client->clientHash)
         {
            receivers->insert(clientIdIterator);
            if(u32ShutdownType | NSM_SHUTDOWNTYPE_RUNUP)
               DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: Will inform client"), DLT_INT64(client->clientHash), DLT_STRING("about run up ("), DLT_UINT(u32ShutdownType), DLT_STRING(") without timeout!"));
            else
               DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: Will inform client"), DLT_INT64(client->clientHash), DLT_STRING("about shutdown ("), DLT_UINT(u32ShutdownType), DLT_STRING(") without timeout!"));
            break;
         }
      }
      NSMA_consumerService->fireShutdownEventsSelective(u32ShutdownType, receivers);
      retVal = TRUE;
   }
   return retVal;
}

int32_t NSMA_ClientRequestFinish(const std::shared_ptr<CommonAPI::ClientId> client, int32_t status)
{
  int32_t errorCode = NsmErrorStatus_Error;
  size_t clientID = client->hashCode();
  std::unique_lock<std::mutex> lock(NSMA__mutexParallelShutdown);

  /* Current sequential client */
  if (clientID == NSMA_currentConsumer)
  {
    NSMA_stopLcClientTimeout();
    NSMA_currentConsumer = 0;
    errorCode = NsmErrorStatus_Ok;
    DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: LifecycleRequest successful for (sequential) client:"), DLT_INT64(clientID), DLT_STRING("Return Value:"), DLT_STRING(ERRORSTATUS_STRING[status]));
    lock.unlock();
    NSMA__stObjectCallbacks.pfLcClientRequestFinish(clientID, FALSE, FALSE);
  }
  else /* Maybe parallel? */
  {
    /* There should be exactly one client in parallelShutdownReceivers with matching ClientId */
    auto search = NSMA_clParallelShutdownPendingReceivers.find(clientID);
    if (search != NSMA_clParallelShutdownPendingReceivers.end())
    {
      /* Cancel timeout for one parallel shutdown client */
      search->second->cancelTimer();
      NSMA_clParallelShutdownPendingReceivers.erase(search);
      errorCode = NsmErrorStatus_Ok;
      DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: LifecycleRequest successful for (parallel) client:"), DLT_INT64(clientID), DLT_STRING("Return Value:"), DLT_STRING(ERRORSTATUS_STRING[status]));
      lock.unlock();
      NSMA__stObjectCallbacks.pfLcClientRequestFinish(clientID, FALSE, FALSE);
    }
    else
    {
      /* Maybe late client? */
      std::shared_ptr<CommonAPI::ClientIdList> availableClients(NSMA_consumerService->getSubscribersForShutdownEventsSelective());
      for (const auto &clientIdIterator : *availableClients)
      {
        if (clientIdIterator->hashCode() == clientID)
        {
          /* Client has been found but it was not the expected one -> as it is late? */
          errorCode = NsmErrorStatus_WrongClient;
          DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: LifecycleRequest returned too late for client:"), DLT_INT64(clientID), DLT_STRING("Return Value:"), DLT_STRING(ERRORSTATUS_STRING[status]));
          lock.unlock();
          NSMA__stObjectCallbacks.pfLcClientRequestFinish(clientID, FALSE, TRUE);
          break;
        }
      }
    }
  }
  return errorCode;
}

void NSMA_stopLcClientTimeout()
{
   if (NSMA_simpleTimer != NULL)
   {
      NSMA_simpleTimer->stopTimer();
      NSMA_simpleTimer = NULL;
   }
}

void NSMA_stopParallelLcClientTimeout()
{
   for (const auto clientIdIterator : NSMA_clParallelShutdownPendingReceivers)
   {
      clientIdIterator.second->stopTimer();
   }
   NSMA_clParallelShutdownPendingReceivers.clear();
}

gboolean NSMA__ParallelClientHasPendingActiveCall(size_t clientID)
{
  gboolean retVal = FALSE;
  if(clientID == 0)
  {
    retVal = !NSMA_clParallelShutdownPendingReceivers.empty();
  }
  else
  {
    auto search = NSMA_clParallelShutdownPendingReceivers.find(clientID);
    if (search != NSMA_clParallelShutdownPendingReceivers.end())
    {
      retVal = TRUE;
    }
  }
  return retVal;
}

gboolean NSMA__SequentialClientHasPendingActiveCall()
{
  return (NSMA_currentConsumer != 0);
}

gboolean NSMA_boDeleteLifecycleClient(NSM__tstLifecycleClient *client)
{
  gboolean retVal = FALSE;
  std::lock_guard<std::mutex> lock(NSMA__mutexParallelShutdown);
  if(client->clientHash == NSMA_currentConsumer)
  {
    NSMA_stopLcClientTimeout();
  }
  else
  {
    auto search = NSMA_clParallelShutdownPendingReceivers.find(client->clientHash);
    if (search != NSMA_clParallelShutdownPendingReceivers.end())
    {
      /* Cancel timeout for one parallel shutdown client */
      search->second->cancelTimer();
      NSMA_clParallelShutdownPendingReceivers.erase(search);
    }
  }
  return retVal;
}

void NSMA_setLcCollectiveTimeout()
{
   std::lock_guard<std::mutex> lock(NSMA__mutexParallelShutdown);

   NSMA_stopLcClientTimeout();
   NSMA_currentConsumer = 0;

   for (const auto clientIdIterator : NSMA_clParallelShutdownPendingReceivers)
   {
      clientIdIterator.second->cancelTimer();
   }
   NSMA_clParallelShutdownPendingReceivers.clear();
}

gboolean NSMA_boDeInit(void)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   gboolean retVal = TRUE;
   NSMA__boInitialized = FALSE;
   NSMA_stopLcClientTimeout();
   NSMA_stopParallelLcClientTimeout();
   for (const auto clientIdIterator : NSMA_clParallelShutdownPendingReceivers)
   {
      clientIdIterator.second->stopTimer();
   }
   retVal &= NSMA_runtime->unregisterService(gCapiDomain, NodeStateConsumer_StubImpl::StubInterface::getInterface(), v1::org::genivi::nodestatemanager::Consumer_INSTANCES[0]);
   retVal &= NSMA_runtime->unregisterService(gCapiDomain, NodeStateLifecycleControl_StubImpl::StubInterface::getInterface(), v1::org::genivi::nodestatemanager::LifecycleControl_INSTANCES[0]);
   std::lock_guard<std::mutex> lock(NSMA__mutex);
   NSMA_consumerService = NULL;
   NSMA_lifecycleControlService = NULL;
   return retVal;
}
