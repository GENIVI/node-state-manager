#!/bin/bash
# fake session dbus as system dbus
export $(dbus-launch)
export DBUS_SYSTEM_BUS_ADDRESS=$DBUS_SESSION_BUS_ADDRESS

./NodeStateManager/NodeStateManager > /dev/null 2>&1 &
pid_nsm=$!

# wait until nsm has initialized
sleep 1
./NodeStateMachineTest/NodeStateTest
ret_val=$?
sleep 1

# Terminate NSM
kill -15 $pid_nsm

# ... and wait for it to exit
( sleep 60 ; kill -9 $pid_nsm ; ) &
killerPid=$!
wait $pid_nsm
kill $killerPid
kill $DBUS_SESSION_BUS_PID

exit $ret_val
