#######################################################################################################################
#
# Copyright (C) 2012 Continental Automotive Systems, Inc.
#
# Author: Jean-Pierre.Bogler@continental-corporation.com
#
# Makefile template for the NodeStateTest executable and NodeStateMachineTest library.
#
# Process this file with automake to produce a Makefile.in.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Date             Author              Reason
# 24.01.2013       Jean-Pierre Bogler  CSP_WZ#1194: Initial creation.
#
#######################################################################################################################

bin_PROGRAMS = NodeStateTest

NodeStateTest_SOURCES = NodeStateTest.c

nodist_NodeStateTest_SOURCES = NodeStateMachineTestApi.c                                            \
                               $(top_srcdir)/NodeStateAccess/generated/NodeStateConsumer.c          \
                               $(top_srcdir)/NodeStateAccess/generated/NodeStateLifecycleConsumer.c \
                               $(top_srcdir)/NodeStateAccess/generated/NodeStateLifecycleControl.c

NodeStateTest_CFLAGS =	-I$(top_srcdir)/NodeStateManager          \
                        -I$(top_srcdir)/NodeStateAccess           \
                        -I$(top_srcdir)/NodeStateAccess/generated \
                        $(GIO_CFLAGS)		                  \
                        $(GIO_UNIX_CFLAGS)                        \
                        $(GLIB_CFLAGS)                            \
                        $(GOBJECT_CFLAGS)

NodeStateTest_LDADD =	$(GIO_LIBS)      \
			$(GIO_UNIX_LIBS) \
			$(GLIB_LIBS)     \
		 	$(GOBJECT_LIBS)

lib_LTLIBRARIES = libNodeStateMachineTest.la

libNodeStateMachineTest_la_CFLAGS = -I../NodeStateManager \
                                    $(GIO_CFLAGS)         \
                                    $(GIO_UNIX_CFLAGS)    \
                                    $(GLIB_CFLAGS)        \
                                    $(GOBJECT_CFLAGS)

libNodeStateMachineTest_la_SOURCES = NodeStateMachine.c

nodist_libNodeStateMachineTest_la_SOURCES = NodeStateMachineTestApi.c

libNodeStateMachineTest_la_LDFLAGS = -avoid-version

BUILT_SOURCES = NodeStateMachineTestApi.h

CLEANFILES    = NodeStateMachineTestApi.c \
                NodeStateMachineTestApi.h

NodeStateMachineTestApi.c NodeStateMachineTestApi.h: org.genivi.NodeStateMachineTest.xml
	gdbus-codegen --interface-prefix com.contiautomotive.NodeStateMachineTest. --c-namespace NodeState --generate-c-code NodeStateMachineTestApi --annotate "com.contiautomotive.NodeStateMachineTest.Test.SetNsmData()[Data]" org.gtk.GDBus.C.ForceGVariant true --annotate "com.contiautomotive.NodeStateMachineTest.Test.GetNsmData()[DataIn]" org.gtk.GDBus.C.ForceGVariant true  --annotate "com.contiautomotive.NodeStateMachineTest.Test.GetNsmData()[DataOut]" org.gtk.GDBus.C.ForceGVariant true $<
