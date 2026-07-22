Session - 리신·야스오·정글·미니언·오브젝트 FX 회귀를 현재 코드 근거로 분류하고 다음 구현 세션에 인계
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성, C8 검증이 병목
관련: 2026-07-19_CHAMPION_DAMAGE_DATA_AUTHORITY_CLOSURE_PLAN.md, 2026-07-19_CHAMPION_DAMAGE_DATA_AUTHORITY_CLOSURE_RESULT.md

# 목적과 사용법

이 문서는 구현 계획이 아니라 2026-07-20 현재 작업 트리의 사실·검증·미결정점을 고정한 인수인계서다. 다음 세션은 이 문서와 `AGENTS.md`, `.claude/gotchas.md`, `CLAUDE_Legacy.md`, `.md/계획서작성규칙.md`를 먼저 읽고, 아래 `문제 인터뷰`를 사용자에게 제시해 답을 받은 뒤 같은 범위의 `*_PLAN.md`/`*_RESULT.md` 쌍을 작성한다. 소스 수정은 계획 비평 게이트 통과 후 시작한다.

## 중단 시점과 작업 트리 안전 경계

- 현재 브랜치: `codex/2026-07-16-replay-backend-worktree`.
- replay 구현 세션과 이 gameplay 세션이 같은 Local checkout과 dirty worktree를 공유했다. Git unmerged 파일이나 conflict marker는 없었지만, 두 세션의 변경이 한 브랜치에 섞여 있고 병렬 빌드 중 공용 `EngineSDK` 복사 경합이 발생했다.
- 이 인수인계 작성 직전 `MSBuild`, `cl`, `link`, `Server`, `Client` 실행 프로세스는 없었다.
- `git diff --check`의 현재 확인된 오류는 `Client/Private/Scene/Scene_InGameInput.cpp:244` trailing whitespace 1건이다. 이번 조사 세션은 이를 수정하지 않았다.
- 다음 세션은 단독 writer로 진행하고 Client/Server/SimLab 빌드는 `/m:1`로 순차 실행한다. 새 worktree를 HEAD에서 만들면 현재 uncommitted gameplay 변경을 볼 수 없으므로, checkpoint commit 없이 임의로 분리하지 않는다.
- 이번 전수조사 단계에서는 소스·데이터를 수정하지 않았다. 새로 추가한 파일은 이 인수인계서뿐이다.

# 타임라인

1. `2026-07-19_CHAMPION_DAMAGE_DATA_AUTHORITY_CLOSURE`에서 authoring JSON과 generated pack이 어긋나 있음을 확인했다. 당시 generated 값은 야스오 Q AD ratio `0`, 리신 Q AD ratio `0`, 야스오 E cooldown `3`으로 stale이었다.
2. 두 생성기를 다시 cook하고 검증해 야스오 Q total AD ratio `2.0`, 리신 Q total AD ratio `1.0`, 야스오 E cooldown `0.1`을 generated/server 경로에 반영했다. 17 챔피언, 85 스킬, 323 rank case가 SimLab을 통과했다.
3. 이어진 야스오·정글 수정에서 Q3 애니메이션 root position track, 피격원별 패시브 보호막, E target-death FX 정리, 작은 돌거북 corpse hide, 정글 30초 재생성을 다뤘다.
4. replay 세션이 같은 checkout에서 병행되면서 논리적 변경 혼합과 공용 SDK 병렬 빌드 경합이 발생했다. Git merge conflict는 발생하지 않았으나 안전한 완료 판정을 위해 구현을 멈추고 전수조사로 전환했다.
5. 2026-07-20 조사에서 기존 문서·authoring JSON·generated pack·GameSim·Server projectile·Client FX·AI combo·미니언 이동 경로를 대조하고, 기존 Debug SimLab과 generator `--check`만 실행했다. 신규 소스 구현과 전체 빌드는 하지 않았다.

# 현재 판정 요약

| 문제 | 판정 | 현재 근거 / 남은 일 |
|---|---|---|
| 야스오 Q3 사용 시 캐릭터가 아래로 꺼짐 | 정적 수정 완료, F5 시각 검증 미완료 | `patch_wanim_root_track.py --dry-run`은 대상 `.wanim`에 degenerate position track이 없다고 판정. 실제 플레이 캡처 필요 |
| 야스오 보호막이 미니언 피해에 미발동 | 구현·SimLab 완료 | Champion/Jungle/Minion source를 모두 허용. `SimLab --shield-only` PASS |
| 야스오 E target 사망 후 ring FX 잔존 | 정적 경로 완료, 런타임 시각 검증 미완료 | attach target Health dead + `kill_when_anchor_invalid=true`에서 FX 제거. 사망 프레임 캡처 필요 |
| 작은 돌거북 사망 후 미소멸 | 구현·정적 확인 완료, F5 미완료 | death animation이 없어도 1.5초 후 corpse visual hide. 재생성 시 다시 표시 |
| 정글몹 재생성 30초 | 데이터·SimLab 완료 | 모든 camp `respawnDelaySec=30`, `SimLab --jungle-respawn-only` PASS |
| 야스오/리신 F4 Flat Damage 공백 및 AD 미반영 의심 | 원인 규명·데이터 경로 완료 | Flat 부재가 아니라 variant UI와 stale generated pack 문제. 아래 계산 경로 참조 |
| 야스오 E 0.1초가 서버에서 잘리는지 | 잘리지 않음 | 서버는 0.1을 읽음. 별도 action lock 0.4초와 동일 target E lockout 10초가 체감 재사용을 막음 |
| 대포 미니언 크기 1.5배 | 미구현 | 미니언 visual definition에는 per-role scale이 없고 전 역할이 `m_fVisualScale=0.006` 공유. 검색된 `visualScaleMultiplier=1.5`는 정글 visual 항목 |
| 리신 Q2 재사용 무반응 | 코드 존재하지만 command/stage 연결 결함 | 잘못된/빈 target에서 Q2 hook이 reject되어도 generic stage가 먼저 소비될 수 있음. client는 Q mark를 몰라 hover target 없으면 command 자체를 못 보냄 |
| 리신 봇 Q1→Q2 돌진→평타 | combo 정의는 존재, end-to-end 검증 없음 | AI는 Q2 전 mark를 확인하지만 기존 일반 SimLab은 server projectile collision/mark를 수행하지 않아 실제 연계 증명이 없음 |
| 리신 E1 범위 피해 / E2 slow | 미구현·오배선 | 현재 E1/E2 모두 같은 slow 함수만 실행. damage data는 있으나 DamageRequest를 만들지 않음 |
| 리신 R 10m knockback | 미구현 | damage + airborne만 적용해 제자리에서 뜸. authoritative forced motion 필요 |
| 리신 ward visual | 기능 완료, 표현 교체 필요 | ward entity, 90초 수명, 시야 반경 10, snapshot/vision은 구현. generic floating billboard만 어색함 |
| 칼리스타 W sentinel billboard 제거 | 미구현 | avatar billboard와 ground vision cone을 둘 다 spawn. avatar 생략 후 moving cone 유지 가능 |
| 리신 Q 속도 0.8배 | 미구현 | Q1 projectile speed가 `24.f` 하드코딩. 0.8배는 `19.2f` |
| 리신 Q1 terrain wall 통과 | 미구현 | projectile의 `bCollidesWithTerrain` 경로가 nav clamp 후 Terrain contact로 파괴 |
| 리신 Q2 wall 통과 후 walkable landing | 미구현 | 매 tick `TryClampMoveSegmentXZ`가 막히면 dash를 즉시 끝냄. 도착점 snap만으로는 벽 통과 불가 |
| 장로 버프 FX 추적·사망 제거 | 정적 구현 완료, F5 미완료 | snapshot reconcile이 champion entity에 attach하고 사망 시 server flag 제거/client prune. ground decal이라 발밑 표현은 의도된 현재 형태 |
| 바론 버프 보라 FX 추적·사망 제거 | 정적 구현 완료, F5 미완료 | purple WFX가 entity attach, buff duration 동안 유지, 사망 시 state 제거. live 이동/사망 캡처 필요 |
| 레드 미드 외곽 포탑 근처 근접 미니언 교착 | 원인 후보 특정, 실제 위치 재현 미완료 | lane path에서 allied minion을 hard blocker로 보지 않고 static+soft depenetration vector가 상쇄될 수 있음. combat chase가 path를 clear하고 목표 방향을 바꿔 대칭을 깨므로 풀림 |

# 피해 데이터 권위 판정

## 실제 계산 경로

- `DamageQueueSystem::UsesParamDrivenDamageVariant`는 야스오 Q, 칼리스타 E, 리신 Q, 이즈리얼 R을 variant-flat 스킬로 분류한다.
- variant-flat은 gameplay request가 만든 Q1/Q2/Q3별 Flat을 보존하고 canonical AD/AP ratio를 결합한다.
- AD Ratio는 문자 그대로의 숫자 `2`를 더하는 값이 아니라 `StatSystem`이 계산한 최종 total Attack Damage에 곱한다.

현재 raw physical damage는 방어력 감소 전에 다음과 같다.

- 야스오 Q1/Q2: `60 + totalAD * 2.0`
- 야스오 Q3: `100 + totalAD * 2.0`
- 리신 Q1 rank 1~5: `[55, 80, 105, 130, 155] + totalAD * 1.0`
- 리신 Q2: `95 + totalAD * 1.0`

예를 들어 total AD가 100이면 야스오 일반 Q는 260, Q3는 300, 리신 rank 1 Q1은 155, Q2는 195 raw damage다. 방어력 처리는 그 다음 단계다.

## F4에서 Flat이 비어 보였던 이유

- 야스오 Q와 리신 Q는 stage마다 Flat 원천이 달라 전체 스킬을 `Runtime Parameters`로 뭉뚱그려 표시하던 UI 문제였다.
- Flat 데이터 자체는 존재했다. 2026-07-19 당시 핵심 회귀는 authoring JSON을 고치고 generated pack을 cook하지 않아 server가 ratio 0을 계속 읽은 것이었다.
- 현재 dirty tree의 tuner는 Q1 canonical Flat과 Q2/Q3 runtime variant를 분리해 보여주는 방향으로 수정되어 있고 `SimLab --f4-balance-only`가 통과한다.
- Debug F4 hot reload는 runtime overlay를 사용한다. Release는 저장한 JSON을 직접 읽지 않으므로 generator cook과 rebuild가 필요하다.

## 추가로 발견한 데이터 부채

- 야스오 E runtime `baseDamage=80`과 canonical Flat `200`, total AD ratio `1.0`이 공존한다. E는 variant-flat 예외가 아니므로 현재 canonical formula가 runtime Flat을 대체한다. runtime 80은 죽은/혼동 유발 값이다.
- 리신 R runtime `baseDamage=150`과 canonical Flat `[350,400,500]`, total AD ratio `1.0`도 공존한다. 현재 R 역시 canonical formula가 runtime 150을 대체한다.
- 리신 E canonical Flat `200`과 total AD ratio `1.0`은 존재하지만 gameplay hook이 damage request를 만들지 않아 전혀 소비되지 않는다.
- 다음 계획은 이 값들을 무작정 삭제하지 말고 F4 표시 권위와 runtime parameter 용도를 먼저 결정해야 한다.

# 리신 상세 원인과 권장 수정 경계

## Q1/Q2

1. Q1은 `CommandExecutor`가 server projectile을 생성하고 projectile hit 시 `LeeSinQMarkComponent`를 대상에 붙인다.
2. client local skill state는 Q1 cast 즉시 3초 stage window를 열지만 Q mark를 복제받지 않는다. 따라서 사용자가 marked target을 hover하지 않으면 Q2 Unit command를 만들 수 없다.
3. server generic stage state도 Q1 cast 시 시작되어 mark의 hit 시점 3초와 어긋난다. projectile travel time만큼 실제 Q2 시간이 줄어든다.
4. Q2 command가 잘못된 target을 가리키면 generic executor는 stage/cooldown/action state를 진행한 뒤 LeeSin hook에서 missing mark로 reject할 수 있다. 결과적으로 아무 일도 없는데 Q2 기회가 소비되는 증상이 가능하다.
5. Q2 dash는 target 현재 위치에서 gap을 뺀 고정 endpoint를 만들고, 매 tick nav segment clamp가 막히면 중단한다.

권장 방향은 server가 caster 소유의 유효 mark target을 authoritative하게 해석하고, invalid Q2는 stage를 소비하기 전에 거절하며, Q2 window를 projectile hit/mark와 맞추는 것이다. Q2 이동 중 terrain segment block은 무시하되 최종 위치만 nearest walkable cell로 투영한다.

## E

현재 `OnE`는 stage를 구분하지 않고 `ApplyTempestCrippleSlow`만 호출한다. E1은 enemy mobile units in radius에 canonical physical DamageRequest를 만들고 E1-hit 표식을 남기며, E2는 그 표식 대상에게 slow를 적용하는 구조가 가장 원본 의도와 가깝다. 사용자 확정 전에는 E2를 반경 내 모든 적에게 적용할지 E1-hit 대상에만 적용할지 결정하지 않는다.

## R

현재 R은 physical damage와 airborne status만 적용한다. 기존 `ForcedMotionComponent` 경로를 재사용해 `normalize(targetPos - leePos)` 방향으로 약 10 world units 밀고, airborne duration과 이동 duration을 결합해야 한다. 벽 밖/비보행 착지는 허용하지 않고 마지막 walkable 또는 nearest walkable로 종결하는 안을 권장한다.

## Bot 검증

generated combo는 이미 `Q1 → Q2 → BA → E1 → BA → E2 → ward → W ward-hop → R` 순서다. 그러나 일반 SimLab은 GameRoom projectile collision과 mark 생성 전체를 거치지 않아 Q1→Q2→BA 성공 증거가 없다. 다음 세션은 `GameRoomProjectileIntegrationProbe` 또는 동급 server-authoritative harness에 다음 assert를 추가한다.

- Bot AI는 GameCommand 생산자이며 mark, dash, damage, attack truth를 직접 변경하지 않는다.
- Q1 projectile hit 후 target에 caster-owned mark가 생긴다.
- 3초 내 Q2 command가 accepted되고 Lee Sin 위치가 target 인접 walkable cell로 이동한다.
- Q2 후 basic attack command/result가 발생하고 target HP가 감소한다.
- terrain wall 사이에서도 Q2 transit은 통과하되 최종 위치는 walkable이다.

# 비주얼·오브젝트 판정

## 리신 ward

- server gameplay: item 3340, placement range 6, sight range 10, duration 90초, spatial radius 0.35.
- client snapshot: Ward에 VisionSensor/SpatialAgent/VisionSource/Visibility/Targetable을 구성한다.
- 현재 표현: generic structure marker billboard, 높이 0.45, 크기 0.45.
- Obsidian/asset extraction 도구는 현재 세션에 설치되어 있지 않으며 기능상 새 asset이 필요하지 않다. 권장안은 기존 Q mark/indicator texture를 작은 원형 ground decal로 재사용해 ward entity에 attach하는 것이다.

## 칼리스타 W sentinel

- 현재 floating avatar billboard와 sight-range ground cone을 둘 다 sentinel에 attach한다.
- vision truth는 별도 component이므로 avatar billboard를 생성하지 않아도 기능은 유지된다.
- 권장안은 avatar FX를 제거하고 이동·회전하는 cone만 유지하는 것이다. 꼭 avatar가 필요하다는 사용자 답이 있을 때만 작은 크기로 복구한다.

## 장로/바론

- `EventApplier::ReconcileObjectiveSnapshot`은 `Objective.Baron.Buff`와 `Objective.Elder.Buff`를 champion entity에 attach한다.
- server death cleanup이 objective state flag를 제거하고 client가 stale FX를 prune한다.
- Baron WFX는 보라색, Elder WFX는 주황/갈색 ground decal이다.
- 코드 계약은 이미 요청과 일치한다. 남은 것은 F5에서 이동 중 attachment와 죽는 프레임의 즉시 제거를 캡처하는 것이다. “캐릭터 밑에 뜸”은 ground decal 형태라 정상일 수 있으나, 월드 원점에 남거나 이동을 못 따라가는지는 live capture로만 확정한다.

# 대포 미니언 1.5배

- `ObjectVisualDefs.json`의 minion 항목에는 mesh/shader/texture만 있고 scale이 없다.
- generated `MinionVisualDefinition`에도 scale field가 없다.
- `CMinion_Manager`는 Tibbers 외 모든 역할의 Transform scale에 `m_fVisualScale=0.006`을 적용한다.
- snapshot의 `visualScaleMultiplier`는 현재 Baron empowered minion render multiplier 용도이며 siege 기본 크기와 무관하다.
- 권장 구현은 minion visual schema에 `visualScaleMultiplier` 기본값 1.0을 추가하고 blue/red siege에만 1.5를 authoring/cook한 뒤, local/network spawn 모두 `baseScale * definitionMultiplier`를 적용하는 것이다. 충돌 반경과 server gameplay 크기는 바꾸지 않는 visual-only 변경을 권장한다.

# 레드 미드 외곽 포탑 미니언 교착

첨부 재현 이미지: `C:/Users/user/AppData/Local/Temp/codex-clipboard-b31abb99-fb37-491e-b1af-218dc8423b4b.png`.

현재 코드에서 가장 강한 원인 후보는 다음 결합이다.

1. indexed avoidance가 allied Unit/minion을 hard blocker mask에서 제외해 두 근접 미니언이 좁은 동일 위치로 진입할 수 있다.
2. 후단 depenetration은 Unit과 static blocker를 함께 처리한다.
3. minion-only overlap일 때는 `ResolveForwardSafeDirection`으로 뒤쪽 성분을 제거하고 결정적 측면 분리를 하지만, static blocker가 하나라도 섞이면 raw push vector 합을 사용한다.
4. 포탑/바위의 static push와 두 미니언의 soft push가 상쇄되거나 nav clamp에 막히면 교착이 유지된다.
5. 적을 감지하면 combat chase가 lane path runtime을 clear하고 새 target 방향을 만들기 때문에 대칭이 깨져 빠져나온다.

권장 수정은 allied minion을 hard obstacle로 바꾸는 전역 변경이 아니다. soft-minion separation을 먼저 forward-safe/tangential 방향으로 정규화한 뒤 static push와 결합하고, composite candidate가 clamp될 때 static 표면의 결정적 tangent escape를 한 번 시도한다. 기존 `[MinionMove][Resolve]`, `[Depenetrate]`, `[Stuck]` trace로 exact red-mid-outer 위치의 current cell, waypoint, blocker count, correction direction을 먼저 캡처한다.

# 이번 조사에서 실행한 검증

- `Tools/Bin/Debug/SimLab.exe --shield-only`: PASS. Yasuo shield source Champion/Jungle/Minion 포함.
- `Tools/Bin/Debug/SimLab.exe --jungle-respawn-only`: PASS.
- `Tools/Bin/Debug/SimLab.exe --f4-balance-only`: PASS. DamageAuthority 17 champions / 85 skills / 323 rank cases, FormulaData, CommandResultWire, PracticeMinionAD, CooldownReload 포함.
- `python Tools/LoLData/Build-LoLDefinitionPack.py --check`: PASS. 현재 pack hash `0xAEAA5291`, 17 champions, 85 skills.
- `python Tools/LoLData/build_champion_game_data.py --check`: PASS. hash `0x6820D4E0`.
- `python Tools/Anim/patch_wanim_root_track.py ... --dry-run`: PASS, Yasuo Q3 clip에 degenerate position track 없음.
- 이번 전수조사에서는 전체 Server/Client build와 F5 live visual smoke를 실행하지 않았다.

# 문제 인터뷰 — 새 세션이 구현 계획 전에 사용자에게 확인

사용자가 `추천안대로`라고 답하면 각 항목의 추천안을 확정값으로 사용한다.

1. **리신 Q2 target 해석**
   질문: Q2는 cursor hover와 무관하게 리신이 마지막으로 Q1 mark를 붙인 유효 target을 자동 선택할까?
   추천: 예. 유효 mark가 정확히 하나일 때 server가 자동 선택하고, 없거나 만료되면 stage를 소비하지 않고 reject.

2. **Q1 벽 정책**
   질문: “벽에 안 막힘”은 terrain/nav wall만 통과하고 Yasuo Wind Wall 같은 projectile barrier에는 계속 막히는 정책인가?
   추천: terrain은 통과, champion-created projectile barrier는 유지.

3. **Q 속도 0.8배 범위**
   질문: `24 → 19.2`는 Q1 projectile에만 적용하고 Q2 dash duration `0.18초`는 유지할까?
   추천: Q1 projectile만 0.8배. Q2는 벽 통과/착지 정책만 수정.

4. **E1/E2 대상 정책**
   질문: E1은 반경 3.5의 적 mobile unit에 physical AoE damage를 주고, E2는 E1에 맞은 생존 대상만 slow할까, 아니면 현재 반경의 모든 적을 slow할까?
   추천: E1-hit 대상만 E2 slow, mark는 stage window와 함께 만료.

5. **R knockback 착지**
   질문: target을 `적 위치 - 리신 위치` 방향으로 10 world units 밀되 벽/맵 밖에서는 마지막 walkable 지점에 멈추게 할까?
   추천: 예. terrain 관통 없이 마지막/nearest walkable 착지.

6. **대포 미니언 크기 범위**
   질문: blue/red siege mesh만 시각적으로 1.5배 하고 server collision/spatial radius는 유지할까?
   추천: visual-only.

7. **ward·Kalista sentinel 표현**
   질문: Lee ward는 작은 원형 ground vision marker만, Kalista W는 floating avatar를 완전히 제거하고 moving vision cone만 유지할까?
   추천: 예. 새 asset extraction 없이 기존 texture 재사용.

8. **objective buff FX 형태**
   질문: 장로/바론은 champion 발밑 ground decal 형태를 유지하되 이동 추적과 사망 즉시 제거만 검증·보강할까?
   추천: 예. 공중 billboard로 바꾸지 않음.

9. **작업 트리 운영**
   질문: replay 세션을 중지한 채 새 세션 하나만 현재 Local checkout의 writer로 사용할까?
   추천: 예. checkpoint commit 전에는 별도 worktree로 이동하지 않고 빌드는 순차 실행.

# 다음 세션 실행 순서

1. 위 문제 인터뷰를 사용자에게 제시하고 확정값을 기록한다.
2. 현재 dirty diff에서 replay 소유 변경과 gameplay 소유 변경을 파일 단위로 다시 분류하고, 겹치는 파일은 먼저 사용자에게 알린다.
3. 동일 범위의 dated `*_PLAN.md`를 작성한다. 코드 anchor, 새 schema/codegen, 권위 경계, 예측과 검증 명령을 포함한다.
4. 독립 sub-agent read-only 비평을 받아 P0/P1 잔존 0으로 만든다.
5. 구현은 작은 묶음으로 진행한다: (A) Lee Q/Q2+bot probe, (B) Lee E/R, (C) ward/Kalista/objective visual, (D) siege scale, (E) exact minion stuck repro+resolver.
6. 각 묶음마다 generator check/SimLab 또는 GameRoom probe를 먼저 통과시키고, EngineSDK를 공유하는 aggregate build는 `/m:1` 순차 실행한다.
7. Debug Server→Client F5에서 Yasuo Q3/E, Lee combo, ward/sentinel, objective buff follow/death, siege scale, red-mid-outer minion traversal을 캡처한다.
8. 같은 이름의 `*_RESULT.md`에 예측 대 실측, 판결, 갱신된 대가를 기록하고 미검증 시각 항목을 완료로 과장하지 않는다.

# 인계 시점 잔여 위험

- 현재 dirty worktree가 매우 넓어 “빌드 성공”만으로 이번 slice 변경의 소유권을 증명할 수 없다.
- 기존 Debug SimLab binary의 PASS는 현재 소스 전부를 새로 빌드했다는 뜻이 아니다. 신규 구현 후 반드시 관련 target을 다시 빌드한다.
- objective FX와 Yasuo Q3/E는 정적 계약만으로는 사용자 체감 버그 종료를 확정할 수 없다.
- minion 교착 원인은 코드상 강한 후보이지만 첨부 위치의 live trace가 아직 없으므로 계획에서 `확정 원인`으로 표현하지 않는다.
- Yasuo 기존 slice에는 대응 `*_RESULT.md` 누락이 있어 다음 세션이 실제 완료 범위와 문서 이름을 확인해 보완한다.

## 서브 에이전트 비평

- 비평 주체: `/root/handoff_critic` 독립 read-only 검토.
- 판정: `PASS P0/P1 0`. 새 구현 세션을 오도할 내부 모순 또는 명백한 사실 오류 없음.
- 처분: 수용·기각·보류할 P0/P1 지적 없음. 구현 계획 자체의 비평은 다음 세션에서 별도로 수행한다.
