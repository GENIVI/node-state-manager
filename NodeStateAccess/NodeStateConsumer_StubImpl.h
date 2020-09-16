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

#ifndef NODESTATECONSUMERSTUBIMPL_H_
#define NODESTATECONSUMERSTUBIMPL_H_

#include <CommonAPI/CommonAPI.hpp>
#include <v1/org/genivi/nodestatemanager/ConsumerStubDefault.hpp>
#include <v1/org/genivi/NodeStateManagerTypes.hpp>

#include "NodeStateAccess.h"
#include "NodeStateAccess.hpp"

namespace GENIVI = v1::org::genivi;
using namespace GENIVI::nodestatemanager;

class NodeStateConsumer_StubImpl: public ConsumerStubDefault {

public:
   NodeStateConsumer_StubImpl(NSMA_tstObjectCallbacks* NSMA__stObjectCallbacks);

   /**
    * The following functions implements the interfaces defined in fidl/NodeStateManager.fidl
    * Please have a look at this fidl file for more information.
    */
   virtual void GetNodeState(const std::shared_ptr<CommonAPI::ClientId> _client, GetNodeStateReply_t _reply);
   virtual void SetSessionState(const std::shared_ptr<CommonAPI::ClientId> _client, std::string _SessionName, std::string _SessionOwner, GENIVI::NodeStateManagerTypes::NsmSeat_e _SeatID, GENIVI::NodeStateManagerTypes::NsmSessionState_e _SessionState, SetSessionStateReply_t _reply);
   virtual void GetSessionState(const std::shared_ptr<CommonAPI::ClientId> _client, std::string _SessionName, GENIVI::NodeStateManagerTypes::NsmSeat_e _SeatID, GetSessionStateReply_t _reply);
   virtual void RegisterShutdownClient(const std::shared_ptr<CommonAPI::ClientId> _client, uint32_t _ShutdownMode, uint32_t _TimeoutMs, RegisterShutdownClientReply_t _reply);
   virtual void UnRegisterShutdownClient(const std::shared_ptr<CommonAPI::ClientId> _client, uint32_t _ShutdownMode, UnRegisterShutdownClientReply_t _reply);
   virtual void RegisterSession(const std::shared_ptr<CommonAPI::ClientId> _client, std::string _SessionName, std::string _SessionOwner, GENIVI::NodeStateManagerTypes::NsmSeat_e _SeatID, GENIVI::NodeStateManagerTypes::NsmSessionState_e _SessionState, RegisterSessionReply_t _reply);
   virtual void UnRegisterSession(const std::shared_ptr<CommonAPI::ClientId> _client, std::string _SessionName, std::string _SessionOwner, GENIVI::NodeStateManagerTypes::NsmSeat_e _SeatID, UnRegisterSessionReply_t _reply);
   virtual void GetAppHealthCount(const std::shared_ptr<CommonAPI::ClientId> _client, GetAppHealthCountReply_t _reply);
   virtual void GetInterfaceVersion(const std::shared_ptr<CommonAPI::ClientId> _client, GetInterfaceVersionReply_t _reply);
   virtual void LifecycleRequestComplete(const std::shared_ptr<CommonAPI::ClientId> _client, GENIVI::NodeStateManagerTypes::NsmErrorStatus_e _Status, LifecycleRequestCompleteReply_t _reply);

   /**
    * This function is called by CommonAPI when new clients register for ShutdownEvents
    */
   virtual void onShutdownEventsSelectiveSubscriptionChanged(const std::shared_ptr<CommonAPI::ClientId> _client, const CommonAPI::SelectiveBroadcastSubscriptionEvent _event);
private:
   NSMA_tstObjectCallbacks* NSMA__stObjectCallbacks = NULL;
   std::mutex mMutex;
};

#endif /* NODESTATECONSUMERSTUBIMPL_H_ */
