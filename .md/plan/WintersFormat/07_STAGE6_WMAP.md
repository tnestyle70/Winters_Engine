# Stage 6 — `.wmap` 맵 데이터 (Stage1.dat 승격 + NavGrid 통합)

> **목표**: 현재 `Client/Private/Map/MapDataIO.cpp` 의 `Stage1.dat` POD flat dump → `.wmap` 로 승격.
> 동시에 별도 파일 또는 런타임 생성이던 **NavGrid / Waypoint / MinionSpawn / JungleCamp 전체를 한 파일에 통합**.
> Phase 3-B (NavMesh 통합) 의 최종 데이터 원천.

> **기존 구조 참조** (현재 파일):
> - `Client/Public/Map/MapDataFormats.h` — `StageHeader`, `StructureEntry`, `JungleEntry`, `MinionWaypointEntry`
> - `Client/Private/Map/MapDataIO.cpp` — FILE* 순차 기록
> - `STAGE_VERSION = 4`, `STAGE_VERSION_MIN_COMPAT = 3`

---

## 1. 왜 승격

| 기존 `.dat` 문제 | `.wmap` 해결 |
|---|---|
| 매직 `STAGE_MAGIC` 만 — 공통 헤더 없음 (버전/압축/서명 X) | `WintersFileHeader` 재사용 |
| SHA256 없음 → 치트 가능 (Structure 위치 옮겨 와드 치트 등) | 표준 SHA256 |
| NavGrid 분리 저장 → 두 파일 로드 동기화 문제 | 한 파일에 섹션 통합 |
| Minion Waypoint 만 에디터 지원, Jungle/Turret 은 분리 | 전 섹션 에디터 지원 |
| STAGE_VERSION 수동 bump + 기존 파일 수동 삭제 | Stage 11 VersionMigrator 로 자동 |

---

## 2. 파일 레이아웃

```
[ WintersFileHeader 16B ]  flags=LZ4(옵션)
[ Payload ]
    MapMetaHeader              (64 B)
    StructureEntry[]           (기존 POD 유지, count 선두)
    JungleEntry[]
    MinionWaypointEntry[]      (v4 추가됨)
    ObstacleEntry[]            (NavGrid 차단 영역 — 신규)
    BushEntry[]                (덤불 — 신규)
    WardSlotEntry[]            (제어 와드 거점 — 신규)
    NavGridBlock               (비트맵 + origin + cell size)
    NavCellEntry[] (옵션)      (NavMesh cell 삼각화 — Phase C-4)
    SpawnPointEntry[]          (챔피언/봇 리스폰 — 신규)
[ SHA256 32B ]
```

### 2.1 POD

```cpp
// Engine/Public/AssetFormat/Map/WMapFormat.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <cstdint>

namespace Winters::Asset
{
    constexpr char WMAP_MAGIC[4] = { 'W','M','A','P' };

    // ---- Enum 재노출 (기존 Winters::Map:: 동일 값) ----
    enum eStructureKind : uint32_t
    {
        STRUCT_Nexus        = 0,
        STRUCT_Inhibitor    = 1,
        STRUCT_Turret       = 2,
    };
    enum eJungleKind : uint32_t
    {
        JUNGLE_Epic         = 0,
        JUNGLE_Normal       = 1,
    };
    enum eTeamMap : uint32_t
    {
        MAP_TEAM_Blue       = 0,
        MAP_TEAM_Red        = 1,
        MAP_TEAM_Neutral    = 2,
    };
    enum eLaneMap : uint32_t
    {
        LANE_Top  = 0, LANE_Mid = 1, LANE_Bot = 2, LANE_Base = 3, LANE_None = 255,
    };
    enum eTurretTierMap : uint32_t
    {
        TIER_Outer = 0, TIER_Inner = 1, TIER_Inhib = 2, TIER_Nexus = 3, TIER_None = 255,
    };

    #pragma pack(push, 1)
    struct MapMetaHeader
    {
        char     magic[4];               // "WMAP"
        uint32_t scene_id;                // Stage1, Stage2 (ARAM, etc.)
        uint32_t structure_count;
        uint32_t jungle_count;
        uint32_t waypoint_count;
        uint32_t obstacle_count;
        uint32_t bush_count;
        uint32_t ward_slot_count;
        uint32_t spawn_count;
        uint32_t navcell_count;          // 0 = NavGrid 만
        float    map_origin_x;           // 좌하단 월드 좌표
        float    map_origin_z;
        float    map_size_x;             // 맵 실제 크기 (m)
        float    map_size_z;
        uint32_t nav_grid_cell_size_bits;// 1 << n = 셀 크기 (m 단위 power of 2)
        uint32_t reserved[4];
    };
    static_assert(sizeof(MapMetaHeader) == 76);

    // 기존 POD 재사용 (Winters::Map::StructureEntry 와 동일 레이아웃)
    struct StructureEntry
    {
        char  name[64];
        uint32_t sub_kind;     // eStructureKind
        uint32_t team;         // eTeamMap
        uint32_t tier;         // eTurretTierMap
        uint32_t lane;         // eLaneMap
        float px, py, pz;
        float rx, ry, rz;
        float scale;
        uint32_t visible;
    };
    static_assert(sizeof(StructureEntry) == 64 + 12 * 4);

    struct JungleEntry
    {
        char  name[64];
        uint32_t sub_kind;     // eJungleKind
        uint32_t camp_id;
        float px, py, pz;
        float rx, ry, rz;
        float scale;
        uint32_t visible;
    };
    static_assert(sizeof(JungleEntry) == 64 + 10 * 4);

    struct MinionWaypointEntry
    {
        uint32_t team;
        uint32_t lane;
        uint32_t order;
        float px, py, pz;
        uint32_t reserved;
    };
    static_assert(sizeof(MinionWaypointEntry) == 4 * 4 + 3 * 4);

    // 신규 섹션들

    struct ObstacleEntry
    {
        char  name[32];
        uint32_t shape;        // 0=Box, 1=Sphere, 2=Polygon
        float px, py, pz;
        float sx, sy, sz;       // Box 반경 / Sphere radius / Polygon 점 수 (uint 재해석)
        uint32_t polygon_offset;// Polygon 모드일 때만 의미 — 글로벌 vertex 풀 인덱스
    };
    static_assert(sizeof(ObstacleEntry) == 32 + 9 * 4);

    struct BushEntry
    {
        char  name[32];
        uint32_t team;          // Neutral 일반
        float px, py, pz;
        float rx, ry, rz;       // OOB 회전
        float sx, sz;           // 직사각형 크기 (y 는 높이 고정 2 m)
        uint32_t reserved;
    };
    static_assert(sizeof(BushEntry) == 32 + 10 * 4);

    struct WardSlotEntry
    {
        uint32_t slot_id;
        float px, py, pz;
        uint32_t kind;          // 0=BushEye, 1=PixelBrush 등
    };
    static_assert(sizeof(WardSlotEntry) == 5 * 4);

    struct SpawnPointEntry
    {
        uint32_t team;
        uint32_t role;          // 0=Top, 1=Jungle, 2=Mid, 3=ADC, 4=Support
        float px, py, pz;
        float face_yaw;         // 리스폰 방향
    };
    static_assert(sizeof(SpawnPointEntry) == 5 * 4 + 4);

    struct NavGridBlockHeader
    {
        float origin_x, origin_z;    // 좌하단
        uint32_t cells_x;
        uint32_t cells_z;
        float cell_size_m;           // 0.125 / 0.25 / 0.5 보통
        uint32_t byte_count;         // (cells_x * cells_z + 7) / 8
        uint32_t reserved[2];
    };
    static_assert(sizeof(NavGridBlockHeader) == 32);

    // Phase C-4 NavMesh 지원용 — 삼각화 된 통로 cell
    struct NavCellEntry
    {
        float v0[3], v1[3], v2[3];   // world-space 삼각형
        int32_t neighbors[3];        // 인접 셀 인덱스 (-1 = edge)
        uint8_t region_id;           // 섬 구분 (연결성)
        uint8_t flags;               // 물/늪/덤불 등 기하 플래그
        uint16_t reserved;
    };
    static_assert(sizeof(NavCellEntry) == 9 * 4 + 3 * 4 + 4);
    #pragma pack(pop)
}
```

### 2.2 Scene_ID 값

| scene_id | 맵 |
|---|---|
| 1 | 소환사의 협곡 (Summoner's Rift — LoL 메인) |
| 2 | 하울링 애비스 (ARAM — 선택) |
| 10 | Custom 연습모드 (에디터) |
| 100+ | 엘든링 맵 (별도 번호대) |

---

## 3. 기존 Stage1.dat 와 대응

| 기존 `.dat` 섹션 | `.wmap` 대응 |
|---|---|
| `StageHeader { magic, version, reserved[6] }` | `MapMetaHeader` (더 많은 메타 포함) |
| `u32 count + StructureEntry[]` | `structure_count` + `StructureEntry[]` |
| `u32 count + JungleEntry[]` | `jungle_count` + `JungleEntry[]` |
| `u32 count + MinionWaypointEntry[]` (v4) | `waypoint_count` + `MinionWaypointEntry[]` |
| (없음) | `obstacle_count`, `bush_count`, `ward_slot_count`, `spawn_count`, `NavGridBlock`, `NavCellEntry[]` |

바이트 레이아웃 **동일 POD 유지** → 변환 스크립트는 바이트 복사 + 새 메타 필드 0 채우기만.

---

## 4. 컨버터 / 마이그레이션

### 4.1 Stage1.dat → Stage1.wmap

```cpp
// Engine/Private/AssetFormat/Map/WMapWriter.cpp
HRESULT CWMapWriter::MigrateFromStage1Dat(const wchar_t* pDatPath,
                                            const wchar_t* pWmapPath,
                                            const MapMigrationOptions& opt)
{
    // 1. 기존 Stage1.dat 읽기 (기존 MapDataIO 재사용)
    Winters::Map::StageHeader legacyHdr;
    std::vector<Winters::Map::StructureEntry> structs;
    std::vector<Winters::Map::JungleEntry>    jungles;
    std::vector<Winters::Map::MinionWaypointEntry> wps;

    if (FAILED(Winters::Map::Load(pDatPath, legacyHdr, structs, jungles, wps)))
        return E_FAIL;

    // 2. NavGrid 는 별도 .bin 또는 런타임 계산
    auto navGrid = opt.loadNavGrid(pDatPath);   // 콜백 — Scene_InGame 에서 구현

    // 3. 새 MapMetaHeader
    MapMetaHeader hdr{};
    std::memcpy(hdr.magic, WMAP_MAGIC, 4);
    hdr.scene_id        = 1;
    hdr.structure_count = (uint32_t)structs.size();
    hdr.jungle_count    = (uint32_t)jungles.size();
    hdr.waypoint_count  = (uint32_t)wps.size();
    hdr.obstacle_count  = 0;            // 초기 이관 단계 — 이후 에디터로 추가
    hdr.bush_count      = 0;
    hdr.ward_slot_count = 0;
    hdr.spawn_count     = 0;
    hdr.navcell_count   = 0;
    hdr.map_origin_x    = navGrid.origin_x;
    hdr.map_origin_z    = navGrid.origin_z;
    hdr.map_size_x      = navGrid.cells_x * navGrid.cell_size_m;
    hdr.map_size_z      = navGrid.cells_z * navGrid.cell_size_m;
    hdr.nav_grid_cell_size_bits = NavCellSizeBits(navGrid.cell_size_m);

    // 4. 직렬화 — 바이트 레이아웃 동일하므로 memcpy
    CBinaryWriter w;
    w.Write(hdr);
    for (const auto& s : structs) {
        StructureEntry entry;
        std::memcpy(&entry, &s, sizeof(entry));
        w.Write(entry);
    }
    for (const auto& j : jungles) {
        JungleEntry entry;
        std::memcpy(&entry, &j, sizeof(entry));
        w.Write(entry);
    }
    for (const auto& wp : wps) {
        MinionWaypointEntry entry;
        std::memcpy(&entry, &wp, sizeof(entry));
        w.Write(entry);
    }

    // NavGrid 블록
    NavGridBlockHeader nhdr{};
    nhdr.origin_x    = navGrid.origin_x;
    nhdr.origin_z    = navGrid.origin_z;
    nhdr.cells_x     = navGrid.cells_x;
    nhdr.cells_z     = navGrid.cells_z;
    nhdr.cell_size_m = navGrid.cell_size_m;
    nhdr.byte_count  = navGrid.byte_count;
    w.Write(nhdr);
    w.WriteBytes(navGrid.bits.data(), navGrid.bits.size());

    return w.SaveToFile(pWmapPath, WINTERS_FLAG_LZ4);
}
```

---

## 5. 런타임 로더

```cpp
// Engine/Public/AssetFormat/Map/WMapLoader.h
namespace Winters::Asset
{
    struct WMapData
    {
        MapMetaHeader                    meta{};
        std::vector<StructureEntry>      structures;
        std::vector<JungleEntry>         jungles;
        std::vector<MinionWaypointEntry> waypoints;
        std::vector<ObstacleEntry>       obstacles;
        std::vector<BushEntry>           bushes;
        std::vector<WardSlotEntry>       wards;
        std::vector<SpawnPointEntry>     spawns;
        NavGridBlockHeader               navGridHdr{};
        std::vector<uint8_t>             navGridBits;
        std::vector<NavCellEntry>        navCells;
    };

    class WINTERS_API CWMapLoader
    {
    public:
        static HRESULT Load(const std::wstring& path, WMapData& out);
    };
}
```

---

## 6. Scene_InGame / Scene_Editor 통합

### 6.1 로드 경로

```cpp
// Client/Private/Scene/Scene_InGame.cpp
HRESULT Scene_InGame::LoadMapData()
{
    // 1. 새 포맷 우선 탐색
    auto wmapPath = Winters::Path::ResolveContent(L"Data/Stage1.wmap");
    if (FileExists(wmapPath)) {
        WMapData data;
        if (SUCCEEDED(CWMapLoader::Load(wmapPath, data))) {
            ApplyMapData(data);
            return S_OK;
        }
    }

    // 2. 레거시 .dat 폴백 (개발 빌드)
#ifdef _DEBUG
    return LoadStage1Dat();
#else
    return E_FAIL;
#endif
}
```

### 6.2 Scene_Editor 저장

```cpp
// Client/Private/Scene/Scene_Editor.cpp
void Scene_Editor::OnSave()
{
    WMapData data = CollectFromManagers(
        CStructure_Manager::Get(),
        CJungle_Manager::Get(),
        CMinionWaypoint_Manager::Get(),
        CNavigation_Manager::Get());

    CWMapWriter::Save(L"Data/Stage1.wmap", data);

    // 레거시 .dat 도 병행 저장 (개발 빌드 — 점진 이관)
#ifdef _DEBUG
    Winters::Map::Save(L"Data/Stage1.dat", legacyHdr,
                        data.structures, data.jungles, data.waypoints);
#endif
}
```

---

## 7. NavGrid 통합 (Phase 3-B 와 연동)

현재 Phase 3-B (NavMesh) 계획 — `.wmap` 의 NavGrid 섹션이 **단일 진실의 원천**:

```cpp
// Engine/Public/AI/Pathfinding/NavGrid.h
class CNavGrid
{
public:
    static std::unique_ptr<CNavGrid> FromWMapBlock(const NavGridBlockHeader& hdr,
                                                     const uint8_t* pBits);

    bool_t IsBlocked(float wx, float wz) const;
    bool_t IsBlockedCell(uint32_t cx, uint32_t cz) const;
    void   SetBlocked(float wx, float wz, bool_t b);

    // Structure 배치 / 제거 시 호출 → bit 패턴 갱신
    void   ApplyStructure(const StructureEntry& e, bool_t bPlacing);

private:
    NavGridBlockHeader m_hdr;
    std::vector<uint8_t> m_bits;
};
```

---

## 8. 에디터 UI 통합

Scene_Editor 에 패널 추가:

```
Hierarchy (좌)          Inspector (우)
├─ Structure/            [선택 오브젝트 파라미터]
│   └─ Turret_T1_Mid     ───────────────────────
├─ Jungle/               NavGrid:
│   └─ DragonPit         [x] Draw overlay
├─ Waypoint/             [x] Show blocked cells
│   └─ Top_Minion_3      [ ] Rebuild NavCells
├─ Obstacle/             ───────────────────────
├─ Bush/                 Ward Slot:
├─ Ward Slot/            Kind: [Bush Eye ▼]
├─ Spawn Point/          Range: 900 m
└─ NavCell (read-only)
```

Ctrl+S 로 `.wmap` 저장. 기존 Ctrl+S 로 `.dat` 저장은 개발 빌드에서 병행.

---

## 9. 성능

| 작업 | 기존 .dat | `.wmap` 목표 |
|---|---|---|
| 로드 | 2 ms | < 1.5 ms (LZ4 해제 포함) |
| NavGrid 로드 | 별도 1 ms | 통합 (0 추가) |
| 저장 | 3 ms | < 2 ms |

---

## 10. 보안 고려사항

| 위협 | 방어 |
|---|---|
| 치트가 NavGrid bit 조작 (벽 통과) | SHA256 + 서버 권위 (서버는 자기 NavGrid 사용) |
| Structure 좌표 조작 (포탑 위치 옮겨 자기 팀 유리) | 서버 권위 — 서버가 내려준 배치 기준으로 판정 |
| Ward Slot / Bush 위치 조작 (시야 확장) | 서버에서 판정 위치 송신 |
| count 거대값 (OOM) | 상한 검증 (`MAX_STRUCTURES = 512`, `MAX_BUSHES = 256`, etc.) |

### 10.1 Validator

```cpp
HRESULT ValidateMapMeta(const MapMetaHeader& hdr)
{
    constexpr uint32_t MAX_STRUCT = 512, MAX_JUNGLE = 64, MAX_WP = 512;
    constexpr uint32_t MAX_OBS = 256, MAX_BUSH = 256, MAX_WARD = 64, MAX_SPAWN = 32;
    constexpr uint32_t MAX_NAVCELL = 10'000'000;

    if (hdr.structure_count > MAX_STRUCT)   return E_WINTERS_SIZE_OVERFLOW;
    if (hdr.jungle_count > MAX_JUNGLE)       return E_WINTERS_SIZE_OVERFLOW;
    if (hdr.waypoint_count > MAX_WP)         return E_WINTERS_SIZE_OVERFLOW;
    if (hdr.obstacle_count > MAX_OBS)        return E_WINTERS_SIZE_OVERFLOW;
    if (hdr.bush_count > MAX_BUSH)           return E_WINTERS_SIZE_OVERFLOW;
    if (hdr.ward_slot_count > MAX_WARD)      return E_WINTERS_SIZE_OVERFLOW;
    if (hdr.spawn_count > MAX_SPAWN)         return E_WINTERS_SIZE_OVERFLOW;
    if (hdr.navcell_count > MAX_NAVCELL)     return E_WINTERS_SIZE_OVERFLOW;

    // 맵 크기 sanity check
    if (hdr.map_size_x <= 0 || hdr.map_size_x > 100'000) return E_WINTERS_SIZE_OVERFLOW;
    if (hdr.map_size_z <= 0 || hdr.map_size_z > 100'000) return E_WINTERS_SIZE_OVERFLOW;

    return S_OK;
}
```

---

## 11. 서버 연계 (Phase 4 네트워크)

서버가 매치 시작 시 클라이언트에 `.wmap` 파일 자체가 아닌 **scene_id + map_hash** 전송:
- 클라이언트는 로컬 `.wmap` 로드 → SHA256 검증
- 해시 일치 시 게임 시작
- 해시 불일치 시 재다운로드 (Stage 8 번들에서)

```cpp
// Client/Public/Network/MatchJoinPacket.h
struct MatchStartPacket
{
    uint32_t scene_id;
    uint8_t  map_sha256[32];
    // ...
};
```

---

## 12. 완료 기준

- [ ] `WMapFormat.h` POD + static_assert
- [ ] `WMapWriter` 마이그레이션 `.dat → .wmap`
- [ ] `WMapLoader` 통합 로드
- [ ] Scene_InGame 새 포맷 우선 로드
- [ ] Scene_Editor Ctrl+S 로 `.wmap` 저장 (+ 레거시 .dat 병행)
- [ ] 에디터 UI 확장 (Obstacle/Bush/Ward/Spawn 탭)
- [ ] NavGrid 한 파일 통합 확인
- [ ] Validator 상한 테스트
- [ ] Phase 3-B NavMesh 통합 경로 검증

---

## 13. 다음 단계

Stage 7 (`.winters` 번들) 로 이동 — 여러 에셋 묶음 + TOC + 서명.
