Session - S018 이즈리얼 투사체 권위·복제 vertical slice를 구현하고 Shared/GameSim, Server, Client 제품 빌드, 실제 GameRoom 통합 probe와 독립 계약 하네스로 검증했다.

# S018 Ezreal Projectile Authority 결과 보고서

작성일: 2026-07-13

상태: `Handoff`

## 1. 완료 결과

이즈리얼 기본 공격과 Q/W/E/R의 판정 결과를 `Shared/GameSim -> Server -> Event/Snapshot -> Client Presentation` 단일 권위 흐름으로 연결했다.

- 기본 공격: 대상 추적형 투사체, 최초 유효 접촉 후 소멸, W 표식 격발 가능, 패시브 스택은 증가시키지 않는다.
- Q: 최초 유효 유닛 접촉 후 소멸하고 피해·on-hit·쿨다운 환급을 한 번만 적용한다.
- W 1타: 피해 없이 source-target 표식을 생성 또는 갱신하고 투사체는 소멸한다.
- W 2타: 같은 이즈리얼의 표식만 소비하고 `60 + 명령 수락 시 실제 지불 마나`를 회복한다. 기본 공격 격발은 마나를 회복하지 않는다.
- E: 4.75 범위 clamp, 짧은 지점 partial blink, 후방 탐색 기반 착지 보정과 원점 fallback을 적용한다. 표식 챔피언, 표식 구조물, 가장 가까운 유효 대상 순으로 bolt 대상을 고른다.
- R: 대상별 한 번만 적중하는 ordered unique pierce를 사용하며 챔피언과 non-epic 대상 공식을 구분한다.
- Rising Spell Force: 범용 `BuffComponent`로 적중당 1스택, 최대 5스택, 스택당 공격 속도 10%, 180 tick `[T,T+180)` 만료를 구현했다.

## 2. 공용 투사체·이동 기반

- 투사체의 유닛 접촉 정책을 destroy/pierce로 분리하고 target mask, 최대 고유 적중 수, 장막 차단 여부, 지형 충돌 여부, source 사망 후 존속 여부를 권위 컴포넌트가 소유한다.
- 이동 표적의 `T-1 -> T` 상대 운동을 사용하는 swept CCD, 접촉 시간 양자화, 동일 TOI의 `EntityHandle` 순서 결정을 적용했다.
- Flash, 이즈리얼 E, 제드 W/R, 귀환, 야스오 R, 요네 귀환 보정 같은 순간 위치 변경은 `PositionDiscontinuityComponent`로 표시해 허위 swept hit를 막는다.
- 모든 현재 `SkillProjectileComponent` 생성 경로와 포탑 `StructureProjectileComponent` 생성 경로가 source generation handle을, targeted projectile인 경우 target generation handle도 발사 시점에 저장한다. 엔티티 슬롯 재사용 뒤 이전 투사체가 새 소유자나 새 표적을 가리키지 않는다.
- projectile/source/target Net ID를 spawn 시점에 보존하고 terminal contact enqueue 직후 projectile Net binding을 해제한다.
- keyframe raw component layout 변경을 명시하기 위해 `WKF1` 포맷 버전을 2로 올렸다. v1 blob은 migration 없이 헤더에서 명시적으로 거부한다.

## 3. 복제와 클라이언트 수렴

- Event/Snapshot FlatBuffers는 기존 필드 순서를 유지하고 table tail에만 필드를 추가했다.
- contact reason과 contact ordinal, event ordinal, projectile target Net ID, 방향·이동 거리, persistent gameplay state를 복제한다.
- Client mutation 순서를 `(serverTick, phase, eventOrdinal)`로 고정해 같은 tick의 spawn/contact와 W mark/clear가 역순 도착해도 terminal/clear가 이긴다.
- event-first, snapshot-first, snapshot-only가 같은 투사체 presentation으로 수렴한다.
- full snapshot에서만 보이지 않는 projectile/W 표식/Yasuo wall을 삭제하고 delta snapshot absence는 삭제로 해석하지 않는다.
- timeline rebase 시 projectile visual, W relation, wall, Net binding, dedupe와 mutation stamp를 함께 비운다.
- 중간 접속 시 이즈리얼 W·Rising Spell Force와 야스오 장막 `(sourceNet, spawnTick)` 상태를 남은 수명·위치·방향·폭으로 복원한다.

## 4. 데이터 결과

- LoL definition pack: `0x0498E9BA` (`17 champions / 85 skills / 1 summoner spell`)
- ChampionGameData: canonical 계산값과 generated 값 모두 `0xB8EF76C4`
- 이즈리얼 패시브 데이터: `maxStacks=5`, `stackWindowSec=6`, `bonusAttackSpeed=0.10`
- Q/W/R effect params의 중복 `range`를 제거하고 ChampionGameData의 skill range를 단일 소유자로 유지했다.

## 5. 최종 검증

| 검증 | 결과 |
|---|---|
| Shared/GameSim Debug x64 | PASS |
| SimLab Debug x64 build | PASS |
| `SimLab.exe --ezreal-only` | PASS |
| `SimLab.exe 1800 42` | PASS |
| 동일 시드 결정론 | `DB0DC85E451999AD` |
| seed+1 민감도 | `57A9B2394575042A` |
| Server Debug x64 build/link | PASS |
| Client Debug x64 build/link | PASS |
| 실제 GameRoom projectile integration: Ezreal BA·포탑 lifecycle, skill/structure target generation 재사용, serializer delayed-unbind, Rising Spell Force pre-command expiry | PASS |
| Server projectile authority contract | PASS |
| Projectile replication append-only field ID + current-schema omitted-tail default contract | PASS |
| Production presentation mutation ordering comparator contract | PASS |
| Shared dependency boundary | PASS |
| LoL definition pack `--check` | PASS |
| ChampionGameData canonical/generated hash 비교 | PASS |
| Yasuo wind wall WFX JSON parse | PASS |
| legacy API/termination enum scan | PASS |

최종 산출물 SHA-256:

```text
SimLab.exe        42539F6E8FBC84C832D716CB7C6B04FE050AB7A7DB2C724BEA292A0DFA1ECEEE
WintersServer.exe 0C19DC341338CBF38739077A29B394EE2B433360FFC36F889E7D18656D3F0F9E
WintersGame.exe   B386203F7BCDF536C1726F5428177A7B7D7E719482F6D1122AB7945E8EDCFB4B
```

빌드 오류는 0개다. 최종 Client 재빌드에는 기존 EngineSDK DLL interface C4275 계열 경고가 남았고, 앞선 full rebuild에서 관측된 `ChampionSpawnService.cpp` C4477도 S018 범위 밖이라 변경하지 않았다.

## 6. 의도적으로 남긴 후속 경계

- 실제 GameRoom의 Ezreal BA·포탑 projectile lifecycle, skill/structure target 슬롯의 동일 ID·새 generation 재사용, serializer delayed-unbind, passive pre-command expiry는 integration probe로 검증했다. 다만 Client packet delivery를 포함한 event-first/snapshot-first/terminal-loss 다중 클라이언트 렌더 스모크는 아직 실행하지 않았으며, Client 쪽 완료 근거는 제품 링크와 production mutation-order comparator contract다.
- wire probe는 현 generated schema에서 신규 tail field를 생략했을 때의 기본값과 append-only field ID를 검사한다. 이전 generated schema 바이너리로 만든 historical byte fixture가 필요한 cross-version release gate는 후속이다.
- Barrier/Terrain/Expire 전용 cue와 `Ezreal.BA.Hit` 승인 에셋이 없어 catalog 값은 비어 있다. BA hit은 현재 Q hit cue를 재사용한다.
- 지형 first-contact TOI/normal API가 아직 없으므로 이즈리얼은 `bCollidesWithTerrain=false` 범위로 출하한다.
- designer-authorable `ProjectileGameplayDefs.json`과 cooker/profile 계약은 별도 패킷이다. 검증되지 않은 런타임 JSON 경로를 이번 세션에 추가하지 않았다.
- 기본 공격 확정 receipt 계약이 없어 이즈리얼 Q의 기존 Lethal Tempo 직접 호출은 이번 세션에서 제거하지 않았다. `DamageFlag_OnHit`만으로 일반화하면 스킬 오발동과 실드 적중 누락이 생길 수 있다.
- keyframe v1에서 v2로의 cross-build migration은 제공하지 않는다. 같은 빌드의 save/restore와 결정론은 검증됐으며 이전 blob은 fail-closed다.

## 7. 협업·handoff

기존 AI 작업을 포함한 dirty worktree를 reset/revert하지 않았고 stage/commit도 수행하지 않았다. S018 구현과 검증 결과는 다음 계획과 work packet을 기준으로 이어받는다.

- `.md/plan/2026-07-12_S018_EZREAL_PROJECTILE_AUTHORITY_REMAINING_PLAN.md`
- `.md/collab/work-packets/2026-07-12_s018_ezreal_projectile_authority.md`
