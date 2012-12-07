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

dnl ***************************
dnl *** Initialize autoconf ***
dnl ***************************
AC_INIT([node-state-manager], [1.2.0.0])
AC_COPYRIGHT([Copyright (c) 2012 Continental Automotive GmbH])
AC_PREREQ([2.50])

dnl ***************************
dnl *** Initialize automake ***
dnl ***************************
AM_INIT_AUTOMAKE()
AC_CONFIG_MACRO_DIR([m4])
AC_SUBST([ACLOCAL_AMFLAGS], ["$ACLOCAL_FLAGS -I m4"])

dnl ********************************
dnl *** Check for basic programs ***
dnl ********************************
AC_PROG_CC()
AM_PROG_CC_C_O()
AC_PROG_INSTALL()

dnl ***************************************
dnl *** Initialize lib tool             ***
dnl ***************************************
AC_DISABLE_STATIC()
AC_PROG_LIBTOOL()

dnl ***************************************
dnl *** Check for standard header files ***
dnl ***************************************
AC_CHECK_HEADERS([string.h])

dnl ************************************
dnl *** Check for standard functions ***
dnl ************************************
AC_CHECK_FUNCS([memcpy])

dnl ***********************************
dnl *** Check for required packages ***
dnl ***********************************
PKG_CHECK_MODULES([DLT], [automotive-dlt >= 2.2.0])
PKG_CHECK_MODULES([GIO], [gio-2.0 >= 2.30.0])
PKG_CHECK_MODULES([GIO_UNIX], [gio-unix-2.0 >= 2.30.0])
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.30.0])
PKG_CHECK_MODULES([GOBJECT], [gobject-2.0 >= 2.30.0])

dnl *************************************
dnl *** Define extra paths            ***
dnl *************************************
AC_ARG_WITH([systemdsystemunitdir],
        AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
        [],
        [with_systemdsystemunitdir=$(pkg-config --silence-errors --variable=systemdsystemunitdir systemd)])
AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])

AC_ARG_WITH([dbussystemunitdir],
        AS_HELP_STRING([--with-dbussystemunitdir=DIR], [Directory for D-Bus system service files]),
        [],
        [with_dbussystemunitdir=$(pkg-config --silence-errors --variable=system_bus_services_dir dbus-1)])
AC_SUBST([dbussystemunitdir], [$with_dbussystemunitdir])

AC_ARG_WITH([dbuspolicydir],
        AS_HELP_STRING([--with-dbuspolicydirdir=DIR], [Directory for D-Bus system policy files]),
        [],
        [with_dbuspolicydir=$(pkg-config --silence-errors --variable=sysconfdir dbus-1)/dbus-1/system.d])
AC_SUBST([dbuspolicydir], [$with_dbuspolicydir])

dnl ************************************
dnl *** Define configure output      ***
dnl ************************************
AC_OUTPUT(Makefile NodeStateManager/Makefile NodeStateAccessStub/Makefile NodeStateMachineStub/Makefile NodeStateManager/config/nodestatemanager-daemon.service NodeStateManager/config/org.genivi.NodeStateManager.LifeCycleControl.service)
