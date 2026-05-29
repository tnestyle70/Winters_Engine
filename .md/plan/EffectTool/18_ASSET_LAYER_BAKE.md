# 18. Asset Layer 박제 (`.wfx` JSON v1 + `CFxSystemAsset / CFxEmitterAsset / CFxScriptAsset`)

작성일: 2026-05-07
재박제일: 2026-05-07 (CLAUDE.md §8.2 본문 룰 적용 — stub 0 / 라인 번호 명시 / 추상 지시 0)
권위: 본 18 = 17 마스터 §15 부속 1번. EFX-0 manifest 작성 + EFX-1 JSON round-trip 진입 직전 박제.
의존: 17 §1 사전 결정 12 항목, 17 §0.1 codex 5/7 실측 정정 7 항목, 17 §3.1 자산 계층 매핑.
참조 코드:
- Niagara: `Engine/Plugins/FX/Niagara/Source/Niagara/{Public,Private}/{NiagaraSystem.h+.cpp, NiagaraEmitter.h+.cpp, NiagaraScript.h+.cpp, NiagaraScriptSourceBase.h}`
- Winters v1 보존: `Engine/Public/FX/{FxAsset.h, ParameterMap.h, ParticlePool.h, DeterministicRandom.h}`
- 16 EFX-1 박제: `.md/plan/EffectTool/16_EFX_PROGRESS_AND_NEXT_ACTIONS.md` 의 Step 0-4 / 0-5

목적:
- Layer 1 자산 5 클래스 박제 (`CFxSystemAsset / CFxEmitterAsset / CFxScriptAsset / CFxAssetRegistry / CFxJsonLoader+Saver`)
- `.wfx` JSON v1 schema 권위 박제 (포맷 / 버전 / round-trip 의미)
- v1 (`Engine/Public/FX/`) adapter 계층 박제 (EFX-0 의 v1 자산 보존)
- Manifest 자산 dump 명령 박제 (11 챔프 우선 hook)

박제 진입 전 8 단계 관문 적용:
- 관문 A (TBD 0, P-1+P-6): §1 사전 결정 5 항목, 모두 결정값
- 관문 B (PIMPL 미사용, P-2): 모든 클래스 헤더 + cpp 동시 박제, 본문 풀
- 관문 C (모든 path 동시 박제, P-3): 본 18 = Asset path 단독, Renderer/VM 은 부속 19~22
- 관문 D (Scene 직접 의존 0, P-4): 본 18 의 모든 클래스 = `Scene_*` 0 hit 강제
- 관문 E (bitmask 폭, P-7): 자산은 mask 미사용
- 관문 F (인용 의미, P-8): Niagara `NiagaraSystem.cpp:4` / `NiagaraScript.h` 인용 시 직접 인용 동반
- 관문 G (ECS 동시성, P-9): Asset 은 ECS 무관 (Layer 1)
- 관문 H (Owner Scope, P-10): `CFxAssetRegistry` = `CGameInstance` Tier-1, `CFxSystemAsset` 인스턴스 = Registry owned

---

## §0.1 5/7 codex 본문 룰 적용 (재박제)

본 18 v1 (5/7 첫 박제) 의 stub 위치 4 개 본문화:

```txt
1. FxJsonLoader.cpp 의 graph json / renderer json
   v1 = "raw string 보관 + 부속 22 가 파싱" 추상 표기
   v2 = std::string m_strSourceGraphJson 멤버 + raw renderer json blob vector 박제

2. FxJsonSaver.cpp 의 user_params 값
   v1 = "값은 부속 19 ParameterStore 박제 시점에 채움" 추상 표기
   v2 = ParameterMap 의 default value (entry.uByteSize 만큼 0) 직접 출력

3. FxV1Adapter.cpp 의 renderer 변환
   v1 = "본 18 박제 시점에는 raw 값 보관만" 추상 표기
   v2 = v1 의 renderType / texturePath / blendMode 직접 raw json blob 으로 변환

4. DumpFxManifest.cpp 의 v1 변환 본문
   v1 = "(실제 변환 본문 = EFX-0 코드 작업 시점에 채움)" 추상 표기
   v2 = BuildFxAsset(champion, hookId) 빌더 함수 11 종 박제. Yasuo Q_Straight 대표 본문 + 10 hook 은 동일 패턴 표기 (호출자 풀 본문)
```

라인 번호 = 모든 신규 헤더 / cpp 에 L1- 부터 본문 옆에 명시.

---

## §1 사전 결정 (TBD 0)

| 결정 항목 | 결정값 | 근거 |
|---|---|---|
| JSON parser | nlohmann/json (header-only, ThirdPartyLib 추가) | rapidjson 보다 API 친숙 + STL 직접 매핑. 17 §0.1.5 의 "수동 string parser 교체" 충족 |
| `.wfx` 파일 확장자 / 인코딩 | UTF-8 (BOM 없음) | git diff / merge 가능. 한글 자산명 대응 |
| Schema versioning | 최상위 `"version": 1` 필드 + `FxAssetVersion` enum (Loader 가 분기) | Niagara `FNiagaraCustomVersion` 패턴 직접 차용 |
| Asset handle | 기존 `Engine/Public/FX/FxAsset.h` 의 `FxAssetHandle = RHIHandle 기반 generation` 재사용 (16 박제) | P-17 회피 (typedef 일괄 변경 0). 단 `FxAssetHandle` 구조체는 v2 namespace 로 forward |
| Path/Loader 책임 | `CFxAssetRegistry` = path → handle. `CFxJsonLoader` = path → 트리 (Asset 객체 생성). 분리 | 캐싱 / refcount / hot reload 분리. Niagara `UAssetManager` + `FAssetData` 차용 |

---

## §2 신규 파일 트리

```txt
Engine/Public/FX/v2/Asset/
  FxAssetHandle.h              POD handle (v1 와 동일 RHIHandle 기반)
  FxAttributeBinding.h         POD (renderer ↔ DataSet 슬롯 binding)
  FxParameterMap.h             5 namespace (System/Engine/Emitter/Particles/User)
  FxSystemAsset.h              UNiagaraSystem 차용
  FxEmitterAsset.h             UNiagaraEmitter 차용
  FxScriptAsset.h              UNiagaraScript 차용 (eFxScriptUsage 8 종)
  FxAssetRegistry.h            CGameInstance Tier-1, path → handle 캐시
  FxJsonLoader.h               .wfx → 트리
  FxJsonSaver.h                트리 → .wfx
  FxV1Adapter.h                v1 FxAsset → v2 CFxSystemAsset 자동 변환

Engine/Private/FX/v2/Asset/
  FxSystemAsset.cpp
  FxEmitterAsset.cpp
  FxScriptAsset.cpp
  FxAssetRegistry.cpp
  FxJsonLoader.cpp
  FxJsonSaver.cpp
  FxV1Adapter.cpp

Engine/ThirdPartyLib/nlohmann_json/
  json.hpp                      header-only (3.11.x)

Tools/WintersAssetConverter/Commands/
  DumpFxManifest.cpp            11 챔프 우선 hook → .wfx dump
  BuildFxAsset_Yasuo.cpp        + Annie/Ashe/Fiora/Garen/Irelia/Jax/Kalista/Riven/Yone/Zed (11 빌더)
```

---

## §3 헤더 박제 (전문, L1- 라인 번호 명시)

### §3.1 `Engine/Public/FX/v2/Asset/FxAssetHandle.h` (L1-L23)

```cpp
// L1
#pragma once
// L2
// L3
#include "WintersAPI.h"
// L4
#include "WintersTypes.h"
// L5
// L6
namespace Winters::FX::v2
// L7
{
// L8
    struct FxAssetHandle
// L9
    {
// L10
        u32_t uIndex = 0;
// L11
        u32_t uGeneration = 0;
// L12
// L13
        bool_t IsValid() const { return uGeneration != 0; }
// L14
        bool_t operator==(const FxAssetHandle& other) const
// L15
        {
// L16
            return uIndex == other.uIndex && uGeneration == other.uGeneration;
// L17
        }
// L18
        bool_t operator!=(const FxAssetHandle& other) const { return !(*this == other); }
// L19
    };
// L20
// L21
    inline constexpr FxAssetHandle kInvalidFxAssetHandle{ 0, 0 };
// L22
}
// L23
```

P-17 회피: 기존 `Engine/Public/FX/FxAsset.h` 의 `FxAssetHandle` 동일 layout. v2 namespace 로 forward, ABI 동일.

### §3.2 `Engine/Public/FX/v2/Asset/FxAttributeBinding.h` (L1-L13)

```cpp
// L1
#pragma once
// L2
// L3
#include "WintersAPI.h"
// L4
#include "WintersTypes.h"
// L5
#include <string>
// L6
// L7
namespace Winters::FX::v2
// L8
{
// L9
    struct FxAttributeBinding
// L10
    {
// L11
        std::wstring strSlotName;
// L12
        std::wstring strParameterName;
// L13
        u32_t uDataSetSlotIdx = 0;
    };
}
```

### §3.3 `Engine/Public/FX/v2/Asset/FxParameterMap.h` (L1-L88)

```cpp
// L1
#pragma once
// L2
// L3
#include "WintersAPI.h"
// L4
#include "WintersTypes.h"
// L5
#include <string>
// L6
#include <unordered_map>
// L7
#include <vector>
// L8
// L9
namespace Winters::FX::v2
// L10
{
// L11
    enum class eFxNamespace : u8_t
// L12
    {
// L13
        System    = 0,
// L14
        Engine    = 1,
// L15
        Emitter   = 2,
// L16
        Particles = 3,
// L17
        User      = 4,
// L18
    };
// L19
// L20
    enum class eFxParameterType : u8_t
// L21
    {
// L22
        Float    = 0,
// L23
        Float2   = 1,
// L24
        Float3   = 2,
// L25
        Float4   = 3,
// L26
        Int      = 4,
// L27
        Bool     = 5,
// L28
        Curve    = 6,
// L29
        Texture  = 7,
// L30
        Spline   = 8,
// L31
    };
// L32
// L33
    inline u32_t GetFxParameterByteSize(eFxParameterType eType)
// L34
    {
// L35
        switch (eType)
// L36
        {
// L37
        case eFxParameterType::Float:    return 4;
// L38
        case eFxParameterType::Float2:   return 8;
// L39
        case eFxParameterType::Float3:   return 12;
// L40
        case eFxParameterType::Float4:   return 16;
// L41
        case eFxParameterType::Int:      return 4;
// L42
        case eFxParameterType::Bool:     return 1;
// L43
        case eFxParameterType::Curve:    return 4;     // handle index
// L44
        case eFxParameterType::Texture:  return 4;
// L45
        case eFxParameterType::Spline:   return 4;
// L46
        }
// L47
        return 4;
// L48
    }
// L49
// L50
    struct FxParameterID
// L51
    {
// L52
        eFxNamespace eNs = eFxNamespace::User;
// L53
        eFxParameterType eType = eFxParameterType::Float;
// L54
        u32_t uNameHash = 0;
// L55
    };
// L56
// L57
    struct FxParameterEntry
// L58
    {
// L59
        FxParameterID id{};
// L60
        std::wstring strName;
// L61
        u32_t uByteOffset = 0;
// L62
        u32_t uByteSize = 0;
// L63
    };
// L64
// L65
    class WINTERS_ENGINE FxParameterMap
// L66
    {
// L67
    public:
// L68
        FxParameterMap() = default;
// L69
// L70
        const std::vector<FxParameterEntry>& GetEntries() const { return m_vecEntries; }
// L71
        u32_t GetTotalByteSize() const { return m_uTotalByteSize; }
// L72
// L73
        void AddEntry(FxParameterEntry entry);
// L74
        const FxParameterEntry* FindByName(const std::wstring& strName) const;
// L75
        const FxParameterEntry* FindByHash(u32_t uHash) const;
// L76
        void Clear();
// L77
// L78
    private:
// L79
        std::vector<FxParameterEntry> m_vecEntries;
// L80
        std::unordered_map<u32_t, u32_t> m_mapHashToIdx;
// L81
        u32_t m_uTotalByteSize = 0;
// L82
    };
// L83
}
```

`FxParameterMap` 의 cpp 본문은 §4.0 박제 (`AddEntry / FindByName / FindByHash / Clear` 4 메서드).

### §3.4 `Engine/Public/FX/v2/Asset/FxScriptAsset.h` (L1-L60)

```cpp
// L1
#pragma once
// L2
// L3
#include "WintersAPI.h"
// L4
#include "WintersTypes.h"
// L5
#include "FX/v2/Asset/FxParameterMap.h"
// L6
#include <vector>
// L7
#include <memory>
// L8
#include <string>
// L9
// L10
namespace Winters::FX::v2
// L11
{
// L12
    class CFxGraph;     // 부속 22 박제 (Engine/Public/FX/v2/Compiler/FxGraph.h)
// L13
    struct FxVMExecutableData;     // 부속 21 박제 (Engine/Public/FX/v2/VM/FxVMExecutableData.h)
// L14
// L15
    enum class eFxScriptUsage : u8_t
// L16
    {
// L17
        SystemSpawn      = 0,
// L18
        SystemUpdate     = 1,
// L19
        EmitterSpawn     = 2,
// L20
        EmitterUpdate    = 3,
// L21
        ParticleSpawn    = 4,
// L22
        ParticleUpdate   = 5,
// L23
        SimulationStage  = 6,
// L24
        Module           = 7,
// L25
    };
// L26
// L27
    enum class eFxCompileStatus : u8_t
// L28
    {
// L29
        NotCompiled = 0,
// L30
        InFlight    = 1,
// L31
        Succeeded   = 2,
// L32
        Failed      = 3,
// L33
    };
// L34
// L35
    class WINTERS_ENGINE CFxScriptAsset
// L36
    {
// L37
    public:
// L38
        ~CFxScriptAsset();
// L39
        CFxScriptAsset(const CFxScriptAsset&) = delete;
// L40
        CFxScriptAsset& operator=(const CFxScriptAsset&) = delete;
// L41
// L42
        static std::unique_ptr<CFxScriptAsset> Create(eFxScriptUsage usage);
// L43
// L44
        eFxScriptUsage GetUsage() const { return m_eUsage; }
// L45
        eFxCompileStatus GetCompileStatus() const { return m_eStatus; }
// L46
// L47
        CFxGraph* GetSourceGraph() const { return m_pSourceGraph.get(); }
// L48
        void SetSourceGraph(std::unique_ptr<CFxGraph> pGraph) { m_pSourceGraph = std::move(pGraph); }
// L49
// L50
        const std::string& GetSourceGraphJsonRaw() const { return m_strSourceGraphJsonRaw; }
// L51
        void SetSourceGraphJsonRaw(std::string strJson) { m_strSourceGraphJsonRaw = std::move(strJson); }
// L52
// L53
        const FxVMExecutableData* GetVMData() const { return m_pVMData.get(); }
// L54
        void SetVMData(std::unique_ptr<FxVMExecutableData> pData) { m_pVMData = std::move(pData); }
// L55
// L56
        const std::vector<u8_t>& GetHlslBytes() const { return m_vecHlslBytes; }
// L57
        void SetHlslBytes(std::vector<u8_t> bytes) { m_vecHlslBytes = std::move(bytes); }
// L58
// L59
        void SetCompileStatus(eFxCompileStatus eStatus) { m_eStatus = eStatus; }
// L60
        u32_t GetCompileVersion() const { return m_uCompileVersion; }
        void SetCompileVersion(u32_t uVersion) { m_uCompileVersion = uVersion; }

    private:
        CFxScriptAsset() = default;

        eFxScriptUsage m_eUsage = eFxScriptUsage::Module;
        eFxCompileStatus m_eStatus = eFxCompileStatus::NotCompiled;

        std::unique_ptr<CFxGraph> m_pSourceGraph;     // 부속 22 박제 후 채움
        std::string m_strSourceGraphJsonRaw;           // JSON loader 가 raw 보관, 부속 22 가 parse 후 SetSourceGraph 호출
        std::unique_ptr<FxVMExecutableData> m_pVMData; // 부속 22 Compiler 의 산출
        std::vector<u8_t> m_vecHlslBytes;              // 부속 22 Compiler 의 산출
        u32_t m_uCompileVersion = 0;
    };
}
```

P-1 본문 룰 회피: `m_strSourceGraphJsonRaw` 멤버 = JSON loader 가 graph json string 을 그대로 보관. 부속 22 박제 시 string → CFxGraph 변환 method (`CFxJsonGraphLoader::Parse`) 가 추가되며, 그 메서드가 `SetSourceGraph(parsed)` 호출. 본 18 박제 = string 보관 동작 자체는 풀 본문.

`CFxGraph` / `FxVMExecutableData` = forward declare. 본문 부속 22 / 21 박제.

### §3.5 `Engine/Public/FX/v2/Asset/FxEmitterAsset.h` (L1-L78)

```cpp
// L1
#pragma once
// L2
// L3
#include "WintersAPI.h"
// L4
#include "WintersTypes.h"
// L5
#include "FX/v2/Asset/FxAttributeBinding.h"
// L6
#include <vector>
// L7
#include <memory>
// L8
#include <string>
// L9
// L10
namespace Winters::FX::v2
// L11
{
// L12
    class CFxScriptAsset;
// L13
    struct FxRendererProperties;     // 부속 20 박제 (Engine/Public/FX/v2/Renderer/FxRendererProperties.h)
// L14
// L15
    enum class eFxExecMode : u8_t
// L16
    {
// L17
        CPU = 0,
// L18
        GPU = 1,
// L19
    };
// L20
// L21
    class WINTERS_ENGINE CFxEmitterAsset
// L22
    {
// L23
    public:
// L24
        ~CFxEmitterAsset();
// L25
        CFxEmitterAsset(const CFxEmitterAsset&) = delete;
// L26
        CFxEmitterAsset& operator=(const CFxEmitterAsset&) = delete;
// L27
// L28
        static std::unique_ptr<CFxEmitterAsset> Create(const std::wstring& strName);
// L29
// L30
        const std::wstring& GetName() const { return m_strName; }
// L31
        eFxExecMode GetExecMode() const { return m_eExecMode; }
// L32
        u32_t GetMaxParticles() const { return m_uMaxParticles; }
// L33
// L34
        void SetExecMode(eFxExecMode mode) { m_eExecMode = mode; }
// L35
        void SetMaxParticles(u32_t uMax) { m_uMaxParticles = uMax; }
// L36
// L37
        CFxScriptAsset* GetEmitterSpawnScript() const { return m_pEmitterSpawnScript.get(); }
// L38
        CFxScriptAsset* GetEmitterUpdateScript() const { return m_pEmitterUpdateScript.get(); }
// L39
        CFxScriptAsset* GetParticleSpawnScript() const { return m_pParticleSpawnScript.get(); }
// L40
        CFxScriptAsset* GetParticleUpdateScript() const { return m_pParticleUpdateScript.get(); }
// L41
// L42
        void SetEmitterSpawnScript(std::unique_ptr<CFxScriptAsset> p);
// L43
        void SetEmitterUpdateScript(std::unique_ptr<CFxScriptAsset> p);
// L44
        void SetParticleSpawnScript(std::unique_ptr<CFxScriptAsset> p);
// L45
        void SetParticleUpdateScript(std::unique_ptr<CFxScriptAsset> p);
// L46
// L47
        const std::vector<std::unique_ptr<FxRendererProperties>>& GetRenderers() const { return m_vecRenderers; }
// L48
        void AddRenderer(std::unique_ptr<FxRendererProperties> pRenderer);
// L49
        void ClearRenderers() { m_vecRenderers.clear(); }
// L50
// L51
        const std::vector<std::string>& GetRendererJsonBlobs() const { return m_vecRendererJsonBlobs; }
// L52
        void AddRendererJsonBlob(std::string strJson) { m_vecRendererJsonBlobs.emplace_back(std::move(strJson)); }
// L53
        void ClearRendererJsonBlobs() { m_vecRendererJsonBlobs.clear(); }
// L54
// L55
        const std::vector<FxAttributeBinding>& GetAttributeBindings() const { return m_vecBindings; }
// L56
        void AddAttributeBinding(FxAttributeBinding binding) { m_vecBindings.push_back(binding); }
// L57
// L58
    private:
// L59
        CFxEmitterAsset() = default;
// L60
// L61
        std::wstring m_strName;
// L62
        eFxExecMode m_eExecMode = eFxExecMode::CPU;
// L63
        u32_t m_uMaxParticles = 4096;
// L64
// L65
        std::unique_ptr<CFxScriptAsset> m_pEmitterSpawnScript;
// L66
        std::unique_ptr<CFxScriptAsset> m_pEmitterUpdateScript;
// L67
        std::unique_ptr<CFxScriptAsset> m_pParticleSpawnScript;
// L68
        std::unique_ptr<CFxScriptAsset> m_pParticleUpdateScript;
// L69
// L70
        std::vector<std::unique_ptr<FxRendererProperties>> m_vecRenderers;     // 부속 20 박제 후 빌드
// L71
        std::vector<std::string> m_vecRendererJsonBlobs;                       // JSON loader 가 raw 보관, 부속 20 박제 후 deserialize → m_vecRenderers 채움
// L72
        std::vector<FxAttributeBinding> m_vecBindings;
// L73
    };
// L74
}
```

P-1 본문 룰 회피: `m_vecRendererJsonBlobs` 멤버 = JSON loader 가 renderer json 을 raw 보관. 부속 20 박제 시 type 별 deserialize method 추가되며, 본 18 박제 = raw blob 보관 동작 자체는 풀 본문.

### §3.6 `Engine/Public/FX/v2/Asset/FxSystemAsset.h` (L1-L52)

```cpp
// L1
#pragma once
// L2
// L3
#include "WintersAPI.h"
// L4
#include "WintersTypes.h"
// L5
#include "FX/v2/Asset/FxParameterMap.h"
// L6
#include <vector>
// L7
#include <memory>
// L8
#include <string>
// L9
// L10
namespace Winters::FX::v2
// L11
{
// L12
    class CFxScriptAsset;
// L13
    class CFxEmitterAsset;
// L14
// L15
    class WINTERS_ENGINE CFxSystemAsset
// L16
    {
// L17
    public:
// L18
        ~CFxSystemAsset();
// L19
        CFxSystemAsset(const CFxSystemAsset&) = delete;
// L20
        CFxSystemAsset& operator=(const CFxSystemAsset&) = delete;
// L21
// L22
        static std::unique_ptr<CFxSystemAsset> Create(const std::wstring& strName);
// L23
// L24
        const std::wstring& GetName() const { return m_strName; }
// L25
// L26
        const std::vector<std::unique_ptr<CFxEmitterAsset>>& GetEmitters() const { return m_vecEmitters; }
// L27
        void AddEmitter(std::unique_ptr<CFxEmitterAsset> pEmitter);
// L28
        void RemoveEmitter(u32_t uIdx);
// L29
// L30
        CFxScriptAsset* GetSystemSpawnScript() const { return m_pSystemSpawnScript.get(); }
// L31
        CFxScriptAsset* GetSystemUpdateScript() const { return m_pSystemUpdateScript.get(); }
// L32
        void SetSystemSpawnScript(std::unique_ptr<CFxScriptAsset> p);
// L33
        void SetSystemUpdateScript(std::unique_ptr<CFxScriptAsset> p);
// L34
// L35
        FxParameterMap& GetUserParameterMap() { return m_UserParams; }
// L36
        const FxParameterMap& GetUserParameterMap() const { return m_UserParams; }
// L37
// L38
        u32_t GetSchemaVersion() const { return m_uSchemaVersion; }
// L39
        void SetSchemaVersion(u32_t uVer) { m_uSchemaVersion = uVer; }
// L40
// L41
    private:
// L42
        CFxSystemAsset() = default;
// L43
// L44
        std::wstring m_strName;
// L45
        u32_t m_uSchemaVersion = 1;
// L46
// L47
        std::vector<std::unique_ptr<CFxEmitterAsset>> m_vecEmitters;
// L48
        std::unique_ptr<CFxScriptAsset> m_pSystemSpawnScript;
// L49
        std::unique_ptr<CFxScriptAsset> m_pSystemUpdateScript;
// L50
// L51
        FxParameterMap m_UserParams;
// L52
    };
}
```

박제 함정 (CLAUDE.md §5.2 dllexport + unique_ptr) 회피: `CFxSystemAsset` = `WINTERS_ENGINE` export, copy ctor / assign 명시 `= delete`, vector / unique_ptr 멤버는 OK (move-only).

### §3.7 `Engine/Public/FX/v2/Asset/FxAssetRegistry.h` (L1-L48)

```cpp
// L1
#pragma once
// L2
// L3
#include "WintersAPI.h"
// L4
#include "WintersTypes.h"
// L5
#include "FX/v2/Asset/FxAssetHandle.h"
// L6
#include <vector>
// L7
#include <memory>
// L8
#include <unordered_map>
// L9
#include <string>
// L10
// L11
namespace Winters::FX::v2
// L12
{
// L13
    class CFxSystemAsset;
// L14
// L15
    class WINTERS_ENGINE CFxAssetRegistry
// L16
    {
// L17
    public:
// L18
        ~CFxAssetRegistry();
// L19
        CFxAssetRegistry(const CFxAssetRegistry&) = delete;
// L20
        CFxAssetRegistry& operator=(const CFxAssetRegistry&) = delete;
// L21
// L22
        static std::unique_ptr<CFxAssetRegistry> Create();
// L23
// L24
        FxAssetHandle Register(std::unique_ptr<CFxSystemAsset> pAsset, const std::wstring& strPath);
// L25
        FxAssetHandle Find(const std::wstring& strPath) const;
// L26
        CFxSystemAsset* Resolve(FxAssetHandle handle) const;
// L27
// L28
        FxAssetHandle LoadFromFile(const std::wstring& strPath);
// L29
        bool ReloadFromFile(FxAssetHandle handle);
// L30
        bool Unload(FxAssetHandle handle);
// L31
// L32
        u32_t GetCount() const;
// L33
        std::vector<FxAssetHandle> GetAllHandles() const;
// L34
// L35
    private:
// L36
        CFxAssetRegistry() = default;
// L37
// L38
        struct Slot
// L39
        {
// L40
            std::unique_ptr<CFxSystemAsset> pAsset;
// L41
            std::wstring strPath;
// L42
            u32_t uGeneration = 0;
// L43
            bool_t bAlive = false;
// L44
        };
// L45
// L46
        std::vector<Slot> m_vecSlots;
// L47
        std::unordered_map<std::wstring, u32_t> m_mapPathToIdx;
// L48
        std::vector<u32_t> m_vecFreeIdx;
    };
}
```

### §3.8 `Engine/Public/FX/v2/Asset/FxJsonLoader.h` (L1-L25)

```cpp
// L1
#pragma once
// L2
// L3
#include "WintersAPI.h"
// L4
#include "WintersTypes.h"
// L5
#include <memory>
// L6
#include <string>
// L7
#include <vector>
// L8
// L9
namespace Winters::FX::v2
// L10
{
// L11
    class CFxSystemAsset;
// L12
// L13
    struct FxLoadResult
// L14
    {
// L15
        std::unique_ptr<CFxSystemAsset> pAsset;
// L16
        std::vector<std::wstring> vecErrors;
// L17
        std::vector<std::wstring> vecWarnings;
// L18
        bool_t bSucceeded = false;
// L19
    };
// L20
// L21
    class WINTERS_ENGINE CFxJsonLoader
// L22
    {
// L23
    public:
// L24
        static FxLoadResult LoadFromFile(const std::wstring& strPath);
// L25
        static FxLoadResult LoadFromString(const std::string& strJsonUtf8);
    };
}
```

### §3.9 `Engine/Public/FX/v2/Asset/FxJsonSaver.h` (L1-L24)

```cpp
// L1
#pragma once
// L2
// L3
#include "WintersAPI.h"
// L4
#include "WintersTypes.h"
// L5
#include <string>
// L6
#include <vector>
// L7
// L8
namespace Winters::FX::v2
// L9
{
// L10
    class CFxSystemAsset;
// L11
// L12
    struct FxSaveResult
// L13
    {
// L14
        std::vector<std::wstring> vecErrors;
// L15
        bool_t bSucceeded = false;
// L16
    };
// L17
// L18
    class WINTERS_ENGINE CFxJsonSaver
// L19
    {
// L20
    public:
// L21
        static FxSaveResult SaveToFile(const CFxSystemAsset* pAsset, const std::wstring& strPath);
// L22
        static std::string SaveToString(const CFxSystemAsset* pAsset);
// L23
    };
// L24
}
```

### §3.10 `Engine/Public/FX/v2/Asset/FxV1Adapter.h` (L1-L18)

```cpp
// L1
#pragma once
// L2
// L3
#include "WintersAPI.h"
// L4
#include "WintersTypes.h"
// L5
#include <memory>
// L6
// L7
class FxAsset;     // v1 forward (Engine/Public/FX/FxAsset.h)
// L8
// L9
namespace Winters::FX::v2
// L10
{
// L11
    class CFxSystemAsset;
// L12
// L13
    class WINTERS_ENGINE CFxV1Adapter
// L14
    {
// L15
    public:
// L16
        static std::unique_ptr<CFxSystemAsset> ConvertFromV1(const FxAsset& v1Asset);
// L17
        static bool_t ConvertToV1(const CFxSystemAsset* pV2, FxAsset& outV1);
// L18
    };
}
```

EFX-0 의 핵심: 기존 v1 자산을 v2 로 변환하되 v1 객체 자체는 보존 (Client 빌드 깨짐 방지).

---

## §4 cpp 본문 박제 (전문, L1- 라인 번호 명시, stub 0)

### §4.0 `Engine/Private/FX/v2/Asset/FxParameterMap.cpp` (L1-L43)

```cpp
// L1
#include "FX/v2/Asset/FxParameterMap.h"
// L2
// L3
namespace Winters::FX::v2
// L4
{
// L5
    void FxParameterMap::AddEntry(FxParameterEntry entry)
// L6
    {
// L7
        if (entry.uByteSize == 0)
// L8
            entry.uByteSize = GetFxParameterByteSize(entry.id.eType);
// L9
// L10
        entry.uByteOffset = m_uTotalByteSize;
// L11
        m_uTotalByteSize += entry.uByteSize;
// L12
// L13
        const u32_t uIdx = static_cast<u32_t>(m_vecEntries.size());
// L14
        m_mapHashToIdx.emplace(entry.id.uNameHash, uIdx);
// L15
        m_vecEntries.emplace_back(std::move(entry));
// L16
    }
// L17
// L18
    const FxParameterEntry* FxParameterMap::FindByName(const std::wstring& strName) const
// L19
    {
// L20
        for (const FxParameterEntry& e : m_vecEntries)
// L21
            if (e.strName == strName) return &e;
// L22
        return nullptr;
// L23
    }
// L24
// L25
    const FxParameterEntry* FxParameterMap::FindByHash(u32_t uHash) const
// L26
    {
// L27
        auto it = m_mapHashToIdx.find(uHash);
// L28
        if (it == m_mapHashToIdx.end()) return nullptr;
// L29
        if (it->second >= m_vecEntries.size()) return nullptr;
// L30
        return &m_vecEntries[it->second];
// L31
    }
// L32
// L33
    void FxParameterMap::Clear()
// L34
    {
// L35
        m_vecEntries.clear();
// L36
        m_mapHashToIdx.clear();
// L37
        m_uTotalByteSize = 0;
// L38
    }
// L39
}
```

### §4.1 `Engine/Private/FX/v2/Asset/FxScriptAsset.cpp` (L1-L13)

```cpp
// L1
#include "FX/v2/Asset/FxScriptAsset.h"
// L2
// L3
namespace Winters::FX::v2
// L4
{
// L5
    std::unique_ptr<CFxScriptAsset> CFxScriptAsset::Create(eFxScriptUsage usage)
// L6
    {
// L7
        auto p = std::unique_ptr<CFxScriptAsset>(new CFxScriptAsset());
// L8
        p->m_eUsage = usage;
// L9
        return p;
// L10
    }
// L11
// L12
    CFxScriptAsset::~CFxScriptAsset() = default;
// L13
}
```

### §4.2 `Engine/Private/FX/v2/Asset/FxEmitterAsset.cpp` (L1-L36)

```cpp
// L1
#include "FX/v2/Asset/FxEmitterAsset.h"
// L2
#include "FX/v2/Asset/FxScriptAsset.h"
// L3
#include "FX/v2/Renderer/FxRendererProperties.h"
// L4
// L5
namespace Winters::FX::v2
// L6
{
// L7
    std::unique_ptr<CFxEmitterAsset> CFxEmitterAsset::Create(const std::wstring& strName)
// L8
    {
// L9
        auto p = std::unique_ptr<CFxEmitterAsset>(new CFxEmitterAsset());
// L10
        p->m_strName = strName;
// L11
        return p;
// L12
    }
// L13
// L14
    CFxEmitterAsset::~CFxEmitterAsset() = default;
// L15
// L16
    void CFxEmitterAsset::SetEmitterSpawnScript(std::unique_ptr<CFxScriptAsset> p)
// L17
    {
// L18
        m_pEmitterSpawnScript = std::move(p);
// L19
    }
// L20
// L21
    void CFxEmitterAsset::SetEmitterUpdateScript(std::unique_ptr<CFxScriptAsset> p)
// L22
    {
// L23
        m_pEmitterUpdateScript = std::move(p);
// L24
    }
// L25
// L26
    void CFxEmitterAsset::SetParticleSpawnScript(std::unique_ptr<CFxScriptAsset> p)
// L27
    {
// L28
        m_pParticleSpawnScript = std::move(p);
// L29
    }
// L30
// L31
    void CFxEmitterAsset::SetParticleUpdateScript(std::unique_ptr<CFxScriptAsset> p)
// L32
    {
// L33
        m_pParticleUpdateScript = std::move(p);
// L34
    }
// L35
// L36
    void CFxEmitterAsset::AddRenderer(std::unique_ptr<FxRendererProperties> pRenderer)
    {
        if (pRenderer) m_vecRenderers.emplace_back(std::move(pRenderer));
    }
}
```

### §4.3 `Engine/Private/FX/v2/Asset/FxSystemAsset.cpp` (L1-L34)

```cpp
// L1
#include "FX/v2/Asset/FxSystemAsset.h"
// L2
#include "FX/v2/Asset/FxScriptAsset.h"
// L3
#include "FX/v2/Asset/FxEmitterAsset.h"
// L4
// L5
namespace Winters::FX::v2
// L6
{
// L7
    std::unique_ptr<CFxSystemAsset> CFxSystemAsset::Create(const std::wstring& strName)
// L8
    {
// L9
        auto p = std::unique_ptr<CFxSystemAsset>(new CFxSystemAsset());
// L10
        p->m_strName = strName;
// L11
        return p;
// L12
    }
// L13
// L14
    CFxSystemAsset::~CFxSystemAsset() = default;
// L15
// L16
    void CFxSystemAsset::AddEmitter(std::unique_ptr<CFxEmitterAsset> pEmitter)
// L17
    {
// L18
        if (pEmitter) m_vecEmitters.emplace_back(std::move(pEmitter));
// L19
    }
// L20
// L21
    void CFxSystemAsset::RemoveEmitter(u32_t uIdx)
// L22
    {
// L23
        if (uIdx < m_vecEmitters.size())
// L24
            m_vecEmitters.erase(m_vecEmitters.begin() + uIdx);
// L25
    }
// L26
// L27
    void CFxSystemAsset::SetSystemSpawnScript(std::unique_ptr<CFxScriptAsset> p)
// L28
    {
// L29
        m_pSystemSpawnScript = std::move(p);
// L30
    }
// L31
// L32
    void CFxSystemAsset::SetSystemUpdateScript(std::unique_ptr<CFxScriptAsset> p)
// L33
    {
// L34
        m_pSystemUpdateScript = std::move(p);
    }
}
```

### §4.4 `Engine/Private/FX/v2/Asset/FxAssetRegistry.cpp` (L1-L120)

```cpp
// L1
#include "FX/v2/Asset/FxAssetRegistry.h"
// L2
#include "FX/v2/Asset/FxSystemAsset.h"
// L3
#include "FX/v2/Asset/FxJsonLoader.h"
// L4
// L5
namespace Winters::FX::v2
// L6
{
// L7
    std::unique_ptr<CFxAssetRegistry> CFxAssetRegistry::Create()
// L8
    {
// L9
        return std::unique_ptr<CFxAssetRegistry>(new CFxAssetRegistry());
// L10
    }
// L11
// L12
    CFxAssetRegistry::~CFxAssetRegistry() = default;
// L13
// L14
    FxAssetHandle CFxAssetRegistry::Register(std::unique_ptr<CFxSystemAsset> pAsset, const std::wstring& strPath)
// L15
    {
// L16
        if (!pAsset) return kInvalidFxAssetHandle;
// L17
// L18
        auto it = m_mapPathToIdx.find(strPath);
// L19
        if (it != m_mapPathToIdx.end())
// L20
        {
// L21
            const u32_t uIdx = it->second;
// L22
            m_vecSlots[uIdx].pAsset = std::move(pAsset);
// L23
            ++m_vecSlots[uIdx].uGeneration;
// L24
            if (m_vecSlots[uIdx].uGeneration == 0) m_vecSlots[uIdx].uGeneration = 1;
// L25
            m_vecSlots[uIdx].bAlive = true;
// L26
            return { uIdx, m_vecSlots[uIdx].uGeneration };
// L27
        }
// L28
// L29
        u32_t uIdx;
// L30
        if (!m_vecFreeIdx.empty())
// L31
        {
// L32
            uIdx = m_vecFreeIdx.back();
// L33
            m_vecFreeIdx.pop_back();
// L34
        }
// L35
        else
// L36
        {
// L37
            uIdx = static_cast<u32_t>(m_vecSlots.size());
// L38
            m_vecSlots.emplace_back();
// L39
        }
// L40
// L41
        Slot& slot = m_vecSlots[uIdx];
// L42
        slot.pAsset = std::move(pAsset);
// L43
        slot.strPath = strPath;
// L44
        ++slot.uGeneration;
// L45
        if (slot.uGeneration == 0) slot.uGeneration = 1;
// L46
        slot.bAlive = true;
// L47
// L48
        if (!strPath.empty()) m_mapPathToIdx.emplace(strPath, uIdx);
// L49
// L50
        return { uIdx, slot.uGeneration };
// L51
    }
// L52
// L53
    FxAssetHandle CFxAssetRegistry::Find(const std::wstring& strPath) const
// L54
    {
// L55
        auto it = m_mapPathToIdx.find(strPath);
// L56
        if (it == m_mapPathToIdx.end()) return kInvalidFxAssetHandle;
// L57
        const u32_t uIdx = it->second;
// L58
        if (uIdx >= m_vecSlots.size()) return kInvalidFxAssetHandle;
// L59
        const Slot& slot = m_vecSlots[uIdx];
// L60
        if (!slot.bAlive) return kInvalidFxAssetHandle;
// L61
        return { uIdx, slot.uGeneration };
// L62
    }
// L63
// L64
    CFxSystemAsset* CFxAssetRegistry::Resolve(FxAssetHandle handle) const
// L65
    {
// L66
        if (!handle.IsValid()) return nullptr;
// L67
        if (handle.uIndex >= m_vecSlots.size()) return nullptr;
// L68
        const Slot& slot = m_vecSlots[handle.uIndex];
// L69
        if (!slot.bAlive) return nullptr;
// L70
        if (slot.uGeneration != handle.uGeneration) return nullptr;
// L71
        return slot.pAsset.get();
// L72
    }
// L73
// L74
    FxAssetHandle CFxAssetRegistry::LoadFromFile(const std::wstring& strPath)
// L75
    {
// L76
        FxLoadResult result = CFxJsonLoader::LoadFromFile(strPath);
// L77
        if (!result.bSucceeded || !result.pAsset) return kInvalidFxAssetHandle;
// L78
        return Register(std::move(result.pAsset), strPath);
// L79
    }
// L80
// L81
    bool CFxAssetRegistry::ReloadFromFile(FxAssetHandle handle)
// L82
    {
// L83
        if (!handle.IsValid() || handle.uIndex >= m_vecSlots.size()) return false;
// L84
        Slot& slot = m_vecSlots[handle.uIndex];
// L85
        if (!slot.bAlive || slot.uGeneration != handle.uGeneration) return false;
// L86
// L87
        FxLoadResult result = CFxJsonLoader::LoadFromFile(slot.strPath);
// L88
        if (!result.bSucceeded || !result.pAsset) return false;
// L89
// L90
        slot.pAsset = std::move(result.pAsset);
// L91
        ++slot.uGeneration;
// L92
        if (slot.uGeneration == 0) slot.uGeneration = 1;
// L93
        return true;
// L94
    }
// L95
// L96
    bool CFxAssetRegistry::Unload(FxAssetHandle handle)
// L97
    {
// L98
        if (!handle.IsValid() || handle.uIndex >= m_vecSlots.size()) return false;
// L99
        Slot& slot = m_vecSlots[handle.uIndex];
// L100
        if (!slot.bAlive || slot.uGeneration != handle.uGeneration) return false;
// L101
// L102
        if (!slot.strPath.empty()) m_mapPathToIdx.erase(slot.strPath);
// L103
        slot.pAsset.reset();
// L104
        slot.strPath.clear();
// L105
        slot.bAlive = false;
// L106
        m_vecFreeIdx.push_back(handle.uIndex);
// L107
        return true;
// L108
    }
// L109
// L110
    u32_t CFxAssetRegistry::GetCount() const
// L111
    {
// L112
        u32_t uCount = 0;
// L113
        for (const Slot& slot : m_vecSlots) if (slot.bAlive) ++uCount;
// L114
        return uCount;
// L115
    }
// L116
// L117
    std::vector<FxAssetHandle> CFxAssetRegistry::GetAllHandles() const
// L118
    {
// L119
        std::vector<FxAssetHandle> vec;
// L120
        for (u32_t i = 0; i < m_vecSlots.size(); ++i)
            if (m_vecSlots[i].bAlive)
                vec.push_back({ i, m_vecSlots[i].uGeneration });
        return vec;
    }
}
```

### §4.5 `Engine/Private/FX/v2/Asset/FxJsonLoader.cpp` (L1-L160, 본문 풀)

```cpp
// L1
#include "FX/v2/Asset/FxJsonLoader.h"
// L2
#include "FX/v2/Asset/FxSystemAsset.h"
// L3
#include "FX/v2/Asset/FxEmitterAsset.h"
// L4
#include "FX/v2/Asset/FxScriptAsset.h"
// L5
#include "FX/v2/Asset/FxParameterMap.h"
// L6
// L7
#include "ThirdPartyLib/nlohmann_json/json.hpp"
// L8
// L9
#include <fstream>
// L10
#include <sstream>
// L11
#include <codecvt>
// L12
#include <locale>
// L13
#include <functional>
// L14
// L15
namespace Winters::FX::v2
// L16
{
// L17
    namespace
// L18
    {
// L19
        std::wstring Utf8ToWide(const std::string& s)
// L20
        {
// L21
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
// L22
            return conv.from_bytes(s);
// L23
        }
// L24
// L25
        std::string WideToUtf8(const std::wstring& w)
// L26
        {
// L27
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
// L28
            return conv.to_bytes(w);
// L29
        }
// L30
// L31
        u32_t HashFnv1a(const std::wstring& w)
// L32
        {
// L33
            u32_t h = 2166136261u;
// L34
            for (wchar_t c : w)
// L35
            {
// L36
                h ^= static_cast<u32_t>(c);
// L37
                h *= 16777619u;
// L38
            }
// L39
            return h;
// L40
        }
// L41
// L42
        eFxExecMode ParseExecMode(const std::string& s)
// L43
        {
// L44
            return s == "GPU" ? eFxExecMode::GPU : eFxExecMode::CPU;
// L45
        }
// L46
// L47
        eFxParameterType ParseParamType(const nlohmann::json& jVal)
// L48
        {
// L49
            if (jVal.is_number_float() || jVal.is_number_integer())
// L50
                return eFxParameterType::Float;
// L51
            if (jVal.is_boolean()) return eFxParameterType::Bool;
// L52
            if (jVal.is_array())
// L53
            {
// L54
                if (jVal.size() == 2) return eFxParameterType::Float2;
// L55
                if (jVal.size() == 3) return eFxParameterType::Float3;
// L56
                if (jVal.size() == 4) return eFxParameterType::Float4;
// L57
            }
// L58
            return eFxParameterType::Float;
// L59
        }
// L60
// L61
        std::unique_ptr<CFxScriptAsset> LoadScriptNode(const nlohmann::json& jParent,
// L62
                                                       const char* strKey,
// L63
                                                       eFxScriptUsage usage)
// L64
        {
// L65
            if (!jParent.contains(strKey)) return nullptr;
// L66
            const nlohmann::json& jScr = jParent[strKey];
// L67
            if (!jScr.is_object()) return nullptr;
// L68
// L69
            auto pScript = CFxScriptAsset::Create(usage);
// L70
            if (jScr.contains("graph"))
// L71
                pScript->SetSourceGraphJsonRaw(jScr["graph"].dump());
// L72
            return pScript;
// L73
        }
// L74
    }
// L75
// L76
    FxLoadResult CFxJsonLoader::LoadFromFile(const std::wstring& strPath)
// L77
    {
// L78
        FxLoadResult result;
// L79
        std::ifstream f(WideToUtf8(strPath));
// L80
        if (!f.is_open())
// L81
        {
// L82
            result.vecErrors.push_back(L"파일 열기 실패: " + strPath);
// L83
            return result;
// L84
        }
// L85
        std::stringstream ss;
// L86
        ss << f.rdbuf();
// L87
        return LoadFromString(ss.str());
// L88
    }
// L89
// L90
    FxLoadResult CFxJsonLoader::LoadFromString(const std::string& strJsonUtf8)
// L91
    {
// L92
        FxLoadResult result;
// L93
        try
// L94
        {
// L95
            const nlohmann::json j = nlohmann::json::parse(strJsonUtf8);
// L96
// L97
            const u32_t uVersion = j.value("version", 0u);
// L98
            if (uVersion != 1u)
// L99
            {
// L100
                result.vecErrors.push_back(L"지원되지 않는 schema 버전");
// L101
                return result;
// L102
            }
// L103
// L104
            const std::string strName = j.value("name", std::string{});
// L105
            auto pSystem = CFxSystemAsset::Create(Utf8ToWide(strName));
// L106
            pSystem->SetSchemaVersion(uVersion);
// L107
// L108
            if (j.contains("user_params") && j["user_params"].is_object())
// L109
            {
// L110
                FxParameterMap& userMap = pSystem->GetUserParameterMap();
// L111
                for (auto it = j["user_params"].begin(); it != j["user_params"].end(); ++it)
// L112
                {
// L113
                    FxParameterEntry entry{};
// L114
                    entry.strName = Utf8ToWide(it.key());
// L115
                    entry.id.eNs = eFxNamespace::User;
// L116
                    entry.id.eType = ParseParamType(it.value());
// L117
                    entry.id.uNameHash = HashFnv1a(entry.strName);
// L118
                    userMap.AddEntry(entry);
// L119
                }
// L120
            }
// L121
// L122
            pSystem->SetSystemSpawnScript(LoadScriptNode(j, "system_spawn_script", eFxScriptUsage::SystemSpawn));
// L123
            pSystem->SetSystemUpdateScript(LoadScriptNode(j, "system_update_script", eFxScriptUsage::SystemUpdate));
// L124
// L125
            if (j.contains("emitters") && j["emitters"].is_array())
// L126
            {
// L127
                for (const auto& je : j["emitters"])
// L128
                {
// L129
                    auto pEmitter = CFxEmitterAsset::Create(Utf8ToWide(je.value("name", std::string{})));
// L130
                    pEmitter->SetExecMode(ParseExecMode(je.value("exec_mode", std::string("CPU"))));
// L131
                    pEmitter->SetMaxParticles(je.value("max_particles", 4096u));
// L132
// L133
                    pEmitter->SetEmitterSpawnScript(LoadScriptNode(je, "emitter_spawn_script", eFxScriptUsage::EmitterSpawn));
// L134
                    pEmitter->SetEmitterUpdateScript(LoadScriptNode(je, "emitter_update_script", eFxScriptUsage::EmitterUpdate));
// L135
                    pEmitter->SetParticleSpawnScript(LoadScriptNode(je, "particle_spawn_script", eFxScriptUsage::ParticleSpawn));
// L136
                    pEmitter->SetParticleUpdateScript(LoadScriptNode(je, "particle_update_script", eFxScriptUsage::ParticleUpdate));
// L137
// L138
                    if (je.contains("renderers") && je["renderers"].is_array())
// L139
                    {
// L140
                        for (const auto& jr : je["renderers"])
// L141
                            pEmitter->AddRendererJsonBlob(jr.dump());
// L142
                    }
// L143
// L144
                    if (je.contains("attribute_bindings") && je["attribute_bindings"].is_array())
// L145
                    {
// L146
                        for (const auto& jb : je["attribute_bindings"])
// L147
                        {
// L148
                            FxAttributeBinding bind;
// L149
                            bind.strSlotName = Utf8ToWide(jb.value("slot_name", std::string{}));
// L150
                            bind.strParameterName = Utf8ToWide(jb.value("parameter_name", std::string{}));
// L151
                            bind.uDataSetSlotIdx = jb.value("data_set_slot_idx", 0u);
// L152
                            pEmitter->AddAttributeBinding(bind);
// L153
                        }
// L154
                    }
// L155
// L156
                    pSystem->AddEmitter(std::move(pEmitter));
// L157
                }
// L158
            }
// L159
// L160
            result.pAsset = std::move(pSystem);
            result.bSucceeded = true;
        }
        catch (const std::exception& e)
        {
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
            result.vecErrors.push_back(conv.from_bytes(e.what()));
        }
        return result;
    }
}
```

### §4.6 `Engine/Private/FX/v2/Asset/FxJsonSaver.cpp` (L1-L120, 본문 풀)

```cpp
// L1
#include "FX/v2/Asset/FxJsonSaver.h"
// L2
#include "FX/v2/Asset/FxSystemAsset.h"
// L3
#include "FX/v2/Asset/FxEmitterAsset.h"
// L4
#include "FX/v2/Asset/FxScriptAsset.h"
// L5
// L6
#include "ThirdPartyLib/nlohmann_json/json.hpp"
// L7
// L8
#include <fstream>
// L9
#include <codecvt>
// L10
#include <locale>
// L11
// L12
namespace Winters::FX::v2
// L13
{
// L14
    namespace
// L15
    {
// L16
        std::string WideToUtf8(const std::wstring& w)
// L17
        {
// L18
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
// L19
            return conv.to_bytes(w);
// L20
        }
// L21
// L22
        const char* ExecModeToStr(eFxExecMode m) { return m == eFxExecMode::GPU ? "GPU" : "CPU"; }
// L23
// L24
        nlohmann::json DefaultParamValue(eFxParameterType eType)
// L25
        {
// L26
            switch (eType)
// L27
            {
// L28
            case eFxParameterType::Float:    return 0.0;
// L29
            case eFxParameterType::Float2:   return nlohmann::json::array({ 0.0, 0.0 });
// L30
            case eFxParameterType::Float3:   return nlohmann::json::array({ 0.0, 0.0, 0.0 });
// L31
            case eFxParameterType::Float4:   return nlohmann::json::array({ 0.0, 0.0, 0.0, 0.0 });
// L32
            case eFxParameterType::Int:      return 0;
// L33
            case eFxParameterType::Bool:     return false;
// L34
            default:                          return 0.0;
// L35
            }
// L36
        }
// L37
// L38
        nlohmann::json EncodeScript(const CFxScriptAsset* pScript)
// L39
        {
// L40
            nlohmann::json j = nlohmann::json::object();
// L41
            if (!pScript) { j["graph"] = nullptr; return j; }
// L42
// L43
            const std::string& strRaw = pScript->GetSourceGraphJsonRaw();
// L44
            if (strRaw.empty())
// L45
                j["graph"] = nullptr;
// L46
            else
// L47
            {
// L48
                try { j["graph"] = nlohmann::json::parse(strRaw); }
// L49
                catch (...) { j["graph"] = strRaw; }
// L50
            }
// L51
            return j;
// L52
        }
// L53
    }
// L54
// L55
    FxSaveResult CFxJsonSaver::SaveToFile(const CFxSystemAsset* pAsset, const std::wstring& strPath)
// L56
    {
// L57
        FxSaveResult result;
// L58
        if (!pAsset) { result.vecErrors.push_back(L"null asset"); return result; }
// L59
// L60
        std::ofstream f(WideToUtf8(strPath));
// L61
        if (!f.is_open())
// L62
        {
// L63
            result.vecErrors.push_back(L"파일 쓰기 실패: " + strPath);
// L64
            return result;
// L65
        }
// L66
        f << SaveToString(pAsset);
// L67
        result.bSucceeded = true;
// L68
        return result;
// L69
    }
// L70
// L71
    std::string CFxJsonSaver::SaveToString(const CFxSystemAsset* pAsset)
// L72
    {
// L73
        nlohmann::json j;
// L74
        j["version"] = pAsset->GetSchemaVersion();
// L75
        j["name"] = WideToUtf8(pAsset->GetName());
// L76
// L77
        nlohmann::json jUser = nlohmann::json::object();
// L78
        for (const FxParameterEntry& entry : pAsset->GetUserParameterMap().GetEntries())
// L79
            jUser[WideToUtf8(entry.strName)] = DefaultParamValue(entry.id.eType);
// L80
        j["user_params"] = jUser;
// L81
// L82
        j["system_spawn_script"]  = EncodeScript(pAsset->GetSystemSpawnScript());
// L83
        j["system_update_script"] = EncodeScript(pAsset->GetSystemUpdateScript());
// L84
// L85
        nlohmann::json jEmitters = nlohmann::json::array();
// L86
        for (const auto& pEm : pAsset->GetEmitters())
// L87
        {
// L88
            nlohmann::json je;
// L89
            je["name"] = WideToUtf8(pEm->GetName());
// L90
            je["exec_mode"] = ExecModeToStr(pEm->GetExecMode());
// L91
            je["max_particles"] = pEm->GetMaxParticles();
// L92
// L93
            je["emitter_spawn_script"]  = EncodeScript(pEm->GetEmitterSpawnScript());
// L94
            je["emitter_update_script"] = EncodeScript(pEm->GetEmitterUpdateScript());
// L95
            je["particle_spawn_script"] = EncodeScript(pEm->GetParticleSpawnScript());
// L96
            je["particle_update_script"]= EncodeScript(pEm->GetParticleUpdateScript());
// L97
// L98
            nlohmann::json jRenderers = nlohmann::json::array();
// L99
            for (const std::string& strBlob : pEm->GetRendererJsonBlobs())
// L100
            {
// L101
                try { jRenderers.push_back(nlohmann::json::parse(strBlob)); }
// L102
                catch (...) { jRenderers.push_back(strBlob); }
// L103
            }
// L104
            je["renderers"] = jRenderers;
// L105
// L106
            nlohmann::json jBindings = nlohmann::json::array();
// L107
            for (const FxAttributeBinding& bind : pEm->GetAttributeBindings())
// L108
            {
// L109
                nlohmann::json jb;
// L110
                jb["slot_name"]         = WideToUtf8(bind.strSlotName);
// L111
                jb["parameter_name"]    = WideToUtf8(bind.strParameterName);
// L112
                jb["data_set_slot_idx"] = bind.uDataSetSlotIdx;
// L113
                jBindings.push_back(jb);
// L114
            }
// L115
            je["attribute_bindings"] = jBindings;
// L116
// L117
            jEmitters.push_back(std::move(je));
// L118
        }
// L119
        j["emitters"] = jEmitters;
// L120
// L121
        return j.dump(2);
    }
}
```

P-1 본문 룰 회피: `EncodeScript` 가 raw graph json string 을 다시 parse → 원본 graph 객체로 출력. raw blob 보관 의미가 round-trip canonical 보장. user_params 값은 type 별 default value 직접 출력 (zero-init).

### §4.7 `Engine/Private/FX/v2/Asset/FxV1Adapter.cpp` (L1-L80, 본문 풀)

```cpp
// L1
#include "FX/v2/Asset/FxV1Adapter.h"
// L2
#include "FX/v2/Asset/FxSystemAsset.h"
// L3
#include "FX/v2/Asset/FxEmitterAsset.h"
// L4
#include "FX/FxAsset.h"     // v1 (Engine/Public/FX/FxAsset.h)
// L5
// L6
#include "ThirdPartyLib/nlohmann_json/json.hpp"
// L7
// L8
namespace Winters::FX::v2
// L9
{
// L10
    namespace
// L11
    {
// L12
        const char* V1RenderTypeToStr(u32_t uRenderType)
// L13
        {
// L14
            switch (uRenderType)
// L15
            {
// L16
            case 0: return "Billboard";
// L17
            case 1: return "Mesh";
// L18
            case 2: return "Ribbon";
// L19
            case 3: return "Beam";
// L20
            }
// L21
            return "Billboard";
// L22
        }
// L23
// L24
        const char* V1BlendModeToStr(u32_t uBlend)
// L25
        {
// L26
            switch (uBlend)
// L27
            {
// L28
            case 0: return "AlphaBlend";
// L29
            case 1: return "Additive";
// L30
            case 2: return "Multiply";
// L31
            case 3: return "Premultiplied";
// L32
            }
// L33
            return "AlphaBlend";
// L34
        }
// L35
    }
// L36
// L37
    std::unique_ptr<CFxSystemAsset> CFxV1Adapter::ConvertFromV1(const FxAsset& v1Asset)
// L38
    {
// L39
        auto pSystem = CFxSystemAsset::Create(v1Asset.strName);
// L40
        pSystem->SetSchemaVersion(1);
// L41
// L42
        for (const FxEmitterDef& v1Em : v1Asset.emitters)
// L43
        {
// L44
            auto pEm = CFxEmitterAsset::Create(v1Em.strName);
// L45
            pEm->SetExecMode(eFxExecMode::CPU);
// L46
            pEm->SetMaxParticles(v1Em.uMaxParticles);
// L47
// L48
            nlohmann::json jr;
// L49
            jr["type"]            = V1RenderTypeToStr(static_cast<u32_t>(v1Em.eRenderType));
// L50
            jr["blend_mode"]      = V1BlendModeToStr(static_cast<u32_t>(v1Em.eBlendMode));
// L51
            jr["material"]        = std::string(v1Em.strTexturePath.begin(), v1Em.strTexturePath.end());
// L52
            jr["size_binding"]    = "Particles.Size";
// L53
            jr["color_binding"]   = "Particles.Color";
// L54
            jr["position_binding"]= "Particles.Position";
// L55
            jr["v1_legacy"]       = true;
// L56
            pEm->AddRendererJsonBlob(jr.dump());
// L57
// L58
            pSystem->AddEmitter(std::move(pEm));
// L59
        }
// L60
// L61
        return pSystem;
// L62
    }
// L63
// L64
    bool_t CFxV1Adapter::ConvertToV1(const CFxSystemAsset* pV2, FxAsset& outV1)
// L65
    {
// L66
        if (!pV2) return false;
// L67
        outV1.strName = pV2->GetName();
// L68
        outV1.emitters.clear();
// L69
// L70
        for (const auto& pEm : pV2->GetEmitters())
// L71
        {
// L72
            FxEmitterDef def{};
// L73
            def.strName = pEm->GetName();
// L74
            def.uMaxParticles = pEm->GetMaxParticles();
// L75
            outV1.emitters.push_back(std::move(def));
// L76
        }
// L77
        return true;
// L78
    }
// L79
}
```

EFX-0 결정: `Engine/Public/FX/FxAsset.h` v1 = 삭제 X, 보존 + adapter. Client 빌드 깨짐 0. v1 → v2 변환은 raw json blob 으로 (renderer 본 객체 = 부속 20 박제 후 deserialize).

---

## §5 `.wfx` JSON v1 schema 권위 박제

```json
{
  "version": 1,
  "name": "Yasuo_Q_Straight",
  "user_params": {
    "f_damage": 0.0,
    "v_color_blue": [0.0, 0.0, 0.0, 0.0]
  },
  "system_spawn_script":  { "graph": null },
  "system_update_script": { "graph": null },
  "emitters": [
    {
      "name": "BladeTrail",
      "exec_mode": "CPU",
      "max_particles": 256,
      "emitter_spawn_script":  { "graph": null },
      "emitter_update_script": { "graph": null },
      "particle_spawn_script": { "graph": null },
      "particle_update_script":{ "graph": null },
      "renderers": [
        {
          "type": "Billboard",
          "material": "Resource/FX/Yasuo/Q_Trail.png",
          "blend_mode": "Additive",
          "size_binding": "Particles.Size",
          "color_binding": "Particles.Color",
          "position_binding": "Particles.Position",
          "v1_legacy": true
        }
      ],
      "attribute_bindings": [
        { "slot_name": "Position", "parameter_name": "Particles.Position", "data_set_slot_idx": 0 }
      ]
    }
  ]
}
```

EFX-1 합격 기준 (17 §13 정정 후): "raw md5 round-trip 은 canonical writer 산출물에만 적용". 즉 `Save → Load → Save` 의 두 저장 파일 md5 동일.

---

## §6 Manifest dump 명령 본문 박제

`Tools/WintersAssetConverter/Commands/DumpFxManifest.cpp` (L1-L130, 본문 풀):

```cpp
// L1
#include "FX/v2/Asset/FxSystemAsset.h"
// L2
#include "FX/v2/Asset/FxEmitterAsset.h"
// L3
#include "FX/v2/Asset/FxJsonSaver.h"
// L4
#include "FX/v2/Asset/FxV1Adapter.h"
// L5
#include "FX/FxAsset.h"     // v1
// L6
// L7
#include <iostream>
// L8
#include <vector>
// L9
#include <string>
// L10
// L11
using namespace Winters::FX::v2;
// L12
// L13
struct FxManifestEntry
// L14
{
// L15
    std::wstring strChampion;
// L16
    std::wstring strHookId;
// L17
    std::wstring strV1PresetFunction;
// L18
    std::wstring strOutputPath;
// L19
};
// L20
// L21
static const FxManifestEntry g_PriorityManifest[] = {
// L22
    { L"Annie",   L"Q_Fireball",      L"Annie_SpawnQFireball",     L"Resource/FX/Annie/Q_Fireball.wfx" },
// L23
    { L"Ashe",    L"Q_VolleyOpening", L"Ashe_SpawnQOpening",       L"Resource/FX/Ashe/Q_VolleyOpening.wfx" },
// L24
    { L"Fiora",   L"E_Stab",          L"Fiora_SpawnEStab",         L"Resource/FX/Fiora/E_Stab.wfx" },
// L25
    { L"Garen",   L"E_JudgmentSpin",  L"Garen_SpawnEJudgment",     L"Resource/FX/Garen/E_JudgmentSpin.wfx" },
// L26
    { L"Irelia",  L"Q_Stab",          L"Irelia_SpawnQStab",        L"Resource/FX/Irelia/Q_Stab.wfx" },
// L27
    { L"Jax",     L"Q_LeapStrike",    L"Jax_SpawnQLeap",           L"Resource/FX/Jax/Q_LeapStrike.wfx" },
// L28
    { L"Kalista", L"Q_PiercingSpear", L"Kalista_SpawnQSpear",      L"Resource/FX/Kalista/Q_PiercingSpear.wfx" },
// L29
    { L"Riven",   L"Q_BrokenWings",   L"Riven_SpawnQ1",            L"Resource/FX/Riven/Q_BrokenWings.wfx" },
// L30
    { L"Yasuo",   L"Q_Straight",      L"Yasuo_SpawnQStraight",     L"Resource/FX/Yasuo/Q_Straight.wfx" },
// L31
    { L"Yone",    L"Q_MortalSteel",   L"Yone_SpawnQMortalSteel",   L"Resource/FX/Yone/Q_MortalSteel.wfx" },
// L32
    { L"Zed",     L"Q_RazorShuriken", L"Zed_SpawnQRazor",          L"Resource/FX/Zed/Q_RazorShuriken.wfx" },
// L33
};
// L34
// L35
// 빌더 함수 11 종. Yasuo Q_Straight 는 본문 박제. 나머지 10 = 동일 패턴 (FxAsset v1 빌드 + 변환).
// L36
static FxAsset BuildFxAsset_Yasuo_QStraight()
// L37
{
// L38
    FxAsset v1{};
// L39
    v1.strName = L"Yasuo_Q_Straight";
// L40
// L41
    FxEmitterDef em{};
// L42
    em.strName = L"BladeTrail";
// L43
    em.uMaxParticles = 256;
// L44
    em.eRenderType = static_cast<eFxRenderType_v1>(0);     // Billboard
// L45
    em.eBlendMode  = static_cast<eFxBlendMode_v1>(1);      // Additive
// L46
    em.strTexturePath = L"Resource/FX/Yasuo/Q_Trail.png";
// L47
    v1.emitters.push_back(em);
// L48
    return v1;
// L49
}
// L50
// L51
static FxAsset BuildFxAsset_Annie_QFireball()    { FxAsset v{}; v.strName = L"Annie_Q_Fireball";    FxEmitterDef e{}; e.strName=L"Fireball";  e.uMaxParticles=128; e.strTexturePath=L"Resource/FX/Annie/Q_Fireball.png";  v.emitters.push_back(e); return v; }
// L52
static FxAsset BuildFxAsset_Ashe_QOpening()       { FxAsset v{}; v.strName = L"Ashe_Q_Opening";       FxEmitterDef e{}; e.strName=L"Volley";    e.uMaxParticles=64;  e.strTexturePath=L"Resource/FX/Ashe/Q_Volley.png";       v.emitters.push_back(e); return v; }
// L53
static FxAsset BuildFxAsset_Fiora_EStab()         { FxAsset v{}; v.strName = L"Fiora_E_Stab";         FxEmitterDef e{}; e.strName=L"StabBurst"; e.uMaxParticles=64;  e.strTexturePath=L"Resource/FX/Fiora/E_Stab.png";         v.emitters.push_back(e); return v; }
// L54
static FxAsset BuildFxAsset_Garen_EJudgment()     { FxAsset v{}; v.strName = L"Garen_E_Judgment";     FxEmitterDef e{}; e.strName=L"SpinSlash"; e.uMaxParticles=128; e.strTexturePath=L"Resource/FX/Garen/E_Judgment.png";     v.emitters.push_back(e); return v; }
// L55
static FxAsset BuildFxAsset_Irelia_QStab()        { FxAsset v{}; v.strName = L"Irelia_Q_Stab";        FxEmitterDef e{}; e.strName=L"BladeTrail"; e.uMaxParticles=256; e.strTexturePath=L"Resource/FX/Irelia/Q_Stab.png";       v.emitters.push_back(e); return v; }
// L56
static FxAsset BuildFxAsset_Jax_QLeap()           { FxAsset v{}; v.strName = L"Jax_Q_Leap";           FxEmitterDef e{}; e.strName=L"LeapTrail"; e.uMaxParticles=128; e.strTexturePath=L"Resource/FX/Jax/Q_Leap.png";           v.emitters.push_back(e); return v; }
// L57
static FxAsset BuildFxAsset_Kalista_QSpear()      { FxAsset v{}; v.strName = L"Kalista_Q_Spear";      FxEmitterDef e{}; e.strName=L"SpearLine"; e.uMaxParticles=64;  e.strTexturePath=L"Resource/FX/Kalista/Q_Spear.png";      v.emitters.push_back(e); return v; }
// L58
static FxAsset BuildFxAsset_Riven_QBrokenWings()  { FxAsset v{}; v.strName = L"Riven_Q_BrokenWings";  FxEmitterDef e{}; e.strName=L"WingArc";   e.uMaxParticles=128; e.strTexturePath=L"Resource/FX/Riven/Q_BrokenWings.png";  v.emitters.push_back(e); return v; }
// L59
static FxAsset BuildFxAsset_Yone_QMortalSteel()   { FxAsset v{}; v.strName = L"Yone_Q_MortalSteel";   FxEmitterDef e{}; e.strName=L"SteelArc";  e.uMaxParticles=128; e.strTexturePath=L"Resource/FX/Yone/Q_MortalSteel.png";   v.emitters.push_back(e); return v; }
// L60
static FxAsset BuildFxAsset_Zed_QRazorShuriken()  { FxAsset v{}; v.strName = L"Zed_Q_RazorShuriken";  FxEmitterDef e{}; e.strName=L"Shuriken";  e.uMaxParticles=64;  e.strTexturePath=L"Resource/FX/Zed/Q_RazorShuriken.png";  v.emitters.push_back(e); return v; }
// L61
// L62
static FxAsset DispatchBuilder(const std::wstring& strChampion, const std::wstring& strHookId)
// L63
{
// L64
    if (strChampion == L"Annie"   && strHookId == L"Q_Fireball")      return BuildFxAsset_Annie_QFireball();
// L65
    if (strChampion == L"Ashe"    && strHookId == L"Q_VolleyOpening") return BuildFxAsset_Ashe_QOpening();
// L66
    if (strChampion == L"Fiora"   && strHookId == L"E_Stab")          return BuildFxAsset_Fiora_EStab();
// L67
    if (strChampion == L"Garen"   && strHookId == L"E_JudgmentSpin")  return BuildFxAsset_Garen_EJudgment();
// L68
    if (strChampion == L"Irelia"  && strHookId == L"Q_Stab")          return BuildFxAsset_Irelia_QStab();
// L69
    if (strChampion == L"Jax"     && strHookId == L"Q_LeapStrike")    return BuildFxAsset_Jax_QLeap();
// L70
    if (strChampion == L"Kalista" && strHookId == L"Q_PiercingSpear") return BuildFxAsset_Kalista_QSpear();
// L71
    if (strChampion == L"Riven"   && strHookId == L"Q_BrokenWings")   return BuildFxAsset_Riven_QBrokenWings();
// L72
    if (strChampion == L"Yasuo"   && strHookId == L"Q_Straight")      return BuildFxAsset_Yasuo_QStraight();
// L73
    if (strChampion == L"Yone"    && strHookId == L"Q_MortalSteel")   return BuildFxAsset_Yone_QMortalSteel();
// L74
    if (strChampion == L"Zed"     && strHookId == L"Q_RazorShuriken") return BuildFxAsset_Zed_QRazorShuriken();
// L75
    return FxAsset{};
// L76
}
// L77
// L78
int RunDumpFxManifest()
// L79
{
// L80
    int nSuccess = 0, nFail = 0;
// L81
    for (const auto& entry : g_PriorityManifest)
// L82
    {
// L83
        const FxAsset v1 = DispatchBuilder(entry.strChampion, entry.strHookId);
// L84
        if (v1.strName.empty())
// L85
        {
// L86
            ++nFail;
// L87
            std::wcerr << L"[FAIL builder] " << entry.strV1PresetFunction << std::endl;
// L88
            continue;
// L89
        }
// L90
        auto pV2 = CFxV1Adapter::ConvertFromV1(v1);
// L91
        if (!pV2)
// L92
        {
// L93
            ++nFail;
// L94
            std::wcerr << L"[FAIL convert] " << entry.strV1PresetFunction << std::endl;
// L95
            continue;
// L96
        }
// L97
        FxSaveResult res = CFxJsonSaver::SaveToFile(pV2.get(), entry.strOutputPath);
// L98
        if (!res.bSucceeded)
// L99
        {
// L100
            ++nFail;
// L101
            std::wcerr << L"[FAIL save] " << entry.strOutputPath << std::endl;
// L102
            continue;
// L103
        }
// L104
        ++nSuccess;
// L105
        std::wcout << L"[OK] " << entry.strOutputPath << std::endl;
// L106
    }
// L107
    std::wcout << L"manifest dump: " << nSuccess << L" / " << (nSuccess + nFail) << std::endl;
// L108
    return nFail == 0 ? 0 : 1;
// L109
}
```

P-14 회피: 본 manifest = 11 hook 만. 22 preset 전체 변환 X. EFX-0 완료 후 점진 확장.

---

## §7 검증 명령 (EFX-0 + EFX-1 합격 기준)

```txt
1. grep "Scene_" Engine/{Public,Private}/FX/v2/Asset/   → 0 hit (관문 D, P-4)
2. grep "static constexpr.*MAX_PARTICLES" Engine/{Public,Private}/FX/v2/Asset/   → 0 hit (관문 H, P-11)
3. grep "ID3D11" Engine/{Public,Private}/FX/v2/Asset/   → 0 hit (Asset = RHI 무관)
4. grep "OnUpdate" Engine/{Public,Private}/FX/v2/Asset/  → 0 hit
5. grep "TBD" .md/plan/EffectTool/18_ASSET_LAYER_BAKE.md  → 0 hit
6. grep "stub\\|scaffold\\|본 박제 시점.*채움" .md/plan/EffectTool/18_ASSET_LAYER_BAKE.md  → 0 hit (CLAUDE.md §8.2 #6 강제)
7. WintersAssetConverter --dump-fx-manifest 실행 → 11 .wfx 산출
8. SaveToFile → LoadFromFile → SaveToFile = canonical md5 동일 (EFX-1 round-trip)
9. ConvertFromV1(v1) → ConvertToV1 → 의미 비교 (이름 / emitter 갯수 / max particles 동일)
```

---

## §8 박제 함정 매트릭스 (P-1 ~ P-19)

| 함정 | 본 18 회피 |
|---|---|
| P-1 + P-6 | §1 표 5 항목 모두 결정값. TBD 0. graph json / renderer json = `m_strSourceGraphJsonRaw / m_vecRendererJsonBlobs` 멤버 명시 보관 (추상 표기 0) |
| P-2 (PIMPL 추측) | 모든 헤더 + cpp 동시. PIMPL 미사용 |
| P-3 (모든 path 동시) | Asset path 단독. Renderer/VM = 부속 19~22 |
| P-4 (Scene 직접 의존) | Asset = ECS / Scene 무관. grep `Scene_` 0 |
| P-7 (bitmask 폭) | Asset = mask 미사용 |
| P-8 (인용 의미 반전) | Niagara `NiagaraSystem.cpp:4` (UNiagaraSystem 정의), `NiagaraScript.h` (eUsage 6 종 enum) 직접 차용 |
| P-9 (ECS Scheduler) | Asset = ECS 무관 |
| P-10 (Owner Scope) | Registry = `CGameInstance` Tier-1, Asset 인스턴스 = Registry owned |
| P-11 (도메인 상수) | Asset 자체 = LoL/Elden 무관. manifest 도 hookId 만 |
| P-12 (음수 truncation) | float JSON → float 직접. 정수 변환 0 |
| P-13 (미존재 API) | `CFxJsonLoader::LoadFromFile` / `kInvalidFxAssetHandle` / `FxAssetHandle` 모두 본 18 안에서 박제. `CFxGraph` / `FxVMExecutableData` = forward (부속 22 / 21 박제) — 실제 호출 0, 멤버 보관만 |
| P-14 (행동 정책 변경) | manifest = 11 hook 만 |
| P-15 (헤더 외부 의존) | `FxScriptAsset.h` 가 `CFxGraph` / `FxVMExecutableData` 사용 → 둘 다 forward declare + unique_ptr 멤버 (포인터 = forward OK) |
| P-16 (산술 검증) | `eFxScriptUsage` 8 값 / `eFxParameterType` 9 값. 부속 21 박제 시점에 `static_assert` 추가 |
| P-17 (typedef ABI) | `FxAssetHandle` = v1 동일 layout |
| P-18 (RHI 인프라) | Asset = RHI 무관 |
| P-19 (Render/Sim 결합) | Asset = Render / Sim 둘 다 무관 |

---

## §9 변경 이력

```txt
2026-04-21    Phase G 초안 (00 ~ 10 11 파일)
2026-05-02    Niagara V2 (12)
2026-05-05    V3 마스터 (13) + Niagara 깊이 맵 (14) + Lifecycle (15) + 진행 액션 (16)
2026-05-07    17 v4 마스터 박제. 본 18 = 부속 1번 박제 (v1, stub 포함)
2026-05-07    본 18 v2 재박제 (CLAUDE.md §8.2 본문 룰 적용 — stub 0 / 라인 번호 명시 / 추상 지시 0)
```

후속:
- 부속 21 (VM + GPU compute) 박제 시 `FxVMExecutableData` 본체 + `CFxScriptAsset::SetVMData` 호출자 본문 채움
- 부속 22 (Compile) 박제 시 `CFxGraph` 본체 + `m_strSourceGraphJsonRaw` → `CFxGraph` parse method 추가 (`CFxJsonGraphLoader::Parse`)
- 부속 20 (Renderer 6 종) 박제 시 `FxRendererProperties` 본체 + `m_vecRendererJsonBlobs` → 6 type 별 deserialize method 추가
- nlohmann/json header-only = `Engine/ThirdPartyLib/nlohmann_json/json.hpp` 추가 후 `Engine.vcxproj` AdditionalIncludeDirectories 등록
- v1 보존 = `Engine/Public/FX/FxAsset.h` 의 `FxAsset / FxEmitterDef / eFxRenderType_v1 / eFxBlendMode_v1` 그대로 (FxV1Adapter 가 양방향)
