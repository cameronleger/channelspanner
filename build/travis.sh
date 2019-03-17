#!/bin/bash
docker rm channelspanner
rm -rf build
rm -rf cmake-build

set -e

mkdir -p build
mkdir -p cmake-build

mkdir -p ChannelSpanner/$2/

cp ../CMakeLists.txt build/
cp -r ../src build/
cp -r ../dep build/
cp cmake-$1.sh build/build.sh
cp pluginval build/

docker build -t channelspanner:$1 -f Dockerfile.$2 .
docker run --name channelspanner \
    --mount type=bind,source="$(pwd)"/build,target=/build \
    --mount type=bind,source="$(pwd)"/cmake-build,target=/build/cmake-build \
    channelspanner:$1

cp build/bin/ChannelSpanner.so ChannelSpanner/$2/