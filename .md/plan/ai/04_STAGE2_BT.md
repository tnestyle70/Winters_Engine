# Stage 2 — Behavior Tree (BT)

## 목표

HFSM 의 "현재 뭐 하는 중" 과 달리 BT 는 "**지금 이 상황에서 어떻게 반응할지**"를 트리 순회로
결정. 교전/스킬 콤보/회피 판단에 최적.

## 왜 BT 인가

- HFSM 은 상태 폭발 위험 (전이 수 = 상태² 최악)
- BT 는 **재사용 가능한 서브트리** 조합으로 복잡도 제어
- 기획자/아티스트가 `.bt` 에디터로 수정 가능 (Winters 에선 ImGui BT 에디터 계획)
- Unreal/Unity 업계 표준

## 노드 종류

### 컴포지트 (자식 여러 개)

```cpp
enum class BTCompositeType : u8_t {
    Selector,      // 자식 중 하나라도 성공하면 성공 (OR)
    Sequence,      // 모든 자식 성공해야 성공 (AND)
    Parallel,      // 모든 자식 동시 실행
    RandomSelector // 랜덤 가중치로 자식 선택
};
```

### 데코레이터 (자식 하나)

```cpp
enum class BTDecoratorType : u8_t {
    Inverter,       // 성공 ↔ 실패 뒤집기
    Repeater,       // N회 반복
    UntilSuccess,
    UntilFailure,
    Cooldown,       // 재사용 대기 시간
    TimeLimit       // 제한 시간 내에만 실행
};
```

### 리프 (액션/조건)

- **Condition**: `IsInRange(target, range)`, `HasMana(amount)`, `IsCooldownReady(Q)`, `HealthBelow(0.5)`
- **Action**: `MoveTo(pos)`, `CastAbility(Q, target)`, `BasicAttack(target)`, `UseItem(id)`

## 실행 규칙

각 노드는 `Status` 반환: `Running` / `Success` / `Failure`.

```cpp
enum class BTStatus : u8_t { Running, Success, Failure };

class BTNode
{
public:
    virtual BTStatus Tick(BTContext& ctx) = 0;
    virtual void OnEnter(BTContext& ctx) {}
    virtual void OnExit(BTContext& ctx, BTStatus result) {}
};
```

`Running` 을 반환하면 다음 프레임에 동일 노드 재Tick. Sequence 중간에 Running 있으면 위치 저장.

## 예시 트리 — Irelia 교전

```
AttackChampion (Selector)
├── EmergencyEscape (Sequence)
│   ├── HealthBelow(0.25)
│   ├── HasSummoner(Flash)
│   └── FlashAwayFromTarget
│
├── FullCombo (Sequence)                     ← 처형각
│   ├── Condition: TargetHealthBelow(0.4)
│   ├── Condition: ManaAbove(50%)
│   ├── Condition: QStackReady(3)
│   ├── CastQ (mark target)
│   ├── CastE (stun if 2 marks)
│   ├── CastR (damage amp)
│   ├── CastQ (reset dash)
│   └── BasicAttackUntilDead
│
├── PokeAndRetreat (Sequence)                ← 견제
│   ├── Condition: InRange(QDash)
│   ├── CastQOnMinionNearTarget            ← 미니언 타고 Q
│   └── AutoAttackOnce
│
└── RepositionSafely (Action)                ← 폴백: 안전 포지션
```

## `.bt` 파일 포맷 (JSON 기반 초안)

```json
{
  "name": "Irelia_AttackChampion",
  "root": {
    "type": "Selector",
    "children": [
      {
        "type": "Sequence",
        "name": "EmergencyEscape",
        "children": [
          { "type": "Condition", "id": "HealthBelow", "args": { "ratio": 0.25 } },
          { "type": "Condition", "id": "HasSummoner", "args": { "spell": "Flash" } },
          { "type": "Action",    "id": "FlashAwayFromTarget" }
        ]
      },
      { "type": "SubTree", "ref": "FullCombo" },
      { "type": "SubTree", "ref": "PokeAndRetreat" },
      { "type": "Action",  "id": "RepositionSafely" }
    ]
  }
}
```

FlatBuffers 로 바이너리 컴파일해서 런타임 로드 (Phase 0 에셋 파이프라인 연동).

## 구현 스케치

```cpp
// Engine/Public/AI/BehaviorTree/BTInterpreter.h
class CBTInterpreter
{
public:
    static unique_ptr<CBTInterpreter> Create(const string& path);

    BTStatus Tick(BTContext& ctx);

    void Reset();

private:
    unique_ptr<BTNode> m_root;
    std::vector<u32_t> m_runningStack;   // Sequence/Parallel Running 저장
};

// Engine/Public/AI/BehaviorTree/BTContext.h
struct BTContext
{
    CWorld*      pWorld;
    EntityID     self;
    EntityID     currentTarget;
    Blackboard*  pTeamBlackboard;
    f32_t        dt;
    
    // 각 노드가 쓰는 일시적 상태
    std::unordered_map<u32_t, std::any> nodeLocal;
};
```

## 조건/액션 노드 라이브러리

Winters AI 기본 노드 세트 (약 50개 예상):

### 조건 (20+)

| 이름 | 파라미터 | 설명 |
|---|---|---|
| `IsInRange` | targetRef, range | 대상과 거리 비교 |
| `HealthBelow` | ratio | HP 비율 이하 |
| `ManaAbove` | ratio | 마나 비율 이상 |
| `HasCooldownReady` | skillSlot | 스킬 사용 가능 |
| `HasSummoner` | spellID | 소환사 주문 보유 |
| `EnemyCountNearby` | radius, ge | 반경 내 적 수 비교 |
| `AllyCountNearby` | radius, ge | 반경 내 아군 수 비교 |
| `IsTargetCCd` | targetRef | 대상 CC 상태 |
| `IsInFogOfWar` | — | 적 시야 밖 |
| `IsWaveCloseEnough` | maxDist | 미니언 웨이브 근처 |

### 액션 (30+)

| 이름 | 설명 |
|---|---|
| `MoveToTarget` | 대상 쪽 이동 (사거리 맞춤) |
| `MoveToPosition` | 좌표로 이동 |
| `MoveToCell` | Influence Map 셀 |
| `BasicAttack` | 평타 시전 |
| `CastAbility` | 지정 스킬 시전 (Q/W/E/R) |
| `CastOnTarget` | 특정 대상에 스킬 |
| `CastOnPosition` | 특정 좌표에 스킬 |
| `UseItem` | 아이템 사용 |
| `Recall` | 귀환 채널링 시작 |
| `BuyItem` | 상점 도착 시 구매 |
| `PlaceWard` | 와드 설치 |
| `Kite` | 평타 후 백스텝 반복 |
| `Flash` | 플래시 시전 |

## 챔피언별 트리 저장소

```
Client/Bin/Resource/AI/BT/
├── Irelia.bt           교전 로직
├── Irelia_Lane.bt      라인전 로직
├── Yasuo.bt
├── Yasuo_Lane.bt
├── Sylas.bt
├── Viego.bt
└── Kalista.bt
```

HFSM 이 현재 루트 상태에 맞는 `.bt` 를 로드하여 Tick.

## ImGui BT 디버거

- 트리 노드 계층 시각화
- 현재 Running 노드 하이라이트
- 최근 순회 경로 빨간색
- 조건 노드 결과 실시간 (O/X)
- Tick 속도 조절 (느리게 디버깅)

## 구현 순서

1. `BTNode` 추상 + Composite/Decorator/Leaf 베이스
2. 조건/액션 기본 20개 구현
3. `.bt` JSON 파서 (간이 nlohmann/json 이나 직접 파서)
4. `CBTInterpreter::Tick` 엔진
5. `Irelia.bt` 샘플 트리 1개 먼저 완성 → Stage 4 Utility 연동
6. ImGui 디버거
7. 나머지 4명 챔피언 트리 확장
