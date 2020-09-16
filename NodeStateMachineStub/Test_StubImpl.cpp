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
#include "Test_StubImpl.hpp"

#include "NodeStateManager.h"
#include "Watchdog.hpp"

#include <string.h>
#include <unistd.h>

void Test_StubImpl::SetNsmData(const std::shared_ptr<CommonAPI::ClientId> __attribute__((__unused__)) _client, GENIVI::NodeStateManagerTypes::NsmDataType_e _DataType,
      std::vector<uint8_t> _Data, uint32_t _DataLen, SetNsmDataReply_t _reply)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   _reply(NsmSetData((NsmDataType_e)(int) _DataType, _Data.data(), _DataLen));
   NSMUnregisterWatchdog();
}

void Test_StubImpl::GetNsmData(const std::shared_ptr<CommonAPI::ClientId> __attribute__((__unused__)) _client, GENIVI::NodeStateManagerTypes::NsmDataType_e _DataType,
      std::vector<uint8_t> _DataIn, uint32_t _DataLen, GetNsmDataReply_t _reply)
{
   NSMTriggerWatchdog(NsmWatchdogState_Active);
   /*
    * The NSM has a read write interface for getting data. The largest data frame that can be
    * exchanged is a NsmSession_s. Therefore, pDataIn is translated into this kind of variable.
    */
   int retVal = 0;
   std::vector<uint8_t> DataOut;

   NsmSession_s pData;
   if(!_DataIn.empty() && sizeof(pData) <= _DataLen)
   {
      memcpy(&pData, _DataIn.data(), _DataLen);
   }
   else
   {
      memset(&pData, 0, sizeof(pData));
   }

   if (_DataLen != (uint32_t)(retVal = NsmGetData((NsmDataType_e)(int) _DataType, (unsigned char*) &pData, _DataLen)))
   {
      memset(&pData, 0, sizeof(pData));
      DataOut.insert(DataOut.begin(), (uint8_t*) &pData, ((uint8_t*) &pData) + sizeof(pData));
   }
   else
   {
      DataOut.insert(DataOut.begin(), (uint8_t*) &pData, ((uint8_t*) &pData) + _DataLen);
   }

   _reply(DataOut, retVal);
   NSMUnregisterWatchdog();
}
