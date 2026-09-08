#
# ---------------------------------------------------------
# bgfx (rendering backend)
# ---------------------------------------------------------
#
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bgfx.cmake/CMakeLists.txt")
    message(FATAL_ERROR
        "thirdparty/bgfx.cmake is empty. Fetch the submodules with:\n"
        "  git submodule update --init --recursive")
endif()

# bgfx builds shader and texture tools by default; the engine ships neither.
set(BGFX_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)
set(BGFX_CONFIG_RENDERER_WEBGPU OFF CACHE BOOL "" FORCE)

# The engine links the static CRT, so its dependencies must agree.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_subdirectory(bgfx.cmake)

# The encoder supports the disabled texture tools and is not linked into OpenTS. The decoder
# reads the PNG and TGA files the UI documents reference, so it is built with everything else.
set_target_properties(bimg_encode PROPERTIES EXCLUDE_FROM_ALL TRUE)

if(OPENTS_EXPERIMENTAL_CLANG_CL AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    # bx treats Clang with the MSVC CRT like a non-x86 compiler and erases
    # __stdcall. Restore it before Windows declarations are parsed.
    foreach(target bx bimg bgfx)
        target_compile_options(${target} PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:/FI${PROJECT_SOURCE_DIR}/thirdparty/bx-clang-compat.h>"
        )
    endforeach()

    # The ASTC encoder selects a 64-bit popcount intrinsic when SSE4.2 is enabled,
    # but that intrinsic is unavailable for the Win32 target. Its SSE2 path is portable.
    target_compile_definitions(bimg PRIVATE ASTCENC_POPCNT=0 ASTCENC_SSE=20)
endif()

#
# ---------------------------------------------------------
# miniaudio (audio device, resampling, decoders)
# ---------------------------------------------------------
#
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/miniaudio/miniaudio.h")
    message(FATAL_ERROR
        "thirdparty/miniaudio is empty. Fetch the submodules with:\n"
        "  git submodule update --init --recursive")
endif()

# The library is built here rather than through its own CMakeLists.txt, which applies
# the MA_NO_* options privately; consumers of miniaudio.h must see the same set or the
# structures they share with the implementation change layout.
add_library(miniaudio STATIC "${CMAKE_CURRENT_SOURCE_DIR}/miniaudio-impl.c")
target_include_directories(miniaudio PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/miniaudio")

# Only the device layer, the resampler, and the decoders are used. The engine, node
# graph, and resource manager are left out along with generation and encoding.
target_compile_definitions(miniaudio PUBLIC
    MA_NO_ENGINE
    MA_NO_NODE_GRAPH
    MA_NO_RESOURCE_MANAGER
    MA_NO_GENERATION
    MA_NO_ENCODING
)

if(WIN32)
    # Backends are loaded at run time, so no import library is needed for them.
    target_compile_definitions(miniaudio PUBLIC
        MA_ENABLE_ONLY_SPECIFIC_BACKENDS
        MA_ENABLE_WASAPI
        MA_ENABLE_DSOUND
        MA_ENABLE_WINMM
        MA_ENABLE_NULL
    )
endif()

if(MSVC)
    # Match the engine's floating-point model so the resampler behaves the same in both.
    target_compile_options(miniaudio PRIVATE /arch:SSE2 /fp:precise)
endif()

#
# ---------------------------------------------------------
# LZO (LZO1X compression for saves, maps, and network blocks)
# ---------------------------------------------------------
#
# A vendored copy rather than a submodule. LZO is published only as a source tarball from
# oberhumer.com; the git copies that exist are unofficial mirrors, so there is no upstream
# repository a submodule could pin. Only the LZO1X-1 sources the engine calls are kept,
# which is every file the three it links can reach.
#
# The library is built here rather than through its own CMakeLists.txt, which asks for
# compatibility with CMake 3.0 and is refused outright by CMake 4, and which also builds
# the test drivers and install rules that the engine does not ship.
file(GLOB LZO_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/lzo/src/*.c")
add_library(lzo STATIC ${LZO_SOURCES})

# LZO reads a generated config.h only when LZO_HAVE_CONFIG_H is defined, and probes the
# target with its own preprocessor tests otherwise. Leaving it undefined keeps the
# vendored tree free of a configure step.
target_include_directories(lzo
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/lzo/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lzo/src"
)

#
# ---------------------------------------------------------
# SDL3 (window, event loop, cursor, keyboard) -- non-Windows only
# ---------------------------------------------------------
#
# Windows supplies the window, the message loop and the cursor itself, so the supported
# build links none of this. Every other target needs one library that also reaches iOS,
# which rules out a desktop-only toolkit and rules out writing AppKit and UIKit twice.
# Audio stays on miniaudio; only the video, event and keyboard subsystems are built.
if(NOT WIN32)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SDL/CMakeLists.txt")
        message(FATAL_ERROR
            "thirdparty/SDL is empty. Fetch the submodules with:\n"
            "  git submodule update --init --recursive")
    endif()

    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
    set(SDL_RENDER OFF CACHE BOOL "" FORCE)
    set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
    set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
    set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
    set(SDL_POWER OFF CACHE BOOL "" FORCE)
    set(SDL_DIALOG OFF CACHE BOOL "" FORCE)

    add_subdirectory(SDL)
endif()

#
# ---------------------------------------------------------
# FreeType (glyph rasterization for the UI font engine)
# ---------------------------------------------------------
#
# Only RmlUi reaches this library, and only for the shipped sans-serif face. Its optional
# dependencies would each add a library the engine does not otherwise carry, so every one
# of them is refused here rather than left to whatever the build machine happens to have.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/freetype/CMakeLists.txt")
    message(FATAL_ERROR
        "thirdparty/freetype is empty. Fetch the submodules with:\n"
        "  git submodule update --init --recursive")
endif()

set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)

add_subdirectory(freetype)

# FreeType names its export Freetype::Freetype but declares no target under that name in the
# build tree, which is the name RmlUi looks for.
if(NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype)
endif()

#
# ---------------------------------------------------------
# RmlUi (documents, styling and layout for the UI shell)
# ---------------------------------------------------------
#
# Built as a static library with the FreeType font engine. The samples carry their own
# window and renderer backends, which is what the UI shell replaces, so none of them are
# built and no backend is selected.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RmlUi/CMakeLists.txt")
    message(FATAL_ERROR
        "thirdparty/RmlUi is empty. Fetch the submodules with:\n"
        "  git submodule update --init --recursive")
endif()

set(BUILD_SHARED_LIBS OFF)
set(RMLUI_FONT_ENGINE "freetype" CACHE STRING "" FORCE)
set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
set(RMLUI_LUA_BINDINGS OFF CACHE BOOL "" FORCE)
set(RMLUI_SVG_PLUGIN OFF CACHE BOOL "" FORCE)
set(RMLUI_LOTTIE_PLUGIN OFF CACHE BOOL "" FORCE)
set(RMLUI_HARFBUZZ_SAMPLE OFF CACHE BOOL "" FORCE)
set(RMLUI_TRACY_PROFILING OFF CACHE BOOL "" FORCE)
set(RMLUI_INSTALL_TARGETS_DIR "" CACHE STRING "" FORCE)
set(RMLUI_IS_ROOT_PROJECT FALSE)

add_subdirectory(RmlUi)

#
# ---------------------------------------------------------
# Dear ImGui (developer tooling on the same UI shell)
# ---------------------------------------------------------
#
# The core sources only. The bundled platform and renderer backends are left out; the UI
# shell feeds ImGui through the engine's own message hook and draws it on bgfx.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp")
    message(FATAL_ERROR
        "thirdparty/imgui is empty. Fetch the submodules with:\n"
        "  git submodule update --init --recursive")
endif()

add_library(imgui STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_tables.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp"
)
target_include_directories(imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
