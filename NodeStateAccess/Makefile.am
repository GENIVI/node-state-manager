################################################################################
#
# Copyright (C) 2012 Continental Automotive Systems, Inc.
#
# Author: Jean-Pierre.Bogler@continental-corporation.com
#
# Makefile template for the NodeStateAccess library
#
# Process this file with automake to produce a Makefile.in.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
###############################################################################

lib_LTLIBRARIES = libNodeStateAccess.la

libNodeStateAccess_la_SOURCES = NodeStateAccess.c                      \
                                generated/NodeStateConsumer.c          \
                                generated/NodeStateLifecycleControl.c  \
                                generated/NodeStateLifecycleConsumer.c

libNodeStateAccess_la_CFLAGS  = -I$(top_srcdir)/NodeStateManager \
                                -I generated                     \
                                $(GIO_CFLAGS)                    \
                                $(GIO_UNIX_CFLAGS)               \
                                $(GLIB_CFLAGS)                   \
                                $(GOBJECT_CFLAGS)

libNodeStateAccess_la_LIBDADD = $(GIO_LIBS)                  \
                                $(GIO_UNIX_LIBS)             \
                                $(GLIB_LIBS)                 \
                                $(GOBJECT_LIBS)

libNodeStateAccess_la_LDFLAGS = -avoid-version

# List generated headers here to assert that they are available when built is started
BUILT_SOURCES =  generated/NodeStateConsumer.h          \
                 generated/NodeStateLifecycleControl.h  \
                 generated/NodeStateLifecycleConsumer.h

# Delete all generated files on 'make clean'
CLEANFILES    =  generated/NodeStateConsumer.c          \
                 generated/NodeStateConsumer.h          \
                 generated/NodeStateLifecycleControl.c  \
                 generated/NodeStateLifecycleControl.h  \
                 generated/NodeStateLifecycleConsumer.c \
                 generated/NodeStateLifecycleConsumer.h

# Targets to create generated sources during the build
generated/NodeStateConsumer.c generated/NodeStateConsumer.h: model/org.genivi.NodeStateManager.Consumer.xml
	gdbus-codegen --interface-prefix=org.genivi.NodeStateManager. --c-namespace=NodeState --generate-c-code=generated/NodeStateConsumer --generate-docbook=doc/NodeStateConsumer model/org.genivi.NodeStateManager.Consumer.xml

generated/NodeStateLifecycleControl.c generated/NodeStateLifecycleControl.h: model/org.genivi.NodeStateManager.LifecycleControl.xml
	gdbus-codegen --interface-prefix=org.genivi.NodeStateManager. --c-namespace=NodeState --generate-c-code=generated/NodeStateLifecycleControl --generate-docbook=doc/LifecycleControl  model/org.genivi.NodeStateManager.LifecycleControl.xml

generated/NodeStateLifecycleConsumer.c generated/NodeStateLifecycleConsumer.h: model/org.genivi.NodeStateManager.LifecycleConsumer.xml
	gdbus-codegen --interface-prefix=org.genivi.NodeStateManager. --c-namespace=NodeState --generate-c-code=generated/NodeStateLifecycleConsumer --generate-docbook=doc/LifecycleConsumer model/org.genivi.NodeStateManager.LifecycleConsumer.xml


