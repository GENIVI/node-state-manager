#######################################################################################################################
#
# Copyright (C) 2012 Continental Automotive Systems, Inc.
#
# Author: Jean-Pierre.Bogler@continental-corporation.com
#
# Makefile template for the NodeStateMachineStub
#
# Process this file with automake to produce a Makefile.in.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#######################################################################################################################

lib_LTLIBRARIES = libNodeStateMachineStub.la

libNodeStateMachineStub_la_CFLAGS =						\
	-I$(top_srcdir)/NodeStateManager				\
        $(PLATFORM_CFLAGS)

libNodeStateMachineStub_la_SOURCES = NodeStateMachine.c NodeStateMachine.h

libNodeStateMachineStub_la_LDFLAGS = -avoid-version
