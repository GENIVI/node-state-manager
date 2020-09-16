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

#include "NodeStateLifecycleControl_StubImpl.h"
#include "Watchdog.hpp"

#include <dlt.h>
#include <iostream>

DLT_IMPORT_CONTEXT(NsmaContext);

NodeStateLifecycleControl_StubImpl::NodeStateLifecycleControl_StubImpl(NSMA_tstObjectCallbacks* objectCallbacks)
{
   NSMA__stObjectCallbacks = objectCallbacks;
}

NodeStateLifecycleControl_StubImpl::~NodeStateLifecycleControl_StubImpl()
{
}

void NodeStateLifecycleControl_StubImpl::RequestNodeRestart(const std::shared_ptr<CommonAPI::ClientId> __attribute__((__unused__)) _client, GENIVI::NodeStateManagerTypes::NsmRestartReason_e _RestartReason, uint32_t _RestartType, RequestNodeRestartReply_t _reply)
{
  NSMTriggerWatchdog(NsmWatchdogState_Active);
  if (_RestartReason >= NsmRestartReason_NotSet && _RestartReason <= NsmRestartReason_Last)
  {
    DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: RequestNodeRestart. RestartReason:"), DLT_UINT(_RestartReason), DLT_STRING(RESTARTREASON_STRING[_RestartReason]),
        DLT_STRING("RestartType:"), DLT_UINT(_RestartType));
    GENIVI::NodeStateManagerTypes::NsmErrorStatus_e error = (GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::Literal)NSMA__stObjectCallbacks->pfRequestNodeRestartCb((NsmRestartReason_e) (int) _RestartReason, _RestartType);
    DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: RequestNodeRestart. Reply"), DLT_INT(error));
    _reply(error);
    DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: RequestNodeRestart. Reply finished"));
  }
  else
  {
    DLT_LOG(NsmaContext, DLT_LOG_ERROR, DLT_STRING("NSMA: RequestNodeRestart. Invalid RestartReason:"), DLT_UINT(_RestartReason), DLT_STRING("RestartType:"), DLT_UINT(_RestartType));
  }
  NSMUnregisterWatchdog();
}

void NodeStateLifecycleControl_StubImpl::SetNodeState(const std::shared_ptr<CommonAPI::ClientId> __attribute__((__unused__)) _client, GENIVI::NodeStateManagerTypes::NsmNodeState_e _NodeStateId, SetNodeStateReply_t _reply)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: SetNodeState. NodeStateId:"), DLT_UINT(_NodeStateId));
   _reply(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e((GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::Literal)
         NSMA__stObjectCallbacks->pfSetNodeStateCb((NsmNodeState_e)(int) _NodeStateId)));
   NSMUnregisterWatchdog();
}

void NodeStateLifecycleControl_StubImpl::SetBootMode(const std::shared_ptr<CommonAPI::ClientId> __attribute__((__unused__)) _client, int32_t _BootMode, SetBootModeReply_t _reply)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: SetBootMode. BootMode"), DLT_UINT(_BootMode));
   _reply(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e((GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::Literal)
         NSMA__stObjectCallbacks->pfSetBootModeCb(_BootMode)));
   NSMUnregisterWatchdog();
}

void NodeStateLifecycleControl_StubImpl::SetAppHealthStatus(const std::shared_ptr<CommonAPI::ClientId> __attribute__((__unused__)) _client, std::string _AppName, bool _AppRunning, SetAppHealthStatusReply_t _reply)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: SetAppHealthStatus. AppName:"), DLT_STRING(_AppName.data()), DLT_STRING("AppRunning:"), DLT_BOOL(_AppRunning));
   _reply(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e((GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::Literal)
         NSMA__stObjectCallbacks->pfSetAppHealthStatusCb(_AppName.c_str(), _AppRunning)));
   NSMUnregisterWatchdog();
}

void NodeStateLifecycleControl_StubImpl::CheckLucRequired(const std::shared_ptr<CommonAPI::ClientId> __attribute__((__unused__)) _client, CheckLucRequiredReply_t _reply)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   DLT_LOG(NsmaContext, DLT_LOG_INFO, DLT_STRING("NSMA: CheckLucRequired."));

   _reply(GENIVI::NodeStateManagerTypes::NsmErrorStatus_e((GENIVI::NodeStateManagerTypes::NsmErrorStatus_e::Literal)
         NSMA__stObjectCallbacks->pfCheckLucRequiredCb()));
   NSMUnregisterWatchdog();
}
