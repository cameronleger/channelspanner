#!/bin/bash
cd cmake-build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
cd /build
./pluginval --validate-in-process --strictness-level 5 --skip-gui-tests --validate bin/ChannelSpanner.so