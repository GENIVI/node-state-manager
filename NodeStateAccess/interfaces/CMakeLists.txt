#######################################################################################################################
#
# Copyright (C) 2020 Mentor Graphics (Deutschland) GmbH
#
# Author: Sven_Hassler@mentor.com
#
# CMake file of NSM Interfaces
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#######################################################################################################################

cmake_minimum_required(VERSION 3.7.2 FATAL_ERROR)

project(NodeStateAccessInterfaces LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)


###########################################
# Options & settings
###########################################

option(USE_INSTALLED_COMMONAPI "Set to OFF to use the local (build tree) version of CommonAPI" ON)

set(SRC_GEN_DEST "src-gen" CACHE STRING "Path to the destination directory of the generated sources")
if(NOT IS_ABSOLUTE ${SRC_GEN_DEST})
    set(SRC_GEN_DEST "${CMAKE_CURRENT_BINARY_DIR}/${SRC_GEN_DEST}")
endif()

# Core generator:
set(COMMONAPI_CORE_TOOL_GENERATOR "" CACHE STRING "CommonAPI-Core-Tools generator")
if("${COMMONAPI_CORE_TOOL_GENERATOR}" STREQUAL "")
    message(FATAL_ERROR "The file path for the SomeIP generator needs to be specified! Use '-DCOMMONAPI_CORE_TOOL_GENERATOR=<path>'")
    return()
else()
    message("COMMONAPI_CORE_TOOL_GENERATOR specified: ${COMMONAPI_CORE_TOOL_GENERATOR}")
endif()

# SomeIP generator:
set(COMMONAPI_SOMEIP_TOOL_GENERATOR "" CACHE STRING "CommonAPI-SomeIP-Tools generator")
if("${COMMONAPI_SOMEIP_TOOL_GENERATOR}" STREQUAL "")
    message(FATAL_ERROR "The file path for the SomeIP generator needs to be specified! Use '-DCOMMONAPI_SOMEIP_TOOL_GENERATOR=<path>'")
    return()
else()
    message("COMMONAPI_SOMEIP_TOOL_GENERATOR specified: ${COMMONAPI_SOMEIP_TOOL_GENERATOR}")
endif()


###########################################
# Find required packages
###########################################

if("${USE_INSTALLED_COMMONAPI}" STREQUAL "ON")
    find_package(CommonAPI REQUIRED CONFIG NO_CMAKE_PACKAGE_REGISTRY)
    find_package(CommonAPI-SomeIP REQUIRED CONFIG NO_CMAKE_PACKAGE_REGISTRY)
    find_package(vsomeip REQUIRED CONFIG NO_CMAKE_PACKAGE_REGISTRY)
else()
    find_package(CommonAPI REQUIRED CONFIG NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)
    find_package(CommonAPI-SomeIP REQUIRED CONFIG NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)
    find_package(vsomeip REQUIRED CONFIG NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)
endif()

find_package(Java 1.8 EXACT REQUIRED COMPONENTS Development)


###########################################
# Generate code
###########################################

set(INTERFACE_NAME "org.genivi.nodestatemanager")

execute_process(
    COMMAND
        ${COMMONAPI_CORE_TOOL_GENERATOR}
        --dest-common ${SRC_GEN_DEST}/${INTERFACE_NAME}/common/core/
        --dest-proxy ${SRC_GEN_DEST}/${INTERFACE_NAME}/proxy/core/
        --dest-stub ${SRC_GEN_DEST}/${INTERFACE_NAME}/stub/core/
        --dest-skel ${SRC_GEN_DEST}/${INTERFACE_NAME}/stub/core/
        --skel
        ${CMAKE_CURRENT_SOURCE_DIR}/NodeStateManager.fdepl

    COMMAND
        ${COMMONAPI_SOMEIP_TOOL_GENERATOR}
        --dest-common ${SRC_GEN_DEST}/${INTERFACE_NAME}/common/someip/
        --dest-proxy ${SRC_GEN_DEST}/${INTERFACE_NAME}/proxy/someip/
        --dest-stub ${SRC_GEN_DEST}/${INTERFACE_NAME}/stub/someip/
        ${CMAKE_CURRENT_SOURCE_DIR}/NodeStateManager.fdepl

    RESULTS_VARIABLE
        ERRORS
)

list(GET ERRORS 0 CORE_ERROR)
if(NOT ${CORE_ERROR} EQUAL 0)
message(FATAL_ERROR "Error calling core generators: ${CORE_ERROR}")
endif()

list(GET ERRORS 1 SOMEIP_ERROR)
if(NOT ${SOMEIP_ERROR} EQUAL 0)
    message(FATAL_ERROR "Error calling someip generators: ${SOMEIP_ERROR}")
endif()


###########################################
# Build libraries
###########################################

# Core stub:
set(CORE_STUB_NAME "org.genivi.nodestatemanager.commonapi.core.stub")
file(GLOB_RECURSE CORE_STUB_SRCS
    "${SRC_GEN_DEST}/${INTERFACE_NAME}/stub/core/**/*.cpp"
)
set(CORE_STUB_INCLUDES
    "${SRC_GEN_DEST}/${INTERFACE_NAME}/stub/core/"
    "${SRC_GEN_DEST}/${INTERFACE_NAME}/common/core/"
)
set(CORE_STUB_INSTALL_INCLUDES
    "${CMAKE_INSTALL_INCLUDEDIR}/${INTERFACE_NAME}/stub/core/"
    "${CMAKE_INSTALL_INCLUDEDIR}/${INTERFACE_NAME}/common/core/"
)
set(CORE_STUB_LINK_LIBRARIES
    "CommonAPI"
)

add_library(${CORE_STUB_NAME} SHARED ${CORE_STUB_SRCS})
target_link_libraries(${CORE_STUB_NAME} PUBLIC ${CORE_STUB_LINK_LIBRARIES})
target_include_directories(
    ${CORE_STUB_NAME} PUBLIC
    "$<BUILD_INTERFACE:${CORE_STUB_INCLUDES}>"
    "$<INSTALL_INTERFACE:${CORE_STUB_INSTALL_INCLUDES}>"
    ${COMMONAPI_INCLUDE_DIRS}
)

# SomeIP stub:
set(SOMEIP_STUB_NAME "org.genivi.nodestatemanager.commonapi.someip.stub")
file(GLOB_RECURSE SOMEIP_STUB_SRCS
    "${SRC_GEN_DEST}/${INTERFACE_NAME}/stub/someip/**/*.cpp"
    "${SRC_GEN_DEST}/${INTERFACE_NAME}/common/someip/**/*.cpp"
    "${SRC_GEN_DEST}/${INTERFACE_NAME}/common/someip/**/*.cpp"
)
set(SOMEIP_STUB_INCLUDES
    "${SRC_GEN_DEST}/${INTERFACE_NAME}/stub/someip"
    "${SRC_GEN_DEST}/${INTERFACE_NAME}/stub/core"
    "${SRC_GEN_DEST}/${INTERFACE_NAME}/common/someip"
    "${SRC_GEN_DEST}/${INTERFACE_NAME}/common/core"
)
set(SOMEIP_STUB_INSTALL_INCLUDES
    "${CMAKE_INSTALL_INCLUDEDIR}/${INTERFACE_NAME}/stub/someip"
    "${CMAKE_INSTALL_INCLUDEDIR}/${INTERFACE_NAME}/stub/core"
    "${CMAKE_INSTALL_INCLUDEDIR}/${INTERFACE_NAME}/common/someip"
    "${CMAKE_INSTALL_INCLUDEDIR}/${INTERFACE_NAME}/common/core"
)
set(SOMEIP_STUB_LINK_LIBRARIES
    "CommonAPI"
    "CommonAPI-SomeIP"
)

add_library(${SOMEIP_STUB_NAME} SHARED ${SOMEIP_STUB_SRCS})
target_link_libraries(${SOMEIP_STUB_NAME} PUBLIC ${SOMEIP_STUB_LINK_LIBRARIES})
target_include_directories(
    ${SOMEIP_STUB_NAME} PUBLIC
    ${COMMONAPI_INCLUDE_DIRS}
    ${COMMONAPI_SOMEIP_INCLUDE_DIRS}
    "$<BUILD_INTERFACE:${SOMEIP_STUB_INCLUDES}>"
    "$<INSTALL_INTERFACE:${SOMEIP_STUB_INSTALL_INCLUDES}>"
)

# SomeIP proxy:
set(SOMEIP_PROXY_NAME "org.genivi.nodestatemanager.commonapi.someip.proxy")
file(GLOB_RECURSE SOMEIP_PROXY_SRCS
    "${SRC_GEN_DEST}/${INTERFACE_NAME}/proxy/someip/**/*.cpp"
    "${SRC_GEN_DEST}/${INTERFACE_NAME}/common/someip/**/*.cpp"
    "${SRC_GEN_DEST}/${INTERFACE_NAME}/common/someip/**/*.cpp"
)
set(SOMEIP_PROXY_INCLUDES
    "${SRC_GEN_DEST}/${INTERFACE_NAME}/proxy/someip"
    "${SRC_GEN_DEST}/${INTERFACE_NAME}/proxy/core"
    "${SRC_GEN_DEST}/${INTERFACE_NAME}/common/someip"
    "${SRC_GEN_DEST}/${INTERFACE_NAME}/common/core"
)
set(SOMEIP_PROXY_INSTALL_INCLUDES
    "${CMAKE_INSTALL_INCLUDEDIR}/${INTERFACE_NAME}/proxy/someip"
    "${CMAKE_INSTALL_INCLUDEDIR}/${INTERFACE_NAME}/proxy/core"
    "${CMAKE_INSTALL_INCLUDEDIR}/${INTERFACE_NAME}/common/someip"
    "${CMAKE_INSTALL_INCLUDEDIR}/${INTERFACE_NAME}/common/core"
)
set(SOMEIP_PROXY_LINK_LIBRARIES
    "CommonAPI"
    "CommonAPI-SomeIP"
)

add_library(${SOMEIP_PROXY_NAME} SHARED ${SOMEIP_PROXY_SRCS})
target_link_libraries(${SOMEIP_PROXY_NAME} PUBLIC ${SOMEIP_PROXY_LINK_LIBRARIES})
target_include_directories(
    ${SOMEIP_PROXY_NAME} PUBLIC
    ${COMMONAPI_INCLUDE_DIRS}
    ${COMMONAPI_SOMEIP_INCLUDE_DIRS}
    "$<BUILD_INTERFACE:${SOMEIP_PROXY_INCLUDES}>"
    "$<INSTALL_INTERFACE:${SOMEIP_PROXY_INSTALL_INCLUDES}>"
)


###########################################
# Install

# Install libraries:
install(
    TARGETS ${CORE_STUB_NAME}
    EXPORT ${CORE_STUB_NAME}Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(
    TARGETS ${SOMEIP_STUB_NAME}
    EXPORT ${SOMEIP_STUB_NAME}Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(
    TARGETS ${SOMEIP_PROXY_NAME}
    EXPORT ${SOMEIP_PROXY_NAME}Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Install includes:
install(
    DIRECTORY ${CORE_STUB_INCLUDES}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${CORE_STUB_NAME}"
    FILES_MATCHING PATTERN "*.hpp"
)

install(
    DIRECTORY ${SOMEIP_STUB_INCLUDES}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${SOMEIP_STUB_NAME}"
    FILES_MATCHING PATTERN "*.hpp"
)

install(
    DIRECTORY ${SOMEIP_PROXY_INCLUDES}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${SOMEIP_PROXY_NAME}"
    FILES_MATCHING PATTERN "*.hpp"
)

# Create and install package configuration files:
configure_package_config_file(
    cmake/core_stub_config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${CORE_STUB_NAME}Config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${CORE_STUB_NAME}
)
# install(
#     FILES ${CMAKE_CURRENT_BINARY_DIR}/${CORE_STUB_NAME}Config.cmake
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${CORE_STUB_NAME}
# )

configure_package_config_file(
    cmake/someip_stub_config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${SOMEIP_STUB_NAME}Config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${SOMEIP_STUB_NAME}
)
# install(
#     FILES ${CMAKE_CURRENT_BINARY_DIR}/${SOMEIP_STUB_NAME}Config.cmake
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${SOMEIP_STUB_NAME}
# )

configure_package_config_file(
    cmake/someip_proxy_config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${SOMEIP_PROXY_NAME}Config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${SOMEIP_PROXY_NAME}
)
# install(
#     FILES ${CMAKE_CURRENT_BINARY_DIR}/${SOMEIP_PROXY_NAME}Config.cmake
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${SOMEIP_PROXY_NAME}
# )

# Create and install target files:
export(EXPORT ${CORE_STUB_NAME}Targets)
install(
    EXPORT ${CORE_STUB_NAME}Targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${CORE_STUB_NAME}
)

export(EXPORT ${SOMEIP_STUB_NAME}Targets)
install(
    EXPORT ${SOMEIP_STUB_NAME}Targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${SOMEIP_STUB_NAME}
)

export(EXPORT ${SOMEIP_PROXY_NAME}Targets)
install(
    EXPORT ${SOMEIP_PROXY_NAME}Targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${SOMEIP_PROXY_NAME}
)

# Create and install version files:
write_basic_package_version_file(
    ${CORE_STUB_NAME}ConfigVersion.cmake
    VERSION 1.0
    COMPATIBILITY SameMajorVersion
)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/${CORE_STUB_NAME}ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${CORE_STUB_NAME}
)

write_basic_package_version_file(
    ${SOMEIP_STUB_NAME}ConfigVersion.cmake
    VERSION 1.0
    COMPATIBILITY SameMajorVersion
)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/${SOMEIP_STUB_NAME}ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${SOMEIP_STUB_NAME}
)

write_basic_package_version_file(
    ${SOMEIP_PROXY_NAME}ConfigVersion.cmake
    VERSION 1.0
    COMPATIBILITY SameMajorVersion
)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/${SOMEIP_PROXY_NAME}ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${SOMEIP_PROXY_NAME}
)

# Export the packages for use from the build-tree:
# (this registers the build-tree with the global CMake-registry)
export(PACKAGE ${CORE_STUB_NAME})
export(PACKAGE ${SOMEIP_STUB_NAME})
export(PACKAGE ${SOMEIP_PROXY_NAME})
