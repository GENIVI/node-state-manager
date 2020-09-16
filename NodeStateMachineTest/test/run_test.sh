#######################################################################################################################
#
# Copyright (C) 2020 Mentor Graphics (Deutschland) GmbH
#
# Author: Vignesh_Rajendran@mentor.com
#
# Script to run the NSM Tests
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#######################################################################################################################

#!/bin/bash

BUILD_DIR=$(readlink -f $(dirname $0)/..)

# Make sure test command is given as argument:
if [ -z $1 ]
then
    echo "No test command given."
    echo "It is recommended to run the tests using ctest."
    exit 1
fi

# Set exit handler:
function on_exit
{
    echo "Exiting test script"
    if [ ! -z $NSM_PID ]
    then
        echo "Killing NSM"
        kill -9 $NSM_PID
    fi
}
trap on_exit EXIT

# Kill running NSM instances:
pkill -f NodeStateManager

# Delete old temp files:
rm -rf /tmp/vsomeip*
rm -rf /tmp/nsm*

# Launch NSM:
echo "Starting NSM"
export LD_LIBRARY_PATH=$BUILD_DIR/NodeStateAccess:$BUILD_DIR/NodeStateMachineStub:$LD_LIBRARY_PATH
$BUILD_DIR/NodeStateManager/NodeStateManager > /tmp/nsm_out 2>&1 & NSM_PID=$!
sleep 2

# Check if NSM launched successfully:
ps -p $NSM_PID >/dev/null
if (( $? != 0 ))
then
    echo "ERROR: NSM didn't launch successfully. See /tmp/nsm_out"
    exit 1
fi

export WATCHDOG_USEC=1000000

GTEST_OUTPUT_PREFIX=xml:/tmp/
export GTEST_OUTPUT=${GTEST_OUTPUT_PREFIX}nsm_test_result_${1}.xml

# Launch test command given as argument:
echo "Running test \"$1\""
$1
exit $?
