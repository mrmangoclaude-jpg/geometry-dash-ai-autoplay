cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(AIAutoPlay VERSION 1.0.0)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DFMT_USE_CONSTEVAL=0")
add_definitions(-DFMT_USE_CONSTEVAL=0)

add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    src/macro/MacroManager.cpp
    src/solver/GamemodeController.cpp
    src/solver/SolverEngine.cpp
    src/hooks/GJBaseGameLayerHook.cpp
    src/hooks/PlayerObjectHook.cpp
    src/hooks/PlayLayerHook.cpp
    src/hooks/PauseLayerHook.cpp
    src/ui/AutoPlayPopup.cpp
)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

setup_geode_mod(${PROJECT_NAME})
