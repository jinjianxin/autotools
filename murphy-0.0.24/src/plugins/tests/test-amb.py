#!/usr/bin/env python

# Copyright (c) 2012, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#  * Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of Intel Corporation nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# This script is an example on how to use the Murphy D-Bus resource API.


from __future__ import print_function

import dbus
import dbus.service
import gobject
import glib
import sys
import os
from dbus.mainloop.glib import DBusGMainLoop

# D-Bus initialization

"""
dbus-send --system --type=signal /org/automotive/test \
        org.automotive.test.Test variant:uint32:5
"""

class FakeAMB(dbus.service.Object):

    def __init__(self, conn, path):
        dbus.service.Object.__init__(self, conn, path)

    @dbus.service.signal('org.automotive.test')
    def Test(self, data):
        pass


DBusGMainLoop(set_as_default=True)
mainloop = gobject.MainLoop()

bus = dbus.SystemBus()

if not bus:
    print("ERROR: failed to get system bus")
    exit(1)

famb = FakeAMB(bus, '/org/automotive/test')
famb.Test(dbus.String("test", variant_level=1))

famb1 = FakeAMB(bus, '/org/automotive/test1')
famb1.Test(dbus.Struct((dbus.String("test"), dbus.UInt32(12)), variant_level=1))

famb2 = FakeAMB(bus, '/org/automotive/test2')
famb2.Test(dbus.Array([dbus.String("foo"), dbus.String("bar")], variant_level=1))

famb3 = FakeAMB(bus, '/org/automotive/test3')
famb3.Test(dbus.Dictionary({ "foo" : dbus.UInt32(4), "bar" : dbus.UInt32(6) }, variant_level=1))

struct1 = (dbus.UInt32(30), dbus.UInt32(15))
struct2 = (dbus.UInt32(63), dbus.UInt32(7))
famb4 = FakeAMB(bus, '/org/automotive/test4')
famb4.Test(dbus.Dictionary({ "first" : struct1, "second" : struct2 }, variant_level=1))
