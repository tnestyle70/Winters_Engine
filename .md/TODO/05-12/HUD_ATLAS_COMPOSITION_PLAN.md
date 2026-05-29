# HUD Atlas Composition Plan

- 작성일: 2026-05-12
- 목표: `HUD_Irelia.png` 캡쳐본 의존을 제거하고, 현재 보유 atlas PNG들을 잘라 조립해 LoL식 하단 HUD를 만든다.
- 기준 자산 루트: `Client/Bin/Resource/Texture/UI`
- 현재 구현 진입점:
  - `Engine/Private/Manager/UI/UI_Manager.cpp`
  - `CUI_Manager::Draw_ChampionHUD`
  - `CUI_Manager::Render_Overlay`

---

## 1. 최종 목표

이렐리아 HUD는 지금처럼 캡쳐 PNG 1장을 통째로 붙이는 방식이 아니라, 아래 부품을 atlas에서 잘라 조립한다.

```text
HUD frame / panel / slot frame     -> clarity_hudatlas.png
ability slot frame / cooldown mask -> clarity_abilityatlas.png
level-up arrow / glow              -> clarity_levelupatlas.png
HP / MP / XP resource strips       -> clarity_hudatlas.png 또는 clarity_resourceatlas.png
stat panel pieces                  -> statspanel_atlas.png
item icons                         -> UI/Items/*.png
champion portrait                  -> champion별 portrait/square/circle asset
skill icons                        -> champion별 skill icon asset
```

UI 렌더 방식은 “이미지를 잘라서 새 PNG로 저장”이 아니라 `SRV + UV rect + quad`이다.

```text
texture atlas 1장
  -> JSON/code rect로 필요한 영역만 UV crop
  -> 화면 좌표에 AddImage
  -> cooldown/HP/MP/XP는 dest width 또는 clip rect로 동적 표시
```

---

## 2. 지금 있는 자산으로 가능한 것

### 2.1 공용 HUD atlas

- `HUD/clarity_hudatlas.png`
  - 하단 HUD 큰 프레임, HP/MP/XP strip, 원형/사각 슬롯, 아이콘 프레임, 버튼류.
- `HUD/clarity_abilityatlas.png`
  - 스킬 슬롯, spell key slot, cooldown/active frame 후보.
- `HUD/clarity_levelupatlas.png`
  - 스킬 레벨업 화살표/강조 UI 후보.
- `HUD/clarity_resourceatlas.png`
  - 얇은 resource bar 후보.
- `HUD/statspanel_atlas.png`
  - 공격력/방어력/이속 등 stat panel 후보.
- `HUD/hudresetatlas.png`
  - reset/버튼 프레임 후보.
- `HUD/animatedmeters.png`
  - 고급 meter 애니메이션 후보. 1차는 보류.

### 2.2 아이템

- `UI/Items/*.png`
  - 이미 아이템 아이콘 PNG가 대량으로 있다.
  - 1차는 inventory slot 6칸에 고정 sample item icon을 넣고, 이후 InventoryComponent와 연결한다.

### 2.3 챔피언 초상화 / 스킬 아이콘

- 일부 챔피언은 `Client/Bin/Resource/Texture/MAP/assets/characters/.../hud/...` 계열에 skill icon 후보가 있다.
- 일부는 `Character/<Champion>/<champion>loadscreen.png`만 있고 HUD portrait가 없을 수 있다.
- 그래서 1차 정책:
  - portrait가 있으면 portrait/square/circle asset 사용.
  - 없으면 loadscreen을 임시 crop하거나 placeholder portrait를 사용.
  - skill icon이 없으면 챔피언 FX/스킬 이름 기반 placeholder icon 사용.

---

## 3. 설계 원칙

1. `HUD_Irelia.png`는 레퍼런스 스크린샷으로만 사용한다.
   - 최종 렌더에는 atlas 조각들을 조립한다.

2. UI layout은 코드에 박지 않고 JSON으로 빼는 방향으로 간다.
   - 1차 구현은 code table로 빠르게 맞춘다.
   - 좌표가 안정되면 `Client/Bin/Data/UI/lol_hud_layout.json`로 분리한다.

3. 챔피언별로 바뀌는 것은 `ChampionHudAssetSet`으로 분리한다.
   - 초상화
   - Q/W/E/R icon
   - passive icon
   - resource type
   - accent color

4. 아이템은 HUD layout과 별도이다.
   - item slot frame은 HUD atlas.
   - icon은 `UI/Items/<itemId>_*.png`.
   - item id -> texture path registry를 둔다.

5. 모든 동적 수치는 서버/Snapshot 값을 따른다.
   - HP/MP/XP
   - level
   - skill rank
   - cooldown
   - item slot

---

## 4. 데이터 구조 계획

### 4.1 공용 atlas rect

1차 code table 예시:

```cpp
struct UIAtlasRect
{
    const wchar_t* texturePath;
    f32_t x0;
    f32_t y0;
    f32_t x1;
    f32_t y1;
};
```

후속 JSON:

```json
{
  "textures": {
    "hud": "Resource/Texture/UI/HUD/clarity_hudatlas.png",
    "ability": "Resource/Texture/UI/HUD/clarity_abilityatlas.png",
    "levelup": "Resource/Texture/UI/HUD/clarity_levelupatlas.png",
    "resource": "Resource/Texture/UI/HUD/clarity_resourceatlas.png"
  },
  "rects": {
    "bottom_panel": [263, 441, 668, 584],
    "portrait_frame": [5, 604, 202, 635],
    "ability_slot": [486, 200, 533, 247],
    "level_up_arrow": [486, 200, 533, 247],
    "hp_empty": [393, 99, 878, 118],
    "hp_green": [1, 920, 461, 937],
    "hp_red": [1, 898, 461, 915]
  }
}
```

### 4.2 HUD layout

```cpp
struct HUDSlotLayout
{
    Vec2 pos;
    Vec2 size;
    const char* frameRectId;
};

struct ChampionHUDLayout
{
    Vec2 anchorBottomCenter;
    Vec2 panelSize;
    HUDSlotLayout portrait;
    HUDSlotLayout passive;
    HUDSlotLayout skills[4];
    HUDSlotLayout summoners[2];
    HUDSlotLayout items[7];
    HUDSlotLayout hpBar;
    HUDSlotLayout mpBar;
    HUDSlotLayout xpBar;
};
```

### 4.3 챔피언별 HUD asset

```cpp
struct ChampionHudAssetSet
{
    eChampion champion;
    const wchar_t* portraitPath;
    const wchar_t* passiveIconPath;
    const wchar_t* skillIconPath[4];
    const char* resourceKind;  // mana, energy, none, rage
};
```

후속 JSON:

```json
{
  "IRELIA": {
    "portrait": "Resource/Texture/UI/Champions/Irelia/irelia_square.png",
    "passive": "Resource/Texture/UI/Champions/Irelia/irelia_passive.png",
    "skills": [
      "Resource/Texture/UI/Champions/Irelia/irelia_q.png",
      "Resource/Texture/UI/Champions/Irelia/irelia_w.png",
      "Resource/Texture/UI/Champions/Irelia/irelia_e.png",
      "Resource/Texture/UI/Champions/Irelia/irelia_r.png"
    ],
    "resource": "mana"
  }
}
```

---

## 5. 렌더 순서

`CUI_Manager::Draw_ChampionHUD` 내부 순서:

```text
1. HUD root 계산
   - 화면 하단 중앙 anchor
   - 16:9 기준 scale

2. 큰 panel / 장식 frame
   - clarity_hudatlas.png 조각 여러 개
   - 1차는 stretch, 후속은 9-slice

3. portrait frame
   - atlas frame
   - champion portrait texture
   - level number overlay

4. resource bars
   - HP atlas empty + HP fill
   - MP atlas empty + MP fill
   - XP thin bar

5. passive / QWER / summoner slots
   - slot frame atlas
   - champion skill icon
   - rank dot/text
   - cooldown radial or dark overlay
   - level-up arrow if skillPoints > 0

6. item slots
   - item slot frame atlas
   - item icon texture
   - cooldown/stack count later

7. foreground text
   - HP/MP numbers
   - cooldown seconds
   - key labels Q/W/E/R/D/F
```

---

## 6. 구현 단계

### Slice HUD-1: Irelia screenshot replacement shell

- `CUI_Manager::Draw_ChampionHUD`에서 `HUD_Irelia.png` 통짜 렌더를 제거한다.
- `clarity_hudatlas.png` + `clarity_abilityatlas.png` 조각으로 하단 HUD shell을 만든다.
- portrait/skill/item은 임시 placeholder rect 또는 sample texture로 둔다.
- 목표: 캡쳐 PNG 없이도 이렐리아 HUD 형태가 나온다.

### Slice HUD-2: Atlas rect table

- `UIHudAtlasRect.h/.cpp` 또는 `UI_Manager.cpp` 내부 table 추가.
- rect id로 draw 가능하게 한다.
- `DrawAtlasRect(textureId, rectId, dstRect, tint)` 헬퍼 추가.
- 이후 JSON 전환이 쉽게 되도록 `rectId` 기반으로만 호출한다.

### Slice HUD-3: ChampionHudAssetRegistry

- 챔피언별 portrait/Q/W/E/R/passive path를 등록한다.
- `m_PlayerChampion` 또는 `GameContext.SelectedChampion`에 따라 asset set을 고른다.
- 없는 asset은 placeholder로 fallback한다.

### Slice HUD-4: HUD data binding

- `ChampionComponent`에서 HP/MP/level/cooldowns를 읽는다.
- `SkillRankComponent`가 client world에 있으면 rank도 읽는다.
- 이후 Snapshot 확장 후 XP/skillPoints를 읽는다.
- 하단 HUD는 `m_PlayerChampion`만 보지 말고 `LocalPlayerTag` 엔티티를 찾아 값을 표시한다.

### Slice HUD-5: Item slot binding

- 임시 `HudInventoryDebugState`로 item id 6개를 표시한다.
- 이후 `InventoryComponent` 또는 backend inventory와 연결한다.
- `UI/Items/*.png`에서 item id prefix로 texture를 찾는다.

### Slice HUD-6: JSON layout

- `Client/Bin/Data/UI/lol_hud_layout.json`
- `Client/Bin/Data/UI/champion_hud_assets.json`
- `Client/Bin/Data/UI/item_icon_manifest.json`
- code table을 JSON loader로 교체한다.

---

## 7. 파일 터치 계획

### Engine

- `Engine/Public/Manager/UI/UI_Manager.h`
  - HUD atlas SRV 추가.
  - champion HUD 상태/cache 추가.
  - `Draw_PlayerHUD`, `Draw_AbilityBar`, `Draw_ItemSlots` 분리 후보.

- `Engine/Private/Manager/UI/UI_Manager.cpp`
  - `Draw_ChampionHUD` 통짜 PNG 출력 제거.
  - atlas 조각 기반 `DrawAtlasRect` 도입.
  - cooldown/level-up/item overlay 추가.

### Client

- `Client/Public/UI/ChampionHudAssets.h`
- `Client/Private/UI/ChampionHudAssets.cpp`
  - 챔피언별 portrait/skill icon registry.

- `Client/Public/UI/HudLayout.h`
- `Client/Private/UI/HudLayout.cpp`
  - JSON 전환 전 code table.

### Data

- `Client/Bin/Data/UI/lol_hud_layout.json`
- `Client/Bin/Data/UI/champion_hud_assets.json`
- `Client/Bin/Data/UI/item_icon_manifest.json`

---

## 8. 검증 기준

- `HUD_Irelia.png`를 사용하지 않아도 하단 HUD가 나온다.
- 선택 챔피언을 Irelia/Yasuo/Kalista 등으로 바꾸면 portrait와 스킬 아이콘 registry가 바뀐다.
- HP/MP 수치가 `ChampionComponent` 값과 같이 움직인다.
- 스킬 cooldown이 slot 위에 어둡게 덮이고 숫자로 표시된다.
- 스킬 레벨업 가능 시 level-up atlas 화살표가 Q/W/E/R 위에 뜬다.
- 아이템 slot은 비어 있으면 frame만, item id가 있으면 `UI/Items` icon을 표시한다.

---

## 9. 주의점

- atlas를 잘라 별도 PNG로 복사하지 않는다.
- 좌표는 처음에는 code table로 빠르게 맞추고, 안정되면 JSON으로 옮긴다.
- HUD layout은 Scene에 넣지 않고 UI layer에 둔다.
- 챔피언별 데이터는 `ChampionDef` 본체에 억지로 섞지 말고 HUD asset registry로 둔다.
- `HUD_Irelia.png`는 비교용 레퍼런스일 뿐 최종 렌더 path에서 제거한다.
