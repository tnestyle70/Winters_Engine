Session - CMake/Ninja Engine bootstrap를 기존 MSBuild와 병렬로 연결한다.

목표:
- 기존 `Winters.sln`, `.vcxproj`, `.vcxproj.filters`를 삭제하거나 수정하지 않는다.
- 루트 `CMakeLists.txt`를 새 빌드 진실의 원천 후보로 추가한다.
- Session 1에서는 `WintersEngine` target만 CMake/Ninja로 빌드 가능하게 만든다.
- CMake 생성물은 `out/` 아래에만 두고 git 추적에서 제외한다.
- Engine 산출물 경로는 기존 MSBuild와 같은 `Engine/Bin/Debug`, `Engine/Bin/Release`를 사용한다.
- Engine 빌드 후 기존 `UpdateLib.bat` 배포 흐름을 그대로 호출한다.

가정:
- Windows + Visual Studio 2022 v143 + MSVC 환경을 기준으로 한다.
- Ninja 빌드는 Developer PowerShell 또는 MSVC 환경이 잡힌 터미널에서 실행한다.
- Session 1은 DX11 Debug/Release를 우선 통과시키고, DX12는 preset만 열어두되 별도 검증 항목으로 둔다.
- Server/Client/GameSim target 분리는 Session 2 이후에 진행한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/.gitignore

기존 코드:

```gitignore
# Build results folders
ipch/
*.ilk
*.meta
*.pdb
```

아래에 추가:

```gitignore
# CMake generated build trees
out/
```

1-2. C:/Users/user/Desktop/Winters/CMakeLists.txt

새 파일:

```cmake
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

set(WINTERS_RHI_BACKEND "DX11" CACHE STRING "RHI backend to compile: DX11 or DX12")
set_property(CACHE WINTERS_RHI_BACKEND PROPERTY STRINGS DX11 DX12)

if(NOT WINTERS_RHI_BACKEND STREQUAL "DX11" AND NOT WINTERS_RHI_BACKEND STREQUAL "DX12")
    message(FATAL_ERROR "WINTERS_RHI_BACKEND must be DX11 or DX12.")
endif()

set(WINTERS_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(WintersCompilerOptions)
include(WintersThirdParty)
include(WintersEngine)
```

1-3. C:/Users/user/Desktop/Winters/CMakePresets.json

새 파일:

```json
{
  "version": 6,
  "cmakeMinimumRequired": {
    "major": 3,
    "minor": 25,
    "patch": 0
  },
  "configurePresets": [
    {
      "name": "msvc-ninja",
      "displayName": "MSVC Ninja Multi-Config",
      "generator": "Ninja Multi-Config",
      "binaryDir": "${sourceDir}/out/build/msvc-ninja",
      "cacheVariables": {
        "CMAKE_CONFIGURATION_TYPES": "Debug;Release",
        "WINTERS_RHI_BACKEND": "DX11",
        "WINTERS_ENABLE_ENGINE_POST_BUILD_DEPLOY": "ON"
      }
    },
    {
      "name": "msvc-ninja-dx12",
      "displayName": "MSVC Ninja Multi-Config DX12",
      "inherits": "msvc-ninja",
      "binaryDir": "${sourceDir}/out/build/msvc-ninja-dx12",
      "cacheVariables": {
        "WINTERS_RHI_BACKEND": "DX12"
      }
    },
    {
      "name": "vs2022",
      "displayName": "Visual Studio 2022",
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "binaryDir": "${sourceDir}/out/build/vs2022",
      "cacheVariables": {
        "CMAKE_CONFIGURATION_TYPES": "Debug;Release",
        "WINTERS_RHI_BACKEND": "DX11",
        "WINTERS_ENABLE_ENGINE_POST_BUILD_DEPLOY": "ON"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "engine-debug",
      "displayName": "Build WintersEngine Debug with Ninja",
      "configurePreset": "msvc-ninja",
      "configuration": "Debug",
      "targets": [
        "WintersEngine"
      ]
    },
    {
      "name": "engine-release",
      "displayName": "Build WintersEngine Release with Ninja",
      "configurePreset": "msvc-ninja",
      "configuration": "Release",
      "targets": [
        "WintersEngine"
      ]
    },
    {
      "name": "engine-debug-dx12",
      "displayName": "Build WintersEngine Debug DX12 with Ninja",
      "configurePreset": "msvc-ninja-dx12",
      "configuration": "Debug",
      "targets": [
        "WintersEngine"
      ]
    },
    {
      "name": "engine-vs-debug",
      "displayName": "Build WintersEngine Debug with Visual Studio generator",
      "configurePreset": "vs2022",
      "configuration": "Debug",
      "targets": [
        "WintersEngine"
      ]
    }
  ]
}
```

1-4. C:/Users/user/Desktop/Winters/cmake/WintersCompilerOptions.cmake

새 파일:

```cmake
function(WintersApplyMsvcCommonOptions TargetName)
    target_compile_features(${TargetName} PUBLIC cxx_std_20)

    if(MSVC)
        target_compile_options(${TargetName} PRIVATE
            /W3
            /sdl
            /permissive-
            /utf-8
            /FS
        )

        target_compile_options(${TargetName} PRIVATE
            $<$<CONFIG:Debug>:/Od>
            $<$<CONFIG:Release>:/O2>
            $<$<CONFIG:Release>:/Gy>
        )

        target_link_options(${TargetName} PRIVATE
            $<$<CONFIG:Debug>:/DEBUG>
            $<$<CONFIG:Release>:/OPT:REF>
            $<$<CONFIG:Release>:/OPT:ICF>
        )

        set_property(TARGET ${TargetName} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        )
    endif()
endfunction()

function(WintersSetOutputDirectories TargetName OutputRoot)
    set_target_properties(${TargetName} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${OutputRoot}/$<CONFIG>"
        LIBRARY_OUTPUT_DIRECTORY "${OutputRoot}/$<CONFIG>"
        ARCHIVE_OUTPUT_DIRECTORY "${OutputRoot}/$<CONFIG>"
    )
endfunction()
```

1-5. C:/Users/user/Desktop/Winters/cmake/WintersThirdParty.cmake

새 파일:

```cmake
set(WINTERS_THIRD_PARTY_DIR "${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib")

function(WintersRequireFile FilePath Description)
    if(NOT EXISTS "${FilePath}")
        message(FATAL_ERROR "Missing ${Description}: ${FilePath}")
    endif()
endfunction()

function(WintersAddImportedConfigLib TargetName DebugLib ReleaseLib)
    WintersRequireFile("${DebugLib}" "${TargetName} Debug library")
    WintersRequireFile("${ReleaseLib}" "${TargetName} Release library")

    add_library(${TargetName} UNKNOWN IMPORTED GLOBAL)
    set_target_properties(${TargetName} PROPERTIES
        IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
        IMPORTED_LOCATION_DEBUG "${DebugLib}"
        IMPORTED_LOCATION_RELEASE "${ReleaseLib}"
    )
endfunction()

WintersAddImportedConfigLib(Winters::Assimp
    "${WINTERS_THIRD_PARTY_DIR}/Assimp/Lib/Debug/assimp-vc143-mtd.lib"
    "${WINTERS_THIRD_PARTY_DIR}/Assimp/Lib/Release/assimp-vc143-mt.lib"
)

WintersAddImportedConfigLib(Winters::DirectXTK
    "${WINTERS_THIRD_PARTY_DIR}/DirectXTK/Lib/Debug/DirectXTK.lib"
    "${WINTERS_THIRD_PARTY_DIR}/DirectXTK/Lib/Release/DirectXTK.lib"
)

set(WINTERS_FMOD_LIB "${WINTERS_THIRD_PARTY_DIR}/FMOD/Lib/fmod_vc.lib")
WintersRequireFile("${WINTERS_FMOD_LIB}" "FMOD library")

add_library(Winters::FMOD UNKNOWN IMPORTED GLOBAL)
set_target_properties(Winters::FMOD PROPERTIES
    IMPORTED_LOCATION "${WINTERS_FMOD_LIB}"
)

add_library(Winters::WindowsGraphicsDX11 INTERFACE IMPORTED GLOBAL)
target_link_libraries(Winters::WindowsGraphicsDX11 INTERFACE
    d3d11
    dxgi
    d3dcompiler
    dxguid
    windowscodecs
    ole32
)

add_library(Winters::WindowsGraphicsDX12 INTERFACE IMPORTED GLOBAL)
target_link_libraries(Winters::WindowsGraphicsDX12 INTERFACE
    d3d12
    d3d11
    dxgi
    d3dcompiler
    dxguid
    windowscodecs
    ole32
)
```

1-6. C:/Users/user/Desktop/Winters/cmake/WintersEngine.cmake

새 파일:

```cmake
set(WINTERS_ENGINE_DIR "${WINTERS_ROOT_DIR}/Engine")
set(WINTERS_ENGINE_OUTPUT_DIR "${WINTERS_ENGINE_DIR}/Bin")

set(WINTERS_IMGUI_SOURCES
    "${WINTERS_ENGINE_DIR}/External/imgui/backends/imgui_impl_dx11.cpp"
    "${WINTERS_ENGINE_DIR}/External/imgui/backends/imgui_impl_win32.cpp"
    "${WINTERS_ENGINE_DIR}/External/imgui/imgui.cpp"
    "${WINTERS_ENGINE_DIR}/External/imgui/imgui_demo.cpp"
    "${WINTERS_ENGINE_DIR}/External/imgui/imgui_draw.cpp"
    "${WINTERS_ENGINE_DIR}/External/imgui/imgui_tables.cpp"
    "${WINTERS_ENGINE_DIR}/External/imgui/imgui_widgets.cpp"
)

set(WINTERS_LUA_SOURCES
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lapi.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lauxlib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lbaselib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lcode.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lcorolib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lctype.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/ldblib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/ldebug.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/ldo.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/ldump.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lfunc.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lgc.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/linit.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/liolib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/llex.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lmathlib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lmem.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/loadlib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lobject.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lopcodes.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/loslib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lparser.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lstate.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lstring.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lstrlib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/ltable.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/ltablib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/ltm.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lundump.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lutf8lib.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lvm.c"
    "${WINTERS_ENGINE_DIR}/External/lua-5.4.8/src/lzio.c"
)

file(GLOB_RECURSE WINTERS_ENGINE_PRIVATE_SOURCES CONFIGURE_DEPENDS
    "${WINTERS_ENGINE_DIR}/Private/*.cpp"
)

file(GLOB_RECURSE WINTERS_ENGINE_HEADERS CONFIGURE_DEPENDS
    "${WINTERS_ENGINE_DIR}/Include/*.h"
    "${WINTERS_ENGINE_DIR}/Public/*.h"
    "${WINTERS_ENGINE_DIR}/Private/*.h"
)

add_library(WintersEngine SHARED
    ${WINTERS_IMGUI_SOURCES}
    ${WINTERS_LUA_SOURCES}
    ${WINTERS_ENGINE_PRIVATE_SOURCES}
    ${WINTERS_ENGINE_HEADERS}
)

add_library(Winters::Engine ALIAS WintersEngine)

set_target_properties(WintersEngine PROPERTIES
    OUTPUT_NAME "WintersEngine"
    FOLDER "Engine"
)

WintersApplyMsvcCommonOptions(WintersEngine)
WintersSetOutputDirectories(WintersEngine "${WINTERS_ENGINE_OUTPUT_DIR}")

target_precompile_headers(WintersEngine PRIVATE
    "${WINTERS_ENGINE_DIR}/Public/WintersPCH.h"
)

set_source_files_properties(${WINTERS_LUA_SOURCES} PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON
)

target_compile_definitions(WintersEngine PRIVATE
    WINTERS_ENGINE_EXPORTS
    WIN32
    _WINDOWS
    _USRDLL
    WINTERS_PROFILING
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_include_directories(WintersEngine
    PUBLIC
        "${WINTERS_ENGINE_DIR}/Include"
        "${WINTERS_ENGINE_DIR}/Public"
    PRIVATE
        "${WINTERS_ENGINE_DIR}/External/imgui"
        "${WINTERS_ENGINE_DIR}/External/imgui/backends"
        "${WINTERS_ENGINE_DIR}/Private"
        "${WINTERS_ENGINE_DIR}/Public/ECS"
        "${WINTERS_ENGINE_DIR}/Public/ECS/Components"
        "${WINTERS_ENGINE_DIR}/Public/ECS/Systems"
        "${WINTERS_ENGINE_DIR}/Public/Sound"
        "${WINTERS_ENGINE_DIR}/ThirdPartyLib/Assimp/Inc"
        "${WINTERS_ENGINE_DIR}/ThirdPartyLib/DirectXTK/Inc"
        "${WINTERS_ENGINE_DIR}/ThirdPartyLib/FMOD/Inc"
)

target_link_libraries(WintersEngine PRIVATE
    Winters::Assimp
    Winters::DirectXTK
    Winters::FMOD
)

if(WINTERS_RHI_BACKEND STREQUAL "DX12")
    target_compile_definitions(WintersEngine PRIVATE
        WINTERS_RHI_BACKEND_DX12
    )

    target_include_directories(WintersEngine PRIVATE
        "${WINTERS_ENGINE_DIR}/ThirdPartyLib/D3D12MA/Inc"
    )

    target_link_libraries(WintersEngine PRIVATE
        Winters::WindowsGraphicsDX12
    )
else()
    target_link_libraries(WintersEngine PRIVATE
        Winters::WindowsGraphicsDX11
    )
endif()

source_group(TREE "${WINTERS_ROOT_DIR}" FILES
    ${WINTERS_IMGUI_SOURCES}
    ${WINTERS_LUA_SOURCES}
    ${WINTERS_ENGINE_PRIVATE_SOURCES}
    ${WINTERS_ENGINE_HEADERS}
)

if(WINTERS_ENABLE_ENGINE_POST_BUILD_DEPLOY)
    add_custom_command(TARGET WintersEngine POST_BUILD
        COMMAND "${WINTERS_ROOT_DIR}/UpdateLib.bat"
        WORKING_DIRECTORY "${WINTERS_ROOT_DIR}"
        COMMENT "Deploying WintersEngine artifacts through UpdateLib.bat"
        VERBATIM
    )
endif()
```

2. 검증

미검증:
- CMake configure 미검증.
- Ninja `WintersEngine` Debug 빌드 미검증.
- Ninja `WintersEngine` Release 빌드 미검증.
- CMake generated Visual Studio solution 필터 구조 미검증.
- DX12 preset 빌드 미검증.

검증 명령:

```powershell
cmake --preset msvc-ninja
cmake --build --preset engine-debug
cmake --build --preset engine-release
```

Visual Studio generator 확인:

```powershell
cmake --preset vs2022
cmake --build --preset engine-vs-debug
```

DX12 별도 확인:

```powershell
cmake --preset msvc-ninja-dx12
cmake --build --preset engine-debug-dx12
```

기존 MSBuild 병렬 검증:

```powershell
& "C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" Winters.sln /p:Configuration=Debug /p:Platform=x64 /t:Engine
& "C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" Winters.sln /p:Configuration=Release /p:Platform=x64 /t:Engine
```

정리 검증:

```powershell
git diff --check
git status --short
```

성공 기준:
- `out/build/msvc-ninja` 아래에 CMake/Ninja 생성물이 생긴다.
- `Engine/Bin/Debug/WintersEngine.dll`, `Engine/Bin/Debug/WintersEngine.lib`가 Ninja Debug 빌드로 생성된다.
- `Engine/Bin/Release/WintersEngine.dll`, `Engine/Bin/Release/WintersEngine.lib`가 Ninja Release 빌드로 생성된다.
- `UpdateLib.bat` 호출 후 `EngineSDK/inc`, `EngineSDK/lib`, `Client/Bin/Debug`, `Client/Bin/Release` 배포 흐름이 기존처럼 유지된다.
- 기존 `Winters.sln` MSBuild Engine Debug/Release 빌드가 계속 통과한다.

확인 필요:
- `Engine/Private/Tools/AssetConverter/main.cpp`는 현재 `Engine.vcxproj`에도 포함되어 있으므로 Session 1 CMake에서는 기존 구조 복제를 위해 포함한다. 이후 Tools target 분리 세션에서 Engine target에서 제외할지 별도 결정한다.
- `file(GLOB_RECURSE CONFIGURE_DEPENDS)`는 Session 1에서 `Engine/Private`만 대상으로 제한한다. CMake 안정화 후 source list를 명시형 `cmake/sources/EngineSources.cmake`로 고정할지 결정한다.
- `UpdateLib.bat`는 Debug/Release 산출물만 배포한다. DX12 산출물 배포는 기존 MSBuild도 별도 Debug-DX12/Release-DX12 경로를 쓰므로 Session 1 성공 기준에서 제외한다.
