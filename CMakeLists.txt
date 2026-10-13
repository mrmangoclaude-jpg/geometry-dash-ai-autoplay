cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(AIAutoPlay VERSION 1.0.0)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DFMT_USE_CONSTEVAL=0")
add_definitions(-DFMT_USE_CONSTEVAL=0 -DNOMINMAX)

# Reachability DP Solver core (compiled once into static library, standard C++)
add_library(gdsolver_dp INTERFACE)
target_include_directories(gdsolver_dp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/dp/src)
target_compile_features(gdsolver_dp INTERFACE cxx_std_20)

add_library(gdsolver_dpcore STATIC dp/src/dpcore.cpp)
target_link_libraries(gdsolver_dpcore PUBLIC gdsolver_dp)
if (MSVC)
    target_compile_options(gdsolver_dpcore PRIVATE /W3 /permissive- /utf-8 /bigobj)
else()
    target_compile_options(gdsolver_dpcore PRIVATE -O2)
endif()

add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    src/macro/MacroManager.cpp
    src/solver/GamemodeController.cpp
    src/solver/SolverEngine.cpp
    src/solver/LevelExporter.cpp
    src/hooks/GJBaseGameLayerHook.cpp
    src/hooks/PlayerObjectHook.cpp
    src/hooks/PlayLayerHook.cpp
    src/hooks/PauseLayerHook.cpp
    src/ui/AutoPlayPopup.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/dp/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} gdsolver_dpcore)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

setup_geode_mod(${PROJECT_NAME})
