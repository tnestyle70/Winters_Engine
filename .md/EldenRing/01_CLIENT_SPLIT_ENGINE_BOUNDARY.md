# Client Split And Engine Boundary

## 목표

`Client.vcxproj` 하나에 장르별 코드를 계속 쌓지 않고, 클라이언트 프로젝트를 분리한다.

```
WintersEngine.dll
├── WintersLOL.exe
└── WintersElden.exe
```

이 구조는 포트폴리오에서 가장 강한 메시지를 만든다.

> 하나의 자체 C++ DX11 엔진 DLL이 MOBA와 3인칭 액션 RPG라는 서로 다른 게임 클라이언트를 구동한다.

## 현재 구조 해석

현재 `Engine`은 이미 `IWintersApp` 경계로 게임 로직을 받는다.

```
Client/main.cpp
  CGameApp gameApp;
  WintersRun(&gameApp, config);

Engine/WintersEngine.cpp
  WintersRun(IWintersApp* app, EngineConfig config)
    CGameInstance::Initialize_Engine(...)
    CEngineApp::Initialize(app, config)
    CEngineApp::Run()
```

즉, 엔진은 이미 "게임 앱을 주입받는 런타임"이다. 두 번째 클라이언트를 추가해도 엔진 구조 철학과 충돌하지 않는다.

## 최종 디렉토리 목표

1차는 안전하게 새 프로젝트를 추가한다. 기존 `Client/`를 바로 대규모 리네임하지 않는다.

```
Winters/
├── Engine/
├── EngineSDK/
├── Shared/
├── Client/                  // 현재 LoL 클라이언트. 추후 WintersLOL로 리네임 가능
├── WintersElden/
│   ├── Include/
│   │   ├── WintersElden.vcxproj
│   │   └── WintersElden.vcxproj.filters
│   ├── Public/
│   │   ├── CEldenGameApp.h
│   │   ├── Defines.h
│   │   ├── Scene/
│   │   ├── Camera/
│   │   ├── Combat/
│   │   ├── World/
│   │   ├── Raid/
│   │   └── UI/
│   ├── Private/
│   │   ├── main.cpp
│   │   ├── CEldenGameApp.cpp
│   │   ├── Scene/
│   │   ├── Camera/
│   │   ├── Combat/
│   │   ├── World/
│   │   ├── Raid/
│   │   └── UI/
│   └── Bin/
│       ├── Debug/
│       ├── Release/
│       └── Resource/
└── Tools/
```

추후 리네임 안정화:

```
Client/        -> WintersLOL/
Client.vcxproj -> WintersLOL.vcxproj
WintersGame.exe -> WintersLOL.exe
```

## vcxproj 분리 규칙

`WintersElden.vcxproj`는 `Client.vcxproj`를 복제하되 아래만 분리한다.

| 항목 | WintersLOL | WintersElden |
|---|---|---|
| TargetName | `WintersLOL` 또는 기존 `WintersGame` | `WintersElden` |
| OutDir | `Client/Bin/{Config}` | `WintersElden/Bin/{Config}` |
| Resource copy source | `Client/Bin/Resource` | `WintersElden/Bin/Resource` |
| Public include root | `Client/Public` | `WintersElden/Public` |
| LocalDebuggerWorkingDirectory | `$(SolutionDir)` 유지 | `$(SolutionDir)` 유지 |
| Engine link | `EngineSDK/lib/WintersEngine.lib` | 동일 |
| Engine DLL runtime copy | `Client/Bin/{Config}` | `WintersElden/Bin/{Config}`에도 복사 |

## UpdateLib.bat 확장

현재 `UpdateLib.bat`은 `EngineSDK`와 `Client/Bin/{Debug,Release}`에 엔진 DLL/서드파티 DLL을 복사한다.

Elden 분리 후에는 다음 출력도 추가한다.

```
WintersElden/Bin/Debug/
WintersElden/Bin/Release/
```

복사 대상:

| 파일 | 이유 |
|---|---|
| `WintersEngine.dll` | 공통 엔진 런타임 |
| `WintersEngine.pdb` | Debug 디버깅 |
| Assimp DLLs | 런타임/변환 fallback |
| DirectXTK DLLs | WIC/Texture helpers |
| `fmod.dll` | 공통 오디오 |

주의:

1. `UpdateLib.bat`은 여러 클라이언트 출력으로 복사해도 엔진 소스 의존 방향을 깨지 않는다.
2. 엔진은 `WintersElden`을 절대 include하지 않는다.
3. 클라이언트 간 상호 include도 금지한다.

## EngineConfig 분리

`WintersElden/Private/main.cpp` 예시:

```cpp
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    CEldenGameApp gameApp;

    EngineConfig config;
    config.windowTitle = L"Winters Elden | Action RPG Prototype";
    config.windowWidth = 1600;
    config.windowHeight = 900;
    config.vsync = true;
    config.fullscreen = false;
    config.iNumScenes = static_cast<uint32_t>(eEldenSceneID::End);

    return WintersRun(&gameApp, config);
}
```

## Scene ID 분리

LoL의 `eSceneID`와 Elden의 `eEldenSceneID`는 섞지 않는다.

```cpp
enum class eEldenSceneID : uint32_t
{
    Boot = 0,
    Loading,
    MainMenu,
    CharacterPreview,
    FieldTest,
    RaidTest,
    Editor,
    End
};
```

## 게임별 책임 경계

| 영역 | Engine | WintersLOL | WintersElden |
|---|---|---|---|
| Win32 window | O | X | X |
| DX11 device/RHI | O | X | X |
| ResourceCache base | O | X | X |
| Model/Animator | O | 사용 | 사용 |
| ECS storage | O | 사용 | 사용 |
| Scene 구현 | 인터페이스만 | MOBA scenes | Action RPG scenes |
| DynamicCamera | 기본 Camera만 | LoL top-down | Elden third-person |
| Combat | 공용 컴포넌트/유틸 가능 | Skill/QWER/MOBA | stamina/action/hitbox |
| World Partition | 엔진 시스템 후보 | 거의 미사용 | 핵심 사용 |
| Raid/PvP 룰 | X 또는 Shared | MOBA match | Elden raid/duel |
| UI | ImGui/renderer infra | HUD/BanPick | lock-on/raid/editor |

## Shared 사용 기준

`Shared/`에는 게임 장르와 무관한 순수 데이터/프로토콜만 둔다.

가능:

```
Shared/NetProtocol/
Shared/GameSim/Common/
Shared/Math/
Shared/Serialization/
```

금지:

```
Shared/EldenBossSpecificPattern.cpp
Shared/IreliaSpecificSkill.cpp
```

Elden 전용 룰은 `WintersElden/` 또는 서버의 Elden module에 둔다. 다만 서버/클라가 동일 판정을 공유해야 하는 POD와 pure function은 `Shared/GameSim/Elden/`로 승격 가능하다.

## 포트폴리오 메시지

이 분리는 단순 파일 정리가 아니다.

면접에서 보여줄 수 있는 포인트:

1. 엔진 DLL과 게임 EXE의 ABI/API 경계 설계
2. 동일 엔진에서 장르별 클라이언트 분리
3. 공통 에셋 포맷과 공통 렌더/애니 시스템 재사용
4. 게임별 카메라/전투/네트워크 요구사항 분리
5. Unreal 같은 범용 엔진의 큰 기능들을 자체 엔진 모듈로 설계한 경험

## 완료 기준

| 체크 | 기준 |
|---|---|
| Solution | `WintersElden` 프로젝트 추가 |
| Build | `WintersElden.exe` 빌드 성공 |
| Runtime | 같은 `WintersEngine.dll`로 부팅 |
| Resource | `WintersElden/Bin/Resource`에서 shader/resource copy 정상 |
| Scene | `Scene_EldenFieldTest` 진입 |
| Isolation | LoL 코드 include 없이 동작 |
