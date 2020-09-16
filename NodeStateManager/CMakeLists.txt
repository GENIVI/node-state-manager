#######################################################################################################################
#
# Copyright (C) 2020 Mentor Graphics (Deutschland) GmbH
#
# Author: Vignesh_Rajendran@mentor.com
#
# CMake file of NodeStateManager
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#######################################################################################################################


cmake_minimum_required(VERSION 3.7.2 FATAL_ERROR)

project(NodeStateManager LANGUAGES C CXX)

set(SOURCE_FILES NodeStateManager.c Watchdog.cpp)
set(SYSTEMD_SERVICE config/nodestatemanager-daemon.service)
set(SYSTEMD_SERVICES_INSTALL_DIR "/etc/systemd/system/")

add_definitions(-DWATERMARK="${PROJECT_VERSION}")

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -export-dynamic")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-variable -Wno-unused-parameter")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(
    ${CMAKE_SOURCE_DIR}/NodeStateAccess
    ${CMAKE_SOURCE_DIR}/NodeStateMachineStub
)

add_executable(NodeStateManager ${SOURCE_FILES})

target_link_libraries(NodeStateManager PRIVATE
                      NodeStateAccess
                      NodeStateMachineStub
                      ${DLT_LIBRARIES}
                      pthread
                      ${GLIB_LIBRARIES}
                      ${SYSTEMD_LIBRARIES})

install(TARGETS NodeStateManager DESTINATION bin)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp")

if(WITH_SYSTEMD_SERVICE)
    install(
        FILES ${CMAKE_CURRENT_SOURCE_DIR}/${SYSTEMD_SERVICE}
        DESTINATION ${SYSTEMD_SERVICES_INSTALL_DIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ
    )
endif(WITH_SYSTEMD_SERVICE)
