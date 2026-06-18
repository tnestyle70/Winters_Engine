cmake_minimum_required(VERSION 3.25)

project(Winters VERSION 0.1.0 LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Winters CMake bootstrap currently supports Windows/MSVC only.")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE STRING "" FORCE)
endif()

option(WINTERS_ENABLE_ENGINE_POST_BUILD_DEPLOY "Run UpdateLib.bat after WintersEngine builds." ON)

set(WINTERS_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(WintersCompilerOptions)
include(WintersThirdParty)
include(WintersEngine)
add_subdirectory(EldenRingClient)
add_subdirectory(EldenRingEditor)
include(WintersWorkspaceMap)
