Session - 전 챔피언 AI 전투·파밍 기반 개선과 요네 영혼해방 권위 구현

## 1. 목표와 완료 조건

이번 세션은 챔피언별 거대 하드코딩을 추가하는 작업이 아니다. 공통 AI 판단층의 막힘을 먼저 제거하고, 17개 챔피언의 콤보·스킬 우선순위를 데이터로 연결한 뒤, 요네 E의 피해 저장/귀환 폭발을 서버 권위 게임플레이로 완성한다.

완료 조건은 다음과 같다.

1. 모든 봇이 현재 자원으로 지불할 수 있는 스킬만 명령 후보로 선택한다.
2. 레벨업 후 남은 스킬 포인트를 `LevelSkill` 명령으로 소비하며, 챔피언별 `skillRules` 우선순위와 궁극기 레벨 제약을 따른다.
3. 17개 챔피언 모두 전투 콤보 오버라이드를 가지며 Q/W/E/R 중 전투에서 합법적인 슬롯이 데이터 경로로 도달한다.
4. 원거리 챔피언은 `preferredRange`와 `kiteBias`를 실제 이동 판단에 사용한다.
5. 미니언 선택은 단순 체력 비율이 아니라 현재 기본 공격으로 막타 가능한 대상을 최우선으로 선택하고, 기본 공격이 불가능할 때는 데이터의 스킬 우선순위 중 실제 처치 가능한 스킬을 사용한다.
6. 요네의 기본 교전은 `E -> Q -> BA -> W -> BA -> Q` 순서로 진입·교환하고, E 지속시간 임박·공용 후퇴 점수 우세·체력 교환 열세·적 포탑 위협 상승 중 하나가 성립하면 `E2`로 귀환한다. R은 이 기본 콤보의 강제 선행 조건으로 사용하지 않는다.
7. 요네 E 중 기본 공격/스킬로 적 챔피언의 체력에 실제 적용된 물리/마법 피해를 대상별로 저장하고, 귀환 시작 시 E 랭크별 `25 / 27.5 / 30 / 32.5 / 35%`를 고정 피해로 재적용한다. 아이템/룬 피해, 보호막이 흡수한 양, E 폭발 자체는 재저장하지 않는다.
8. 최초 저장 시 `yone_base_e_mark.png` 표식이 대상 머리 위에 붙고, 귀환/사망/취소 시 서버 EffectTrigger를 통해 한 번만 제거된다.
9. 생성기 `--check`, 관련 빌드, SimLab 결정성·AI·요네 E 회귀 검증이 모두 통과한다.

## 2. 현재 코드 증거와 근본 원인

- `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`
  - `ApplyChampionAIProfileAndTuning`은 신규 스킬 시도 간격을 기본 3초로 둔다.
  - 활성 `comboTarget`만 이 게이트를 면제하지만 기존 `TryExecuteYoneChampionCombat`은 콤보 상태를 만들지 않고 R/E/Q/W를 매번 단발 시도한다.
  - 요네 분기는 R을 E/Q보다 먼저 검사하므로 관측상 R만 반복되기 쉽다.
  - `preferredRange`, `kiteBias`는 데이터에는 있으나 전투 이동 목표에 소비되지 않는다.
  - `FindEnemyMinion`은 `(1-hpRatio)*60 + distanceFit*25`만 사용하므로 실제 막타 피해를 모른다.
  - 야스오 외에는 스킬 막타 경로가 없다.
- `Data/LoL/ServerPrivate/AI/ChampionAIGameplayDefs.json`
  - 콤보 오버라이드는 Jax/Fiora/Ashe/Riven/LeeSin/Sylas 6명뿐이다.
  - 나머지는 `Q -> BA -> E -> BA -> R` 기본 계획을 공유하여 W가 영구 미도달하고 다단 스킬 상태를 표현하지 못한다.
- `Shared/GameSim/Spawn/ChampionGameplayAssembly.cpp`
  - 모든 봇이 동일한 `Q,W,E,Q,Q,R,...` 순서로 시작 랭크를 받는다.
  - 이후 `ExperienceSystem`은 포인트만 늘리며 AI가 `LevelSkill`을 생산하지 않는다.
- `Shared/GameSim/Champions/Yone/YoneGameSim.cpp`
  - E 영혼 상태, 앵커, 자동 귀환 이동은 존재하지만 피해 저장/폭발은 없다.
- `Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp`
  - `ApplyDamageRequest` 뒤의 `DamageResult.finalAmount`가 저항력·보호막·사망 바닥을 반영한 실제 체력 피해이며, 이 지점이 E 저장의 단일 권위 지점이다.
- `Client/Private/GameObject/Champion/Yone/Yone_Skills.cpp`
  - 서버 EffectTrigger가 동일한 E visual hook/stage로 들어오는 경로가 이미 있다.
- `Client/Bin/Resource/Texture/Character/Yone/particles/yone_base_e_mark.png`
  - 요청한 요네 E 머리 위 표식 원본이 존재한다.
- 병행 세션 변경 경계
  - `ChampionAISystem.cpp/.h`, `GameRoomChampionAI.cpp`, `Tools/SimLab/main.cpp`에는 살아 있는 포탑만 안전 앵커로 고르는 미커밋 변경이 있다. 해당 diff를 보존하고 같은 함수 안에서 충돌 없이 확장한다.

## 3. 권위와 소유 경계

```text
AI perception/profile -> GameCommand only
GameCommand -> CommandExecutor -> YoneGameSim
DamageQueue DamageResult -> YoneSoulMarkComponent accumulation
Yone E return -> DamageRequest(True) + EffectTrigger(stage 4)
Snapshot/Event -> Client Yone visual hook -> attached WFX mark
```

- AI는 체력/피해/쿨다운을 직접 변경하지 않고 `GameCommand`만 생산한다.
- 요네 E 저장·폭발은 Shared/GameSim만 소유한다.
- 클라이언트는 표식의 생성/제거만 담당하고 피해 진실을 만들지 않는다.
- 기존 안전 포탑 앵커 변경과 AI 연구 trace/checkpoint 계약을 유지한다.

## 4. 단계별 반영 계획

### 4.1 공통 AI 합법성: 자원과 레벨업

`Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`의 `EmitSkillCommand`에서 `IsSkillReady` 검사 직후 아래 자원 검사를 추가한다. E2 등 재시전은 추가 비용을 부과하지 않는다.

```cpp
const f32_t resourceCost = GameplayDefinitionQuery::ResolveSkillManaCost(
    world, self, tc, champion, slot);
if (!bStage2 && resourceCost > 0.f &&
    (!world.HasComponent<ChampionComponent>(self) ||
        world.GetComponent<ChampionComponent>(self).mana + 0.001f < resourceCost))
{
    SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::ResourceInsufficient);
    return false;
}
```

`ChampionAIPolicy.h/.cpp`에 기존 `skillRules` 점수를 재사용하는 레벨 슬롯 해석기를 추가한다. R은 6/11/16의 합법 랭크를 우선하고, 그 외에는 점수가 높은 Q/W/E 중 현재 레벨에서 올릴 수 있는 슬롯을 고른다.

```cpp
u8_t ResolveChampionAISkillLevelSlot(
    const ChampionAIProfile& profile,
    const SkillRankComponent& ranks,
    u8_t championLevel);
```

`ChampionGameplayAssembly.cpp`의 고정 `kLevelOrder`를 위 해석기로 교체하고, 런타임 AI는 포인트가 남으면 다음 명령을 다른 행동보다 먼저 한 번 발행한다.

```cpp
GameCommand cmd = MakeAICommand(ai, tc, self, eCommandKind::LevelSkill);
cmd.slot = ResolveChampionAISkillLevelSlot(profile, ranks, champion.level);
outCommands.push_back(cmd);
```

### 4.2 공통 AI 캐스팅 간격·표적·카이팅·막타

- 비콤보 신규 스킬의 기본 최소 간격은 3초에서 0.35초로 낮춘다. 실제 연속 사용 제한은 각 스킬 쿨다운·행동 lock·자원·다단 stage window가 담당한다.
- `FindEnemyChampion`은 가장 가까운 적 하나가 아니라 거리와 잃은 체력을 함께 점수화하여 합법적인 킬 타깃을 안정적으로 고른다.
- `FindEnemyMinion`은 `StatComponent.ad`로 현재 기본 공격 막타 가능 여부를 먼저 분류하고, 막타 가능 대상 중 최저 체력/근거리 대상을 고른다.
- `TryExecuteMinionFarm`은 기본 공격 처치 가능 -> 데이터 우선 스킬 처치 가능 -> 일반 라인 압박 순으로 처리한다.
- 기본 공격/스킬이 쿨다운일 때 `preferredRange`보다 가까우며 `kiteBias>0`이면 적 반대 방향으로 물러나고, 멀면 선호 사거리까지만 접근한다.

기존 `TryEmitAttackChampion` 마지막 이동 블록을 아래 계산 목표 사용으로 교체한다.

```cpp
const Vec3 targetPos = ...;
const Vec3 away = WintersMath::DirectionXZ(targetPos, selfPos);
const bool_t bKiteBack = profile.kiteBias > 0.f &&
    ctx.enemyDistance < profile.preferredRange * 0.85f;
const Vec3 combatGoal = bKiteBack
    ? Vec3{ selfPos.x + away.x * profile.kiteBias,
            selfPos.y,
            selfPos.z + away.z * profile.kiteBias }
    : targetPos;
return EmitMoveCommand(..., combatGoal, ...);
```

### 4.3 데이터: 17개 챔피언 스킬 우선순위와 콤보

`Data/LoL/ServerPrivate/AI/ChampionAIGameplayDefs.json`에서 모든 profile의 `skillRules`를 Q/W/E/R 4개로 완성한다. 점수는 레벨업과 단발 fallback에 공통 사용한다. 콤보는 다음 정책을 최대 10단계 안에서 명시한다.

| 챔피언 | 전투 계획 |
|---|---|
| Annie | E -> R(저체력) -> Q -> W -> BA |
| Ashe | R(원거리) -> W -> BA -> Q -> BA |
| Ezreal | W -> Q -> BA -> E(Away) -> R(저체력) |
| Fiora | R -> Q -> E -> BA -> BA -> W |
| Garen | Q -> W -> BA -> E -> R(저체력) |
| Irelia | E1 -> E2 -> R -> Q -> BA -> W1 -> W2 |
| Jax | E1 -> Q -> W -> BA -> E2 -> R |
| Kalista | Q -> BA -> BA -> E -> R1 -> R2 |
| Kindred | W -> E -> Q -> BA -> R(자기 저체력 조건) |
| Lee Sin | 기존 Q1/Q2/E1/E2/ward-W/R 유지 |
| Master Yi | R -> E -> Q -> BA -> W(저체력) |
| Riven | R1 -> E -> Q -> BA -> Q -> BA -> W -> Q -> R2 |
| Sylas | 기존 Q/E1/E2/BA/W/BA/R steal/cast 유지 |
| Viego | E -> W1 -> W2 -> Q -> BA -> R |
| Yasuo | 전용 Q3/R/E/BA 전술 유지, W를 원거리 적 방어 fallback에 추가 |
| Yone | E -> Q -> BA -> W -> BA -> Q -> E2(Self, 조건부 귀환) |
| Zed | W1 -> W2 -> E -> Q -> R1 -> BA -> R2 |

이를 위해 AI 전용 target mode에 `Self`를 추가하고 생성기 및 runtime overlay enum 파서를 함께 갱신한다.

```cpp
enum class eChampionAIComboTargetMode : u8_t
{
    TargetEntity,
    AwayFromTarget,
    WardBehindTarget,
    LastOwnWard,
    SylasHijackTarget,
    SylasStolenUltimateTarget,
    Self,
};
```

요네 전용 함수는 독자 R/E/Q/W 스팸 분기를 삭제하고, 신규 콤보 여부와 단계 전진은 공용 콤보 상태기계에 맡긴다. 요네 쪽에서 `comboTarget`을 선점하지 않아 신규 콤보 간격 계약을 우회하지 않는다.

```cpp
return TryEmitAttackChampionCombo(
    world, tc, self, ai, champion, selfPos, target, ctx, outCommands);
```

요네 계획의 마지막 `E2(Self)`는 즉시 시전 단계가 아니라 **복귀 판단 대기 게이트**다. `TryEmitAttackChampionCombo`는 이 단계에서 멈추고, 실제 E2 명령은 아래 공통 후퇴 점수 또는 E 만료 임박 조건을 통과한 `TryEmitYoneSoulReturnCommand`만 발행한다. 따라서 콤보 직후 무조건 돌아가거나, 새 콤보가 E1부터 재시작되어 E2로 오인되는 경로를 차단한다.

E2 귀환은 요네 전용 체력 비교식을 별도로 만들어 공용 AI 판단과 어긋나게 하지 않는다. 공용 perception이 적의 현재 기본 공격·Q/W/E/R 준비 상태, 사거리, 스킬 랭크와 데이터 기반 예상 피해를 `observedEnemyComboDamageRatio`로 계산하고, `ChampionAIValuation`이 이 값과 체력·경제·거리·포탑 위험을 함께 사용해 `fChampionDecisionScore`와 `fRetreatDecisionScore`를 만든다. 요네는 이 공용 결과를 그대로 사용한다. 이는 서버 권위 시뮬레이션 내부 판단이며 클라이언트가 피해나 쿨다운을 결정하지 않는다.

```cpp
const bool_t bRetreatUtilityDominates =
    ai.fRetreatDecisionScore >= 0.50f &&
    ai.fRetreatDecisionScore >=
        ai.fChampionDecisionScore + ai.fChampionScoreMargin;
```

### 4.4 요네 E 서버 권위 피해 저장과 폭발

`Shared/GameSim/Components/YoneSimComponent.h`의 `YoneSimComponent` 아래에 대상 부착형 상태를 추가한다.

```cpp
struct YoneSoulMarkComponent
{
    EntityID sourceEntity = NULL_ENTITY;
    f32_t storedPostMitigationDamage = 0.f;
    f32_t remainingSec = 0.f;
    u8_t sourceERank = 1u;
    u8_t reservedAlignment[3]{};
};
```

`WorldKeyframe.cpp`에 위 컴포넌트를 등록한다. `DamageQueueSystem.cpp`은 `ApplyDamageRequest`와 아이템/Fiora 후처리 뒤 요네 콜백을 한 번 호출한다.

```cpp
YoneGameSim::OnDamageResolved(world, tc, request, result);
```

`YoneGameSim::OnDamageResolved` 저장 조건:

```cpp
source is YONE && source E active && !returning
target is enemy Champion
sourceKind is BasicAttack or Skill
damage type is Physical or Magic
result.finalAmount > 0
request.flags does not contain DamageFlag_YoneSoulEcho
```

귀환 시작 시 정렬된 표식 대상을 순회해 아래 비율로 고정 피해를 enqueue하고 표식을 제거한다.

```cpp
constexpr f32_t kSoulEchoRatioByRank[5] =
{
    0.25f, 0.275f, 0.30f, 0.325f, 0.35f
};
echo.type = eDamageType::True;
echo.flatAmount = mark.storedPostMitigationDamage * ratio;
echo.eSourceKind = eDamageSourceKind::Skill;
echo.iSourceSlot = static_cast<u8_t>(eSkillSlot::E);
echo.flags = DamageFlag_YoneSoulEcho;
```

`DamageTypes.h`에 내부 재귀 방지 플래그를 추가하고, Yone E를 `UsesParamDrivenDamageVariant`에 넣어 저장된 flat amount가 일반 E의 0 피해 데이터로 덮이지 않게 한다.

### 4.5 서버 cue 기반 요네 E 표식

서버는 최초 표식에 E stage 3, 제거에 stage 4 EffectTrigger를 `targetEntity`와 함께 보낸다. 클라이언트 `OnCastFrame_E_Visual`은 stage 3에서 cue entity handle을 대상별로 보관하고 stage 4에서 안전하게 파괴한다.

새 파일: `Data/LoL/FX/Champions/Yone/e_soul_mark.wfx`

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Yone.E.SoulMark",
  "emitters": [
    {
      "name": "e_soul_mark_head",
      "render_type": "Billboard",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Yone/particles/yone_base_e_mark.png",
      "max_particles": 1,
      "spawn_rate": 0.0,
      "lifetime": 5.0,
      "fade_in": 0.05,
      "fade_out": 0.15,
      "width": 1.25,
      "height": 1.25,
      "color": [0.75, 0.92, 1.35, 1.0],
      "attach_offset": [0.0, 2.35, 0.0],
      "billboard": true,
      "blockable_by_wind_wall": false
    }
  ]
}
```

### 4.6 에이전트 비평과 반영 판정

계획 초안과 1차 구현은 별도 비평 에이전트가 서버 권위·수명주기·결정성 관점에서 검토했다. 비평 결과와 이번 세션의 판정은 다음과 같다.

| 우선순위 | 지적 | 반영 판정 |
|---|---|---|
| P0 | 사망한 요네의 E 상태·표식이 남을 수 있음 | `YoneGameSim::Tick`에서 사망 시 무폭발 표식 제거, 대시 제거, 상태 초기화. SimLab 사망 회귀 추가 |
| P0 | 기본 공격/Q에 합쳐진 아이템 온힛 피해가 E 저장량에 들어갈 수 있음 | 데이터 공식 적용 전 요청과 최종 요청을 분리하고 동일 방어력 문맥의 post-mitigation 기본/아이템 기여량을 계산해 아이템분을 제외 |
| P0 | 평타 주기·스킬 쿨다운 대기 중 콤보 단계가 건너뛰어짐 | 사거리 안의 BA/스킬 readiness pending은 현재 단계 유지. 미습득·영구 조건 불충족만 건너뜀 |
| P0 | 위협항이 체력·포탑만 보고 적 평타+QWER 예상 피해를 보지 않음 | 관측 가능한 서버 상태의 BA/Q/W/E/R 준비·사거리·랭크·공격력·치명타·방어력으로 `observedEnemyComboDamageRatio`를 계산해 Fight/Retreat 공통 점수에 연결 |
| P1 | 요네 전용 진입 코드가 공통 콤보 시작 쿨다운을 우회함 | 전용 `comboTarget/comboStep` 선설정을 제거하고 공통 상태 머신만 진입 소유 |
| P1 | E 시간이 CC/강제 이동 중 멈추거나 귀환 대시가 취소될 수 있음 | E 타이머는 상태 이상과 무관하게 감소하고 SoulReturn 대시는 이동 불가/강제 이동 일반 취소에서 제외 |
| P1 | 대상 사망·E 랭크 변경·거절된 E2의 정리 순서 | 대상 사망 표식 제거, E1 시 랭크 스냅샷, E2 승인 후 `bReturning` 확인 시에만 콤보 정리 |
| P1 | 마지막 E2가 콤보 직후 무조건 발동 가능 | 마지막 E2를 대기 게이트로 변경. 공통 후퇴 점수 우세 또는 남은 시간 0.75초 이하에서만 실제 명령 발행 |
| P1 2차 | 공통 위협항이 자원 부족 스킬과 사거리 0 자기강화·1단계 스킬을 즉발 피해로 합산 | 자원 보유 검사, target shape, OnHit+BA 준비, 2단계 recast 상태, effect radius/range를 함께 판정. SimLab에서 마나 0 Annie Q와 Riven R1은 0, 마나 보유 Q와 열린 R2만 양수임을 검사 |
| P1 2차 | E2가 강제 이동을 우회해도 기존 `ForcedMotionComponent`가 다음 틱 앵커를 덮음 | SoulReturn 시작 시 기존 강제 이동 궤적을 제거하고, 공중 상태 E 만료 후 복귀 완료 위치를 자동 검사 |
| P1 2차 | `CancelRuntime` 표식 제거에 stage 4 cue가 없음 | TickContext를 Viego 빙의 종료 경로까지 전달하고 취소 시 표식별 stage 4를 정확히 한 번 발행 |
| P2 | 클라이언트 정적 표식 handle이 씬 교체 뒤 남을 수 있음 | `CScene_InGame::OnExit`에서 요네 표식 FX handle 전체 파괴·맵 초기화 |
| P2 | 동일 대상에 양 팀 요네가 동시에 E 표식을 중첩하는 미러 매치 | 현재 고유 챔피언 로스터의 단일 `YoneSoulMarkComponent` 계약 밖이다. 멀티소스 컨테이너가 필요한 별도 확장으로 명시하며 이번 완료 범위에는 포함하지 않음 |

비평 후 재검증은 즉시 E2 방지, 공통 위협 점수의 방향성, 사망·CC·대상 사망 정리, 아이템 피해 제외를 자동 게이트로 둔다.

## 5. 검증 계획

### 5.1 정적/생성 계약

```powershell
python Tools/ChampionData/build_champion_game_data.py --check
python Tools/LoLData/Build-LoLDefinitionPack.py --check
git diff --check
```

### 5.2 SimLab 회귀

`Tools/SimLab/main.cpp`에 다음을 추가한다.

- 17개 AI profile/combo 존재 및 Q/W/E/R 도달성 검사
- 마나 부족 스킬 명령 미발행, 포인트 보유 시 `LevelSkill` 명령 발행
- 기본 공격 막타 가능 미니언 우선 선택
- 실제 AI와 CommandExecutor를 최대 360틱 구동해 승인 명령이 E/Q/BA/W/BA/Q/E2 순서인지, R이 개입하지 않는지 검사
- 마지막 E2가 콤보 직후가 아니라 공통 후퇴 점수 우세 또는 E 만료 임박 뒤에만 승인되는지 검사
- 콤보 단계가 공격 주기·쿨다운 동안 건너뛰지 않고 유지되는지 검사
- 공통 위협항이 자원 부족, 사거리 0 자기강화 1단계, OnHit+BA 준비, 2단계 recast를 구분하는지 검사
- E 중 기본 공격 온힛 아이템 피해가 저장량에서 제외되는지, rank 1의 25% echo와 사망 시 무폭발 정리를 검사
- 요네 E 중 물리+마법 실제 피해 누적, rank 1 25%와 rank 5 35% 고정 피해, 아이템/보호막/echo 재귀 제외
- 공중 강제 이동 중 E 만료가 궤적을 제거하고 앵커 복귀를 유지하는지 검사
- 귀환·대상 사망·취소·시전자 사망 시 표식 제거와 취소 stage 4 cue 단일 발행 검사
- 기존 live safe turret anchor와 MidDefense 결정성 해시 유지 또는 의도 변동 고정값 갱신

### 5.3 빌드와 실행

```powershell
msbuild Winters.sln /m:1 /p:Configuration=Debug /p:Platform=x64
Client/Bin/Debug/SimLab.exe 600 1234
```

솔루션/바이너리 경로가 실제 프로젝트와 다르면 `rg --files -g '*.sln' -g 'SimLab.exe'`로 확인한 실경로를 RESULT에 기록한다.

## 6. 비목표와 후속 인게임 게이트

- 이번 세션은 라인전 마이크로, 스킬 도달성, 막타, 카이팅, 요네 E 진실을 닫는다.
- 정글 루트, 오브젝트 스마이트, 5인 한타 역할 배치, 로밍/텔레포트 같은 상위 매크로는 현재 4후보 연구 스키마(`Retreat/Fight/Farm/Siege`)를 확장해야 하므로 별도 세션이다.
- 최종 체감 판정은 사용자가 인게임에서 챔피언별 Q/W/E/R, 요네 지정 콤보/귀환, 표식 위치와 크기를 확인한다. 자동 검증은 명령·피해·cue 계약을 먼저 닫는다.

## 7. 롤백 범위

- AI: `ChampionAISystem`, `ChampionAIPolicy`, `ChampionGameplayAssembly`, AI JSON, 생성물, runtime overlay의 이번 변경만 되돌린다.
- 요네: `YoneSimComponent`, `YoneGameSim`, `DamageQueueSystem`, `DamageTypes`, `WorldKeyframe`, Yone client visual, 신규 WFX만 되돌린다.
- 병행 작업인 live safe turret anchor 및 기존 resource/command schema diff는 롤백 대상이 아니다.

## 8. 결과 문서

반영 후 동일 경로에 `2026-07-18_CHAMPION_AI_COMBAT_FARM_YONE_SOUL_UNBOUND_RESULT.md`를 작성하고 실제 파일 목록, 생성 해시, 테스트/빌드 exit code, 남은 인게임 게이트를 기록한다.
