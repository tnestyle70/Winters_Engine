# Irelia HUD Status Window Plan

- 작성일: 2026-05-12
- 목적: `HUD_Irelia.png` 캡쳐본을 기준선으로 삼아, 실제 렌더는 보유 atlas/PNG를 조립해서 LoL식 이렐리아 하단 HUD 상태창을 만든다.
- 현재 구현 진입점: `Engine/Private/Manager/UI/UI_Manager.cpp::Draw_ChampionHUD`
- 기준 레퍼런스:
  - `Client/Bin/Resource/Texture/UI/HUD_Irelia.png` - 850x146
  - `Client/Bin/Resource/Texture/UI/HUD_Irelia_2.png` - 861x167

---

## 1. 목표 화면

이렐리아 HUD 상태창은 아래 영역을 가진다.

```text
[초상화 + 레벨] [HP/MP/XP] [패시브] [Q] [W] [E] [R] [D] [F] [아이템 1~6 + 장신구]
                [AD/AP/방어/마저/공속/이속 등 간단 stat line]
```

첫 버전은 `HUD_Irelia.png`와 최대한 비슷한 실루엣을 만드는 것이 목표다.

- 캡쳐 PNG는 화면에 그대로 쓰지 않는다.
- 캡쳐 PNG는 좌표/비율/구성 참고용으로만 쓴다.
- 실제 렌더는 atlas crop 또는 잘라낸 개별 PNG를 사용한다.

---

## 2. 사용할 자산

### 2.1 레퍼런스

```text
Resource/Texture/UI/HUD_Irelia.png       850x146
Resource/Texture/UI/HUD_Irelia_2.png     861x167
```

용도:

- 전체 HUD 폭/높이 기준.
- portrait, bar, ability, item slot 배치 비율 기준.
- atlas 조립 결과와 비교하는 QA 기준 이미지.

### 2.2 Atlas

```text
Resource/Texture/UI/HUD/clarity_hudatlas.png        1024x1024
Resource/Texture/UI/HUD/clarity_abilityatlas.png    1024x1024
Resource/Texture/UI/HUD/clarity_levelupatlas.png    1024x1024
Resource/Texture/UI/HUD/clarity_resourceatlas.png    512x128
Resource/Texture/UI/HUD/statspanel_atlas.png         512x512
Resource/Texture/UI/HUD/hudresetatlas.png           1024 계열
```

용도:

- `clarity_hudatlas`: HUD 프레임, panel, bar strip, item slot frame.
- `clarity_abilityatlas`: Q/W/E/R 스킬 슬롯 프레임, 쿨다운/활성 상태 프레임.
- `clarity_levelupatlas`: 스킬 레벨업 화살표/강조 효과.
- `clarity_resourceatlas`: 얇은 resource bar 후보.
- `statspanel_atlas`: stat 상태창 icon/frame 후보.

### 2.3 개별 PNG

```text
Resource/Texture/UI/Items/*.png
```

용도:

- 아이템 슬롯 icon.
- 1차 구현은 sample item icon으로 채우고, 후속에 InventoryComponent와 연결한다.

챔피언별 portrait/skill icon은 아직 완전하지 않으므로 별도 registry가 필요하다.

---

## 3. 구현 방향 2가지

## A안: Atlas UV 조립 방식

### 장점

- 실제 게임 UI 방식과 가장 유사하다.
- draw call/texture 전환을 줄이기 쉽다.
- 원본 atlas를 유지하므로 자산 관리가 깔끔하다.
- 나중에 JSON rect table로 빼기 좋다.

### 단점

- 초반에 atlas rect 좌표 잡는 시간이 든다.
- 눈으로 맞추는 튜닝 UI가 없으면 반복이 피곤하다.

### A안 렌더 구조

```text
Load SRV:
  clarity_hudatlas
  clarity_abilityatlas
  clarity_levelupatlas
  clarity_resourceatlas
  statspanel_atlas
  item icons
  champion portrait / skill icons

Draw_ChampionHUD:
  1. bottom-center anchor
  2. HUD background frame crop
  3. portrait frame crop
  4. portrait texture
  5. HP / MP / XP bars
  6. passive slot
  7. Q/W/E/R slots
  8. level-up arrows
  9. cooldown overlays
  10. item slots
  11. stat row
```

## B안: 미리 잘라낸 PNG 조립 방식

### 장점

- 구현이 빠르다.
- atlas 좌표 튜닝 스트레스가 작다.
- UI 결과물을 눈으로 관리하기 쉽다.

### 단점

- 파일 수가 늘어난다.
- 같은 atlas 조각을 여러 PNG로 복제하게 된다.
- 나중에 스타일 변경 시 다시 잘라야 한다.

### B안 자산 구조

```text
Client/Bin/Resource/Texture/UI/HUD/Slices/Irelia/
  hud_panel_left.png
  hud_panel_center.png
  hud_panel_right.png
  portrait_frame.png
  ability_slot.png
  item_slot.png
  hp_bar_empty.png
  hp_bar_fill_green.png
  mp_bar_fill_blue.png
  xp_bar_fill.png
  levelup_arrow.png
```

### B안 렌더 구조

```text
Draw image slices by fixed layout.
HP/MP/XP는 이미지 fill을 dest width crop으로 줄여 표시.
QWER cooldown은 검은 rect overlay + 숫자.
```

---

## 4. 추천: Hybrid 방식

1차는 A안으로 간다.

- 이유: 이미 `clarity_hudatlas.png` HP bar 구현에서 atlas UV crop이 동작한다.
- `AddImage(texture, dstMin, dstMax, uv0, uv1)` 경로가 검증됐다.

단, 좌표 맞추기가 너무 오래 걸리면 B안으로 바로 전환한다.

- atlas rect를 직접 잘라 `HUD/Slices/Common/*.png`로 export.
- 코드는 그대로 `DrawImageSlice` 방식으로 유지.
- 후속에 다시 atlas rect table로 되돌릴 수 있게 slice 이름과 rect id를 동일하게 둔다.

```text
rect id: ability_slot
slice : HUD/Slices/Common/ability_slot.png
```

---

## 5. 상태창 레이아웃

기준 크기:

```text
reference: HUD_Irelia_2.png 861x167
screen placement: bottom center
default draw size: 861x167 또는 화면 폭 기준 0.45~0.50 scale
```

구성:

```text
root
  panel background
  portrait cluster
    portrait frame
    champion portrait
    level circle/text
  resource cluster
    hp empty
    hp fill
    hp text
    mp empty
    mp fill
    mp text
    xp strip
  ability cluster
    passive
    Q
    W
    E
    R
    D
    F
  inventory cluster
    item 1
    item 2
    item 3
    item 4
    item 5
    item 6
    trinket
  stat cluster
    attack
    ability power
    armor
    magic resist
    attack speed
    move speed
```

---

## 6. JSON layout 초안

나중에 좌표를 JSON으로 뺄 때의 목표 형태:

```json
{
  "name": "lol_bottom_hud_irelia_reference",
  "reference": {
    "image": "Resource/Texture/UI/HUD_Irelia_2.png",
    "size": [861, 167]
  },
  "textures": {
    "hud": "Resource/Texture/UI/HUD/clarity_hudatlas.png",
    "ability": "Resource/Texture/UI/HUD/clarity_abilityatlas.png",
    "levelup": "Resource/Texture/UI/HUD/clarity_levelupatlas.png",
    "resource": "Resource/Texture/UI/HUD/clarity_resourceatlas.png",
    "stats": "Resource/Texture/UI/HUD/statspanel_atlas.png"
  },
  "rects": {
    "hp_empty": [393, 99, 878, 118],
    "hp_green": [1, 920, 461, 937],
    "hp_red": [1, 898, 461, 915],
    "mp_blue": [1, 942, 461, 959],
    "xp_gold": [1, 876, 461, 893],
    "ability_slot": [0, 0, 64, 64],
    "levelup_arrow": [0, 0, 48, 48],
    "item_slot": [0, 0, 48, 48]
  },
  "layout": {
    "root": [0, 0, 861, 167],
    "portrait": [18, 33, 86, 101],
    "level": [72, 96, 24, 24],
    "hp": [113, 32, 235, 15],
    "mp": [113, 51, 235, 13],
    "xp": [113, 68, 235, 5],
    "passive": [360, 74, 36, 36],
    "q": [405, 62, 48, 48],
    "w": [458, 62, 48, 48],
    "e": [511, 62, 48, 48],
    "r": [564, 62, 48, 48],
    "d": [623, 64, 42, 42],
    "f": [670, 64, 42, 42],
    "items": [
      [724, 45, 34, 34],
      [762, 45, 34, 34],
      [800, 45, 34, 34],
      [724, 84, 34, 34],
      [762, 84, 34, 34],
      [800, 84, 34, 34],
      [838, 84, 34, 34]
    ]
  }
}
```

주의: 위 좌표는 구현 시작용 초안이다. 실제 atlas rect는 ImGui tuner에서 화면을 보며 조정한다.

---

## 7. 코드 구조 계획

### 7.1 UI_Manager 1차 확장

```cpp
void Draw_ChampionHUD(ImDrawList* pDraw);
void Draw_HudPanel(ImDrawList* pDraw, const ImVec2& root);
void Draw_HudResources(ImDrawList* pDraw, const HudRuntimeState& state);
void Draw_HudAbilities(ImDrawList* pDraw, const HudRuntimeState& state);
void Draw_HudItems(ImDrawList* pDraw, const HudRuntimeState& state);
void Draw_HudStats(ImDrawList* pDraw, const HudRuntimeState& state);
```

### 7.2 Runtime state

```cpp
struct HudRuntimeState
{
    EntityID localEntity;
    eChampion champion;
    f32_t hp;
    f32_t maxHp;
    f32_t mp;
    f32_t maxMp;
    f32_t xpRatio;
    u8_t level;
    f32_t cooldowns[4];
    f32_t maxCooldowns[4];
    u8_t skillRanks[4];
    u8_t skillPoints;
    u32_t itemIds[7];
};
```

1차는 가능한 값만 채운다.

- HP/MP/level/cooldown: `ChampionComponent`
- skill rank/skillPoints: 이후 `SkillRankComponent` 또는 Snapshot 확장 후 연결
- itemIds: debug sample
- xpRatio: Snapshot 확장 전까지 0 또는 debug value

### 7.3 Asset registry

```cpp
struct ChampionHudAssets
{
    eChampion champion;
    const wchar_t* portrait;
    const wchar_t* passive;
    const wchar_t* skill[4];
};
```

첫 등록:

```text
IRELIA
  portrait: placeholder 또는 Irelia square/png 추가 예정
  Q/W/E/R : placeholder 또는 Irelia skill icon png 추가 예정
```

없는 경우:

```text
portrait fallback: HUD_Irelia reference crop 또는 champion loadscreen crop
skill fallback   : clarity_abilityatlas slot icon placeholder
```

---

## 8. 구현 순서

### Step 1. Reference overlay tuner

- `OnImGui_Tuner`에 `Show Irelia HUD Reference` 토글 추가.
- `HUD_Irelia_2.png`를 반투명으로 하단에 깔아 atlas 조립 결과와 비교한다.
- 개발 중에만 사용하고 최종에는 끈다.

### Step 2. Atlas SRV 로드

- `m_pSRV_ChampionHUD` 통짜 HUD 대신 아래 SRV 추가:
  - `m_pSRV_HudAtlas`
  - `m_pSRV_AbilityAtlas`
  - `m_pSRV_LevelUpAtlas`
  - `m_pSRV_ResourceAtlas`
  - `m_pSRV_StatPanelAtlas`

### Step 3. DrawAtlasRect helper

```cpp
DrawAtlasRect(pDraw, texture, atlasSize, srcPxRect, dstPxRect, tint);
DrawAtlasBar(pDraw, texture, atlasSize, srcPxRect, dstPxRect, ratio, tint);
```

### Step 4. Panel shell

- 하단 중앙 root 계산.
- panel/background부터 atlas 조각으로 만든다.
- 처음에는 stretch 허용.
- 2차에서 9-slice로 개선한다.

### Step 5. Resource bars

- HP: atlas green/red fill 사용.
- MP: atlas blue/cyan fill 사용.
- XP: thin gold/cyan strip 사용.
- 숫자는 ImGui text로 임시 표시.

### Step 6. Ability slots

- Q/W/E/R slot frame.
- skill icon.
- cooldown overlay.
- rank dot/text.
- level-up arrow.

### Step 7. Items and stats

- item slot frame 7칸.
- sample item icon path로 표시.
- stat line은 text 우선, 아이콘은 후속.

### Step 8. Champion swap

- `m_PlayerChampion` 또는 LocalPlayer `ChampionComponent.id` 기준으로 asset set 선택.
- Irelia 이후 Yasuo/Kalista 등도 같은 layout 재사용.

---

## 9. 복잡할 때 PNG slice 전환 기준

다음 중 2개 이상이면 slice PNG 방식으로 전환한다.

- atlas rect 좌표 조정에 1시간 이상 걸린다.
- 패널 frame이 stretch 시 너무 깨진다.
- atlas 안의 조각이 불완전해서 HUD_Irelia 레퍼런스와 큰 차이가 난다.
- 스킬/아이템 slot frame이 실제로는 여러 조각을 겹쳐야 해서 코드가 지저분해진다.

전환 시에도 구조는 유지한다.

```text
DrawAtlasRect -> DrawSliceImage
rect id       -> slice id
layout JSON   -> 그대로 유지
```

---

## 10. 검증 기준

- `HUD_Irelia.png` 통짜 렌더를 꺼도 HUD가 나온다.
- reference overlay를 켰을 때 주요 slot 위치가 5px 이내로 맞는다.
- HP/MP bar가 ChampionComponent 값과 같이 변한다.
- Q/W/E/R cooldown이 줄어든다.
- item slot에 `UI/Items/*.png` 아이콘이 들어간다.
- `m_PlayerChampion`을 바꾸면 portrait/skill icon asset set이 바뀐다.

---

## 11. 2026-05-12 적용 로그

- `CUI_Manager::Draw_ChampionHUD`에서 `HUD_Irelia.png` 통짜 렌더를 제거했다.
- 하단 중앙에 `clarity_hudatlas.png` + `clarity_abilityatlas.png` 기반 atlas 조립 HUD shell을 표시한다.
- 현재 조립 요소:
  - panel background
  - portrait placeholder
  - level circle
  - HP / MP / XP bars
  - passive / Q / W / E / R / D / F slots
  - item slot placeholders
  - stat strip placeholder
- HP/MP/level/cooldown 값은 `LocalPlayerTag + ChampionComponent`를 우선 읽고, HP는 `HealthComponent`가 있으면 그 값을 우선 사용한다.
- Engine build 검증:

```powershell
MSBuild Engine\Include\Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

결과: 성공, 오류 0개.

- Client build는 현재 별도 dirty 작업의 오류로 실패했다.
  - `Client/Private/Scene/InGameCombatInputBridge.cpp(302)`: private member access.
  - `Client/Private/Scene/Scene_InGame.cpp(294)`: 기존 min/max macro 충돌/brace 오류.
  - 이번 HUD atlas 조립 변경에서 발생한 Engine 컴파일 오류는 없다.
