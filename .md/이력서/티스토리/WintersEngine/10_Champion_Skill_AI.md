# Winters Engine 해부기 10 - Champion Skill과 AI

Champion AI의 본질은 "그럴듯하게 움직이는 봇"이 아니다.

Winters에서 AI의 본질은 다음이다.

> AI가 game state를 관찰하고, 서버 권위 경로로 실행 가능한 command sequence를 생산하는 것

## 문제 정의

단순한 적 추적 AI는 if문 몇 개로 만들 수 있다.

하지만 MOBA 챔피언 AI는 다르다. 특히 LeeSin이나 Sylas처럼 스킬 연계가 있는 챔피언은 단순히 "적이 가까우면 Q"로 끝나지 않는다.

LeeSin의 기본 콤보만 봐도 여러 조건이 필요하다.

```text
Q 사용
-> Q 적중 여부 관찰
-> Q2 사용
-> BA
-> E
-> BA
-> E2
-> 상대 뒤로 ward 설치
-> ward 또는 아군 대상 W
-> R
```

Sylas도 마찬가지다.

```text
Q 사용
-> E1으로 거리 좁히기
-> E2 사슬 적중
-> W 사용
-> R로 궁극기 강탈
-> 훔친 궁극기 사용
-> 스킬 후 passive BA window 처리
```

이 문제는 단순 스킬 호출 문제가 아니다.

다음 요소가 동시에 연결된다.

- target selection
- skill readiness
- cooldown
- skill rank
- projectile hit result
- two-stage skill window
- ward entity
- ally/ward targeting
- passive attack window
- server command sequencing
- debug trace

## Winters의 접근

Winters AI는 상태를 직접 조작하지 않는다.

AI는 world state를 관찰하고 다음 command를 생산한다. 그 command는 Server GameSim의 권위 경로를 통과한다.

관련 파일:

- `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`
- `Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h`
- `Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.h`
- `Shared/GameSim/Components/ChampionAIComponent.h`
- `Shared/GameSim/Champions/LeeSin`
- `Shared/GameSim/Champions/Sylas`

AI context 예시:

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

이 context는 AI가 world를 읽기 위한 자료다. AI는 이 값을 바탕으로 command를 고른다.

## 왜 command sequencing인가

LeeSin Q2는 Q가 맞았을 때 의미가 있다. Sylas E2는 E1 이후의 window와 target 상태가 중요하다. Passive BA는 스킬 사용 이후 일정 시간 안에 basic attack이 발생해야 한다.

이런 구조는 "스킬 버튼을 누른다"가 아니라 "상태를 관찰하고 다음 command를 순서대로 결정한다"에 가깝다.

따라서 AI는 다음을 고려해야 한다.

- 현재 action lock이 남아 있는가
- skill cooldown이 끝났는가
- skill rank가 있는가
- target이 살아 있는가
- target이 range 안에 있는가
- projectile hit result가 있는가
- 다음 command를 지금 보낼 수 있는가

## Debug의 중요성

AI는 결과만 보면 왜 그런 행동을 했는지 알기 어렵다.

그래서 AI debug trace와 panel이 중요하다.

AI가 어떤 target을 선택했는지, 어떤 action을 고르려 했는지, 어떤 조건 때문에 block되었는지 볼 수 있어야 한다. 그렇지 않으면 combo AI는 감으로 튜닝하게 된다.

관련 파일:

- `Shared/GameSim/Components/ChampionAIComponent.h`
- `Client/Private/UI/AIDebugPanel.cpp`
- `Client/Private/UI/DebugDrawSystem.cpp`

## 면접에서 말할 포인트

"리신 콤보를 구현했습니다"는 기능 설명이다.

더 좋은 설명은 다음이다.

> 챔피언별 복합 행동을 server-authoritative command sequencing 문제로 정의하고, skill readiness, target valuation, two-stage window, ward/ally targeting을 고려해 AI decision pipeline으로 풀었다.

## 이 글을 이력서 문장으로 압축하면

> LeeSin/Sylas 등 챔피언별 복합 combo를 skill readiness, target valuation, two-stage window, ward/ally targeting을 고려한 server-authoritative command sequencing 문제로 구현했습니다.

## 다음 글

다음 글에서는 AI, 스킬, FX, 네트워크 문제를 추측 대신 관측 가능하게 만드는 Debug/FX 파이프라인을 설명한다.

