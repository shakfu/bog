cmake_minimum_required(VERSION 3.16)
project(bog LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(BOG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/bog)

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

install(TARGETS bog bog_cli)
