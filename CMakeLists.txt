cmake_minimum_required(VERSION 3.21)
project(AIAutoPlay VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT DEFINED GEODE_MOD_ID)
    set(GEODE_MOD_ID "antigravity.ai_autoplay")
endif()

find_package(Geode REQUIRED)

add_geode_mod(
    AIAutoPlay
    SOURCES
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
