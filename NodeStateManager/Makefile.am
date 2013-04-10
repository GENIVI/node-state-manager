#######################################################################################################################
#
# Copyright (C) 2012 Continental Automotive Systems, Inc.
#
# Author: Jean-Pierre.Bogler@continental-corporation.com
#
# Makefile template for the NodeStateManager
#
# Process this file with automake to produce a Makefile.in.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#######################################################################################################################

bin_PROGRAMS = NodeStateManager

NodeStateManager_SOURCES = NodeStateManager.c

NodeStateManager_CFLAGS =						\
	-I$(top_srcdir)/NodeStateMachineStub                            \
	-I$(top_srcdir)/NodeStateAccess                                 \
	$(DLT_CFLAGS)							\
	$(GIO_CFLAGS)							\
	$(GIO_UNIX_CFLAGS)						\
	$(GLIB_CFLAGS)							\
	$(GOBJECT_CFLAGS)						\
	$(PLATFORM_CFLAGS)

NodeStateManager_LDFLAGS =						\
	$(PLATFORM_LDFLAGS)                                             \
        -export-dynamic

NodeStateManager_LDADD =						\
	-L$(top_srcdir)/NodeStateAccess      -lNodeStateAccess          \
	-L$(top_srcdir)/NodeStateMachineStub -lNodeStateMachine         \
	$(DLT_LIBS)							\
	$(GIO_LIBS)							\
	$(GIO_UNIX_LIBS)						\
	$(GLIB_LIBS)							\
	$(GOBJECT_LIBS)

include_HEADERS = NodeStateManager.h NodeStateTypes.h

systemdsystemunit_DATA = config/nodestatemanager-daemon.service

dbussystemunit_DATA = config/org.genivi.NodeStateManager.LifeCycleControl.service
dbuspolicy_DATA = config/org.genivi.NodeStateManager.conf

# Write the package config file of the NHM to it destination
pkgconfigdir   = $(libdir)/pkgconfig
pkgconfig_DATA = config/node-state-manager.pc

# Export the dbus interface descrition of the NHM
dbusinterfaces_DATA = $(top_srcdir)/NodeStateAccess/model/org.genivi.NodeStateManager.Consumer.xml          \
                      $(top_srcdir)/NodeStateAccess/model/org.genivi.NodeStateManager.LifecycleConsumer.xml \
                      $(top_srcdir)/NodeStateAccess/model/org.genivi.NodeStateManager.LifecycleControl.xml  \
                      $(top_srcdir)/NodeStateTest/org.genivi.NodeStateMachineTest.xml

EXTRA_DIST = $(systemdsystemunit_DATA) \
             $(dbussystemunit_DATA)    \
             $(dbuspolicy_DATA)        \
             $(dbusinterfaces_DATA)
