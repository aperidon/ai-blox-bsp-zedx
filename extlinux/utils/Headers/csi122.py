# SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: MIT
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
from Jetson import header_def


HDR = header_def.HeaderDef( \
            #name
            'Jetson 122pin CSI Connector',
            #prefix
            'csi',
            #pin_count
            122,
            #static_pins
            {
                #idx    :       label
                  1     :       'GND',
                  2     :       'GND',
                  7     :       'GND',
                  8     :       'GND',
                 13     :       'GND',
                 14     :       'GND',
                 19     :       'GND',
                 20     :       'GND',
                 25     :       'GND',
                 26     :       'GND',
                 31     :       'GND',
                 32     :       'GND',
                 37     :       'GND',
                 38     :       'GND',
                 43     :       'GND',
                 44     :       'GND',
                 50     :       'GND',
                 55     :       'GND',
                 56     :       'GND',
                 61     :       'GND',
                 62     :       'GND',
                 67     :       'GND',
                 68     :       'GND',
                 73     :       'GND',
                 74     :       'GND',
                 79     :       'GND',
                 80     :       'GND',
                 82     :       '2.8V',
                 99     :       'GND',
                100     :       'GND',
                102     :       '1.8V',
                108     :       '3.3V',
                110     :       '3.3V',
                115     :       'GND',
                118     :       '3.3V',
                120     :       '3.3V',
                121     :       'GND',
                122     :       'GND',
            }
        )
