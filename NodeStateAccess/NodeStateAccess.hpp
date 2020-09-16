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

#ifndef NODESTATEACCESS_HPP_
#define NODESTATEACCESS_HPP_

/**********************************************************************************************************************
*
*  HEADER FILE INCLUDES
*
**********************************************************************************************************************/

#include "NodeStateTypes.h" /* NodeStateManagerTypes to communicate with NSM */
#include <gio/gio.h>        /* glib types for easier ICP connection   */

#include <CommonAPI/CommonAPI.hpp>

/**********************************************************************************************************************
*
* The function is called after a (parallel) lifecycle client was informed about the changed life cycle.
* The return value of the last informed client will be evaluated and the next lifecycle client
* to inform will be determined and called.
* If there is no client left, the lifecycle sequence will be finished.
*
* @param pSrcObject: Source object (lifecycle client proxy)
* @param pRes:       Result of asynchronous call
* @param pUserData:  Pointer to the current lifecycle client object
*
* @return void
*
**********************************************************************************************************************/
int32_t NSMA_ClientRequestFinish(const std::shared_ptr<CommonAPI::ClientId> client, int32_t status);

#endif /* NODESTATEACCESS_HPP_ */
