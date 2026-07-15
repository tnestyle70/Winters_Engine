Session - 미니언 공격 clip 이중 재생을 단일 traversal로 교정하고, Map11 미니맵 실측 projection을 복원하며, 피격·공격 legacy fallback 이미지 경로를 authored WFX 단일 경로로 정리한 결과를 고정한다.

## 1. 결론

- 미니언의 피해나 서버 공격 판정이 두 번 발생한 문제가 아니었다. 클라이언트 `CMinion_Manager::UpdateMinionVisual`이 한 공격에서 동일한 `attack` clip을 정방향 전체 1회 재생한 뒤, 별도 `Recover` 단계에서 역방향 전체 1회를 다시 재생한 것이 이중 공격처럼 보인 직접 원인이다.
- 한 `ActionStateComponent.sequence`에서는 `attack` 재생 함수를 정확히 한 번만 호출하도록 수정했다. 홀수 sequence는 정방향, 짝수 sequence는 역방향이며 local smoke도 공격마다 방향만 교대한다. 역방향 재생은 앞 공격의 복귀 동작이 아니라 그 자체로 다음 공격 1회다.
- 서버의 `attackWindup`, `attackRecovery`, 단일 `bHitFired`/투사체 생성 계약은 바꾸지 않았다. 따라서 시각 방향 교대가 피해 횟수나 투사체 수를 늘리지 않는다.
- 미니맵 축소 문제는 아이콘 크기가 아니라 world-to-UV projection 범위를 NavGrid 256 회전 정사각형으로 과대 해석한 것이 원인이었다. 실제 Stage landmark가 미니맵 중앙 약 50%에만 투영되면서 전체 오브젝트가 2배 안팎 작게 모여 보였다.
- projection 중심 `(104.5, 0)`은 유지하고 실제 Map11 구조물·웨이포인트 span에 맞춰 두 축을 재보정했다. 같은 projection을 소비하는 유닛/구조물 아이콘, 카메라 박스, 클릭 이동, FOW가 함께 보정된다.
- 공격·피격 fallback은 런타임 생성 분기를 제거했다. 일반 projectile/hit ring, physical hit card, turret top-beam, fullscreen hit/stun overlay와 Annie/Jax/Yone/Yasuo/Irelia의 WFX 실패 후 추가 생성 경로가 더 이상 실행되지 않는다.
- 실제 authored WFX가 텍스처를 재료로 쓰는 경우가 있으므로 공유 PNG 자산 자체를 무차별 삭제하지 않았다. 제거 대상은 WFX 실패 시 별도 이미지를 중복 생성하던 fallback 경로이며, fallback 전용 Annie 소스 파일은 프로젝트와 디스크에서 삭제했다.

## 2. 원인과 코드 근거

### 2-1. 미니언 이중 공격 모션

- 변경 전 흐름:
  - 새 공격 sequence 수신.
  - `PlayAnimationByNameAdvanced("attack", false, false, 1.f)` 호출.
  - clip 종료 뒤 `Recover` 진입.
  - 같은 `attack` clip을 `reverse=true`로 다시 호출.
  - 결과적으로 서버 공격 1회에 시각 clip traversal 2회가 대응했다.
- 변경 후 흐름:
  - 새 공격 sequence 수신.
  - sequence parity로 정방향 또는 역방향 한 방향을 선택.
  - `attackWindup + attackRecovery` 동안 clip 전체를 한 번만 재생.
  - cycle 종료 뒤 바로 idle/run base animation으로 복귀.
- `bPendingAttack`과 `Recover` 상태를 삭제했으므로 이전 공격의 역재생 구간에 다음 공격을 보류했다가 추가 재생하는 경로도 사라졌다.
- Debug에서는 최대 128건까지 아래 형식으로 확인할 수 있다.

```text
[MinionAnim] entity=<id> seq=<n> direction=forward|reverse clip=<sec> cycle=<sec>
```

### 2-2. 미니맵 스케일 축소

- 변경 전 기준점:

```text
UV(0,0) = World(104.50, 181.02)
UV(1,0) = World(285.52,   0.00)
UV(0,1) = World(-76.52,   0.00)
```

- 이 projection의 world footprint는 두 축 모두 약 362.04였지만, 실제 Stage 구조물과 경로 landmark는 그 절반 안팎의 범위에 있다. 중심은 맞아도 오브젝트가 미니맵 중앙으로 압축되는 이유다.
- 변경 후 기준점:

```text
UV(0,0) = World(104.500, 156.690)
UV(1,0) = World(198.885,   0.000)
UV(0,1) = World( 10.115,   0.000)
```

- 수학 검증 결과:

```text
World center (104.5, 0.0)       -> UV (0.500000, 0.500000)
Blue Nexus  (22.7776, 0.9204)   -> UV (0.064143, 0.929983)
Red Nexus   (187.0120, 0.1020)  -> UV (0.936778, 0.062571)
Stage 구조물+웨이포인트 57개     -> U 0.061015..0.946078 / V 0.058876..0.950898
```

### 2-3. 전투 fallback 이미지

- `ProjectileVisualDesc`에서 fallback texture, 크기, enable 필드를 삭제하고 spawn/hit/attach cue만 남겼다.
- `CEventApplier`에서 다음 generic visual을 삭제했다.
  - 이름 없는 projectile billboard.
  - 이름 없는 projectile hit billboard.
  - 처리되지 않은 EffectTrigger의 공용 ring.
  - champion 간 Damage 이벤트의 공용 physical hit card.
  - turret projectile 시작 시 별도로 만들던 top-beam 이미지.
- `CUI_Manager`에서 HP 감소와 stun에 반응하던 fullscreen legacy image overlay의 로드, timer, draw, tuner 상태를 모두 삭제했다. HUD frame, 상태창, circular portrait, passive, damage number, HP bar/trail은 유지했다.
- Annie fallback 전용 `.cpp/.h`와 vcxproj 항목을 삭제했다.
- Jax/Yone/Yasuo/Irelia는 cue 실패 뒤 PNG 또는 수동 mesh/billboard를 만드는 분기를 없애고 WFX 호출 한 번으로 종료한다.
- Irelia의 Q trail, E blade placement, R blade fan은 실패 시 대체하는 fallback이 아니라 원래의 primary visual이므로 유지했다.
- 정상 F5에서 사용되지 않는 `--show-snapshot-markers` 진단 표식은 전투 fallback 계약과 분리된 명시적 debug 기능이므로 이번 범위에서 유지했다.

## 3. 실제 반영 파일

### 3-1. 미니언과 미니맵

- `Client/Public/Manager/Minion_Manager.h`
- `Client/Private/Manager/Minion_Manager.cpp`
- `Client/Public/UI/MinimapPanel.h`

### 3-2. 공용 projectile/event/HUD fallback

- `Client/Public/GameObject/Projectile/ProjectileVisualCatalog.h`
- `Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp`
- `Client/Public/Network/Client/EventApplier.h`
- `Client/Private/Network/Client/EventApplier.cpp`
- `Engine/Public/Manager/UI/UI_Manager.h`
- `Engine/Private/Manager/UI/UI_Manager.cpp`

### 3-3. 챔피언 fallback

- `Client/Private/GameObject/Champion/Annie/Annie_Skills.cpp`
- `Client/Private/GameObject/Champion/Jax/Jax_FxPresets.cpp`
- `Client/Private/GameObject/Champion/Yone/Yone_FxPresets.cpp`
- `Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp`
- `Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp`
- `Client/Private/GameObject/FX/FxLegacyManifest.cpp`
- `Client/Include/Client.vcxproj`
- `Client/Include/Client.vcxproj.filters`
- 삭제: `Client/Private/GameObject/Champion/Annie/Annie_FxPresets.cpp`
- 삭제: `Client/Public/GameObject/Champion/Annie/Annie_FxPresets.h`

### 3-4. 이전 설계 폐기 표시

- `Plan/S001_MINION_MOVEMENT_TARGETING_ATTACK_ANIMATION_SESSION_20260711.md`
- `Plan/S001_MINION_NAVGRID_RESULT_20260711.md`
- `.md/plan/2026-06-24_GAMEPLAY_DASH_WALL_MINIMAP_TURRET_CURSORLOCK_PLAN.md`
- `.md/plan/2026-06-24_GAMEPLAY_DASH_WALL_MINIMAP_TURRET_CURSORLOCK_RESULT.md`

## 4. 자동 검증 결과

- `git diff --check`: PASS. 저장소의 기존 LF/CRLF 변환 안내만 출력됐다.
- 폐기 심볼 `Recover`, `bPendingAttack`, `reverseSpeed`: 대상 minion 파일에서 0건.
- `attack` animation 시작 호출: `CMinion_Manager::UpdateMinionVisual`에서 1개 경로.
- 제거 대상 projectile/HUD/champion fallback 심볼: 대상 런타임 범위에서 0건.
- Annie fallback 프로젝트 참조: 0건.
- `Client.vcxproj`, `Client.vcxproj.filters` XML parse: PASS.
- 관련 named WFX cue 파일 존재 검사: 36개 PASS.
- Engine Debug x64 build/link: PASS.
  - 산출물: `Engine/Bin/Debug/WintersEngine.dll`.
  - post-build EngineSDK 배포와 Client DLL 복사가 완료됐다.
- Client Debug x64 build/link: PASS.
  - 산출물: `Client/Bin/Debug/WintersGame.exe`.
- Server Debug x64 build/link: PASS.
  - 산출물: `Server/Bin/Debug/WintersServer.exe`.
- 남은 경고는 기존 DLL interface `C4251/C4275`, `UI_Manager.cpp`의 기존 invalid UTF-8 `C4828`, `ChampionSpawnService.cpp`의 기존 format `C4477`이다. 이번 변경에서 새 컴파일 오류는 발생하지 않았다.

## 5. 사용자 인게임 검증 절차

### 5-1. 미니언 공격

1. 새 Server와 Client 프로세스로 인게임에 진입한다.
2. melee 미니언 한 개체를 정해 연속 공격 4회를 본다.
3. 기대 결과:
   - 공격 1회당 팔/무기 motion은 정확히 한 번이다.
   - Debug 로그는 같은 entity에서 sequence마다 한 줄이고 방향은 `forward/reverse/forward/reverse`로 교대한다.
   - 정방향 뒤 즉시 역방향을 붙여 한 공격처럼 재생하는 동작은 없다.
   - 대상 HP 감소는 공격당 한 번이다.
4. ranged 미니언도 같은 방식으로 확인한다.
5. 기대 결과:
   - 공격 motion 한 번과 투사체 한 개가 대응한다.
   - 역방향 공격 차례에도 투사체가 두 개 생기지 않는다.

### 5-2. 미니맵

1. Blue/Red Nexus가 각각 좌하단/우상단 base 원형 중심에 놓이는지 본다.
2. 세 lane outer turret와 미니언이 배경 lane landmark를 따라 미니맵 전체 범위를 사용하는지 본다.
3. 카메라 박스, 미니맵 클릭 이동, FOW 경계가 아이콘과 같은 위치를 가리키는지 확인한다.
4. 기대 결과는 첨부 화면처럼 오브젝트가 중앙 작은 영역에 뭉치지 않고 대략 UV 0.06~0.95 범위를 사용한다.

### 5-3. fallback 폐기

1. melee/ranged 기본 공격과 champion BA/Q를 각각 맞고 때린다.
2. generic 밝은 ring, 공용 physical hit card, 이름 없는 projectile 이미지, fullscreen hit/stun 이미지가 한 번도 보이지 않아야 한다.
3. Annie/Jax/Yone/Yasuo/Irelia 대표 공격에서는 authored WFX만 보인다.
4. cue가 누락된 경우 대체 PNG가 나타나지 않고 Debug Output의 `[FxCuePlayer] Missing cue`만 남는 것이 정상이다.

## 6. 남은 경계

- 현재 WANIM에는 contact event가 없다. 따라서 공격 motion이 한 번이라는 계약과 서버 피해 1회는 보증하지만, 칼날이 대상에 닿는 정확한 프레임과 서버 windup impact의 프레임 단위 일치까지 자동 보증하지는 않는다.
- 수동 캡처에서 접촉 시점만 어긋나면 다음 단계는 asset별 normalized impact marker와 piecewise time mapping이다. 같은 clip을 두 번째로 역재생하는 방식으로 되돌리면 안 된다.
- turret top-beam fallback을 제거했기 때문에 turret muzzle 연출이 필요하면 `Turret.Attack.Red` 같은 authored cue를 authoritative attack event에 연결해야 한다.
- 이번 턴에서는 사용자가 직접 인게임을 확인하기로 했으므로 Client/Server 창을 자동 실행하지 않았다.
