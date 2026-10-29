cmake_minimum_required(VERSION 3.20)
project(c1 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Option to toggle shared libraries
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Preprocessor definitions
add_compile_definitions(
    CFLAGS_HIRES_TEST
    CFLAGS_GFX_SW_PERSP
    CFLAGS_EXTENSIONS
    CFLAGS_GUI
    CFLAGS_GOOL_DEBUG
)

# Compiler flags
if(MSVC)
    add_compile_options(/W3 /utf-8)
else()
    add_compile_options(-m32 $<$<COMPILE_LANGUAGE:C>:-fplan9-extensions> 
                        -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
                        -Wno-missing-braces -Wno-implicit-fallthrough)
    add_link_options(-m32)

    # -m32 on a 64-bit Debian/Ubuntu host: resolve dependencies from the
    # i386 multiarch directories instead of the x86_64 ones.
    if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND EXISTS "/usr/lib/i386-linux-gnu")
        set(CMAKE_LIBRARY_ARCHITECTURE i386-linux-gnu)
        set(ENV{PKG_CONFIG_LIBDIR}
            "/usr/lib/i386-linux-gnu/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig")
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    endif()
endif()

include(FetchContent)

find_package(OpenGL REQUIRED)

# Prefer the 32-bit (i386 multiarch) SDL2 config when building with -m32
if(NOT MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8
   AND EXISTS "/usr/lib/i386-linux-gnu/cmake/SDL2")
    set(SDL2_DIR "/usr/lib/i386-linux-gnu/cmake/SDL2" CACHE PATH "" FORCE)
endif()
find_package(SDL2 QUIET)

# Checks if SDL2 exists locally; if not, fetches automatically
if(NOT SDL2_FOUND)
    message(STATUS "SDL2 not found locally; fetching via FetchContent...")
    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-2.30.x
        GIT_SHALLOW    TRUE
    )
    if (BUILD_SHARED_LIBS)
        set(SDL_SHARED ON CACHE BOOL "" FORCE)
        set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    else()
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
    endif()
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL2)
    if (BUILD_SHARED_LIBS)
        set(SDL2_LIBRARIES SDL2::SDL2)
    else()
        set(SDL2_LIBRARIES SDL2::SDL2-static)
    endif()
else()
    set(SDL2_LIBRARIES SDL2::SDL2)
endif()

# always build ImGui as a static library
set(IMGUI_STATIC ON CACHE BOOL "" FORCE)

set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)

# Fetch cimgui directly
FetchContent_Declare(
    cimgui
    URL      https://github.com/cimgui/cimgui/archive/refs/tags/1.90.9dock.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR do-not-add-subdirectory
)
FetchContent_MakeAvailable(cimgui)

# cimgui expects imgui at <cimgui>/imgui (normally a git submodule)
FetchContent_Declare(
    imgui
    URL      https://github.com/ocornut/imgui/archive/refs/tags/v1.90.9-docking.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_DIR ${cimgui_SOURCE_DIR}/imgui
)
FetchContent_MakeAvailable(imgui)

# cimgui's own CMake expects an imgui/ submodule and does not build the
# SDL2/OpenGL2 backends, so build everything here from the fetched sources.
add_library(cimgui STATIC
    ${cimgui_SOURCE_DIR}/cimgui.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
)
target_include_directories(cimgui PUBLIC
    ${cimgui_SOURCE_DIR}
    ${cimgui_SOURCE_DIR}/generator/output
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# Export the imgui backends with C linkage so cimgui_impl.h can declare them
target_compile_definitions(cimgui PRIVATE "IMGUI_IMPL_API=extern \"C\"")
target_compile_definitions(cimgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS=1)
target_link_libraries(cimgui PUBLIC ${SDL2_LIBRARIES} OpenGL::GL)

# Locate Fluidsynth system installation
find_package(Fluidsynth QUIET)

if(NOT FluidSynth_FOUND)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(FLUIDSYNTH QUIET fluidsynth)
    endif()
endif()

if(TARGET FluidSynth::fluidsynth)
    message(STATUS "Found installed FluidSynth CMake target")
    set(FLUIDSYNTH_LIBRARIES FluidSynth::fluidsynth)
    set(FLUIDSYNTH_INCLUDE_DIRS "")
elseif (FLUIDSYNTH_FOUND)
    message(STATUS "Found installed FluidSynth via pkg-config")
    add_library(FluidSynth::fluidsynth INTERFACE IMPORTED)
    target_include_directories(FluidSynth::fluidsynth INTERFACE ${FLUIDSYNTH_INCLUDE_DIRS})
    target_link_libraries(FluidSynth::fluidsynth INTERFACE ${FLUIDSYNTH_LIBRARIES})
    set(FLUIDSYNTH_LIBRARIES FluidSynth::fluidsynth)
else()
    message(STATUS "FluidSynth not found locally; fetching via FetchContent...")

    # Fetch Fluidsynth and build as a static lib
    foreach(opt alsa pulseaudio pipewire jack sndfile libsndfile dbus network readline
                openmp midishare coreaudio wasapi dsound winmidi waveout oss sdl2
                libinstpatch ladspa systemd opensles oboe portaudio)
        set(enable-${opt} OFF CACHE BOOL "" FORCE)
    endforeach()

    FetchContent_Declare(
        fluidsynth
        GIT_REPOSITORY https://github.com/FluidSynth/fluidsynth.git
        GIT_TAG        v2.3.6
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(fluidsynth)

    set(FLUIDSYNTH_LIBRARIES libfluidsynth)
    set(FLUIDSYNTH_INCLUDE_DIRS
        ${fluidsynth_SOURCE_DIR}/include
        ${fluidsynth_BINARY_DIR}/include
    )
endif()

# Core source files
file(GLOB C1_CORE_SOURCES
    "src/*.c"
)

# PC source files
file(GLOB_RECURSE C1_PC_SOURCES
    "src/pc/*.c"
)

# Extension source files
file(GLOB_RECURSE C1_EXT_SOURCES
    "src/ext/*.c"
    "src/util/*.c"
)

add_library(c1_core 
    ${C1_CORE_SOURCES}
    ${C1_EXT_SOURCES}
)

target_include_directories(c1_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(c1_core PUBLIC cimgui)

add_executable(c1 ${C1_PC_SOURCES})
set_target_properties(c1 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

target_include_directories(c1 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    ${cimgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${FLUIDSYNTH_INCLUDE_DIRS}
)

target_link_libraries(c1 PRIVATE
    c1_core
    ${SDL2_LIBRARIES}
    OpenGL::GL
    ${FLUIDSYNTH_LIBRARIES}
    cimgui
)

if(UNIX AND NOT APPLE)
    target_link_libraries(c1 PRIVATE m pthread dl)
endif()

