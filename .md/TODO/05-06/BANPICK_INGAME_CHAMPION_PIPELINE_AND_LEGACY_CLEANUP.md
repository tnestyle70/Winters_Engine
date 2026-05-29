# BanPick / InGame 기준 챔피언 추가 파이프라인과 Legacy 정리

작성일: 2026-05-06
최종 갱신: 2026-05-07 - Phase 7D/7E 완료 + S1 hidden BanPick/InGame local binding/skill/full-map telemetry 통과 기준 stale 정리

## 1. 현재 목표 흐름

```txt
BanPick
  - 10개 슬롯
  - Human 또는 Bot 자리 선택
  - 각 슬롯 championId 선택
  - GameContext.Roster에 확정

MatchLoading
  - Roster 읽기
  - champion / team / seat 상태 표시

InGame
  - GameContext.Roster 읽기
  - ChampionSpawnService로 ECS champion 생성
  - Player local entity 바인딩
  - Champion module self-register hook 실행
```

현재 자동 smoke 진입로:

```txt
Server/Bin/Debug/WintersServer.exe --smoke-seconds=130
Client/Bin/Debug/WintersGame.exe --banpick-smoke --smoke-slot=0 --smoke-champion=YONE --smoke-start --smoke-start-min-humans=3
Client/Bin/Debug/WintersGame.exe --banpick-smoke --smoke-slot=1 --smoke-champion=FIORA
Client/Bin/Debug/WintersGame.exe --banpick-smoke --smoke-slot=2 --smoke-champion=EZREAL
```

주의:

```txt
작업 디렉터리는 SolutionDir(C:\Users\user\Desktop\Winters) 기준.
--banpick-smoke 기본값은 빠른 검증을 위해 local-only roster, map init skip, FX mesh preload skip.
전체 커버리지는 --smoke-human-roster / --smoke-full-roster / --smoke-full-map / --smoke-full-ingame 옵션으로 확장.
스킬 자동 smoke를 끄려면 --smoke-no-skill.
```

2026-05-07 확인:

```txt
JoinSlot/PickChampion/StartGame 통과
SpawnLobby slot=0 champ=YONE, slot=1 champ=FIORA, slot=2 champ=EZREAL
Snapshot broadcast to 3 sids
45초 후 server/client 3 프로세스 생존
Client smoke logs:
  slot=0 YONE(13), sid=1, net=1 local entity bind
  slot=1 FIORA(8), sid=2, net=2 local entity bind
  slot=2 EZREAL(12), sid=3, net=3 local entity bind
  Hello bindNet/bindSid matched, snapshot len=984 received
  Q Dispatch/castFrame:
    YONE  hook=0x000D0032 gameplay=1 visual=1 legacy=0
    FIORA hook=0x00080032 gameplay=1 visual=1 legacy=0
    EZREAL hook=0x000C0032 gameplay=1 visual=1 legacy=0

Full-map smoke:
  Client/Bin/Debug/WintersGame.exe --banpick-smoke --smoke-slot=0 --smoke-champion=YONE --smoke-start --smoke-start-min-humans=1 --smoke-full-map --smoke-no-skill
  map init done ok=1, InGameMap entity created, bootstrap done, snapshot received

Direct local fallback telemetry:
  `[ECS:RosterFallback]` logs selected player slot + Sylas bot slot.
  Per-slot `[ECS:Roster] created` logs champion/team/human/bot/net/entity/pos.
```

핵심 원칙:

```txt
BanPick 선택값이 유일한 identity source다.
InGameScene은 championId를 재해석하거나 index로 추론하지 않는다.
```

---

## 2. 챔피언 추가 파이프라인

새 챔피언 `Xxx`를 추가할 때 필요한 최소 파일:

```txt
Client/Public/GameObject/Champion/Xxx/Xxx_Skills.h
Client/Private/GameObject/Champion/Xxx/Xxx_Skills.cpp
Client/Private/GameObject/Champion/Xxx/Xxx_Registration.cpp
Client/Public/GameObject/Champion/Xxx/Xxx_Tuning.h        optional but 권장
Client/Private/GameObject/Champion/Xxx/Xxx_Tuning.cpp     optional but 권장
Client/Private/GameObject/Champion/Xxx/XxxFxPresets.cpp   FX 필요 시
Client/Public/GameObject/Champion/Xxx/XxxFxPresets.h      FX 필요 시
```

등록 파일이 소유해야 하는 것:

```txt
ChampionDef
SkillDef 5개
GameplayHook registration
VisualHook registration
SkillHook registration only when Scene callback 또는 client-local action 필요
KeepAlive function
```

예시:

```cpp
constexpr u32_t kXxx_Q_Cast = MakeHookId(eChampion::XXX, HookVariant::Q_CastFrame);

SkillDef s{};
s.champ = eChampion::XXX;
s.slot = static_cast<uint8_t>(eSkillSlot::Q);
s.targetMode = eTargetMode::Direction;
s.animKey = "spell1";
s.castFrameHookId = kXxx_Q_Cast;
CSkillRegistry::Instance().Add(eChampion::XXX, s.slot, s);

CGameplayHookRegistry::Instance().Register(kXxx_Q_Cast, &Xxx::Gameplay::OnCastFrame_Q);
CVisualHookRegistry::Instance().Register(kXxx_Q_Cast, &Xxx::Visual::OnCastFrame_Q);
```

Scene 진입 시:

```cpp
extern void Xxx_KeepAlive();
Xxx_KeepAlive();
```

KeepAlive 호출 목록은 `ChampionModuleBootstrap`으로 단일화했다.

---

## 3. BanPick에 챔피언을 보이게 하는 조건

```txt
1. eChampion enum에 ID 존재
2. ChampionCatalog entry 존재
3. ChampionDef 또는 Registry entry 존재
4. asset path / texture path 유효
5. BanPick selectable list가 catalog를 읽음
```

금지:

```txt
button index -> champion enum 직접 매핑
slot index -> champion enum 추론
host champion -> all clients champion 복사
```

---

## 4. InGame 스폰 조건

```txt
1. GameContext.Roster에 slot/team/champion 확정
2. InGameRosterSpawner가 roster 순회
3. ChampionSpawnService::SpawnChampion 호출
4. ECS component 추가
5. Renderer map에 ModelRenderer 생성
6. local player entity만 input/camera/player renderer에 바인딩
```

버그 방지 규칙:

```txt
local player champion을 전체 roster champion으로 사용하지 않는다.
host selected champion을 bot/client champion에 전파하지 않는다.
ChampionSpawnService는 id를 직접 받는다.
```

---

## 5. Legacy 정리 순서

```txt
L1. Scene_InGame champion-specific keySwap branch 제거
L2. Scene_InGame champion-specific castFrame fallback 제거
L3. Scene_InGame champion-specific onAccepted fallback 제거
L4. Scene_InGame champion-specific dash/R 상태를 callback 또는 component로 분리
L5. ChampionTuner 값은 Xxx_Tuning으로 이동
L6. SkillTable fallback은 legacy only로 축소
L7. 모든 active champion은 Xxx_Registration.cpp self-register 사용
```

현재 완료:

```txt
Yone/Fiora/Ezreal: gameplay/visual hook 분리, legacy duplicate dispatch 제거
Kalista: BA/Q/E + passive recoveryHook 이관, Kalista_Tuning 단일화
Yasuo: tuning 단일화, Q keySwapHook, Q/W/E/R accepted hook 이관 완료
Irelia: tuning 단일화, Q/W/E/R accepted hook, E blade local state 이관 완료
Garen/Zed/Riven: castFrame fallback hook 이관 완료
ChampionModuleBootstrap: KeepAlive 목록 단일화 완료
Bridge cleanup: SkillDispatch/CombatInput bridge no longer include champion skill headers; local dash/damage/passive reset are Scene APIs; Riven Q key swap moved to VisualHook
Scene runtime cleanup: Yasuo/Kalista/Irelia local runtime update and local damage/passive dash plumbing moved behind InGameChampionStateBridge
```

남은 큰 덩어리:

```txt
Scene_InGame bridge 추출(Render / PlayerControl / CombatInput / Debug) [완료]
SkillTable fallback을 legacy-only로 축소
Sylas/Viego는 asset/anim 가용성 확인 후 hook self-register 대상 결정
```

---

## 6. 다음 챔피언 추가 체크리스트

```txt
[ ] eChampion enum 추가
[ ] ChampionCatalog entry 추가
[ ] ChampionDef 경로/스케일/텍스처 확인
[ ] Xxx_Registration.cpp 작성
[ ] SkillDef 5개 self-register
[ ] GameplayHook / VisualHook 분리
[ ] 필요 시 Xxx_Tuning 작성
[x] vcxproj / filters 등재
[x] BanPick에서 선택 가능 확인 (hidden smoke: Yone/Fiora/Ezreal)
[ ] MatchLoading에서 표시 확인
[ ] InGame에서 local/bot/network roster 별로 다른 champion spawn 확인 (hidden local binding + skill hook + full-map telemetry 통과, direct-local selected+Sylas telemetry 추가, bot/full roster visual은 user-owned)
[x] Client Debug 빌드
[x] Server 1 + Client 3 hidden smoke
```
