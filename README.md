#Building
See `dep/vstsdk` for instructions related to downloading and extracting the VST2SDK.

FreeType and Jansson must be installed including their development headers.

Ensure that the submodules under `dep` are loaded:
#####Configure GLEW
```
cd dep/glew
make extensions
make
```
###Build ChannelSpanner
ChannelSpanner is built using CMake. You can simply run
```
mkdir cmake-build-release
cd cmake-build-release
cmake ..
make
```
or

```
cmake-build-debug
cd cmake-build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```
Copy the resulting library file from `bin` to wherever you store your VSTs.

#Credits
- FreeType
- GLEW
- KISSFFT
- LGLW
- Steinberg VST SDK