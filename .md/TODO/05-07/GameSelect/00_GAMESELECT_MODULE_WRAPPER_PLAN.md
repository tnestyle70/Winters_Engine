# GameSelect / GameModule Wrapper 1차 계획

작성일: 2026-05-08 KST
대상: 단일 `WintersGame.exe` 안에서 `WintersLOL`, `WintersElden`, `ClassServant` 를 선택 실행하는 첫 구조
상태: 검토용 계획서. 코드 반영 전.

---

## 1. 목표

현재 클라이언트는 `CGameApp::OnInit()` 에서 LoL 전용 매니저와 챔피언 등록을 초기화한 뒤 바로 `Loading -> BanPick` 으로 들어간다.

이번 1차 목표는 기존 LoL 코드를 이동하지 않고, 시작 지점만 다음 구조로 바꾸는 것이다.

```text
CGameApp
  -> Scene_GameSelect
  -> CGameModuleRegistry
  -> LOLGameModule
  -> 기존 Loading -> BanPick -> MatchLoading -> InGame
```

중요:

- `Scene_InGame` 에 Elden / ClassServant 분기를 넣지 않는다.
- 기존 LoL 씬과 챔피언 코드를 대이동하지 않는다.
- `LOLGameModule` 은 기존 플로우를 감싸기만 한다.
- `Elden` 과 `ClassServant` 는 이번 단계에서 비활성 placeholder 로만 둔다.

---

## 2. 현재 기준선

현재 시작 흐름:

```text
Client/Private/main.cpp
  -> WintersRun(&gameApp, config)
  -> CEngineApp::Initialize()
  -> CGameApp::OnInit()
  -> CStructure_Manager / CJungle_Manager / CMinion_Manager Initialize
  -> champion KeepAlive + RegisterAllLegacy
  -> CScene_Loading(BanPick)
```

현재 scene id:

```cpp
enum class eSceneID : int
{
    MainMenu,
    BanPick,
    Shop,
    MatchLoading,
    InGame,
    Editor,
    Result,
    SceneLoading,
    End
};
```

현재 문제:

- `CGameApp` 이 LoL 전용 초기화와 앱 부트스트랩을 동시에 한다.
- `GameSelect` 가 없어 멀티게임 제품 선택 지점이 없다.
- Account/Login/Store/Friend 같은 공통 쉘이 들어올 자리와 LoL 런타임이 아직 같은 시작 흐름에 묶여 있다.

---

## 3. 1차 반영 범위

신규 파일:

```text
Client/Public/GameModule/GameProduct.h
Client/Public/GameModule/GameLaunchConfig.h
Client/Public/GameModule/IGameModule.h
Client/Public/GameModule/GameModuleRegistry.h
Client/Private/GameModule/GameModuleRegistry.cpp
Client/Public/GameModule/LOL/LOLGameModule.h
Client/Private/GameModule/LOL/LOLGameModule.cpp
Client/Public/Scene/Scene_GameSelect.h
Client/Private/Scene/Scene_GameSelect.cpp
```

수정 파일:

```text
Client/Public/Defines.h
Client/Private/CGameApp.cpp
Client/Include/Client.vcxproj
Client/Include/Client.vcxproj.filters
```

이번 단계에서 만들지 않는 것:

```text
Client/Public/GameMode/IGameMode.h
Client/Public/ClientShell/*
Content/*.json
gameMode.json
Elden 실제 InGame scene
Friend / Replay / Store 화면
```

---

## 4. 책임 분리

```text
CGameApp
  - 엔진 초기화 후 첫 scene 을 GameSelect 로 연결
  - active module shutdown 호출

CScene_GameSelect
  - 제품 선택 UI
  - module registry 에서 module 조회
  - 선택된 module 초기화
  - module 이 만든 initial scene 으로 Change_Scene

CGameModuleRegistry
  - module 등록과 active module 수명 관리

LOLGameModule
  - 현재 CGameApp 에 있던 LoL 전용 초기화 흡수
  - 기존 Loading -> BanPick 플로우 생성
  - 종료 시 LoL manager shutdown
```

---

## 5. 신규 코드 전문

### 5.1 `Client/Public/GameModule/GameProduct.h`

```cpp
#pragma once

#include "Defines.h"

enum class eGameProduct : u32_t
{
    None = 0,
    WintersLOL,
    WintersElden,
    ClassServant,
};

inline const char* GetGameProductName(eGameProduct product)
{
    switch (product)
    {
    case eGameProduct::WintersLOL:
        return "Winters League";
    case eGameProduct::WintersElden:
        return "Winters Elden";
    case eGameProduct::ClassServant:
        return "Class & Servant";
    default:
        return "None";
    }
}
```

### 5.2 `Client/Public/GameModule/GameLaunchConfig.h`

```cpp
#pragma once

#include "GameModule/GameProduct.h"

struct GameLaunchConfig
{
    eGameProduct eProduct = eGameProduct::None;
    wstring_t strContentRoot{};
    wstring_t strServiceNamespace{};
    wstring_t strServerEndpoint{};
    bool_t bUseOnlineServices = false;
    bool_t bUseEditorTools = true;
};
```

### 5.3 `Client/Public/GameModule/IGameModule.h`

```cpp
#pragma once

#include "Defines.h"
#include "GameModule/GameLaunchConfig.h"
#include "IScene.h"

#include <memory>

class IGameModule
{
public:
    virtual ~IGameModule() = default;

    virtual eGameProduct GetProductID() const = 0;
    virtual const char* GetDisplayName() const = 0;
    virtual bool_t IsAvailable() const = 0;

    virtual bool_t InitializeClient(const GameLaunchConfig& config) = 0;
    virtual void ShutdownClient() = 0;

    virtual eSceneID GetInitialSceneID() const = 0;
    virtual std::unique_ptr<IScene> CreateInitialScene() = 0;
};
```

### 5.4 `Client/Public/GameModule/GameModuleRegistry.h`

```cpp
#pragma once

#include "Defines.h"
#include "GameModule/IGameModule.h"

#include <memory>
#include <vector>

class CGameModuleRegistry final
{
public:
    static CGameModuleRegistry& Instance();

    void Register(std::unique_ptr<IGameModule> pModule);
    void RegisterDefaults();

    IGameModule* Find(eGameProduct product) const;
    const std::vector<std::unique_ptr<IGameModule>>& GetModules() const { return m_Modules; }

    bool_t Activate(eGameProduct product, const GameLaunchConfig& config);
    void ShutdownActiveModule();
    IGameModule* GetActiveModule() const { return m_pActiveModule; }
    eGameProduct GetActiveProduct() const;

private:
    CGameModuleRegistry() = default;
    ~CGameModuleRegistry() = default;
    CGameModuleRegistry(const CGameModuleRegistry&) = delete;
    CGameModuleRegistry& operator=(const CGameModuleRegistry&) = delete;

    bool_t IsRegistered(eGameProduct product) const;

    std::vector<std::unique_ptr<IGameModule>> m_Modules{};
    IGameModule* m_pActiveModule = nullptr;
};
```

### 5.5 `Client/Private/GameModule/GameModuleRegistry.cpp`

```cpp
#include "GameModule/GameModuleRegistry.h"
#include "GameModule/LOL/LOLGameModule.h"

#include <Windows.h>

namespace
{
    class CPlaceholderGameModule final : public IGameModule
    {
    public:
        explicit CPlaceholderGameModule(eGameProduct product)
            : m_eProduct(product)
        {
        }

        eGameProduct GetProductID() const override { return m_eProduct; }
        const char* GetDisplayName() const override { return GetGameProductName(m_eProduct); }
        bool_t IsAvailable() const override { return false; }

        bool_t InitializeClient(const GameLaunchConfig& config) override
        {
            (void)config;
            return false;
        }

        void ShutdownClient() override {}
        eSceneID GetInitialSceneID() const override { return eSceneID::GameSelect; }
        std::unique_ptr<IScene> CreateInitialScene() override { return nullptr; }

    private:
        eGameProduct m_eProduct = eGameProduct::None;
    };
}

CGameModuleRegistry& CGameModuleRegistry::Instance()
{
    static CGameModuleRegistry s_Instance;
    return s_Instance;
}

void CGameModuleRegistry::Register(std::unique_ptr<IGameModule> pModule)
{
    if (!pModule)
        return;
    if (IsRegistered(pModule->GetProductID()))
        return;

    m_Modules.push_back(std::move(pModule));
}

void CGameModuleRegistry::RegisterDefaults()
{
    Register(CLOLGameModule::Create());
    Register(std::unique_ptr<IGameModule>(new CPlaceholderGameModule(eGameProduct::WintersElden)));
    Register(std::unique_ptr<IGameModule>(new CPlaceholderGameModule(eGameProduct::ClassServant)));
}

IGameModule* CGameModuleRegistry::Find(eGameProduct product) const
{
    for (const auto& pModule : m_Modules)
    {
        if (pModule && pModule->GetProductID() == product)
            return pModule.get();
    }
    return nullptr;
}

bool_t CGameModuleRegistry::Activate(eGameProduct product, const GameLaunchConfig& config)
{
    IGameModule* pModule = Find(product);
    if (!pModule || !pModule->IsAvailable())
        return false;

    if (m_pActiveModule && m_pActiveModule != pModule)
        ShutdownActiveModule();

    if (!m_pActiveModule)
    {
        if (!pModule->InitializeClient(config))
        {
            OutputDebugStringA("[GameModuleRegistry] InitializeClient failed\n");
            return false;
        }
        m_pActiveModule = pModule;
    }

    return true;
}

void CGameModuleRegistry::ShutdownActiveModule()
{
    if (!m_pActiveModule)
        return;

    m_pActiveModule->ShutdownClient();
    m_pActiveModule = nullptr;
}

eGameProduct CGameModuleRegistry::GetActiveProduct() const
{
    return m_pActiveModule ? m_pActiveModule->GetProductID() : eGameProduct::None;
}

bool_t CGameModuleRegistry::IsRegistered(eGameProduct product) const
{
    return Find(product) != nullptr;
}
```

### 5.6 `Client/Public/GameModule/LOL/LOLGameModule.h`

```cpp
#pragma once

#include "GameModule/IGameModule.h"

class CLOLGameModule final : public IGameModule
{
public:
    static std::unique_ptr<CLOLGameModule> Create();

    eGameProduct GetProductID() const override { return eGameProduct::WintersLOL; }
    const char* GetDisplayName() const override { return "Winters League"; }
    bool_t IsAvailable() const override { return true; }

    bool_t InitializeClient(const GameLaunchConfig& config) override;
    void ShutdownClient() override;

    eSceneID GetInitialSceneID() const override { return eSceneID::SceneLoading; }
    std::unique_ptr<IScene> CreateInitialScene() override;

private:
    CLOLGameModule() = default;
    ~CLOLGameModule() override = default;
    CLOLGameModule(const CLOLGameModule&) = delete;
    CLOLGameModule& operator=(const CLOLGameModule&) = delete;

    bool_t m_bInitialized = false;
    GameLaunchConfig m_Config{};
};
```

### 5.7 `Client/Private/GameModule/LOL/LOLGameModule.cpp`

```cpp
#include "GameModule/LOL/LOLGameModule.h"

#include "GameObject/ChampionDef.h"
#include "GamePlay/ChampionCatalog.h"
#include "Manager/Jungle_Manager.h"
#include "Manager/Minion_Manager.h"
#include "Manager/Structure_Manager.h"
#include "Scene/Scene_BanPick.h"
#include "Scene/Scene_Loading.h"

namespace
{
    void RegisterLOLChampions()
    {
        extern void Ezreal_KeepAlive();
        extern void Fiora_KeepAlive();
        extern void Jax_KeepAlive();
        extern void Annie_KeepAlive();
        extern void Ashe_KeepAlive();
        extern void Yone_KeepAlive();

        Ezreal_KeepAlive();
        Fiora_KeepAlive();
        Jax_KeepAlive();
        Annie_KeepAlive();
        Ashe_KeepAlive();
        Yone_KeepAlive();
        RegisterAllLegacy();

        CChampionCatalog::Instance().RebuildFromRegistry();
    }
}

std::unique_ptr<CLOLGameModule> CLOLGameModule::Create()
{
    return std::unique_ptr<CLOLGameModule>(new CLOLGameModule());
}

bool_t CLOLGameModule::InitializeClient(const GameLaunchConfig& config)
{
    if (m_bInitialized)
        return true;

    m_Config = config;

    CStructure_Manager::Get()->Initialize(nullptr);
    CJungle_Manager::Get()->Initialize(nullptr);
    CMinion_Manager::Get()->Initialize(nullptr);

    RegisterLOLChampions();

    m_bInitialized = true;
    return true;
}

void CLOLGameModule::ShutdownClient()
{
    if (!m_bInitialized)
        return;

    CJungle_Manager::Get()->Shutdown();
    CStructure_Manager::Get()->Shutdown();
    CMinion_Manager::Get()->ShutDown();

    m_bInitialized = false;
}

std::unique_ptr<IScene> CLOLGameModule::CreateInitialScene()
{
    return CScene_Loading::Create(
        eSceneID::BanPick,
        []() -> std::unique_ptr<IScene>
        {
            return CScene_BanPick::Create();
        });
}
```

### 5.8 `Client/Public/Scene/Scene_GameSelect.h`

```cpp
#pragma once

#include "IScene.h"
#include "GameModule/GameLaunchConfig.h"

#include <memory>
#include <string>

class CScene_GameSelect final : public IScene
{
public:
    static std::unique_ptr<CScene_GameSelect> Create();

    bool OnEnter() override;
    void OnExit() override;
    void OnUpdate(f32_t dt) override;
    void OnLateUpdate(f32_t dt) override;
    void OnRender() override;
    void OnImGui() override;

private:
    CScene_GameSelect() = default;
    ~CScene_GameSelect() override = default;
    CScene_GameSelect(const CScene_GameSelect&) = delete;
    CScene_GameSelect& operator=(const CScene_GameSelect&) = delete;

    GameLaunchConfig BuildLaunchConfig(eGameProduct product) const;
    bool_t TryLaunch(eGameProduct product);
    void DrawProductButton(eGameProduct product);

    std::string m_strStatus{};
};
```

### 5.9 `Client/Private/Scene/Scene_GameSelect.cpp`

```cpp
#include "Scene/Scene_GameSelect.h"

#include "GameInstance.h"
#include "GameModule/GameModuleRegistry.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

std::unique_ptr<CScene_GameSelect> CScene_GameSelect::Create()
{
    return std::unique_ptr<CScene_GameSelect>(new CScene_GameSelect());
}

bool CScene_GameSelect::OnEnter()
{
    CGameModuleRegistry::Instance().RegisterDefaults();
    m_strStatus.clear();
    return true;
}

void CScene_GameSelect::OnExit()
{
}

void CScene_GameSelect::OnUpdate(f32_t dt)
{
    (void)dt;
}

void CScene_GameSelect::OnLateUpdate(f32_t dt)
{
    (void)dt;
}

void CScene_GameSelect::OnRender()
{
}

void CScene_GameSelect::OnImGui()
{
    ImGui::SetNextWindowPos(ImVec2(80.f, 80.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(460.f, 300.f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Game Select"))
    {
        DrawProductButton(eGameProduct::WintersLOL);
        DrawProductButton(eGameProduct::WintersElden);
        DrawProductButton(eGameProduct::ClassServant);

        if (!m_strStatus.empty())
        {
            ImGui::Separator();
            ImGui::TextUnformatted(m_strStatus.c_str());
        }
    }

    ImGui::End();
}

GameLaunchConfig CScene_GameSelect::BuildLaunchConfig(eGameProduct product) const
{
    GameLaunchConfig config{};
    config.eProduct = product;
    config.bUseOnlineServices = false;
    config.bUseEditorTools = true;

    switch (product)
    {
    case eGameProduct::WintersLOL:
        config.strContentRoot = L"Data/LoL/";
        config.strServiceNamespace = L"/v1/lol/";
        config.strServerEndpoint = L"127.0.0.1:9000";
        break;
    case eGameProduct::WintersElden:
        config.strContentRoot = L"Data/Elden/";
        config.strServiceNamespace = L"/v1/elden/";
        break;
    case eGameProduct::ClassServant:
        config.strContentRoot = L"Data/ClassServant/";
        config.strServiceNamespace = L"/v1/class-servant/";
        break;
    default:
        break;
    }

    return config;
}

bool_t CScene_GameSelect::TryLaunch(eGameProduct product)
{
    CGameModuleRegistry& registry = CGameModuleRegistry::Instance();
    IGameModule* pModule = registry.Find(product);
    if (!pModule)
    {
        m_strStatus = "Module is not registered.";
        return false;
    }
    if (!pModule->IsAvailable())
    {
        m_strStatus = std::string(pModule->GetDisplayName()) + " is not available yet.";
        return false;
    }

    const GameLaunchConfig config = BuildLaunchConfig(product);
    if (!registry.Activate(product, config))
    {
        m_strStatus = std::string(pModule->GetDisplayName()) + " failed to initialize.";
        return false;
    }

    std::unique_ptr<IScene> pNextScene = pModule->CreateInitialScene();
    if (!pNextScene)
    {
        registry.ShutdownActiveModule();
        m_strStatus = std::string(pModule->GetDisplayName()) + " returned no initial scene.";
        return false;
    }

    CGameInstance::Get()->Change_Scene(
        static_cast<u32_t>(pModule->GetInitialSceneID()),
        std::move(pNextScene));
    return true;
}

void CScene_GameSelect::DrawProductButton(eGameProduct product)
{
    CGameModuleRegistry& registry = CGameModuleRegistry::Instance();
    IGameModule* pModule = registry.Find(product);
    const char* label = pModule ? pModule->GetDisplayName() : GetGameProductName(product);
    const bool_t bAvailable = pModule && pModule->IsAvailable();

    if (!bAvailable)
        ImGui::BeginDisabled();

    if (ImGui::Button(label, ImVec2(260.f, 44.f)))
    {
        if (TryLaunch(product))
        {
            if (!bAvailable)
                ImGui::EndDisabled();
            return;
        }
    }

    if (!bAvailable)
        ImGui::EndDisabled();
}
```

---

## 6. 기존 파일 수정 방향

### 6.1 `Client/Public/Defines.h`

수정 전:

```cpp
enum class eSceneID : int
{
    MainMenu,
    BanPick,
    Shop,
    MatchLoading,
    InGame,
    Editor,
    Result,
    SceneLoading,
    End
};
```

수정 후:

```cpp
enum class eSceneID : int
{
    GameSelect,
    MainMenu,
    BanPick,
    Shop,
    MatchLoading,
    InGame,
    Editor,
    Result,
    SceneLoading,
    End
};
```

주의:

- `End` 값이 하나 밀린다.
- `main.cpp` 의 `config.iNumScenes = static_cast<uint32_t>(eSceneID::End);` 는 그대로 유효하다.

### 6.2 `Client/Private/CGameApp.cpp`

수정 방향:

```text
LoL 전용 include 제거:
  Scene_InGame.h
  Scene_Loading.h
  Scene_BanPick.h
  Manager/Minion_Manager.h
  Manager/Structure_Manager.h
  Manager/Jungle_Manager.h
  Map/MapDataIO.h
  GameObject/ChampionDef.h

공통 include 추가:
  Scene/Scene_GameSelect.h
  GameModule/GameModuleRegistry.h
```

수정 후 핵심 형태:

```cpp
bool CGameApp::OnInit()
{
    CGameModuleRegistry::Instance().RegisterDefaults();

    CGameInstance::Get()->Change_Scene(
        static_cast<uint32_t>(eSceneID::GameSelect),
        CScene_GameSelect::Create());

    return true;
}

void CGameApp::OnShutdown()
{
    CGameInstance::Get()->Change_Scene(
        static_cast<uint32_t>(eSceneID::End),
        nullptr);

    CGameModuleRegistry::Instance().ShutdownActiveModule();
}
```

---

## 7. 검증 순서

```text
1. Client Debug 빌드
2. 실행 시 Game Select 창 표시
3. Winters League 클릭
4. 기존 Loading -> BanPick 진입
5. Local Custom Room 에서 챔피언 선택 후 Start Game
6. MatchLoading -> InGame 진입
7. 기존 direct local fallback selected champion + Sylas bot 화면 확인
8. 종료 시 manager shutdown 순서 이상 없음 확인
```

서버 로비 회귀:

```text
1. Server 실행
2. Client 1~3개 실행
3. Game Select 에서 Winters League 선택
4. 기존 BanPick 서버 로비 접속
5. Join/Pick/StartGame
6. local binding / snapshot / Q skill hook smoke 기존 결과 유지
```

---

## 8. 합격 기준

```text
[ ] CGameApp 에 LoL 전용 Manager 초기화가 남지 않는다.
[ ] GameSelect 에서 LOL 선택 시 기존 BanPick 플로우가 그대로 동작한다.
[ ] Elden / ClassServant 버튼은 보이지만 disabled 또는 not available 상태다.
[ ] Scene_InGame 에 product 분기가 추가되지 않는다.
[ ] Engine 코드 변경 없이 Client 쪽 module wrapper 만으로 1차 구조가 선다.
[ ] Client.vcxproj / filters 에 신규 파일이 모두 등록된다.
```

---

## 9. 다음 단계

1차가 통과하면 다음 단계는 `GameMode` 가 아니라 `ClientShell` 의 자리 만들기다.

```text
GS-2:
  Client/Public/ClientShell/
    ShellState.h
    AccountSession.h

GS-3:
  Scene_Login
  Scene_MainMenu
  Scene_Lobby

GS-4:
  Store/Profile/Matchmaking 화면을 Shell 아래로 배치

GS-5:
  SummonersRift / ARAM / PracticeTool 을 LOLGameModule 내부 GameMode 로 분리
```

`gameMode.json` 은 `LOLGameModule` 과 `Scene_BanPick -> InGame` 경계가 안정된 뒤 도입한다.

---

## 10. 보존해야 할 규칙

- Engine 은 `eChampion`, `Irelia`, `BanPick`, `LoL Skill` 을 알면 안 된다.
- `Scene_GameSelect` 는 제품 선택까지만 한다.
- `LOLGameModule` 은 LoL 초기화와 LoL 첫 scene 생성까지만 한다.
- `Scene_InGame` 은 당분간 `LOL InGame` 으로 간주한다.
- Elden 은 별도 scene / 별도 module 로만 들어온다.
- 공통 Account/Login/Store/Friend 는 `ClientShell` 로 들어오고, `Scene_InGame` 에 붙지 않는다.

