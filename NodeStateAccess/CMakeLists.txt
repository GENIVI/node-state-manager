#######################################################################################################################
#
# Copyright (C) 2020 Mentor Graphics (Deutschland) GmbH
#
# Author: Vignesh_Rajendran@mentor.com
#
# CMake file of NodeStateAccess
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#######################################################################################################################

cmake_minimum_required(VERSION 3.7.2 FATAL_ERROR)

project(NodeStateAccess LANGUAGES CXX)

file(GLOB SOURCE_FILES *.cpp)

add_library(NodeStateAccess SHARED ${SOURCE_FILES})

target_compile_features(NodeStateAccess PRIVATE cxx_std_11)

target_compile_options(NodeStateAccess PRIVATE -Werror -pthread)

target_link_libraries(NodeStateAccess PRIVATE
                      ${COMMONAPI_LIBS}
                      ${DLT_LIBRARIES}
                      org.genivi.nodestatemanager.commonapi.core.stub
                      org.genivi.nodestatemanager.commonapi.someip.stub
                      pthread)

install(TARGETS NodeStateAccess
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} 
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING 
    PATTERN "*.h"
    PATTERN "*.hpp"
    PATTERN "interfaces" EXCLUDE)