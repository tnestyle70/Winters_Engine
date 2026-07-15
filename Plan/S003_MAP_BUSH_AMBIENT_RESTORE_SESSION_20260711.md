Session - Map11의 부쉬 64개와 곤충 앰비언트를 canonical Stage 좌표로 복구하고 normal F5 렌더 경로에 연결한다.

RETIRED - 2026-07-11 S004에서 crossed-card foliage 결과를 시각 실패로 판정했다. 이 문서는 실패한 시도의 기록이며 normal F5 활성 계획이 아니다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Tools/build_map11_bush_cluster.py

새 파일:

```python
import json
import math
import struct
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
REBUILD = ROOT / "Client/Bin/Resource/Texture/MAP/Map11_Rebuild"
SOURCE_GLB = REBUILD / "glb/sru_wolfden_blueside_windgrass_01.glb"
SOURCE_DIR = REBUILD / "source/bush"
OBJ_OUT = SOURCE_DIR / "map11_bush_cluster.obj"
MTL_OUT = SOURCE_DIR / "map11_bush_cluster.mtl"
MESH_OUT = REBUILD / "cooked/map11_bush_cluster.wmesh"
CONVERTER = ROOT / "Tools/Bin/Debug/WintersAssetConverter.exe"
BRUSH_TEXTURE_FROM_SOURCE = (
    "../../../output/textures/assets/maps/kitpieces/srx/textures/sru_brush.png"
)


def load_glb(path: Path):
    payload = path.read_bytes()
    if len(payload) < 20 or payload[:4] != b"glTF":
        raise SystemExit(f"invalid GLB: {path}")
    version, declared_size = struct.unpack_from("<II", payload, 4)
    if version != 2 or declared_size != len(payload):
        raise SystemExit(f"unsupported GLB header: version={version} size={declared_size}")

    json_doc = None
    binary = None
    offset = 12
    while offset + 8 <= len(payload):
        chunk_size, chunk_type = struct.unpack_from("<II", payload, offset)
        offset += 8
        chunk = payload[offset:offset + chunk_size]
        offset += chunk_size
        if chunk_type == 0x4E4F534A:
            json_doc = json.loads(chunk.rstrip(b"\x00 ").decode("utf-8"))
        elif chunk_type == 0x004E4942:
            binary = chunk

    if json_doc is None or binary is None:
        raise SystemExit(f"GLB JSON/BIN chunk missing: {path}")
    return json_doc, binary


def read_accessor(doc, binary, accessor_index):
    accessor = doc["accessors"][accessor_index]
    view = doc["bufferViews"][accessor["bufferView"]]
    component_formats = {
        5121: "B",
        5123: "H",
        5125: "I",
        5126: "f",
    }
    component_counts = {
        "SCALAR": 1,
        "VEC2": 2,
        "VEC3": 3,
        "VEC4": 4,
    }
    component_type = accessor["componentType"]
    if component_type not in component_formats or accessor["type"] not in component_counts:
        raise SystemExit(f"unsupported accessor: {accessor}")

    fmt_char = component_formats[component_type]
    component_count = component_counts[accessor["type"]]
    fmt = "<" + fmt_char * component_count
    item_size = struct.calcsize(fmt)
    stride = view.get("byteStride", item_size)
    base = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)

    values = []
    for index in range(accessor["count"]):
        values.append(struct.unpack_from(fmt, binary, base + index * stride))
    return values


def main():
    doc, binary = load_glb(SOURCE_GLB)
    primitive = doc["meshes"][0]["primitives"][0]
    positions = read_accessor(doc, binary, primitive["attributes"]["POSITION"])
    texcoords = read_accessor(doc, binary, primitive["attributes"]["TEXCOORD_0"])
    indices = [int(value[0]) for value in read_accessor(doc, binary, primitive["indices"])]
    if len(positions) != len(texcoords) or len(indices) % 3 != 0:
        raise SystemExit("windgrass source topology mismatch")

    min_x = min(position[0] for position in positions)
    max_x = max(position[0] for position in positions)
    min_z = min(position[2] for position in positions)
    center_x = (min_x + max_x) * 0.5
    source_to_world = 0.035
    card_angles = (0.0, math.pi / 3.0, math.pi * 2.0 / 3.0)

    SOURCE_DIR.mkdir(parents=True, exist_ok=True)
    MTL_OUT.write_text(
        "newmtl Map11BushFoliage\n"
        "Kd 1.0 1.0 1.0\n"
        f"map_Kd {BRUSH_TEXTURE_FROM_SOURCE}\n",
        encoding="utf-8",
    )

    lines = [
        f"mtllib {MTL_OUT.name}",
        "o Map11BushCluster",
        "usemtl Map11BushFoliage",
    ]
    vertex_base = 1
    for angle in card_angles:
        cosine = math.cos(angle)
        sine = math.sin(angle)
        for x, _, z in positions:
            local_x = (x - center_x) * source_to_world
            local_y = (z - min_z) * source_to_world
            world_x = local_x * cosine
            world_z = -local_x * sine
            lines.append(f"v {world_x:.7f} {local_y:.7f} {world_z:.7f}")
        for u, v in texcoords:
            lines.append(f"vt {u:.7f} {v:.7f}")

        for tri in range(0, len(indices), 3):
            a = vertex_base + indices[tri]
            b = vertex_base + indices[tri + 1]
            c = vertex_base + indices[tri + 2]
            lines.append(f"f {a}/{a} {b}/{b} {c}/{c}")
            lines.append(f"f {c}/{c} {b}/{b} {a}/{a}")
        vertex_base += len(positions)

    OBJ_OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    if not CONVERTER.is_file():
        raise SystemExit(f"asset converter missing: {CONVERTER}")

    subprocess.run(
        [str(CONVERTER), "mesh", str(OBJ_OUT), "-o", str(MESH_OUT)],
        check=True,
    )
    subprocess.run([str(CONVERTER), "info", str(MESH_OUT)], check=True)
    subprocess.run([str(CONVERTER), "info", str(MESH_OUT.with_suffix(".wmat"))], check=True)
    print(f"built Map11 bush cluster -> {MESH_OUT}")


if __name__ == "__main__":
    main()
```

1-2. C:/Users/user/Desktop/Winters/Tools/cook_map11_brush_volumes.py

기존 코드:

```python
# map11_brush_volumes.csv -> map11_brush_volumes.wbrush (v1) 쿡 스크립트.
# 포맷: u32 magic 'WBSH' | u32 version=1 | u32 count | u32 reserved
#       entry[count]: u32 bushId | f32 worldX | f32 worldZ | f32 radius
# TODO(wmap): Stage 데이터 .wmap 통합 시 BushEntry(07_STAGE6_WMAP.md)로 승격.
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CSV = ROOT / "Client/Bin/Resource/Texture/MAP/Map11_Rebuild/manifest/map11_brush_volumes.csv"
OUT = ROOT / "Client/Bin/Resource/Texture/MAP/Map11_Rebuild/cooked/map11_brush_volumes.wbrush"

MAGIC = 0x48534257  # 'WBSH'
VERSION = 1

entries = []
for line in CSV.read_text(encoding="utf-8").splitlines():
    line = line.strip()
    if not line or line.startswith("#"):
        continue
    parts = [p.strip() for p in line.split(",")]
    if len(parts) != 4:
        raise SystemExit(f"bad row: {line!r}")
    bush_id = int(parts[0])
    x, z, radius = (float(parts[1]), float(parts[2]), float(parts[3]))
    if radius <= 0.0:
        raise SystemExit(f"bad radius: {line!r}")
    entries.append((bush_id, x, z, radius))

payload = struct.pack("<IIII", MAGIC, VERSION, len(entries), 0)
for bush_id, x, z, radius in entries:
    payload += struct.pack("<Ifff", bush_id, x, z, radius)

OUT.write_bytes(payload)
print(f"cooked {len(entries)} brush volumes -> {OUT} ({len(payload)} bytes)")
```

아래로 교체:

```python
# Map11 centered brush authoring CSV -> canonical Stage-frame WBRUSH/Stage1 v5.
import argparse
import math
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CSV = ROOT / "Client/Bin/Resource/Texture/MAP/Map11_Rebuild/manifest/map11_brush_volumes.csv"
OUT = ROOT / "Client/Bin/Resource/Texture/MAP/Map11_Rebuild/cooked/map11_brush_volumes.wbrush"
STAGE = ROOT / "Data/Stage1.dat"

WBRUSH_MAGIC = 0x48534257  # 'WBSH'
WBRUSH_VERSION = 1
STAGE_MAGIC = 0x47545357  # 'WSTG'
STAGE_VERSION = 5
MAP11_STAGE_CENTER_X = 104.50
BUSH_MESH = "Texture/MAP/Map11_Rebuild/cooked/map11_bush_cluster.wmesh"

STRUCTURE_ENTRY_SIZE = 112
JUNGLE_ENTRY_SIZE = 104
WAYPOINT_ENTRY_SIZE = 28
BUSH_ENTRY_SIZE = 252


def read_entries():
    entries = []
    for line in CSV.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = [part.strip() for part in line.split(",")]
        if len(parts) != 4:
            raise SystemExit(f"bad row: {line!r}")
        bush_id = int(parts[0])
        local_x, local_z, radius = (float(parts[1]), float(parts[2]), float(parts[3]))
        if bush_id <= 0 or radius <= 0.0:
            raise SystemExit(f"bad brush record: {line!r}")
        entries.append((bush_id, local_x + MAP11_STAGE_CENTER_X, local_z, radius))
    if not entries:
        raise SystemExit("no Map11 brush records")
    return entries


def fixed_utf8(value, capacity):
    encoded = value.encode("utf-8")
    if len(encoded) >= capacity:
        raise SystemExit(f"fixed string overflow: {value!r}")
    return encoded + b"\0" * (capacity - len(encoded))


def build_bush_entry(index, record):
    bush_id, world_x, world_z, radius = record
    scale = max(0.75, radius / 4.0)
    yaw = (index % 6) * math.pi / 18.0
    payload = struct.pack(
        "<64sII8fI128s4I",
        fixed_utf8(f"Map11_Bush_{bush_id}_{index:02d}", 64),
        bush_id,
        1,  # eBushRenderKind::Mesh
        world_x,
        0.0,
        world_z,
        yaw,
        radius,
        radius * 2.0,
        3.6 * scale,
        scale,
        1,
        fixed_utf8(BUSH_MESH, 128),
        0,
        0,
        0,
        0,
    )
    if len(payload) != BUSH_ENTRY_SIZE:
        raise SystemExit(f"BushEntry ABI mismatch: {len(payload)}")
    return payload


def read_block_end(payload, offset, entry_size, label):
    if offset + 4 > len(payload):
        raise SystemExit(f"{label} count truncated")
    count = struct.unpack_from("<I", payload, offset)[0]
    end = offset + 4 + count * entry_size
    if end > len(payload):
        raise SystemExit(f"{label} block truncated: count={count}")
    return end, count


def migrate_stage(entries):
    payload = STAGE.read_bytes()
    if len(payload) < 32:
        raise SystemExit("Stage1 header truncated")
    magic, version = struct.unpack_from("<II", payload, 0)
    if magic != STAGE_MAGIC or version not in (4, 5):
        raise SystemExit(f"unsupported Stage1: magic=0x{magic:08X} version={version}")

    offset, structure_count = read_block_end(payload, 32, STRUCTURE_ENTRY_SIZE, "structure")
    offset, jungle_count = read_block_end(payload, offset, JUNGLE_ENTRY_SIZE, "jungle")
    offset, waypoint_count = read_block_end(payload, offset, WAYPOINT_ENTRY_SIZE, "waypoint")
    bush_offset = offset
    old_bush_count = 0
    if version == 5:
        offset, old_bush_count = read_block_end(payload, offset, BUSH_ENTRY_SIZE, "bush")
    if offset != len(payload):
        raise SystemExit(f"unexpected trailing Stage1 bytes: parsed={offset} file={len(payload)}")

    migrated = bytearray(payload[:bush_offset])
    struct.pack_into("<I", migrated, 4, STAGE_VERSION)
    migrated += struct.pack("<I", len(entries))
    for index, record in enumerate(entries):
        migrated += build_bush_entry(index, record)

    temporary = STAGE.with_suffix(".dat.tmp")
    temporary.write_bytes(migrated)
    temporary.replace(STAGE)
    print(
        f"migrated Stage1 v{version}->v5 S={structure_count} J={jungle_count} "
        f"W={waypoint_count} B={old_bush_count}->{len(entries)} bytes={len(migrated)}"
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--migrate-stage", action="store_true")
    args = parser.parse_args()

    entries = read_entries()
    payload = struct.pack("<IIII", WBRUSH_MAGIC, WBRUSH_VERSION, len(entries), 0)
    for bush_id, world_x, world_z, radius in entries:
        payload += struct.pack("<Ifff", bush_id, world_x, world_z, radius)
    OUT.write_bytes(payload)
    print(f"cooked {len(entries)} canonical brush volumes -> {OUT} ({len(payload)} bytes)")

    if args.migrate_stage:
        migrate_stage(entries)


if __name__ == "__main__":
    main()
```

1-3. C:/Users/user/Desktop/Winters/Client/Public/Manager/Bush_Manager.h

`class CBush_Manager final`의 public 영역에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    void    RenderEditorOverlay(const Mat4& matViewProj, i32_t selectedIndex) const;
```

아래에 추가:

```cpp
    void    Render(const Mat4& matViewProj, const Vec3& vCameraWorld,
        void* pAmbientOcclusionSRV = nullptr);
    u32_t   AppendRenderSnapshotMeshes(RenderWorldSnapshot& snapshot,
        const Mat4& matViewProj);
```

`BushRuntimeData`에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
        std::string assetPath;
```

아래에 추가:

```cpp
        std::unique_ptr<ModelRenderer> pMeshRenderer;
```

`class CBush_Manager final`의 private 영역에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    void Sync_FxComponent(u32_t iIndex);
```

아래에 추가:

```cpp
    void Sync_MeshRenderer(u32_t iIndex);
```

헤더 include 영역에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
#include "ECS/Entity.h"
```

아래에 추가:

```cpp
#include "Renderer/ModelRenderer.h"
#include "Renderer/RenderWorldSnapshot.h"
```

기존 코드:

```cpp
#include <cstdio>
```

아래에 추가:

```cpp
#include <memory>
```

1-4. C:/Users/user/Desktop/Winters/Client/Private/Manager/Bush_Manager.cpp

`CBush_Manager::RenderEditorOverlay` 바로 아래에 추가:

```cpp
void CBush_Manager::Render(const Mat4& matViewProj, const Vec3& vCameraWorld,
    void* pAmbientOcclusionSRV)
{
    if (!m_pWorld)
        return;

    for (u32_t i = 0; i < static_cast<u32_t>(m_vecEntities.size()); ++i)
    {
        if (i >= m_vecData.size())
            break;
        const EntityID entity = m_vecEntities[i];
        BushRuntimeData& data = m_vecData[i];
        if (!data.bVisible || !data.pMeshRenderer ||
            !m_pWorld->IsAlive(entity) ||
            !m_pWorld->HasComponent<TransformComponent>(entity))
        {
            continue;
        }

        TransformComponent& transform = m_pWorld->GetComponent<TransformComponent>(entity);
        data.pMeshRenderer->SetAmbientOcclusionSRV(pAmbientOcclusionSRV);
        data.pMeshRenderer->UpdateCamera(matViewProj, vCameraWorld);
        data.pMeshRenderer->UpdateTransform(transform.GetWorldMatrix());
        data.pMeshRenderer->RenderFrustumCulled(matViewProj);
    }
}

u32_t CBush_Manager::AppendRenderSnapshotMeshes(RenderWorldSnapshot& snapshot,
    const Mat4& matViewProj)
{
    if (!m_pWorld)
        return 0;

    u32_t appendedCount = 0;
    for (u32_t i = 0; i < static_cast<u32_t>(m_vecEntities.size()); ++i)
    {
        if (i >= m_vecData.size())
            break;
        const EntityID entity = m_vecEntities[i];
        BushRuntimeData& data = m_vecData[i];
        if (!data.bVisible || !data.pMeshRenderer ||
            !m_pWorld->IsAlive(entity) ||
            !m_pWorld->HasComponent<TransformComponent>(entity))
        {
            continue;
        }

        TransformComponent& transform = m_pWorld->GetComponent<TransformComponent>(entity);
        data.pMeshRenderer->UpdateTransform(transform.GetWorldMatrix());
        appendedCount += data.pMeshRenderer->AppendRenderSnapshotMeshesFrustumCulled(
            snapshot,
            matViewProj);
    }
    return appendedCount;
}
```

`CBush_Manager::Set_RenderKind`, `Set_AssetPath`, `Set_Visible`에서 값을 변경한 뒤 아래 호출을 추가:

```cpp
    Sync_MeshRenderer(iIndex);
```

`CBush_Manager::Set_VisualSize`의 기존 코드:

```cpp
    m_vecData[iIndex].scale = (std::max)(0.01f, scale);
    Sync_FxComponent(iIndex);
```

아래로 교체:

```cpp
    m_vecData[iIndex].scale = (std::max)(0.01f, scale);
    const EntityID entity = m_vecEntities[iIndex];
    if (m_pWorld && m_pWorld->HasComponent<TransformComponent>(entity))
        m_pWorld->GetComponent<TransformComponent>(entity).SetScale(m_vecData[iIndex].scale);
    Sync_FxComponent(iIndex);
    Sync_MeshRenderer(iIndex);
```

`CBush_Manager::Spawn_FromEntry`의 기존 코드:

```cpp
    m_vecEntities.push_back(entity);
    m_vecData.push_back(std::move(data));
    Sync_FxComponent(static_cast<u32_t>(m_vecEntities.size() - 1));
    return entity;
```

아래로 교체:

```cpp
    m_vecEntities.push_back(entity);
    m_vecData.push_back(std::move(data));
    const u32_t index = static_cast<u32_t>(m_vecEntities.size() - 1);
    Sync_FxComponent(index);
    Sync_MeshRenderer(index);
    return entity;
```

`CBush_Manager::Sync_FxComponent` 바로 아래에 추가:

```cpp
void CBush_Manager::Sync_MeshRenderer(u32_t iIndex)
{
    if (!m_pWorld || iIndex >= m_vecEntities.size() || iIndex >= m_vecData.size())
        return;

    BushRuntimeData& data = m_vecData[iIndex];
    const bool_t bNeedsMesh =
        data.renderKind == Winters::Map::eBushRenderKind::Mesh &&
        data.bVisible &&
        !data.assetPath.empty();
    if (!bNeedsMesh)
    {
        data.pMeshRenderer.reset();
        return;
    }
    if (data.pMeshRenderer)
        return;

    auto renderer = std::make_unique<ModelRenderer>();
    if (!renderer->Initialize(data.assetPath, L"Shaders/Mesh3D.hlsl"))
        return;
    data.pMeshRenderer = std::move(renderer);
}
```

1-5. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameRender.cpp

RHI scene snapshot 조립에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
        appendedCount += CMinion_Manager::Get()->AppendRenderSnapshotMeshes(
            snapshot,
            vp,
            bRevealAllForPlayback);
```

아래에 추가:

```cpp
        appendedCount += CBush_Manager::Get()->AppendRenderSnapshotMeshes(snapshot, vp);
```

legacy scene object 렌더에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
        CMinion_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV, bRevealAllForPlayback);
```

아래에 추가:

```cpp
        CBush_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV);
```

1-6. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_Editor.cpp

`CScene_Editor::OnRender`의 기존 코드:

```cpp
    CStructure_Manager::Get()->Render(matVP);
    CJungle_Manager::Get()->Render(matVP);
    CBush_Manager::Get()->RenderEditorOverlay(
```

아래로 교체:

```cpp
    CStructure_Manager::Get()->Render(matVP);
    CJungle_Manager::Get()->Render(matVP);
    CBush_Manager::Get()->Render(matVP, m_pCamera->GetEye());
    CBush_Manager::Get()->RenderEditorOverlay(
```

1-7. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameLifecycle.cpp

삭제할 범위:
`void SeedPracticeBushesForBootstrap(CWorld& world)` 줄부터
`bool_t SeedMap11BrushesFromResourceForBootstrap(CWorld& world)` 함수 끝까지 삭제한다.
`void SendServerInGameReady()`는 남긴다.

`InitializeMapSurfaceSampler` 호출 바로 아래에 추가:

기존 코드:

```cpp
    InitializeMapSurfaceSampler(bMapInit, SelectMapSurfacePath());
```

아래에 추가:

```cpp
    for (u32_t i = 0; i < CBush_Manager::Get()->Get_Count(); ++i)
    {
        Vec3 position = CBush_Manager::Get()->Get_Position(i);
        if (TryProjectToMapSurface(position, 0.02f))
            CBush_Manager::Get()->Set_Position(i, position);
    }
```

앰비언트 Spawn 호출의 기존 코드:

```cpp
    CAmbientProp_Manager::Get()->Spawn(
        m_MapTransform.GetWorldMatrix(),
        m_vMapRotation.y,
        [this](Vec3& pos) { (void)TryProjectToMapSurface(pos, 0.02f); });
```

아래로 교체:

```cpp
    CAmbientProp_Manager::Get()->Spawn(
        [this](Vec3& pos) { (void)TryProjectToMapSurface(pos, 0.02f); });
```

1-8. C:/Users/user/Desktop/Winters/Engine/Private/ECS/ConcealmentVolumeIndex.cpp

`CConcealmentVolumeIndex::QueryVolumeAt`의 기존 코드:

```cpp
        if (dx * dx + dz * dz <= volume.radius * volume.radius)
            return volume.ID;
```

아래로 교체:

```cpp
        if (dx * dx + dz * dz <= volume.radius * volume.radius)
        {
            return volume.volumeId != 0u
                ? static_cast<EntityID>(volume.volumeId)
                : volume.ID;
        }
```

1-9. C:/Users/user/Desktop/Winters/Tools/cook_map11_ambient_props.py

기존 코드:

```python
KINDS = {"levelprop_sru_bird": 0, "levelprop_sru_duck": 1}

out = subprocess.run(
    [str(PROBE), "levelprops", str(MATERIALS_BIN), "sru_bird,sru_duck"],
    capture_output=True, text=True, check=True).stdout
```

아래로 교체:

```python
KINDS = {
    "levelprop_sru_bird": 0,
    "levelprop_sru_duck": 1,
    "audio-emitter_sru_insects": 2,
}

out = subprocess.run(
    [str(PROBE), "levelprops", str(MATERIALS_BIN), "sru_bird,sru_duck,sru_insects"],
    capture_output=True, text=True, check=True).stdout
```

기존 코드:

```python
    "# schema: name,kind(0=bird/1=duck),lolX,lolY,lolZ,lolYawRad,scale\n"
```

아래로 교체:

```python
    "# schema: name,kind(0=bird/1=duck/2=firefly),lolX,lolY,lolZ,lolYawRad,scale\n"
```

기존 코드:

```python
for name in ("sru_bird", "sru_duck"):
```

아래로 교체:

```python
for name in ("sru_bird", "sru_duck", "chemtech_firefly_animated"):
```

1-10. C:/Users/user/Desktop/Winters/Data/LoL/ClientPublic/Visual/ObjectVisualDefs.json

`ambientProps.props`에서 duck 정의 바로 아래에 추가:

```json
      {
        "key": "ambient.chemtech_firefly",
        "kind": 2,
        "mesh": "Texture/MAP/Map11_Rebuild/cooked/ambient/chemtech_firefly_animated/chemtech_firefly_animated.wmesh",
        "shader": "Shaders/Mesh3D.hlsl",
        "idleAnimation": "firefly_fairy_idle"
      }
```

1-11. C:/Users/user/Desktop/Winters/Client/Public/Manager/AmbientProp_Manager.h

기존 코드:

```cpp
    void Spawn(const Mat4& mapWorld, f32_t mapYaw,
        const std::function<void(Vec3&)>& projectToSurface);
```

아래로 교체:

```cpp
    void Spawn(const std::function<void(Vec3&)>& projectToSurface);
```

1-12. C:/Users/user/Desktop/Winters/Client/Private/Manager/AmbientProp_Manager.cpp

anonymous namespace의 `MapAmbientPropRecord` 바로 아래에 추가:

```cpp
    constexpr f32_t kLolMap11ToStage = 0.01f * 0.70710678118f;
    constexpr u32_t kFireflyKind = 2u;

    Vec3 ConvertMap11LoLToStage(const MapAmbientPropRecord& record)
    {
        return {
            (record.lolX + record.lolZ) * kLolMap11ToStage,
            record.lolY * 0.01f,
            (record.lolX - record.lolZ) * kLolMap11ToStage
        };
    }

    f32_t ConvertMap11LoLYawToStage(f32_t lolYaw)
    {
        const f32_t rawX = std::sin(lolYaw);
        const f32_t rawZ = std::cos(lolYaw);
        const f32_t stageX = rawX + rawZ;
        const f32_t stageZ = rawX - rawZ;
        return std::atan2(stageX, stageZ);
    }
```

기존 함수 시그니처와 map world 로드 코드:

```cpp
void CAmbientProp_Manager::Spawn(const Mat4& mapWorld, f32_t mapYaw,
    const std::function<void(Vec3&)>& projectToSurface)
```

아래로 교체:

```cpp
void CAmbientProp_Manager::Spawn(
    const std::function<void(Vec3&)>& projectToSurface)
```

삭제할 코드:

```cpp
    const DirectX::XMMATRIX xmMapWorld =
        DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&mapWorld.m));
```

기존 좌표/회전/스케일 코드:

```cpp
        const DirectX::XMFLOAT3 lolPos{ record.lolX, record.lolY, record.lolZ };
        const DirectX::XMVECTOR vWorld = DirectX::XMVector3TransformCoord(
            DirectX::XMLoadFloat3(&lolPos), xmMapWorld);
        Vec3 worldPos{
            DirectX::XMVectorGetX(vWorld),
            DirectX::XMVectorGetY(vWorld),
            DirectX::XMVectorGetZ(vWorld)
        };
        if (projectToSurface)
            projectToSurface(worldPos);

        Prop prop{};
        prop.pRenderer = std::move(pRenderer);
        prop.transform.SetPosition(worldPos);
        // X-flip 맵이라 LoL yaw는 부호가 반전된 채 맵 회전에 더해진다.
        prop.transform.SetRotation({ 0.f, mapYaw - record.lolYaw, 0.f });
        const f32_t fScale = 0.01f * record.scale;
```

아래로 교체:

```cpp
        Vec3 worldPos = ConvertMap11LoLToStage(record);
        if (projectToSurface)
            projectToSurface(worldPos);
        if (record.kind == kFireflyKind)
        {
            worldPos.y += 1.4f;
            pRenderer->SetMaterialOverrideColor({ 0.52f, 0.95f, 0.20f, 1.f }, true);
        }

        Prop prop{};
        prop.pRenderer = std::move(pRenderer);
        prop.transform.SetPosition(worldPos);
        prop.transform.SetRotation({ 0.f, ConvertMap11LoLYawToStage(record.lolYaw), 0.f });
        const f32_t fScale = 0.01f * record.scale;
```

include 영역에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
#include <cstdio>
```

아래에 추가:

```cpp
#include <cmath>
```

1-13. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomSpawn.cpp

CONFIRM_NEEDED:
- 이번 S003은 map presentation과 client concealment identity 복구까지만 반영한다.
- 서버가 `StageData::bushes`를 읽지 않고 snapshot relevance도 세션별로 거르지 않는 현재 상태는 별도 서버 권위 세션에서 `GameRoomSpawn`, `GameRoomTick`, `SnapshotBuilder`, `GameRoomReplication`의 실제 호출 순서를 다시 검사한 뒤 코드 블록을 확정한다.
- 클라이언트 부쉬 복구를 서버 권위 완료로 표현하지 않는다.

2. 검증

사전 고정 증거:
- `Data/Stage1.dat`: v4, S=30/J=12/W=27/B=0, 5,408 bytes.
- `map11_brush_volumes.csv`: centered local frame 64 records, bushId 1..32 각각 2개.
- raw CSV는 25/64가 NavGrid 밖이며 `X + 104.50` 변환 뒤 64/64가 NavGrid bounds 안이다.
- 기존 WAMB는 bird 5 + duck 1이며 Insects 8개는 `base.materials.bin`의 `Audio-Emitter_SRU_Insects*`에서 얻는다.
- standalone bush mesh는 없고 35-vertex windgrass card만 있으므로 이 카드를 3방향 교차 클러스터로 bake한다. raw PNG 한 장을 overlay billboard로 직접 그리지 않는다.

생성 명령:
- `python Tools/build_map11_bush_cluster.py`
- `python Tools/cook_map11_brush_volumes.py --migrate-stage`
- `python Tools/cook_map11_ambient_props.py`
- `python Tools/LoLData/Build-LoLDefinitionPack.py`

자동 검증:
- `Tools/Bin/Debug/WintersAssetConverter.exe info Client/Bin/Resource/Texture/MAP/Map11_Rebuild/cooked/map11_bush_cluster.wmesh`
- `Tools/Bin/Debug/WintersAssetConverter.exe info Client/Bin/Resource/Texture/MAP/Map11_Rebuild/cooked/map11_bush_cluster.wmat`
- Stage가 v5/S30/J12/W27/B64이고 21,540 bytes인지 확인.
- bushId 1..32가 각각 정확히 2개인지 확인.
- WAMB가 v1/count=14인지 확인.
- `python Tools/LoLData/Build-LoLDefinitionPack.py --check`
- `powershell -ExecutionPolicy Bypass -File Tools/Harness/Check-SharedBoundary.ps1`
- `git diff --check`
- Client Debug x64 빌드.
- Server Debug x64 빌드. Stage ABI 공유 회귀를 확인하되 서버 부쉬 권위 완료 판정으로 사용하지 않는다.

수동 확인:
- normal F5에서 부쉬가 맵 깊이 뒤/앞 관계에 맞게 렌더되고 카메라를 움직여도 overlay처럼 떠 있지 않는지 확인.
- Editor에서 동일 Mesh bush가 보이고 선택 원과 시각 중심이 일치하는지 확인.
- 기존 bird/duck 좌표가 Stage-frame 골든 위치 `(116.453,59.164)`, `(108.025,21.885)` 부근으로 교정됐는지 확인.
- Insects 8개 위치에 firefly idle animation이 지면 위 1.4m에서 재생되는지 확인.
- 같은 bushId의 두 원 사이에서 concealmentId가 바뀌지 않는지 확인.

천장 30% 산출물:
- normal F5에서 중앙·상단·하단 부쉬와 firefly가 함께 보이는 30초 영상 또는 전후 스크린샷 2장을 남긴다.
- 서버 권위 부쉬 은신은 이 영상으로 완료 처리하지 않고 별도 LAN 세션의 패킷/타게팅 검증으로 닫는다.

후속 동기화:
- Engine public header는 변경하지 않으므로 `UpdateLib.bat`은 필요 없다.
- 생성 C++는 직접 편집하지 않고 LoL definition codegen 산출물만 갱신한다.
