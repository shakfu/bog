cmake_minimum_required(VERSION 3.16)
project(bog LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BOG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/bog)
set(RTMIDI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rtmidi-6.0.0)

set(CORE_SOURCES
    ${BOG_DIR}/builtins.c
    ${BOG_DIR}/scheduler.c
    ${BOG_DIR}/livecoding.c
    ${BOG_DIR}/bog.c
)

add_library(bog STATIC ${CORE_SOURCES})
target_include_directories(bog PUBLIC ${BOG_DIR})

if (UNIX)
    target_link_libraries(bog PUBLIC m)
endif()

add_executable(bog_cli src/main.c)
target_link_libraries(bog_cli PRIVATE bog)
set_target_properties(bog_cli PROPERTIES OUTPUT_NAME bog)

add_library(rtmidi STATIC
    ${RTMIDI_DIR}/RtMidi.cpp
)
target_include_directories(rtmidi PUBLIC ${RTMIDI_DIR})
target_compile_features(rtmidi PRIVATE cxx_std_17)

if (APPLE)
    target_compile_definitions(rtmidi PUBLIC __MACOSX_CORE__)
    find_library(COREAUDIO_LIBRARY CoreAudio REQUIRED)
    find_library(COREMIDI_LIBRARY CoreMIDI REQUIRED)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)
    target_link_libraries(rtmidi PUBLIC
        ${COREAUDIO_LIBRARY}
        ${COREMIDI_LIBRARY}
        ${COREFOUNDATION_LIBRARY}
    )
elseif (WIN32)
    target_compile_definitions(rtmidi PUBLIC __WINDOWS_MM__)
    target_link_libraries(rtmidi PUBLIC winmm)
else()
    find_package(ALSA REQUIRED)
    target_compile_definitions(rtmidi PUBLIC __LINUX_ALSA__)
    target_link_libraries(rtmidi PUBLIC ALSA::ALSA pthread)
endif()

add_executable(bog_midi src/main_midi.cpp)
target_link_libraries(bog_midi PRIVATE bog rtmidi)

install(TARGETS bog bog_cli bog_midi)

# Tests
enable_testing()

set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests)

add_executable(test_parser ${TEST_DIR}/test_parser.c)
target_link_libraries(test_parser PRIVATE bog)
target_include_directories(test_parser PRIVATE ${TEST_DIR})
add_test(NAME parser COMMAND test_parser)

add_executable(test_unify ${TEST_DIR}/test_unify.c)
target_link_libraries(test_unify PRIVATE bog)
target_include_directories(test_unify PRIVATE ${TEST_DIR})
add_test(NAME unify COMMAND test_unify)

add_executable(test_builtins ${TEST_DIR}/test_builtins.c)
target_link_libraries(test_builtins PRIVATE bog)
target_include_directories(test_builtins PRIVATE ${TEST_DIR})
add_test(NAME builtins COMMAND test_builtins)

add_executable(test_state_manager ${TEST_DIR}/test_state_manager.c)
target_link_libraries(test_state_manager PRIVATE bog)
target_include_directories(test_state_manager PRIVATE ${TEST_DIR})
add_test(NAME state_manager COMMAND test_state_manager)

add_executable(test_resolution ${TEST_DIR}/test_resolution.c)
target_link_libraries(test_resolution PRIVATE bog)
target_include_directories(test_resolution PRIVATE ${TEST_DIR})
add_test(NAME resolution COMMAND test_resolution)

add_executable(test_livecoding ${TEST_DIR}/test_livecoding.c)
target_link_libraries(test_livecoding PRIVATE bog)
target_include_directories(test_livecoding PRIVATE ${TEST_DIR})
add_test(NAME livecoding COMMAND test_livecoding)
