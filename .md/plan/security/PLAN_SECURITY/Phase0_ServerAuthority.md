# Phase 0: 서버 권위 검증 — 구현 계획서

> **목표**: 클라이언트를 신뢰하지 않는 서버 사이드 검증
> **위치**: `Server/Public/AntiCheat/`, `Server/Private/AntiCheat/`
> **필터**: `03. AntiCheat` (Server.vcxproj.filters)
> **의존성**: 없음 (독립적으로 구현 가능)

---

## 파일 목록

| # | 경로 | 설명 |
|---|------|------|
| 1 | `Server/Public/AntiCheat/CServerValidator.h` | 서버 검증 매니저 |
| 2 | `Server/Private/AntiCheat/CServerValidator.cpp` | 구현 |
| 3 | `Server/Public/AntiCheat/CSpeedChecker.h` | 이동 속도 검증 |
| 4 | `Server/Private/AntiCheat/CSpeedChecker.cpp` | 구현 |
| 5 | `Server/Public/AntiCheat/CRangeChecker.h` | 스킬 사거리 검증 |
| 6 | `Server/Private/AntiCheat/CRangeChecker.cpp` | 구현 |
| 7 | `Server/Public/AntiCheat/CCooldownVerifier.h` | 쿨다운 서버 타이머 |
| 8 | `Server/Private/AntiCheat/CCooldownVerifier.cpp` | 구현 |
| 9 | `Server/Public/AntiCheat/CFOWManager.h` | 시야(Fog of War) 관리 |
| 10 | `Server/Private/AntiCheat/CFOWManager.cpp` | 구현 |
| 11 | `Server/Public/AntiCheat/AntiCheatTypes.h` | 공용 타입/열거형 |

---

## 1. AntiCheatTypes.h

**경로**: `Server/Public/AntiCheat/AntiCheatTypes.h`

```cpp
#pragma once
#include <cstdint>

// 치트 탐지 유형
enum class ECheatType : uint32_t
{
    None            = 0,
    SpeedHack       = 1,    // 이동 속도 초과
    Teleport        = 2,    // 비정상 순간이동
    RangeHack       = 3,    // 사거리 밖 스킬 사용
    CooldownHack    = 4,    // 쿨다운 무시
    DamageHack      = 5,    // 비정상 데미지
    FOWViolation    = 6,    // 시야 밖 정보 접근
    InvalidInput    = 7,    // 불가능한 입력 시퀀스
};

// 탐지 심각도
enum class ESeverity : uint8_t
{
    Low      = 1,   // 로그만
    Medium   = 5,   // 경고 + 감시
    High     = 8,   // 즉시 킥 대상
    Critical = 10,  // 즉시 킥 + 밴 큐
};

// 검증 결과
enum class EValidateResult : uint8_t
{
    Valid       = 0,    // 정상
    Corrected   = 1,    // 서버 보정 적용 (약간의 오차)
    Rejected    = 2,    // 거부 (치트 의심)
    Flagged     = 3,    // 누적 감시 대상
};

// 위반 기록
struct ViolationRecord
{
    uint32_t    entityID;
    uint32_t    ownerUserID;
    ECheatType  type;
    ESeverity   severity;
    float32     value;       // 위반 수치 (예: 실제 이동 거리)
    float32     expected;    // 기대 수치 (예: 최대 허용 거리)
    double      timestamp;   // 서버 시간
};
```

---

## 2. CSpeedChecker.h / .cpp

**경로**: `Server/Public/AntiCheat/CSpeedChecker.h`

```cpp
#pragma once
#include "AntiCheatTypes.h"
#include "Core/WintersMath.h"
#include <unordered_map>
#include <memory>

// 이동 속도 검증 — 프레임 간 이동 거리가 물리적으로 가능한지 확인
class CSpeedChecker
{
public:
    ~CSpeedChecker() = default;

    static std::unique_ptr<CSpeedChecker> Create(float32 tolerancePercent = 0.15f);

    // 매 서버 틱마다 호출
    // @param entityID    엔티티 식별자
    // @param newPos      클라이언트가 보낸 새 위치
    // @param maxSpeed    해당 엔티티의 현재 최대 이동 속도
    // @param serverDT    서버 틱 간격 (1/20 = 0.05s @20TPS)
    // @return            검증 결과
    EValidateResult Validate(
        uint32_t entityID,
        const Vec3& newPos,
        float32 maxSpeed,
        float32 serverDT);

    // 서버 보정된 위치 반환 (Rejected/Corrected 시)
    Vec3 GetCorrectedPosition(uint32_t entityID) const;

    // 엔티티 제거 시 호출
    void RemoveEntity(uint32_t entityID);

    // 위반 누적 횟수
    uint32_t GetViolationCount(uint32_t entityID) const;

private:
    CSpeedChecker() = default;

    struct EntityState
    {
        Vec3     lastValidPos   = {0, 0, 0};
        Vec3     correctedPos   = {0, 0, 0};
        uint32_t violationCount = 0;
        double   lastUpdateTime = 0.0;
        bool     initialized    = false;
    };

    float32 m_fTolerance = 0.15f;  // 15% 허용 오차
    std::unordered_map<uint32_t, EntityState> m_States;
};
```

**경로**: `Server/Private/AntiCheat/CSpeedChecker.cpp`

```cpp
#include "AntiCheat/CSpeedChecker.h"

std::unique_ptr<CSpeedChecker> CSpeedChecker::Create(float32 tolerancePercent)
{
    auto checker = std::unique_ptr<CSpeedChecker>(new CSpeedChecker());
    checker->m_fTolerance = tolerancePercent;
    return checker;
}

EValidateResult CSpeedChecker::Validate(
    uint32_t entityID,
    const Vec3& newPos,
    float32 maxSpeed,
    float32 serverDT)
{
    auto& state = m_States[entityID];

    if (!state.initialized)
    {
        state.lastValidPos = newPos;
        state.correctedPos = newPos;
        state.initialized = true;
        return EValidateResult::Valid;
    }

    float32 distance = Vec3::Distance(state.lastValidPos, newPos);
    float32 maxAllowed = maxSpeed * serverDT * (1.0f + m_fTolerance);

    // 텔레포트 급 이동 (5배 초과)
    if (distance > maxAllowed * 5.0f)
    {
        state.violationCount += 5;
        state.correctedPos = state.lastValidPos; // 이전 위치 유지
        return EValidateResult::Rejected;
    }

    // 속도 초과 (허용 범위 밖)
    if (distance > maxAllowed)
    {
        state.violationCount++;

        // 보정: 허용 최대 거리만큼만 이동
        Vec3 dir = Vec3::Normalize(newPos - state.lastValidPos);
        state.correctedPos = state.lastValidPos + dir * maxAllowed;
        state.lastValidPos = state.correctedPos;

        if (state.violationCount > 10)
            return EValidateResult::Flagged;

        return EValidateResult::Corrected;
    }

    // 정상
    state.lastValidPos = newPos;
    state.correctedPos = newPos;
    state.violationCount = (state.violationCount > 0) ?
        state.violationCount - 1 : 0; // 정상이면 카운터 감소
    return EValidateResult::Valid;
}

Vec3 CSpeedChecker::GetCorrectedPosition(uint32_t entityID) const
{
    auto it = m_States.find(entityID);
    if (it == m_States.end()) return {0, 0, 0};
    return it->second.correctedPos;
}

void CSpeedChecker::RemoveEntity(uint32_t entityID)
{
    m_States.erase(entityID);
}

uint32_t CSpeedChecker::GetViolationCount(uint32_t entityID) const
{
    auto it = m_States.find(entityID);
    if (it == m_States.end()) return 0;
    return it->second.violationCount;
}
```

---

## 3. CRangeChecker.h / .cpp

**경로**: `Server/Public/AntiCheat/CRangeChecker.h`

```cpp
#pragma once
#include "AntiCheatTypes.h"
#include "Core/WintersMath.h"
#include <memory>

// 스킬 사거리 검증
class CRangeChecker
{
public:
    ~CRangeChecker() = default;

    static std::unique_ptr<CRangeChecker> Create(float32 tolerancePercent = 0.05f);

    // 스킬 사용 시 사거리 검증
    EValidateResult ValidateSkillRange(
        const Vec3& casterPos,
        const Vec3& targetPos,
        float32 maxRange);

    // 기본 공격 사거리 검증
    EValidateResult ValidateAttackRange(
        const Vec3& attackerPos,
        const Vec3& targetPos,
        float32 attackRange);

private:
    CRangeChecker() = default;
    float32 m_fTolerance = 0.05f;
};
```

**경로**: `Server/Private/AntiCheat/CRangeChecker.cpp`

```cpp
#include "AntiCheat/CRangeChecker.h"

std::unique_ptr<CRangeChecker> CRangeChecker::Create(float32 tolerancePercent)
{
    auto checker = std::unique_ptr<CRangeChecker>(new CRangeChecker());
    checker->m_fTolerance = tolerancePercent;
    return checker;
}

EValidateResult CRangeChecker::ValidateSkillRange(
    const Vec3& casterPos,
    const Vec3& targetPos,
    float32 maxRange)
{
    float32 dist = Vec3::Distance(casterPos, targetPos);
    float32 allowed = maxRange * (1.0f + m_fTolerance);

    if (dist > allowed * 2.0f)
        return EValidateResult::Rejected;  // 명백한 치트

    if (dist > allowed)
        return EValidateResult::Corrected; // 약간 초과 (네트워크 지연)

    return EValidateResult::Valid;
}

EValidateResult CRangeChecker::ValidateAttackRange(
    const Vec3& attackerPos,
    const Vec3& targetPos,
    float32 attackRange)
{
    return ValidateSkillRange(attackerPos, targetPos, attackRange);
}
```

---

## 4. CCooldownVerifier.h / .cpp

**경로**: `Server/Public/AntiCheat/CCooldownVerifier.h`

```cpp
#pragma once
#include "AntiCheatTypes.h"
#include <unordered_map>
#include <memory>
#include <utility>

// 쿨다운 서버 타이머 — 서버가 직접 쿨다운을 관리
class CCooldownVerifier
{
public:
    ~CCooldownVerifier() = default;

    static std::unique_ptr<CCooldownVerifier> Create();

    // 스킬 사용 시 쿨다운 시작
    void StartCooldown(uint32_t entityID, uint32_t skillSlot, float32 duration);

    // 스킬 사용 가능 여부 검증
    EValidateResult ValidateUse(uint32_t entityID, uint32_t skillSlot, double serverTime);

    // 쿨다운 잔여 시간
    float32 GetRemaining(uint32_t entityID, uint32_t skillSlot, double serverTime) const;

    // 엔티티 제거
    void RemoveEntity(uint32_t entityID);

private:
    CCooldownVerifier() = default;

    using CooldownKey = std::pair<uint32_t, uint32_t>; // entityID, skillSlot

    struct PairHash
    {
        size_t operator()(const CooldownKey& k) const
        {
            return std::hash<uint64_t>()(
                (static_cast<uint64_t>(k.first) << 32) | k.second);
        }
    };

    // key → 쿨다운 만료 서버 시간
    std::unordered_map<CooldownKey, double, PairHash> m_Cooldowns;
};
```

**경로**: `Server/Private/AntiCheat/CCooldownVerifier.cpp`

```cpp
#include "AntiCheat/CCooldownVerifier.h"

std::unique_ptr<CCooldownVerifier> CCooldownVerifier::Create()
{
    return std::unique_ptr<CCooldownVerifier>(new CCooldownVerifier());
}

void CCooldownVerifier::StartCooldown(
    uint32_t entityID, uint32_t skillSlot, float32 duration)
{
    // 현재 서버 시간은 호출자가 관리
    // 여기서는 만료 시간만 기록
    // 실제 구현 시 serverTime 인자 추가 필요
}

EValidateResult CCooldownVerifier::ValidateUse(
    uint32_t entityID, uint32_t skillSlot, double serverTime)
{
    CooldownKey key = {entityID, skillSlot};
    auto it = m_Cooldowns.find(key);

    if (it == m_Cooldowns.end())
        return EValidateResult::Valid; // 쿨다운 기록 없음 = 사용 가능

    if (serverTime < it->second)
        return EValidateResult::Rejected; // 아직 쿨다운 중!

    return EValidateResult::Valid;
}

float32 CCooldownVerifier::GetRemaining(
    uint32_t entityID, uint32_t skillSlot, double serverTime) const
{
    CooldownKey key = {entityID, skillSlot};
    auto it = m_Cooldowns.find(key);

    if (it == m_Cooldowns.end()) return 0.0f;

    double remaining = it->second - serverTime;
    return (remaining > 0.0) ? static_cast<float32>(remaining) : 0.0f;
}

void CCooldownVerifier::RemoveEntity(uint32_t entityID)
{
    for (auto it = m_Cooldowns.begin(); it != m_Cooldowns.end(); )
    {
        if (it->first.first == entityID)
            it = m_Cooldowns.erase(it);
        else
            ++it;
    }
}
```

---

## 5. CFOWManager.h / .cpp

**경로**: `Server/Public/AntiCheat/CFOWManager.h`

```cpp
#pragma once
#include "AntiCheatTypes.h"
#include "Core/WintersMath.h"
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>

// 시야(Fog of War) 관리 — 클라이언트에 보낼 엔티티 필터링
// 시야 밖 적 정보를 전송하지 않으면 월핵/맵핵이 근본적으로 불가능
class CFOWManager
{
public:
    ~CFOWManager() = default;

    static std::unique_ptr<CFOWManager> Create(
        float32 defaultVisionRange = 1200.0f,
        float32 gridCellSize = 200.0f);

    // 엔티티 위치 갱신
    void UpdatePosition(uint32_t entityID, uint8_t team, const Vec3& pos);

    // 엔티티의 시야 범위 설정 (챔피언, 와드 등 다를 수 있음)
    void SetVisionRange(uint32_t entityID, float32 range);

    // 특정 관찰자가 대상을 볼 수 있는지 판단
    bool IsVisible(uint32_t observerID, uint32_t targetID) const;

    // 특정 팀에서 대상을 볼 수 있는지 판단
    bool IsVisibleToTeam(uint8_t team, uint32_t targetID) const;

    // 특정 관찰자에게 전송할 엔티티 목록 (네트워크 최적화)
    std::vector<uint32_t> GetVisibleEntities(uint32_t observerID) const;

    void RemoveEntity(uint32_t entityID);

private:
    CFOWManager() = default;

    struct EntityInfo
    {
        Vec3     position     = {0, 0, 0};
        float32  visionRange  = 1200.0f;
        uint8_t  team         = 0;
    };

    float32 m_fDefaultRange = 1200.0f;
    float32 m_fGridCellSize = 200.0f;
    std::unordered_map<uint32_t, EntityInfo> m_Entities;
};
```

**경로**: `Server/Private/AntiCheat/CFOWManager.cpp`

```cpp
#include "AntiCheat/CFOWManager.h"

std::unique_ptr<CFOWManager> CFOWManager::Create(
    float32 defaultVisionRange, float32 gridCellSize)
{
    auto mgr = std::unique_ptr<CFOWManager>(new CFOWManager());
    mgr->m_fDefaultRange = defaultVisionRange;
    mgr->m_fGridCellSize = gridCellSize;
    return mgr;
}

void CFOWManager::UpdatePosition(uint32_t entityID, uint8_t team, const Vec3& pos)
{
    auto& info = m_Entities[entityID];
    info.position = pos;
    info.team = team;
    if (info.visionRange == 0.0f)
        info.visionRange = m_fDefaultRange;
}

void CFOWManager::SetVisionRange(uint32_t entityID, float32 range)
{
    m_Entities[entityID].visionRange = range;
}

bool CFOWManager::IsVisible(uint32_t observerID, uint32_t targetID) const
{
    auto obsIt = m_Entities.find(observerID);
    auto tgtIt = m_Entities.find(targetID);
    if (obsIt == m_Entities.end() || tgtIt == m_Entities.end())
        return false;

    const auto& obs = obsIt->second;
    const auto& tgt = tgtIt->second;

    // 같은 팀이면 항상 보임
    if (obs.team == tgt.team)
        return true;

    // 거리 기반 시야 판정
    float32 dist = Vec3::Distance(obs.position, tgt.position);
    return dist <= obs.visionRange;
}

bool CFOWManager::IsVisibleToTeam(uint8_t team, uint32_t targetID) const
{
    auto tgtIt = m_Entities.find(targetID);
    if (tgtIt == m_Entities.end()) return false;

    // 같은 팀이면 항상 보임
    if (tgtIt->second.team == team) return true;

    // 해당 팀의 아군 중 하나라도 대상을 볼 수 있으면 true
    for (const auto& [eid, info] : m_Entities)
    {
        if (info.team != team) continue;

        float32 dist = Vec3::Distance(info.position, tgtIt->second.position);
        if (dist <= info.visionRange)
            return true;
    }
    return false;
}

std::vector<uint32_t> CFOWManager::GetVisibleEntities(uint32_t observerID) const
{
    std::vector<uint32_t> result;
    auto obsIt = m_Entities.find(observerID);
    if (obsIt == m_Entities.end()) return result;

    uint8_t observerTeam = obsIt->second.team;

    for (const auto& [eid, info] : m_Entities)
    {
        if (eid == observerID) continue;

        // 같은 팀은 항상 보임
        if (info.team == observerTeam)
        {
            result.push_back(eid);
            continue;
        }

        // 적은 팀 시야 확인
        if (IsVisibleToTeam(observerTeam, eid))
        {
            result.push_back(eid);
        }
    }
    return result;
}

void CFOWManager::RemoveEntity(uint32_t entityID)
{
    m_Entities.erase(entityID);
}
```

---

## 6. CServerValidator.h / .cpp (통합 매니저)

**경로**: `Server/Public/AntiCheat/CServerValidator.h`

```cpp
#pragma once
#include "AntiCheatTypes.h"
#include "AntiCheat/CSpeedChecker.h"
#include "AntiCheat/CRangeChecker.h"
#include "AntiCheat/CCooldownVerifier.h"
#include "AntiCheat/CFOWManager.h"
#include <memory>
#include <vector>
#include <functional>

// 서버 안티치트 통합 매니저
class CServerValidator
{
public:
    ~CServerValidator() = default;

    static std::unique_ptr<CServerValidator> Create();

    // 서브시스템 접근
    CSpeedChecker*      GetSpeedChecker()      { return m_pSpeed.get(); }
    CRangeChecker*      GetRangeChecker()      { return m_pRange.get(); }
    CCooldownVerifier*  GetCooldownVerifier()  { return m_pCooldown.get(); }
    CFOWManager*        GetFOWManager()        { return m_pFOW.get(); }

    // 위반 콜백 등록 (위반 발생 시 호출)
    using ViolationCallback = std::function<void(const ViolationRecord&)>;
    void SetViolationCallback(ViolationCallback cb) { m_ViolationCB = std::move(cb); }

    // 위반 기록
    void RecordViolation(const ViolationRecord& record);

    // 엔티티 제거 (모든 서브시스템에서)
    void RemoveEntity(uint32_t entityID);

private:
    CServerValidator() = default;

    std::unique_ptr<CSpeedChecker>     m_pSpeed;
    std::unique_ptr<CRangeChecker>     m_pRange;
    std::unique_ptr<CCooldownVerifier> m_pCooldown;
    std::unique_ptr<CFOWManager>       m_pFOW;
    ViolationCallback                  m_ViolationCB;
};
```

**경로**: `Server/Private/AntiCheat/CServerValidator.cpp`

```cpp
#include "AntiCheat/CServerValidator.h"

std::unique_ptr<CServerValidator> CServerValidator::Create()
{
    auto validator = std::unique_ptr<CServerValidator>(new CServerValidator());
    validator->m_pSpeed    = CSpeedChecker::Create(0.15f);
    validator->m_pRange    = CRangeChecker::Create(0.05f);
    validator->m_pCooldown = CCooldownVerifier::Create();
    validator->m_pFOW      = CFOWManager::Create(1200.0f, 200.0f);
    return validator;
}

void CServerValidator::RecordViolation(const ViolationRecord& record)
{
    if (m_ViolationCB)
        m_ViolationCB(record);
}

void CServerValidator::RemoveEntity(uint32_t entityID)
{
    m_pSpeed->RemoveEntity(entityID);
    m_pCooldown->RemoveEntity(entityID);
    m_pFOW->RemoveEntity(entityID);
}
```

---

## vcxproj.filters 추가 (Server)

```xml
<!-- Server.vcxproj.filters에 추가 -->
<Filter Include="03. AntiCheat">
  <UniqueIdentifier>{AC000001-0000-0000-0000-000000000001}</UniqueIdentifier>
</Filter>

<!-- 헤더 파일 -->
<ClInclude Include="..\Public\AntiCheat\AntiCheatTypes.h">
  <Filter>03. AntiCheat</Filter>
</ClInclude>
<ClInclude Include="..\Public\AntiCheat\CServerValidator.h">
  <Filter>03. AntiCheat</Filter>
</ClInclude>
<ClInclude Include="..\Public\AntiCheat\CSpeedChecker.h">
  <Filter>03. AntiCheat</Filter>
</ClInclude>
<ClInclude Include="..\Public\AntiCheat\CRangeChecker.h">
  <Filter>03. AntiCheat</Filter>
</ClInclude>
<ClInclude Include="..\Public\AntiCheat\CCooldownVerifier.h">
  <Filter>03. AntiCheat</Filter>
</ClInclude>
<ClInclude Include="..\Public\AntiCheat\CFOWManager.h">
  <Filter>03. AntiCheat</Filter>
</ClInclude>

<!-- 소스 파일 -->
<ClCompile Include="..\Private\AntiCheat\CServerValidator.cpp">
  <Filter>03. AntiCheat</Filter>
</ClCompile>
<ClCompile Include="..\Private\AntiCheat\CSpeedChecker.cpp">
  <Filter>03. AntiCheat</Filter>
</ClCompile>
<ClCompile Include="..\Private\AntiCheat\CRangeChecker.cpp">
  <Filter>03. AntiCheat</Filter>
</ClCompile>
<ClCompile Include="..\Private\AntiCheat\CCooldownVerifier.cpp">
  <Filter>03. AntiCheat</Filter>
</ClCompile>
<ClCompile Include="..\Private\AntiCheat\CFOWManager.cpp">
  <Filter>03. AntiCheat</Filter>
</ClCompile>
```

---

## Verification

```
[ ] CSpeedChecker: 정상 이동 (maxSpeed=300, dt=0.05) → Valid
[ ] CSpeedChecker: 2배 속도 이동 → Corrected, 보정 위치 반환
[ ] CSpeedChecker: 10배 속도 이동(텔레포트) → Rejected
[ ] CSpeedChecker: 10회 연속 위반 → Flagged
[ ] CRangeChecker: 사거리 800, 거리 750 → Valid
[ ] CRangeChecker: 사거리 800, 거리 850 → Corrected (5% 허용)
[ ] CRangeChecker: 사거리 800, 거리 2000 → Rejected
[ ] CCooldownVerifier: 쿨다운 8초, 3초 후 재사용 → Rejected
[ ] CCooldownVerifier: 쿨다운 8초, 9초 후 재사용 → Valid
[ ] CFOWManager: 같은 팀 → 항상 Visible
[ ] CFOWManager: 적 팀, 시야 범위 내 → Visible
[ ] CFOWManager: 적 팀, 시야 범위 밖 → NOT Visible → 패킷 미전송
[ ] GetVisibleEntities: 적 팀 중 보이는 것만 반환
```
