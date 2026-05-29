Session - CMake/Ninja에 Shared/GameSim과 Server target을 연결한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/CMakeLists.txt

기존 코드:

```cmake
include(WintersCompilerOptions)
include(WintersThirdParty)
include(WintersEngine)
```

아래로 교체:

```cmake
include(WintersCompilerOptions)
include(WintersThirdParty)
include(WintersFlatbuffers)
include(WintersEngine)
include(WintersSharedGameSim)
include(WintersServer)
```

1-2. C:/Users/user/Desktop/Winters/CMakePresets.json

기존 코드:

```json
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
```

아래로 교체:

```json
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
      "name": "server-debug",
      "displayName": "Build WintersServer Debug with Ninja",
      "configurePreset": "msvc-ninja",
      "configuration": "Debug",
      "targets": [
        "WintersServer"
      ]
    },
    {
      "name": "server-release",
      "displayName": "Build WintersServer Release with Ninja",
      "configurePreset": "msvc-ninja",
      "configuration": "Release",
      "targets": [
        "WintersServer"
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
    },
    {
      "name": "server-vs-debug",
      "displayName": "Build WintersServer Debug with Visual Studio generator",
      "configurePreset": "vs2022",
      "configuration": "Debug",
      "targets": [
        "WintersServer"
      ]
    }
  ]
```

1-3. C:/Users/user/Desktop/Winters/cmake/WintersFlatbuffers.cmake

새 파일:

```cmake
set(WINTERS_SCHEMA_DIR "${WINTERS_ROOT_DIR}/Shared/Schemas")

set(WINTERS_FLATBUFFERS_SCHEMA_FILES
    "${WINTERS_SCHEMA_DIR}/Command.fbs"
    "${WINTERS_SCHEMA_DIR}/Snapshot.fbs"
    "${WINTERS_SCHEMA_DIR}/Event.fbs"
    "${WINTERS_SCHEMA_DIR}/Hello.fbs"
    "${WINTERS_SCHEMA_DIR}/LobbyTypes.fbs"
    "${WINTERS_SCHEMA_DIR}/LobbyState.fbs"
    "${WINTERS_SCHEMA_DIR}/LobbyCommand.fbs"
)

add_custom_target(WintersFlatbuffersCodegen
    COMMAND "${WINTERS_SCHEMA_DIR}/run_codegen.bat"
    WORKING_DIRECTORY "${WINTERS_SCHEMA_DIR}"
    SOURCES ${WINTERS_FLATBUFFERS_SCHEMA_FILES}
    COMMENT "Generating FlatBuffers schema outputs"
    VERBATIM
)

set_target_properties(WintersFlatbuffersCodegen PROPERTIES
    FOLDER "Generated"
)
```

1-4. C:/Users/user/Desktop/Winters/cmake/WintersSharedGameSim.cmake

새 파일:

```cmake
set(WINTERS_SHARED_GAMESIM_DIR "${WINTERS_ROOT_DIR}/Shared/GameSim")

set(WINTERS_SHARED_GAMESIM_SOURCES
    "${WINTERS_SHARED_GAMESIM_DIR}/Champions/AsheGameSim.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Champions/FioraGameSim.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Champions/IreliaGameSim.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Champions/JaxGameSim.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Champions/KindredGameSim.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Champions/LeeSinGameSim.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Champions/MasterYiGameSim.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Champions/ViegoGameSim.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Champions/YasuoGameSim.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Champions/YoneGameSim.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Definitions/ChampionRuntimeDefaults.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Definitions/MapSpawnPoints.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Registries/ChampionStatsRegistry.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Registries/RewardRegistry.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Registries/SkillScalingRegistry.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Registries/SkinRegistry.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/AreaAuraSystem.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/AttackChaseSystem.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/BuffSystem.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/ChampionAIPolicy.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/ChampionAISystem.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/CombatActionSystem.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/CombatFormula.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/CommandExecutor.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/DamagePipeline.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/DamageQueueSystem.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/DeathSystem.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/ExperienceSystem.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/GameplayHookRegistry.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/GameplayStateQuery.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/MoveSystem.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/RecallSystem.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/ReplicatedEventSerializer.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/SkillCooldownSystem.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/SkillRankSystem.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/SpellbookFormOverrideSystem.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/StatSystem.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/StatusEffectSystem.cpp"
    "${WINTERS_SHARED_GAMESIM_DIR}/Systems/WaypointPatrolSystem.cpp"
)

file(GLOB_RECURSE WINTERS_SHARED_GAMESIM_HEADERS CONFIGURE_DEPENDS
    "${WINTERS_SHARED_GAMESIM_DIR}/*.h"
)

add_library(WintersGameSim OBJECT
    ${WINTERS_SHARED_GAMESIM_SOURCES}
    ${WINTERS_SHARED_GAMESIM_HEADERS}
)

add_library(Winters::GameSim ALIAS WintersGameSim)

set_target_properties(WintersGameSim PROPERTIES
    FOLDER "Shared"
)

target_compile_features(WintersGameSim PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(WintersGameSim PRIVATE
        /W3
        /sdl
        /permissive-
        /utf-8
        /FS
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/Gy>
    )

    set_property(TARGET WintersGameSim PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
endif()

target_compile_definitions(WintersGameSim PUBLIC
    WIN32
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_include_directories(WintersGameSim PUBLIC
    "${WINTERS_ROOT_DIR}/Shared"
    "${WINTERS_ROOT_DIR}/EngineSDK/inc"
    "${WINTERS_ROOT_DIR}/Engine/Public"
    "${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/FlatBuffers/Inc"
)

add_dependencies(WintersGameSim WintersFlatbuffersCodegen)

source_group(TREE "${WINTERS_ROOT_DIR}" FILES
    ${WINTERS_SHARED_GAMESIM_SOURCES}
    ${WINTERS_SHARED_GAMESIM_HEADERS}
)
```

1-5. C:/Users/user/Desktop/Winters/cmake/WintersServer.cmake

새 파일:

```cmake
set(WINTERS_SERVER_DIR "${WINTERS_ROOT_DIR}/Server")
set(WINTERS_SERVER_OUTPUT_DIR "${WINTERS_SERVER_DIR}/Bin")

set(WINTERS_SERVER_SOURCES
    "${WINTERS_SERVER_DIR}/Private/Game/AOI.cpp"
    "${WINTERS_SERVER_DIR}/Private/Game/CommandDispatcher.cpp"
    "${WINTERS_SERVER_DIR}/Private/Game/GameLogic.cpp"
    "${WINTERS_SERVER_DIR}/Private/Game/GameRoom.cpp"
    "${WINTERS_SERVER_DIR}/Private/Game/ReplayRecorder.cpp"
    "${WINTERS_SERVER_DIR}/Private/Game/ServerEntry.cpp"
    "${WINTERS_SERVER_DIR}/Private/Game/ServerMinionFlowField.cpp"
    "${WINTERS_SERVER_DIR}/Private/Game/ServerMinionWaveRuntime.cpp"
    "${WINTERS_SERVER_DIR}/Private/Game/ServerWorld.cpp"
    "${WINTERS_SERVER_DIR}/Private/Game/SnapshotBuilder.cpp"
    "${WINTERS_SERVER_DIR}/Private/main.cpp"
    "${WINTERS_SERVER_DIR}/Private/Network/FrameParser.cpp"
    "${WINTERS_SERVER_DIR}/Private/Network/IOCPCore.cpp"
    "${WINTERS_SERVER_DIR}/Private/Network/PacketDispatcher.cpp"
    "${WINTERS_SERVER_DIR}/Private/Network/Session.cpp"
    "${WINTERS_SERVER_DIR}/Private/Network/Session_Manager.cpp"
    "${WINTERS_SERVER_DIR}/Private/Security/AntiCheatServer.cpp"
    "${WINTERS_SERVER_DIR}/Private/Security/LagCompensation.cpp"
)

file(GLOB_RECURSE WINTERS_SERVER_HEADERS CONFIGURE_DEPENDS
    "${WINTERS_SERVER_DIR}/Public/*.h"
)

add_executable(WintersServer
    ${WINTERS_SERVER_SOURCES}
    ${WINTERS_SERVER_HEADERS}
)

set_target_properties(WintersServer PROPERTIES
    OUTPUT_NAME "WintersServer"
    FOLDER "Server"
)

WintersApplyMsvcCommonOptions(WintersServer)
WintersSetOutputDirectories(WintersServer "${WINTERS_SERVER_OUTPUT_DIR}")

target_compile_definitions(WintersServer PRIVATE
    WIN32
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CONSOLE
    UNICODE
    _UNICODE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_include_directories(WintersServer PRIVATE
    "${WINTERS_SERVER_DIR}"
    "${WINTERS_SERVER_DIR}/Public"
    "${WINTERS_ROOT_DIR}/Shared"
    "${WINTERS_ROOT_DIR}/EngineSDK/inc"
    "${WINTERS_ROOT_DIR}/Engine/Public"
    "${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/FlatBuffers/Inc"
)

target_link_libraries(WintersServer PRIVATE
    Winters::GameSim
    Winters::Engine
    ws2_32
    Mswsock
)

add_dependencies(WintersServer
    WintersFlatbuffersCodegen
    WintersEngine
)

source_group(TREE "${WINTERS_ROOT_DIR}" FILES
    ${WINTERS_SERVER_SOURCES}
    ${WINTERS_SERVER_HEADERS}
)

set(WINTERS_ASSIMP_RUNTIME_DIR
    "$<IF:$<CONFIG:Debug>,${WINTERS_THIRD_PARTY_DIR}/Assimp/Bin/Debug,${WINTERS_THIRD_PARTY_DIR}/Assimp/Bin/Release>"
)

set(WINTERS_DIRECTXTK_RUNTIME_DIR
    "$<IF:$<CONFIG:Debug>,${WINTERS_THIRD_PARTY_DIR}/DirectXTK/Bin/Debug,${WINTERS_THIRD_PARTY_DIR}/DirectXTK/Bin/Release>"
)

add_custom_command(TARGET WintersServer POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:WintersEngine>"
        "$<TARGET_FILE_DIR:WintersServer>"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${WINTERS_ASSIMP_RUNTIME_DIR}"
        "$<TARGET_FILE_DIR:WintersServer>"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${WINTERS_DIRECTXTK_RUNTIME_DIR}"
        "$<TARGET_FILE_DIR:WintersServer>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${WINTERS_THIRD_PARTY_DIR}/FMOD/Bin/fmod.dll"
        "$<TARGET_FILE_DIR:WintersServer>"
    COMMENT "Copying WintersServer runtime dependencies"
    VERBATIM
)
```

2. 검증

미검증
- CMake `Shared/GameSim` object target 빌드 미검증.
- CMake `WintersServer` Debug/Release 빌드 미검증.
- Visual Studio generator에서 `WintersServer.vcxproj.filters` 자동 생성 구조 미검증.
- 기존 MSBuild Server Debug/Release 병렬 통과 미검증.

검증 명령:

```powershell
cmd /c "set ""PATH=C:\Users\user\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe;%PATH%"" && ""C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"" && cmake --preset msvc-ninja"
cmd /c "set ""PATH=C:\Users\user\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe;%PATH%"" && ""C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"" && cmake --build --preset server-debug"
cmd /c "set ""PATH=C:\Users\user\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe;%PATH%"" && ""C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"" && cmake --build --preset server-release"
cmd /c """C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"" && cmake --preset vs2022"
cmd /c """C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"" && cmake --build --preset server-vs-debug"
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Winters.sln /p:Configuration=Debug /p:Platform=x64 /t:Server
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Winters.sln /p:Configuration=Release /p:Platform=x64 /t:Server
git diff --check
```

성공 기준:
- `Server/Bin/Debug/WintersServer.exe`가 CMake/Ninja Debug 빌드로 생성된다.
- `Server/Bin/Release/WintersServer.exe`가 CMake/Ninja Release 빌드로 생성된다.
- `out/build/vs2022/WintersServer.vcxproj.filters`가 CMake Visual Studio generator로 생성된다.
- 기존 `Winters.sln` Server Debug/Release 빌드가 계속 통과한다.
- `git diff --check`가 통과한다.

확인 필요:
- `Shared/GameSim/Champions/ZedGameSim.cpp`는 현재 파일은 있으나 `Server.vcxproj`에 포함되어 있지 않으므로 Session 2 CMake source list에도 포함하지 않는다.
- `WintersFlatbuffersCodegen`은 `run_codegen.bat`를 그대로 호출하므로 cpp/go generated output이 동시에 갱신될 수 있다.
- `WintersGameSim`은 `OBJECT` target으로 시작한다. Client target 연결 시 같은 object target을 재사용할지, Client용 별도 source 묶음을 둘지는 Client CMake Session에서 결정한다.
