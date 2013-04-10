#!/bin/sh -e
#
#
# Copyright (C) 2012 Continental Automotive Systems, Inc.
#
# Author: Jean-Pierre.Bogler@continental-corporation.com
#
# Script to create necessary files/folders from a fresh git check out.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Date       Author             Reason
# 24.10.2012 Jean-Pierre Bogler Initial creation
#
###############################################################################

mkdir -p m4
mkdir -p NodeStateAccess/doc
mkdir -p NodeStateAccess/generated

autoreconf --verbose --install --force
./configure $@
