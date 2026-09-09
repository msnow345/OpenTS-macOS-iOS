# The harness drives the file a saved game is kept in: savefile.cpp's writer and reader over
# the field table, the compressed content, and every refusal the reader makes. It links the
# same LZO the engine does.
add_executable(SaveTest
    "${CMAKE_CURRENT_SOURCE_DIR}/savetest.cpp"
    "${CMAKE_SOURCE_DIR}/code/savefile.cpp"
)

target_compile_features(SaveTest PRIVATE cxx_std_20)

target_include_directories(SaveTest PRIVATE
    "${CMAKE_SOURCE_DIR}/code"
)

target_compile_definitions(SaveTest PRIVATE WIN32 _WINDOWS _MBCS)

target_compile_options(SaveTest PRIVATE
    $<$<CONFIG:Debug>:/MTd /EHsc /Zc:__cplusplus>
    $<$<CONFIG:Release>:/MT /EHsc /Zc:__cplusplus>
)

target_link_libraries(SaveTest PRIVATE kernel32 lzo)

set_target_properties(SaveTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# The harness writes the files it reads back, so it is given a directory of its own.
add_test(NAME save COMMAND SaveTest "${CMAKE_CURRENT_BINARY_DIR}")
