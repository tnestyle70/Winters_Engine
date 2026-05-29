Session - vcxproj.filters를 CMake/Ninja 기반 생성 빌드로 단계적 전환한다.

목표:
- `.vcxproj.filters`를 사람이 직접 고치는 대상에서 제거한다.
- CMake가 Visual Studio solution/project/filter를 생성하게 하고, CLI 빌드는 Ninja로 빠르게 돌린다.
- 기존 `Winters.sln`은 전환 기간 동안 검증 기준으로 유지한다.

권장 결론:
- 빌드 소스 오브 트루스는 `CMakeLists.txt` + `cmake/*.cmake`.
- Visual Studio 필터는 `source_group(TREE ...)`로 자동 생성.
- CLI 빌드는 `CMakePresets.json` + Ninja.
- 파일 목록은 처음에는 명시형 `target_sources()`로 시작하고, 파일 추가가 너무 잦은 폴더만 `file(GLOB CONFIGURE_DEPENDS ...)`를 제한적으로 쓴다.

1. 설치 도구

```powershell
winget install Kitware.CMake
winget install Ninja-build.Ninja
```

Visual Studio 2022 C++ toolchain은 이미 MSBuild 빌드에 사용 중인 설치를 그대로 쓴다.

2. 첫 파일 구조

```text
CMakeLists.txt
CMakePresets.json
cmake/
  WintersCompilerOptions.cmake
  WintersThirdParty.cmake
  WintersFlatbuffers.cmake
  sources/
    EngineSources.cmake
    SharedGameSimSources.cmake
    ServerSources.cmake
    ClientSources.cmake
    ToolsSources.cmake
```

3. Preset 초안

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "msvc-debug-ninja",
      "displayName": "MSVC Debug Ninja",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/out/build/msvc-debug-ninja",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_CXX_STANDARD": "20",
        "CMAKE_MSVC_RUNTIME_LIBRARY": "MultiThreadedDebugDLL"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "server-debug",
      "configurePreset": "msvc-debug-ninja",
      "targets": ["WintersServer"]
    },
    {
      "name": "client-debug",
      "configurePreset": "msvc-debug-ninja",
      "targets": ["WintersClient"]
    }
  ]
}
```

4. 필터 자동화 핵심

Visual Studio generator를 쓰면 CMake가 `.vcxproj`와 `.vcxproj.filters`를 생성한다. Ninja를 쓰면 필터 파일 자체가 필요 없다.

```cmake
target_sources(WintersServer PRIVATE ${WINTERS_SERVER_SOURCES})
source_group(TREE "${CMAKE_SOURCE_DIR}" FILES ${WINTERS_SERVER_SOURCES})
```

이 한 줄이 손으로 XML filter를 고치던 일을 대체한다. 폴더 이동/파일 추가는 CMake source list만 바꾸면 된다.

5. 전환 순서

1. `Engine`만 CMake target으로 만든다.
   - 산출물: `WintersEngine.dll`, `WintersEngine.lib`
   - 검증: 기존 `msbuild /t:Engine`과 CMake `WintersEngine` 산출물이 같은 위치 정책을 따르는지 확인.

2. `Shared/GameSim` 소스 묶음을 `INTERFACE` 또는 `OBJECT` 라이브러리로 분리한다.
   - Server와 Client가 같은 GameSim 코드를 링크해야 하므로 중복 source list를 없앤다.

3. `Server`를 붙인다.
   - FlatBuffers codegen은 `add_custom_command()`로 연결한다.
   - 검증: `cmake --build --preset server-debug`.

4. `Client`를 붙인다.
   - DX11/Assimp/DirectXTK/FMOD 경로를 `IMPORTED` target으로 정리한다.
   - 검증: `cmake --build --preset client-debug`.

5. `Tools/WintersAssetConverter`를 붙인다.
   - 빌드 도구도 같은 dependency 선언을 재사용한다.

6. 기존 `.sln/.vcxproj`와 CMake 산출물을 1~2주 병렬 운영한다.
   - 이 기간에는 둘 다 빌드가 통과해야 한다.
   - CMake가 안정화되면 `.vcxproj.filters`를 수동 편집 금지 대상으로 지정한다.

6. Winters에서 특히 조심할 점

- `EngineSDK/inc`는 현재 `UpdateLib.bat` 산출물이다. CMake 초기에는 기존 `UpdateLib.bat`를 custom target으로 호출해서 동작을 보존한다.
- FlatBuffers generated cpp/go 파일은 기존 `Shared/Schemas/run_codegen.bat` 결과와 동일해야 한다.
- Client/Server 둘 다 쓰는 `Shared/GameSim`은 별도 target으로 빼야 파일 추가 누락이 줄어든다.
- 외부 라이브러리는 경로 문자열을 각 target에 흩뿌리지 말고 `WintersThirdParty.cmake`의 imported target으로 모은다.
- 기존 normal F5 흐름을 깨지 않기 위해 Visual Studio generator도 같이 지원한다.

7. 일상 명령

```powershell
cmake --preset msvc-debug-ninja
cmake --build --preset server-debug -j
cmake --build --preset client-debug -j
```

Visual Studio 프로젝트가 필요할 때:

```powershell
cmake -S . -B out/vs2022 -G "Visual Studio 17 2022" -A x64
cmake --build out/vs2022 --config Debug --target WintersServer
```

8. 완료 기준

- Ninja Server Debug 빌드 성공.
- Ninja Client Debug 빌드 성공.
- Visual Studio generator로 생성한 solution에서 필터가 실제 폴더 구조대로 보임.
- 기존 `Winters.sln` Debug x64 Server/Client 빌드와 산출물 차이가 설명 가능함.
- 신규 C++ 파일 추가 시 `.vcxproj.filters` 직접 수정이 필요 없음.
