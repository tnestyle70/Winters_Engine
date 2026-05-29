# Stage 0 — 단순 Aggro (정글몹/미니언)

## 목표

봇이 아닌 **중립 몬스터·미니언**의 기본 행동. 가장 가벼운 AI. Phase F 시작점.

## 적용 대상

- 정글 몬스터: Baron, Dragon, Blue, Red, Krug, Gromp, Wolf, Scuttle (전령)
- 미니언: Order_Melee/Ranged/Siege/Super, Chaos_Melee/Ranged/Siege/Super
- 포탑 (공격 어그로 — 풀 AI 아님)

## 설계

### 어그로 우선순위 (평가 점수 내림차순)

```
Priority = w1 * (1 / distance)
         + w2 * (damageReceivedFromTarget / totalDamageReceived)
         + w3 * (isLastAttacker ? 1 : 0)
         + w4 * (targetHP_percent_low)
         - w5 * (isMinion ? 0.1 : 0)    // 챔피언 우선
```

가중치는 몹별 차등:
- **정글몹**: `w4` (낮은 체력) 높음 — 각개격파
- **미니언**: `w3` (마지막 공격자) 높음 — 어그로 끌린 상대 집중
- **포탑**: 챔피언 < 미니언 < 어그로 잡힌 챔피언

### 상태 (간단 FSM)

```
Idle  →  Alerted  →  Attacking  →  Returning  →  Idle
                         │
                         └→ Dead
```

- **Idle**: 스폰 지점 주변 순찰 또는 정지
- **Alerted**: 어그로 대상 발견, 접근 중
- **Attacking**: 사거리 내, 평타/스킬
- **Returning**: 리쉬 반경 초과 → 스폰 지점 복귀 (이동 중 무적 + HP 회복)
- **Dead**: 사망 상태 (일정 시간 후 리스폰)

### 스킬 시스템 (정글몹 특성)

| 몹 | 특수 행동 |
|---|---|
| Baron | 범위 침묵 + 원거리 표식 + 넉백. HP 50%/25% 페이즈 |
| Dragon | 브레스 (종족별 고유 효과) |
| Blue | 힘의 기운 버프 → 처치자에게 마나 회복 버프 |
| Red | 감속 평타 → 처치자에게 도트 감속 버프 |
| Krug | 보스 → 2마리 Medium → 각각 2마리 Mini 로 분열 |
| Gromp | 독 구름 (근접 채널러 카운터) |
| Wolf | 늑대장 + 늑대새끼 2마리 (소형 어그로 공유) |

각 몹마다 `MonsterBehavior` 컴포넌트 + 전용 로직 함수.

## ECS 구현 스케치

```cpp
// Engine/Public/AI/Core/MonsterComponent.h
struct MonsterComponent {
    enum class State : u8_t { Idle, Alerted, Attacking, Returning, Dead };
    State       state = State::Idle;
    EntityID    aggroTarget = INVALID_ENTITY;
    Vec3        spawnPos;
    f32_t       leashRadius = 12.f;   // 유닛 = 맵 스케일 기준
    f32_t       aggroRadius = 8.f;
    f32_t       hp, maxHP;
    f32_t       respawnTimer = 0.f;
    u32_t       monsterType; // Baron/Dragon/Blue/...
};

// Engine/Public/AI/Systems/MonsterSystem.h
class CMonsterSystem : public ISystem
{
public:
    void Execute(CWorld& world, f32_t dt) override
    {
        world.ForEach<MonsterComponent, TransformComponent>(
            [&](EntityID e, MonsterComponent& m, TransformComponent& t)
        {
            switch (m.state) {
                case MonsterComponent::State::Idle:      UpdateIdle(world, e, m, t, dt); break;
                case MonsterComponent::State::Alerted:   UpdateAlerted(world, e, m, t, dt); break;
                case MonsterComponent::State::Attacking: UpdateAttacking(world, e, m, t, dt); break;
                case MonsterComponent::State::Returning: UpdateReturning(world, e, m, t, dt); break;
                case MonsterComponent::State::Dead:      UpdateDead(world, e, m, t, dt); break;
            }
        });
    }

private:
    // 가장 높은 우선순위의 어그로 대상 선정
    EntityID FindAggroTarget(CWorld& world, EntityID self, const Vec3& selfPos, f32_t radius);
    
    // 리쉬 반경 체크
    bool_t IsOutsideLeash(const Vec3& spawn, const Vec3& current, f32_t radius);
};
```

## 리스폰 타이머 (봇전/연습모드 차이)

| 몬스터 | 첫 스폰 | 리스폰 간격 |
|---|---|---|
| Blue/Red | 1:30 | 5:00 |
| Krug/Gromp/Wolf | 1:30 | 2:15 |
| Dragon | 5:00 | 5:00 (소형) / 6:00 (영겁) |
| Baron | 20:00 | 6:00 |
| Scuttle | 3:15 | 2:30 |

`CMonsterSpawner` 엔티티가 타이머 관리. 연습모드에선 ImGui 에서 즉시 스폰/리스폰 조작.

## 구현 순서

1. `MonsterComponent` + `CMonsterSystem` 기본 FSM
2. 평타/이동 (Navigation 필요 — Phase C-4 이후)
3. 어그로 우선순위 계산
4. 리쉬 복귀
5. 몬스터별 특수 스킬 (Baron 침묵, Dragon 브레스, Krug 분열 등)
6. 리스폰 타이머 + ImGui 조작 UI

## 검증

- 정글몹에 평타 → 어그로 잡히고 추격
- 리쉬 범위 벗어나면 복귀 + HP 회복
- 처치 시 골드 획득 + 5초 후 재스폰 (연습용)
- Baron 에 접근 시 침묵 디버프 걸림
