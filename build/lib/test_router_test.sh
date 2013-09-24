#!/bin/sh
export GR_DONT_LOAD_PREFS=1
export srcdir=/home/tjt7a/Src/new/gr-router/lib
export PATH=/home/tjt7a/Src/new/gr-router/build/lib:$PATH
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$DYLD_LIBRARY_PATH
export DYLD_LIBRARY_PATH=$LD_LIBRARY_PATH:$DYLD_LIBRARY_PATH
export PYTHONPATH=$PYTHONPATH
test-router 
