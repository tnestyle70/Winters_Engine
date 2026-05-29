# 맵 벽 NavGrid 차단 계획서 (Map Wall NavGrid Blocking)

**작성일**: 2026-05-13
**작성자**: Claude (사용자 요청: "현재 포탑처럼 맵의 이동 불가능한 벽도 동일하게 차단")
**상태**: 박제 완료 — 사용자 옵션 선택 (A / B / A→B 순차) 대기
**버전**: v1

**관계 문서**:
- `.md/process/PLAN_AUTHORING_PITFALLS.md` (P-1 ~ P-18 의무 게이트 통과)
- `.md/plan/engine/MINION_CROWD_COLLISION_PLAN.md` §0 ("미니언 vs 구조물 (포탑/벽): 완전 막힘" — 같은 카테고리로 분류된 문서)
- `.md/plan/WintersFormat/07_STAGE6_WMAP.md` §2 (장기 `.wmap` 의 `ObstacleEntry` — 본 계획 옵션 B 의 최종 통합 목표)
- `CLAUDE.md` §5.7 Plan Authoring Pitfalls + §8 계획서 규칙

---

## 0. 한 줄

> 현재 `Scene_InGame::Mark_StructuresOnNavGrid()` 가 **포탑/억제기/넥서스** 를 NavGrid carve out (원형 영역 `SetWalkable=false`) 하는 패턴을, **맵 지형의 벽** 영역에도 동일하게 적용한다.
> 1차 (Option A) = **자동 bake 임계값 튜닝** — 이미 작동 중인 `CMapWalkableBaker::BakeIntoNavGrid` 의 `MapWalkableBakeDesc` 4 파라미터를 ImGui 슬라이더로 노출 + 디폴트 강화.
> 2차 (Option B) = **명시적 Wall Obstacle 데이터** — `Stage1.dat` 에 `WallObstacleEntry[]` 추가 + `Mark_WallObstaclesOnNavGrid()` 신규 (`Mark_StructuresOnNavGrid` 와 동일 패턴).

---

## 1. 사실 수집 — Preflight (P-1 / P-2 / P-13 / P-18 통과)

> §1 의 모든 라인은 메인 레포 (`C:\Users\user\Desktop\Winters\`) 의 직접 Read + grep 결과. 줄 번호는 메인 레포 실측치. TBD / 추정 / 필요 단어 0 (P-6 통과).

### 1.1 현재 NavGrid 인프라

| 항목 | 위치 | 상태 | 핵심 사실 |
|---|---|---|---|
| `CNavGrid` | [`Engine/Public/Manager/Navigation/NavGrid.h:17-55`](Engine/Public/Manager/Navigation/NavGrid.h:17) | ✅ 작동 | 300×300 cells × 0.5 unit = 150×150m world, 1 bit/cell, default walkable, `SetWalkable(int x, int y, bool)` 공개 API |
| `CPathfinder::Find_Path` | [`Engine/Public/Manager/Navigation/Pathfinder.h:14`](Engine/Public/Manager/Navigation/Pathfinder.h:14), [`.cpp:35-123`](Engine/Private/Manager/Navigation/Pathfinder.cpp:35) | ✅ 작동 | A* 8-dir Octile heuristic. `IsWalkable(false)` 셀은 expand 시 자동 skip (line 88) |
| `CNavigationSystem` | [`Engine/Public/ECS/Systems/NavigationSystem.h:29`](Engine/Public/ECS/Systems/NavigationSystem.h:29) | ✅ 작동 | `Phase=3` (정수, P-9 통과). NavAgent.bPathDirty → Find_Path → 웨이포인트 follow → Velocity 갱신 |
| `NavAgentComponent` | [`Engine/Public/ECS/Components/NavAgentComponent.h:21-32`](Engine/Public/ECS/Components/NavAgentComponent.h:21) | ✅ 작동 | `pathCellsX/Y` (vector), `iPathIndex`, `bHasGoal`, `bPathDirty` |
| `CMapSurfaceSampler` | [`Engine/Public/Manager/Navigation/MapSurfaceSampler.h:23-74`](Engine/Public/Manager/Navigation/MapSurfaceSampler.h:23) | ✅ 작동 | 맵 `.wmesh` 의 모든 삼각형 → 512×512 grid 에 `height` + `normalY` 래스터화. `LoadFromWMesh(path, matWorld)` API |
| `CMapWalkableBaker::BakeIntoNavGrid` | [`Engine/Public/Manager/Navigation/MapWalkableBaker.h:23-34`](Engine/Public/Manager/Navigation/MapWalkableBaker.h:23), [`.cpp:120-234`](Engine/Private/Manager/Navigation/MapWalkableBaker.cpp:120) | ✅ 작동 | 자동 bake (다음 §1.3 상세) |
| `MapWalkableBakeDesc` | [`Engine/Public/Manager/Navigation/MapWalkableBaker.h:15-21`](Engine/Public/Manager/Navigation/MapWalkableBaker.h:15) | ✅ 존재 | 4 파라미터 디스크: `minNormalY=0.45f`, `maxStepHeight=0.75f`, `maxWorldY=8.0f`, `agentRadiusCells=1` |

### 1.2 현재 "포탑 차단" 패턴 — 사용자가 "동일하게" 라고 인용한 구체 코드

`Client/Private/Scene/Scene_InGame.cpp:1965-2000` 본문 직접 인용 (수정 없이):

```cpp
void CScene_InGame::Mark_StructuresOnNavGrid()
{
    if (!m_pNavGrid)
        return;
    const uint32_t iCount = CStructure_Manager::Get()->Get_Count();
    for (uint32_t i = 0; i < iCount; ++i)
    {
        TransformComponent* pTf = CStructure_Manager::Get()->Get_Transform(i);
        if (!pTf)
            continue;
        const Vec3 vPos = pTf->GetLocalPosition();
        f32_t radius = 2.f;
        EntityID entity = CStructure_Manager::Get()->Get_EntityAt(i);
        if (m_World.HasComponent<StructureComponent>(entity))
        {
            auto& sc = m_World.GetComponent<StructureComponent>(entity);
            if (sc.kind == static_cast<uint32_t>(Winters::Map::eObjectKind::Structure_Nexus))
                radius = 4.f;
            else if (sc.kind == static_cast<uint32_t>(Winters::Map::eObjectKind::Structure_Inhibitor))
                radius = 3.f;
        }
        const CNavGrid::Cell center = m_pNavGrid->WorldToCell(vPos);
        const int32_t rCells = static_cast<int32_t>(radius / CNavGrid::kCellSize);
        for (int32_t dy = -rCells; dy <= rCells; ++dy)
        {
            for (int32_t dx = -rCells; dx <= rCells; ++dx)
            {
                if (dx * dx + dy * dy <= rCells * rCells)
                    m_pNavGrid->SetWalkable(center.x + dx, center.y + dy, false);
            }
        }
    }
    char msg[128];
    sprintf_s(msg, "[NavGrid] %u structures marked as blocked\n", iCount);
    OutputDebugStringA(msg);
}
```

#### 1.2.1 추출된 "포탑처럼 막기" 패턴 (옵션 B 가 그대로 재사용할 4 요소)

1. **데이터 원천**: 싱글턴 매니저 (`CStructure_Manager::Get()`) 가 위치/유형 보유
2. **반경**: kind 별 (`Turret=2.0`, `Inhibitor=3.0`, `Nexus=4.0`)
3. **차단 방식**: `WorldToCell(center)` + 원형 영역 `dx²+dy² ≤ rCells²` 에 `SetWalkable(false)`
4. **호출 시점**: `InGameBootstrapBridge.cpp:325` — `BakeMapWalkableNavGrid()` (지형 bake) 이후 **1회**, ECS Scheduler 등록 이전

### 1.3 현재 맵 지형 자동 bake — 이미 부분적으로 작동 중

`Engine/Private/Manager/Navigation/MapWalkableBaker.cpp:120-234` 핵심 단계 직접 인용:

```cpp
bool_t CMapWalkableBaker::BakeIntoNavGrid(
    const CMapSurfaceSampler& surface,
    CNavGrid& navGrid,
    const std::vector<Vec3>& playableSeeds,
    const MapWalkableBakeDesc& desc)
{
    if (!surface.IsReady())
        return false;

    // 1) Candidates 선정 — 각 cell 의 표면 샘플 검사
    //    조건: normalY >= minNormalY (경사 한도)
    //         && height <= maxWorldY (높이 상한)
    //         && 이웃 8 cell 모두 |height_neighbor - height| <= maxStepHeight
    //    → candidates[idx] = 1
    //    (line 134-152)

    // 2) Connected — seeds (player + champion + minion waypoint) 부터 BFS
    //    candidates 위에서만 expand. 도달하지 못한 candidate 는 island 로 간주 → 제외
    //    (line 154-198)

    // 3) Final — agentRadiusCells 만큼의 clearance 검사 (≥1000 connected 일 때만)
    //    각 final walkable cell 은 반경 N cell 안의 모든 cell 이 connected 여야 함
    //    → 벽 가장자리의 좁은 띠가 자동 erosion 됨 = 에이전트 반경 만큼 "벽 패딩"
    //    (line 200-220)

    navGrid.SetAllWalkable(false);           // line 204
    // ... final walkable cell 만 SetWalkable(x, y, true)
}
```

`MapWalkableBakeDesc` 기본값 [`MapWalkableBaker.h:15-21`](Engine/Public/Manager/Navigation/MapWalkableBaker.h:15) 직접 인용:

```cpp
struct MapWalkableBakeDesc
{
    f32_t minNormalY = 0.45f;       // cosθ=0.45 → θ≈63°. 즉 63° 이하 경사면 walkable 후보
    f32_t maxStepHeight = 0.75f;    // 이웃 cell 과의 높이 차 허용 (계단 1단)
    f32_t maxWorldY = 8.0f;         // 표면 절대 높이 상한 (지붕/지나 높은 단상 제외)
    i32_t agentRadiusCells = 1;     // 0.5 cell × 1 = 0.5m 패딩
};
```

### 1.4 호출 흐름 (P-3 grep 결과 — 모든 SetWalkable 호출 경로)

```
[bake 시작] InGameBootstrapBridge.cpp:250     scene.m_pNavGrid = CNavGrid::Create(...)
[지형 bake] InGameBootstrapBridge.cpp:253     scene.BakeMapWalkableNavGrid()
            └─ Scene_InGame.cpp:1739          BakeMapWalkableNavGrid()
               └─ Scene_InGame.cpp:1784       CMapWalkableBaker::BakeIntoNavGrid(...)
                  └─ MapWalkableBaker.cpp:204 navGrid.SetAllWalkable(false)
                  └─ MapWalkableBaker.cpp:217 navGrid.SetWalkable(x, y, true)
[구조물 차단] InGameBootstrapBridge.cpp:325   scene.Mark_StructuresOnNavGrid()
            └─ Scene_InGame.cpp:1965         Mark_StructuresOnNavGrid()
               └─ Scene_InGame.cpp:1993      m_pNavGrid->SetWalkable(..., false)
```

**부재 (사용자 요청)**: 맵 지형의 "벽" 영역에 대한 **명시적** `SetWalkable(false)` 호출 0 hit.
**부분 존재**: `BakeIntoNavGrid` 가 `minNormalY=0.45f` (경사 63°) / `maxStepHeight=0.75f` (계단 0.75m) / `agentRadiusCells=1` (0.5m 패딩) 로 **자동 차단 중** — 단 임계값이 관대해 일부 벽 (얕은 경사 절벽, 좁은 통로 옆 절벽) 누락 가능.

### 1.5 ImGui 디버그 확인 채널 (이미 있음)

`Client/Public/Scene/Scene_InGame.h:177` + `Client/Private/UI/RenderDebug.cpp:25-46`:
```cpp
bool_t   IsDbgShowNavGrid() const { return m_bDbgShowNavGrid; }
void     SetDbgShowNavGrid(bool_t b) { m_bDbgShowNavGrid = b; }
// ImGui 체크박스: "NavGrid blocked cells"
// 슬라이더: "NavGrid radius (m)" (시각화 반경, 10~200m)
```

→ 인게임 F12 디버그 UI 에서 NavGrid blocked cells 을 빨간 점으로 오버레이. **1차 진단 도구 이미 있음**.

### 1.6 데이터 (맵 모델) 실측

`Client/Private/Scene/Scene_InGame.cpp:1728-1730`:
```cpp
if (!sampler->LoadFromWMesh(
    L"Client/Bin/Resource/Texture/MAP/output/sr_base_flip.wmesh",
    m_MapTransform.GetWorldMatrix()))
```

→ 맵 모델 = `Client/Bin/Resource/Texture/MAP/output/sr_base_flip.wmesh` (소환사의 협곡 단일 메쉬). 별도 wall collision mesh 분리 없음.

### 1.7 §1 결론 (P-1 / P-6 통과)

- `SetWalkable` 공개 API: `void SetWalkable(int32_t cx, int32_t cy, bool_t walkable)` — `NavGrid.h:33` 실재 (P-13 미존재 API 0).
- `CStructure_Manager` 패턴은 옵션 B 가 그대로 재사용 가능 (P-18 기존 인프라 인지).
- Owner scope: `CNavGrid` 은 `Scene_InGame::m_pNavGrid` 멤버 (`Scene_InGame.h:295`). Engine 전역 X — Scene 한정. 옵션 B 의 `WallObstacle_Manager` 도 동일 scope 로 박제 (P-10 통과).
- 음수 좌표 안전: `CNavGrid::WorldToCell` (`NavGrid.cpp:46-52`) 이 `std::floor` 사용 (P-12 통과 — 메인 레포 이미 안전).

---

## 2. 솔루션 옵션 비교

| 옵션 | 작업량 | 데이터 변경 | 사용자 의도 부합 | 권장 |
|---|---|---|---|---|
| **A. 자동 bake 임계값 튜닝** | 1~2 시간 | 0 (디폴트값만) | ⚠ 부분 (포탑처럼 "명시적" X) | **1차 즉시 진입** |
| **B. 명시적 Wall Obstacle 데이터** | 2~3일 | Stage1.dat 확장 + Editor + Manager | ✅ 포탑과 동일 패턴 | **2차 (1차 검증 후)** |
| C. 별도 wall .wmesh + invert mark | 1~2일 | 맵 모델 분리 필요 (외부 작업) | ⚠ 부분 | 보류 |

### 2.1 옵션 A vs B 분기 기준

A 만으로 충분한 경우:
- 임계값 (`minNormalY=0.7f` 정도로 강화 + `agentRadiusCells=2`) 조정만으로 인게임 미니언/챔프 벽 통과 0
- ImGui 슬라이더 토글 후 `BakeMapWalkableNavGrid` 재호출 시 시각적으로 빨간 점이 모든 벽을 덮음

B 가 필요한 경우:
- 임계값 강화 시 좁은 통로 (e.g., 정글 길) 까지 false-positive 로 차단되어 미니언 path empty 다발
- 특정 좁은 벽 (e.g., 부쉬 사이 갭) 이 어떤 임계값에서도 차단 안 됨
- 사용자가 "에디터에서 마우스로 추가 차단 영역 박제하고 싶음" 요구

분기는 1차 (A) 진입 후 인게임 검증 후 결정.

### 2.2 본 계획서가 박제할 것

§3 — 옵션 A 상세 박제 (즉시 진입 가능)
§4 — 옵션 B outline (사용자 채택 시 v2 박제)
§5 — 검증/마일스톤
§6 — 함정 (PLAN_AUTHORING_PITFALLS 자체 점검)

---

## 3. Option A — 자동 bake 임계값 튜닝 + ImGui 슬라이더 (1차 진입)

### 3.1 작업 요약

`CMapWalkableBaker::BakeIntoNavGrid` 가 받는 `MapWalkableBakeDesc` 를:
1. Scene_InGame 의 멤버로 **노출** (기존: 함수 안에서 `{}` 기본 생성).
2. ImGui 슬라이더로 **실시간 조정**.
3. "Re-Bake NavGrid" 버튼으로 **즉시 재계산**.
4. 기본값을 **벽 차단 강화** 쪽으로 조정.

### 3.2 새/수정 파일 목록 (P-3 호출 경로 전수)

| # | 파일 | 작업 | 라인 |
|---|---|---|---|
| 1 | `Client/Public/Scene/Scene_InGame.h` | 멤버 추가 + getter/setter + Re-Bake 메서드 | +25 |
| 2 | `Client/Private/Scene/Scene_InGame.cpp` | `BakeMapWalkableNavGrid()` 가 멤버 desc 사용 + Re-Bake 함수 본체 | +30 / -2 |
| 3 | `Client/Public/UI/NavGridTunerPanel.h` | 신규 ImGui 패널 헤더 | +30 |
| 4 | `Client/Private/UI/NavGridTunerPanel.cpp` | 신규 ImGui 패널 본문 | +90 |
| 5 | `Client/Private/UI/RenderDebug.cpp` | NavGridTunerPanel 호출 추가 | +5 |
| 6 | `Engine/Public/Manager/Navigation/MapWalkableBaker.h` | (선택) 디폴트 값 강화 — 또는 변경 없이 Scene 멤버에서만 조정 | +0 또는 -4 / +4 |

→ **Engine 측 헤더는 변경 없이** Scene 측 멤버로 디폴트 강화 박제 (Surgical, §7 가이드라인). Engine 디폴트는 다른 게임/맵에 영향 주므로 보류.

### 3.3 코드 박제 (전문)

#### 3.3.1 `Client/Public/Scene/Scene_InGame.h` 수정

**위치 1**: L25 (기존 include) 다음에 추가 — `MapWalkableBakeDesc` 가져오기

기존:
```cpp
#include "Manager/Navigation/NavGrid.h"    // CNavGrid — Scene 이 소유
```

수정 후:
```cpp
#include "Manager/Navigation/NavGrid.h"    // CNavGrid — Scene 이 소유
#include "Manager/Navigation/MapWalkableBaker.h"  // MapWalkableBakeDesc — ImGui 튜닝
```

**위치 2**: L294 (NavGrid 멤버 주석) 직전에 멤버 + 메서드 추가

기존:
```cpp
    // NavGrid — 외부 시스템이 SetWalkable 로 구조물 점유 마킹 (향후). 빈 그리드로 부트스트랩.
    unique_ptr<CNavGrid> m_pNavGrid;
```

수정 후:
```cpp
    // NavGrid — 외부 시스템이 SetWalkable 로 구조물 점유 마킹 (향후). 빈 그리드로 부트스트랩.
    unique_ptr<CNavGrid> m_pNavGrid;

    // [Map Wall] BakeMapWalkableNavGrid 의 임계값 — ImGui 튜닝.
    // 디폴트는 MapWalkableBakeDesc 의 0.45/0.75/8.0/1 보다 벽 차단 강화한 값으로 시작.
    Engine::MapWalkableBakeDesc m_BakeDesc{ 0.70f, 0.40f, 8.0f, 2 };
```

**위치 3**: L330 (`BakeMapWalkableNavGrid()` 선언) 직후에 메서드 + getter/setter 추가

기존:
```cpp
    void InitializeMapSurfaceSampler(bool_t bMapLoaded);
    void BakeMapWalkableNavGrid();
```

수정 후:
```cpp
    void InitializeMapSurfaceSampler(bool_t bMapLoaded);
    void BakeMapWalkableNavGrid();
    // [Map Wall] 슬라이더 변경 후 ImGui 의 "Re-Bake" 버튼이 호출. 지형 bake + 구조물 mark 재실행.
    void ReBakeNavGridForWalls();

public:
    // [Map Wall] ImGui 패널 접근. 슬라이더가 직접 desc 수정.
    Engine::MapWalkableBakeDesc&       Get_BakeDesc()       { return m_BakeDesc; }
    const Engine::MapWalkableBakeDesc& Get_BakeDesc() const { return m_BakeDesc; }
```

#### 3.3.2 `Client/Private/Scene/Scene_InGame.cpp` 수정

**위치 1**: `BakeMapWalkableNavGrid()` L1783 — 기본 생성 desc 를 멤버 desc 로 교체

기존 (L1783-L1788):
```cpp
    Engine::MapWalkableBakeDesc desc{};
    const bool_t bBaked = Engine::CMapWalkableBaker::BakeIntoNavGrid(
        *m_pMapSurfaceSampler,
        *m_pNavGrid,
        seeds,
        desc);
```

수정 후:
```cpp
    const bool_t bBaked = Engine::CMapWalkableBaker::BakeIntoNavGrid(
        *m_pMapSurfaceSampler,
        *m_pNavGrid,
        seeds,
        m_BakeDesc);
```

**위치 2**: `Mark_StructuresOnNavGrid()` L2000 (함수 끝) 직후에 새 함수 추가

기존 (L1997-L2000):
```cpp
    char msg[128];
    sprintf_s(msg, "[NavGrid] %u structures marked as blocked\n", iCount);
    OutputDebugStringA(msg);
}
```

수정 후 (L2000 직후 새 함수 추가):
```cpp
    char msg[128];
    sprintf_s(msg, "[NavGrid] %u structures marked as blocked\n", iCount);
    OutputDebugStringA(msg);
}

void CScene_InGame::ReBakeNavGridForWalls()
{
    // [Map Wall] ImGui 튜닝 후 호출. 임계값 변경 시 NavGrid 전체 재계산.
    // 1) NavGrid 비트 전체 reset (BakeIntoNavGrid 가 내부에서 SetAllWalkable(false) 호출하므로 명시 reset 불필요)
    // 2) 지형 bake — 새 임계값 적용
    BakeMapWalkableNavGrid();
    // 3) 구조물 차단 — 포탑/억제기/넥서스 carve out 재적용
    Mark_StructuresOnNavGrid();

    OutputDebugStringA("[NavGrid] re-baked with new thresholds\n");
}
```

#### 3.3.3 `Client/Public/UI/NavGridTunerPanel.h` 신규

```cpp
#pragma once
#include "Engine_Defines.h"
#include "WintersTypes.h"

class CScene_InGame;

// ─────────────────────────────────────────────────────────────
// NavGridTunerPanel — Map Wall NavGrid 임계값 슬라이더
//   - MapWalkableBakeDesc 의 4 파라미터를 실시간 조정
//   - "Re-Bake NavGrid" 버튼으로 즉시 재계산
//   - "Show Blocked Cells" 체크박스 (m_bDbgShowNavGrid 토글)
// ─────────────────────────────────────────────────────────────
class CNavGridTunerPanel
{
public:
    // RenderDebug.cpp 의 ImGui Window 안에서 호출. Window 시작/끝은 caller 책임.
    static void Render(CScene_InGame* pScene);

private:
    CNavGridTunerPanel() = delete;
};
```

#### 3.3.4 `Client/Private/UI/NavGridTunerPanel.cpp` 신규

```cpp
#include "UI/NavGridTunerPanel.h"
#include "Scene/Scene_InGame.h"
#include "Manager/Navigation/MapWalkableBaker.h"
#include "imgui.h"

void CNavGridTunerPanel::Render(CScene_InGame* pScene)
{
    if (!pScene)
        return;

    if (!ImGui::CollapsingHeader("NavGrid Wall Tuner", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    Engine::MapWalkableBakeDesc& desc = pScene->Get_BakeDesc();

    // 1) Show blocked cells (이미 있는 m_bDbgShowNavGrid 재사용)
    bool_t bShow = pScene->IsDbgShowNavGrid();
    if (ImGui::Checkbox("Show NavGrid blocked cells", &bShow))
        pScene->SetDbgShowNavGrid(bShow);

    ImGui::Separator();
    ImGui::TextUnformatted("Wall Detection Thresholds (auto-bake)");

    // 2) minNormalY — 경사 한도. cos θ. 1.0 = 완전 수평만. 0.0 = 완전 수직 허용
    //    0.70 ≈ 45.6°, 0.85 ≈ 31.8°
    bool_t bDirty = false;
    bDirty |= ImGui::SliderFloat("Min Normal Y (slope)",
        &desc.minNormalY, 0.10f, 0.95f, "%.2f");
    ImGui::SameLine();
    ImGui::Text("(%.1f°)", static_cast<f32_t>(::acosf(desc.minNormalY) * 57.2958f));

    // 3) maxStepHeight — 이웃 cell 과의 높이 차 (계단 한도)
    bDirty |= ImGui::SliderFloat("Max Step Height (m)",
        &desc.maxStepHeight, 0.10f, 2.0f, "%.2f");

    // 4) maxWorldY — 표면 절대 높이 상한
    bDirty |= ImGui::SliderFloat("Max World Y (m)",
        &desc.maxWorldY, 1.0f, 50.0f, "%.1f");

    // 5) agentRadiusCells — 에이전트 반경 (clearance 셀 수). 1 cell = 0.5m
    bDirty |= ImGui::SliderInt("Agent Radius (cells)",
        &desc.agentRadiusCells, 0, 4);
    ImGui::SameLine();
    ImGui::Text("(%.2fm)",
        static_cast<f32_t>(desc.agentRadiusCells) * 0.5f);

    ImGui::Separator();

    // 6) Re-Bake 버튼 (수동 트리거)
    const bool_t bClicked = ImGui::Button("Re-Bake NavGrid (apply thresholds)");
    if (bClicked || (bDirty && ImGui::IsKeyDown(ImGuiKey_LeftShift)))
    {
        pScene->ReBakeNavGridForWalls();
    }

    // 7) 디폴트 복귀
    ImGui::SameLine();
    if (ImGui::Button("Reset to Default"))
    {
        desc = Engine::MapWalkableBakeDesc{ 0.70f, 0.40f, 8.0f, 2 };
        pScene->ReBakeNavGridForWalls();
    }

    ImGui::TextDisabled("Shift + slider = live re-bake (느릴 수 있음)");
    ImGui::TextDisabled("default: 0.70 / 0.40 / 8.0 / 2 (벽 강화)");
}
```

#### 3.3.5 `Client/Private/UI/RenderDebug.cpp` 수정

**위치**: L46 부근 (`pScene->SetDbgShowNavGrid(true);` 호출 영역) 다음, 디버그 윈도우 안에 패널 추가

기존 (예시 — 실제 라인 검증 후 박제):
```cpp
            bool bn = pScene->IsDbgShowNavGrid();
            if (ImGui::Checkbox("NavGrid blocked cells", &bn)) pScene->SetDbgShowNavGrid(bn);
            // ... 기존 NavGrid radius 슬라이더 ...
```

수정 후 — 기존 NavGrid 컨트롤 영역 끝에 패널 호출 추가:
```cpp
            bool bn = pScene->IsDbgShowNavGrid();
            if (ImGui::Checkbox("NavGrid blocked cells", &bn)) pScene->SetDbgShowNavGrid(bn);
            // ... 기존 NavGrid radius 슬라이더 ...

            // [Map Wall] 임계값 튜닝 패널
            CNavGridTunerPanel::Render(pScene);
```

또한 include 추가 (RenderDebug.cpp 상단):
```cpp
#include "UI/NavGridTunerPanel.h"
```

### 3.4 vcxproj 추가 (CLAUDE.md §5.1 빌드 강제)

`Client.vcxproj` 와 `Client.vcxproj.filters` 에 2 파일 추가:
- `Client/Public/UI/NavGridTunerPanel.h` → `<ClInclude>` (Filter: `Public\UI`)
- `Client/Private/UI/NavGridTunerPanel.cpp` → `<ClCompile>` (Filter: `Private\UI`)

(파일 추가는 IDE 또는 직접 vcxproj 텍스트 편집 — 표준 절차이므로 별도 박제 생략).

### 3.5 검증 매트릭스 (M0~M4)

| # | 단계 | 검증 |
|---|---|---|
| M0 | 빌드 (Debug x64) — vcxproj 추가 후 | unresolved 0, C1083 0 |
| M1 | F5 게임 실행 → ImGui 디버그 윈도우 → "NavGridTunerPanel" 헤더 펼치기 | 슬라이더 4개 + Re-Bake 버튼 표시 |
| M2 | "Show NavGrid blocked cells" 체크 | 빨간 점이 차단 영역 표시 (포탑 + 벽) |
| M3 | `minNormalY` 0.70 → 0.85 변경 + Re-Bake | 빨간 점 더 빽빽해짐 (가파른 벽 추가 차단) |
| M4 | 미니언 라인전 진행 → 우클릭으로 벽 너머 좌표 클릭 | 미니언이 벽 우회 또는 정지 (벽 통과 X) |

### 3.6 옵션 A 가 부족한 경우 → 옵션 B 진입 조건

다음 중 1개 이상 충족 시 옵션 B (§4) 박제 진입:
- M3 의 빨간 점이 좁은 통로 (정글 길) 까지 false-positive 차단
- M4 에서 특정 벽 영역이 어떤 임계값에서도 차단 안 됨
- 사용자가 "에디터에서 마우스로 추가 차단 영역 박제" 요구

---

## 4. Option B — 명시적 Wall Obstacle 데이터 (2차, 사용자 채택 시 v2 박제)

### 4.1 outline

포탑이 `CStructure_Manager` 싱글턴 + `Mark_StructuresOnNavGrid()` 패턴 그대로 벽에 적용:

1. **데이터 포맷** — `Client/Public/Map/MapDataFormats.h` 에 `WallObstacleEntry` 신규 POD:
   ```cpp
   struct WallObstacleEntry
   {
       uint32_t shape;    // 0=Box, 1=Sphere
       float    px, py, pz;     // center world pos
       float    sx, sy, sz;     // Box half-extents 또는 Sphere radius (sx 만 사용)
       float    yaw;            // Box yaw (radian)
       uint32_t reserved;
   };
   static_assert(sizeof(WallObstacleEntry) == 9 * 4);
   ```
2. **Stage1.dat 버전 bump**:
   - `STAGE_VERSION` 4 → 5
   - `STAGE_VERSION_MIN_COMPAT` 3 → 5 (또는 4 유지 + v5 한정 옵션)
   - `MapDataIO.cpp` 의 Save/Load 에 `WallObstacleEntry[]` 섹션 추가
3. **싱글턴 매니저** — `Client/Public/Manager/WallObstacle_Manager.h/.cpp`:
   - `CStructure_Manager` 와 동일 구조 (DECLARE_SINGLETON)
   - `Get_Count()`, `Get_Entry(i)`, `Add(...)`, `Remove(i)`, `Save(...)`, `Load(...)` API
4. **NavGrid 차단** — `Scene_InGame::Mark_WallObstaclesOnNavGrid()` 신규:
   - `Mark_StructuresOnNavGrid()` 와 동일 패턴
   - Box 는 OOB 회전 후 셀 raster, Sphere 는 원형 carve out (포탑 패턴 그대로)
5. **호출 추가** — `InGameBootstrapBridge.cpp:325` 직후 (또는 직전):
   ```cpp
   scene.Mark_StructuresOnNavGrid();
   scene.Mark_WallObstaclesOnNavGrid();   // 신규 — 동일 시점, 동일 패턴
   ```
6. **Scene_Editor 확장** — 마우스 클릭으로 Box/Sphere 박제 + Ctrl+S 로 Stage1.dat 저장:
   - 기존 `Client/Private/Scene/Scene_Editor.cpp` 의 구조물 배치 UI 확장
   - "Wall" 탭 추가
7. **장기 통합**: Stage6 `.wmap` (`.md/plan/WintersFormat/07_STAGE6_WMAP.md` §2 `ObstacleEntry`) 으로 흡수 — 본 계획의 `WallObstacleEntry` 는 동일 의미 + 더 단순. Stage6 진입 시 변환 단방향.

### 4.2 옵션 B 채택 시 v2 박제 항목

사용자가 옵션 B 진입 결정 시 본 계획서 v2 박제:
- §1 Preflight 에 `STAGE_VERSION = 4`, 기존 Stage1.dat 의 모든 entry 표 실측 추가
- §3 (옵션 A) 박제 유지 + §4 (옵션 B) 코드 전문 박제
- `WallObstacle_Manager.h/.cpp` 전문 박제 (`CStructure_Manager` 본문 인용 후 동일 구조)
- `Mark_WallObstaclesOnNavGrid()` 본문 박제 — Box OOB 회전 raster + Sphere 원형 carve

---

## 5. 마일스톤 (옵션 A 즉시 진입)

```
M0  현재 빌드 상태 확인 (Debug x64 success)
M1  Engine/Public/Manager/Navigation/MapWalkableBaker.h 확인 — desc 4 필드 그대로 사용
M2  Scene_InGame.h 수정 — m_BakeDesc + Get_BakeDesc + ReBakeNavGridForWalls 박제
M3  Scene_InGame.cpp 수정 — BakeMapWalkableNavGrid 가 m_BakeDesc 사용 + ReBakeNavGridForWalls 본문
M4  Client/Public/UI/NavGridTunerPanel.h 신규
M5  Client/Private/UI/NavGridTunerPanel.cpp 신규
M6  Client/Private/UI/RenderDebug.cpp 에 패널 등록
M7  Client.vcxproj + .filters 에 2 파일 추가
M8  Build (Debug x64) — unresolved 0
M9  F5 → ImGui 디버그 윈도우 → NavGridTunerPanel 표시 검증
M10 미니언 라인전에서 벽 통과 0 검증 (사용자 시각 확인)
M11 임계값 다양한 조합 시험 후 최적 디폴트 결정 → Scene_InGame.h 의 m_BakeDesc 초기값 갱신
```

예상 시간: **1~2 시간** (M0~M11 일괄).

---

## 6. PLAN_AUTHORING_PITFALLS 자체 점검 (8 GATE)

| GATE | 점검 항목 | 본 계획서 |
|---|---|---|
| A — 사실 수집 | 인용 모든 .h/.cpp 의 줄 번호 + 직접 인용 블록 박제 | ✅ §1.1~§1.4 |
| B — TODO 0 | "추정/필요/TBD" 0 | ✅ §1.7 |
| C — 호출 경로 grep | SetWalkable 호출 전수 (4 hit) + bake 호출 전수 (1 hit) | ✅ §1.4 |
| D — ECS 책임 경계 | 본 계획은 ECS Phase 변경 0. NavGrid 변경은 게임 시작 1회 (Phase 무관) + Bridge.cpp 호출 패턴 그대로 | ✅ N/A (ECS 미진입) |
| E — 자료형 | NavGrid 90K cells × 1 bit = 11.25 KB. 옵션 B 의 `WallObstacleEntry` 36B × MAX_WALLS=256 = 9.2 KB | ✅ |
| F — Scheduler / 동시성 | Phase 변경 0. NavGrid 변경은 Scheduler 등록 이전 (Bridge.cpp:325) → race 0 | ✅ |
| G — Owner Scope | CNavGrid = Scene 멤버 (m_pNavGrid). m_BakeDesc = Scene 멤버. Wall_Manager 도 Scene 한정 (Stage1.dat 의존이므로 LoL specific OK) | ✅ §1.7 |
| H — 인용 의미 + 행동 보존 | 옵션 A 는 **임계값 디폴트만 강화** — 기존 자동 bake 알고리즘 변경 0 (행동 보존). 옵션 B 는 별도 데이터 추가 (기존 동작 무변) | ✅ |

특별 점검:
- **P-7 (자료형 미래 사례)**: 옵션 B 의 `WallObstacleEntry::shape` 가 `uint32_t` — Box/Sphere/Polygon 3 종만으로 충분. MAX_WALLS=256 도 LoL 협곡 기준 충분.
- **P-11 (도메인 상수)**: CNavGrid `kCellSize=0.5f` 는 LoL 한정 도메인. 엘든링 진입 시 별도 NavGrid 인스턴스 필요 (별도 계획서). 본 계획은 LoL 한정 박제 — 향후 InitDesc 주입 분리 박제 시 영향 0.
- **P-14 (행동 정책 변경)**: 옵션 A 의 디폴트 변경 (`0.45/0.75/-/1` → `0.70/0.40/8.0/2`) 은 **자동 차단 강화** — 라인전 미니언이 좁은 벽 가까이 못 가게 됨. 시각/플레이 변경 의도된 결과. M10 사용자 검증 필수.

---

## 7. 위험 + 대응

| 위험 | 영향 | 대응 |
|---|---|---|
| 디폴트 임계값 강화 후 정글 좁은 길까지 차단 → 미니언 path empty 폭증 | 미니언 stuck | M10 검증 시 정글 경로 확인. 발생 시 `agentRadiusCells` 2 → 1 또는 `minNormalY` 0.70 → 0.55 후퇴 |
| `BakeIntoNavGrid` 가 매 Re-Bake 마다 O(W×H×8) → 60ms+ stall | 슬라이더 드래그 시 프레임 끊김 | Live re-bake 는 Shift+슬라이더로 한정 (§3.3.4 line 65). 일반 드래그는 desc 만 갱신, 버튼 클릭 시만 bake |
| 슬라이더 변경 후 m_pNavGrid 비트 변경 시 NavigationSystem 이 같은 프레임에 path 읽으면 race | 미니언 path 한 프레임 invalid | ReBake 는 Main thread + 동기. NavigationSystem 의 path 재계산 트리거는 `agent.bPathDirty=true` (다음 프레임 자연 갱신) — 안전 |
| 옵션 A 만으로 사용자 의도 미충족 (벽 = 명시적 obstacle 원함) | 옵션 B 박제 필요 | §2.1 분기 기준 + §4 outline 박제 — v2 박제 가능 |

---

## 8. 다음 단계 (사용자 결정 사항)

| 결정 | 다음 행동 |
|---|---|
| 옵션 A 만 진입 | M0~M11 코드 박제 → 사용자 인게임 검증 → 디폴트 확정 |
| 옵션 A → B 순차 | M0~M11 (옵션 A) 박제 → M10 검증 → 옵션 B v2 박제 시작 |
| 옵션 B 직진 (A 생략) | 본 계획서 v2 박제 (§4 상세 + Stage1.dat 버전 bump + WallObstacle_Manager + Editor 확장) |
| 옵션 C (별도 wall .wmesh) | 본 계획서 v3 박제 (맵 모델 분리 작업 의존성 — 사용자 외부 작업 필요) |

---

## 9. CLAUDE.md §8 Active Plans 등록 권유

박제 완료 후 CLAUDE.md §8 표에 1 줄 추가:

```markdown
| **Map Wall NavGrid Blocking (Option A 진입 대기)** | 박제 완료, 사용자 결정 대기 | `.md/plan/engine/MAP_WALL_NAVGRID_BLOCKING_PLAN.md` |
```

---

**END OF DOCUMENT — v1**
