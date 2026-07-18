Session - S040 이후 Data Driven 전환 gameplay 회귀 전수 조사 결과

> **현재 워크트리 정정(2026-07-18 재조사):** 이 문서 최초 작성 뒤
> `Client/Private/GamePlay/SkillRegistry.cpp`가 다시 변경되어, 현재 코드는
> authored `targetMode`를 수동 `SkillDef` 위에 덮어쓰지 않는다. 따라서 아래
> 2~4절 중 `targetMode 전역 덮어쓰기`를 현재 코드의 직접 원인으로 적은 부분은
> 과거 조사 시점의 상태다. 현재 코드 기준 최종 판정, 전체 원인 코드와 BP는
> **15~20절이 우선한다.**

# 1. 조사 목적과 중단 지점

- 현재 코드베이스를 기준으로 Zed 및 Data Driven 반영 이후 발생한 이렐리아와 다른 챔피언의 gameplay 회귀를 조사했다.
- 사용자의 중단 요청에 따라 추가 조사와 소스 수정은 진행하지 않았다.
- 이 문서는 중단 시점까지 코드, 생성 데이터, 입력 payload, 서버 소비 경로, 빌드와 자동 검증에서 확인한 결과만 기록한다.
- 조사 대상은 구현 챔피언 17종, skill slot 85개다.

# 2. 결론

- 회귀가 적지 않다. 핵심 문제는 JSON 수치 자체의 대규모 변경이 아니라, authored gameplay data가 기존 수동 SkillDef 위에 전역 적용되면서 target, stage, action-lock 계약을 함께 덮어쓴 것이다.
- Data Driven 자동 게이트는 현재 11/12, 약 91.7%다. 이 수치는 데이터 소유권 전환율이며 실제 gameplay 동작 완성도를 의미하지 않는다.
- client/server 수치 데이터의 직접 비교에서는 광범위한 수치 드리프트가 발견되지 않았다.
- 반면 client command payload를 만드는 targetMode 변환에서 7개 챔피언 10개 스킬의 기능 실패가 코드상 확정됐다.
- 사용자가 17개 챔피언을 직접 플레이하며 회귀를 발견해야 하는 상태로 넘기면 안 된다. 수정 후 화면, 조작감, 실제 네트워크 타이밍에 한정한 최소 인게임 승인 검증만 필요하다.

# 3. 직접 원인

## 3-1. authored data 전역 덮어쓰기

- Client/Private/GamePlay/SkillRegistry.cpp의 ApplyAuthoredGameplayData가 기존 수동 SkillDef의 targetMode, cooldown, range, mana, stage count/window, stage lock, skill/scaling id를 authored data로 덮어쓴다.
- 현재 champions.json의 광범위한 숫자 변경이 다른 챔피언 회귀를 만든 것이 아니다.
- 기존부터 있던 Conditional과 불완전한 stage 데이터가 전역 덮어쓰기 경로를 타면서 기존 수동 정의보다 우선하게 된 것이 주원인이다.

## 3-2. Conditional target 손실

- Shared/GameSim/Definitions/SkillDefGameDataAdapter.h에서 Conditional은 항상 Direction shape로 변환된다.
- Client/Private/Scene/Scene_InGameLocalSkills.cpp의 Direction 명령은 direction만 채우며 targetEntityId와 groundPos를 채우지 않는다.
- UnitTarget 스킬은 target entity를, GroundTarget 스킬은 groundPos를 필요로 하므로 Conditional 전환 뒤 서버가 필요한 payload를 받지 못한다.

## 3-3. stage target 생성 누락

- Tools/ChampionData/build_champion_game_data.py는 각 stage의 targetMode를 파싱한다.
- 그러나 ChampionGameData 생성 단계에서는 top-level targetMode와 stage lock만 출력하고 stage별 targetMode를 출력하지 않는다.
- 따라서 JSON에 2타 target shape를 작성해도 client generated data에 전달되지 않는다.
- 현재 다단 스킬은 기존 수동 stage2TargetMode에 우연히 의존하고 있다.

# 4. 이렐리아 확정 회귀

## 4-1. W2 이후 즉시 이동 불가

- 이렐리아 W1 lock은 5.0초, W2 lock은 0.4초다.
- W2 이동 정책은 QueueUntilUnlock이며 Shared/GameSim/Definitions/GameplayDefinitionQuery.cpp에서 최대 8 tick까지 movement lock을 유지한다.
- 30 tick 기준 최대 약 267ms 동안 이동이 지연된다.
- W를 놓으면 W2가 즉시 발동하고 바로 이동할 수 있어야 한다는 요구와 맞지 않으며, 보고된 증상과 일치한다.

## 4-2. E 즉시 사용 실패처럼 보이는 문제

- W2의 QueueUntilUnlock은 E command 자체를 막는 정책이 아니다.
- E가 늦게 나가는 것처럼 보이는 직접 원인은 Conditional이 Direction으로 바뀌어 cursor groundPos가 command에서 사라지는 것이다.
- command가 접수되더라도 좌표가 0 또는 잘못된 위치가 되어 실제 스킬이 실패한 것처럼 보일 수 있다.

## 4-3. Q 대상 손실

- 기존 Q는 UnitTarget이지만 authored Conditional 전환 뒤 Direction command가 된다.
- targetEntityId가 비어 서버 Q 처리에 필요한 대상이 전달되지 않는다.

# 5. 다른 챔피언 확정 payload 회귀

서버 gameplay handler 또는 fallback이 실제로 누락된 target/ground payload를 소비하는 경로까지 확인한 결과다.

| 챔피언 | 스킬 | 누락 payload |
|---|---|---|
| 이렐리아 | Q | target entity |
| 이렐리아 | E | ground position |
| 잭스 | Q | target entity |
| 킨드레드 | W | ground position |
| 킨드레드 | E | target entity |
| 리 신 | W | target entity |
| 마스터 이 | Q | target entity |
| 사일러스 | Q | ground position |
| 사일러스 | W | target entity |
| 야스오 | E | target entity |

- 확정 범위: 7개 챔피언, 10개 스킬.
- 수동 정의와 JSON 1타 target shape의 전체 불일치: 27개.
- target shape 불일치 영향 챔피언: 15개.
- 나머지 불일치는 Self 계열처럼 server가 payload를 무시하거나 Ground가 direction도 함께 제공하는 경우가 있어 정적 조사만으로 실제 실패라고 확정하지 않았다.

# 6. 다단 스킬과 입력 의미 회귀

- Client/Private/Scene/Scene_InGameInput.cpp는 Zed를 제외한 W 다단 스킬에 공통으로 W press=1타, W release=2타 의미를 적용한다.
- 이렐리아와 비에고의 charge/release에는 맞을 수 있으나 리 신 W처럼 두 번째 press로 2타를 사용해야 하는 스킬에는 맞지 않는다.
- 다단 스킬 13개를 확인했으며 Zed W/R을 제외한 stage target 정보는 generated data에 보존되지 않는다.
- 요네 E는 stageCount가 2지만 raw stages에 두 번째 stage가 없어 stage2 lock 0.6초가 암묵적 기본값으로 채워진다.
- Kalista R은 gameplay stage 2, visual stage 1로 stage count 계약이 일치하지 않는다. 수동 stage2 animation fallback 때문에 즉시 실패가 확정된 것은 아니지만 자동 검증 대상이어야 한다.

# 7. 행동 잠금 고위험 항목

아래 항목은 코드상 실제 lock이 존재하지만 인게임 재현까지 수행하지 않았으므로 고위험 계약 오류로 분류한다.

| 챔피언/스킬 | 정책과 시간 | 위험 |
|---|---:|---|
| 사일러스 E2 | Forced 0.5초 | chain hit 이후 이동과 후속 cast 과잠금 가능성 |
| 리 신 Q2 | Forced 0.6초 | 실제 dash 도착 뒤에도 잠금이 남을 가능성 |
| 제드 R1 | Forced 1.5초 | visible landing 이후 후속 입력을 계속 막을 가능성 |
| 요네 E2 | implicit Forced 0.6초 | authored stage2 부재로 기본 잠금 적용 |

# 8. 네트워크와 replay 호환 회귀

- 새 Snapshot, Hello, Lobby reader는 championDefinitionKey만 읽는다.
- 기존 championId를 이용하는 compatibility fallback이 없다.
- 동일한 현재 build끼리는 동작하지만 구버전 replay, 구버전 server, mixed-version lobby에서는 definition key 0이 END champion으로 해석되거나 요청이 거절될 수 있다.
- 관련 reader:
  - Client/Private/Network/Client/SnapshotApplier.cpp
  - Client/Private/Network/Client/GameSessionClient.cpp
  - Server/Private/Game/LobbyAuthority.cpp
- 현재 Data Driven gate는 legacy reader가 0개라는 점만 검사하므로 compatibility fallback 부재를 검출하지 못한다.

# 9. Zed 별도 확인 결과

- Shared/GameSim/Champions/Zed/ZedGameSim.cpp에 kZedQProjectileSpeedScale = 0.5f가 남아 있다.
- authored Q projectile speed에 다시 0.5를 곱하므로 실제 속도가 JSON 값의 절반이 된다.
- 이 하드코딩 때문에 RequireComplete gate가 11/12에서 실패한다.

회귀 여부와 별개로 현재 구현 완성도 차이도 확인했다.

- passive basic attack은 동일 target별 cooldown state가 없어 체력 조건만 맞으면 반복 발동한다.
- Death Mark는 표식 동안 누적한 피해가 아니라 pop 시점의 현재 missing health 비율을 사용한다.
- caster, W shadow, R shadow Q의 duplicate projectile falloff가 없다.
- 현재 SimLab Zed probe는 passive threshold와 mark/pop 기본 동작만 검사하며 Q/W/E/R 전체 계약은 검사하지 않는다.

# 10. 수치, AI, FX 전수 확인 결과

## 10-1. 17 champions / 85 skills client-server 비교

| 항목 | 차이 |
|---|---:|
| champion stats | 0 |
| cooldown | 0 |
| mana | 0 |
| range | 0 |
| stage count | 0 |
| raw stage lock | 1 |
| top-level target | 2 |
| missing loadout | 0 |

- raw stage lock 차이 1개는 stage2 raw data가 없는 요네 E다.
- top-level target 차이 2개는 수동 stage-dependent 정의를 유지하는 Zed W/R이다.
- 현재 champions.json과 HEAD의 의미 변경은 Zed 관련 7개 필드에 집중되어 있었다.
- 다른 챔피언의 광범위한 회귀는 숫자 대량 변경이 아니라 전역 authored-data 적용으로 발생했다.

## 10-2. AI

- AI command producer는 targetEntity, groundPos, direction을 모두 채우므로 client target 변환 오류를 그대로 재현하지 않는다.
- 17개 AI profile은 존재하고 build도 통과했다.
- AI도 server action lock과 stage policy의 영향은 받지만 챔피언별 QWER combo matrix 검증은 없다.

## 10-3. FX와 resource

- 전체 141개 WFX JSON parse 성공.
- model/texture reference 누락 0.
- duplicate cue name 0.
- empty cue/emitter 0.
- resource 정적 무결성은 확인했지만 animation blend, 좌표, 크기, 색, 실제 플레이 시각 품질은 capture 검증이 필요하다.

# 11. 실행한 검증과 결과

| 검증 | 결과 |
|---|---|
| Client Debug build | PASS |
| Server Debug build | PASS |
| SimLab Debug build/run | PASS |
| Build-LoLDefinitionPack check | PASS, hash 0x6E61A3CC |
| ChampionData check | PASS, hash 0x780B63F7 |
| Data Driven draft round-trip | PASS |
| WFX 141 files parse/reference audit | PASS |
| Verify-LoLDataDrivenPipeline -RequireComplete | FAIL, 11/12 |
| git diff --check | whitespace error 없음, 기존 CRLF warning만 존재 |

# 12. 자동 검증이 회귀를 놓친 이유

- Data Driven gate는 canonical ownership과 legacy literal 제거를 검사하지만 client command payload와 실제 skill behavior parity를 검사하지 않는다.
- SimLab은 주로 server path를 검사하며 실제 client BuildCastCommand의 target 변환을 통과하지 않는다.
- AI는 세 payload를 모두 채우기 때문에 실제 client 누락을 가린다.
- Irelia W press/hold/release/move timeline test가 없다.
- 17개 챔피언 QWER command payload matrix와 execution matrix가 없다.
- old snapshot/hello/lobby champion identity compatibility test가 없다.
- Zed Q speed/clone count, W swap, E shadow dedupe, R landing/recast/action-lock test가 없다.

# 13. 인게임 검증 경계

사용자가 직접 전수 탐색할 필요는 없다. 아래는 수정 이후 자동 검증으로 대체할 수 없는 최소 승인 범위다.

1. 이렐리아 W hold/release 직후 이동, Q 대상 지정, E1/E2 cursor 위치.
2. 잭스 Q, 킨드레드 W/E, 리 신 W, 마스터 이 Q, 사일러스 Q/W, 야스오 E payload 동작.
3. 리 신 W의 second press, 리 신 Q2, 사일러스 E2, 요네 E return, Zed R landing의 lock 체감.
4. Zed Q 속도와 shadow projectile, W/R swap, passive 동일 target 반복.
5. Kalista R2, Zed/Sylas 강화 평타, 변경 WFX의 animation/FX capture.

구버전 replay 및 mixed-version network compatibility는 수동 플레이가 아니라 fixture 기반 자동 테스트로 검증해야 한다.

# 14. 중단 상태와 인수인계

- 이 문서를 작성한 시점에는 gameplay source를 수정하지 않았다.
- 사용자의 요청에 따라 추가 재현, 수정, build 반복은 중단한 상태다.
- 현재 상태를 regression-free 또는 gameplay Data Driven 100%로 판정하면 안 된다.
- 다음 작업을 재개한다면 target/stage 데이터 계약 복구, Irelia W2 movement policy 수정, 챔피언별 W stage input 의미 분리, network compatibility fallback, Zed Q literal 제거, client payload regression test 추가 순서가 우선이다.

# 15. 현재 워크트리 재조사 최종 판정

## 15-1. 전체 롤백 여부

**현재 변경을 전부 롤백해도 사용자가 보고한 네 증상은 해결되지 않는다.**

- 이렐리아 W2의 `0.4초 + QueueUntilUnlock`은 HEAD에도 동일했다.
- 이렐리아 W 홀드 애니메이션 loop 누락과 `bWHoldCueActive = false`는 HEAD에도 동일했다.
- 이렐리아 E의 복제 이벤트 위치를 시전자 위치로 덮는 코드도 HEAD에 동일했다.
- 비에고 서버에는 HEAD부터 W charge 시작 시각이나 charge 비율 상태가 없다.
- 이즈리얼 E는 HEAD부터 client `Direction`, server `Ground` 계약이 충돌한다.
- 현재 변경에는 `targetMode` 덮어쓰기 중단과 Ground 명령에 direction도 채우는 보정이 포함되어 있다. 전부 롤백하면 이 보정도 같이 사라진다.

따라서 `git 전체 롤백`은 원인 분리가 아니며, 일부 증상을 그대로 남기거나 다시 악화시킬 수 있다. 현재 문제는 다음 세 부류가 섞여 있다.

1. **현재 Data Driven 혼합 소유권 문제:** 숫자는 generated data, target/animation/hook은 수동 등록이 권위다.
2. **기존 network-authoritative 구현 누락:** charge loop/state, ground event 위치 보존, authoritative E visual이 없다.
3. **기존 정책 문제:** 이렐리아 W2 뒤 이동을 최대 8 tick 지연시키고 W1 상태에서는 다른 스킬을 막는다.

## 15-2. 현재 빌드 상태

- `Client/Bin/Debug/WintersGame.exe`와 PDB는 현재 `SkillRegistry.cpp`보다 나중에 빌드되어 있다.
- `Server/Bin/Debug/WintersServer.exe`와 PDB도 재빌드되어 있다.
- 조사 시점에 `WintersGame` 또는 `WintersServer` 프로세스는 실행 중이지 않았다.
- 따라서 BP는 Release보다 Debug 실행 파일에 거는 것이 안전하다. 사용자가 증상을 확인했던 실행 시각이 이 재빌드 전인지 후인지는 코드만으로 확정할 수 없다.

# 16. 네 스킬의 현재 실제 계약

| 스킬 | client 수동 등록 | client runtime GameAtom | Champion JSON | server pack | 실제 문제 |
|---|---|---|---|---|---|
| Irelia W | stage1/2 Direction | stage1/2 Direction | Conditional, 5.0/0.4초 | Self/Contextual, 5.0/0.4초 | W1 loop 누락, hold flag 강제 false, W2 이동 8 tick 지연 |
| Irelia E | stage1/2 Ground | stage1/2 Ground | Conditional, 0.9/0.45초 | Self/Contextual, 0.9/0.45초 | 서버 ground는 정상 수신 가능하지만 client event 적용 때 caster 위치로 덮임 |
| Viego W | stage1/2 Direction | stage1/2 Direction | Direction, 0.7/0.3초 | Direction, 0.7/0.3초 | charge 시간 상태/스케일링 없음, loop도 없음 |
| Ezreal E | Direction | Direction | Ground, 0.25초 | Ground, 0.25초 + castTime 0.25초 | client는 groundPos=0, server는 ground가 있다고 강제, authoritative E visual도 억제 |

현재 `CSkillRegistry::Add`의 실제 병합 순서는 다음과 같다.

```cpp
// Client/Private/GamePlay/SkillRegistry.cpp:38-65, 97-112
SkillDef ApplyAuthoredGameplayData(eChampion champion, u8_t slot, SkillDef def)
{
    ...
    // targetMode는 의도적으로 덮지 않는다.
    def.cooldownSec = skill.cooldownSecByRank[0];
    def.rangeMax = skill.rangeMax;
    def.manaCost = skill.manaCostByRank[0];
    def.stageCount = (std::max)(def.stageCount, skill.stageCount);
    def.stageWindowSec = skill.stageWindowSec;
    def.lockDurationSec = skill.stages[0].lockDurationSec;
    if (skill.stageCount >= 2u)
        def.stage2LockSec = skill.stages[1].lockDurationSec;
    ...
}

void CSkillRegistry::Add(eChampion champ, u8_t slot, const SkillDef& def)
{
    SkillDef legacy = ...ApplyAuthoredGameplayData(champ, slot, def)...;
    ...
    m_GameAtoms.try_emplace(
        key,
        SkillDefAdapters::BuildSkillGameAtomBundle(it->second));
}
```

즉 현재 client runtime target은 JSON이 아니라 아래 수동 등록값이다.

```cpp
// Irelia_Registration.cpp:77-111
Irelia W: targetMode = Direction, stage2TargetMode = Direction
Irelia E: targetMode = GroundTarget, stage2TargetMode = GroundTarget

// Viego_Registration.cpp:68-80
Viego W: targetMode = Direction, stage2TargetMode = Direction

// Ezreal_Registration.cpp:74-82
Ezreal E: targetMode = Direction
```

# 17. 문제 원인이 되는 공통 코드 전체 지도

## 17-1. per-stage targetMode가 생성 데이터에서 유실된다

`Tools/ChampionData/build_champion_game_data.py:80-90`은 stage별 `targetMode`를 읽지만,
`Shared/GameSim/Definitions/ChampionGameData.h:13-16`의 stage 구조체에는 lock만 있다.
생성기 `build_champion_game_data.py:285-308`도 top-level target과 stage lock만 출력한다.

```cpp
// Shared/GameSim/Definitions/ChampionGameData.h:13-16
struct ChampionGameDataSkillStage
{
    f32_t lockDurationSec = 0.6f;
};
```

```python
# Tools/ChampionData/build_champion_game_data.py:87-90
result["targetMode"] = as_enum_name(
    stage.get("targetMode", fallback_target_mode),
    f"{path}.targetMode")

# :306-308 — targetMode는 emit하지 않음
for stage_index, stage in enumerate(skill["stages"]):
    ...
    lines.append(...stage['lockDurationSec']...)
```

또한 `Shared/GameSim/Definitions/SkillDefGameDataAdapter.h:21-35`는 `Conditional`을 무조건 `Direction`으로 축약한다.

```cpp
case eTargetMode::Direction:
case eTargetMode::Conditional:
    return eTargetShape::Direction;
```

현재는 target 덮어쓰기를 중단해 네 스킬을 임시로 피했지만, data schema 자체는 여전히 stage별 target을 표현하지 못한다.

## 17-2. 모든 비-Zed W가 press=1타, release=2타다

`Client/Private/Scene/Scene_InGameInput.cpp:474-507`은 Zed만 별도 처리하고 나머지 챔피언 전체에 아래 규칙을 적용한다.

```cpp
if (in.IsKeyPressed('W'))
{
    if (!HasPendingSkillStage(*this, wSlot))
    {
        const bool_t bDispatched = DispatchSkillInput(wSlot);
        s_bWReleasePending = bDispatched && HasPendingSkillStage(*this, wSlot);
    }
}
else if (in.IsKeyReleased('W') &&
    (s_bWReleasePending || HasPendingSkillStage(*this, wSlot)))
{
    DispatchSkillInput(wSlot, 2u);
}
```

이렐리아/비에고 charge-release에는 맞지만 Lee Sin W처럼 press recast가 필요한 스킬도 같은 의미로 처리된다. 네 증상의 직접 원인만이 아니라 W 다단 스킬 전체의 회귀 범위다.

## 17-3. client command payload는 target shape에 따라 서로 다르다

`Client/Private/Scene/Scene_InGameLocalSkills.cpp:2215-2294`:

```cpp
case eTargetMode::GroundTarget:
    outCmd.groundPos = ground;
    outCmd.direction = DirectionXZ(origin, ground, ...);
    return true;

case eTargetMode::Direction:
    outCmd.direction = normalizedCursorDirection;
    return true; // groundPos는 기본값 (0,0,0)
```

따라서 이즈리얼 E가 수동 등록의 `Direction`을 유지하는 한, 최근 추가된 GroundTarget 보정은 실행되지 않는다.

## 17-4. network-authoritative 모드에서는 local gameplay/visual prediction을 건너뛴다

`Client/Private/Scene/Scene_InGameLocalSkills.cpp:2127-2147, 2174-2183`:

```cpp
SendNetworkSkillCommand(slot, cmd, 2);
if (m_bNetworkAuthoritativeGameplay)
{
    slotState.currentStage = 0;
    slotState.stageWindow = 0.f;
    return true; // ApplyLocalPrediction 호출 안 함
}

...

if (m_bNetworkAuthoritativeGameplay)
{
    SendNetworkSkillCommand(slot, cmd, 1);
    ...
    return true; // ApplyLocalPrediction 호출 안 함
}
```

따라서 과거 local hook이 맞게 보이더라도 network 실행에서는 server EffectTrigger와 snapshot 경로가 완성되어 있어야 한다.

## 17-5. skill 명령은 직렬화 과정에서는 보존된다

`Client/Private/Network/Client/CommandSerializer.cpp:155-191`에서 `groundPos`, `direction`, `stage`를 각각 wire에 넣고,
`Server/Private/Game/CommandIngress.cpp:45-64`,
`Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp:3670-3690`에서 그대로 복원한다.

```cpp
wire.groundPos = groundPos;
wire.direction = direction;
wire.itemId = static_cast<u16_t>(skillStage);

...

cmd.groundPos = wire.groundPos;
cmd.direction = wire.direction;
cmd.itemId = wire.itemId;
```

단, `CommandSerializer.cpp:170`의 cast trace와 `:490`의 serialize trace가 `if (false && ...)`로 꺼져 있어 로그만 보고 payload를 확인할 수 없다. BP가 필요한 이유다.

## 17-6. server stage2 승인 실패 시 W1 lock이 남는다

`Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp:2333-2370, 2424-2429`:

```cpp
const bool_t bRequestedStage2 = cmd.itemId == 2u;
const bool_t bStage2 =
    bRequestedStage2 &&
    slot.currentStage == 1 &&
    slot.stageWindow > 0.f &&
    IsSkillTwoStage(...);

if (bRequestedStage2 && !bStage2)
    return Rejected(...InvalidSkillStage);
```

W2가 승인되면 `CommandExecutor.cpp:2669-2673`에서 stage가 지워지고 새 W2 action state가 시작된다. 거절되면 기존 W1 action state는 교체되지 않는다.

## 17-7. Stationary/Forced action은 다른 스킬을 막고 같은 스킬 2타만 예외다

`CommandExecutor.cpp:104-134, 1933-1943`:

```cpp
if (action.movePolicy != StationaryChannel &&
    action.movePolicy != ForcedMotion)
    return false;

const bool_t bSameSkillStageRelease =
    cmd.kind == CastSkill && cmd.itemId == 2u &&
    cmd.slot == action.sourceSlot;
return !bSameSkillStageRelease;
```

이렐리아 W1은 `StationaryChannel`이다. 따라서 W2는 통과하지만 W1이 남아 있는 동안 E는 `ActionBlocked`로 `HandleCastSkill`에 도달하기 전에 거절된다.

## 17-8. ground EffectTrigger 위치가 client에서 caster 위치로 덮인다

서버 `CommandExecutor.cpp:621-629, 2759-2760, 2823-2843`은 groundPos를 effect event 위치에 넣는다.

그러나 client `Client/Private/Network/Client/EventApplier.cpp:357-369, 1582-1598`은 Kindred W/R만 위치 보존 대상으로 인정한다.

```cpp
bool_t ShouldKeepEffectEventPosition(eChampion hookChampion, u8_t slot)
{
    if (hookChampion != eChampion::KINDRED)
        return false;
    return slot == W || slot == R;
}

Vec3 pos{ ev->posX(), ev->posY(), ev->posZ() };
if (!bKeepEventPosition && attachTo != NULL_ENTITY)
    pos = sourceOrTargetTransformPosition;
```

그 뒤 `EventApplier.cpp:1641-1674`가 이 덮인 `pos`를 `command.groundPos`로 만들어 visual hook에 전달한다. 이렐리아 E 서버 판정 좌표와 client 칼날 표시 좌표가 달라지는 직접 원인이다.

## 17-9. charge action loop는 Jax E에만 하드코딩되어 있다

- `Client/Private/Network/Client/EventApplier.cpp:151-168`
- `Client/Private/Scene/Scene_InGameNetwork.cpp:426-453, 486-494, 526-562`

두 경로 모두 `JAX + SkillE + stage1`만 loop로 인정한다.

```cpp
return champion == eChampion::JAX &&
    actionId == SkillE &&
    stage <= 1u;
```

이렐리아 W1과 비에고 W1은 one-shot으로 재생되고, network action 표시 시간도 `min(lock duration, animation duration)`으로 끝난다. 서버 stage window가 살아 있어도 화면에서는 홀드 애니메이션이 끊긴다.

## 17-10. pack miss가 발생하면 legacy fallback 없이 0/false가 된다

현재 `Shared/GameSim/Definitions/GameplayDefinitionQuery.cpp`는 pack miss 시 다음과 같이 반환한다.

- skill range: `0.f` (`:255-258`)
- cooldown: `0.f` (`:276-279`)
- action lock: `0 tick` (`:425-429` 이후)
- two-stage: `false` (`:551-555`)
- stage window: `0.f` (`:568-572`)

네 스킬의 현재 server pack entry는 존재하지만, 다른 챔피언/override/form에서 definition lookup이 빗나가면 스킬 의미가 조용히 바뀌는 공통 회귀 경로다.

# 18. 챔피언별 직접 원인 코드

## 18-1. Irelia W hold/release/move

### 원인 A — W hold FX 상태를 생성 직후 false로 만든다

`Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp:190-203`:

```cpp
const bool_t bPlayed =
    CFxCuePlayer::PlayAll(..., &state.wHoldCueIds) != NULL_ENTITY;
state.bWHoldCueActive = false;
return bPlayed;
```

`Irelia_Skills.cpp:336-343`의 `UpdateWAimCue`는 이 값이 false면 즉시 return한다. 이 필드를 true로 설정하는 경로는 현재 없다.

### 원인 B — W1은 5초 Stationary, W2는 QueueUntilUnlock

데이터:

```json
// champions.json:67-84 / SkillGameplayDefs.json:2959-2995
"stageCount": 2,
"stageWindowSec": 4.0,
"lockSeconds": [5.0, 0.4]
```

정책 `GameplayDefinitionQuery.cpp:478-483`:

```cpp
if (slot == W && stage <= 1u)
    return StationaryChannel;
if (slot == E)
    return Allow;
return QueueUntilUnlock; // Irelia W2
```

`GameplayDefinitionQuery.cpp:431-440`은 QueueUntilUnlock을 최대 8 tick으로 제한한다. 30 Hz 기준 최대 약 267 ms다. 즉 W2가 정상 승인돼도 현재 정책은 “release 즉시 이동”이 아니다.

client도 `Scene_InGameLocalSkills.cpp:1313-1316, 1411-1422`에서 lock 중 move를 서버에는 보내지만 local prediction은 기록하지 않는다. 서버 `CommandExecutor.cpp:2067-2077`은 이동을 queue하고 `MoveSystem.cpp:351-419`가 lock 종료 후 실행한다.

### 원인 C — W2 거절 시 이동과 E가 함께 멈춘다

W2가 `stage2-window`로 거절되면 W1의 5초 Stationary action이 남는다. 이 상태에서:

- 이동 명령은 queue된다.
- E는 `IsCommandBlockedByAuthoritativeAction`에서 `ActionBlocked`로 거절된다.
- client는 W2 명령을 보낸 직후 local stage를 0으로 지워 server 거절을 UI 상태로 복구하지 않는다.

따라서 “W release 뒤 아예 못 움직이고 E도 안 됨”은 먼저 W2 승인 여부를 확인해야 한다.

### 원인 D — server W에는 hold 시간 기반 피해/사거리 상태가 없다

`Shared/GameSim/Champions/Irelia/IreliaGameSim.cpp:494-565`는 stage1에서 로그 후 return하고 stage2에서 즉시 고정 수치를 적용한다. hold 시작 tick이나 charge 비율을 저장하지 않는다.

## 18-2. Irelia E

client 수동 등록과 BuildCastCommand 기준으로는 현재 E1/E2 모두 Ground이며 non-zero `groundPos`를 보낼 수 있다. 서버 `IreliaGameSim.cpp:567-650`도 E1에서 `state.blade1Pos = cmd.groundPos`, E2에서 `state.blade2Pos = cmd.groundPos`를 사용한다.

직접 실패 지점은 두 곳이다.

1. W1 Stationary가 남아 있으면 E는 server executor 입구에서 `ActionBlocked`다.
2. E가 server에서 승인돼도 `EventApplier.cpp:1591-1598`이 event position을 caster 위치로 덮어 client blade visual이 잘못 표시된다.

또한 `Irelia_Skills.cpp:477-482`는 visual command의 `resolvedTargetMode`가 Ground가 아니면 아무것도 표시하지 않는다.

```cpp
if (ctx.pCommand->resolvedTargetMode !=
    static_cast<u8_t>(eTargetMode::GroundTarget))
    return;
```

현재 소스/PDB에서 이 값은 Ground여야 한다. BP에서 Direction이면 실행 중 바이너리/심볼이 현재 소스와 다르거나, registry가 다른 정의를 반환한 것이다.

## 18-3. Viego W

`Shared/GameSim/Champions/Viego/ViegoGameSim.cpp:634-716`:

```cpp
const bool_t bReleaseStage = ctx.pCommand && ctx.pCommand->itemId >= 2u;
if (!bReleaseStage)
    return;

const f32_t dashRange = ResolveViegoSkillRange(...);
const f32_t damage = ResolveViegoSkillEffectParam(...BaseDamage);
const f32_t stunDurationSec = ResolveViegoSkillEffectParam(...StunDurationSec);
```

stage1은 방향만 돌리고 끝난다. `ViegoSimComponent.h:11-37`에는 mist/possession 상태만 있고 W charge 시작 tick, 누적 시간, charge 비율이 없다. stage2는 hold 시간과 무관하게 고정 range/damage/stun을 쓴다.

client `Viego_Skills.cpp:138-149`는 stage1 glow와 stage2 missile만 나누며, 공통 network loop는 Jax E만 지원한다. 즉 비에고 W는 “Data Driven 반영이 안 된 것”이 아니라 network-authoritative charge 기능이 아직 구현되지 않은 상태다.

## 18-4. Ezreal E

### 원인 A — client Direction, data/server Ground

`Ezreal_Registration.cpp:74-82`가 Direction이고 현재 `SkillRegistry`가 이를 보존한다. 따라서 `BuildCastCommand`의 Direction branch가 실행되어 `direction`만 채우고 `groundPos`는 0이다.

반면 server `EzrealGameSim.cpp:1126-1157`은 slot이 E이면 payload와 무관하게 ground target이 있다고 강제한다.

```cpp
pending.vGroundTarget = ctx.pCommand->groundPos; // (0,0,0)
pending.vDirection = ResolveCastDirection(...);
pending.bHasGroundTarget = slot == kEzrealESlot; // 무조건 true
```

`EzrealGameSim.cpp:956-1006`은 `bHasGroundTarget`이 true면 `groundTarget - origin`으로 blink 방향과 거리를 계산한다. 결과적으로 이즈리얼이 월드 원점 쪽으로 이동하거나, 원점 근처에서는 이동 거리가 0이 될 수 있다.

### 원인 B — 0.25초 delay가 데이터와 fallback에 모두 박혀 있다

- `SkillGameplayDefs.json:1180-1182`: `castTimeSec = 0.25`
- `EzrealGameSim.cpp:1175-1178`: `QueuePendingCast(..., 0.25f)`
- `EzrealGameSim.cpp:1156-1157`: `uLaunchTick = current + SecondsToTicksCeil(castTimeSec)`

현재 서버 구현은 의도적으로 즉시 E가 아니라 약 0.25초 뒤 `LaunchArcaneShift`를 실행한다.

### 원인 C — authoritative E visual은 명시적으로 실행하지 않는다

`Client/Private/GameObject/Champion/Ezreal/Ezreal_Skills.cpp:311-315`:

```cpp
void OnCastAccepted_E_Visual(VisualHookContext& ctx)
{
    if (!ctx.bAuthoritativeEvent && ctx.pWorld && ctx.pCommand)
        SpawnArcaneShiftVisual(...);
}
```

network effect event는 `ctx.bAuthoritativeEvent = true`이므로 E flash visual이 이 hook에서는 재생되지 않는다. local prediction도 network 모드에서 건너뛴다. 위치 snapshot만 바뀌어도 스킬이 발동하지 않은 것처럼 보일 수 있다.

# 19. 직접 BP를 거는 순서

아래 순서로 Debug Client와 Debug Server를 동시에 디버깅하면 어느 단계에서 끊기는지 한 번의 재현으로 확인할 수 있다.

챔피언 enum은 `IRELIA=1`, `VIEGO=5`, `EZREAL=12`, slot은 `W=2`, `E=3`이다.

## 19-1. client registry — 게임 시작 직후 1회

| 파일:줄 | 조건 | 확인 값 |
|---|---|---|
| `SkillRegistry.cpp:53` | `(int)champion==1 && (slot==2 || slot==3)` | `def.targetMode`, JSON 수치 병합 전후 |
| `SkillRegistry.cpp:53` | `(int)champion==5 && slot==2` | Viego W target/stage/lock |
| `SkillRegistry.cpp:53` | `(int)champion==12 && slot==3` | Ezreal E가 여전히 Direction인지 |
| `SkillDefGameDataAdapter.h:68` | 위 champion/slot | `data.target.shape[0]`, `shape[1]` |
| `SkillRegistry.cpp:111` | 위 champion/slot | 최종 `it->second`, GameAtom 생성 시점 |

정상 예상값:

- Irelia W: shape0/1 Direction
- Irelia E: shape0/1 Ground
- Viego W: shape0/1 Direction
- Ezreal E: shape0 Direction

## 19-2. client input/command

| 파일:줄 | 재현 | 확인 값 |
|---|---|---|
| `Scene_InGameInput.cpp:491` | W press | `s_bWReleasePending`, local `currentStage/window` |
| `Scene_InGameInput.cpp:500` | W release | release 분기 진입 여부 |
| `Scene_InGameInput.cpp:504` | W release | `requestedStage=2`로 호출되는지 |
| `Scene_InGameInput.cpp:509` | E press | W 입력과 같은 frame인지 |
| `Scene_InGameLocalSkills.cpp:2120` | W release | `bRequestedStage2`, `bLocalStage2Ready`, `gameData.stage.stageCount` |
| `Scene_InGameLocalSkills.cpp:2130` | W2 send | `cmd.slot`, `cmd.groundPos`, `cmd.direction` |
| `Scene_InGameLocalSkills.cpp:2164` | stage1/E | BuildCastCommand 성공 여부 |
| `Scene_InGameLocalSkills.cpp:2220` | 모든 스킬 | `mode` |
| `Scene_InGameLocalSkills.cpp:2261` | Irelia E | non-zero `ground`, `outCmd.groundPos`, `direction` |
| `Scene_InGameLocalSkills.cpp:2288` | Ezreal E/Viego W | direction은 non-zero, Ezreal ground는 zero인지 |
| `CommandSerializer.cpp:159` | 전송 직전 | `slot`, `skillStage`, `groundPos`, `direction` |
| `CommandSerializer.cpp:167` | 전송 직전 | `wire.itemId`가 W release에서 2인지 |

## 19-3. server ingress/stage/action

| 파일:줄 | 조건/확인 |
|---|---|
| `CommandIngress.cpp:45` | `wire.slot==2 || wire.slot==3`; packet의 item/ground/direction |
| `CommandIngress.cpp:58` | W release의 `wire.itemId==2` |
| `CommandExecutor.cpp:3674` | `cmd` 복원 전후 값 |
| `CommandExecutor.cpp:1933` | `cmd.slot`; E가 여기서 ActionBlocked 되는지 |
| `CommandExecutor.cpp:2333` | `bRequestedStage2`, `hookChampion`, `hookSlot` |
| `CommandExecutor.cpp:2343` | `slot.currentStage`, `slot.stageWindow`, `IsSkillTwoStage` |
| `CommandExecutor.cpp:2424` | W2가 여기로 오면 stage2 거절 확정 |
| `CommandExecutor.cpp:2669` | W2 승인 시 stage clear |
| `CommandExecutor.cpp:2689` | W1 승인 시 stage arm |
| `CommandExecutor.cpp:2705` | `actionMovePolicy` |
| `CommandExecutor.cpp:2717` | 새 action state 시작 |
| `CommandExecutor.cpp:2767` | gameplay hook dispatch 성공 여부 |
| `CommandExecutor.cpp:2823` | effect event의 position/direction/stage flags |
| `CommandExecutor.cpp:585` | `action.movePolicy` |
| `CommandExecutor.cpp:589` | `lockTicks` |
| `CommandExecutor.cpp:602` | `lockEndTick - tc.tickIndex` |

이렐리아 W 예상:

- W1: `itemId=1`, `currentStage`가 1로 arm, policy Stationary, lock 150 tick(5초).
- W2: `itemId=2`, `bStage2=true`, policy QueueUntilUnlock, lock 8 tick.
- W2 뒤 E: W2가 승인됐다면 `CommandExecutor.cpp:1933`에서 막히지 않아야 한다.
- E가 ActionBlocked이면 W2가 거절됐거나 client가 release 명령을 보내지 않아 W1 action이 남은 것이다.

## 19-4. champion server hook

| 파일:줄 | 확인 값 |
|---|---|
| `IreliaGameSim.cpp:494` | W hook 진입 |
| `IreliaGameSim.cpp:496` | `bStage2` |
| `IreliaGameSim.cpp:500` | stage1 return / stage2 실행 분기 |
| `IreliaGameSim.cpp:567` | E hook 진입 |
| `IreliaGameSim.cpp:579` | E1 `cmd.groundPos` 저장 |
| `IreliaGameSim.cpp:588` | E2 `cmd.groundPos` 저장 |
| `ViegoGameSim.cpp:634` | W hook 진입 |
| `ViegoGameSim.cpp:643` | `bReleaseStage` |
| `ViegoGameSim.cpp:651` | hold 시간과 무관한 고정 `dashRange` |
| `EzrealGameSim.cpp:1126` | pending cast 생성 |
| `EzrealGameSim.cpp:1140` | zero `vGroundTarget` 여부 |
| `EzrealGameSim.cpp:1148` | `bHasGroundTarget=true` 강제 확인 |
| `EzrealGameSim.cpp:1156` | `uLaunchTick-currentTick` |
| `EzrealGameSim.cpp:956` | 실제 E launch |
| `EzrealGameSim.cpp:979` | zero ground를 valid로 쓰는 분기 |

## 19-5. client authoritative return/visual

| 파일:줄 | 확인 값 |
|---|---|
| `EventApplier.cpp:151` | Irelia/Viego W1에서 loop=false |
| `EventApplier.cpp:1581` | `skillStage`가 W2/E2에서 2인지 |
| `EventApplier.cpp:1592` | server event pos가 caster pos로 덮이는지 |
| `EventApplier.cpp:1644` | visual `command.groundPos` |
| `EventApplier.cpp:1655` | `resolvedTargetMode`; Irelia E는 Ground여야 함 |
| `EventApplier.cpp:1674` | visual hook dispatch 성공 여부 |
| `Scene_InGameNetwork.cpp:426` | network action lock/visual duration |
| `Scene_InGameNetwork.cpp:486` | W1 loop=false |
| `Irelia_Skills.cpp:202` | W hold 직후 flag=false |
| `Irelia_Skills.cpp:339` | aim update가 return하는지 |
| `Irelia_Skills.cpp:481` | E visual target mode 검사 |
| `Viego_Skills.cpp:145` | stage1/2 visual 분기 |
| `Ezreal_Skills.cpp:313` | authoritative event라 visual 생략되는지 |

# 20. 한 번의 재현으로 판정하는 체크리스트

## Irelia W → release → move → E

1. client `Input.cpp:504`가 stage2를 호출하지 않음 → input/release 감지 문제.
2. client `Serializer.cpp:167`에서 itemId가 2가 아님 → command 생성 문제.
3. server `CommandExecutor.cpp:2424`에 걸림 → server stage window/state 불일치.
4. W2가 승인됐지만 `lockEndTick-currentTick == 8` → 현재 W2 이동 지연 정책이 직접 원인.
5. W2 승인 후 E가 `CommandExecutor.cpp:1933`에서 막힘 → W2 action overwrite 또는 snapshot/action state 회귀.
6. E server hook은 도달하지만 client 칼날이 caster에 뜸 → `EventApplier.cpp:1592` 위치 덮어쓰기.

## Viego W

1. W1/W2 command가 둘 다 도달해도 server state에 charge start가 생기지 않는 것이 현재 정상 코드다.
2. `ViegoGameSim.cpp:651-667`의 값이 hold 시간과 무관하면 기능 미구현 확정이다.
3. client loop가 false면 charge animation hold 미구현 확정이다.

## Ezreal E

1. client `BuildCastCommand` mode가 Direction이고 ground가 0이면 현재 계약 충돌 재현 완료다.
2. server `bHasGroundTarget=true`, ground가 0이면 월드 원점 기준 blink 오류 재현 완료다.
3. `uLaunchTick-currentTick`이 약 8 tick이면 0.25초 지연이 적용된 것이다.
4. 이동 snapshot은 왔는데 FX가 없으면 `Ezreal_Skills.cpp:313`의 authoritative visual 억제가 직접 원인이다.

이 조사에서는 gameplay source를 수정하지 않았다. 변경한 것은 이 Session 결과 문서뿐이다.

# 21. W release 분기 미진입 추가 조사

## 21-1. `Conditional`과 관계없는 input edge 문제

`Scene_InGameInput.cpp:500`에 진입하지 않는 현상은 target의 `Conditional` 여부보다 앞단에서 발생한다.

현재 순서는 다음과 같다.

```text
Win32 WM_KEYUP
→ CInput::OnKeyUp('W')
→ CInput::IsKeyReleased('W')
→ UpdateCombatInput의 W release 분기
→ DispatchSkillInput(slot=2, requestedStage=2)
→ SkillRegistry/GameAtom target shape
→ BuildCastCommand
```

따라서 release 분기에 들어오기 전에는 `Conditional`, `GroundTarget`, `Direction`이 평가되지 않는다.

## 21-2. `IsKeyReleased`는 한 프레임짜리 edge다

`Engine/Private/Platform/CInput.cpp:20-23`:

```cpp
bool CInput::IsKeyReleased(uint8 vKey) const
{
    return !m_Keys[vKey] && m_pPrevKeys[vKey];
}
```

`Engine/Public/Core/CInput.h:59-65`와 `CEngineApp.cpp:367`에서 매 프레임 끝 현재 상태를 이전 상태로 복사한다.

```cpp
std::memcpy(m_pPrevKeys, m_Keys, sizeof(m_Keys));
```

정상 release 프레임은 `m_Keys['W']==false`, `m_pPrevKeys['W']==true`여야 한다. 이 프레임을 한 번 건너뛰면 다음 프레임에는 둘 다 false가 되어 release edge는 복구되지 않는다.

## 21-3. BP 자체가 `WM_KEYUP`을 잃게 할 수 있다

`CWin32Window.cpp:422-424`에서 게임 창이 `WM_KEYUP`을 받아야 `OnKeyUp`을 호출한다.

W press 처리 중 BP에 걸린 뒤 Visual Studio가 활성화된 상태에서 W를 놓으면 `WM_KEYUP`은 Visual Studio 창으로 전달되고 게임 창에는 오지 않는다. 현재 `CWin32Window`/`CInput`에는 `WM_KILLFOCUS` 시 키 배열을 초기화하거나 OS 상태로 재동기화하는 경로가 없다. 그러면 게임의 `m_Keys['W']`는 true로 남아 `IsKeyReleased`가 계속 false다.

확인 BP:

1. `CWin32Window.cpp:422`, 조건 `wParam == 0x57`.
2. 여기에도 걸리지 않으면 게임이 W key-up 메시지를 받지 못한 것이다.
3. `CInput.cpp:22`에서 다음 값을 확인한다.
   - `m_Keys[0x57]=0, m_pPrevKeys[0x57]=1`: 정상 release edge.
   - `1,1`: key-up 유실 또는 focus-loss stuck key.
   - `0,0`: release 프레임을 이미 건너뛰어 edge가 소실됨.

W press 처리 내부에는 BP를 걸지 말고, key-up WndProc에만 BP를 걸거나 tracepoint를 사용해야 입력을 방해하지 않고 확인할 수 있다.

## 21-4. release 프레임을 건너뛰는 코드 경로

`Scene_InGameInput.cpp:318-373`에는 W 분기보다 앞선 return/guard가 있다.

- replay playback이면 `Scene_InGame.cpp:924-928`에서 `UpdateCombatInput` 자체를 호출하지 않는다.
- dead, ping wheel, renderer 없음, Kalista carried, stun이면 W 분기 전에 return한다.
- `ImGui::GetIO().WantCaptureKeyboard == true`면 `if (!bImGuiKbd)` 내부의 W 분기를 통째로 실행하지 않는다.

이 중 어느 하나가 release 한 프레임에만 true여도 `CEngineApp.cpp:367`의 `EndFrame()`은 실행되므로 edge가 사라진다.

## 21-5. release 조건의 두 번째 절

`IsKeyReleased('W')`가 true여도 아래 값이 둘 다 false면 body에는 들어가지 않는다.

```cpp
s_bWReleasePending || HasPendingSkillStage(*this, wSlot)
```

- `s_bWReleasePending`은 W press 때 `DispatchSkillInput`이 성공하고 즉시 local stage가 열렸을 때만 true다.
- `HasPendingSkillStage`는 `currentStage == 1 && stageWindow > 0`일 때만 true다.
- Irelia current source는 stageCount 2이므로 정상 W1 dispatch 직후 두 값 중 적어도 pending flag는 true여야 한다.

`Scene_InGameInput.cpp:500`에서 다음 네 값을 동시에 확인하면 원인이 바로 갈린다.

```text
in.IsKeyReleased('W')
in.IsKeyDown('W')
s_bWReleasePending
HasPendingSkillStage(*this, wSlot)
```

## 21-6. 기존 SkillTable/Conditional의 실제 관계

현재 `Client/Private/GameObject/SkillTable.cpp`의 `FindSkillDef`는 별도 table이 아니라 `CSkillRegistry::Find`를 호출하는 호환 facade다.

`DispatchSkillInput`에 들어온 뒤에야 registry에서 `SkillDef`와 GameAtom을 찾고, `BuildCastCommand`가 target shape를 사용한다.

`Conditional`은 adapter에서 다음처럼 변환된다.

```cpp
Conditional → target shape Direction
Conditional → resolvePolicy ChampionStateDependent
```

그러나 현재 `BuildCastCommand`는 `resolvePolicy`를 읽지 않고 stage별 `shape`만 switch한다. 더구나 현재 `SkillRegistry::ApplyAuthoredGameplayData`는 JSON targetMode를 덮어쓰지 않으므로 Irelia W/E client target은 수동 등록의 `Direction/GroundTarget`이다.

결론적으로 현재 W release 미진입에는 Conditional이 관여하지 않는다. Conditional 문제는 release가 정상 감지되어 `DispatchSkillInput`까지 들어온 뒤 어떤 payload를 만들 것인가의 별도 문제다.

# 22. 사용자 BP 반영 최종 원인 정정 — SkillTable 시절과 Data Driven cutover의 차이

사용자 BP에서 아래 network-authoritative W1 분기가 `stageCount == 2`를 통과하고 `return true`까지 도달했다.

```cpp
SendNetworkSkillCommand(slot, cmd, 1);
if (gameData.stage.stageCount == 2)
{
    slotState.currentStage = 1;
    slotState.stageWindow = gameData.stage.stageWindowSec;
}
return true;
```

따라서 이 재현에서 `stageCount`/W1 local bookkeeping 누락 가설은 반증됐다. `s_bWReleasePending` 또는 `HasPendingSkillStage` 우변은 arm되므로 W2 분기 미진입은 W release edge가 false이거나 `UpdateCombatInput`이 release frame에 실행되지 않은 경우로 좁혀진다.

## 22-1. Data Driven이 증상을 치명적으로 만든 지점

commit `18ca031` 전 `ActionStateComponent`는 `actionId/startTick/sequence/stage`만 가졌고 다른 command를 막지 않았다. 같은 commit의 Data Driven cutover 뒤 `lockEndTick`, `sourceSlot`, `movePolicy`, queued move와 `IsCommandBlockedByAuthoritativeAction`이 추가됐다.

현재 Irelia W1은 authored 5.0초 lock과 hard-coded `StationaryChannel`을 결합한다. W2가 한 번이라도 전송되지 않거나 거절되면 다음 연쇄가 생긴다.

```text
W2 미전송/거절
  -> server W1 StationaryChannel 5초 유지
  -> Move command queue
  -> E command ActionBlocked
```

W2가 정상 수락돼도 Irelia W2는 현재 `QueueUntilUnlock`이고 lock 상한 8 tick이 적용돼 30 Hz에서 최대 약 267 ms 이동 지연이 남는다. 그러므로 input edge 복구와 별개로 W2 정책을 `Allow`로 바꿔야 “release 즉시 이동/E” 요구를 만족한다.

## 22-2. 이전 SkillTable이 동작한 이유

- 초기 SkillTable의 Irelia W는 stage1/2 `Direction`, E는 stage1/2 `GroundTarget`으로 payload를 명시했다.
- W 입력도 당시 press=stage1/release=stage2였으므로 새로운 hold 전용 conditional이 있었던 것은 아니다.
- server action state가 command lock으로 사용되지 않아 release 유실 또는 짧은 animation timing이 Move/E 전체를 5초 막는 증폭 경로가 없었다.
- 현재 JSON의 `Conditional`은 target resolve 계약이다. input activation/hold 계약이 아니며 현재 input code에서 소비되지 않는다.

따라서 사용자의 “홀드 스킬을 일반 사용과 구분해야 한다”는 진단은 맞다. 정확한 수정 모델은 `Conditional` 재사용이 아니라 `PressRelease`, `PressRecast`, `Press` activation을 별도 데이터 타입으로 추가하는 것이다.

## 22-3. 후속 계획서

가설별 BP, V0~V5 직접 수정 순서, exact code anchor, 13개 2단계 activation 분류, Irelia/Viego authoritative charge, Ezreal E payload, 자동 회귀 gate는 다음 Session 계획서에 기록했다.

- `.md/plan/2026-07-18_IRELIA_VIEGO_EZREAL_STAGE_INPUT_REGRESSION_RECOVERY_PLAN.md`

Claude CLI 2.1.209 교차 감사는 OAuth session 만료로 실행 실패했으며 계획서 상태를 `CLAUDE_REVIEW_PENDING`으로 남겼다. 인증 복구 전에는 Claude 검증 완료로 판정하지 않는다.

# 23. 후속 구현·자동 검증 결과

후속 세션에서 W release state fallback, network send 성공 여부, Irelia W2 `Allow`,
Irelia W FX flag, Ezreal E/Viego R payload 정렬, client registry audit, SimLab 전수
probe를 반영했다. Release SimLab은 17 champions / 85 skills / 13 two-stage와
Irelia W1→W2→Move/E 동틱 수락을 PASS했고, 전체 120-tick seed 42 회귀도 PASS했다.
Client Debug/Release와 Server Release는 모두 오류 0으로 링크됐다.

최신 판결과 완전 이관의 rollback gate는 아래 RESULT가 우선한다.

- `.md/plan/2026-07-18_IRELIA_VIEGO_EZREAL_STAGE_INPUT_REGRESSION_RECOVERY_RESULT.md`
