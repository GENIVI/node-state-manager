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
	-I$(top_srcdir)/NodeStateAccessStub                             \
	$(DLT_CFLAGS)							\
	$(GIO_CFLAGS)							\
	$(GIO_UNIX_CFLAGS)						\
	$(GLIB_CFLAGS)							\
	$(GOBJECT_CFLAGS)						\
	$(PLATFORM_CFLAGS)

NodeStateManager_LDFLAGS =						\
	$(PLATFORM_LDFLAGS)

NodeStateManager_LDADD =						\
	-L$(top_srcdir)/NodeStateAccessStub  -lNodeStateAccess          \
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

EXTRA_DIST = $(systemdsystemunit_DATA) \
             $(dbussystemunit_DATA)    \
             $(dbuspolicy_DATA)
