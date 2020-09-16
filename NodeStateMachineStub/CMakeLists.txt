#######################################################################################################################
#
# Copyright (C) 2020 Mentor Graphics (Deutschland) GmbH
#
# Author: Vignesh_Rajendran@mentor.com
#
# CMake file of NodeStateMachineStub
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#######################################################################################################################

cmake_minimum_required(VERSION 3.7.2 FATAL_ERROR)

project(NodeStateMachineStub LANGUAGES CXX)

set(SOURCE_FILES NodeStateMachine.cpp)

if(WITH_TESTS)
    file(GLOB SOURCE_FILES *.cpp)
endif(WITH_TESTS)

add_library(NodeStateMachineStub SHARED ${SOURCE_FILES})

target_compile_features(NodeStateMachineStub PRIVATE cxx_std_11)

target_compile_options(NodeStateMachineStub PRIVATE -Werror -pthread)

target_link_libraries(NodeStateMachineStub PRIVATE
                      ${COMMONAPI_LIBS} 
                      org.genivi.nodestatemanager.commonapi.core.stub
                      org.genivi.nodestatemanager.commonapi.someip.stub
                      pthread)

install(TARGETS NodeStateMachineStub DESTINATION lib)

install(TARGETS NodeStateMachineStub
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} 
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING 
    PATTERN "*.h"
    PATTERN "*.hpp")

