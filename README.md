#ChannelSpanner
This VST2 Plugin aims to be a quick, light, and no-fuss multi-track Spectrum Analyzer. To that end, simply add multiple instances of this plugin, possibly tweak some parameters in your host, and enjoy! Use it for visualizing each bus, each lead, or even before and after an effect chain.

The GUI should be self-explanatory: it displays the frequency spectrum for the input data, and the crosshair under the mouse displays decibel levels, frequency in Hz, and note, octave, and detuning. By default, other loaded instances of this plugin will also display their frequency spectrums, although visually in the 'background.' The only feature that's not immediately obvious is that you can 'sweep' the current instances frequencies with a variable bandpass filter by left-clicking on the graph. Moving from left to right adjusts the cutoff frequency to follow your mouse, and moving from bottom to top adjusts the Q or slope of the filter, where the top of the window is very narrow and the bottom is pretty wide.

Parameters are tweaked in your VST Host, and not the plugin window! These values rarely change, so I didn't want to clutter the UI with them. All parameters are only for the instance that you set them on, and they apply when sharing the spectrum data, e.g. setting instance A to a red spectrum will display that spectrum as red also when viewing through instance B.

- FFT Size: controls the 'resolution' of the spectrum. Higher values require more processing power, but it should be negligible for your project.
- Group: you can set different instances to different 'groups' so that only those in Group 1 will be visible when you open the plugin window for Group 1, etc.
- Color: sets the color of the spectrum line to one from a rainbow.
- Speed: how fast the spectrum appears to move. A value of 0 implies that the spectrum is frozen, 1 would be always displaying the exact result in every frame, and in-between values 'smooth' the updates of the spectrum by blending new results with previous results according to this value.
- Window Size: multiplier for how large the GUI should be.

#Requirements
These have been determined by installing the distribution with the default options and identifying the missing components. Your machine's needs may vary, and you can run `ldd ChannelSpanner.so | grep "not found"` to check any missing libraries.
## Ubuntu
`sudo apt-get install libglew2.0 libopengl0`
## Debian
`sudo apt-get install libglew2.0`

#Motivations
I was frustrated by the lack of decent (read: simple, pretty, fast, free) Spectrum Analyzer plugins, especially on Linux. I set out to make a quick analyzer that worked for my needs, but figured I could add a feature not normally found in free plugins: inter-process communications.

Another frustration was that most of these plugins relied on sidechaining signals in order to view multiple tracks, so in your VST Host you have to select the other track you want to see and keep changing it if your options are limited or duplicate the plugin.

It was actually rather simple to get this up and running with those goals; more time was probably spend optimizing and tweaking the code than anything else!

#Contributing
Windows implementations of `spanner.h`, CMake updates for Windows libraries, or fixes and some features are welcome!

#Technical
Prior to building, it may help to understand some behind-the-scenes aspects of this plugin. First, there are a few constants used when building that affect the plugin:

- `MAX_CHANNELS`: First, this sets the number of input/output ports of the plugin, so all instances will always have `x` input/outputs. It's also a multiplier on the memory usage. Obviously, processing more information will affect performance. Note that with '1' channel, whether or not this is a mono signal or simply the left or right channel will depend on how the host recognizes this.
- `MAX_FFT`: In order for this plugin to work efficiently, this must be a power of two! This will define the maximum FFT Size parameter of the plugin, and is another multiplier on the memory usage. Unless you're using the higher sizes, this does not affect performance, only the memory usage.
- `MAX_INSTANCES`: This is not the maximum allowed instances of the plugin, but the maximum amount of instances that share their information. This is the last multiplier on the Shared Memory usage, but not the individual usage of each instance. It also affects the Group parameter, as each shared instance can have its own Group. This negligibly affects drawing performance.

To simplify the Shared Memory code and make it more robust, these settings are compile-time constants. If they weren't, it would require dynamic resizing and restructuring of the Shared Memory which would have to be synchronized across instances. Together, these values imply the memory usage of the instances and Shared Memory as the maximum required data is always allocated even if some of it is unused, which will save time and issues when changing settings. Even with larger-than-default values, the memory requirements are actually pretty small. For example, 64 instances with 2 channels and an FFT Size of 8192 only requires 2Mb of memory!

By a vast margin, running the FFT on the input data is the most costly operation of this plugin, followed distantly by mixing the new and old results together. Creating and using the Shared Memory is extremely fast, as well as drawing the results. Larger FFT sizes will require exponentially more time, although it's still a small amount. If any channels are 'empty' there is a small amount of overhead in looping and checking their results, but the costly FFT operation is not performed.

Another benefit of the compile-time constants is that some math operations can be made significantly faster, and the FFT operations can use FFTW's 'wisdom' to speed itself up. Due to this, the first time that an FFT is run for a certain FFT Size, there can be a minor delay while the system is optimizing for the settings and hardware capabilities.

The first loaded instance will create the Shared Memory, and the last unloaded instance will destroy it. Each instance will try to find an area to store its results in this memory. If it's claimed an area, it will keep pushing to that area; if it hasn't, it will find an unclaimed area to claim. If a claimed area has not been updated for a few seconds, that claim is lost. So, the maximum instances parameter is only a 'running' maximum where `x` instances can be processing at the same time and share their results.

Mixing different settings for different builds of this plugin and using them together is not supported. It will almost certainly crash constantly.

#Building
NOTE: This plugin and its build files are currently setup only for Linux. Minor Makefile changes and a Windows-specific implementation of the Shared Memory code is required before this will work on Windows.

See `dep/vstsdk` for instructions related to downloading and extracting the VST2SDK, if you'd rather use that than the included `vestige.h`. Namely, it would enable slightly better parameter interactions in your host, if it supports it.

FreeType, Fontconfig, Jansson, FFTW, and GLEW must be installed including their development headers.

You may want to tweak some of the definitions in `CMakeLists.txt` if you're not happy with the defaults.
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
mkdir cmake-build-debug
cd cmake-build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```
Copy the resulting library file from `bin` to wherever you store your VSTs.

#Credits
- FreeType
- Fontconfig
- GLEW
- FFTW
- LGLW
- Steinberg VST SDK