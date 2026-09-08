# Builds the portable string table from the Windows resource script.
#
# The resource script is the only place the strings live, and only the RC compiler turns it
# into a module resource. A host without an RC compiler still needs the same strings, so this
# script reads the STRINGTABLE blocks out of the script, resolves each symbolic identifier
# through the header the script itself includes, and writes a flat data file the game reads
# at runtime. The Windows build is untouched and keeps using the compiled resource.
#
# Run with:
#   cmake -DRC_FILE=<language.rc> -DHEADER_FILE=<language.h> -DOUTPUT=<file> -P StringTable.cmake
#
# Encoding: the resource script carries "#pragma code_page(65001)", so its bytes are already
# UTF-8 and are copied through unchanged. The data file is UTF-8 for the same reason.

if(NOT DEFINED RC_FILE OR NOT DEFINED HEADER_FILE OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "StringTable.cmake needs RC_FILE, HEADER_FILE and OUTPUT")
endif()

# A semicolon separates list elements everywhere in this language, and at least one string
# contains one. It is carried as a marker through every step that treats text as a list and
# put back only when the record is written. No string contains "@", which is what makes the
# marker safe to pick.
set(SEMICOLON_MARK "@OPENTS_SEMI@")
set(BACKSLASH_MARK "@OPENTS_BSLASH@")

# ---------------------------------------------------------------------------------------------
# The identifiers. The resource script names its strings symbolically and the header gives each
# name its number.
# ---------------------------------------------------------------------------------------------

file(READ "${HEADER_FILE}" HEADER_TEXT)
string(REGEX MATCHALL "#define[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]+[0-9]+" HEADER_DEFINES "${HEADER_TEXT}")

foreach(define IN LISTS HEADER_DEFINES)
    string(REGEX MATCH "#define[ \t]+([A-Za-z_][A-Za-z0-9_]*)[ \t]+([0-9]+)" ignored "${define}")
    set("ID_${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}")
endforeach()

list(LENGTH HEADER_DEFINES DEFINE_COUNT)

# ---------------------------------------------------------------------------------------------
# The strings. Only the STRINGTABLE blocks are read; the dialog templates in the same script
# hold quoted text that is not a string resource.
# ---------------------------------------------------------------------------------------------

file(READ "${RC_FILE}" RC_TEXT)
string(REPLACE ";" "${SEMICOLON_MARK}" RC_TEXT "${RC_TEXT}")
string(REPLACE "\r" "" RC_TEXT "${RC_TEXT}")
string(REPLACE "\n" ";" RC_LINES "${RC_TEXT}")

set(IN_TABLE FALSE)
set(IN_BODY FALSE)
set(PENDING_NAME "")
set(RECORD_COUNT 0)
set(RECORDS "")
set(MISSING "")

foreach(line IN LISTS RC_LINES)
    if(NOT IN_TABLE)
        if(line MATCHES "^STRINGTABLE([ \t]|$)")
            set(IN_TABLE TRUE)
        endif()
        continue()
    endif()

    if(NOT IN_BODY)
        if(line MATCHES "^BEGIN[ \t]*$")
            set(IN_BODY TRUE)
        endif()
        continue()
    endif()

    if(line MATCHES "^END[ \t]*$")
        set(IN_TABLE FALSE)
        set(IN_BODY FALSE)
        set(PENDING_NAME "")
        continue()
    endif()

    set(name "")
    set(raw "")
    set(have_string FALSE)

    if(line MATCHES "^[ \t]*([A-Za-z_][A-Za-z0-9_]*)[ \t]+\"(.*)\"[ \t]*$")
        set(name "${CMAKE_MATCH_1}")
        set(raw "${CMAKE_MATCH_2}")
        set(have_string TRUE)
    elseif(line MATCHES "^[ \t]*([A-Za-z_][A-Za-z0-9_]*)[ \t]*$")
        set(PENDING_NAME "${CMAKE_MATCH_1}")
        continue()
    elseif(line MATCHES "^[ \t]*\"(.*)\"[ \t]*$")
        set(name "${PENDING_NAME}")
        set(raw "${CMAKE_MATCH_1}")
        set(have_string TRUE)
        set(PENDING_NAME "")
    elseif(line MATCHES "^[ \t]*$")
        continue()
    else()
        message(FATAL_ERROR "StringTable.cmake did not understand a STRINGTABLE line: ${line}")
    endif()

    if(NOT have_string)
        continue()
    endif()

    if(name STREQUAL "")
        message(FATAL_ERROR "StringTable.cmake found a string with no identifier: ${line}")
    endif()

    if(NOT DEFINED "ID_${name}")
        list(APPEND MISSING "${name}")
        continue()
    endif()

    # The escapes the resource compiler understands, in the order that keeps an escaped
    # backslash from being read twice.
    string(REPLACE "\\\\" "${BACKSLASH_MARK}" raw "${raw}")
    string(REPLACE "\\n" "\n" raw "${raw}")
    string(REPLACE "\\r" "\r" raw "${raw}")
    string(REPLACE "\\t" "\t" raw "${raw}")
    string(REPLACE "\\\"" "\"" raw "${raw}")
    string(REPLACE "\"\"" "\"" raw "${raw}")
    string(REPLACE "${BACKSLASH_MARK}" "\\" raw "${raw}")
    string(REPLACE "${SEMICOLON_MARK}" ";" raw "${raw}")

    string(LENGTH "${raw}" length)

    # Length-prefixed, so a string that contains a newline needs no escaping of its own.
    string(APPEND RECORDS "${ID_${name}} ${length}\n${raw}\n")
    math(EXPR RECORD_COUNT "${RECORD_COUNT} + 1")
endforeach()

if(IN_TABLE)
    message(FATAL_ERROR "StringTable.cmake reached the end of ${RC_FILE} inside a STRINGTABLE")
endif()

if(MISSING)
    list(REMOVE_DUPLICATES MISSING)
    string(REPLACE ";" ", " MISSING_TEXT "${MISSING}")
    message(FATAL_ERROR "StringTable.cmake found no identifier in ${HEADER_FILE} for: ${MISSING_TEXT}")
endif()

if(RECORD_COUNT EQUAL 0)
    message(FATAL_ERROR "StringTable.cmake found no strings in ${RC_FILE}")
endif()

get_filename_component(OUTPUT_DIR "${OUTPUT}" DIRECTORY)
if(OUTPUT_DIR)
    file(MAKE_DIRECTORY "${OUTPUT_DIR}")
endif()

file(WRITE "${OUTPUT}" "OPENTS-STRINGS 1\n${RECORD_COUNT}\n${RECORDS}")

message(STATUS "String table: ${RECORD_COUNT} strings from ${DEFINE_COUNT} identifiers -> ${OUTPUT}")
