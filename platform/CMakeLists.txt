#
# ---------------------------------------------------------
# Win32 compatibility layer (non-Windows targets)
# ---------------------------------------------------------
#
# The engine is written against the Win32 window, message, cursor and GDI surface: the
# window procedure, the keyboard queue, the scroll handler and every dialog driver switch
# on WM_ values. Rather than teaching those call sites a second event model, this library
# keeps the surface and supplies it from SDL, so both builds dispatch the same messages.
#
# What it implements is the window, the event loop, the cursor and the keyboard. What it
# refuses is the dialog and GDI surface, which docs/UI_DESIGN.md step 13 replaces rather
# than ports: those entry points report failure so a caller takes its own no-dialog path.
add_library(win32compat STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/win32compat/src/dialog.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/win32compat/src/entry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/win32compat/src/gdi.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/win32compat/src/input.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/win32compat/src/kernel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/win32compat/src/message.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/win32compat/src/pointer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/win32compat/src/strings.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/win32compat/src/window.cpp"
)

target_compile_features(win32compat PRIVATE cxx_std_20)

# Consumers include <windows.h> and the rest of the surface by their Windows spellings.
target_include_directories(win32compat PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/win32compat/include")

target_link_libraries(win32compat PUBLIC SDL3::SDL3-static)

if(APPLE)
    target_link_libraries(win32compat PUBLIC "-framework QuartzCore")
endif()

# iOS differs from the desktop hosts in its entry point and in what an application bundle may
# hold. The guard is defined here, and everything that links the layer inherits it.
if(IOS)
    target_compile_definitions(win32compat PUBLIC OPENTS_IOS)
endif()
