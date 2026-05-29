# 01. Module System — UE5-Style Dynamic Module Architecture

> **UE5 대응**: `FModuleManager`, `IModuleInterface`, `IMPLEMENT_MODULE`, `IMPLEMENT_GAME_MODULE`
> **현재 Winters**: DLL 1개 (`WintersEngine.dll`), 모듈 개념 없음, 전부 정적 링크
> **목표**: 엔진/게임/에디터 코드를 독립 모듈 DLL 로 분리, 동적 로드/언로드, 핫 리로드 기반

---

## 1. Architecture Overview

### 1.1 UE5 Module System 핵심 개념

```
UE5:
  FModuleManager::Get().LoadModule("MyModule")
  → DLL 로드 → InitializeModule() 호출 → IModuleInterface* 반환
  → 모듈 간 의존성 자동 해결
  → 에디터에서 핫 리로드 지원
```

### 1.2 Winters Module System 설계

```
CModuleManager::Get().LoadModule("WintersRenderer")
  → WintersRenderer.dll 로드 (LoadLibraryW)
  → CreateModule() export 호출 → IWintersModule* 반환
  → StartupModule() 호출
  → 모듈 레지스트리에 등록
  → 종료 시 ShutdownModule() → FreeLibrary
```

### 1.3 모듈 분류 (UE5 매핑)

| UE5 Module Type | Winters 대응 | 예시 |
|----------------|-------------|------|
| Runtime | `eModuleType::Runtime` | Core, RHI, Renderer, Animation |
| Editor | `eModuleType::Editor` | EditorFramework, ContentBrowser |
| Developer | `eModuleType::Developer` | Profiler, DebugDraw |
| Game | `eModuleType::Game` | LOLGame, EldenGame |

---

## 2. 파일 구조

```
Engine/
├── Public/Module/
│   ├── IWintersModule.h          ← 모듈 인터페이스 (UE5 IModuleInterface)
│   ├── ModuleManager.h           ← 모듈 매니저 (UE5 FModuleManager)
│   ├── ModuleDescriptor.h        ← 모듈 기술자 (이름, 타입, 의존성)
│   └── ModuleMacros.h            ← IMPLEMENT_MODULE 등 매크로
├── Private/Module/
│   ├── ModuleManager.cpp
│   └── ModuleRegistry.cpp
```

---

## 3. 코드 전문

### `Engine/Public/Module/IWintersModule.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include <string>
#include <vector>

/// UE5 IModuleInterface 대응
/// 모든 엔진/게임 모듈이 구현하는 인터페이스
class WINTERS_API IWintersModule
{
public:
    virtual ~IWintersModule() = default;

    /// 모듈 로드 직후 호출. 의존 모듈은 이미 로드 보장.
    virtual void StartupModule() {}

    /// 모듈 언로드 직전 호출.
    virtual void ShutdownModule() {}

    /// 에디터 핫 리로드 지원 여부
    virtual bool SupportsHotReload() const { return false; }

    /// 핫 리로드 직전: 상태 저장
    virtual void PreHotReload() {}

    /// 핫 리로드 직후: 상태 복원
    virtual void PostHotReload() {}

    /// 모듈 이름 (고유 식별자)
    virtual const char* GetModuleName() const = 0;

    /// 의존 모듈 이름 목록
    virtual std::vector<const char*> GetDependencies() const { return {}; }
};

/// DLL export 함수 타입 — 각 모듈 DLL 이 export
using CreateModuleFn = IWintersModule* (*)();
```

### `Engine/Public/Module/ModuleDescriptor.h`

```cpp
#pragma once

#include <string>
#include <vector>

/// 모듈 타입 분류
enum class eModuleType : uint8_t
{
    Runtime,    // 게임 실행에 필수
    Editor,     // 에디터에서만 로드
    Developer,  // 개발 빌드에서만 로드
    Game,       // 게임 모듈 (핫 리로드 대상)
};

/// 모듈 로드 Phase
enum class eModuleLoadPhase : uint8_t
{
    PreDefault,     // 다른 모듈보다 먼저 (Core, RHI)
    Default,        // 일반
    PostDefault,    // 다른 모듈 이후 (Editor)
};

/// 모듈 기술자 — 빌드 시스템이 생성하거나 매크로로 정의
struct ModuleDescriptor
{
    const char*              name = nullptr;
    eModuleType              type = eModuleType::Runtime;
    eModuleLoadPhase         loadPhase = eModuleLoadPhase::Default;
    std::vector<const char*> dependencies;
    const char*              dllPath = nullptr;  // nullptr = 정적 링크
};
```

### `Engine/Public/Module/ModuleMacros.h`

```cpp
#pragma once

#include "Module/IWintersModule.h"

/// UE5 IMPLEMENT_MODULE 대응
/// 모듈 DLL 에서 export 함수를 자동 생성
///
/// 사용법:
///   class CMyModule : public IWintersModule { ... };
///   IMPLEMENT_MODULE(CMyModule, "MyModule")
///
#define IMPLEMENT_MODULE(ModuleClass, ModuleName)                          \
    extern "C" __declspec(dllexport) IWintersModule* CreateModule()        \
    {                                                                      \
        static ModuleClass s_Instance;                                     \
        return &s_Instance;                                                \
    }                                                                      \
    extern "C" __declspec(dllexport) const char* GetModuleNameExport()     \
    {                                                                      \
        return ModuleName;                                                 \
    }

/// UE5 IMPLEMENT_GAME_MODULE 대응 — 핫 리로드 지원
#define IMPLEMENT_GAME_MODULE(ModuleClass, ModuleName)                     \
    extern "C" __declspec(dllexport) IWintersModule* CreateModule()        \
    {                                                                      \
        return new ModuleClass();                                          \
    }                                                                      \
    extern "C" __declspec(dllexport) void DestroyModule(IWintersModule* m) \
    {                                                                      \
        delete m;                                                          \
    }                                                                      \
    extern "C" __declspec(dllexport) const char* GetModuleNameExport()     \
    {                                                                      \
        return ModuleName;                                                 \
    }

/// 정적 링크 모듈 등록 (DLL 없이 엔진 내부 모듈)
#define IMPLEMENT_STATIC_MODULE(ModuleClass, ModuleName)                   \
    namespace {                                                            \
        struct ModuleClass##_StaticRegistrar {                             \
            ModuleClass##_StaticRegistrar() {                              \
                static ModuleClass s_Instance;                             \
                CModuleManager::Get().RegisterStaticModule(                \
                    ModuleName, &s_Instance);                              \
            }                                                              \
        } g_##ModuleClass##_Registrar;                                     \
    }
```

### `Engine/Public/Module/ModuleManager.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "Module/IWintersModule.h"
#include "Module/ModuleDescriptor.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>

#ifdef _WIN32
#include <Windows.h>
#endif

/// UE5 FModuleManager 대응
/// 모듈의 동적 로드/언로드/조회를 관리하는 싱글턴
class WINTERS_API CModuleManager final
{
public:
    static CModuleManager& Get();

    /// 모듈 로드 (DLL 동적 로드 + StartupModule 호출)
    /// @param moduleName 모듈 이름 (DLL 파일명에서 확장자 제외)
    /// @return 모듈 인터페이스 포인터 (실패 시 nullptr)
    IWintersModule* LoadModule(const char* moduleName);

    /// 모듈 언로드 (ShutdownModule + FreeLibrary)
    void UnloadModule(const char* moduleName);

    /// 모듈 조회 (이미 로드된 모듈)
    IWintersModule* GetModule(const char* moduleName) const;

    /// 타입 캐스트 포함 조회
    template<typename T>
    T* GetModuleChecked(const char* moduleName) const
    {
        auto* mod = GetModule(moduleName);
        return mod ? static_cast<T*>(mod) : nullptr;
    }

    /// 모듈 로드 여부 확인
    bool IsModuleLoaded(const char* moduleName) const;

    /// 정적 모듈 등록 (IMPLEMENT_STATIC_MODULE 에서 호출)
    void RegisterStaticModule(const char* name, IWintersModule* module);

    /// 모듈 기술자 등록 (빌드 시스템 또는 수동)
    void RegisterModuleDescriptor(const ModuleDescriptor& desc);

    /// 의존성 순서로 모든 등록된 모듈 로드
    void LoadAllModules();

    /// 역순으로 모든 모듈 언로드
    void UnloadAllModules();

    /// 게임 모듈 핫 리로드
    bool HotReloadModule(const char* moduleName);

    /// 로드된 모듈 목록
    std::vector<const char*> GetLoadedModuleNames() const;

    /// 모듈 이벤트 콜백
    using ModuleChangedCallback = std::function<void(const char* moduleName, bool loaded)>;
    void OnModuleChanged(ModuleChangedCallback callback);

private:
    CModuleManager() = default;
    ~CModuleManager();

    CModuleManager(const CModuleManager&) = delete;
    CModuleManager& operator=(const CModuleManager&) = delete;

    struct LoadedModule
    {
        IWintersModule*     pModule = nullptr;
        HMODULE             hDLL = nullptr;      // nullptr = 정적 모듈
        bool                bStatic = false;
        bool                bGameModule = false;  // 핫 리로드 대상
        ModuleDescriptor    descriptor;
    };

    /// DLL 경로 해석 (모듈 이름 → DLL 파일 경로)
    std::wstring ResolveDLLPath(const char* moduleName) const;

    /// 의존성 위상 정렬
    std::vector<const char*> TopologicalSort() const;

    std::unordered_map<std::string, LoadedModule>     m_LoadedModules;
    std::vector<ModuleDescriptor>                     m_Descriptors;
    std::vector<ModuleChangedCallback>                m_Callbacks;
    std::vector<std::string>                          m_LoadOrder;  // 로드 순서 기록 (언로드 시 역순)
};
```

### `Engine/Private/Module/ModuleManager.cpp`

```cpp
#include "Module/ModuleManager.h"

#include <algorithm>
#include <cassert>
#include <queue>
#include <sstream>

#ifdef _DEBUG
#include <crtdbg.h>
#endif

// OutputDebugStringA wrapper
#ifdef _DEBUG
#define MODULE_LOG(fmt, ...) do {                                  \
    char _buf[512];                                                \
    snprintf(_buf, sizeof(_buf), "[ModuleManager] " fmt "\n",      \
             ##__VA_ARGS__);                                       \
    OutputDebugStringA(_buf);                                       \
} while(0)
#else
#define MODULE_LOG(fmt, ...) ((void)0)
#endif

CModuleManager& CModuleManager::Get()
{
    static CModuleManager s_Instance;
    return s_Instance;
}

CModuleManager::~CModuleManager()
{
    UnloadAllModules();
}

IWintersModule* CModuleManager::LoadModule(const char* moduleName)
{
    // 이미 로드 확인
    auto it = m_LoadedModules.find(moduleName);
    if (it != m_LoadedModules.end())
        return it->second.pModule;

    // 의존 모듈 먼저 로드
    for (auto& desc : m_Descriptors)
    {
        if (strcmp(desc.name, moduleName) == 0)
        {
            for (auto* dep : desc.dependencies)
            {
                if (!IsModuleLoaded(dep))
                    LoadModule(dep);
            }
            break;
        }
    }

    // 정적 모듈 확인 (이미 RegisterStaticModule 으로 등록됨)
    it = m_LoadedModules.find(moduleName);
    if (it != m_LoadedModules.end())
    {
        if (!it->second.pModule)
            return nullptr;
        it->second.pModule->StartupModule();
        MODULE_LOG("Static module started: %s", moduleName);
        return it->second.pModule;
    }

    // DLL 동적 로드
    std::wstring dllPath = ResolveDLLPath(moduleName);
    HMODULE hDLL = LoadLibraryW(dllPath.c_str());
    if (!hDLL)
    {
        MODULE_LOG("Failed to load DLL: %s (error: %lu)",
                   moduleName, GetLastError());
        return nullptr;
    }

    // CreateModule export 조회
    auto createFn = reinterpret_cast<CreateModuleFn>(
        GetProcAddress(hDLL, "CreateModule"));
    if (!createFn)
    {
        MODULE_LOG("CreateModule export not found in: %s", moduleName);
        FreeLibrary(hDLL);
        return nullptr;
    }

    // 게임 모듈 여부 확인
    auto destroyFn = GetProcAddress(hDLL, "DestroyModule");

    IWintersModule* pModule = createFn();
    if (!pModule)
    {
        MODULE_LOG("CreateModule returned nullptr: %s", moduleName);
        FreeLibrary(hDLL);
        return nullptr;
    }

    LoadedModule loaded;
    loaded.pModule = pModule;
    loaded.hDLL = hDLL;
    loaded.bStatic = false;
    loaded.bGameModule = (destroyFn != nullptr);

    m_LoadedModules[moduleName] = loaded;
    m_LoadOrder.push_back(moduleName);

    pModule->StartupModule();
    MODULE_LOG("Module loaded: %s", moduleName);

    // 콜백 통지
    for (auto& cb : m_Callbacks)
        cb(moduleName, true);

    return pModule;
}

void CModuleManager::UnloadModule(const char* moduleName)
{
    auto it = m_LoadedModules.find(moduleName);
    if (it == m_LoadedModules.end())
        return;

    auto& loaded = it->second;

    if (loaded.pModule)
        loaded.pModule->ShutdownModule();

    // 게임 모듈은 DestroyModule 호출
    if (loaded.bGameModule && loaded.hDLL)
    {
        using DestroyFn = void(*)(IWintersModule*);
        auto destroyFn = reinterpret_cast<DestroyFn>(
            GetProcAddress(loaded.hDLL, "DestroyModule"));
        if (destroyFn)
            destroyFn(loaded.pModule);
    }

    if (loaded.hDLL)
    {
        FreeLibrary(loaded.hDLL);
        MODULE_LOG("Module unloaded: %s", moduleName);
    }

    // 콜백 통지
    for (auto& cb : m_Callbacks)
        cb(moduleName, false);

    m_LoadedModules.erase(it);
}

IWintersModule* CModuleManager::GetModule(const char* moduleName) const
{
    auto it = m_LoadedModules.find(moduleName);
    return (it != m_LoadedModules.end()) ? it->second.pModule : nullptr;
}

bool CModuleManager::IsModuleLoaded(const char* moduleName) const
{
    return m_LoadedModules.count(moduleName) > 0;
}

void CModuleManager::RegisterStaticModule(const char* name, IWintersModule* module)
{
    LoadedModule loaded;
    loaded.pModule = module;
    loaded.hDLL = nullptr;
    loaded.bStatic = true;
    loaded.bGameModule = false;
    m_LoadedModules[name] = loaded;
    m_LoadOrder.push_back(name);
    MODULE_LOG("Static module registered: %s", name);
}

void CModuleManager::RegisterModuleDescriptor(const ModuleDescriptor& desc)
{
    m_Descriptors.push_back(desc);
}

void CModuleManager::LoadAllModules()
{
    auto sorted = TopologicalSort();
    for (auto* name : sorted)
    {
        if (!IsModuleLoaded(name))
            LoadModule(name);
    }
}

void CModuleManager::UnloadAllModules()
{
    // 역순 언로드
    for (auto it = m_LoadOrder.rbegin(); it != m_LoadOrder.rend(); ++it)
    {
        auto modIt = m_LoadedModules.find(*it);
        if (modIt != m_LoadedModules.end())
        {
            if (modIt->second.pModule)
                modIt->second.pModule->ShutdownModule();

            if (modIt->second.bGameModule && modIt->second.hDLL)
            {
                using DestroyFn = void(*)(IWintersModule*);
                auto destroyFn = reinterpret_cast<DestroyFn>(
                    GetProcAddress(modIt->second.hDLL, "DestroyModule"));
                if (destroyFn)
                    destroyFn(modIt->second.pModule);
            }

            if (modIt->second.hDLL)
                FreeLibrary(modIt->second.hDLL);
        }
    }
    m_LoadedModules.clear();
    m_LoadOrder.clear();
}

bool CModuleManager::HotReloadModule(const char* moduleName)
{
    auto it = m_LoadedModules.find(moduleName);
    if (it == m_LoadedModules.end() || !it->second.bGameModule)
        return false;

    auto& loaded = it->second;

    // 1. PreHotReload
    if (loaded.pModule)
        loaded.pModule->PreHotReload();

    // 2. Unload old DLL
    if (loaded.bGameModule && loaded.hDLL)
    {
        using DestroyFn = void(*)(IWintersModule*);
        auto destroyFn = reinterpret_cast<DestroyFn>(
            GetProcAddress(loaded.hDLL, "DestroyModule"));
        if (destroyFn)
            destroyFn(loaded.pModule);
    }
    FreeLibrary(loaded.hDLL);

    // 3. Copy DLL to temp (avoid file lock)
    std::wstring origPath = ResolveDLLPath(moduleName);
    std::wstring tempPath = origPath + L".hotreload";
    CopyFileW(origPath.c_str(), tempPath.c_str(), FALSE);

    // 4. Load new DLL
    HMODULE hNewDLL = LoadLibraryW(tempPath.c_str());
    if (!hNewDLL)
    {
        MODULE_LOG("Hot reload failed: %s", moduleName);
        return false;
    }

    auto createFn = reinterpret_cast<CreateModuleFn>(
        GetProcAddress(hNewDLL, "CreateModule"));
    if (!createFn)
    {
        FreeLibrary(hNewDLL);
        return false;
    }

    IWintersModule* pNew = createFn();
    loaded.pModule = pNew;
    loaded.hDLL = hNewDLL;

    // 5. PostHotReload
    pNew->StartupModule();
    pNew->PostHotReload();

    MODULE_LOG("Hot reload success: %s", moduleName);

    for (auto& cb : m_Callbacks)
        cb(moduleName, true);

    return true;
}

std::vector<const char*> CModuleManager::GetLoadedModuleNames() const
{
    std::vector<const char*> names;
    for (auto& [name, _] : m_LoadedModules)
        names.push_back(name.c_str());
    return names;
}

void CModuleManager::OnModuleChanged(ModuleChangedCallback callback)
{
    m_Callbacks.push_back(std::move(callback));
}

std::wstring CModuleManager::ResolveDLLPath(const char* moduleName) const
{
    // exe 디렉토리 기준
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    std::wstring dir(exePath);
    auto pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        dir = dir.substr(0, pos + 1);

    // moduleName (ASCII) → wstring
    std::wstring wName(moduleName, moduleName + strlen(moduleName));

#ifdef _DEBUG
    return dir + wName + L"_d.dll";
#else
    return dir + wName + L".dll";
#endif
}

std::vector<const char*> CModuleManager::TopologicalSort() const
{
    // Kahn's algorithm
    std::unordered_map<std::string, int> inDegree;
    std::unordered_map<std::string, std::vector<std::string>> graph;

    for (auto& desc : m_Descriptors)
    {
        std::string name(desc.name);
        if (inDegree.find(name) == inDegree.end())
            inDegree[name] = 0;

        for (auto* dep : desc.dependencies)
        {
            std::string d(dep);
            graph[d].push_back(name);
            inDegree[name]++;
            if (inDegree.find(d) == inDegree.end())
                inDegree[d] = 0;
        }
    }

    std::queue<std::string> q;
    for (auto& [name, deg] : inDegree)
    {
        if (deg == 0)
            q.push(name);
    }

    std::vector<const char*> result;
    while (!q.empty())
    {
        auto cur = q.front();
        q.pop();

        // 기술자에서 원본 name 포인터 찾기
        for (auto& desc : m_Descriptors)
        {
            if (cur == desc.name)
            {
                result.push_back(desc.name);
                break;
            }
        }

        for (auto& next : graph[cur])
        {
            if (--inDegree[next] == 0)
                q.push(next);
        }
    }

    return result;
}
```

---

## 4. 사용 예시

### 4.1 엔진 코어 모듈 (정적)

```cpp
// Engine/Private/Core/CoreModule.cpp
#include "Module/ModuleMacros.h"

class CCoreModule : public IWintersModule
{
public:
    const char* GetModuleName() const override { return "WintersCore"; }

    void StartupModule() override
    {
        // 타이머, 입력 등 코어 시스템 초기화
    }

    void ShutdownModule() override
    {
        // 정리
    }
};

IMPLEMENT_STATIC_MODULE(CCoreModule, "WintersCore")
```

### 4.2 게임 모듈 (동적, 핫 리로드)

```cpp
// LOLGame/LOLGameModule.cpp
#include "Module/ModuleMacros.h"
#include "Module/IWintersModule.h"

class CLOLGameModule : public IWintersModule
{
public:
    const char* GetModuleName() const override { return "LOLGame"; }

    std::vector<const char*> GetDependencies() const override
    {
        return {"WintersCore", "WintersRenderer", "WintersNetwork"};
    }

    bool SupportsHotReload() const override { return true; }

    void StartupModule() override
    {
        // 챔피언 Registry 등록
        // 스킬 Hook 등록
        // GameMode 등록
    }
};

IMPLEMENT_GAME_MODULE(CLOLGameModule, "LOLGame")
```

### 4.3 엔진 초기화 시 모듈 로드

```cpp
// Engine/Private/Framework/CEngineApp.cpp
void CEngineApp::Initialize()
{
    auto& mm = CModuleManager::Get();

    // 코어 모듈 기술자 등록
    mm.RegisterModuleDescriptor({"WintersCore", eModuleType::Runtime,
                                  eModuleLoadPhase::PreDefault, {}});
    mm.RegisterModuleDescriptor({"WintersRHI", eModuleType::Runtime,
                                  eModuleLoadPhase::PreDefault, {"WintersCore"}});
    mm.RegisterModuleDescriptor({"WintersRenderer", eModuleType::Runtime,
                                  eModuleLoadPhase::Default, {"WintersRHI"}});
    mm.RegisterModuleDescriptor({"WintersNetwork", eModuleType::Runtime,
                                  eModuleLoadPhase::Default, {"WintersCore"}});

    // 에디터 빌드에서만
#ifdef WINTERS_EDITOR
    mm.RegisterModuleDescriptor({"WintersEditor", eModuleType::Editor,
                                  eModuleLoadPhase::PostDefault,
                                  {"WintersCore", "WintersRenderer"}});
#endif

    // 게임 모듈
    mm.RegisterModuleDescriptor({"LOLGame", eModuleType::Game,
                                  eModuleLoadPhase::PostDefault,
                                  {"WintersCore", "WintersRenderer", "WintersNetwork"}});

    // 위상 정렬 순서로 전체 로드
    mm.LoadAllModules();
}
```

---

## 5. Verification Checklist

```
[ ] CModuleManager::Get() 싱글턴 정상 동작
[ ] RegisterStaticModule 으로 CoreModule 등록 + StartupModule 호출
[ ] RegisterModuleDescriptor 3개 + LoadAllModules → 의존성 순서 로드 확인
[ ] 게임 모듈 DLL 동적 로드 + CreateModule export 호출 성공
[ ] UnloadAllModules → 역순 ShutdownModule + FreeLibrary
[ ] HotReloadModule: DLL 복사 → 재로드 → PostHotReload 콜백
[ ] 의존성 순환 감지 (TopologicalSort 결과 부족 시 에러)
[ ] 존재하지 않는 모듈 LoadModule → nullptr + 에러 로그
[ ] 이미 로드된 모듈 재호출 → 기존 포인터 반환 (중복 로드 X)
```

---

## 6. 기존 코드 마이그레이션

| 현재 | 마이그 후 |
|------|----------|
| `CGameInstance` 에 모든 매니저 포인터 | 각 모듈이 자기 매니저 소유 |
| `Engine_Defines.h` 에서 전역 include | 모듈별 PCH 분리 |
| 정적 링크 1개 DLL | Core + RHI + Renderer + Network + Game 분리 |
| `CEngineApp::Initialize()` 에 모든 초기화 | 각 모듈 `StartupModule()` 로 분산 |
