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

noinst_LTLIBRARIES = libNodeStateAccess.la

libNodeStateAccess_la_CFLAGS =						\
	-I$(top_srcdir)/NodeStateManager			 	\
	$(GIO_CFLAGS)							\
	$(GIO_UNIX_CFLAGS)						\
	$(GLIB_CFLAGS)							\
	$(GOBJECT_CFLAGS)						\
	$(PLATFORM_CFLAGS)

libNodeStateAccess_la_SOURCES = NodeStateAccess.c NodeStateAccess.h
