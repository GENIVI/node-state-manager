/**********************************************************************************************************************
*
* Copyright (C) 2017 BMW AG
*
* The header file defines the interfaces offered by the NodeStateMachine stub.
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
**********************************************************************************************************************/

#ifndef TEST_STUBIMPL_HPP
#define TEST_STUBIMPL_HPP

#include <v1/org/genivi/nodestatemachinetest/TestStubDefault.hpp>
#include <v1/org/genivi/NodeStateManagerTypes.hpp>

namespace GENIVI = v1::org::genivi;
namespace NodeStateMachineTest = v1::org::genivi::nodestatemachinetest;

/*********************************************************************************************
 *
 * NodeStateMachineTest Class
 *
 *********************************************************************************************/

class Test_StubImpl: public NodeStateMachineTest::TestStubDefault
{

public:
   void SetNsmData(const std::shared_ptr<CommonAPI::ClientId> _client, GENIVI::NodeStateManagerTypes::NsmDataType_e _DataType,
                   std::vector<uint8_t> _Data, uint32_t _DataLen, SetNsmDataReply_t _reply);

   void GetNsmData(const std::shared_ptr<CommonAPI::ClientId> _client, GENIVI::NodeStateManagerTypes::NsmDataType_e _DataType,
                   std::vector<uint8_t> _DataIn, uint32_t _DataLen, GetNsmDataReply_t _reply);

};



#endif /* NODESTATEMACHINELIFECYCLE_TEST_STUBIMPL_HPP */
