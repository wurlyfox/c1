cmake_minimum_required(VERSION 3.20)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(c1 LANGUAGES C CXX)

# Default to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Option to build dependencies
option(BUILD_DEPS "Build dependencies if not found" OFF)

# Option to toggle shared libraries
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Preprocessor definitions
add_compile_definitions(
    CFLAGS_GFX_SW_PERSP
    CFLAGS_DRAW_EXTENSIONS
    CFLAGS_GUI
    CFLAGS_GOOL_DEBUG
)

# Compiler flags
if(MSVC)
    add_compile_options(/W3 /utf-8 /wd4200 /Zc:zeroSizeArray-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN
                            _USE_MATH_DEFINES)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR
            "The game code requires 32-bit pointers; target Win32 (cmake -A Win32).")
    endif()
else()
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-fplan9-extensions> 
                        -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
                        -Wno-missing-braces -Wno-implicit-fallthrough)
    if(WIN32)
        # MinGW: a 64-bit toolchain must be retargeted to 32-bit; a 32-bit
        # (i686) toolchain needs nothing.
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            add_compile_options(-m32)
            add_link_options(-m32)
        endif()
    else()
        add_compile_options(-m32)
        add_link_options(-m32)

        # -m32 on a 64-bit Debian/Ubuntu host: resolve dependencies from the
        # i386 multiarch directories instead of the x86_64 ones.
        if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND EXISTS "/usr/lib/i386-linux-gnu")
            set(CMAKE_LIBRARY_ARCHITECTURE i386-linux-gnu)
            set(ENV{PKG_CONFIG_LIBDIR}
                "/usr/lib/i386-linux-gnu/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig")
        endif()
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    endif()
endif()

if(WIN32)
    # main() stays a console entry point; skip SDL's main/WinMain redirection
    add_compile_definitions(SDL_MAIN_HANDLED)

    if(NOT BUILD_DEPS OR BUNDLE_MESA)
        set(PRECOMPILED_DIR "${CMAKE_BINARY_DIR}/deps")
        file(MAKE_DIRECTORY "${PRECOMPILED_DIR}")
    endif()
endif()

include(FetchContent)

find_package(OpenGL REQUIRED)

if(WIN32)
    option(BUNDLE_MESA "Bundle Mesa software opengl32.dll for systems lacking modern OpenGL drivers" OFF)

    if(BUNDLE_MESA)
        # Fetch earlier Mesa; later releases require seperate libgallium
        set(MESA_VER "20.1.8")
        set(MESA_PKG_DIR "${PRECOMPILED_DIR}/mesa3d-${MESA_VER}-msvc")

        if (NOT EXISTS "${MESA_PKG_DIR}/x86/opengl32.dll")
            message(STATUS "Mesa ${MESA_VER} not found locally; fetching pre-compiled package...")
            file(DOWNLOAD
                "https://github.com/pal1000/mesa-dist-win/releases/download/${MESA_VER}/mesa3d-${MESA_VER}-release-msvc.7z"
                "${PRECOMPILED_DIR}/mesa-msvc.7z"
                SHOW_PROGRESS
            )
            file(ARCHIVE_EXTRACT INPUT "${PRECOMPILED_DIR}/mesa-msvc.7z" DESTINATION "${MESA_PKG_DIR}")
            file(REMOVE "${PRECOMPILED_DIR}/mesa-msvc.7z")
        endif()
        
        set(MESA_RUNTIME_DLLS
            "${MESA_PKG_DIR}/x86/opengl32.dll"
            "${MESA_PKG_DIR}/x86/libglapi.dll"
        )    
    endif()
endif()

# Prefer the 32-bit (i386 multiarch) SDL2 config when building with -m32
if(NOT MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8
   AND EXISTS "/usr/lib/i386-linux-gnu/cmake/SDL2")
    set(SDL2_DIR "/usr/lib/i386-linux-gnu/cmake/SDL2" CACHE PATH "" FORCE)
endif()
find_package(SDL2 QUIET)

# Checks if SDL2 exists locally; if not, fetches automatically
if(NOT SDL2_FOUND)
    if(NOT BUILD_DEPS)
        message(STATUS "SDL2 not found locally; fetching pre-compiled package...")

        if(WIN32)
            set(SDL2_VER "2.30.2")
            set(SDL2_PKG_DIR "${PRECOMPILED_DIR}/SDL2-${SDL2_VER}")
            if(NOT EXISTS "${SDL2_PKG_DIR}")
                file(DOWNLOAD
                    "https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VER}/SDL2-devel-${SDL2_VER}-VC.zip"
                    "${PRECOMPILED_DIR}/sdl2.zip"
                    SHOW_PROGRESS
                )
                file(ARCHIVE_EXTRACT INPUT "${PRECOMPILED_DIR}/sdl2.zip" DESTINATION "${PRECOMPILED_DIR}")
                file(REMOVE "${PRECOMPILED_DIR}/sdl2.zip")
            endif() 

            # Create an SDL2 alias directory so #include <SDL2/SDL.h> works
            if(NOT EXISTS "${SDL2_PKG_DIR}/SDL2")
                file(MAKE_DIRECTORY "${SDL2_PKG_DIR}/SDL2")
                file(COPY "${SDL2_PKG_DIR}/include/" DESTINATION "${SDL2_PKG_DIR}/SDL2")
            endif()

            if(NOT TARGET SDL2::SDL2)
                add_library(SDL2::SDL2 SHARED IMPORTED GLOBAL)
            endif()

            set_target_properties(SDL2::SDL2 PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${SDL2_PKG_DIR}/include;${SDL2_PKG_DIR}"
                IMPORTED_LOCATION "${SDL2_PKG_DIR}/lib/x86/SDL2.dll"
                IMPORTED_IMPLIB "${SDL2_PKG_DIR}/lib/x86/SDL2.lib"
            )
            set(SDL2_LIBRARIES SDL2::SDL2)
            set(SDL2_FOUND TRUE)
        elseif (UNIX AND NOT APPLE)
            find_program(APT_GET_PROG apt-get)
            find_program(DPKG_PROG dpkg)

            if (APT_GET_PROG)
                # Ensure i386 architecture is added
                if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND DPKG_PROG)
                    execute_process(
                        COMMAND sudo ${DPKG_PROG} --add-architecture i386
                        RESULT_VARIABLE DPKG_RES
                    )
                endif()

                # Update index and install 32-bit development package
                execute_process(
                    COMMAND sudo ${APT_GET_PROG} update
                    RESULT_VARIABLE APT_UPDATE_RES
                )
                execute_process(
                    COMMAND sudo ${APT_GET_PROG} install -y libsdl2-dev:i386
                    RESULT_VARIABLE APT_INSTALL_RES
                )

                if(APT_INSTALL_RES EQUAL 0)
                    # Re-run find_package now that the libraries are in place
                    if(EXISTS "/usr/lib/i386-linux-gnu/cmake/SDL2")
                        set(SDL2_DIR "/usr/lib/i386-linux-gnu/cmake/SDL2" CACHE PATH "" FORCE)
                    endif()
                    find_package(SDL2 REQUIRED)
                    set(SDL2_LIBRARIES SDL2::SDL2)
                else()
                    message(FATAL_ERROR "Failed to install libsdl2-dev:i386.")
                endif()
            else()
                message(FATAL_ERROR "apt-get not found. Install 32-bit development libraries manually "
                                    "or re-run CMake with -DBUILD_DEPS=ON.")
            endif()
        endif()
    else()
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
    endif()
else()
    if(TARGET SDL2::SDL2)
        set(SDL2_LIBRARIES SDL2::SDL2)
    elseif(TARGET SDL2::SDL2-static)
        set(SDL2_LIBRARIES SDL2::SDL2-static)
    endif()
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
    message(STATUS "Found installed FluidSynth CMake target.")
    set(FLUIDSYNTH_LIBRARIES FluidSynth::fluidsynth)
    set(FLUIDSYNTH_INCLUDE_DIRS "")
elseif(FLUIDSYNTH_FOUND)
    message(STATUS "Found installed FluidSynth via pkg-config.")
    add_library(FluidSynth::fluidsynth INTERFACE IMPORTED)
    target_include_directories(FluidSynth::fluidsynth INTERFACE ${FLUIDSYNTH_INCLUDE_DIRS})
    target_link_libraries(FluidSynth::fluidsynth INTERFACE ${FLUIDSYNTH_LIBRARIES})
    set(FLUIDSYNTH_LIBRARIES FluidSynth::fluidsynth)
elseif(NOT BUILD_DEPS)
    message(STATUS "FluidSynth not found locally; fetching pre-compiled package...")

    if(WIN32)
        # Target earlier FluidSynth as newer versions require SDL3
        set(FS_VER "2.3.5")
        set(FS_PKG_DIR "${PRECOMPILED_DIR}/fluidsynth-${FS_VER}-winXP-x86")
        if(NOT EXISTS "${FS_PKG_DIR}")
            file(DOWNLOAD
                "https://github.com/FluidSynth/fluidsynth/releases/download/v${FS_VER}/fluidsynth-${FS_VER}-winXP-x86.zip"
                "${PRECOMPILED_DIR}/fluidsynth.zip"
                SHOW_PROGRESS
            )
            file(ARCHIVE_EXTRACT INPUT "${PRECOMPILED_DIR}/fluidsynth.zip" DESTINATION "${PRECOMPILED_DIR}/fluidsynth-${FS_VER}-winXP-x86")
            file(REMOVE "${PRECOMPILED_DIR}/fluidsynth.zip")
        endif()

        add_library(FluidSynth::fluidsynth SHARED IMPORTED GLOBAL)
        set_target_properties(FluidSynth::fluidsynth PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${FS_PKG_DIR}/include"
            IMPORTED_LOCATION "${FS_PKG_DIR}/bin/libfluidsynth-3.dll"
            IMPORTED_IMPLIB "${FS_PKG_DIR}/lib/fluidsynth.lib"
        )

        file(GLOB FS_EXTRA_DLLS "${FS_PKG_DIR}/bin/*.dll")
        set(FS_DEP_TARGETS "")
        foreach(dll IN LISTS FS_EXTRA_DLLS)
            get_filename_component(dll_name "${dll}" NAME)
            set(tgt_name "FluidSynth_${dll_name}")
            if(NOT TARGET ${tgt_name})
                add_library(${tgt_name} SHARED IMPORTED GLOBAL)
                set_target_properties(${tgt_name} PROPERTIES
                    IMPORTED_LOCATION "${dll}"
                    IMPORTED_IMPLIB "${FS_PKG_DIR}/lib/fluidsynth.lib"
                )
                list(APPEND FS_DEP_TARGETS ${tgt_name})
            endif()
        endforeach()

        set_target_properties(FluidSynth::fluidsynth PROPERTIES
            INTERFACE_LINK_LIBRARIES "${FS_DEP_TARGETS}"
        )

        set(FLUIDSYNTH_LIBRARIES FluidSynth::fluidsynth)
        set(FLUIDSYNTH_INCLUDE_DIRS "${FS_PKG_DIR}/include")
    elseif(UNIX AND NOT APPLE)
        find_program(APT_GET_PROG apt-get)
        if(APT_GET_PROG)
            execute_process(
                COMMAND sudo ${APT_GET_PROG} install -y libfluidsynth-dev:i386
                RESULT_VARIABLE APT_FS_RES
            )
            if(APT_FS_RES EQUAL 0)
                find_package(PkgConfig REQUIRED)
                pkg_check_modules(FLUIDSYNTH REQUIRED fluidsynth)
                add_library(FluidSynth::fluidsynth INTERFACE IMPORTED)
                target_include_directories(FluidSynth::fluidsynth INTERFACE ${FLUIDSYNTH_INCLUDE_DIRS})
                target_link_libraries(FluidSynth::fluidsynth INTERFACE ${FLUIDSYNTH_LIBRARIES})
                set(FLUIDSYNTH_LIBRARIES FluidSynth::fluidsynth)
            else()
                message(FATAL_ERROR "Failed to install libfluidsynth-dev:i386.")
            endif()
        else()
            message(FATAL_ERROR "apt-get not found. Install 32-bit development libraries manually "
                                "or re-run CMake with -DBUILD_DEPS=ON.")
        endif()
    endif()
else()
    message(STATUS "FluidSynth not found locally; fetching via FetchContent...")

    # Fetch Fluidsynth and build as a static lib
    set(BUILD_UNIT_TESTS  OFF CACHE BOOL "" FORCE)
    set(BUILD_DOC         OFF CACHE BOOL "" FORCE)
    set(BUILD_BY_DEFAULT  OFF CACHE BOOL "" FORCE)
    set(osal             "cpp11" CACHE STRING "" FORCE)
    set(enable-floats     ON  CACHE BOOL "" FORCE)

    foreach(opt alsa pulseaudio pipewire jack sndfile libsndfile dbus network readline
                openmp midishare coreaudio wasapi dsound winmidi waveout oss sdl2
                libinstpatch ladspa systemd opensles oboe portaudio)
        set(enable-${opt} OFF CACHE BOOL "" FORCE)
    endforeach()

    FetchContent_Declare(
        fluidsynth
        GIT_REPOSITORY https://github.com/FluidSynth/fluidsynth.git
        GIT_TAG        v2.5.0
        GIT_SHALLOW    TRUE
        GIT_SUBMODULES ""
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

add_library(c1core STATIC
    ${C1_CORE_SOURCES}
    ${C1_EXT_SOURCES}
)

target_include_directories(c1core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(c1core PUBLIC cimgui)

add_executable(c1 ${C1_PC_SOURCES})
set_target_properties(c1 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    if(NOT BUILD_DEPS OR BUILD_SHARED_LIBS)
        add_custom_command(TARGET c1 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:c1>
                $<TARGET_FILE_DIR:c1>
            COMMAND_EXPAND_LISTS
            COMMENT "Deploying DLLs..."
        )
    endif()

    if(BUNDLE_MESA)
        add_custom_command(TARGET c1 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${MESA_RUNTIME_DLLS}
                $<TARGET_FILE_DIR:c1>
        )
    endif()

    if(NOT BUILD_DEPS OR BUNDLE_MESA)
        add_custom_command(TARGET c1 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${PRECOMPILED_DIR}"
            COMMENT "Removing pre-compiled staging directory..."
        )
    endif()
endif()
    
target_include_directories(c1 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    ${cimgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${FLUIDSYNTH_INCLUDE_DIRS}
)

target_link_libraries(c1 PRIVATE
    c1core
    ${SDL2_LIBRARIES}
    OpenGL::GL
    ${FLUIDSYNTH_LIBRARIES}
    cimgui
)

if(UNIX AND NOT APPLE)
    target_link_libraries(c1 PRIVATE m pthread dl)
endif()

