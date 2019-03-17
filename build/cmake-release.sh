#!/bin/bash
cd cmake-build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
cd /build
./pluginval --validate-in-process --strictness-level 10 --skip-gui-tests --validate bin/ChannelSpanner.so