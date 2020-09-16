#######################################################################################################################
#
# Copyright (C) 2020 Mentor Graphics (Deutschland) GmbH
#
# Author: Vignesh_Rajendran@mentor.com
#
# CMake file of NSM
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#######################################################################################################################


cmake_minimum_required(VERSION 3.7.2 FATAL_ERROR)

project(nsm LANGUAGES C CXX)

# set version
set(NSM_VERSION_MAJOR   3)
set(NSM_VERSION_MINOR   0)
set(NSM_VERSION_BUGFIX  0)

include(CMakeDependentOption)
include(GNUInstallDirs)

# set default build type, if not defined by user
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
                         "Choose build type: Debug, Release"
                         FORCE)
    message(STATUS "Build type not defined. Using default build type 'Release'.")
endif()


option(WITH_TESTS "Build together with unitTest" OFF)
message(STATUS "WITH_TESTS is set to value: ${WITH_TESTS}")

option(CODE_COVERAGE "Enable coverage reporting" OFF)
message(STATUS "CODE_COVERAGE is set to value: ${CODE_COVERAGE}")

if(CMAKE_BUILD_TYPE STREQUAL "coverage" OR CODE_COVERAGE)
    if("${CMAKE_C_COMPILER_ID}" MATCHES "(Apple)?[Cc]lang" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "(Apple)?[Cc]lang")
        message("Building with llvm Code Coverage Tools")

        # Warning/Error messages
        if(NOT LLVM_COV_PATH)
            message(FATAL_ERROR "llvm-cov not found! Aborting.")
        endif()

        # set Flags
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate -fcoverage-mapping")

    elseif(CMAKE_COMPILER_IS_GNUCXX)
        message("Building with lcov Code Coverage Tools")

        # Warning/Error messages
        if(NOT (CMAKE_BUILD_TYPE STREQUAL "Debug"))
            message(FATAL_ERROR "Code coverage results with an optimized (non-Debug) build may be misleading")
        endif()

        if(NOT WITH_TESTS)
            message(FATAL_ERROR "test is not enabled! Aborting.")
        endif()

        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 --coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 --coverage -fprofile-arcs -ftest-coverage")
    else()
        message(FATAL_ERROR "Code coverage requires Clang or GCC. Aborting.")
    endif()
endif()


OPTION(USE_INSTALLED_COMMONAPI "Set to OFF to use the local (build tree) version of CommonAPI" ON)
message(STATUS "USE_INSTALLED_COMMONAPI is set to value: ${USE_INSTALLED_COMMONAPI}")

OPTION(WITH_SYSTEMD_SERVICE "Install systemd service file" OFF)
message(STATUS "WITH_SYSTEMD_SERVICE is set to value: ${WITH_SYSTEMD_SERVICE}")

find_package(PkgConfig REQUIRED)

pkg_check_modules(DLT      REQUIRED   automotive-dlt>=2.2.0)
pkg_check_modules(GIO      REQUIRED   gio-2.0>=2.30.0)
pkg_check_modules(GIO_UNIX REQUIRED   gio-unix-2.0>=2.30.0)
pkg_check_modules(GLIB     REQUIRED   glib-2.0>=2.30.0)
pkg_check_modules(GOBJECT  REQUIRED   gobject-2.0>=2.30.0)
pkg_check_modules(SYSTEMD  REQUIRED   libsystemd>=209)

find_package(CommonAPI 3.1.12 REQUIRED CONFIG NO_CMAKE_PACKAGE_REGISTRY)

find_package(org.genivi.nodestatemanager.commonapi.core.stub  REQUIRED)
find_package(org.genivi.nodestatemanager.commonapi.someip.stub REQUIRED)
find_package(org.genivi.nodestatemanager.commonapi.someip.proxy  REQUIRED)
find_package(vsomeip3 REQUIRED)

if(WITH_TESTS)
    find_package(GTest REQUIRED)
    include(CTest)
    add_definitions(-DENABLE_TESTS)
endif(WITH_TESTS)

# cflags
set(NSM_CMAKE_C_FLAGS "-Wall -Wextra -Werror -fstack-protector-strong -D_GLIBCXX_USE_NANOSLEEP -pthread -Wformat -Wformat-security -fexceptions -fstrict-aliasing -fasynchronous-unwind-tables -fno-omit-frame-pointer -Wconversion -Wno-sign-conversion -Wno-unused-parameter")
set(NSM_CMAKE_CXX_FLAGS "-Wall -Wextra -Werror -fstack-protector-strong -D_GLIBCXX_USE_NANOSLEEP -pthread -Wformat -Wformat-security -fexceptions -fstrict-aliasing -fasynchronous-unwind-tables -fno-omit-frame-pointer -Wconversion -Wno-sign-conversion")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${NSM_CMAKE_CXX_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${NSM_CMAKE_CXX_FLAGS}")

# ldflags
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-as-needed")

include_directories(
    ${COMMONAPI_INCLUDE_DIRS}
    ${COMMONAPI_SOMEIP_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${GLIB2_INCLUDE_DIRS}
    ${VSOMEIP_INCLUDE_DIRS}
    ${DLT_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/NodeStateManager
    ${VSOMEIP3_INCLUDE_DIRS}
)

add_subdirectory(NodeStateMachineStub)
add_subdirectory(NodeStateAccess)
add_subdirectory(NodeStateManager)

if(WITH_TESTS)
    add_subdirectory(NodeStateMachineTest)
endif(WITH_TESTS)