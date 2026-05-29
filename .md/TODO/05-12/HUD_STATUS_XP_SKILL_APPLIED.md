# HUD Status Flash + XP + Skill Upgrade Applied

작성일: 2026-05-12

목표: HUD가 전투 상태를 실제 데이터로 먹도록 1차 연결했다. 피격/스턴 이미지는 `Client/Bin/Resource/Texture/UI/HUD`의 실제 PNG를 로드해서 0.5초 플래시로 표시하고, 경험치/레벨/스킬 포인트/스킬 랭크를 HUD 내부 슬롯에 반영한다.

## 적용 내용

### 1. 피격/스턴 HUD 이미지

- `lol_ingame_hit.png`, `lol_ingame_stun.png`를 `CUI_Manager`에서 WIC SRV로 로드한다.
- local champion의 HP가 직전 프레임보다 감소하면 hit flash timer를 0.5초로 켠다.
- local champion에 `StunComponent`가 새로 붙으면 stun flash timer를 0.5초로 켠다.
- ImGui 튜너에서 `Test Hit Flash`, `Test Stun Flash` 버튼으로 즉시 확인할 수 있다.

주요 코드:

- `Engine/Private/Manager/UI/UI_Manager.cpp:38` - hit/stun texture path
- `Engine/Private/Manager/UI/UI_Manager.cpp:235` - `Update_HUDStatusTimers`
- `Engine/Private/Manager/UI/UI_Manager.cpp:267` - `Draw_HUDStatusFlash`
- `Engine/Private/Manager/UI/UI_Manager.cpp:437` - HUD 렌더 중 status flash 호출

### 2. ExperienceComponent

신규 ECS 컴포넌트:

```cpp
struct ExperienceComponent
{
    f32_t current = 0.f;
    f32_t requiredForNextLevel = 280.f;
    f32_t total = 0.f;
    u8_t  level = 1;
};
```

주요 코드:

- `Engine/Public/ECS/Components/GameplayComponents.h:52`
- `EngineSDK/inc/ECS/Components/GameplayComponents.h:52`

### 3. XP 증가와 레벨업

- Shared `DamagePipeline`에서 kill 발생 시 source champion에게 XP를 지급한다.
- champion kill: 300 XP
- minion kill: 60 XP
- jungle kill: 100 XP
- XP가 요구량을 넘으면 level 증가, `ChampionComponent.level`, `StatComponent.level`, `SkillRankComponent.pointsAvailable`을 갱신한다.

주요 코드:

- `Shared/GameSim/Systems/DamagePipeline.cpp:89` - XP 요구량 공식
- `Shared/GameSim/Systems/DamagePipeline.cpp:124` - `AwardExperience`
- `Shared/GameSim/Systems/DamagePipeline.cpp:233` - kill 시 XP 지급
- `Client/Private/GamePlay/Systems/Damage.cpp:12` - legacy local ApplyDamage용 XP 지급

### 4. 챔피언 생성 시 XP/스킬 랭크 부착

- client champion spawn 시 `ExperienceComponent`, `SkillRankComponent`를 부착한다.
- 초기 스킬 포인트는 1로 둔다.
- server champion spawn도 동일하게 초기화한다.
- snapshot fallback champion 생성에도 기본 컴포넌트를 부착한다.

주요 코드:

- `Client/Private/GameObject/ChampionSpawnService.cpp:195`
- `Client/Private/Network/Client/SnapshotApplier.cpp:687`
- `Server/Private/Game/GameRoom.cpp:3631`
- `Server/Private/Game/GameRoom.cpp:3773`

### 5. HUD XP/스킬 업그레이드 표시

- HUD XP bar가 `ExperienceComponent.current / requiredForNextLevel`을 읽는다.
- XP 숫자도 HUD 내부에 표시한다.
- Q/W/E/R 슬롯 아래에 현재 스킬 랭크 dot을 표시한다.
- 스킬 포인트가 있고 레벨 조건을 만족하면 슬롯 위에 `+` 버튼이 뜬다.
- `+` 클릭 시 local `SkillRankComponent`가 즉시 증가하고 `pointsAvailable`이 감소한다.

주요 코드:

- `Engine/Private/Manager/UI/UI_Manager.cpp:362` - XP read
- `Engine/Private/Manager/UI/UI_Manager.cpp:583` - 스킬 레벨업 가능 조건
- `Engine/Private/Manager/UI/UI_Manager.cpp:606` - local skill rank 증가

### 6. 서버 LevelSkill command 정리

- 기존 `HandleLevelSkill`은 points 차감 없이 rank만 올렸다.
- 이제 `pointsAvailable`, max rank, 레벨 조건을 검증하고 성공 시 포인트를 1 차감한다.

주요 코드:

- `Shared/GameSim/Systems/CommandExecutor.cpp:1264`

### 7. 스킬 데미지 랭크 연결

- local/offline cast frame hook에서 `gameCtx.skillRank = 1` 고정이던 부분을 `SkillRankComponent`에서 읽도록 변경했다.
- 이제 HUD에서 스킬을 올리면 local skill hook context에도 rank가 들어간다.

주요 코드:

- `Client/Private/Scene/Scene_InGame.cpp:1260`

## 검증

- Engine Debug x64 빌드 통과.
- Client Debug x64 빌드 통과.
- Server Debug x64 빌드 통과.
- 남은 경고는 기존 DLL interface / vcpkg applocal fallback 계열이며 오류는 0개.

## 다음 연결

- snapshot schema에 XP/rank/skillPoints 필드 추가해서 서버 권위 상태를 클라이언트 HUD에 정확히 복제한다.
- HUD `+` 직접 local mutation은 offline 1차용이다. 네트워크 모드에서는 `GameCommand::LevelSkill` 전송으로 전환한다.
- item shop의 구매 성공도 `ExperienceComponent`/`SkillRankComponent`처럼 HUD slot에 직접 반영하는 같은 패턴으로 붙인다.
