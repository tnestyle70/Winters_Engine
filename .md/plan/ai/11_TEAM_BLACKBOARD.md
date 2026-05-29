# 공통 — 팀 협동 (Blackboard)

## 목표

봇 5명이 **공유 메모리**를 통해 역할 분담·조율. 사람 플레이어와 팀일 때는 핑 시스템으로만.

## Blackboard 패턴

Game AI Pro 고전 기법. 공유 key-value 저장소에 각 봇이 읽기/쓰기.

```cpp
// Engine/Public/AI/Blackboard/Blackboard.h
class CBlackboard
{
public:
    template<typename T> void  Set(const std::string& key, const T& value);
    template<typename T> T     Get(const std::string& key, const T& defaultVal) const;
    bool_t Has(const std::string& key) const;
    void   Remove(const std::string& key);

    // 봇 등록/해제
    void RegisterBot(EntityID bot);
    void UnregisterBot(EntityID bot);
    const std::vector<EntityID>& GetBots() const;

private:
    std::unordered_map<std::string, std::any> m_data;
    std::vector<EntityID>                     m_bots;
    mutable std::mutex                        m_mutex;   // 병렬 접근
};
```

팀당 하나의 Blackboard 인스턴스 (블루팀 / 레드팀).

## 표준 키 목록

| Key | 타입 | 의미 |
|---|---|---|
| `CurrentObjective` | enum | Dragon/Baron/Pushing/Farming/Defending |
| `ObjectivePriority` | f32 | 0~1, 팀의 목표 집중도 |
| `GroupLeader` | EntityID | 현재 팀 리더 (정글이 기본) |
| `GroupingAt` | Vec3 | 모이기 좌표 |
| `FlaggedEnemy` | EntityID | 우선 공격 대상 |
| `EnemyBaronWarning` | bool | 적이 바론 시도 중 |
| `SafeRetreatPos` | Vec3 | 후퇴 집결 좌표 |
| `WardPriorities` | vector<Vec3> | 와드 놓을 후보 위치 |
| `LastPingTime` | f32 | 마지막 핑 시간 (스팸 방지) |
| `RoleAssignment` | map<EntityID, Role> | 탑솔/정글/미드/원딜/서폿 |
| `GoldLead` | f32 | 팀 골드 차이 (상대 팀 대비) |
| `MissingEnemies` | vector<EntityID> | 시야에서 사라진 적 목록 |
| `LastKnownPositions` | map<EntityID, Vec3> | 적 마지막 목격 위치 |

## 역할 할당 (Role Assignment)

매치 시작 시 자동:

```cpp
void AssignRoles(CBlackboard& bb, const std::vector<EntityID>& bots)
{
    std::map<EntityID, Role> roles;

    // 챔피언 타입 기반 기본 할당
    for (auto bot : bots) {
        auto ch = GetChampion(bot);
        if (ch == ChampionID::Yasuo || ch == ChampionID::Sylas) roles[bot] = Role::Mid;
        else if (ch == ChampionID::Viego)                       roles[bot] = Role::Jungle;
        else if (ch == ChampionID::Kalista)                     roles[bot] = Role::ADC;
        else if (ch == ChampionID::Irelia)                      roles[bot] = Role::Top;
        // ...
    }

    // 충돌 해결 (같은 역할 2명 → 하나는 Support 로)
    ResolveConflicts(roles);

    bb.Set("RoleAssignment", roles);
}
```

연습모드에선 ImGui 에서 수동 지정 가능.

## 목표 전파 (Objective Broadcast)

한 봇이 "드래곤 가자" 제안 → Blackboard 에 후보로 쌓임 → 과반 동의 시 확정.

```cpp
class CObjectiveDecider
{
public:
    void ProposeObjective(CBlackboard& bb, EntityID proposer, ObjectiveType type, f32_t score);

    // 매 0.5초 호출 — 제안들 모아서 결정
    void FinalizeObjective(CBlackboard& bb);

private:
    struct Proposal {
        EntityID  proposer;
        ObjectiveType type;
        f32_t     score;
        f32_t     proposedAt;
    };
    std::vector<Proposal> m_pendingProposals;
};
```

## 사람 플레이어와 혼합 팀

사람이 블랙보드를 읽으면 안 됨 (치트). 봇팀이 사람과 섞일 때:

- 봇들끼리만 Blackboard 공유
- 사람 → 봇 통신은 **게임 내 핑 시스템** 만 사용
- 봇이 사람의 핑을 `Blackboard.ExternalPings` 로 기록 → 의사결정 입력

```cpp
struct ExternalPing {
    EntityID  from;       // 사람 플레이어
    Vec3      position;
    u32_t     type;       // Danger/OnMyWay/Missing/Gank/Assist
    f32_t     time;
};
```

## 적 팀 블랙보드 (시야 기반 제한)

봇팀은 적을 **시야에 들어온 정보만** 관찰.

```cpp
class CEnemyObservation
{
public:
    void Update(CWorld& world, Team myTeam);

    // 적 마지막 목격 위치 (fog 들어가도 N 초 유지)
    std::optional<Vec3> GetLastKnown(EntityID enemy) const;

    // 적 아이템 빌드 (상점에 들를 때 추정)
    std::vector<u32_t> InferItems(EntityID enemy) const;

private:
    std::map<EntityID, Vec3>    m_lastPositions;
    std::map<EntityID, f32_t>   m_lastSeenTime;
};
```

## ECS 통합

```cpp
// 각 팀당 싱글턴 Resource
struct TeamBlackboardResource
{
    CBlackboard blueTeam;
    CBlackboard redTeam;
};

class CBlackboardSystem : public ISystem
{
public:
    void Execute(CWorld& world, f32_t dt) override
    {
        auto& res = world.GetResource<TeamBlackboardResource>();
        UpdateBlackboard(res.blueTeam, world, Team::Blue);
        UpdateBlackboard(res.redTeam, world, Team::Red);
    }
};
```

## 디버깅

ImGui 창에 현재 Blackboard 덤프:

```
[Team Blackboard — Blue]
  CurrentObjective      : Dragon (priority: 0.82)
  GroupingAt            : (45.2, 0, 12.1)
  GroupLeader           : #3 Viego (Jungle)
  FlaggedEnemy          : #7 Yasuo (enemy mid)
  MissingEnemies        : [Kalista, Sylas]
  LastKnownPositions    :
    Kalista             : (82.1, 0, 35.4) @ 12s ago
    Sylas               : (15.3, 0, 48.2) @ 8s ago
  GoldLead              : +850
  RoleAssignment        :
    #1 Irelia           : Top
    #2 Sylas            : Mid
    #3 Viego            : Jungle
    #4 Kalista          : ADC
    #5 Yasuo            : Support
```

## 구현 순서

1. `CBlackboard` 기본 Get/Set/Has (thread-safe)
2. 역할 할당 (매치 시작 시)
3. 적 관찰 시스템 (`CEnemyObservation`)
4. 목표 결정 투표 (`CObjectiveDecider`)
5. 표준 키 API 래퍼 (타입 안전성)
6. ImGui 디버거
7. 핑 시스템 통합 (사람-봇 혼합 팀)
