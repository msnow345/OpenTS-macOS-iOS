file(GLOB_RECURSE LANG_SRC CONFIGURE_DEPENDS
    "*.cpp" "*.c" "*.cc" "*.rc"
    "*.h" "*.hpp" "*.hm"
)

# An iOS bundle holds no loose libraries beside its executable, and off Windows this one
# carries no code anyway: only the resource compiler turns language.rc into a module resource,
# and every other host reads the same strings out of Language.dat. The compatibility layer
# answers the load with the handle that stands for the running program instead.
if(NOT IOS)
    add_library(Language SHARED ${LANG_SRC})
    target_compile_features(Language PRIVATE cxx_std_20)
    target_compile_definitions(Language PRIVATE NOMINMAX)

    # The version resource is stamped from the generated headers, which the resource compiler
    # reaches through the target's include paths.
    target_include_directories(Language PRIVATE "${OPENTS_GENERATED_DIR}")
    add_dependencies(Language OpenTSBuildStamp)

    # The game loads its strings from this library at runtime, so building the executable alone
    # would leave a stale Language.dll beside it.
    add_dependencies(OpenTS Language)

    if(MSVC)
        target_link_options(Language PRIVATE "/NOENTRY" "/NODEFAULTLIB")
    endif()

    # The game loads this library by name from its own directory, so it is built beside the
    # executable rather than copied there. A shared library is a runtime artifact only on
    # Windows; everywhere else it is placed by the library output path.
    set_target_properties(Language PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()

# A UI document names a string by its symbolic name, so the same script that reads the resource
# script writes the table that turns a name back into its identifier. Every platform needs it,
# because every platform builds the UI shell.
set(OPENTS_STRING_NAMES "${OPENTS_GENERATED_DIR}/stringnames.hh")

add_custom_command(
    OUTPUT "${OPENTS_STRING_NAMES}"
    COMMAND ${CMAKE_COMMAND}
        "-DRC_FILE=${CMAKE_CURRENT_SOURCE_DIR}/language.rc"
        "-DHEADER_FILE=${CMAKE_CURRENT_SOURCE_DIR}/language.h"
        "-DNAME_TABLE=${OPENTS_STRING_NAMES}"
        -P "${CMAKE_SOURCE_DIR}/cmake/StringTable.cmake"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/language.rc"
        "${CMAKE_CURRENT_SOURCE_DIR}/language.h"
        "${CMAKE_SOURCE_DIR}/cmake/StringTable.cmake"
    COMMENT "Generating the string name table from language.rc"
    VERBATIM
)

add_custom_target(OpenTSStringNames ALL DEPENDS "${OPENTS_STRING_NAMES}")
add_dependencies(OpenTS OpenTSStringNames)

# Only the resource compiler turns the string table in language.rc into a module resource, so a
# host without one reads the same strings out of a flat data file generated from the same
# script. The Windows build neither generates nor ships it and keeps using LoadString.
if(NOT WIN32)
    set(OPENTS_STRING_TABLE "${OPENTS_GENERATED_DIR}/Language.dat")

    add_custom_command(
        OUTPUT "${OPENTS_STRING_TABLE}"
        COMMAND ${CMAKE_COMMAND}
            "-DRC_FILE=${CMAKE_CURRENT_SOURCE_DIR}/language.rc"
            "-DHEADER_FILE=${CMAKE_CURRENT_SOURCE_DIR}/language.h"
            "-DOUTPUT=${OPENTS_STRING_TABLE}"
            -P "${CMAKE_SOURCE_DIR}/cmake/StringTable.cmake"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/language.rc"
            "${CMAKE_CURRENT_SOURCE_DIR}/language.h"
            "${CMAKE_SOURCE_DIR}/cmake/StringTable.cmake"
        COMMENT "Generating the portable string table from language.rc"
        VERBATIM
    )

    add_custom_target(OpenTSStringTable ALL
        DEPENDS "${OPENTS_STRING_TABLE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENTS_STRING_TABLE}"
            "${CMAKE_BINARY_DIR}/bin/Language.dat"
        VERBATIM
    )

    add_dependencies(OpenTS OpenTSStringTable)
endif()
