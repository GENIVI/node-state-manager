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

#include "NodeStateConsumer_StubImpl.h"
#include "Watchdog.hpp"

#include <dlt.h>
#include <iostream>

DLT_IMPORT_CONTEXT(NsmaContext);

NodeStateConsumer_StubImpl::NodeStateConsumer_StubImpl(NSMA_tstObjectCallbacks* objectCallbacks)
{
   NSMA__stObjectCallbacks = objectCallbacks;
   trySetBootModeAttribute(0);
   setRestartReasonAttribute(GENIVI::NodeStateManagerTypes::NsmRestartReason_e::NsmRestartReason_NotSet);
   setShutdownReasonAttribute(GENIVI::NodeStateManagerTypes::NsmShutdownReason_e::NsmShutdownReason_NotSet);
   setWakeUpReasonAttribute(GENIVI::NodeStateManagerTypes::NsmRunningReason_e::NsmRunningReason_NotSet);
}

void NodeStateConsumer_StubImpl::GetInterfaceVersion(const std::shared_ptr<CommonAPI::ClientId> _client, GetInterfaceVersionReply_t _reply)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   CommonAPI::Version v = ConsumerStubDefault::getInterfaceVersion(_client);
   _reply( (v.Major << 24) | (v.Minor << 16));
   NSMUnregisterWatchdog();
}

void NodeStateConsumer_StubImpl::GetNodeState(const std::shared_ptr<CommonAPI::ClientId> __attribute__((__unused__)) _client, GetNodeStateReply_t _reply)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: GetNodeState."));

   NsmNodeState_e enNodeState = NsmNodeState_NotSet;
   NsmErrorStatus_e nodeStateId = NSMA__stObjectCallbacks->pfGetNodeStateCb(&enNodeState);
   _reply(GENIVI::NodeStateManagerTypes::NsmNodeState_e((GENIVI::NodeStateManagerTypes::NsmNodeState_e::Literal) enNodeState),
          GENIVI::NodeStateManagerTypes::NsmErrorStatus_e((GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::Literal) nodeStateId));
   NSMUnregisterWatchdog();
}

void NodeStateConsumer_StubImpl::SetSessionState(const std::shared_ptr<CommonAPI::ClientId> __attribute__((__unused__)) _client, std::string _SessionName, std::string _SessionOwner, GENIVI::NodeStateManagerTypes::NsmSeat_e _SeatID, GENIVI::NodeStateManagerTypes::NsmSessionState_e _SessionState, SetSessionStateReply_t _reply)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: SetSessionState. SessionName:"), DLT_STRING(_SessionName.data()),
         DLT_STRING("SessionOwner:"), DLT_STRING(_SessionOwner.data()),
         DLT_STRING("SeatID:"), DLT_INT(_SeatID),
         DLT_STRING("SessionState:"), DLT_INT(_SessionState));

   _reply(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e((GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::Literal)
         NSMA__stObjectCallbacks->pfSetSessionStateCb(_SessionName.c_str(), _SessionOwner.c_str(), (NsmSeat_e)(int) _SeatID, (NsmSessionState_e)(int) _SessionState)));
   NSMUnregisterWatchdog();
}

void NodeStateConsumer_StubImpl::GetSessionState(const std::shared_ptr<CommonAPI::ClientId> __attribute__((__unused__)) _client, std::string _SessionName, GENIVI::NodeStateManagerTypes::NsmSeat_e _SeatID, GetSessionStateReply_t _reply)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: GetSessionState."));
   NsmSessionState_e enSessionState = NsmSessionState_Unregistered;
   NsmErrorStatus_e errorCode = NSMA__stObjectCallbacks->pfGetSessionStateCb(_SessionName.c_str(), (NsmSeat_e)(int) _SeatID, &enSessionState);
   DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: GetSessionState. Reply:"), DLT_INT(enSessionState), DLT_INT(errorCode));
   _reply(GENIVI::NodeStateManagerTypes::NsmSessionState_e((GENIVI::NodeStateManagerTypes::NsmSessionState_e::Literal)enSessionState),
          GENIVI::NodeStateManagerTypes::NsmErrorStatus_e((GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::Literal)errorCode));
   DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: GetSessionState. Reply done"));
   NSMUnregisterWatchdog();
}

void NodeStateConsumer_StubImpl::RegisterShutdownClient(const std::shared_ptr<CommonAPI::ClientId> _client, uint32_t _ShutdownMode, uint32_t _TimeoutMs, RegisterShutdownClientReply_t _reply)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: RegisterShutdownClient. ShutdownMode:"), DLT_UINT(_ShutdownMode),
                                      DLT_STRING("TimeoutMs:"), DLT_INT(_TimeoutMs));
   std::unique_lock<std::mutex> lock(mMutex);
   /*
    *   Pass NULL because client should already be registered by onShutdownEventsSelectiveSubscriptionChanged.
    *   Just update ShutdownMode and TimeoutMs
    */
   NsmErrorStatus_e errorCode =  NSMA__stObjectCallbacks->pfRegisterLifecycleClientCb(_client->hashCode(), _ShutdownMode, _TimeoutMs);
   /* NSMA returns NsmErrorStatus_Last when client already registered -> this is the normal case here */
   errorCode = errorCode == NsmErrorStatus_Last ? NsmErrorStatus_Ok : errorCode;

   _reply(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e((GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::Literal) errorCode ));
   lock.unlock();
   NSMUnregisterWatchdog();
}

void NodeStateConsumer_StubImpl::UnRegisterShutdownClient(const std::shared_ptr<CommonAPI::ClientId> _client, uint32_t _ShutdownMode, UnRegisterShutdownClientReply_t _reply)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: UnRegisterShutdownClient. ShutdownMode:"), DLT_UINT(_ShutdownMode));
   std::unique_lock<std::mutex> lock(mMutex);
   NsmErrorStatus_e enRetVal = NSMA__stObjectCallbacks->pfUnRegisterLifecycleClientCb(_client->hashCode(), _ShutdownMode);

   if(enRetVal != NsmErrorStatus_Ok)
   {
     DLT_LOG(NsmaContext, DLT_LOG_WARN, DLT_STRING("NSM: Failed to unregister lifecycle consumer."),
                                        DLT_STRING("Client hash:"),  DLT_UINT64(_client->hashCode()),
                                        DLT_STRING("Unregistered mode(s):"), DLT_INT(_ShutdownMode));
   }
   _reply(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e((GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::Literal)enRetVal));
   lock.unlock();
   NSMUnregisterWatchdog();
}

void NodeStateConsumer_StubImpl::RegisterSession(const std::shared_ptr<CommonAPI::ClientId> __attribute__((__unused__)) _client, std::string _SessionName, std::string _SessionOwner, GENIVI::NodeStateManagerTypes::NsmSeat_e _SeatID, GENIVI::NodeStateManagerTypes::NsmSessionState_e _SessionState, RegisterSessionReply_t _reply)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: RegisterSession. SessionName:"), DLT_STRING(_SessionName.data()),
           DLT_STRING("SessionOwner:"), DLT_STRING(_SessionOwner.data()),
           DLT_STRING("SeatID:"), DLT_INT(_SeatID),
           DLT_STRING("SessionState:"), DLT_INT(_SessionState));
   _reply(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e((GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::Literal)NSMA__stObjectCallbacks->pfRegisterSessionCb(_SessionName.c_str(), _SessionOwner.c_str(), (NsmSeat_e)(int) _SeatID, (NsmSessionState_e)(int) _SessionState)));
   NSMUnregisterWatchdog();
}

void NodeStateConsumer_StubImpl::UnRegisterSession(const std::shared_ptr<CommonAPI::ClientId> __attribute__((__unused__)) _client, std::string _SessionName, std::string _SessionOwner, GENIVI::NodeStateManagerTypes::NsmSeat_e _SeatID, UnRegisterSessionReply_t _reply)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: UnRegisterSession. SessionName:"), DLT_STRING(_SessionName.data()),
           DLT_STRING("SessionOwner:"), DLT_STRING(_SessionOwner.data()),
           DLT_STRING("SeatID:"), DLT_INT(_SeatID));
   _reply(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e((GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::Literal)NSMA__stObjectCallbacks->pfUnRegisterSessionCb(_SessionName.c_str(), _SessionOwner.c_str(), (NsmSeat_e)(int) _SeatID)));
   NSMUnregisterWatchdog();
}

void NodeStateConsumer_StubImpl::GetAppHealthCount(const std::shared_ptr<CommonAPI::ClientId> __attribute__((__unused__)) _client, GetAppHealthCountReply_t _reply)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: GetAppHealthCount."));
   _reply(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e((GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::Literal)NSMA__stObjectCallbacks->pfGetAppHealthCountCb()));
   NSMUnregisterWatchdog();
}

void NodeStateConsumer_StubImpl::LifecycleRequestComplete(const std::shared_ptr<CommonAPI::ClientId> _client, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e _Status, LifecycleRequestCompleteReply_t _reply)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   size_t client = _client->hashCode();
   if(_Status >= NsmErrorStatus_NotSet && _Status <= NsmErrorStatus_Last)
   {
     DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: LifecycleRequestComplete Client: "), DLT_INT64(client), DLT_STRING(" Return Value: "), DLT_STRING(ERRORSTATUS_STRING[_Status]));
     std::lock_guard<std::mutex> lock(mMutex);
     _reply(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e((GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::Literal)NSMA_ClientRequestFinish(_client, _Status)));
   }
   else
   {
     DLT_LOG(NsmaContext, DLT_LOG_ERROR, DLT_STRING("NSMA: LifecycleRequestComplete Client: "), DLT_INT64(client), DLT_STRING(" Invalid return value: "), DLT_INT(_Status));
   }
   NSMUnregisterWatchdog();
}

void NodeStateConsumer_StubImpl::onShutdownEventsSelectiveSubscriptionChanged(const std::shared_ptr<CommonAPI::ClientId> _client, const CommonAPI::SelectiveBroadcastSubscriptionEvent _event)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   std::lock_guard<std::mutex> lock(mMutex);
   if (_event == CommonAPI::SelectiveBroadcastSubscriptionEvent::SUBSCRIBED)
   {
      NSMA__stObjectCallbacks->pfRegisterLifecycleClientCb(_client->hashCode(), NSM_SHUTDOWNTYPE_NOT, 0);
   }
   else if (_event == CommonAPI::SelectiveBroadcastSubscriptionEvent::UNSUBSCRIBED)
   {
      NSMA__stObjectCallbacks->pfUnRegisterLifecycleClientCb(_client->hashCode(), NSM_SHUTDOWNTYPE_FAST | NSM_SHUTDOWNTYPE_NORMAL | NSM_SHUTDOWNTYPE_PARALLEL | NSM_SHUTDOWNTYPE_RUNUP);
   }
   NSMUnregisterWatchdog();
}
