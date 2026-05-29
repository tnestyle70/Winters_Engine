# Champion-Attached HP Bar UI Pipeline

- 작성일: 2026-05-12
- 기준 자산: `Client/Bin/Resource/Texture/UI/HUD/clarity_hudatlas.png`
- 구현 파일:
  - `Engine/Public/Manager/UI/UI_Manager.h`
  - `Engine/Private/Manager/UI/UI_Manager.cpp`

---

## 1. 이번 Slice 목표

- 기존 `single_bar_blue.png` / `single_bar_red.png` 기반 월드 HP bar 사용을 중단한다.
- 챔피언 머리 위에 붙는 HP bar를 `clarity_hudatlas.png`의 회색 빈 바 조각으로 교체한다.
- HP fill은 `clarity_hudatlas.png`의 green/red strip을 atlas UV로 잘라 표시한다.
- `HealthComponent::fCurrent / fMaximum`과 직접 연동한다.
- LoL처럼 최대 체력을 100 HP 단위로 나눈 바코드 tick을 그린다.

---

## 2. 현재 UI 파이프라인

```text
Scene_InGame render
  -> Client/Private/Scene/InGameRenderBridge.cpp:291
     CGameInstance::Get()->UI_Render_Overlay(vp)
  -> Engine/Private/GameInstance.cpp:227
     CGameInstance::UI_Render_Overlay
  -> Engine/Private/Manager/UI/UI_Manager.cpp:105
     CUI_Manager::Render_Overlay
  -> Engine/Private/Manager/UI/UI_Manager.cpp:112
     Draw_HealthBars(pDraw, mVP)
  -> Engine/Private/Manager/UI/UI_Manager.cpp:306
     CWorld::ForEach<HealthComponent, TransformComponent>
  -> UI_WorldToScreen
     world position + y offset -> screen position
  -> ImGui BackgroundDrawList
     empty atlas bar -> atlas green/red HP fill -> barcode -> outline
```

Debug / tuner 경로:

```text
Scene_InGame ImGui
  -> Client/Private/Scene/InGameDebugBridge.cpp:107
     CGameInstance::Get()->UI_OnImGui_Tuner()
  -> Engine/Private/GameInstance.cpp:238
     CUI_Manager::OnImGui_Tuner()
```

---

## 3. 코드 변경 요약

### 3.1 Resource

- `Engine/Private/Manager/UI/UI_Manager.cpp:32`
  - `kPathHPBarAtlas = L"Resource/Texture/UI/HUD/clarity_hudatlas.png"`
- `Engine/Public/Manager/UI/UI_Manager.h:95`
  - `m_pSRV_HPBarAtlas` 추가.
- `Engine/Private/Manager/UI/UI_Manager.cpp:50`
  - UI 초기화 시 atlas SRV 로드.
- `Engine/Private/Manager/UI/UI_Manager.cpp:67`
  - shutdown 시 atlas SRV release.

### 3.2 Team Color

- `Engine/Private/Manager/UI/UI_Manager.cpp:227`
  - `UI_Resolve_LocalTeam` 추가.
  - `LocalPlayerTag + ChampionComponent`를 찾아 로컬 플레이어 팀을 구한다.
- `Engine/Private/Manager/UI/UI_Manager.cpp:341`
  - 현재 entity의 team을 구한다.
- `Engine/Private/Manager/UI/UI_Manager.cpp:342`
  - 로컬 팀이면 ally.
- `Engine/Private/Manager/UI/UI_Manager.cpp:343`
  - ally는 atlas green strip, enemy는 atlas red strip.

### 3.3 Barcode

- `Engine/Private/Manager/UI/UI_Manager.cpp:254`
  - `UI_HealthSegmentCount(maxHP)` 추가.
  - 100 HP 단위로 tick count 계산, 가독성을 위해 최대 30개로 cap.
- `Engine/Private/Manager/UI/UI_Manager.cpp:264`
  - `UI_DrawHealthBarcode` 추가.
  - segment마다 검은 세로 line을 그림.
  - 1000 HP 단위는 더 두꺼운 major tick.

### 3.4 Draw_HealthBars

- `Engine/Private/Manager/UI/UI_Manager.cpp:306`
  - 월드 HP bar 구현 본체.
- `Engine/Private/Manager/UI/UI_Manager.cpp:323`
  - 현재 slice는 챔피언만 렌더한다.
- `Engine/Private/Manager/UI/UI_Manager.cpp:317`
  - atlas UV: `(393, 99) -> (878, 118)` in 1024x1024 atlas.
- 사용 중인 atlas rect:

```text
empty gray : (393,  99) -> (878, 116)
enemy red  : (  1, 898) -> (461, 915)
ally green : (  1, 920) -> (461, 937)
```

- 렌더 순서:

```text
black shadow
empty atlas bar
atlas green/red fill clipped by hp ratio
atlas gloss overlay
barcode ticks
black outline
```

---

## 4. 검증

```powershell
MSBuild Engine\Include\Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
MSBuild Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

결과:

- Engine build: 성공, 오류 0개.
- Client build: 성공, 오류 0개.
- 표시된 경고는 기존 DLL interface / async warning 계열이며 이번 UI 변경 신규 오류는 없다.

---

## 5. 다음 연결

- HUD 하단은 `HUD_Irelia.png`, `HUD_Irelia_2.png`, `clarity_abilityatlas.png`를 잘라 붙여 이렐리아 전용 HUD shell로 확장한다.
- 현 world HP bar는 이미 `HealthComponent`를 직접 읽으므로 서버 Snapshot이 HealthComponent를 갱신하면 자동으로 fill이 줄어든다.
- 다음 slice에서는 `ChampionComponent`의 mana/cooldown/level 값도 HUD cache로 모아 하단 HUD에 바인딩한다.
