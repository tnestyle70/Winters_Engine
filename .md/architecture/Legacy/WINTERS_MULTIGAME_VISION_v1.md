# Winters Engine — Multi-Game Vision (LoL + Elden Ring → Class & Servant) — **rev 2**

> ⚠️ **DEPRECATED 2026-05-04** — 통합본 [`WINTERS_MULTIGAME_ARCHITECTURE.md`](WINTERS_MULTIGAME_ARCHITECTURE.md) 로 대체. 본 rev 2 의 Phase 시간표 / Class & Servant 디자인 후보 3종 / Engine 공유 매트릭스 / PITFALLS GATE 모두 통합본에 박제됨. 직접 참조 금지 — 통합본만 권위.

**작성일**: 2026-05-04
**rev 2 (2026-05-04, Codex 검토 반영)**: ① Codex 의 `IGameModule` / `GameLaunchConfig` / `eGameProduct` 추상화 채택 ② Backend/Server 의 `product_id` 축 채택 (service 분리 v1 폐기) ③ 중기 `Games/*` 디렉토리 채택 ④ 용어 정정: **Class & Servant** (servent → servant)
**v1 폐기**: 디렉토리 분리만 박제, 모듈 추상화 미박제. v1 의 `Services/cmd/{lol_*,elden_*}/` 별도 분리 박제 폐기 — Codex 의 product_id 축이 호환성 우수.
**권위 마스터**: [`2026-05-04_GAME_SELECT_MULTI_GAME_PRODUCT_ARCHITECTURE.md`](../plan/engine/2026-05-04_GAME_SELECT_MULTI_GAME_PRODUCT_ARCHITECTURE.md) (Codex). 본 문서는 그 §2-§7 의 비전 요약 + Engine 측 통합.
**선행**: NEXTGEN_FRAMEWORK_MASTER (rev 2)
**관련**: CLAUDE.md L7-8 (단일 EXE 비전 정정 완료)

---

## §rev 2 Codex 검토 반영 매트릭스

| # | v1 | rev 2 정정 | 사유 |
|---|---|---|---|
| 1 | `Client/Public/Scene/{LoL,Elden,CS}/` 단순 분리 | **`Client/Public/GameModule/` + IGameModule 추상화** — 게임을 모듈로 등록 (`CreateInitialScene/MainMenu/InGame`) | Codex §4.3 IGameModule 채택. 게임 = 모듈 = 등록 단위 |
| 2 | Game Select Scene 단순 메뉴 | **GameProductModule 선택 진입점** — `eGameProduct` (None/WintersLOL/WintersElden/ClassServant) + `GameLaunchConfig` (content_root / service_namespace / server_endpoint / bUseOnlineServices / bUseEditorTools) | Codex §4.1, 4.2 |
| 3 | `Services/cmd/{lol_*,elden_*}/` 별도 service 분리 | **공통 service 유지 + `product_id` 축 추가** — DB 컬럼 + URL prefix (`/v1/lol/`, `/v1/elden/`, `/v1/class-servant/`) | Codex §7.1. 서비스 코드 재사용 + 도메인 분리 양립 |
| 4 | `Server/Public/{LoL,Elden}/` 별도 GameRoom | **공통 Server Runtime + GameMode 분리** — `LOLGameMode`, `EldenGameMode`, `ClassServantGameMode` (Network/Session 위에 GameMode 등록) | Codex §7.2. 네트워크/세션 코드 단일 |
| 5 | 디렉토리 분리만 명시 | **2 단계 진화**: 당장 = `Client/Public/GameModule/` 안에 분리 + 중기 = `Games/WintersLOL/{Client,Server,Data}/` 통째 분리 | Codex §6.1, 6.2 |
| 6 | "Class & Servent" (오타) | **"Class & Servant"** | Codex §0/§2 표기. 의도된 단어 |
| 7 | Phase A/B/C 시간표 | (rev 2 유지) — Codex 가 시간표 별도 박제 X. 본 v1 시간표 유지 |

---

## §rev 2 신규: 코어 추상화 (Codex §4 채택)

### eGameProduct (신규 enum)

**파일**: `Engine/Include/GameProduct.h` (신규 — Engine 공통, Client/Server/Services/AntiCheat 모두 공유)

```cpp
#pragma once
#include "WintersTypes.h"

enum class eGameProduct : u32_t
{
    None         = 0,
    WintersLOL,
    WintersElden,
    ClassServant,
};
```

### GameLaunchConfig (Game Select 결과)

**파일**: `Client/Public/GameModule/GameLaunchConfig.h` (신규)

```cpp
#pragma once
#include "GameProduct.h"
#include <string>

struct GameLaunchConfig
{
    eGameProduct  eProduct = eGameProduct::None;
    std::wstring  strContentRoot;       // "Data/LoL/", "Data/Elden/"
    std::wstring  strServiceNamespace;  // "/v1/lol/", "/v1/elden/"
    std::wstring  strServerEndpoint;    // 게임 서버 IP:Port
    bool_t        bUseOnlineServices = false;
    bool_t        bUseEditorTools    = true;
};
```

### IGameModule (게임 = 모듈 추상화)

**파일**: `Client/Public/GameModule/IGameModule.h` (신규)

```cpp
#pragma once
#include "GameProduct.h"
#include "GameLaunchConfig.h"
#include "IScene.h"
#include <memory>

class IGameModule
{
public:
    virtual ~IGameModule() = default;

    virtual eGameProduct GetProductID() const = 0;
    virtual const char*  GetDisplayName() const = 0;

    virtual bool_t       InitializeClient(const GameLaunchConfig& cfg) = 0;
    virtual void         ShutdownClient() = 0;

    virtual std::unique_ptr<IScene> CreateInitialScene() = 0;
    virtual std::unique_ptr<IScene> CreateMainMenuScene() = 0;
    virtual std::unique_ptr<IScene> CreateInGameScene() = 0;
};
```

**구현 모듈** (각 게임):
- `LOLGameModule` — `Client/Private/GameModule/LOL/LOLGameModule.cpp`
- `EldenGameModule` — `Client/Private/GameModule/Elden/EldenGameModule.cpp`
- `ClassServantGameModule` — 미래

**초기 구현**: 동일 EXE 안의 C++ class registry. 추후 DLL 분리 가능 (선택).

---

## §rev 2 정정: 디렉토리 (2 단계 진화 채택 — Codex §6)

---

## §0. 최종 목표 (Class & Servent)

**Winters Engine 의 최종 산출물**: **Class & Servent** — LoL (5v5 PvP MOBA) + Elden Ring (오픈월드 액션 RPG) **장점 결합 차세대 게임**.

**Engine 검증 게임 2종** (Class & Servent 진입의 prerequisite):
- **WintersLOL** — LoL 30일 모작 (풀스택 MOBA + 백엔드 + 안티치트)
- **WintersElden** — 엘든링 모작 (액션 RPG + 오픈월드 + 보스 + 멀티 던전)

이 두 게임이 동작 가능하면 Engine 이 두 장르 모두 감당함을 증명 → Class & Servent 진입 가능.

---

## §1. 비전 변경 사항 (v0 → v1)

### v0 (기존, CLAUDE.md L7-8)
```
WintersEngine.dll
├── WintersLOL.exe      ← 별도 EXE
└── WintersElden.exe    ← 별도 EXE
```

### v1 (단일 EXE + Game Select)
```
WintersEngine.dll
└── WintersGame.exe     ← 단일 클라
    ├── Scene_Logo
    ├── Scene_Login
    ├── Scene_GameSelect    ★ 신규 — 어떤 게임 플레이할지 선택
    │
    ├── [LoL 분기]
    │   ├── Scene_LoL_MainMenu
    │   ├── Scene_LoL_BanPick
    │   ├── Scene_LoL_MatchLoading
    │   └── Scene_LoL_InGame
    │
    ├── [Elden Ring 분기]
    │   ├── Scene_Elden_TitleScreen
    │   ├── Scene_Elden_CharacterCreate
    │   ├── Scene_Elden_WorldLoading
    │   └── Scene_Elden_OpenWorld
    │
    └── [Class & Servent 분기 — 미래]
        └── Scene_CS_*  (디자인 미정)
```

**왜 단일 EXE?**
1. **Engine 공유 검증** — 둘 다 같은 WintersEngine.dll 사용. 인프라 재사용성 증명 (ECS v2 / Fiber / Render Graph / GPU Driven).
2. **유지보수 단순화** — 빌드 1회, 버그 수정 1회.
3. **Class & Servent 자연 진입** — 동일 EXE 안에 신규 Scene 추가만으로 진입.
4. **사용자 경험** — 런처 1개. 추후 Class & Servent 정식 출시 시 LoL/Elden 선택지 유지 가능 (또는 단계별 폐기).

---

### §rev 2 디렉토리 진화 — Codex 채택

**Stage 1 — 당장 적용** (저장소 흔들기 최소):
```
Client/Public/
├── GameModule/           ★ 신규
│   ├── GameProduct.h
│   ├── GameLaunchConfig.h
│   ├── IGameModule.h
│   └── GameModuleRegistry.h
├── Scene/
│   └── Scene_GameSelect.h    ★ 신규

Client/Private/
├── GameModule/           ★ 신규
│   ├── LOL/LOLGameModule.cpp
│   ├── Elden/EldenGameModule.cpp
│   └── ClassServant/ClassServantGameModule.cpp
└── Scene/
    └── Scene_GameSelect.cpp  ★ 신규
```

기존 LoL 코드 (Scene_BanPick / Scene_InGame / GameObject/Champion 등) **이동 X**. `LOLGameModule` 이 기존 `BanPick → InGame` 플로우를 wrap 만.

**Stage 2 — 중기** (게임별 코드 거대화 시):
```
Games/
├── WintersLOL/
│   ├── Client/   (LoL Scene + GameObject)
│   ├── Server/   (LOLGameMode)
│   └── Data/     (맵 / 챔프 / 미니언)
├── WintersElden/
│   ├── Client/
│   ├── Server/   (EldenGameMode)
│   └── Data/
└── ClassServant/
    ├── Client/
    ├── Server/
    └── Data/

Engine/      (공유 인프라 — ECS v2 / Fiber / RG / GPU Driven)
Shared/      (공통 데이터 스키마)
Services/    (Backend Go service — product_id 축)
AntiCheat/   (커널 + 게임별 Validator)
Tools/
Client/      ← 공통 런처/부트스트랩 (Stage 2 에서 Game Select + Login 만)
```

---

## §2. 디렉토리 구조 (v1 — 폐기, rev 2 의 Stage 1+2 채택)

```
Winters/
├── Engine/                   ← 공유 인프라 (WintersEngine.dll)
│   ├── Public/
│   │   ├── ECS/              ECS v2 (Generation handle)
│   │   ├── Core/             Fiber JobSystem v2
│   │   ├── Renderer/         Render Graph + GPU Driven
│   │   ├── RHI/              IRHIDevice / IRHICommandList / RHIHandles
│   │   └── ...
│   └── Private/
│
├── Shared/                   ← 게임 간 공유 데이터 스키마
│   ├── Schemas/              FlatBuffers .fbs (LoL/Elden 공통: TransformSnapshot 등)
│   └── ...
│
├── Client/                   ← WintersGame.exe 단일 클라
│   ├── Public/
│   │   ├── Scene/
│   │   │   ├── Scene_Logo.h
│   │   │   ├── Scene_Login.h
│   │   │   ├── Scene_GameSelect.h        ★ 신규
│   │   │   ├── LoL/                       ★ 게임별 분리
│   │   │   │   ├── Scene_LoL_MainMenu.h
│   │   │   │   ├── Scene_LoL_BanPick.h
│   │   │   │   ├── Scene_LoL_MatchLoading.h
│   │   │   │   └── Scene_LoL_InGame.h
│   │   │   ├── Elden/                     ★ 신규
│   │   │   │   ├── Scene_Elden_TitleScreen.h
│   │   │   │   └── Scene_Elden_OpenWorld.h
│   │   │   └── ClassServent/              ★ 미래
│   │   ├── GameObject/
│   │   │   ├── LoL/                       LoL 챔프/미니언/타워
│   │   │   ├── Elden/                     보스/장비/소환수
│   │   │   └── ClassServent/
│   │   └── ...
│   └── Private/
│
├── Server/                   ← 멀티플레이어 서버
│   ├── Public/
│   │   ├── LoL/              GameRoom_LoL (5v5 IOCP) / Matchmaking / Anticheat profile
│   │   ├── Elden/            GameRoom_Elden (멀티 던전 / 오픈월드 sharding)
│   │   └── ClassServent/
│   └── Private/
│
├── Services/                 ← Go 백엔드 (각 게임별 분리)
│   ├── cmd/
│   │   ├── auth/             공통 (모든 게임)
│   │   ├── lol_matchmaking/  LoL 전용
│   │   ├── lol_shop/         LoL 전용
│   │   ├── elden_save/       Elden 전용 (캐릭터 저장)
│   │   ├── elden_messages/   Elden 전용 (블러드스테인 / 메시지)
│   │   └── classservent_*/
│   └── internal/
│       ├── lol/
│       ├── elden/
│       └── classservent/
│
├── AntiCheat/                ← 게임별 프로파일 (공통 커널 드라이버 + 게임별 정책)
│   ├── Driver/               공통 커널 .sys
│   ├── Service/              유저모드
│   ├── Profiles/
│   │   ├── lol_profile.json     속도핵/쿨다운 검증 룰
│   │   ├── elden_profile.json   슈퍼아머/공중 부유 검증 룰
│   │   └── classservent_profile.json
│   └── Shared/               IOCTL
│
├── Tools/                    공유 + 게임별 데이터 컨버터
├── EngineSDK/                Engine 헤더 flat 재배포
│
└── Data/                     게임별 에셋 (런타임)
    ├── LoL/                  맵 (소환사의 협곡) / 챔프 / 미니언 / 사운드
    ├── Elden/                오픈월드 청크 / 보스 / 장비 / NPC
    └── ClassServent/         (미래)
```

---

## §3. Game Select Scene — 신규 박제

### 3.1 Scene 흐름
```
Scene_Logo (3초 로고)
   ↓
Scene_Login (서버 인증 — Auth Service 8081)
   ↓
Scene_GameSelect ★ 신규
   ↓ [LoL 선택]                ↓ [Elden 선택]              ↓ [Class & Servent — Coming Soon]
Scene_LoL_MainMenu        Scene_Elden_TitleScreen      (미래)
```

### 3.2 Scene_GameSelect 박제

**파일**: `Client/Public/Scene/Scene_GameSelect.h` (신규)

```cpp
#pragma once
#include "IScene.h"
#include <memory>

enum class eGameMode : uint8_t
{
    None = 0,
    LoL,
    Elden,
    ClassServent,   // 미래
    GameSelect_END
};

class CScene_GameSelect : public IScene
{
private:
    CScene_GameSelect() = default;
public:
    ~CScene_GameSelect() override = default;
    static std::unique_ptr<CScene_GameSelect> Create();

    bool OnEnter() override;
    void OnExit() override;
    void OnUpdate(f32_t dt) override;
    void OnLateUpdate(f32_t dt) override;
    void OnRender() override;
    void OnImGui() override;

private:
    eGameMode m_Selected = eGameMode::None;

    // UI 상태
    bool m_bHoverLoL = false;
    bool m_bHoverElden = false;
};
```

**구현 핵심** (`Scene_GameSelect.cpp`):
```cpp
void CScene_GameSelect::OnImGui()
{
    ImGui::Begin("Winters Engine — Game Select", nullptr,
        ImGuiWindowFlags_NoScrollbar);

    ImGui::TextUnformatted("Choose your game:");
    ImGui::Separator();

    if (ImGui::Button("League of Legends (5v5 MOBA)", ImVec2(400, 80)))
    {
        Get_GameInstance()->Get_GameContext().selectedGame = eGameMode::LoL;
        Get_SceneManager()->Change_Scene(CScene_LoL_MainMenu::Create());
        ImGui::End();
        return;   // ★ Codex F-5
    }

    if (ImGui::Button("Elden Ring (Open World ARPG)", ImVec2(400, 80)))
    {
        Get_GameInstance()->Get_GameContext().selectedGame = eGameMode::Elden;
        Get_SceneManager()->Change_Scene(CScene_Elden_TitleScreen::Create());
        ImGui::End();
        return;
    }

    ImGui::BeginDisabled(true);
    if (ImGui::Button("Class & Servent (Coming 2027)", ImVec2(400, 80))) {}
    ImGui::EndDisabled();

    ImGui::End();
}
```

### 3.3 GameContext 확장

**파일**: `Engine/Include/GameContext.h`
```cpp
enum class eGameMode : uint8_t { None, LoL, Elden, ClassServent };

struct GameContext
{
    eGameMode    selectedGame = eGameMode::None;   // ★ 신규
    eChampion    SelectedChampion = eChampion::END;   // LoL 전용
    // 향후 Elden 캐릭터 ID, ClassServent 클래스 등 추가
};
```

---

## §4. Engine 공유 vs 게임별 분리 매트릭스

| 항목 | 공유 (Engine) | 게임별 (LoL/Elden/CS) |
|---|---|---|
| ECS v2 | ✅ | — |
| Fiber JobSystem v2 | ✅ | — |
| Render Graph | ✅ | 게임별 Pass 라이브러리 (LoL: 시야/타워 / Elden: 보스 부위 파괴 / CS: 미정) |
| GPU Driven | ✅ | 게임별 InstanceData 빌드 시스템 |
| RHI / RHIHandles | ✅ | — |
| Audio (FMOD) | ✅ | 게임별 사운드 뱅크 |
| Input | ✅ | 게임별 키바인딩 (LoL: QWER 클릭 / Elden: 락온 + 회피 / CS: 미정) |
| Network UDP/KCP | ✅ (Sim-10 v2) | 게임별 패킷 스키마 (LoL: ChampionSnapshot / Elden: WorldChunkDelta) |
| Asset 변환 (.wmesh / .wanim) | ✅ | — |
| **Champion / Boss / Class** | ❌ | 게임별 |
| **Map / World / Dungeon** | ❌ | 게임별 |
| **AI (BT / GOAP / MCTS)** | ✅ 인프라 | 게임별 BT 빌더 (LoL 봇 / Elden 보스 / CS 적) |
| **Backend Service** | ❌ | 게임별 (Auth 만 공유) |
| **AntiCheat 룰** | ✅ 인프라 | 게임별 프로파일 |

**원칙**: **인프라/시스템 = Engine 공유**, **콘텐츠/도메인 로직 = 게임별 분리**.

---

## §5. 단계별 진입 로드맵

### Phase A — WintersLOL 완성 (현재 진행, 2026-05~06)
- ECS v2 + Fiber + Render Graph + GPU Driven 인프라
- LoL 챔프 16 + 5v5 + 시야/타워/AI/봇
- Server 권위 (Sim-10 v2 UDP)
- Backend 8 service + AntiCheat
- **검증**: 5v5 매치 종료까지 안정적

### Phase B — WintersElden 진입 (2026-07~08)
- Game Select Scene 신설
- Elden 분기 진입: Scene_Elden_TitleScreen / OpenWorld
- 보스 시스템 (B-12 메쉬 분리 / MeshDestructible / EquipmentSlot — 이미 Phase B-16 v2 에서 hook 박제)
- 오픈월드 청크 스트리밍 (Render Graph + GPU Driven 활용)
- Elden 전용 Backend (캐릭터 저장 / 블러드스테인)
- **검증**: 보스 1체 + 오픈월드 1 청크 안정적

### Phase C — Class & Servent 디자인 + 출시 (2026-09~)
- LoL + Elden Ring 장점 결합 디자인 — **별도 비전 문서 필요** (`.md/design/CLASS_SERVENT_DESIGN.md` 예정)
- 가능 방향 (3 후보 — 디자인 검토 단계):
  - **C-1**: 오픈월드 PvP MOBA — Elden 의 거대 맵 + LoL 의 5v5 + 영지 시스템
  - **C-2**: 액션 MOBA — Elden 의 회피/락온/콤보 + LoL 의 챔프/스킬 + 5v5
  - **C-3**: 신규 장르 — Class (LoL 챔프 풀) + Servent (Elden 소환수 시스템) 결합 PvE/PvP 하이브리드
- **검증**: Steam Early Access 출시

---

## §6. Server / Backend / AntiCheat (★ rev 2 — Codex §7 product_id 축 채택)

### §rev 2 Codex §7 핵심 차이

| 항목 | v1 (별도 분리) | rev 2 (product_id 축) |
|---|---|---|
| Backend service | `cmd/lol_matchmaking/`, `cmd/elden_save/` 별도 | **공통 service** (Auth / Profile / Shop / Payment / Matchmaking / Leaderboard) **유지** + 모든 핵심 데이터에 `product_id` 컬럼 + URL prefix `/v1/lol/`, `/v1/elden/`, `/v1/class-servant/` |
| Server 분리 | `Server/Public/{LoL,Elden}/` 별도 GameRoom | **공통 Server Runtime** (Network / Session / Packet IO) **위에 GameMode** — `LOLGameMode`, `EldenGameMode`, `ClassServantGameMode` 등록 |
| Security | `Profiles/{lol,elden}_profile.json` (rev 2 동일) | (rev 2 동일) — 공통 AntiCheat Core + 제품별 Validator Set |

### §6.0 Backend (rev 2 — product_id 축)

| Service | 공통 | Product별 확장 |
|---|---|---|
| Auth | 계정/토큰 | 제품 권한 |
| Profile | 계정 프로필 | LOL 전적, Elden 캐릭터, ClassServant 성장 |
| Shop | 구매/상품 | 스킨, 장비, 서번트 |
| Payment | 결제 | 제품별 카탈로그 |
| Matchmaking | 큐 | 5v5 MOBA, Co-op 던전, PvPvE 전장 |
| Leaderboard | 랭킹 | 티어, 보스 타임어택, 시즌 랭킹 |

URL: `/v1/lol/...`, `/v1/elden/...`, `/v1/class-servant/...`. DB 에 `product_id` 컬럼 추가.

### §6.1 Server (rev 2 — GameMode 분리)

**rev 2 — 공통 Runtime + GameMode 등록 패턴** (Codex §7.2):
```
Network Session / Packet IO  (공통)
        ↓
Common Server Runtime         (공통)
        ↓
   ┌────┴────┬─────────┬──────────┐
   ▼         ▼         ▼          ▼
LOLGameMode  EldenGameMode  ClassServantGameMode
```

| Server Module | 책임 |
|---|---|
| `LOLGameMode` | 5v5 GameRoom + 라인/정글/포탑/미니언 + 스킬/투사체 검증 |
| `EldenGameMode` | 세션/월드 샤드 + 보스 AI 권위 + 스태미나/회피/패링/피격 검증 |
| `ClassServantGameMode` | PvPvE 전장 + 서번트 AI 권위 + 보스+라인+오브젝트 통합 |

**Multi-World**: 같은 Server process 안에 N 개 GameRoom (LoL 60 + Elden 멀티 던전 4 + ClassServant 매치 N) 동시. ECS v2 의 worldId + GameMode 분리.

### 6.2 Backend (Go service 게임별 분리)

| Service | 공유/게임별 | 포트 |
|---|---|---|
| Auth | 공유 | 8081 |
| LoL_Leaderboard | LoL | 8082 |
| LoL_Matchmaking | LoL | 8083 |
| LoL_Profile | LoL | 8084 |
| LoL_Payment | 공유 | 8085 |
| LoL_Shop | LoL | 8086 |
| Elden_Save | Elden | 8087 |
| Elden_Messages | Elden | 8088 |
| Elden_Coop | Elden | 8089 |
| ClassServent_* | CS | 8090+ |

### 6.3 AntiCheat (커널 공유 + 게임별 룰)

- 커널 드라이버 1개 (모든 게임 공통)
- 유저모드 룰 (`Profiles/*.json`):
  - `lol_profile.json` — 이동속도 4.5/sec, 쿨다운 검증, 사거리 7.75m
  - `elden_profile.json` — 점프 높이 / 슈퍼아머 / 공중 부유 (개구리 점프 사고 등)
  - `classservent_profile.json` — 미래

---

## §7. Class & Servent 디자인 방향 (잠정)

### 7.1 LoL 의 강점
- 5v5 PvP 균형
- 챔프 다양성 (스킬셋 매트릭스)
- 매크로 vs 마이크로 의사결정
- 빠른 매치 (30~40분)

### 7.2 Elden Ring 의 강점
- 오픈월드 탐험
- 보스 디자인 (패턴 학습 + 회피)
- 장비/빌드 자유도
- 액션 (회피/패리/락온)

### 7.3 결합 후보 (디자인 미정)

**A. 오픈월드 PvP MOBA**:
- 거대 맵 (소환사의 협곡 × 10)
- 5v5 → 20v20 영지 전쟁
- 영지/요새/보스 점령
- LoL 챔프 = Class. 보스 = 서번트 (Servent)

**B. 액션 MOBA**:
- LoL 5v5 + Elden 액션 (회피/락온/콤보)
- 컨트롤 진입 장벽 ↑, 숙련도 의미 ↑
- 챔프 시스템 + 회피/패링

**C. Class & Servent 신규 장르**:
- Class = 플레이어 챔프 (LoL 풀)
- Servent = 소환수 (Elden 소환 + 보스 일부)
- 장르: PvE/PvP 하이브리드 — 보스 사냥 + 영지 PvP

**선택 기준**: Engine 검증 (Phase A+B) 후 사용자 / 시장 / 기술 검토 → 별도 비전 문서.

---

## §8. CLAUDE.md / AGENTS.md 갱신 사항

### CLAUDE.md L7-8 정정 (필수)
```diff
-WintersEngine.dll
-├── WintersLOL.exe      ← 첫 타겟: LoL 30일 모작 (풀스택 MOBA)
-└── WintersElden.exe    ← 두 번째 타겟: 엘든링 모작 (액션RPG)
+WintersEngine.dll
+└── WintersGame.exe     ← 단일 클라
+    └── Scene_GameSelect → LoL / Elden / Class&Servent 분기
```

### CLAUDE.md L423-424 정정 (디렉토리)
```diff
-WintersLOL/     Client/(MOBA EXE) Server/(IOCP) Data/(챔피언Lua, 맵)
-WintersElden/   Client/(액션RPG EXE) Data/(보스, 오픈월드)
+Client/Public/Scene/LoL/         LoL 전용 Scene
+Client/Public/Scene/Elden/       Elden 전용 Scene
+Server/Public/LoL/               LoL GameRoom
+Server/Public/Elden/             Elden GameRoom
+Data/LoL/ + Data/Elden/          게임별 에셋
```

### 신규 §"Multi-Game Vision" 카테고리 추가
- 비전: Class & Servent 최종 출시
- 디렉토리 매트릭스
- Game Select Scene 위치
- 게임별 Backend / Server / AntiCheat 분리

---

## §9. PITFALLS GATE 통과

| GATE | 검증 |
|---|---|
| A 사실 수집 | CLAUDE.md L7-8 / L423-424 인용 + 현재 Scene 구조 (Logo/MainMenu/BanPick/MatchLoading/InGame) Glob 결과 |
| B TODO 0 | "미래" / "디자인 미정" 명시 — Class & Servent 만 ("미래" 명확 분리) |
| C 호출 경로 | Scene 변경 매트릭스 — Scene_BanPick → Scene_LoL_BanPick 으로 점진 |
| D ECS 책임 | 각 게임 GameRoom 별 CECSWorld (ECS v2 multi-world) |
| E 향후 자료형 | eGameMode = uint8_t 256 게임 (충분) |
| F Scheduler | 게임별 ECS 시스템 등록 차이 — Engine Scheduler 가 게임 모드 인식 |
| G Owner Scope | Engine 공유, Game-specific = Client/Public/Scene/{LoL,Elden,CS} 분리 |
| H 인용 의미 + 행동 보존 | LoL 단계 (Phase A) 의 모든 기존 Scene 동작 유지. Game Select 만 추가 |

---

## §10. 다음 진입

1. **CLAUDE.md L7-8 / L423-424 정정** — 단일 EXE 비전 박제 (즉시).
2. **Scene_GameSelect 박제** — Phase A 마무리 후 (Phase B 진입 시점).
3. **Class & Servent 디자인 문서** — `.md/design/CLASS_SERVENT_DESIGN.md` (Phase C 진입 시).
4. **게임별 디렉토리 분리** — Phase B 진입 시 Client/Public/Scene/LoL/ + Elden/ 마이그.

---

**END OF MULTI-GAME VISION v1**
