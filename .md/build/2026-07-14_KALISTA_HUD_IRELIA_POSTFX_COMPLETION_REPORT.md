# Session - 2026-07-14 Kalista, HUD XP, Irelia R, LDR PostFx completion report

## 결론

이번 세션 범위는 코드와 표준 Debug 산출물까지 반영되었다.

- Kalista 기본 공격은 서버 권위 투사체가 속도 `30`으로 이동 대상을 계속 추적하고, 대상이 유효한 동안 최초 위치나 최초 사거리에서 임의 종료하지 않는다.
- Kalista Q는 서버 권위 직선 투사체이며 속도 `27`, 사거리 `16.5`, 첫 유효 충돌 소멸 계약을 사용한다.
- Kalista는 서버 스폰 시 6번 슬롯(index 5)에 전용 아이템 `3599`를 받는다. 6키+아군 hover로 결속 의식을 시작하며, 성공한 경우에만 아이템을 소비한다.
- 결속 대상은 HP를 조작하지 않고 `DeathStart` 애니메이션을 재생한 뒤 1.5초 후 `Idle/Bound` 상태로 복귀한다.
- Kalista R은 인간 아군과 봇을 분리한다. 인간 아군은 carried 상태에서 좌클릭 지점을 선택할 때까지 유지되고, 봇은 carried 진입 후 최대 3초 뒤 Kalista 전방으로 자동 발사된다.
- 초상화 우측 XP fill/track은 같은 atlas silhouette과 같은 목적지 사각형을 사용한다.
- Irelia R 칼날 벽은 타원이 아니라 `왼쪽 끝점 -> 전방 꼭짓점 -> 오른쪽 끝점`의 두 직선 쐐기 형태이며 E 본체 FBX를 사용한다. E 공전 칼날 생성 경로는 호출하지 않는다.
- DX11 LDR PostFx는 FX 뒤, HUD/미니맵/스크린 오버레이 전에 실행되며 기본값은 OFF다.

## 권위와 소유 경계

Kalista 피해 결과와 궁극기 상태 전이는 `Shared/GameSim`이 소유한다.

```text
Client input
  -> GameCommand
  -> Server GameSim projectile / oath / Fate's Call state
  -> replicated event and snapshot
  -> Client projectile, animation, FX, HUD presentation
```

네트워크 모드에서는 기존 로컬 BA/Q 투사체를 중복 생성하지 않는다. 로컬 스모크 경로만 같은 수치와 homing 동작을 가진 fallback을 유지한다.

## 구현 증거

### Kalista 기본 공격과 Q

- `Shared/GameSim/Components/ProjectileKindComponent.h`
  - `KalistaBasicAttack = 16`
  - `KalistaPierce = 17`
- `Shared/GameSim/Champions/Kalista/KalistaGameSim.cpp`
  - BA: target entity, speed `30.f`, 유효 대상 실시간 추적, 접촉 시 지연 피해 확정
  - Q: speed `27.f`, range definition fallback `16.5f`, 첫 충돌 제한
- `Shared/GameSim/Systems/Combat/CombatActionSystem.cpp`
  - Kalista BA 즉시 피해를 보류하고 투사체 적중 시점으로 이관
- `Client/Private/GameObject/Champion/Kalista/Kalista_Registration.cpp`
  - 권위 이벤트에서는 legacy 로컬 이동 창 중복 생성 차단
- `Client/Private/GameObject/Champion/Kalista/KalistaProjectileSystem.cpp`
  - 로컬 스모크 BA도 target handle을 매 tick 따라간다.
- `Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp`
  - 권위 투사체 kind를 Kalista BA/Q WFX로 연결
- `Data/LoL/FX/Champions/Kalista/*.wfx`
  - BA/Q 이동 창과 적중 시 Rend 창 비주얼 정의

### Kalista 3599 결속과 R

- `Shared/GameSim/Components/KalistaBondComponent.h`
  - item `3599`, inventory index `5`, binding/bound/pulling/carrying/launching 계약
- `Server/Private/Game/GameRoomSpawn.cpp`
  - Kalista 서버 스폰 inventory slot 6 초기화
- `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`
  - 서버 검증 성공 후에만 3599 소비
- `Shared/GameSim/Champions/Kalista/KalistaGameSim.cpp`
  - 1.5초 death-animation ritual, Bound 전환, R pull/carry/launch, 인간/AI 분기, orphan cleanup
- `Client/Private/Scene/Scene_InGameInput.cpp`
  - 6키+hover 결속 명령
  - carried 인간 아군의 marker-gated 좌클릭 launch
- `Client/Private/GamePlay/LoLUIContentRegistry.cpp`
  - 3599 초록 창 아이콘을 HUD 텍스처 목록에 등록
  - 상점에서는 disabled/non-purchasable
- `Engine/Private/Manager/UI/UI_Manager.cpp`
  - Lua UI 사용 여부와 무관하게 HUD item SRV 로드
  - disabled item 상점 미표시, non-purchasable 구매 거부

### XP 바

- runtime atlas cell
  - fill: `(631, 869, 48, 120)`
  - track: `(729, 869, 48, 120)`
- runtime layout rect
  - fill/track 공통: `(252.00, 48.50, 36.00, 90.00)`
- fill binding
  - `bind = xpRatio`
  - `clip = bottomToTop`
- alpha threshold 128 기준 fill/track mask IoU: `0.9975`
- `Engine/Private/Manager/UI/ActorHUDPanel.cpp` fallback도 같은 위치와 크기로 맞췄다.

이 수정의 핵심은 이미지를 조각내 붙이는 것이 아니다. 동일한 세로 arc silhouette 두 개를 같은 목적지에 겹치고, fill만 XP 비율만큼 아래에서 위로 clip하는 방식이다.

### Irelia R

- `Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp`
  - normalized forward 기준 left/right endpoint와 forward tip 계산
  - 홀수 25개 샘플을 두 직선 위에 배치하여 중앙 tip을 정확히 포함
  - `irelia_base_e_blade.fbx`와 `eBladeColor` 사용
  - E FBX 피벗 기준을 맞추기 위해 양 끝점·꼭짓점·이동 시작점·최종점 모두 `vHitPos.y + 3.f` 사용
  - `CIreliaBladeSystem::SpawnPlaced` 미호출
- `Client/Public/GameObject/Champion/Irelia/IreliaFxPresets.h`
  - triangle/non-triangle 계약과 공전 칼날 미생성 명시
- `Client/Public/GameObject/Champion/Irelia/Irelia_Tuning.h`
  - R blade count 25

### DX11 LDR PostFx

- `Shaders/PostFx/PostFx.hlsl`
  - exact passthrough `Load`
  - grade, gamma, saturation, tint, vignette
  - quarter-resolution bloom extract/blur/composite
- `Engine/Public/Renderer/PostFxPass.h`
- `Engine/Private/Renderer/PostFxPass.cpp`
- `Client/Private/Scene/Scene_InGameRender.cpp`
  - world/FX 완료 후 실행
  - UI overlay/minimap/screen overlay 전에 실행
- `Client/Private/Scene/Scene_InGameLifecycle.cpp`
  - DX11 생성, 기본 OFF
- `Client/Private/UI/RenderDebug.cpp`
  - F1 tuning과 passthrough/LoL Subtle preset

## 검증 결과

| Gate | 결과 |
|---|---|
| Engine Debug 표준 빌드 | PASS |
| GameSim Debug 표준 빌드 | PASS |
| Server Debug 표준 빌드 | PASS |
| Client Debug 표준 빌드 | PASS |
| Engine/GameSim/Server/Client 격리 빌드 | PASS |
| full `SimLab.exe` | PASS |
| `SimLab.exe --kalista-projectile-only` | PASS |
| Kalista R probe | PASS: ritual, Bound, 인간 명시 launch, AI 자동 launch, airborne, orphan cleanup |
| Kalista projectile probe | PASS: BA 30 homing/deferred impact, Q 27/16.5 straight first-hit |
| ChampionGameData freshness | PASS, `0x3A6CDBF9` |
| LoL definition pack freshness | PASS, `0xB6EEDF6E` |
| Kalista WFX JSON 3개 | PASS |
| Engine project/filter XML | PASS |
| PostFx HLSL 4 entry `fxc` | PASS |
| XP fill/track perceptual mask | PASS, IoU `0.9975` |
| full `git diff --check` | PASS; line-ending 안내만 존재 |

표준 실행 파일:

- `Client/Bin/Debug/WintersGame.exe`
- `Server/Bin/Debug/WintersServer.exe`

종료된 런타임을 대신해 hidden startup smoke도 수행했다. 서버는 `0.0.0.0:9000` listen과 45초 smoke loop를 정상 완료했고, 최종 표준 산출물도 PTY에서 listen 상태를 재확인한 뒤 `q`로 정상 종료했다. Client는 이전 smoke에서 약 42초, 최종 산출물 재검증에서 10초 동안 crash 없이 살아 있었다. hidden 실행에서는 visible window와 established TCP endpoint가 관찰되지 않았으므로 이 결과를 실제 인게임 시각 품질 증명으로 확대 해석하지 않는다.

## 수동 인게임 canary

표준 Debug 빌드로 다음을 한 번에 확인한다.

1. Kalista BA가 횡이동하는 적을 접촉까지 추적하고, 최초 위치를 통과해 허공에서 사라지지 않는지 확인한다.
2. Q가 기존 대비 3배 속도와 1.5배 사거리로 보이며 첫 적에서 끝나는지 확인한다.
3. 시작 inventory 6번에 초록 창이 보이고 상점에는 노출되지 않는지 확인한다.
4. 6키+아군 hover가 death animation 후 정상 pose로 복귀하고 아이템을 한 번만 소비하는지 확인한다.
5. 인간 아군 R은 좌클릭 전까지 carried를 유지하고, 봇은 3초 뒤 Kalista forward로 나가는지 확인한다.
6. XP 0%, 중간, 100%에서 보라 fill이 회색 track과 같은 arc 안에만 남는지 확인한다.
7. Irelia R이 타원이 아니라 두 직선의 쐐기로 보이고 E 공전 소형 칼날이 없는지 확인한다.
8. F1 PostFx OFF가 기존 화면과 같고, LoL Subtle preset이 HUD에는 적용되지 않는지 확인한다.

## 협업 주의

- `Engine/Private/Manager/UI/UI_Manager.cpp`와 `ActorHUDPanel.cpp`는 조사 시점 Active packet S004 소유 파일이었다. 변경은 요구 범위에 한정했고 표준 빌드까지 통과했지만, commit 전에 S004 소유 세션과 diff를 합쳐야 한다.
- `Client/Bin/Resource/UI/hud_irelia_layout.json`, `hud_atlas_manifest.json`, `Client/Bin/Resource/Texture/UI/Items/3599_kalistapassiveitem.png`는 현재 `.gitignore` 대상이다. 이 워크스페이스 런타임에는 반영됐지만 새 checkout에 자동 보존되는 source-of-truth는 아직 없다. 유실 시 XP crop은 회귀하고 3599 아이콘은 숫자 fallback으로 보일 수 있으므로 resource checkpoint에서 강제 추적하거나 별도 동기화 소유자를 정해야 한다.
- 적중 시 Rend 창 비주얼은 6초 presentation lifetime을 쓴다. Shared에 영구 Rend stack ledger가 아직 없으므로 E 소비와 정확히 연동되는 영구 창 수명은 별도 범위다.
- 다른 세션이 수정 중인 unrelated dirty 파일은 정리하거나 되돌리지 않았다. stage/commit도 수행하지 않았다.
