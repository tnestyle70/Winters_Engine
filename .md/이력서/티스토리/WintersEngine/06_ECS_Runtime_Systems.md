# Winters Engine 해부기 6 - ECS와 Runtime Systems

ECS의 본질은 "모든 것을 ECS로 만들었다"가 아니다.

Winters에서 중요한 것은 다음이다.

> runtime entity state를 component로 분리하고, system이 명시적인 책임 단위로 갱신하도록 만들어 복잡한 gameplay/runtime 흐름을 추적 가능하게 만든다.

## 문제 정의

게임 로직이 커지면 객체 하나가 너무 많은 책임을 가지기 쉽다.

예를 들어 Champion object 하나가 다음 책임을 모두 들고 있다고 생각해보자.

- transform
- HP / mana
- stat
- skill cooldown
- skill rank
- buff
- animation state
- input
- AI
- network replication
- FX cue

처음에는 편하다. 하지만 어느 순간부터 "누가 이 값을 바꿨는가?"를 찾기 어려워진다.

특히 MOBA에서는 한 tick 안에서 이동, 공격, 스킬, buff, damage, death, projectile, AI, replication이 동시에 얽힌다. 이 상태 변경 경로가 불분명하면 버그는 감으로 추적해야 한다.

## Winters의 접근

Winters는 runtime state를 component로 나누고 system이 책임 단위로 처리하게 한다.

대표 component 도메인:

- `TransformComponent`
- `HealthComponent`
- `StatComponent`
- `SkillStateComponent`
- `SkillRankComponent`
- `MoveTargetComponent`
- `ReplicatedActionComponent`
- `ChampionAIComponent`
- `RuneComponent`
- `VisionComponents`

대표 system 도메인:

- Move
- Damage
- Death
- Buff
- SkillCooldown
- ChampionAI
- AttackChase
- StatusEffect
- Replication

관련 경로:

- `Shared/GameSim/Components`
- `Shared/GameSim/Systems`
- `EngineSDK/inc/ECS`
- `Engine/Public/ECS`

## 중요한 경계

여기서 중요한 점은 Engine ECS와 Shared/GameSim의 경계를 구분하는 것이다.

Engine은 generic ECS primitive와 runtime service를 소유한다.

Shared/GameSim은 gameplay truth를 위한 component, system, deterministic contract를 소유한다.

즉 Shared/GameSim은 Engine renderer, UI, ImGui, DX type에 의존하면 안 된다. gameplay simulation은 render backend나 UI framework 없이도 돌아가야 하기 때문이다.

## 상태 변경 경로

Winters의 상태 변경은 다음 원칙을 따른다.

```text
AI 또는 Client Input
-> GameCommand
-> CommandExecutor
-> gameplay component 변경
-> system update
-> Snapshot/Event
-> Client Visual
```

AI가 직접 적의 HP를 깎지 않는다. AI는 command를 만든다. CommandExecutor와 Damage pipeline이 gameplay state를 바꾼다. SnapshotBuilder가 그 결과를 Client에 보낸다.

이 흐름 덕분에 human input과 bot input이 같은 권위 경로를 탈 수 있다.

## 예시

`ChampionAISystem`의 context는 gameplay state를 관찰하기 위한 데이터만 정리한다.

```cpp
struct ChampionAIContext
{
    EntityID enemyChampion = NULL_ENTITY;
    EntityID lowHpEnemyChampion = NULL_ENTITY;
    EntityID enemyMinion = NULL_ENTITY;
    EntityID enemyStructure = NULL_ENTITY;

    f32_t selfHpRatio = 1.f;
    f32_t enemyHpRatio = 1.f;
    f32_t enemyDistance = 999.f;
    f32_t attackRange = 1.5f;
    f32_t turretDanger = 0.f;
};
```

AI context는 상태를 읽는다. 결과를 직접 조작하는 것이 아니라 다음 command를 고르기 위한 판단 자료다.

## 면접에서 말할 포인트

ECS를 설명할 때 "ECS를 사용했습니다"로 끝내면 약하다.

더 좋은 설명은 이것이다.

> MOBA tick에서 어떤 system이 어떤 component를 변경하는지 추적 가능하도록 상태와 책임을 분리했다.

이 설명은 구조적 디버깅 능력을 보여준다.

## 이 글을 이력서 문장으로 압축하면

> Champion/Skill/AI/Replication 상태를 component와 system 책임으로 분리해 MOBA gameplay tick에서 상태 변경 경로를 추적 가능하게 구성했습니다.

## 다음 글

다음 글에서는 Client가 아니라 Server GameSim이 gameplay truth를 만드는 서버 권위 구조를 설명한다.

