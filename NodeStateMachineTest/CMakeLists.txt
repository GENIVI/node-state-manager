#######################################################################################################################
#
# Copyright (C) 2020 Mentor Graphics (Deutschland) GmbH
#
# Author: Vignesh_Rajendran@mentor.com
#
# CMake file of NodeStateMachineTest
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#######################################################################################################################

cmake_minimum_required(VERSION 3.7.2 FATAL_ERROR)

enable_testing()

set(SOURCE_STRESS NodeStateTest_STRESS.cpp)
set(SOURCE_TIMEOUT NodeStateTest_TIMEOUT.cpp)
set(SOURCE_GENIVI NodeStateTest_GENIVI.cpp)
set(SOURCE_WATCHDOG NodeStateTest_WATCHDOG.cpp ${CMAKE_SOURCE_DIR}/NodeStateManager/Watchdog.cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")
set(CMAKE_EXE_LINKER_FLAGS  "-Wl,--no-as-needed")

# copy test script to binary dir
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/test/run_test.sh
    ${CMAKE_CURRENT_BINARY_DIR}/run_test.sh
    COPYONLY
)

# stress test
add_executable(nsm_test_stress ${SOURCE_STRESS})
target_link_libraries(nsm_test_stress PRIVATE
                      ${COMMONAPI_LIBS}
                      ${DLT_LIBRARIES}
                      ${GTEST_LIBRARY}
                      org.genivi.nodestatemanager.commonapi.someip.proxy
                      pthread)
add_test(NAME nsm_test_stress COMMAND bash -c "${CMAKE_CURRENT_BINARY_DIR}/run_test.sh ${CMAKE_CURRENT_BINARY_DIR}/nsm_test_stress")
set_tests_properties(nsm_test_stress PROPERTIES TIMEOUT 15)

# timeout test
add_executable(nsm_test_timeout ${SOURCE_TIMEOUT})

target_link_libraries(nsm_test_timeout PRIVATE
                      ${COMMONAPI_LIBS}
                      ${DLT_LIBRARIES}
                      ${GTEST_LIBRARY}
                      org.genivi.nodestatemanager.commonapi.someip.proxy
                      pthread)

add_test(NAME nsm_test_timeout COMMAND bash -c "${CMAKE_CURRENT_BINARY_DIR}/run_test.sh ${CMAKE_CURRENT_BINARY_DIR}/nsm_test_timeout")
set_tests_properties(nsm_test_timeout PROPERTIES TIMEOUT 40)

# genivi test
add_executable(nsm_test_genivi ${SOURCE_GENIVI})

target_link_libraries(nsm_test_genivi PRIVATE
                      ${COMMONAPI_LIBS}
                      ${DLT_LIBRARIES}
                      ${GTEST_LIBRARY}
                      org.genivi.nodestatemanager.commonapi.someip.proxy
                      pthread)
add_test(NAME nsm_test_genivi COMMAND bash -c "${CMAKE_CURRENT_BINARY_DIR}/run_test.sh ${CMAKE_CURRENT_BINARY_DIR}/nsm_test_genivi")
set_tests_properties(nsm_test_genivi PROPERTIES TIMEOUT 40)

# watchdog test
add_executable(nsm_test_watchdog ${SOURCE_WATCHDOG})

target_link_libraries(nsm_test_watchdog PRIVATE
                      ${DLT_LIBRARIES}
                      ${GTEST_LIBRARY}
                      pthread)
add_test(NAME nsm_test_watchdog COMMAND bash -c "${CMAKE_CURRENT_BINARY_DIR}/run_test.sh ${CMAKE_CURRENT_BINARY_DIR}//nsm_test_watchdog")
set_tests_properties(nsm_test_watchdog PROPERTIES TIMEOUT 15)