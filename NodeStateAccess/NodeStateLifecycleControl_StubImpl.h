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

#ifndef NODESTATELIFECYCLECONTROLSTUBIMPL_H_
#define NODESTATELIFECYCLECONTROLSTUBIMPL_H_

#include <CommonAPI/CommonAPI.hpp>
#include <v1/org/genivi/nodestatemanager/LifecycleControlStubDefault.hpp>

#include "NodeStateAccess.h"

namespace GENIVI = v1::org::genivi;
using namespace GENIVI::nodestatemanager;

class NodeStateLifecycleControl_StubImpl: public LifecycleControlStubDefault {

public:
   NodeStateLifecycleControl_StubImpl(NSMA_tstObjectCallbacks* NSMA__stObjectCallbacks);
   virtual ~NodeStateLifecycleControl_StubImpl();

   /**
    * The following functions implements the interfaces defined in fidl/NodeStateManager.fidl
    * Please have a look at this fidl file for more information.
    */
   virtual void RequestNodeRestart(const std::shared_ptr<CommonAPI::ClientId> _client, GENIVI::NodeStateManagerTypes::NsmRestartReason_e _RestartReason, uint32_t _RestartType, RequestNodeRestartReply_t _reply);
   virtual void SetNodeState(const std::shared_ptr<CommonAPI::ClientId> _client, GENIVI::NodeStateManagerTypes::NsmNodeState_e _NodeStateId, SetNodeStateReply_t _reply);
   virtual void SetBootMode(const std::shared_ptr<CommonAPI::ClientId> _client, int32_t _BootMode, SetBootModeReply_t _reply);
   virtual void SetAppHealthStatus(const std::shared_ptr<CommonAPI::ClientId> _client, std::string _AppName, bool _AppRunning, SetAppHealthStatusReply_t _reply);
   virtual void CheckLucRequired(const std::shared_ptr<CommonAPI::ClientId> _client, CheckLucRequiredReply_t _reply);
private:
   NSMA_tstObjectCallbacks* NSMA__stObjectCallbacks = NULL;
};

#endif /* NODESTATELIFECYCLECONTROLSTUBIMPL_H_ */
