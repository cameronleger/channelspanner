cmake_minimum_required (VERSION 3.0)
project (ChannelSpanner)

string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
add_definitions("-DSOURCE_PATH_SIZE=${SOURCE_PATH_SIZE}")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -Wall -Og")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g0 -Wall -O3")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

file(REMOVE ${PROJECT_SOURCE_DIR}/bin/ChannelSpanner.so)

include_directories(dep/glew/include)
include_directories(dep/lglw)
include_directories(dep/kissfft)
include_directories(dep/vstsdk)
include_directories(src)

add_library(GLEW STATIC dep/glew/src/glew.c)
set_property(TARGET GLEW PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(LGLW STATIC dep/lglw/lglw_linux.c)
find_package(X11 REQUIRED)
find_package(OpenGL REQUIRED)
target_link_libraries(LGLW OpenGL::GL ${X11_LIBRARIES})
target_compile_definitions(LGLW PRIVATE LGLW_CONTEXT_ALLOW_USE_AFTER_FREE=y)

add_library(KISSFFT STATIC dep/kissfft/kiss_fft.c)
set_target_properties(KISSFFT PROPERTIES
        CMAKE_C_FLAGS "{$CMAKE_C_FLAGS} -DUSE_SIMD"
        )

find_package(Freetype REQUIRED)
include_directories(${FREETYPE_INCLUDE_DIRS})
add_library(ChannelSpanner SHARED
        src/plugin.cpp
        src/spectrumgenerator.c
        src/spectrumdraw.c
        src/spanner_linux.c
        )
target_link_libraries(ChannelSpanner rt LGLW KISSFFT GLEW OpenGL::GL ${CMAKE_DL_LIBS} ${FREETYPE_LIBRARIES})
set_target_properties(ChannelSpanner PROPERTIES
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
        )
