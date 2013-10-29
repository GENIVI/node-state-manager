#######################################################################################################################
#
# Copyright (C) 2012 Continental Automotive Systems, Inc.
#
# Author: Jean-Pierre.Bogler@continental-corporation.com
#
# Configure template for the NodeStateManager
#
# Process this file with autoconf to produce a configure script.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#######################################################################################################################

# Initialize autoconf
AC_INIT([node-state-manager],[1.3.5])
AC_COPYRIGHT([Copyright (c) 2012 Continental Automotive GmbH])
AC_PREREQ([2.50])

# Initialize automake
AM_INIT_AUTOMAKE([tar-ustar])
AC_CONFIG_MACRO_DIR([m4])
AC_SUBST([ACLOCAL_AMFLAGS], ["$ACLOCAL_FLAGS -I m4"])

# Initialize libtool
LT_INIT([disable-static])

# Check for basic programs
AC_PROG_CC
AM_PROG_CC_C_O

# Check for basic headers
AC_CHECK_HEADERS([string.h])

# Check for basic functions
AC_CHECK_FUNCS([memcpy])

# Check for required packages
PKG_CHECK_MODULES([DLT],      [automotive-dlt             >= 2.2.0 ])
PKG_CHECK_MODULES([GIO],      [gio-2.0                    >= 2.30.0])
PKG_CHECK_MODULES([GIO_UNIX], [gio-unix-2.0               >= 2.30.0])
PKG_CHECK_MODULES([GLIB],     [glib-2.0                   >= 2.30.0])
PKG_CHECK_MODULES([GOBJECT],  [gobject-2.0                >= 2.30.0])
PKG_CHECK_MODULES([DBUS],     [dbus-1                     >= 1.4.10])
PKG_CHECK_MODULES([SYSTEMD],  [libsystemd-daemon          >= 37    ])
PKG_CHECK_MODULES([PCL],      [persistence_client_library >= 0.6.0 ])

# Derive path for storing systemd service files (e. g. /lib/systemd/system)
AC_ARG_WITH([systemdsystemunitdir],
        AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
        [],
        [with_systemdsystemunitdir=$(pkg-config --silence-errors --variable=systemdsystemunitdir systemd)])

if test "x${with_systemdsystemunitdir}" = "x"; then
            with_systemdsystemunitdir="/lib/systemd/system"
fi

AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])

# Derive path for storing 'dbus' service files (e. g. /usr/share/dbus-1/system-services)
AC_ARG_WITH([dbussystemunitdir],
        AS_HELP_STRING([--with-dbussystemunitdir=DIR], [Directory for D-Bus system service files]),
        [],
        [with_dbussystemunitdir=$(pkg-config --silence-errors --variable=system_bus_services_dir dbus-1)])
AC_SUBST([dbussystemunitdir], [$with_dbussystemunitdir])

# Derive path for storing 'dbus' policy files (e. g. /etc)
AC_ARG_WITH([dbuspolicydir],
        AS_HELP_STRING([--with-dbuspolicydirdir=DIR], [Directory for D-Bus system policy files]),
        [],
        [with_dbuspolicydir=$(pkg-config --silence-errors --variable=sysconfdir dbus-1)/dbus-1/system.d])
AC_SUBST([dbuspolicydir], [$with_dbuspolicydir])

# Derive path for storing 'dbus' interface files (e. g. /usr/share/dbus-1/interfaces)
AC_ARG_WITH([dbusinterfacesdir],
        AS_HELP_STRING([--with-dbusinterfacesdir=DIR], [Directory for D-Bus interface files]),
        [],
        [with_dbusinterfacesdir=$(pkg-config --silence-errors --variable=interfaces_dir dbus-1)])
AC_SUBST([dbusinterfacesdir], [$with_dbusinterfacesdir])

# Define configure output
AC_CONFIG_FILES([Makefile 
                 NodeStateManager/Makefile 
                 NodeStateTest/Makefile 
                 NodeStateAccess/Makefile 
                 NodeStateMachineStub/Makefile 
                 NodeStateManager/config/nodestatemanager-daemon.service 
                 NodeStateManager/config/org.genivi.NodeStateManager.LifeCycleControl.service 
                 NodeStateManager/config/node-state-manager.pc])
AC_OUTPUT
