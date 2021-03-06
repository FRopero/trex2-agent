#!/bin/bash
#####################################################################
# Software License Agreement (BSD License)
# 
#  Copyright (c) 2011, MBARI.
#  All rights reserved.
# 
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
# 
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above
#     copyright notice, this list of conditions and the following
#     disclaimer in the documentation and/or other materials provided
#     with the distribution.
#   * Neither the name of the TREX Project nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
# 
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
#  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
#  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
#  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
#  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
#  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
#  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
#  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#####################################################################

TREX_HOME=/opt/lsts/trex/trex-0.5.0-rc2-Linux
EUROPA_HOME=/usr/share/europa
TREX_LIBS=$TREX_HOME/lib:/usr/lib

TREX_BINS=$TREX_HOME/bin
TREX_SHARED=$TREX_HOME/share/trex

if [ -d "${EUROPA_HOME}" ]; then 
    export EUROPA_HOME
    TREX_LIBS=${TREX_LIBS}:$EUROPA_HOME/lib
fi

platform='unknown'
lib_path_var=LD_LIBRARY_PATH
unamestr=`uname`
if [ "$unamestr" = 'Darwin' ]; then
    if [ -n "${DYLD_FALLBACK_LIBRARY_PATH+x}" ]; then
	export DYLD_FALLBACK_LIBRARY_PATH=${DYLD_FALLBACK_LIBRARY_PATH}:${TREX_LIBS}
    else
	export DYLD_FALLBACK_LIBRARY_PATH=${TREX_LIBS}
    fi
    export DYLD_BIND_AT_LAUNCH=YES
else 
    if [ -n "${LD_LIBRARY_PATH+x}" ]; then
	export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${TREX_LIBS}
    else
	export LD_LIBRARY_PATH=${TREX_LIBS}
    fi
fi
   

if [ -n "${PATH+x}" ]; then
    export PATH=${PATH}:${TREX_BINS}
else 
    export PATH=${TREX_BINS}
fi

if [ -n "${TREX_PATH+x}" ]; then
    export TREX_PATH=${TREX_PATH}:${TREX_SHARED}/cfg:${TREX_SHARED}/plugins
else
    export TREX_PATH=${TREX_SHARED}/cfg:${TREX_SHARED}/plugins
fi

if [ -z "${TREX_LOG_DIR+x}" ]; then
    export TREX_LOG_DIR=${TREX_SHARED}/log
fi

py_libs=${TREX_SHARED}/python

if [ -n "${PYTHONPATH+x}" ]; then 
    export PYTHONPATH=${py_libs}:${PYTHONPATH}
else
    export PYTHONPATH=${py_libs}
fi


# check for darwin specifics 

