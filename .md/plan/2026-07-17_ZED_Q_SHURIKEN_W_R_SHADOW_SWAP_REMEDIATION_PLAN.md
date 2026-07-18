Session - Zed Q 수리검 축/재질 및 W·R 독립 4초 그림자 교환 복구
좌표: Client Projectile/Fx · Shared/GameSim Zed · LoL Definition Pack
관련: `.claude/gotchas.md`, `CLAUDE_Legacy.md`, `.md/architecture/WINTERS_CODEBASE_COMPASS.md`

## 1. 목표와 완료 기준

① 실패 박제: Q는 투사체 Yaw가 WFX 기본 회전을 덮고 렌더 아이콘 PNG를 UV 텍스처로 사용해 서 있고 형상이 깨져 보였다.
② W 계약: W1 시 기본 쿨다운과 별개인 4초 재입력 창/그림자 수명을 동시에 시작하고, W2는 서버 권위로 본체와 W 그림자 위치를 1회 교환한다.
③ R 계약: R1은 대상 정면의 반대 방향 좌표로 제드를 이동시키고 시전 전 좌표에 독립 R 그림자를 4초 남기며, R2는 같은 방식으로 위치를 교환한다.
④ 공존 계약: W/R 그림자는 서로 덮어쓰지 않고, 살아 있는 두 그림자 모두 Q/E 복제 원점이 된다. 그림자 엔티티에는 ChampionComponent와 체력바를 만들지 않는다.
⑤ 완료: 데이터 생성기 `--check`, JSON/WFX 파싱, 잔존 단일-shadow 참조 검사, `git diff --check` 통과. 실행 중 Debug 프로세스는 재시작하지 않으며 빌드/인게임은 후속 검증으로 남긴다.

범위 상한은 이번 세션 작업량의 30%로 고정한다. 제드 Q/W/R 계약과 그 데이터 생성 경로 외 리팩터링은 하지 않는다.

## 2. 구현 좌표와 변경 계약

### 2.1 서버 권위 그림자 상태

- `Shared/GameSim/Components/ZedSimComponent.h`의 `ZedSimComponent`를 아래 구조로 교체한다.

```cpp
struct ZedShadowState
{
    bool_t bActive = false;
    u8_t reservedAlignment[3]{};
    Vec3 vPosition{};
    Vec3 vDirection{ 0.f, 0.f, 1.f };
    f32_t fRemainingSec = 0.f;
};

struct ZedSimComponent
{
    ZedShadowState wShadow{};
    ZedShadowState rShadow{};
};
```

- `Shared/GameSim/Champions/Zed/ZedGameSim.cpp`의 단일 그림자 접근을 `ResolveShadowState(slot)` 아래로 교체한다.
- `OnW` stage 1은 `wShadow`, `OnR` stage 1은 `rShadow`를 생성하고 stage 2는 `cmd.slot`의 그림자만 교환한다.
- `CommandExecutor.cpp`는 W1 gameplay hook 이후 실제 서버 clamp 좌표를 generic effect event 위치로 다시 읽되, W2/R2는 hook 이전 본체 좌표를 유지해 그림자 이동 좌표로 사용한다.
- `OnQ`/`OnE`의 그림자 원점 수집은 `{ W, R }`을 순회한다.
- `Tick`은 두 수명을 각각 감소시키며 0초에 해당 상태만 비활성화한다.
- `ResolveDeathMarkLandingPosition`은 아래 서버 공식을 유지한다.

```cpp
behind = NormalizeXZ(-targetForward, -fallbackDirection);
distance = casterRadius + targetRadius + gap;
landing = ResolveWalkable(targetPosition + behind * distance);
```

### 2.2 입력·클라이언트 그림자 표현

- `Client/Private/Scene/Scene_InGameInput.cpp`의 Zed W/R 키 분기에서 로컬 stage window 또는 동일 slot 그림자가 살아 있을 때 stage 2를 전송한다.
- `Client/Private/GameObject/Champion/Zed/ZedFxPresets.cpp`의 소유 키를 `owner + sourceSlot`으로 교체하고 `ChampionComponent` 추가 블록을 삭제한다.
- `MoveShadowCloneModel(owner, slot, oldPlayerPosition, direction)`은 해당 slot 그림자만 옛 본체 좌표로 옮기고 재교환 가능 플래그를 닫는다.
- `Client/Private/GameObject/Champion/Zed/Zed_Skills.cpp`에서 R stage 2는 `target == caster`이면 R 그림자 이동, 적 대상이면 기존 Death Mark 폭발 FX로 구분한다.

### 2.3 Q 메쉬 축과 텍스처

- 확인된 `zed_shuriken.wmesh` AABB는 X가 두께축, YZ가 날 평면이다. Pitch(X) 90도가 아니라 Roll(Z) 90도로 XZ 평면에 눕힌다.
- `Data/LoL/FX/Champions/Zed/q_projectile.wfx`의 MeshParticle에 아래 값을 사용한다.

```json
"texture": "Client/Bin/Resource/Texture/Character/Zed/particles/zed_shuriken_tx.png",
"rotation": [0.0, 0.0, 1.5707963],
"world_yaw_spin_speed": 12.566
```

- `ProjectileVisualDesc`에 제드 전용 `bSuppressMeshDirectionalYaw`를 추가하고 `EventApplier::EnsureProjectilePresentation`의 메쉬 Yaw 최종 writer에서만 0을 유지한다. 다른 투사체 Transform Yaw와 FX는 변경하지 않는다.

### 2.4 데이터 원본과 생성기

- `Data/Gameplay/ChampionGameData/champions.json`에서 Zed Q=`Direction`, W=`GroundTarget → Self`, R=`UnitTarget → Self`, W/R stage window=`4.0`으로 고정한다.
- `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`의 Zed W/R `effectDurationSec`를 `4.0`으로 고정한다.
- `Data/LoL/ClientPublic/Visual/ChampionVisualDefs.json`의 Zed R에 재입력 stage를 추가한다.
- `Tools/ChampionData/build_champion_game_data.py`는 stage별 선택적 `targetMode`를 정규화한다.
- `Tools/LoLData/Build-LoLDefinitionPack.py`는 stage별 `targetShapes[]`와 facing을 생성하고, 서로 다르면 `StageDependent` 정책을 낸다.

## 3. 예측, 검증, 인계

예측: Q는 모든 진행 방향에서 수평을 유지하며 초당 약 2회전한다. W/R은 각각 4.0초 동안 1회 재입력 가능하고 서로 동시에 존재할 수 있다. R 착지는 대상이 바라보는 방향의 정확한 반대편이다.

정적 검증 명령:

```powershell
py -3 Tools/LoLData/Build-LoLDefinitionPack.py
py -3 Tools/LoLData/Build-LoLDefinitionPack.py --check
rg -n "bShadowActive|vShadowPosition|fShadowRemainingSec" Shared Client Tools
git diff --check -- <제드 관련 변경 파일>
```

후속 인게임 검증: W1 직후 이동 없음/체력바 없음, W2 위치 교환, 4초 이후 재입력 거부, R 대상 후방 착지, R2 위치 교환, W/R 동시 존재 시 Q/E 3원점 전개, Q 수평 회전과 검은 UV 형상을 확인한다.

미검증: 현재 사용자가 Debug 서버/클라이언트를 실행 중이므로 C++ 빌드 및 인게임 눈 검증은 이 세션에서 실행하지 않는다. 프로세스 종료 후 Debug x64 빌드를 별도 수행한다.
