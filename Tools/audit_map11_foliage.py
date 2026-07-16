from __future__ import annotations

import argparse
import struct
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_VISUAL = ROOT / "Client/Bin/Resource/Texture/MAP/output/sr_base_flip.wmesh"
DEFAULT_SURFACE = ROOT / "Client/Bin/Resource/Texture/MAP/output/sr_base_flip_surface.wmesh"
DEFAULT_STAGE = ROOT / "Data/Stage1.dat"
FOLIAGE_MATERIAL_PREFIX = "Maps/KitPieces/SRX/Base/Models/LevelProp/Materials/VertexDeform"
FOLIAGE_MATERIAL_HASH_NAME = f"{FOLIAGE_MATERIAL_PREFIX}_inst"
FOLIAGE_TEXTURE = "Texture/MAP/output/textures/assets/maps/kitpieces/srx/textures/sru_brush.png"
DEFAULT_GRASS_TINT = (
    ROOT
    / "Client/Bin/Resource/Texture/MAP/output/textures/assets/maps/info/map11/grasstint_srx.png"
)


@dataclass(frozen=True)
class Material:
    name: str
    diffuse_path: str


@dataclass(frozen=True)
class MeshAudit:
    submesh_count: int
    vertex_count: int
    index_count: int
    foliage_indices: tuple[int, ...]
    foliage_hashes: tuple[int, ...]
    used_empty_material_indices: tuple[int, ...]
    bounds: tuple[tuple[float, ...], ...]


def read_wmat(path: Path) -> dict[int, Material]:
    data = path.read_bytes()
    if data[:4] != b"WINT" or data[16:20] != b"WMAT":
        raise ValueError(f"invalid WMAT: {path}")

    material_count = struct.unpack_from("<I", data, 20)[0]
    offset = 24
    materials: dict[int, Material] = {}
    for _ in range(material_count):
        index, _hash, raw_name, raw_path = struct.unpack_from("<IQ64s520s", data, offset)
        offset += 596
        name = raw_name.split(b"\0", 1)[0].decode("utf-8", errors="replace")
        diffuse_path = raw_path.decode("utf-16le").split("\0", 1)[0]
        materials[index] = Material(name=name, diffuse_path=diffuse_path)
    return materials


def read_wmesh(path: Path, materials: dict[int, Material]) -> MeshAudit:
    data = path.read_bytes()
    if data[:4] != b"WINT" or data[16:20] != b"WMSH":
        raise ValueError(f"invalid WMESH: {path}")

    header = struct.unpack_from("<4s7IB3s", data, 16)
    submesh_count = header[1]
    bone_count = header[2]
    vertex_stride = header[4]
    vertex_count = header[5]
    index_count = header[6]
    index_stride = header[7]
    has_bounds = header[8] != 0

    offset = 52
    material_indices: list[int] = []
    material_hashes: list[int] = []
    for _ in range(submesh_count):
        desc = struct.unpack_from("<5IQ20s", data, offset)
        offset += 48
        material_indices.append(desc[4])
        material_hashes.append(desc[5])

    bounds_offset = (
        16
        + 36
        + submesh_count * 48
        + vertex_count * vertex_stride
        + index_count * index_stride
        + bone_count * 128
    )
    bounds = tuple(
        struct.unpack_from("<10f", data, bounds_offset + index * 40)
        for index in range(submesh_count)
    ) if has_bounds else ()

    foliage_indices = tuple(
        index
        for index, material_index in enumerate(material_indices)
        if materials[material_index].name.startswith(FOLIAGE_MATERIAL_PREFIX)
    )
    foliage_hashes = tuple(material_hashes[index] for index in foliage_indices)
    used_empty = tuple(
        index
        for index, material_index in enumerate(material_indices)
        if not materials[material_index].diffuse_path
    )
    return MeshAudit(
        submesh_count=submesh_count,
        vertex_count=vertex_count,
        index_count=index_count,
        foliage_indices=foliage_indices,
        foliage_hashes=foliage_hashes,
        used_empty_material_indices=used_empty,
        bounds=bounds,
    )


def read_stage_bush_count(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if len(data) < 32:
        raise ValueError(f"invalid Stage data: {path}")
    magic, version = struct.unpack_from("<II", data, 0)
    if magic != 0x47545357 or not 3 <= version <= 5:
        raise ValueError(f"unsupported Stage header: magic=0x{magic:08X} version={version}")

    offset = 32
    structure_count = struct.unpack_from("<I", data, offset)[0]
    offset += 4 + structure_count * 112
    jungle_count = struct.unpack_from("<I", data, offset)[0]
    offset += 4 + jungle_count * 104
    if version >= 4:
        waypoint_count = struct.unpack_from("<I", data, offset)[0]
        offset += 4 + waypoint_count * 28
    bush_count = 0
    if version >= 5:
        bush_count = struct.unpack_from("<I", data, offset)[0]
        offset += 4 + bush_count * 252
    if offset != len(data):
        raise ValueError(f"Stage size mismatch: parsed={offset} actual={len(data)}")
    return version, bush_count


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def read_png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise ValueError(f"invalid PNG: {path}")
    return struct.unpack_from(">II", data, 16)


def fnv1a64(value: str) -> int:
    result = 0xCBF29CE484222325
    for byte in value.encode("utf-8"):
        result ^= byte
        result = (result * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return result


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Audit Map11 foliage mesh, authored GrassTint, and retired Stage billboards."
    )
    parser.add_argument("--visual", type=Path, default=DEFAULT_VISUAL)
    parser.add_argument("--surface", type=Path, default=DEFAULT_SURFACE)
    parser.add_argument("--stage", type=Path, default=DEFAULT_STAGE)
    parser.add_argument("--grass-tint", type=Path, default=DEFAULT_GRASS_TINT)
    args = parser.parse_args()

    visual_materials = read_wmat(args.visual.with_suffix(".wmat"))
    visual = read_wmesh(args.visual, visual_materials)
    surface_materials = read_wmat(args.surface.with_suffix(".wmat"))
    surface = read_wmesh(args.surface, surface_materials)
    stage_version, stage_bush_count = read_stage_bush_count(args.stage)
    require(args.grass_tint.is_file(), f"missing authored GrassTint texture: {args.grass_tint}")
    grass_tint_size = read_png_size(args.grass_tint)
    require(grass_tint_size == (256, 256),
            f"unexpected GrassTint atlas size: {grass_tint_size}")

    require(visual.foliage_indices, "visual map has no genuine VertexDeform foliage meshes")
    expected_foliage_hash = fnv1a64(FOLIAGE_MATERIAL_HASH_NAME)
    require(
        set(visual.foliage_hashes) == {expected_foliage_hash},
        "visual foliage material hash no longer matches the runtime grass-tint selector: "
        f"actual={[hex(value) for value in sorted(set(visual.foliage_hashes))]} "
        f"expected={hex(expected_foliage_hash)}",
    )
    require(not visual.used_empty_material_indices,
            f"visual map has {len(visual.used_empty_material_indices)} submeshes using empty diffuse paths")
    require(not surface.foliage_indices, "surface mesh still contains visual foliage triangles")
    require(
        visual.submesh_count - surface.submesh_count == len(visual.foliage_indices),
        "surface mesh excludes geometry other than the intended foliage material",
    )
    require(stage_bush_count == 0, f"Stage billboard/mesh bush entries regressed: B{stage_bush_count}")

    foliage_paths = {
        visual_materials[index].diffuse_path.lower()
        for index in visual_materials
        if visual_materials[index].name.startswith(FOLIAGE_MATERIAL_PREFIX)
    }
    require(foliage_paths == {FOLIAGE_TEXTURE.lower()},
            f"foliage material remap mismatch: {sorted(foliage_paths)}")

    centers = [visual.bounds[index][6:9] for index in visual.foliage_indices]
    x_span = max(center[0] for center in centers) - min(center[0] for center in centers)
    z_span = max(center[2] for center in centers) - min(center[2] for center in centers)
    require(x_span > 10_000.0 and z_span > 10_000.0,
            f"foliage node transforms were not baked across the map: xSpan={x_span:.1f} zSpan={z_span:.1f}")

    print(
        "PASS "
        f"visual=S{visual.submesh_count}/V{visual.vertex_count}/I{visual.index_count} "
        f"foliage={len(visual.foliage_indices)} "
        f"grassTint={grass_tint_size[0]}x{grass_tint_size[1]} "
        f"span=({x_span:.1f},{z_span:.1f}) "
        f"surface=S{surface.submesh_count}/V{surface.vertex_count}/I{surface.index_count} "
        f"stage=v{stage_version}/B{stage_bush_count}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
