cmake_minimum_required (VERSION 3.12)
project (ChannelSpanner LANGUAGES C CXX)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

if(APPLE)
    message(FATAL "macOS is not supported")
endif()

# configure these if desired
add_definitions(
        "-DMAX_CHANNELS=2"
        "-DMAX_FFT=16384"
        "-DMAX_INSTANCES=32"
        "-DVESTIGE"
)

# no absolute paths during logging
string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
add_definitions("-DSOURCE_PATH_SIZE=${SOURCE_PATH_SIZE}")

# appropriate flags for debug/release
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g3 -Wall -Og")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -Wall -Og")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -g0 -Wall -O3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g0 -Wall -O3")

# set some nicer output locations
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# build LGLW first

include_directories(dep/lglw)

find_package(OpenGL REQUIRED)

if(LINUX)
    find_package(X11 REQUIRED)
    add_library(LGLW STATIC dep/lglw/lglw_linux.c)
    target_link_libraries(LGLW OpenGL::GL ${X11_LIBRARIES})
endif()

if(WIN32)
    message(FATAL "TODO: LGLW Windows")
endif()

target_compile_definitions(LGLW PRIVATE LGLW_RESET_CTX_ON_RESIZE=y)
set_property(TARGET LGLW PROPERTY POSITION_INDEPENDENT_CODE ON)

# build ChannelSpanner's main code

find_package(Freetype REQUIRED)
find_package(GLEW REQUIRED)

if(LINUX)
    find_path(JANSSON_INCLUDE_DIRS jansson.h
            /usr/include /usr/local/include)
    find_library(JANSSON_LIBRARIES NAMES jansson
            PATHS /usr/lib /usr/local/lib)

    find_path(FFTW_INCLUDE_DIRS fftw3.h
            /usr/include /usr/local/include)

    find_path(FONTCONFIG_INCLUDE_DIRS fontconfig/fontconfig.h
            /usr/include /usr/local/include)
    find_library(FONTCONFIG_LIBRARIES NAMES fontconfig
            PATHS /usr/lib /usr/local/lib)
endif()

if(WIN32)
    message(FATAL "TODO: FreeType/FontConfig/GLEW/JANSSON/FFTW Windows")
endif()

include_directories(dep/vst2)
include_directories(src)

include_directories(${GLEW_INCLUDE_DIRS})
include_directories(${FREETYPE_INCLUDE_DIRS})
include_directories(${FONTCONFIG_INCLUDE_DIRS})
include_directories(${FFTW_INCLUDE_DIRS})
include_directories(${JANSSON_INCLUDE_DIRS})

if(LINUX)
    set(SPANNER src/spanner_linux.c)
endif()

if(WIN32)
    message(FATAL "TODO: SPANNER Windows")
    set(SPANNER src/spanner_windows.c)
endif()

add_library(ChannelSpanner STATIC
        src/process.c
        src/draw.c
        src/biquad.c
        ${SPANNER}
        )
target_link_libraries(ChannelSpanner
        rt bsd LGLW OpenGL::GL fftw3f GLEW::GLEW
        ${FREETYPE_LIBRARIES} ${FONTCONFIG_LIBRARIES} ${CMAKE_DL_LIBS}
        )
set_property(TARGET ChannelSpanner PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET ChannelSpanner PROPERTY C_STANDARD 11)
set_property(TARGET ChannelSpanner PROPERTY CXX_STANDARD 14)

# build the VST2 plugin

add_library(ChannelSpannerVST2 SHARED
        src/vst2.cpp
        )
target_link_libraries(ChannelSpannerVST2 ChannelSpanner ${JANSSON_LIBRARIES})
set_target_properties(ChannelSpannerVST2 PROPERTIES
        PREFIX ""
        OUTPUT_NAME "ChannelSpanner"
        LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
        )