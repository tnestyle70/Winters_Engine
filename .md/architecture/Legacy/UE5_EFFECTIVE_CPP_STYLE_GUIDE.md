# Winters UE5 + Effective C++ 작성 규칙

작성일: 2026-05-10

목적: Winters C++ 코드를 UE5 스타일의 대형 엔진 코드베이스 감각과 Effective C++ 계열의 안정성 원칙에 맞춰 작성한다.

참고 기준:

- 로컬 UE5 소스: `C:/Users/user/Desktop/UnrealEngine/UnrealEngine/Engine`
- UE AI 모듈 확장 규칙: `Engine/Source/Runtime/AIModule/AICodingStandard.md`
- Winters 기존 규칙: `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md`, `CLAUDE.md`, `AGENTS.md`
- Effective C++ 계열 원칙: RAII, const correctness, Rule of Zero/Five, explicit ownership, initialization, interface discipline

이 문서는 UE5 타입 접두사(`F`, `T`, `U`, `A`, `S`)를 Winters에 그대로 가져오자는 뜻이 아니다. Winters는 이미 `C` class, `I` interface, POD component, ECS/DOD, DX11/Engine DLL 경계 규칙을 갖고 있다. 여기서 채택하는 것은 UE5의 **대규모 C++ 코드 운영 방식**이다.

---

## 0. 우선순위

규칙 충돌 시 아래 순서로 판단한다.

1. 서버 권위/GameSim/네트워크 correctness
2. 기존 public contract 및 ABI/serialization 호환성
3. Winters 기존 컨벤션
4. 이 문서의 UE5 + Effective C++ 규칙
5. 현재 파일의 국소 스타일

중요:

- 기존 코드를 전역 기계식 리네임하지 않는다.
- 손대는 파일과 새 파일부터 점진 적용한다.
- FlatBuffers 생성 코드, 외부 API, 이미 넓게 퍼진 public contract는 이름을 함부로 바꾸지 않는다.
- 사용자가 직접 반영하는 세션에서는 `.h/.cpp` 코드를 먼저 직접 수정하지 않고 handoff 형식으로 제공한다.

---

## 1. Winters에 채택하는 UE5식 큰 원칙

### 1.1 Public/Private 경계

UE5는 모듈 경계를 강하게 나눈다. Winters도 다음 원칙을 따른다.

- `Public/`, `Include/` 헤더는 다른 모듈이 직접 include할 수 있다고 가정한다.
- public 헤더는 가능한 한 가볍게 유지한다.
- 구현 세부 타입은 `.cpp` 또는 `Private/` 헤더로 숨긴다.
- public 헤더에 STL/Windows/DX/Assimp/FMOD 헤더를 과도하게 끌어오지 않는다.
- public 헤더에서 필요한 것은 전방 선언하고, 값 멤버 또는 inline 구현 때문에 꼭 필요할 때만 include한다.

좋은 방향:

```cpp
// Header
class CWorld;
class EntityIdMap;

class CSnapshotApplier final
{
public:
    void OnSnapshot(CWorld& world, EntityIdMap& entityMap, const u8_t* pPayload, u32_t payloadSize);
};
```

피할 방향:

```cpp
// Header
#include "ECS/World.h"
#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"
#include <Windows.h>
```

단, 값 멤버가 `std::unique_ptr<ModelRenderer>`처럼 complete type을 요구하는 경우에는 헤더에서 소멸자를 `.cpp`로 빼고 전방 선언을 사용하는 방식을 우선한다.

### 1.2 IWYU 스타일

UE5는 Include What You Use 성향이 강하다. Winters도 다음을 지킨다.

- `.cpp`는 자신이 직접 쓰는 타입의 헤더를 직접 include한다.
- 다른 헤더가 우연히 include해준 것에 기대지 않는다.
- public 헤더는 더 엄격하다. PCH나 include 순서에 기대면 안 된다.
- EngineSDK로 복사되는 헤더는 폴더 포함 경로를 명시한다.

금지:

```cpp
#include "Entity.h"          // cross-folder 헤더 경로 생략
```

권장:

```cpp
#include "ECS/Entity.h"
#include "ECS/Components/TransformComponent.h"
```

### 1.3 구현 숨김

`.cpp` 전용 helper는 외부 링크 심볼로 노출하지 않는다.

허용 방식:

- anonymous namespace
- `static` file-local function
- `namespace Winters::Private`
- `namespace UE스타일::Private`에 해당하는 named private namespace

선택 기준:

- 한 `.cpp` 안에서만 쓰는 helper 여러 개: anonymous namespace 권장
- 한 `.cpp` 안의 아주 작은 helper 1개: `static`도 허용
- 여러 파일에서 공유하지만 public API가 아닌 helper: `Private/` 헤더 + named private namespace
- 테스트에서 직접 검증해야 하는 helper: anonymous namespace에 숨기지 말고 private test seam을 따로 둔다

---

## 2. Anonymous Namespace 규칙

질문: anonymous namespace는 UE5 규칙 및 Effective C++에 부합하는가?

답: **부합한다. 단, `.cpp` 구현 세부에만 사용해야 한다.**

UE5 Core 소스에서도 anonymous namespace와 named private namespace가 둘 다 쓰인다. 예를 들어 플랫폼 구현 파일들은 anonymous namespace 종료 주석을 남기고, 다른 Core 파일들은 `UE::Something::Private` 형태의 named private namespace를 사용한다. 즉 UE5 감각에서 핵심은 "외부로 노출하지 않을 구현은 숨긴다"이지, anonymous namespace를 금지하는 것이 아니다.

Winters 기준:

```cpp
namespace
{
    constexpr f32_t kMaxBindDistance = 2.5f;

    bool_t IsStructureKind(Shared::Schema::EntityKind kind)
    {
        return kind == Shared::Schema::EntityKind::Turret ||
               kind == Shared::Schema::EntityKind::Inhibitor ||
               kind == Shared::Schema::EntityKind::Nexus;
    }
}
```

좋은 사용:

- `.cpp` 안에서만 쓰는 변환 함수
- `.cpp` 안에서만 쓰는 상수
- `.cpp` 안에서만 쓰는 작은 local struct
- linkage collision을 막아야 하는 helper

나쁜 사용:

- 헤더 안의 anonymous namespace
- 다른 `.cpp`에서도 곧 필요해질 공통 로직
- 테스트해야 하는 핵심 알고리즘
- 큰 시스템을 숨겨서 설계 경계를 흐리는 경우

규칙:

- anonymous namespace는 `.cpp` 파일에서만 사용한다.
- anonymous namespace가 150줄 이상 커지면 private helper class 또는 `Private/` 헤더 분리를 검토한다.
- anonymous namespace 안의 함수도 이름은 명확하게 쓴다.
- 종료 주석을 남긴다.

```cpp
namespace
{
    // ...
} // namespace
```

---

## 3. 네이밍 규칙

### 3.1 타입 이름

Winters 기존 규칙 유지:

- class: `C` 접두사. 예: `CGameRoom`, `CSnapshotApplier`
- interface: `I` 접두사. 예: `ISystem`, `IScene`
- POD/DTO/component struct: `C` 접두사 없음. 예: `TransformComponent`, `HealthComponent`
- enum class: 기존 `e` 접두사 공존. 새 enum은 현재 모듈 스타일에 맞춘다.
- class는 상속 의도가 없으면 `final`을 우선한다.

UE5의 `F/T/U/A/S` 접두사는 Winters에 도입하지 않는다.

이유:

- Winters는 자체 엔진이고 이미 `C`/`I`/POD component 규칙이 있다.
- UE 접두사를 부분 도입하면 `CWorld`, `FWorld`, `WorldComponent`가 섞여 검색/리뷰 비용이 커진다.

### 3.2 함수 이름

허용:

- 기존 수업/Winters 스타일: `Initialize`, `Shutdown`, `Get_Transform`, `Set_Visible`
- UE 감각의 명확한 PascalCase: `FindNetworkBindCandidate`, `BuildSnapshot`

새 코드 기본:

- 이미 해당 파일이 `Get_XXX`, `Set_XXX`를 쓰면 그 스타일 유지
- 새 subsystem/helper는 PascalCase를 우선
- bool 반환 함수는 질문형으로 작성

예:

```cpp
bool_t IsNetworkAuthoritative() const;
bool_t HasPendingSnapshot() const;
EntityID FindNetworkBindCandidate(...);
```

### 3.3 변수 이름

Winters 멤버 변수 규칙 유지:

- 멤버: `m_` 접두사
- bool 멤버: `m_bReady`
- 포인터 멤버: `m_pWorld`
- float 멤버: `m_fElapsed`
- unsigned integer 멤버: `m_uLastSeq`
- Vec3 멤버: `m_vTarget`

함수 파라미터:

- 복잡한 API 또는 constructor에서는 UE식 `In`, `Out`, `InOut` 접두사 권장
- 기존 파일이 `vPos`, `pWorld` 스타일이면 국소 일관성 유지
- network/replication 식별자는 `ID` 표기를 우선한다

예:

```cpp
EntityID FindNetworkBindCandidate(
    Winters::Map::eObjectKind kind,
    eTeam team,
    u32_t subtype,
    const Vec3& vPos,
    f32_t maxDistance) const;
```

또는 새 파일에서는:

```cpp
EntityID FindNetworkBindCandidate(
    Winters::Map::eObjectKind InKind,
    eTeam InTeam,
    u32_t InSubtype,
    const Vec3& InPos,
    f32_t InMaxDistance) const;
```

중요:

- `Id`보다 `ID` 표기 우선: `netID`, `animID`, `cueID`, `sourceNetID`, `targetNetID`
- FlatBuffers 생성 접근자(`netId()`)는 생성 코드라 유지
- 로컬에서 생성 코드 값을 받는 변수는 가능하면 `netID`로 둔다

### 3.4 상수 이름

`.cpp` local 상수:

```cpp
namespace
{
    constexpr f32_t kStructureBindMaxDistance = 2.5f;
}
```

public compile-time 상수:

```cpp
inline constexpr u32_t kMaxPlayerCount = 10;
```

기존 코드의 `STAGE_MAGIC`, `STAGE_VERSION` 같은 serialization 상수는 유지한다.

---

## 4. Effective C++ 핵심 규칙

### 4.1 RAII 우선

리소스 획득과 해제는 객체 수명에 묶는다.

권장:

- `std::unique_ptr`
- `std::shared_ptr`는 공유 소유권이 진짜 있을 때만
- `Microsoft::WRL::ComPtr` for COM
- scope guard 또는 작은 RAII helper

금지:

- owning raw pointer
- `new` 후 여러 return path에서 수동 delete
- COM raw pointer 멤버

예:

```cpp
std::unique_ptr<ModelRenderer> pRenderer = std::make_unique<ModelRenderer>();
Microsoft::WRL::ComPtr<ID3D11Buffer> m_pBuffer;
```

Raw pointer 허용:

- non-owning observer
- lifetime이 외부에서 명확히 관리되는 경우
- 이름이나 주석으로 소유권을 숨기지 않는다

```cpp
CWorld* m_pWorld = nullptr; // non-owning, current scene world
```

### 4.2 Rule of Zero 우선

리소스를 직접 소유하지 않으면 destructor/copy/move를 작성하지 않는다.

좋음:

```cpp
class CReplayLibrary final
{
public:
    static std::unique_ptr<CReplayLibrary> Create();

private:
    std::vector<ReplayEntry> m_Entries;
};
```

직접 리소스를 소유하면 Rule of Five를 명시한다.

```cpp
class CFileHandle final
{
public:
    explicit CFileHandle(HANDLE hFile) noexcept;
    ~CFileHandle();

    CFileHandle(const CFileHandle&) = delete;
    CFileHandle& operator=(const CFileHandle&) = delete;

    CFileHandle(CFileHandle&& rhs) noexcept;
    CFileHandle& operator=(CFileHandle&& rhs) noexcept;
};
```

### 4.3 복사 금지 명시

manager/system/scene/network session처럼 복사가 위험한 타입은 복사를 금지한다.

```cpp
class CGameSessionClient final
{
public:
    CGameSessionClient(const CGameSessionClient&) = delete;
    CGameSessionClient& operator=(const CGameSessionClient&) = delete;
};
```

기존 `NO_COPY`/`DECLARE_SINGLETON` 매크로 사용 파일은 현행 유지한다.

### 4.4 초기화는 선언 지점에서

가능하면 멤버 선언부에서 기본값을 준다.

```cpp
u64_t m_lastServerTick = 0;
bool_t m_bServerLoading = false;
std::unordered_set<u32_t> m_seenNetIDs;
```

생성자에서 다시 같은 값을 반복하지 않는다.

### 4.5 `explicit` 사용

인자 1개 생성자는 암시 변환을 막는다.

```cpp
explicit CReplayPlayer(std::unique_ptr<CReplayStream> pStream);
```

### 4.6 const correctness

상태를 바꾸지 않는 함수는 `const`를 붙인다.

```cpp
EntityID FindNetworkBindCandidate(...) const;
u64_t GetLastAppliedServerTick() const { return m_lastServerTick; }
```

포인터/참조 파라미터:

- 읽기 전용: `const T&`, `const T*`
- 필수 출력: `T& OutValue`
- nullable 출력: `T* pOutValue`

### 4.7 전달 방식

작은 POD:

```cpp
void SetTeam(eTeam team);
void SetNetID(u32_t netID);
```

큰 객체 읽기:

```cpp
void ApplySnapshot(const SnapshotData& snapshot);
```

소유권 이전:

```cpp
void SetRenderer(std::unique_ptr<ModelRenderer> pRenderer);
```

이동이 명확한 경우:

```cpp
m_pRenderer = std::move(pRenderer);
```

### 4.8 C++ 예외 사용 금지

Winters runtime은 HRESULT, bool return, log/assert 기반으로 간다.

- 엔진 hot path에서 C++ exception을 던지지 않는다.
- 실패 가능 함수는 실패 값을 명확히 반환한다.
- HRESULT는 반드시 확인한다.

```cpp
if (FAILED(hr))
{
    OutputDebugStringA("[Renderer] buffer creation failed\n");
    return E_FAIL;
}
```

### 4.9 assert와 runtime validation 구분

UE5의 `check`, `ensure` 감각을 Winters식으로 적용한다.

- 절대 깨지면 안 되는 programmer invariant: assert/check류
- 외부 입력, 네트워크 패킷, 파일 IO 실패: runtime validation + log + return
- 서버 권위 command 검증 실패: reject log + no mutation

네트워크 패킷은 assert로 죽이지 않는다.

```cpp
if (!Shared::Schema::VerifySnapshotBuffer(verifier))
{
    OutputDebugStringA("[SnapshotApplier] invalid Snapshot buffer\n");
    return;
}
```

---

## 5. Header 작성 규칙

### 5.1 기본 구조

```cpp
#pragma once

#include "Defines.h"
#include "ECS/Entity.h"

#include <memory>
#include <vector>

class CWorld;

class CMySystem final
{
public:
    static std::unique_ptr<CMySystem> Create();

    void Tick(CWorld& world, f32_t dt);

private:
    CMySystem() = default;
};
```

### 5.2 public header 금지 항목

- `using namespace std;`
- anonymous namespace
- 큰 구현 함수
- Windows/DX/Assimp/FMOD include 남발
- generated schema include 남발
- private helper struct 노출

### 5.3 include 순서

`.cpp` 권장 순서:

1. 자기 헤더
2. generated/schema 또는 외부 macro push/pop이 필요한 헤더
3. 프로젝트 public/private 헤더
4. Engine/Shared 헤더
5. third-party 헤더
6. STL/Windows

예:

```cpp
#include "Network/Client/SnapshotApplier.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"
#pragma pop_macro("max")
#pragma pop_macro("min")

#include "Shared/GameSim/EntityIdMap.h"
#include "ECS/World.h"

#include <Windows.h>
#include <cstdio>
```

---

## 6. .cpp 구현 조직

UE AI 모듈 확장 규칙처럼, 한 `.cpp`에 여러 타입 구현이 있으면 구분선을 둔다.

```cpp
//----------------------------------------------------------------//
//  CSnapshotApplier
//----------------------------------------------------------------//
```

규칙:

- 같은 타입의 멤버 함수는 흩어놓지 않는다.
- helper → factory → public API → private API 순서를 우선한다.
- 익명 namespace helper가 너무 크면 별도 private helper로 분리한다.
- 긴 함수는 추상화보다 먼저 "단계 이름이 드러나는 작은 helper"로 나눈다.

---

## 7. ECS/DOD 전용 규칙

UE5식 OOP 감각을 가져오더라도 Winters GameSim은 ECS/DOD가 중심이다.

### 7.1 Component

- Component는 가능한 POD/SoA 친화적으로 둔다.
- 가상 함수 금지.
- 리소스 소유 금지.
- 포인터는 가능하면 `EntityID` 또는 `netID`로 대체한다.
- serialization/network 대상 component는 layout 변경을 문서화한다.

좋음:

```cpp
struct BotLaneAIComponent
{
    eBotLaneAIState state = eBotLaneAIState::Spawn;
    EntityID targetMinion = NULL_ENTITY;
    EntityID targetStructure = NULL_ENTITY;
    f32_t decisionCooldown = 0.f;
};
```

피할 것:

```cpp
struct BotLaneAIComponent
{
    virtual void Update();
    std::unique_ptr<BehaviorTree> Tree;
};
```

### 7.2 System

- System은 component를 읽고 쓴다.
- System은 소유권을 숨기지 않는다.
- hot path에서 allocation/log/string 생성 금지.
- deterministic server sim에서는 iteration order를 명시한다.
- command buffer 또는 event queue를 통해 구조 변경을 모은다.

### 7.3 AI

- AI는 서버에서만 판단한다.
- AI는 Transform/HP/Cooldown을 직접 확정하지 않는다.
- AI는 인간 플레이어와 같은 `GameCommand`를 만든다.
- command executor가 검증과 mutation을 담당한다.

---

## 8. Server Authority 규칙

게임 결과를 바꾸는 것은 서버 GameSim이다.

서버 소유:

- 위치
- HP/MP
- cooldown
- damage
- skill hit
- projectile
- tower aggro
- minion AI
- bot AI
- nexus/inhibitor/turret state
- victory/defeat

클라 소유:

- input collection
- command send
- render
- animation playback from server cue
- FX playback from server cue
- UI/debug
- interpolation

금지:

- 네트워크 권위 모드에서 클라 local skill damage 적용
- 네트워크 권위 모드에서 클라 local cooldown 확정
- 네트워크 권위 모드에서 클라 AI 판단으로 world mutation
- server snapshot entity를 클라가 임의로 다른 gameplay entity와 섞는 것

---

## 9. 성능 규칙

UE5식 엔진 코드는 "명확함"과 동시에 hot path 비용을 의식한다.

Hot path 금지:

- 매 tick heap allocation
- 매 snapshot string formatting 폭주
- 매 frame filesystem access
- render/update 중 lazy asset load
- unordered_map lookup 남발이 병목이 되는 구조
- virtual call chain이 깊은 per-entity loop

허용:

- startup/load phase allocation
- debug build 한정 로그
- ring buffer trace
- frame budget이 확인된 cache lookup

서버 GameSim:

- deterministic iteration 우선
- 모든 random은 seed/rng state를 통제
- unordered container iteration 결과에 gameplay가 의존하지 않게 한다

---

## 10. Log / Debug 규칙

UE5의 log category 감각을 Winters에 맞게 적용한다.

원칙:

- 임시 디버그 로그는 prefix를 명확히 붙인다.
- 반복 로그는 throttle/count 제한한다.
- 네트워크 패킷별 로그는 필요한 기간 이후 제거하거나 trace ring buffer로 옮긴다.
- 검토 요청 시 코드 주석의 질문에는 답변하고 해결된 질문 주석은 제거한다.

예:

```cpp
char msg[192]{};
sprintf_s(msg,
    "[SnapshotApplier] structure bind netID=%u entity=%u\n",
    netID,
    static_cast<u32_t>(entity));
OutputDebugStringA(msg);
```

---

## 11. 오류 처리 규칙

### 11.1 외부 입력은 신뢰하지 않는다

- 파일
- 네트워크 packet
- user input
- editor-authored data
- generated asset path

항상 검증한다.

### 11.2 실패 시 side effect 최소화

함수 중간에 실패할 수 있으면, 가능하면 임시 객체에 먼저 구성하고 마지막에 commit한다.

```cpp
auto pRenderer = std::make_unique<ModelRenderer>();
if (!pRenderer->Init(path, L"Shaders/Mesh3D.hlsl"))
    return NULL_ENTITY;

EntityID entity = m_pWorld->CreateEntity();
m_mapRenderers.emplace(entity, std::move(pRenderer));
```

---

## 12. Refactor 규칙

대규모 리팩터는 기능 수정과 분리한다.

허용:

- 손댄 함수 안에서 명확한 이름 개선
- user가 요청한 `Id` -> `ID` 국소 수정
- 컴파일 오류 방지를 위한 최소 signature 정리
- 같은 변경을 이해하기 쉽게 만드는 작은 helper 분리

금지:

- unrelated mass rename
- public schema rename
- generated code 수정
- 기존 user 변경 되돌리기
- network/gameplay semantics와 무관한 파일 churn

---

## 13. Winters 코드 작성 체크리스트

새 `.h/.cpp` 작성 전:

- [ ] 기존 같은 역할의 클래스/시스템을 `rg`로 찾았다.
- [ ] public/private 경계가 맞다.
- [ ] public 헤더가 과도하게 무겁지 않다.
- [ ] include path가 폴더 포함 경로다.
- [ ] ownership이 `unique_ptr`, `ComPtr`, non-owning raw pointer 중 무엇인지 명확하다.
- [ ] class 복사/이동 정책이 명확하다.
- [ ] constructor 1개 인자는 `explicit`이다.
- [ ] 상태 변경 없는 함수는 `const`다.
- [ ] hot path allocation/log가 없다.
- [ ] 네트워크 권위 모드에서 클라 mutation이 없다.
- [ ] `Id` 대신 `ID` 표기를 썼다. 생성 코드/외부 API는 예외다.
- [ ] 질문/임시 주석은 검토 후 제거할 계획이 있다.

---

## 14. 바로 적용할 코드 스타일 예시

### Anonymous namespace helper

```cpp
namespace
{
    constexpr f32_t kStructureBindMaxDistance = 2.5f;

    bool_t TryGetStructureKind(
        Shared::Schema::EntityKind kind,
        Winters::Map::eObjectKind& outKind)
    {
        switch (kind)
        {
        case Shared::Schema::EntityKind::Turret:
            outKind = Winters::Map::eObjectKind::Structure_Turret;
            return true;
        case Shared::Schema::EntityKind::Inhibitor:
            outKind = Winters::Map::eObjectKind::Structure_Inhibitor;
            return true;
        case Shared::Schema::EntityKind::Nexus:
            outKind = Winters::Map::eObjectKind::Structure_Nexus;
            return true;
        default:
            return false;
        }
    }
} // namespace
```

### RAII + commit-last

```cpp
std::unique_ptr<ModelRenderer> pRenderer = std::make_unique<ModelRenderer>();
if (!pRenderer->Init(path, L"Shaders/Mesh3D.hlsl"))
    return NULL_ENTITY;

EntityID entity = world.CreateEntity();
RenderComponent render{};
render.pRenderer = pRenderer.get();
world.AddComponent<RenderComponent>(entity, render);

m_ownedRenderers.emplace(entity, std::move(pRenderer));
```

### Non-owning pointer 명시

```cpp
class CStructure_Manager final
{
public:
    HRESULT Initialize(CWorld* pWorld);

private:
    CWorld* m_pWorld = nullptr; // non-owning, current scene world
};
```

---

## 15. 결론

Winters의 새 코드 기준은 다음 한 문장으로 요약한다.

> UE5처럼 모듈 경계와 구현 숨김을 엄격히 하고, Effective C++처럼 소유권/초기화/const/복사 정책을 명확히 하되, 타입 이름과 ECS/DOD 구조는 Winters 기존 규칙을 유지한다.
