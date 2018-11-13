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

include_directories(dep/lglw)
include_directories(dep/vst2)
include_directories(src)

add_library(LGLW STATIC dep/lglw/lglw_linux.c)
find_package(X11 REQUIRED)
find_package(OpenGL REQUIRED)
target_link_libraries(LGLW OpenGL::GL ${X11_LIBRARIES})
target_compile_definitions(LGLW PRIVATE LGLW_RESET_CTX_ON_RESIZE=y)
set_property(TARGET LGLW PROPERTY POSITION_INDEPENDENT_CODE ON)

find_package(Freetype REQUIRED)
find_package(GLEW REQUIRED)

find_path(JANSSON_INCLUDE_DIRS jansson.h
        /usr/include /usr/local/include
        )
find_library(JANSSON_LIBRARIES NAMES jansson
        PATHS /usr/lib /usr/local/lib
        )

find_path(FFTW_INCLUDE_DIRS fftw3.h
        /usr/include /usr/local/include
        )

include_directories(${GLEW_INCLUDE_DIRS})
include_directories(${FREETYPE_INCLUDE_DIRS})
include_directories(${FFTW_INCLUDE_DIRS})
include_directories(${JANSSON_INCLUDE_DIRS})
add_library(ChannelSpanner SHARED
        src/vst2.cpp
        src/process.c
        src/draw.c
        src/spanner_linux.c
        )
target_link_libraries(ChannelSpanner rt LGLW OpenGL::GL fftw3f ${GLEW_LIBRARIES} ${FREETYPE_LIBRARIES} ${JANSSON_LIBRARIES} ${CMAKE_DL_LIBS})
set_target_properties(ChannelSpanner PROPERTIES
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
        )
target_compile_definitions(ChannelSpanner PRIVATE
        MAX_CHANNELS=2
        MAX_FFT=8192
        MAX_INSTANCES=16
        )