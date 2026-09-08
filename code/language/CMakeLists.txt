file(GLOB_RECURSE LANG_SRC CONFIGURE_DEPENDS
    "*.cpp" "*.c" "*.cc" "*.rc"
    "*.h" "*.hpp" "*.hm"
)

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

add_custom_command(TARGET Language POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${TS_RUN_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:Language>"
        "${TS_RUN_DIR}/$<TARGET_FILE_NAME:Language>"
)

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
        COMMAND ${CMAKE_COMMAND} -E make_directory "${TS_RUN_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENTS_STRING_TABLE}"
            "${TS_RUN_DIR}/Language.dat"
        VERBATIM
    )

    add_dependencies(OpenTS OpenTSStringTable)
endif()
