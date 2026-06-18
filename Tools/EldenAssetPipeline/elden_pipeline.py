#!/usr/bin/env python3
"""Elden Ring asset manifest tooling for Winters.

The parser consumes WitchyBND XML outputs rather than raw FromSoftware binaries.
This keeps the pipeline deterministic and lets WitchyBND own binary format drift.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


SUPPORTED_TEXTURE_EXTS = (".png", ".dds", ".tif", ".tiff", ".tga", ".jpg", ".jpeg")
SUPPORTED_MODEL_EXTS = (".fbx", ".flver", ".obj")
TPF_TEXTURE_SETTLE_SECONDS = 1.0

ROLE_PATTERNS = (
    ("albedo", ("albedo", "diffuse", "basecolor", "basemap", "colormap")),
    ("normal", ("normal", "normalmap")),
    ("mask", ("metallic", "mask", "materialmap", "multi")),
    ("emissive", ("emissive", "emission", "emissivemap")),
    ("roughness", ("roughness", "reflection", "reflectance")),
    ("specular", ("specular", "shininess")),
    ("height", ("height", "displacement", "distortiondepth")),
)

ROLE_SUFFIXES = {
    "albedo": ("_a", "_d", "_albedo", "_diffuse", "_basecolor"),
    "normal": ("_n", "_normal"),
    "mask": ("_m", "_1m", "_3m", "_mask", "_metallic"),
    "emissive": ("_em", "_e", "_emissive", "_emission"),
    "roughness": ("_r", "_roughness"),
    "specular": ("_s", "_specular"),
    "height": ("_h", "_height", "_d"),
}

VARIANT_SUFFIXES = (
    "_metal",
    "_crystal",
    "_cloth",
    "_norich",
    "_leather",
    "_fabric",
    "_wood",
    "_iron",
    "_rope",
)

TEXTURE_ROLE_SUFFIXES = (
    ("emissive", ("_em", "_emissive", "_emission")),
    ("roughness", ("_r", "_roughness")),
    ("normal", ("_n", "_normal")),
    ("metallic", ("_1m", "_3m", "_m", "_metallic", "_mask")),
    ("albedo", ("_a", "_d", "_albedo", "_diffuse", "_basecolor")),
)

MODEL_INFO_RE = re.compile(
    r"submeshes=(?P<submeshes>\d+)\s+"
    r"bones=(?P<bones>\d+)\s+"
    r"vertices=(?P<vertices>\d+)\s+"
    r"indices=(?P<indices>\d+)\s+"
    r"stride=(?P<stride>\d+)"
)

SAFE_FILENAME_RE = re.compile(r"[^A-Za-z0-9_.-]+")

DEFAULT_SOURCE_CHARACTER_TARGETS = {
    "c0000": {"label": "PlayerBase", "reason": "limgrave-map-character-id", "confidence": "candidate"},
    "c0100": {"label": "LimgraveCharacter", "reason": "limgrave-map-character-id", "confidence": "candidate"},
    "c1000": {"label": "LimgraveCharacter", "reason": "limgrave-map-character-id", "confidence": "candidate"},
    "c2050": {"label": "LimgraveNPCOrMob", "reason": "limgrave-map-character-id", "confidence": "map-id"},
    "c2270": {"label": "LimgraveNPCOrMob", "reason": "limgrave-map-character-id", "confidence": "map-id"},
    "c3000": {"label": "GatefrontSoldier", "reason": "requested-limgrave-knight-enemy", "confidence": "candidate"},
    "c3010": {"label": "GatefrontSoldier", "reason": "requested-limgrave-knight-enemy", "confidence": "candidate"},
    "c3200": {"label": "LimgraveNPCOrMob", "reason": "limgrave-map-character-id", "confidence": "map-id"},
    "c3210": {"label": "LimgraveNPCOrMob", "reason": "limgrave-map-character-id", "confidence": "map-id"},
    "c3250": {"label": "TreeGuardVariant", "reason": "requested-tree-guard-family", "confidence": "candidate"},
    "c3251": {"label": "TreeGuard", "reason": "requested-tree-guard", "confidence": "map-id"},
    "c3252": {"label": "TreeGuardVariant", "reason": "requested-tree-guard-family", "confidence": "candidate"},
    "c4200": {"label": "LimgraveNPCOrMob", "reason": "limgrave-map-character-id", "confidence": "map-id"},
    "c6001": {"label": "LimgraveNPCOrMob", "reason": "limgrave-map-character-id", "confidence": "map-id"},
    "c6060": {"label": "LimgraveNPCOrMob", "reason": "limgrave-map-character-id", "confidence": "map-id"},
    "c6080": {"label": "LimgraveNPCOrMob", "reason": "limgrave-map-character-id", "confidence": "map-id"},
    "c6100": {"label": "LimgraveNPCOrMob", "reason": "limgrave-map-character-id", "confidence": "map-id"},
    "c9001": {"label": "TorrentCandidate", "reason": "requested-spirit-steed", "confidence": "candidate"},
}

AEG_ASSET_ID_RE = re.compile(r"\b(AEG\d{3}_\d{3})\b", re.IGNORECASE)

BUNDLE_SUFFIXES = (
    ("character-binder", ".chrbnd.dcx"),
    ("character-texture-binder", ".texbnd.dcx"),
    ("character-animation-binder", ".anibnd.dcx"),
    ("character-behavior-binder", ".behbnd.dcx"),
    ("parts-binder", ".partsbnd.dcx"),
    ("asset-geometry-binder", ".geombnd.dcx"),
    ("map-binder", ".mapbnd.dcx"),
    ("map-msb", ".msb.dcx"),
    ("texture-binder", ".tpfbnd.dcx"),
    ("effect-binder", ".ffxbnd.dcx"),
    ("param-binder", ".parambnd.dcx"),
    ("message-binder", ".msgbnd.dcx"),
    ("navmesh-binder", ".nvmhktbnd.dcx"),
    ("ivinfo-binder", ".ivinfobnd.dcx"),
    ("battle-binder", ".btl.dcx"),
    ("dcx", ".dcx"),
)

PIPELINE_PRIORITY_BY_TOP_DIR = {
    "map": 10,
    "asset": 20,
    "chr": 30,
    "parts": 40,
    "menu": 50,
    "sfx": 60,
    "cutscene": 70,
    "msg": 80,
    "param": 90,
}

PIPELINE_COLLECT_EXTS = {
    ".flver": "model",
    ".tpf": "texture-container",
    ".dds": "texture",
    ".png": "texture",
    ".matbin": "material",
    ".fxr": "effect",
    ".hkx": "animation-raw",
    ".hkxpwv": "animation-raw",
    ".tae": "animation-raw",
    ".xml": "xml",
}

FLVER_TO_FBX_BLENDER_SCRIPT = r'''
import argparse
import json
import sys
import traceback
from pathlib import Path

import bpy

try:
    import gpu
    class _DummyShader:
        def bind(self): pass
        def uniform_float(self, *args, **kwargs): pass
    class _DummyBatch:
        def draw(self, *args, **kwargs): pass
    gpu.shader.from_builtin = lambda *args, **kwargs: _DummyShader()
    import gpu_extras.batch
    gpu_extras.batch.batch_for_shader = lambda *args, **kwargs: _DummyBatch()
except Exception as ex:
    print(f"GPU_STUB_SKIPPED={ex}")

argv = sys.argv
if "--" in argv:
    argv = argv[argv.index("--") + 1:]
else:
    argv = []

parser = argparse.ArgumentParser()
parser.add_argument("--input-json", required=True)
parser.add_argument("--summary", required=True)
parser.add_argument("--soulstruct-root", required=True)
parser.add_argument("--game-root", default="")
parser.add_argument("--import-textures", action="store_true")
args = parser.parse_args(argv)

addon_root = Path(args.soulstruct_root)
io_soulstruct = addon_root / "io_soulstruct"
for path in (io_soulstruct.parent, io_soulstruct):
    path_text = str(path)
    if path_text not in sys.path:
        sys.path.insert(0, path_text)

import io_soulstruct
io_soulstruct.register()

if args.game_root:
    # Without a game root Soulstruct cannot resolve MATBIN material params and
    # material-heavy FLVERs (armor parts) crash with NoneType.get_param.
    try:
        _settings = bpy.context.scene.soulstruct_settings
        _settings.game_enum = "ELDEN_RING"
        _settings.eldenring_game_root_str = args.game_root
        print(f"SOULSTRUCT_GAME_ROOT={args.game_root}")
    except Exception as ex:
        print(f"SOULSTRUCT_SETTINGS_SKIPPED={ex}")

items = json.loads(Path(args.input_json).read_text(encoding="utf-8-sig"))
summary = []

bpy.context.scene.flver_import_settings.import_textures = bool(args.import_textures)
bpy.context.scene.flver_import_settings.merge_mesh_vertices = True

for idx, item in enumerate(items):
    source = Path(item["source"])
    output = Path(item["output"])
    output.parent.mkdir(parents=True, exist_ok=True)
    record = {
        "source": str(source),
        "output": str(output),
        "ok": False,
        "objects": 0,
        "meshes": 0,
        "armatures": 0,
        "detail": "",
    }
    try:
        bpy.ops.object.select_all(action="SELECT")
        bpy.ops.object.delete()
        try:
            bpy.ops.import_scene.flver(directory=str(source.parent), files=[{"name": source.name}])
        except Exception as ex:
            record["detail"] = f"import_exception_ignored={type(ex).__name__}: {str(ex).splitlines()[0] if str(ex).splitlines() else ex}"

        record["objects"] = len(bpy.data.objects)
        record["meshes"] = len(bpy.data.meshes)
        record["armatures"] = len(bpy.data.armatures)
        record["actions"] = len(bpy.data.actions)
        if record["objects"] == 0 or record["meshes"] == 0:
            raise RuntimeError("FLVER import produced no drawable mesh objects")

        bpy.ops.object.select_all(action="SELECT")
        bpy.ops.export_scene.fbx(
            filepath=str(output),
            use_selection=True,
            add_leaf_bones=False,
            bake_anim=False,
        )
        record["ok"] = output.exists() and output.stat().st_size > 0
        record["bytes"] = output.stat().st_size if output.exists() else 0
    except Exception as ex:
        record["ok"] = False
        record["detail"] = f"{type(ex).__name__}: {ex}"
        traceback.print_exc()
    summary.append(record)
    print(f"[{idx + 1}/{len(items)}] {source.name} ok={record['ok']} meshes={record['meshes']} out={output.name}", flush=True)

Path(args.summary).write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(f"SUMMARY={args.summary}")
'''


HKX_ANIM_TO_FBX_BLENDER_SCRIPT = r'''
import argparse
import json
import re
import sys
import traceback
from pathlib import Path

import bpy

try:
    import gpu
    class _DummyShader:
        def bind(self): pass
        def uniform_float(self, *args, **kwargs): pass
    class _DummyBatch:
        def draw(self, *args, **kwargs): pass
    gpu.shader.from_builtin = lambda *args, **kwargs: _DummyShader()
    import gpu_extras.batch
    gpu_extras.batch.batch_for_shader = lambda *args, **kwargs: _DummyBatch()
except Exception as ex:
    print(f"GPU_STUB_SKIPPED={ex}")

argv = sys.argv
if "--" in argv:
    argv = argv[argv.index("--") + 1:]
else:
    argv = []

parser = argparse.ArgumentParser()
parser.add_argument("--input-json", required=True)
parser.add_argument("--summary", required=True)
parser.add_argument("--soulstruct-root", required=True)
args = parser.parse_args(argv)

addon_root = Path(args.soulstruct_root)
io_soulstruct = addon_root / "io_soulstruct"
for path in (io_soulstruct.parent, io_soulstruct):
    path_text = str(path)
    if path_text not in sys.path:
        sys.path.insert(0, path_text)

import io_soulstruct
io_soulstruct.register()

from soulstruct.containers import EntryNotFoundError
from soulstruct.eldenring.containers import DivBinder
from soulstruct.havok.core import HKX
from soulstruct.blender.animation.utilities import read_animation_hkx_entry, read_skeleton_hkx_entry
from soulstruct.blender.animation.types import SoulstructAnimation


class _HeadlessOperator:
    """Minimal LoggingOperator stand-in for SoulstructAnimation API calls."""

    def info(self, msg):
        print(f"[INFO] {msg}", flush=True)

    def debug(self, msg):
        pass

    def warning(self, msg):
        print(f"[WARN] {msg}", flush=True)

    def error(self, msg):
        print(f"[ERR] {msg}", flush=True)
        return {"CANCELLED"}


ANIM_ENTRY_RE = re.compile(r"^a.*\.hkx(\.dcx)?$", re.IGNORECASE)
SKELETON_ENTRY_RE = re.compile(r"skeleton\.hkx(\.dcx)?", re.IGNORECASE)

jobs = json.loads(Path(args.input_json).read_text(encoding="utf-8-sig"))
operator = _HeadlessOperator()
summary = []

for index, job in enumerate(jobs, start=1):
    record = {
        "model": job.get("model"),
        "fbx": job.get("fbx"),
        "anibnd": job.get("anibnd"),
        "output": job.get("output"),
        "ok": False,
        "actions": 0,
        "animsRequested": 0,
        "animsImported": 0,
        "failedEntries": [],
        "detail": "",
    }
    try:
        bpy.ops.object.select_all(action="SELECT")
        bpy.ops.object.delete()
        for action in list(bpy.data.actions):
            bpy.data.actions.remove(action)

        bpy.ops.import_scene.fbx(filepath=str(job["fbx"]))
        armature_obj = next((obj for obj in bpy.data.objects if obj.type == "ARMATURE"), None)
        if armature_obj is None:
            raise RuntimeError("imported FBX has no armature object")
        bpy.context.view_layer.objects.active = armature_obj

        binder = DivBinder.from_path(job["anibnd"])
        try:
            compendium_entry = binder.find_entry_matching_name(r".*\.compendium")
            compendium = HKX.from_binder_entry(compendium_entry)
        except EntryNotFoundError:
            compendium = None
        skeleton_entry = binder[SKELETON_ENTRY_RE]
        skeleton_hkx = read_skeleton_hkx_entry(skeleton_entry, compendium)

        anim_filter = re.compile(job["animFilter"]) if job.get("animFilter") else None
        entries = [entry for entry in binder.entries if ANIM_ENTRY_RE.match(entry.name)]
        if anim_filter is not None:
            entries = [entry for entry in entries if anim_filter.search(entry.name)]
        max_anims = int(job.get("maxAnims") or 0)
        if max_anims > 0:
            entries = entries[:max_anims]
        record["animsRequested"] = len(entries)

        for entry in entries:
            anim_name = entry.name.split(".")[0]
            try:
                animation_hkx = read_animation_hkx_entry(entry, compendium)
                SoulstructAnimation.new_from_hkx_animation(
                    operator,
                    bpy.context,
                    animation_hkx,
                    skeleton_hkx=skeleton_hkx,
                    name=anim_name,
                    armature_obj=armature_obj,
                    model_name=str(job.get("model")),
                )
                record["animsImported"] += 1
            except Exception as ex:
                record["failedEntries"].append({"entry": entry.name, "error": f"{type(ex).__name__}: {ex}"})
            print(f"[{index}/{len(jobs)}] {job.get('model')} {anim_name} imported={record['animsImported']}", flush=True)

        record["actions"] = len(bpy.data.actions)
        if record["animsImported"] == 0:
            raise RuntimeError("no HKX animation could be imported")

        output = Path(job["output"])
        output.parent.mkdir(parents=True, exist_ok=True)
        # Assimp rejects Blender armature-only FBX exports, so keep meshes in
        # the file like the proven legacy anim FBX path (anim3010.fbx).
        bpy.ops.object.select_all(action="SELECT")
        bpy.context.view_layer.objects.active = armature_obj
        bpy.ops.export_scene.fbx(
            filepath=str(output),
            use_selection=True,
            object_types={"ARMATURE", "MESH"},
            add_leaf_bones=False,
            bake_anim=True,
            bake_anim_use_all_actions=True,
            bake_anim_use_nla_strips=False,
            bake_anim_use_all_bones=True,
        )
        record["ok"] = output.exists() and output.stat().st_size > 0
        record["bytes"] = output.stat().st_size if output.exists() else 0
    except Exception as ex:
        record["ok"] = False
        record["detail"] = f"{type(ex).__name__}: {ex}"
        traceback.print_exc()
    summary.append(record)
    print(f"[{index}/{len(jobs)}] {job.get('model')} ok={record['ok']} anims={record['animsImported']}/{record['animsRequested']}", flush=True)

Path(args.summary).write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(f"SUMMARY={args.summary}")
'''


def strip_ns(tag: str) -> str:
    return tag.rsplit("}", 1)[-1] if "}" in tag else tag


def child_text(elem: ET.Element, name: str, default: str = "") -> str:
    for child in elem:
        if strip_ns(child.tag) == name:
            return (child.text or "").strip()
    return default


def child(elem: ET.Element, name: str) -> ET.Element | None:
    for item in elem:
        if strip_ns(item.tag) == name:
            return item
    return None


def parse_scalar(text: str) -> Any:
    text = text.strip()
    if text == "":
        return ""
    if text.lower() == "true":
        return True
    if text.lower() == "false":
        return False
    try:
        if any(ch in text for ch in ".eE"):
            return float(text)
        return int(text)
    except ValueError:
        return text


def parse_value(elem: ET.Element | None) -> Any:
    if elem is None:
        return None
    children = list(elem)
    if not children:
        return parse_scalar(elem.text or "")
    return [parse_scalar(child_item.text or "") for child_item in children]


def windows_stem(path_text: str) -> str:
    if not path_text:
        return ""
    name = re.split(r"[\\/]", path_text.strip())[-1]
    for ext in SUPPORTED_TEXTURE_EXTS + (".matbin", ".fxr"):
        if name.lower().endswith(ext):
            return name[: -len(ext)]
    return Path(name).stem


def normalize_material_key(filename: str, fallback_path: Path) -> str:
    name = filename.strip() if filename.strip() else fallback_path.name
    if name.lower().endswith(".xml"):
        name = name[:-4]
    if name.lower().endswith(".matbin"):
        name = name[:-7]
    return name


def sampler_role(sampler_type: str) -> str:
    lowered = sampler_type.lower()
    for role, needles in ROLE_PATTERNS:
        if any(needle in lowered for needle in needles):
            return role
    return "unknown"


def build_texture_index(texture_roots: list[Path]) -> dict[str, list[Path]]:
    index: dict[str, list[Path]] = {}
    for root in texture_roots:
        if not root.exists():
            continue
        for dirpath, _, filenames in os.walk(root):
            for filename in filenames:
                ext = Path(filename).suffix.lower()
                if ext not in SUPPORTED_TEXTURE_EXTS:
                    continue
                path = Path(dirpath) / filename
                index.setdefault(path.stem.lower(), []).append(path)
    return index


def path_for_json(path: Path | None) -> str | None:
    return str(path).replace("\\", "/") if path else None


def resolve_texture_path(source_path: str, texture_index: dict[str, list[Path]]) -> Path | None:
    stem = windows_stem(source_path).lower()
    if not stem:
        return None
    matches = texture_index.get(stem)
    return matches[0] if matches else None


def parse_matbin_xml(path: Path, texture_index: dict[str, list[Path]]) -> dict[str, Any]:
    root = ET.parse(path).getroot()
    if strip_ns(root.tag) != "MATBIN":
        raise ValueError(f"not a MATBIN XML: {path}")

    filename = child_text(root, "filename", path.name)
    material_key = normalize_material_key(filename, path)

    params: list[dict[str, Any]] = []
    params_elem = child(root, "Params")
    if params_elem is not None:
        for param in params_elem:
            if strip_ns(param.tag) != "Param":
                continue
            params.append(
                {
                    "name": child_text(param, "Name"),
                    "type": child_text(param, "Type"),
                    "key": parse_scalar(child_text(param, "Key")),
                    "value": parse_value(child(param, "Value")),
                }
            )

    samplers: list[dict[str, Any]] = []
    grouped: dict[str, list[dict[str, Any]]] = {}
    samplers_elem = child(root, "Samplers")
    if samplers_elem is not None:
        for sampler in samplers_elem:
            if strip_ns(sampler.tag) != "Sampler":
                continue
            sampler_type = child_text(sampler, "Type")
            source_path = child_text(sampler, "Path")
            role = sampler_role(sampler_type)
            resolved = resolve_texture_path(source_path, texture_index)
            sampler_index = None
            if match := re.search(r"_Texture2D_(\d+)_([A-Za-z0-9]+)", sampler_type):
                sampler_index = int(match.group(1))
            record = {
                "type": sampler_type,
                "role": role,
                "samplerIndex": sampler_index,
                "sourcePath": source_path,
                "sourceStem": windows_stem(source_path),
                "resolvedPath": path_for_json(resolved),
                "key": parse_scalar(child_text(sampler, "Key")),
            }
            samplers.append(record)
            grouped.setdefault(role, []).append(record)

    primary: dict[str, str] = {}
    for role, entries in grouped.items():
        for entry in entries:
            chosen = entry.get("resolvedPath") or entry.get("sourcePath")
            if chosen:
                primary[role] = chosen
                break

    return {
        "name": material_key,
        "sourceXml": path_for_json(path),
        "filename": filename,
        "shaderPath": child_text(root, "ShaderPath"),
        "sourcePath": child_text(root, "SourcePath"),
        "key": parse_scalar(child_text(root, "Key")),
        "params": params,
        "samplers": samplers,
        "bindings": grouped,
        "primary": primary,
    }


def collect_xml_files(input_path: Path, root_name: str) -> list[Path]:
    if input_path.is_file():
        return [input_path]
    result: list[Path] = []
    for path in input_path.rglob("*.xml"):
        try:
            root = ET.parse(path).getroot()
        except ET.ParseError:
            continue
        if strip_ns(root.tag) == root_name:
            result.append(path)
    return sorted(result)


def parse_matbin_command(args: argparse.Namespace) -> int:
    texture_index = build_texture_index([Path(p) for p in args.texture_root])
    materials: dict[str, Any] = {}
    source_files = collect_xml_files(Path(args.input), "MATBIN")
    for source_file in source_files:
        material = parse_matbin_xml(source_file, texture_index)
        materials[material["name"]] = material

    manifest = {
        "schema": "winters.elden.material_manifest.v1",
        "generatedBy": "Tools/EldenAssetPipeline/elden_pipeline.py parse-matbin",
        "sourceFiles": [path_for_json(path) for path in source_files],
        "textureRoots": [str(Path(p)).replace("\\", "/") for p in args.texture_root],
        "materials": materials,
    }
    write_json(Path(args.out), manifest, args.pretty)
    return 0


def collect_typed_values(elem: ET.Element | None) -> dict[str, list[Any]]:
    values: dict[str, list[Any]] = {}
    if elem is None:
        return values
    for item in elem.iter():
        tag = strip_ns(item.tag)
        if tag in {"Int", "Float", "Byte", "SByte", "Short", "UShort", "Bool", "String"}:
            raw = item.attrib.get("Value", item.text or "")
            values.setdefault(tag.lower(), []).append(parse_scalar(raw))
    return values


def collect_integer_occurrences(elem: ET.Element | None, bucket: str) -> list[dict[str, Any]]:
    occurrences: list[dict[str, Any]] = []
    if elem is None:
        return occurrences
    index = 0
    for item in elem.iter():
        if strip_ns(item.tag) != "Int":
            continue
        raw = item.attrib.get("Value", item.text or "")
        value = parse_scalar(raw)
        if isinstance(value, int):
            occurrences.append({"bucket": bucket, "index": index, "value": value})
        index += 1
    return occurrences


def numeric_candidates(values: dict[str, list[Any]]) -> list[int]:
    out: list[int] = []
    for value in values.get("int", []):
        if isinstance(value, int) and value not in (-1, 0, 1) and 10 <= abs(value) <= 10_000_000:
            out.append(value)
    return out


def action_record(action: ET.Element, context: dict[str, Any]) -> dict[str, Any]:
    fields1 = collect_typed_values(child(action, "Fields1"))
    fields2 = collect_typed_values(child(action, "Fields2"))
    props1 = collect_typed_values(child(action, "Properties1"))
    props2 = collect_typed_values(child(action, "Properties2"))
    integer_occurrences = (
        collect_integer_occurrences(child(action, "Fields1"), "fields1")
        + collect_integer_occurrences(child(action, "Fields2"), "fields2")
        + collect_integer_occurrences(child(action, "Properties1"), "properties1")
        + collect_integer_occurrences(child(action, "Properties2"), "properties2")
    )
    all_values: dict[str, list[Any]] = {}
    for bucket in (fields1, fields2, props1, props2):
        for key, values in bucket.items():
            all_values.setdefault(key, []).extend(values)

    return {
        "id": parse_scalar(action.attrib.get("Id", "0")),
        "context": context,
        "flags": {
            "unk02": child_text(action, "Unk02"),
            "unk03": child_text(action, "Unk03"),
            "unk04": child_text(action, "Unk04"),
        },
        "fields1": fields1,
        "fields2": fields2,
        "properties1": props1,
        "properties2": props2,
        "integerOccurrences": integer_occurrences,
        "integerCandidates": numeric_candidates(all_values),
    }


def walk_fxr_container(
    container: ET.Element,
    container_stack: list[int],
    out_containers: list[dict[str, Any]],
    out_effects: list[dict[str, Any]],
    out_actions: list[dict[str, Any]],
) -> None:
    container_id = parse_scalar(container.attrib.get("Id", "0"))
    stack = container_stack + [int(container_id) if isinstance(container_id, int) else 0]
    out_containers.append({"id": container_id, "path": stack})

    actions_elem = child(container, "Actions")
    if actions_elem is not None:
        for action in actions_elem:
            if strip_ns(action.tag) == "Action":
                out_actions.append(action_record(action, {"containerPath": stack, "effectId": None}))

    effects_elem = child(container, "Effects")
    if effects_elem is not None:
        for effect in effects_elem:
            if strip_ns(effect.tag) != "Effect":
                continue
            effect_id = parse_scalar(effect.attrib.get("Id", "0"))
            out_effects.append({"id": effect_id, "containerPath": stack})
            effect_actions = child(effect, "Actions")
            if effect_actions is not None:
                for action in effect_actions:
                    if strip_ns(action.tag) == "Action":
                        out_actions.append(
                            action_record(action, {"containerPath": stack, "effectId": effect_id})
                        )

    nested = child(container, "Containers")
    if nested is not None:
        for child_container in nested:
            if strip_ns(child_container.tag) == "Container":
                walk_fxr_container(child_container, stack, out_containers, out_effects, out_actions)


def parse_fxr_xml(path: Path) -> dict[str, Any]:
    root = ET.parse(path).getroot()
    if strip_ns(root.tag) not in {"FXR1", "FXR3"}:
        raise ValueError(f"not an FXR XML: {path}")

    containers: list[dict[str, Any]] = []
    effects: list[dict[str, Any]] = []
    actions: list[dict[str, Any]] = []

    for container in root:
        if strip_ns(container.tag) == "Container":
            walk_fxr_container(container, [], containers, effects, actions)

    candidate_counts = Counter()
    for action in actions:
        candidate_counts.update(action["integerCandidates"])

    return {
        "sourceXml": path_for_json(path),
        "filename": child_text(root, "filename", path.name),
        "fxrType": strip_ns(root.tag),
        "version": child_text(root, "Version"),
        "id": parse_scalar(child_text(root, "Id")),
        "containerCount": len(containers),
        "effectCount": len(effects),
        "actionCount": len(actions),
        "containers": containers,
        "effects": effects,
        "actions": actions,
        "integerCandidateCounts": [
            {"value": value, "count": count} for value, count in candidate_counts.most_common()
        ],
    }


def parse_fxr_command(args: argparse.Namespace) -> int:
    source_files = collect_xml_files(Path(args.input), "FXR3")
    source_files.extend(path for path in collect_xml_files(Path(args.input), "FXR1") if path not in source_files)
    fxrs = [parse_fxr_xml(path) for path in sorted(source_files)]
    manifest = {
        "schema": "winters.elden.fxr_manifest.v1",
        "generatedBy": "Tools/EldenAssetPipeline/elden_pipeline.py parse-fxr",
        "sourceFiles": [path_for_json(path) for path in sorted(source_files)],
        "fxrs": fxrs,
    }
    write_json(Path(args.out), manifest, args.pretty)
    return 0


def role_from_sfx_suffix(suffix: str) -> str:
    normalized = suffix.lower()
    if normalized and not normalized.startswith("_"):
        normalized = "_" + normalized
    for role, suffixes in ROLE_SUFFIXES.items():
        if normalized in suffixes:
            return role
    return "unknown"


def build_sfx_texture_groups(resource_roots: list[Path]) -> dict[int, dict[str, Any]]:
    groups: dict[int, dict[str, Any]] = {}
    pattern = re.compile(r"^s(\d+)(?:_([A-Za-z0-9]+))?$", re.IGNORECASE)
    for root in resource_roots:
        if not root.exists():
            continue
        for dirpath, _, filenames in os.walk(root):
            for filename in filenames:
                path = Path(dirpath) / filename
                if path.suffix.lower() not in SUPPORTED_TEXTURE_EXTS:
                    continue
                match = pattern.match(path.stem)
                if not match:
                    continue
                resource_id = int(match.group(1))
                suffix = match.group(2) or ""
                role = role_from_sfx_suffix(suffix)
                group = groups.setdefault(
                    resource_id,
                    {
                        "id": resource_id,
                        "stem": f"s{resource_id:05d}",
                        "files": [],
                        "bindings": {},
                    },
                )
                file_record = {
                    "path": path_for_json(path),
                    "name": path.name,
                    "suffix": suffix,
                    "role": role,
                }
                group["files"].append(file_record)
                if role != "unknown" and role not in group["bindings"]:
                    group["bindings"][role] = file_record["path"]

    for group in groups.values():
        group["files"].sort(key=lambda item: str(item["name"]).lower())
    return groups


def build_model_groups(resource_roots: list[Path]) -> dict[int, list[dict[str, str]]]:
    groups: dict[int, list[dict[str, str]]] = {}
    for root in resource_roots:
        if not root.exists():
            continue
        for dirpath, _, filenames in os.walk(root):
            for filename in filenames:
                path = Path(dirpath) / filename
                if path.suffix.lower() not in SUPPORTED_MODEL_EXTS:
                    continue
                for match in re.finditer(r"\d+", path.stem):
                    resource_id = int(match.group(0))
                    groups.setdefault(resource_id, []).append(
                        {"path": path_for_json(path), "name": path.name, "stem": path.stem}
                    )
    return groups


def load_fxr_manifest(path: Path) -> dict[str, Any]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict) or "fxrs" not in data:
        raise ValueError(f"unsupported FXR manifest shape: {path}")
    return data


def fxr_candidate_locations(fxr: dict[str, Any]) -> dict[int, list[dict[str, Any]]]:
    locations: dict[int, list[dict[str, Any]]] = {}
    for action in fxr.get("actions", []):
        if not isinstance(action, dict):
            continue
        action_id = action.get("id")
        context = action.get("context", {})
        for occurrence in action.get("integerOccurrences", []):
            if not isinstance(occurrence, dict):
                continue
            value = occurrence.get("value")
            if not isinstance(value, int):
                continue
            locations.setdefault(value, []).append(
                {
                    "actionId": action_id,
                    "context": context,
                    "bucket": occurrence.get("bucket"),
                    "index": occurrence.get("index"),
                }
            )
    return locations


def resolve_fxr_resources_command(args: argparse.Namespace) -> int:
    fxr_manifest_path = Path(args.fxr_manifest)
    fxr_manifest = load_fxr_manifest(fxr_manifest_path)
    resource_roots = [Path(p) for p in args.resource_root]
    texture_groups = build_sfx_texture_groups(resource_roots)
    model_groups = build_model_groups(resource_roots)
    known_fxr_ids = {
        item.get("id")
        for item in fxr_manifest.get("fxrs", [])
        if isinstance(item, dict) and isinstance(item.get("id"), int)
    }

    fxr_records: list[dict[str, Any]] = []
    used_texture_ids: set[int] = set()
    used_model_ids: set[int] = set()

    for fxr in fxr_manifest.get("fxrs", []):
        if not isinstance(fxr, dict):
            continue
        locations = fxr_candidate_locations(fxr)
        resolved_textures = []
        resolved_models = []
        effect_refs = []
        ignored_candidates = []
        unresolved_candidates = []

        for candidate in fxr.get("integerCandidateCounts", []):
            if not isinstance(candidate, dict):
                continue
            value = candidate.get("value")
            count = candidate.get("count", 0)
            if not isinstance(value, int):
                continue
            record_base = {
                "value": value,
                "count": count,
                "locations": locations.get(value, [])[: args.max_locations],
            }
            if abs(value) < args.min_resource_id:
                ignored_candidates.append({**record_base, "reason": "below-min-resource-id"})
                continue

            matched = False
            texture_group = texture_groups.get(value)
            if texture_group:
                used_texture_ids.add(value)
                resolved_textures.append(
                    {
                        **record_base,
                        "resource": texture_group,
                        "score": int(count) * 10 + len(texture_group.get("bindings", {})),
                    }
                )
                matched = True

            models = model_groups.get(value)
            if models:
                used_model_ids.add(value)
                resolved_models.append({**record_base, "resources": models})
                matched = True

            if value in known_fxr_ids and value != fxr.get("id"):
                effect_refs.append({**record_base, "targetFxrId": value})
                matched = True

            if not matched:
                unresolved_candidates.append(record_base)

        resolved_textures.sort(key=lambda item: (-int(item.get("score", 0)), int(item.get("value", 0))))
        fxr_records.append(
            {
                "filename": fxr.get("filename"),
                "id": fxr.get("id"),
                "sourceXml": fxr.get("sourceXml"),
                "resolvedTextures": resolved_textures,
                "resolvedModels": resolved_models,
                "effectRefs": effect_refs,
                "ignoredCandidates": ignored_candidates,
                "unresolvedCandidates": unresolved_candidates,
            }
        )

    manifest = {
        "schema": "winters.elden.fxr_resource_manifest.v1",
        "generatedBy": "Tools/EldenAssetPipeline/elden_pipeline.py resolve-fxr",
        "fxrManifest": str(fxr_manifest_path).replace("\\", "/"),
        "resourceRoots": [str(path).replace("\\", "/") for path in resource_roots],
        "minResourceId": args.min_resource_id,
        "textureGroupCount": len(texture_groups),
        "modelGroupCount": len(model_groups),
        "usedTextureIds": sorted(used_texture_ids),
        "usedModelIds": sorted(used_model_ids),
        "textureGroups": {str(key): value for key, value in sorted(texture_groups.items())},
        "modelGroups": {str(key): value for key, value in sorted(model_groups.items())},
        "fxrs": fxr_records,
    }
    write_json(Path(args.out), manifest, args.pretty)
    return 0


def load_material_records(path: Path) -> list[dict[str, Any]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if isinstance(data, list):
        records: list[dict[str, Any]] = []
        for item in data:
            if isinstance(item, str):
                records.append({"name": item, "images": []})
            elif isinstance(item, dict):
                name = item.get("name") or item.get("materialName")
                if name:
                    records.append({**item, "name": str(name)})
        return records
    if isinstance(data, dict) and "materials" in data:
        records = []
        for item in data["materials"]:
            if isinstance(item, str):
                records.append({"name": item, "images": []})
            elif isinstance(item, dict):
                name = item.get("name") or item.get("materialName")
                if name:
                    records.append({**item, "name": str(name)})
        return records
    raise ValueError(f"unsupported material JSON shape: {path}")


def extract_material_token(material_name: str) -> str:
    if match := re.search(r"([CP]\[[^\]]+\](?:\[[^\]]+\])?_[^\]|<]+)", material_name):
        return match.group(1).strip()
    if match := re.search(r"([CP]\[[^\]]+\](?:\[[^\]]+\])?)", material_name):
        return match.group(1).strip()
    return material_name.strip()


def normalized_token_candidates(token: str) -> list[str]:
    raw = token.strip()
    candidates = [raw]

    if match := re.match(r"([CP])\[([^\]]+)\]_(.+)", raw):
        asset_id = match.group(2)
        suffix = match.group(3).strip()
        stem = f"{asset_id}_{suffix}"
        candidates.append(stem)
        simplified = stem
        changed = True
        while changed:
            changed = False
            lower = simplified.lower()
            for variant in VARIANT_SUFFIXES:
                if lower.endswith(variant):
                    simplified = simplified[: -len(variant)]
                    candidates.append(simplified)
                    changed = True
                    break
    elif match := re.match(r"P\[([^\]]+)\]_(.+)", raw):
        candidates.extend([f"{match.group(1)}_{match.group(2)}", match.group(1)])

    unique: list[str] = []
    seen = set()
    for candidate in candidates:
        key = candidate.lower()
        if key not in seen:
            unique.append(candidate)
            seen.add(key)
    return unique


def role_from_texture_path(texture_path: str) -> str | None:
    stem = windows_stem(texture_path).lower()
    if not stem:
        return None
    for role, suffixes in ROLE_SUFFIXES.items():
        if any(stem.endswith(suffix) for suffix in suffixes):
            return role
    return None


def bindings_from_inspected_images(material_record: dict[str, Any]) -> dict[str, str]:
    out: dict[str, str] = {}
    images = material_record.get("images", [])
    if not isinstance(images, list):
        return out
    for image in images:
        if not isinstance(image, dict):
            continue
        texture_path = str(image.get("filepath", "")).strip()
        if not texture_path:
            continue
        role = role_from_texture_path(texture_path)
        if role and role not in out:
            out[role] = texture_path.replace("\\", "/")
    return out


def resolve_role_from_suffix(
    role: str,
    stem_candidates: list[str],
    texture_index: dict[str, list[Path]],
) -> str | None:
    suffixes = ROLE_SUFFIXES.get(role, ())
    for stem in stem_candidates:
        lower = stem.lower()
        if lower in texture_index and any(lower.endswith(suffix) for suffix in suffixes):
            return path_for_json(texture_index[lower][0])
        for suffix in suffixes:
            key = f"{lower}{suffix}"
            if key in texture_index:
                return path_for_json(texture_index[key][0])
    for stem in stem_candidates:
        lower = stem.lower()
        for key, paths in texture_index.items():
            if key.startswith(lower) and any(key.endswith(suffix) for suffix in suffixes):
                return path_for_json(paths[0])
    return None


def material_primary_from_manifest(matbin_material: dict[str, Any]) -> dict[str, str]:
    primary = matbin_material.get("primary", {})
    out: dict[str, str] = {}
    if isinstance(primary, dict):
        for role, value in primary.items():
            if isinstance(value, str) and value:
                out[role] = value
    return out


def find_matbin_material(token: str, matbin_manifest: dict[str, Any]) -> tuple[str | None, dict[str, Any] | None]:
    materials = matbin_manifest.get("materials", {})
    if not isinstance(materials, dict):
        return None, None
    candidates = [token] + normalized_token_candidates(token)
    lowered = {str(key).lower(): str(key) for key in materials.keys()}
    for candidate in candidates:
        key = lowered.get(candidate.lower())
        if key:
            return key, materials[key]
    for candidate in candidates:
        c = candidate.lower()
        for lower_key, original_key in lowered.items():
            if lower_key.startswith(c) or c.startswith(lower_key):
                return original_key, materials[original_key]
    return None, None


def load_fxr_texture_groups(path_text: str | None) -> dict[int, dict[str, Any]]:
    if not path_text:
        return {}
    data = json.loads(Path(path_text).read_text(encoding="utf-8"))
    groups = data.get("textureGroups", {})
    if not isinstance(groups, dict):
        return {}
    out: dict[int, dict[str, Any]] = {}
    for key, value in groups.items():
        if not isinstance(value, dict):
            continue
        try:
            resource_id = int(key)
        except ValueError:
            continue
        out[resource_id] = value
    return out


def material_slot_index(material_name: str) -> int | None:
    if match := re.search(r"\[\s*(\d+)\s*\|", material_name):
        return int(match.group(1))
    return None


def p_token_resource_candidates(material_name: str, token: str) -> list[int]:
    match = re.match(r"P\[([^\]]+)\](?:_(.+))?", token)
    if not match:
        return []
    asset_id = match.group(1)
    number_matches = re.findall(r"\d+", asset_id)
    if not number_matches:
        return []

    number_text = number_matches[-1]
    base = int(number_text)
    prefixed_candidates: list[int] = []
    if len(number_text) <= 5:
        prefixed_candidates.append(int("8" + number_text))

    candidates: list[int] = []
    slot = material_slot_index(material_name)
    if slot is not None:
        for prefixed in prefixed_candidates:
            candidates.extend([prefixed + slot // 2, prefixed + slot])
        candidates.extend([base + slot // 2, base + slot])

    for prefixed in prefixed_candidates:
        candidates.extend(prefixed + offset for offset in range(8))
    candidates.extend(base + offset for offset in range(8))

    unique: list[int] = []
    seen = set()
    for candidate in candidates:
        if candidate not in seen:
            unique.append(candidate)
            seen.add(candidate)
    return unique


def bindings_from_fxr_resources(
    material_name: str,
    token: str,
    texture_groups: dict[int, dict[str, Any]],
) -> dict[str, Any]:
    candidates = p_token_resource_candidates(material_name, token)
    for candidate in candidates:
        group = texture_groups.get(candidate)
        if not group:
            continue
        bindings = group.get("bindings", {})
        if isinstance(bindings, dict) and bindings:
            return {
                "resourceId": candidate,
                "stem": group.get("stem"),
                "candidates": candidates,
                "bindings": {str(role): str(path) for role, path in bindings.items()},
            }
    return {"resourceId": None, "stem": None, "candidates": candidates, "bindings": {}}


def build_bindings_command(args: argparse.Namespace) -> int:
    material_records = load_material_records(Path(args.materials_json))
    matbin_manifest = {}
    if args.matbin_manifest:
        matbin_manifest = json.loads(Path(args.matbin_manifest).read_text(encoding="utf-8"))
    texture_index = build_texture_index([Path(p) for p in args.texture_root])
    fxr_texture_groups = load_fxr_texture_groups(args.fxr_resource_manifest)

    records: list[dict[str, Any]] = []
    for material_record in material_records:
        material_name = str(material_record["name"])
        token = extract_material_token(material_name)
        matched_key, matched_matbin = find_matbin_material(token, matbin_manifest)
        bindings: dict[str, str] = {}
        sources: list[str] = []

        if matched_matbin:
            bindings.update(material_primary_from_manifest(matched_matbin))
            sources.append("matbin")

        inspected_bindings = bindings_from_inspected_images(material_record)
        for role, texture_path in inspected_bindings.items():
            if role not in bindings:
                bindings[role] = texture_path
        if inspected_bindings:
            sources.append("fbx-existing")

        fxr_binding = bindings_from_fxr_resources(material_name, token, fxr_texture_groups)
        for role, texture_path in fxr_binding.get("bindings", {}).items():
            if role not in bindings:
                bindings[role] = texture_path
        if fxr_binding.get("bindings"):
            sources.append("fxr-resource")

        stem_candidates = normalized_token_candidates(token)
        for role in ROLE_SUFFIXES:
            if role not in bindings:
                resolved = resolve_role_from_suffix(role, stem_candidates, texture_index)
                if resolved:
                    bindings[role] = resolved
                    if "suffix-fallback" not in sources:
                        sources.append("suffix-fallback")

        records.append(
            {
                "materialName": material_name,
                "token": token,
                "matchedMatbin": matched_key,
                "source": "+".join(sources) if sources else "unresolved",
                "stemCandidates": stem_candidates,
                "inspectedBindings": inspected_bindings,
                "fxrBinding": fxr_binding,
                "bindings": bindings,
            }
        )

    manifest = {
        "schema": "winters.elden.blender_material_bindings.v1",
        "generatedBy": "Tools/EldenAssetPipeline/elden_pipeline.py build-bindings",
        "materialsJson": str(Path(args.materials_json)).replace("\\", "/"),
        "matbinManifest": str(Path(args.matbin_manifest)).replace("\\", "/") if args.matbin_manifest else None,
        "fxrResourceManifest": (
            str(Path(args.fxr_resource_manifest)).replace("\\", "/") if args.fxr_resource_manifest else None
        ),
        "textureRoots": [str(Path(p)).replace("\\", "/") for p in args.texture_root],
        "materials": records,
    }
    write_json(Path(args.out), manifest, args.pretty)
    return 0


def now_utc_iso() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")


def load_json_optional(path: Path) -> Any:
    if not path.exists():
        return None
    return json.loads(path.read_text(encoding="utf-8-sig"))


def path_exists(path_text: str | None) -> bool:
    return bool(path_text) and Path(path_text).exists()


def rel_path(path: Path, root: Path) -> str:
    try:
        return path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def count_files_by_extension(root: Path) -> dict[str, int]:
    counts: Counter[str] = Counter()
    if not root.exists():
        return {}
    for path in root.rglob("*"):
        if path.is_file():
            counts[path.suffix.lower() or "<none>"] += 1
    return dict(sorted(counts.items()))


def infer_texture_role(path: Path) -> tuple[str | None, str]:
    stem = path.stem
    lowered = stem.lower()
    for role, suffixes in TEXTURE_ROLE_SUFFIXES:
        for suffix in suffixes:
            if lowered.endswith(suffix):
                return role, stem[: -len(suffix)]
    return None, stem


def prefer_texture_record(existing: dict[str, Any] | None, candidate: dict[str, Any]) -> dict[str, Any]:
    if existing is None:
        return candidate
    existing_ext = Path(str(existing.get("path", ""))).suffix.lower()
    candidate_ext = Path(str(candidate.get("path", ""))).suffix.lower()
    if existing_ext != ".dds" and candidate_ext == ".dds":
        return candidate
    return existing


def scan_texture_index(resource_root: Path) -> dict[str, Any]:
    texture_files: list[Path] = []
    if resource_root.exists():
        for path in resource_root.rglob("*"):
            if path.is_file() and path.suffix.lower() in SUPPORTED_TEXTURE_EXTS:
                texture_files.append(path)

    role_counts: Counter[str] = Counter()
    ext_counts: Counter[str] = Counter()
    sets: dict[str, dict[str, Any]] = {}
    unclassified: list[dict[str, Any]] = []

    for path in sorted(texture_files):
        ext_counts[path.suffix.lower()] += 1
        role, base_stem = infer_texture_role(path)
        record = {
            "path": rel_path(path, resource_root),
            "bytes": path.stat().st_size,
        }
        if role is None:
            if len(unclassified) < 512:
                unclassified.append(record)
            continue

        role_counts[role] += 1
        set_key = f"{rel_path(path.parent, resource_root)}/{base_stem}".strip("/")
        texture_set = sets.setdefault(
            set_key,
            {
                "id": set_key,
                "baseStem": base_stem,
                "directory": rel_path(path.parent, resource_root),
                "channels": {},
            },
        )
        texture_set["channels"][role] = prefer_texture_record(texture_set["channels"].get(role), record)
        if role == "albedo":
            texture_set["channels"]["diffuse"] = prefer_texture_record(
                texture_set["channels"].get("diffuse"),
                record,
            )

    return {
        "schema": "winters.elden.texture_index.v1",
        "generatedAt": now_utc_iso(),
        "resourceRoot": str(resource_root),
        "counts": {
            "files": len(texture_files),
            "byExtension": dict(sorted(ext_counts.items())),
            "byRole": dict(sorted(role_counts.items())),
            "sets": len(sets),
            "unclassifiedStored": len(unclassified),
        },
        "sets": sorted(sets.values(), key=lambda item: item["id"]),
        "unclassified": unclassified,
    }


def converter_info(converter: Path | None, binary_path: Path) -> dict[str, Any] | None:
    if not converter or not converter.exists() or not binary_path.exists():
        return None
    try:
        completed = subprocess.run(
            [str(converter), "info", str(binary_path)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=10,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as ex:
        return {"ok": False, "error": f"{type(ex).__name__}: {ex}"}

    output = (completed.stdout or "") + (completed.stderr or "")
    info: dict[str, Any] = {
        "ok": completed.returncode == 0,
        "exitCode": completed.returncode,
        "output": output.strip(),
    }
    if match := MODEL_INFO_RE.search(output):
        info.update({key: int(value) for key, value in match.groupdict().items()})
    return info


def collect_binary_files(model_dir: Path, converter: Path | None, runtime_bone_limit: int) -> dict[str, Any]:
    files: dict[str, list[dict[str, Any]]] = {"wmesh": [], "wmat": [], "wskel": [], "wanim": [], "fbx": []}
    max_bones = 0
    if model_dir.exists():
        for path in sorted(model_dir.rglob("*")):
            if not path.is_file():
                continue
            ext = path.suffix.lower().lstrip(".")
            if ext not in files:
                continue
            record: dict[str, Any] = {"path": str(path), "bytes": path.stat().st_size}
            if ext in {"wmesh", "wskel", "wanim"}:
                info = converter_info(converter, path)
                if info:
                    record["info"] = info
                    if ext == "wmesh" and isinstance(info.get("bones"), int):
                        max_bones = max(max_bones, int(info["bones"]))
            files[ext].append(record)

    if not files["wmesh"]:
        runtime_status = "missing-wmesh"
    elif max_bones > runtime_bone_limit:
        runtime_status = "binary-extracted-runtime-bone-limit"
    else:
        runtime_status = "binary-extracted-runtime-ready"

    return {
        "files": files,
        "maxBones": max_bones,
        "runtimeBoneLimit": runtime_bone_limit,
        "runtimeStatus": runtime_status,
    }


def collect_texture_summary(texture_dir: Path) -> dict[str, Any]:
    role_counts: Counter[str] = Counter()
    files: list[dict[str, Any]] = []
    if texture_dir.exists():
        for path in sorted(texture_dir.rglob("*")):
            if not path.is_file() or path.suffix.lower() not in SUPPORTED_TEXTURE_EXTS:
                continue
            role, _ = infer_texture_role(path)
            role_name = role or "unknown"
            role_counts[role_name] += 1
            files.append({"path": str(path), "role": role_name, "bytes": path.stat().st_size})
    return {
        "directory": str(texture_dir),
        "countsByRole": dict(sorted(role_counts.items())),
        "files": files,
    }


def collect_animation_summary(animation_dir: Path, converter: Path | None) -> list[dict[str, Any]]:
    files: list[dict[str, Any]] = []
    if not animation_dir.exists():
        return files
    for path in sorted(animation_dir.rglob("*.wanim")):
        record: dict[str, Any] = {"path": str(path), "bytes": path.stat().st_size}
        info = converter_info(converter, path)
        if info:
            record["info"] = info
        files.append(record)
    return files


def collect_raw_animation_summary(animation_raw_dir: Path) -> list[dict[str, Any]]:
    files: list[dict[str, Any]] = []
    if not animation_raw_dir.exists():
        return files
    for path in sorted(animation_raw_dir.rglob("*")):
        if not path.is_file() or path.suffix.lower() not in {".hkx", ".hkxpwv", ".tae"}:
            continue
        files.append({"path": str(path), "bytes": path.stat().st_size, "extension": path.suffix.lower()})
    return files


def asset_record_from_dir(
    asset_dir: Path,
    kind: str,
    converter: Path | None,
    runtime_bone_limit: int,
    manifest_record: dict[str, Any] | None = None,
) -> dict[str, Any]:
    model_dir = asset_dir / "Model"
    texture_dir = asset_dir / "Texture"
    animation_dir = asset_dir / "Animation"
    animation_raw_dir = asset_dir / "AnimationRaw"
    binary = collect_binary_files(model_dir, converter, runtime_bone_limit)
    binary["files"]["wanim"].extend(collect_animation_summary(animation_dir, converter))
    textures = collect_texture_summary(texture_dir)
    animation_raw = collect_raw_animation_summary(animation_raw_dir)
    if not binary["files"]["wmesh"]:
        if textures["files"]:
            binary["runtimeStatus"] = "texture-only"
        elif animation_raw:
            binary["runtimeStatus"] = "raw-animation-collected"
    record: dict[str, Any] = {
        "id": asset_dir.name,
        "kind": kind,
        "root": str(asset_dir),
        "modelDir": str(model_dir),
        "textureDir": str(texture_dir),
        "animationDir": str(animation_dir),
        "animationRawDir": str(animation_raw_dir),
        "binary": binary,
        "textures": textures,
        "animationRaw": animation_raw,
    }
    if manifest_record:
        record["manifest"] = manifest_record
    return record


def collect_manifest_records(manifest: Any, key: str = "id") -> dict[str, dict[str, Any]]:
    records: dict[str, dict[str, Any]] = {}
    if not isinstance(manifest, dict):
        return records
    cooked = manifest.get("cooked", [])
    if not isinstance(cooked, list):
        return records
    for item in cooked:
        if not isinstance(item, dict):
            continue
        value = item.get(key) or item.get("folder")
        if value:
            records[str(value)] = item
    return records


def collect_asset_dirs(
    root: Path,
    kind: str,
    converter: Path | None,
    runtime_bone_limit: int,
    manifest_by_dir: dict[str, dict[str, Any]] | None = None,
) -> list[dict[str, Any]]:
    if not root.exists():
        return []
    records: list[dict[str, Any]] = []
    for asset_dir in sorted(path for path in root.iterdir() if path.is_dir()):
        manifest_record = None
        if manifest_by_dir:
            manifest_record = manifest_by_dir.get(asset_dir.name)
            if manifest_record is None:
                manifest_record = manifest_by_dir.get(asset_dir.name.split("_", 1)[0])
        records.append(
            asset_record_from_dir(asset_dir, kind, converter, runtime_bone_limit, manifest_record)
        )
    return records


def collect_full_game_assets(
    resource_root: Path,
    converter: Path | None,
    runtime_bone_limit: int,
) -> list[dict[str, Any]]:
    full_game_root = resource_root / "FullGame"
    if not full_game_root.exists():
        return []
    records: list[dict[str, Any]] = []
    for top_dir in sorted(path for path in full_game_root.iterdir() if path.is_dir()):
        for bundle_dir in sorted(path for path in top_dir.iterdir() if path.is_dir()):
            for asset_dir in sorted(path for path in bundle_dir.iterdir() if path.is_dir()):
                manifest = load_json_optional(asset_dir / "manifest.json")
                record = asset_record_from_dir(
                    asset_dir,
                    f"FullGame/{top_dir.name}/{bundle_dir.name}",
                    converter,
                    runtime_bone_limit,
                    manifest if isinstance(manifest, dict) else None,
                )
                record["topDir"] = top_dir.name
                record["bundleKind"] = bundle_dir.name
                records.append(record)
    return records


def collect_maps(resource_root: Path) -> list[dict[str, Any]]:
    maps_root = resource_root / "Maps"
    maps: list[dict[str, Any]] = []
    if not maps_root.exists():
        return maps
    for assembly_path in sorted(maps_root.rglob("map_assembly.json")):
        data = load_json_optional(assembly_path) or {}
        notes = data.get("notes", []) if isinstance(data, dict) else []
        maps.append(
            {
                "path": str(assembly_path),
                "mapId": data.get("mapId"),
                "area": data.get("area"),
                "sourceMsbXml": data.get("sourceMsbXml"),
                "counts": {
                    "mapPieces": len(data.get("mapPieces", []) or []),
                    "collisions": len(data.get("collisions", []) or []),
                    "enemyIds": len(data.get("enemyIds", []) or []),
                    "assetIds": len(data.get("assetIds", []) or []),
                },
                "exactTransformsAvailable": not any(
                    "transform" in str(note).lower() and "require" in str(note).lower()
                    for note in notes
                ),
                "editorSeeds": {
                    "worldPartition": str(assembly_path.with_name("world_partition_seed.json"))
                    if assembly_path.with_name("world_partition_seed.json").exists()
                    else None,
                    "sequencer": str(assembly_path.with_name("sequencer_seed.json"))
                    if assembly_path.with_name("sequencer_seed.json").exists()
                    else None,
                    "editorMap": str(assembly_path.with_name("editor_map_seed.json"))
                    if assembly_path.with_name("editor_map_seed.json").exists()
                    else None,
                },
                "notes": notes,
            }
        )
    return maps


def collect_ui(resource_root: Path, game_root: Path | None) -> dict[str, Any]:
    ui_root = resource_root / "UI"
    main_menu_manifest = load_json_optional(ui_root / "MainMenu" / "manifest.json")
    menu_raw_manifest = load_json_optional(ui_root / "MenuRaw" / "manifest.json")
    source_menu_root = game_root / "menu" if game_root else None
    return {
        "resourceRoot": str(ui_root),
        "resourceCounts": count_files_by_extension(ui_root),
        "mainMenu": main_menu_manifest,
        "menuRaw": menu_raw_manifest,
        "sourceMenuRoot": str(source_menu_root) if source_menu_root else None,
        "sourceMenuCounts": count_files_by_extension(source_menu_root) if source_menu_root else {},
    }


def collect_source_bundles(resource_root: Path) -> dict[str, Any] | None:
    manifest_path = resource_root / "SourceBundles" / "manifest.json"
    manifest = load_json_optional(manifest_path)
    if not isinstance(manifest, dict):
        return None
    return {
        "manifestPath": str(manifest_path),
        "schema": manifest.get("schema"),
        "generatedAt": manifest.get("generatedAt"),
        "roots": manifest.get("roots"),
        "targetProfile": manifest.get("targetProfile"),
        "counts": manifest.get("counts"),
        "notes": manifest.get("notes", []),
    }


def collect_texture_conversion(resource_root: Path) -> dict[str, Any] | None:
    manifest_path = resource_root / "Manifests" / "eldenring_texture_dds_conversion.json"
    manifest = load_json_optional(manifest_path)
    if not isinstance(manifest, dict):
        return None
    return {
        "manifestPath": str(manifest_path),
        "schema": manifest.get("schema"),
        "generatedAt": manifest.get("generatedAt"),
        "textureRoot": manifest.get("textureRoot"),
        "texconv": manifest.get("texconv"),
        "counts": manifest.get("counts"),
    }


def collect_game_inventory(resource_root: Path) -> dict[str, Any] | None:
    manifest_path = resource_root / "Manifests" / "eldenring_game_inventory.json"
    manifest = load_json_optional(manifest_path)
    if not isinstance(manifest, dict):
        return None
    return {
        "manifestPath": str(manifest_path),
        "schema": manifest.get("schema"),
        "generatedAt": manifest.get("generatedAt"),
        "gameRoot": manifest.get("gameRoot"),
        "counts": manifest.get("counts"),
        "diskPolicy": manifest.get("diskPolicy"),
        "queuePath": manifest.get("queuePath"),
    }


def collect_source_state(game_root: Path | None, witchy_root: Path | None, work_root: Path | None) -> dict[str, Any]:
    loose_dirs = {}
    if game_root and game_root.exists():
        for name in ("action", "asset", "chr", "map", "menu", "material", "sfx"):
            loose_dirs[name] = (game_root / name).exists()

    return {
        "gameRoot": str(game_root) if game_root else None,
        "gameLooseDirs": loose_dirs,
        "witchyRoot": str(witchy_root) if witchy_root else None,
        "witchyCounts": count_files_by_extension(witchy_root) if witchy_root else {},
        "workRoot": str(work_root) if work_root else None,
        "workCounts": count_files_by_extension(work_root) if work_root else {},
    }


def build_resource_catalog_command(args: argparse.Namespace) -> int:
    resource_root = Path(args.resource_root)
    manifest_root = resource_root / "Manifests"
    converter = Path(args.converter) if args.converter else None
    game_root = Path(args.game_root) if args.game_root else None
    witchy_root = Path(args.witchy_root) if args.witchy_root else None
    work_root = Path(args.work_root) if args.work_root else None

    pipeline_status = load_json_optional(manifest_root / "eldenring_pipeline_status.json")
    static_manifest = load_json_optional(manifest_root / "limgrave_static_assets.json")
    character_manifest = load_json_optional(manifest_root / "limgrave_characters_static.json")

    static_by_id = collect_manifest_records(static_manifest)
    character_by_folder = collect_manifest_records(character_manifest, "folder")

    limgrave_static = collect_asset_dirs(
        resource_root / "Assets" / "LimgraveStatic",
        "LimgraveStatic",
        converter,
        args.runtime_bone_limit,
        static_by_id,
    )
    other_assets = collect_asset_dirs(
        resource_root / "Assets",
        "Asset",
        converter,
        args.runtime_bone_limit,
        {},
    )
    other_assets = [item for item in other_assets if not item["id"].startswith("LimgraveStatic")]
    characters = collect_asset_dirs(
        resource_root / "Characters",
        "Character",
        converter,
        args.runtime_bone_limit,
        character_by_folder,
    )
    full_game_assets = collect_full_game_assets(resource_root, converter, args.runtime_bone_limit)
    texture_index = scan_texture_index(resource_root)

    all_asset_records = limgrave_static + other_assets + characters + full_game_assets
    runtime_counts = Counter(
        item.get("binary", {}).get("runtimeStatus", "unknown") for item in all_asset_records
    )
    full_game_runtime_counts = Counter(
        item.get("binary", {}).get("runtimeStatus", "unknown") for item in full_game_assets
    )
    maps = collect_maps(resource_root)
    source_bundles = collect_source_bundles(resource_root)
    texture_conversion = collect_texture_conversion(resource_root)
    game_inventory = collect_game_inventory(resource_root)

    catalog = {
        "schema": "winters.elden.asset_catalog.v1",
        "generatedAt": now_utc_iso(),
        "generatedBy": "Tools/EldenAssetPipeline/elden_pipeline.py build-resource-catalog",
        "roots": {
            "resourceRoot": str(resource_root),
            "manifestRoot": str(manifest_root),
            "gameRoot": str(game_root) if game_root else None,
            "witchyRoot": str(witchy_root) if witchy_root else None,
            "workRoot": str(work_root) if work_root else None,
        },
        "sourceState": collect_source_state(game_root, witchy_root, work_root),
        "pipelineStatus": pipeline_status,
        "summaries": {
            "resourceFileCounts": count_files_by_extension(resource_root),
            "runtimeStatusCounts": dict(sorted(runtime_counts.items())),
            "textureCounts": texture_index["counts"],
            "limgraveStaticCount": len(limgrave_static),
            "otherAssetCount": len(other_assets),
            "characterCount": len(characters),
            "fullGameAssetCount": len(full_game_assets),
            "fullGameRuntimeStatusCounts": dict(sorted(full_game_runtime_counts.items())),
            "mapCount": len(maps),
            "sourceBundleCount": (
                source_bundles.get("counts", {}).get("totalTrackedFiles", 0)
                if isinstance(source_bundles, dict)
                else 0
            ),
        },
        "maps": maps,
        "ui": collect_ui(resource_root, game_root),
        "sourceBundles": source_bundles,
        "fullGameInventory": game_inventory,
        "textureConversion": texture_conversion,
        "assets": {
            "limgraveStatic": limgrave_static,
            "other": other_assets,
        },
        "characters": characters,
        "fullGameAssets": full_game_assets,
        "textureIndexPath": str(Path(args.texture_out)) if args.texture_out else None,
        "knownBlockers": [
            "Map assembly currently has model/part IDs; exact transforms still require a fuller MSB parser pass.",
            "High-bone character WMesh binaries are extracted, but runtime CWMeshLoader and shader skinning limits still need expansion.",
            "Material channels are indexed by filename suffix now; MATBIN-driven exact binding remains a follow-up resolver step.",
            "HKX/TAE animation binders are collected as raw animation sources; direct HKX/TAE to .wanim conversion still needs a dedicated converter.",
        ],
    }

    write_json(Path(args.out), catalog, args.pretty)
    if args.texture_out:
        write_json(Path(args.texture_out), texture_index, args.pretty)
    return 0


def sync_ui_menu_command(args: argparse.Namespace) -> int:
    source_root = Path(args.source_menu_root)
    output_root = Path(args.output_root)
    if not source_root.exists():
        raise FileNotFoundError(f"source menu root not found: {source_root}")

    allowed_exts = {ext.lower() for ext in args.ext}
    copied: list[dict[str, Any]] = []
    skipped: list[str] = []
    for source_path in sorted(source_root.rglob("*")):
        if not source_path.is_file():
            continue
        if source_path.suffix.lower() not in allowed_exts:
            skipped.append(str(source_path))
            continue

        relative = source_path.relative_to(source_root)
        output_path = output_root / relative
        output_path.parent.mkdir(parents=True, exist_ok=True)
        should_copy = args.force or not output_path.exists()
        if not should_copy and source_path.stat().st_mtime > output_path.stat().st_mtime:
            should_copy = True
        if should_copy:
            shutil.copy2(source_path, output_path)
        copied.append(
            {
                "source": str(source_path),
                "output": str(output_path),
                "relative": relative.as_posix(),
                "bytes": output_path.stat().st_size if output_path.exists() else 0,
                "copied": should_copy,
            }
        )

    manifest = {
        "schema": "winters.elden.ui_raw_menu.v1",
        "generatedAt": now_utc_iso(),
        "generatedBy": "Tools/EldenAssetPipeline/elden_pipeline.py sync-ui-menu",
        "sourceMenuRoot": str(source_root),
        "outputRoot": str(output_root),
        "allowedExtensions": sorted(allowed_exts),
        "counts": {
            "sourceByExtension": count_files_by_extension(source_root),
            "outputByExtension": count_files_by_extension(output_root),
            "trackedFiles": len(copied),
            "copiedThisRun": sum(1 for item in copied if item["copied"]),
            "skippedByExtension": len(skipped),
        },
        "files": copied,
    }
    write_json(Path(args.out), manifest, args.pretty)
    return 0


def should_copy_source(source_path: Path, output_path: Path, force: bool) -> bool:
    if force or not output_path.exists():
        return True
    source_stat = source_path.stat()
    output_stat = output_path.stat()
    return source_stat.st_size != output_stat.st_size or source_stat.st_mtime > output_stat.st_mtime


def copy_source_bundle_file(
    source_path: Path,
    source_root: Path,
    game_root: Path,
    output_root: Path,
    group: str,
    force: bool,
    metadata: dict[str, Any] | None = None,
) -> dict[str, Any]:
    relative = source_path.relative_to(source_root)
    output_path = output_root / group / relative
    output_path.parent.mkdir(parents=True, exist_ok=True)

    copied = should_copy_source(source_path, output_path, force)
    if copied:
        shutil.copy2(source_path, output_path)

    record: dict[str, Any] = {
        "group": group,
        "source": str(source_path),
        "output": str(output_path),
        "relative": relative.as_posix(),
        "gameRelative": rel_path(source_path, game_root),
        "bytes": output_path.stat().st_size if output_path.exists() else source_path.stat().st_size,
        "copied": copied,
    }
    if metadata:
        record.update(metadata)
    return record


def unique_existing_files(paths: list[Path]) -> tuple[list[Path], list[Path]]:
    seen: set[str] = set()
    existing: list[Path] = []
    missing: list[Path] = []
    for path in paths:
        key = str(path.resolve()).lower()
        if key in seen:
            continue
        seen.add(key)
        if path.is_file():
            existing.append(path)
        else:
            missing.append(path)
    return existing, missing


def collect_map_source_files(game_root: Path, map_tile: str) -> tuple[Path, list[Path]]:
    area = map_tile.rsplit("_", 3)[0]
    map_root = game_root / "map" / area / map_tile
    if not map_root.exists():
        return map_root, []
    return map_root, sorted(path for path in map_root.iterdir() if path.is_file())


def collect_character_source_files(game_root: Path, character_ids: list[str]) -> tuple[Path, list[Path]]:
    chr_root = game_root / "chr"
    files: list[Path] = []
    for character_id in character_ids:
        files.extend(sorted(path for path in chr_root.glob(f"{character_id}*") if path.is_file()))
    return chr_root, files


def collect_parts_source_files(game_root: Path) -> tuple[Path, list[Path]]:
    parts_root = game_root / "parts"
    if not parts_root.exists():
        return parts_root, []
    return parts_root, sorted(path for path in parts_root.iterdir() if path.is_file())


def aeg_bundle_path(game_root: Path, asset_id: str) -> Path:
    family = asset_id[:6].lower()
    stem = asset_id.lower()
    return game_root / "asset" / "aeg" / family / f"{stem}.geombnd.dcx"


def collect_limgrave_static_source_files(
    game_root: Path,
    static_manifest_path: Path | None,
) -> tuple[Path, list[Path], list[str]]:
    asset_root = game_root / "asset" / "aeg"
    if not static_manifest_path or not static_manifest_path.exists():
        return asset_root, [], []

    manifest = load_json_optional(static_manifest_path) or {}
    asset_ids: set[str] = set()
    for item in manifest.get("cooked", []) if isinstance(manifest, dict) else []:
        if not isinstance(item, dict):
            continue
        for key in ("id", "source_flver", "fbx", "wmesh"):
            value = item.get(key)
            if not isinstance(value, str):
                continue
            for match in AEG_ASSET_ID_RE.findall(value):
                asset_ids.add(match.upper())

    paths = [aeg_bundle_path(game_root, asset_id) for asset_id in sorted(asset_ids)]
    return asset_root, paths, sorted(asset_ids)


def summarize_group(records: list[dict[str, Any]]) -> dict[str, Any]:
    return {
        "trackedFiles": len(records),
        "copiedThisRun": sum(1 for item in records if item.get("copied")),
        "bytes": sum(int(item.get("bytes", 0)) for item in records),
        "byExtension": dict(sorted(Counter(Path(item["output"]).suffix.lower() for item in records).items())),
    }


def sync_source_bundles_command(args: argparse.Namespace) -> int:
    game_root = Path(args.game_root)
    output_root = Path(args.output_root)
    if not game_root.exists():
        raise FileNotFoundError(f"game root not found: {game_root}")

    requested_ids = [item.lower() for item in (args.character_id or [])]
    character_targets = dict(DEFAULT_SOURCE_CHARACTER_TARGETS)
    for character_id in requested_ids:
        character_targets.setdefault(
            character_id,
            {"label": "ManualCharacter", "reason": "manual-character-id", "confidence": "manual"},
        )
    character_ids = sorted(character_targets)

    files: list[dict[str, Any]] = []
    missing: list[dict[str, Any]] = []
    group_records: dict[str, list[dict[str, Any]]] = {}

    def add_group(
        group: str,
        source_root: Path,
        source_files: list[Path],
        metadata_by_name: dict[str, dict[str, Any]] | None = None,
    ) -> None:
        existing, missing_paths = unique_existing_files(source_files)
        for source_path in existing:
            metadata = None
            if metadata_by_name:
                metadata = metadata_by_name.get(source_path.name.lower())
            record = copy_source_bundle_file(
                source_path,
                source_root,
                game_root,
                output_root,
                group,
                args.force,
                metadata,
            )
            files.append(record)
            group_records.setdefault(group, []).append(record)
        for path in missing_paths:
            missing.append({"group": group, "source": str(path), "gameRelative": rel_path(path, game_root)})

    map_root, map_files = collect_map_source_files(game_root, args.map_tile)
    add_group("limgrave_map", map_root, map_files)

    chr_root, chr_files = collect_character_source_files(game_root, character_ids)
    metadata_by_name: dict[str, dict[str, Any]] = {}
    for source_path in chr_files:
        character_id = source_path.name.split(".", 1)[0].split("_", 1)[0].lower()
        metadata_by_name[source_path.name.lower()] = {
            "characterId": character_id,
            "target": character_targets.get(character_id, {}),
        }
    add_group("limgrave_characters", chr_root, chr_files, metadata_by_name)

    static_manifest_path = Path(args.static_manifest) if args.static_manifest else None
    asset_root, asset_files, static_asset_ids = collect_limgrave_static_source_files(
        game_root,
        static_manifest_path,
    )
    asset_metadata = {
        path.name.lower(): {"assetId": path.stem.split(".", 1)[0].upper(), "source": "limgrave-static-manifest"}
        for path in asset_files
    }
    add_group("limgrave_static_raw", asset_root, asset_files, asset_metadata)

    if args.include_parts:
        parts_root, parts_files = collect_parts_source_files(game_root)
        add_group("parts_items_equipment", parts_root, parts_files)

    manifest = {
        "schema": "winters.elden.source_bundles.v1",
        "generatedAt": now_utc_iso(),
        "generatedBy": "Tools/EldenAssetPipeline/elden_pipeline.py sync-source-bundles",
        "roots": {
            "gameRoot": str(game_root),
            "outputRoot": str(output_root),
            "staticManifest": str(static_manifest_path) if static_manifest_path else None,
        },
        "targetProfile": {
            "mapTile": args.map_tile,
            "characterTargets": character_targets,
            "includeParts": bool(args.include_parts),
            "staticAssetIds": static_asset_ids,
        },
        "externalResourceManifests": {
            "uiMenuRaw": str(Path(args.ui_menu_manifest)) if args.ui_menu_manifest else None,
            "assetCatalog": str(Path(args.asset_catalog)) if args.asset_catalog else None,
        },
        "counts": {
            "groups": {group: summarize_group(records) for group, records in sorted(group_records.items())},
            "totalTrackedFiles": len(files),
            "totalCopiedThisRun": sum(1 for item in files if item.get("copied")),
            "totalBytes": sum(int(item.get("bytes", 0)) for item in files),
            "missingFiles": len(missing),
        },
        "notes": [
            "SourceBundles stores UXM-loose source binders for repeatable extraction and recook passes.",
            "UI menu raw files are tracked through UI/MenuRaw to avoid duplicating the large menu source tree.",
            "Character names marked as candidate need a later param/name resolver pass before designer-facing labels are final.",
        ],
        "missing": missing,
        "files": files,
    }
    write_json(Path(args.out), manifest, args.pretty)
    return 0 if not missing else 2


def dds_format_for_role(role: str | None) -> str:
    if role in {"albedo", "emissive"}:
        return "BC7_UNORM_SRGB"
    return "BC7_UNORM"


def convert_textures_dds_command(args: argparse.Namespace) -> int:
    texture_root = Path(args.texture_root)
    texconv = Path(args.texconv)
    if not texture_root.exists():
        raise FileNotFoundError(f"texture root not found: {texture_root}")
    if not texconv.exists():
        raise FileNotFoundError(f"texconv not found: {texconv}")

    png_files = sorted(path for path in texture_root.rglob("*.png") if path.is_file())
    if args.limit > 0:
        png_files = png_files[: args.limit]

    records: list[dict[str, Any]] = []
    for source_path in png_files:
        output_path = source_path.with_suffix(".dds")
        role, base_stem = infer_texture_role(source_path)
        texture_format = args.format or dds_format_for_role(role)

        should_convert = args.force or not output_path.exists()
        if not should_convert and source_path.stat().st_mtime > output_path.stat().st_mtime:
            should_convert = True

        command = [
            str(texconv),
            "-nologo",
            "-y",
            "-m",
            str(args.mip_levels),
            "-f",
            texture_format,
            "-ft",
            "dds",
            "-o",
            str(source_path.parent),
            str(source_path),
        ]

        result: dict[str, Any] | None = None
        if should_convert:
            result = run_process(command, args.timeout)

        ok = output_path.exists() and output_path.stat().st_size > 0
        records.append(
            {
                "source": str(source_path),
                "output": str(output_path),
                "relative": rel_path(source_path, texture_root),
                "role": role or "unknown",
                "baseStem": base_stem,
                "format": texture_format,
                "converted": should_convert,
                "ok": ok and (result is None or bool(result.get("ok"))),
                "sourceBytes": source_path.stat().st_size,
                "outputBytes": output_path.stat().st_size if output_path.exists() else 0,
                "exitCode": result.get("exitCode") if result else None,
                "stderr": (result.get("stderr") or "").strip()[-2000:] if result else "",
                "stdout": (result.get("stdout") or "").strip()[-2000:] if result else "",
            }
        )

    role_counts = Counter(record["role"] for record in records)
    format_counts = Counter(record["format"] for record in records)
    manifest = {
        "schema": "winters.elden.texture_dds_conversion.v1",
        "generatedAt": now_utc_iso(),
        "generatedBy": "Tools/EldenAssetPipeline/elden_pipeline.py convert-textures-dds",
        "textureRoot": str(texture_root),
        "texconv": str(texconv),
        "counts": {
            "pngInputs": len(records),
            "convertedThisRun": sum(1 for record in records if record["converted"]),
            "ok": sum(1 for record in records if record["ok"]),
            "failed": sum(1 for record in records if not record["ok"]),
            "byRole": dict(sorted(role_counts.items())),
            "byFormat": dict(sorted(format_counts.items())),
        },
        "records": records,
    }
    write_json(Path(args.out), manifest, args.pretty)
    return 0 if manifest["counts"]["failed"] == 0 else 2


def count_records_by_type(records: list[Any]) -> dict[str, int]:
    counts: Counter[str] = Counter()
    for record in records:
        if isinstance(record, dict):
            counts[str(record.get("type") or "unknown")] += 1
    return dict(sorted(counts.items()))


def build_editor_map_seeds_command(args: argparse.Namespace) -> int:
    map_assembly_path = Path(args.map_assembly)
    map_assembly = load_json_optional(map_assembly_path)
    if not isinstance(map_assembly, dict):
        raise ValueError(f"map assembly must be a JSON object: {map_assembly_path}")

    output_dir = Path(args.out_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    events = map_assembly.get("events", [])
    regions = map_assembly.get("regions", [])
    if not isinstance(events, list):
        events = []
    if not isinstance(regions, list):
        regions = []

    map_id = str(map_assembly.get("mapId") or map_assembly_path.parent.name)
    area = str(map_assembly.get("area") or "")
    source_ref = str(map_assembly_path)
    common = {
        "mapId": map_id,
        "area": area,
        "sourceMapAssembly": source_ref,
        "sourceMsbXml": map_assembly.get("sourceMsbXml"),
        "generatedAt": now_utc_iso(),
    }

    world_partition_path = output_dir / "world_partition_seed.json"
    sequencer_path = output_dir / "sequencer_seed.json"
    editor_map_path = output_dir / "editor_map_seed.json"

    world_partition = {
        "schema": "winters.elden.world_partition_seed.v1",
        **common,
        "exactTransformsAvailable": False,
        "cells": [
            {
                "cellId": map_id,
                "tile": map_id,
                "mapPieces": map_assembly.get("mapPieces", []),
                "collisions": map_assembly.get("collisions", []),
                "enemyIds": map_assembly.get("enemyIds", []),
                "assetIds": map_assembly.get("assetIds", []),
                "partCounts": map_assembly.get("partCounts", []),
                "streamingPolicy": {
                    "state": "seed",
                    "loadRadiusCells": 1,
                    "sourceBundleGroup": "limgrave_map",
                },
            }
        ],
        "notes": [
            "This seed is tile-level until the MSB parser provides exact transforms.",
            "Editor should hydrate model references from eldenring_asset_catalog.json and SourceBundles.",
        ],
    }

    tracks: list[dict[str, Any]] = []
    for index, event in enumerate(events):
        if not isinstance(event, dict):
            continue
        tracks.append(
            {
                "trackId": f"event_{index:04d}",
                "trackKind": "Event",
                "type": event.get("type") or "unknown",
                "name": event.get("name") or "",
            }
        )
    for index, region in enumerate(regions):
        if not isinstance(region, dict):
            continue
        region_type = str(region.get("type") or "unknown")
        if region_type not in {"PatrolRoute", "SpawnPoint", "Message", "Sound"}:
            continue
        tracks.append(
            {
                "trackId": f"region_{index:04d}",
                "trackKind": region_type,
                "type": region_type,
                "name": region.get("name") or "",
            }
        )

    sequencer = {
        "schema": "winters.elden.sequencer_seed.v1",
        **common,
        "counts": {
            "eventsByType": count_records_by_type(events),
            "regionsByType": count_records_by_type(regions),
            "tracks": len(tracks),
        },
        "tracks": tracks,
        "notes": [
            "Tracks are name/type seeds extracted from WitchyBND MSB XML summaries.",
            "Precise keys, timing, and transforms require a fuller MSB event/region parser.",
        ],
    }

    editor_map = {
        "schema": "winters.elden.editor_map_seed.v1",
        **common,
        "assetCatalog": str(Path(args.asset_catalog)) if args.asset_catalog else None,
        "sourceBundles": str(Path(args.source_bundles)) if args.source_bundles else None,
        "worldPartitionSeed": str(world_partition_path),
        "sequencerSeed": str(sequencer_path),
        "contentRoots": {
            "map": "Maps/Limgrave/m60_42_36_00",
            "characters": "Characters",
            "staticAssets": "Assets/LimgraveStatic",
            "ui": "UI",
        },
        "notes": [
            "Editor map seed links extracted map assembly, source bundles, cooked models, DDS texture index, and event tracks.",
            "Use this as the initial Content Browser / World Partition / Sequencer entry point.",
        ],
    }

    write_json(world_partition_path, world_partition, args.pretty)
    write_json(sequencer_path, sequencer, args.pretty)
    write_json(editor_map_path, editor_map, args.pretty)
    return 0


def game_top_dir(relative: Path) -> str:
    return relative.parts[0] if relative.parts else "<root>"


def classify_game_bundle(path: Path) -> str:
    lowered = path.name.lower()
    for bundle_kind, suffix in BUNDLE_SUFFIXES:
        if lowered.endswith(suffix):
            return bundle_kind
    if path.suffix.lower() in {".bhd", ".bdt", ".tpfbhd", ".tpfbdt", ".hkxbhd", ".hkxbdt"}:
        return "paired-data"
    if path.suffix.lower() in {".wem", ".bnk", ".pck"}:
        return "audio"
    if path.suffix.lower() in {".bk2"}:
        return "movie"
    if path.suffix.lower() in {".gfx"}:
        return "ui-gfx"
    if path.suffix.lower() in {".hks"}:
        return "script"
    return path.suffix.lower().lstrip(".") or "unknown"


def pipeline_action_for_game_file(path: Path, bundle_kind: str) -> str:
    if path.suffix.lower() == ".dcx":
        return "witchy-unpack"
    if bundle_kind == "paired-data":
        return "paired-raw-reference"
    if bundle_kind in {"audio", "movie", "ui-gfx", "script"}:
        return "raw-reference"
    return "inventory-only"


def build_pipeline_queue(files: list[dict[str, Any]]) -> dict[str, Any]:
    queue_records: list[dict[str, Any]] = []
    action_counts: Counter[str] = Counter()
    for item in files:
        action = item["pipelineAction"]
        action_counts[action] += 1
        if action not in {"witchy-unpack", "paired-raw-reference", "raw-reference"}:
            continue
        priority = PIPELINE_PRIORITY_BY_TOP_DIR.get(str(item["topDir"]).lower(), 500)
        if action != "witchy-unpack":
            priority += 1000
        queue_records.append(
            {
                "priority": priority,
                "action": action,
                "topDir": item["topDir"],
                "bundleKind": item["bundleKind"],
                "relative": item["relative"],
                "bytes": item["bytes"],
            }
        )

    queue_records.sort(key=lambda record: (record["priority"], record["topDir"], record["relative"]))
    return {
        "counts": {
            "actions": dict(sorted(action_counts.items())),
            "queued": len(queue_records),
        },
        "records": queue_records,
    }


def index_game_root_command(args: argparse.Namespace) -> int:
    game_root = Path(args.game_root)
    resource_root = Path(args.resource_root) if args.resource_root else None
    if not game_root.exists():
        raise FileNotFoundError(f"game root not found: {game_root}")

    files: list[dict[str, Any]] = []
    ext_counts: Counter[str] = Counter()
    top_counts: dict[str, Counter[str]] = {}
    top_bytes: Counter[str] = Counter()
    bundle_counts: Counter[str] = Counter()
    action_counts: Counter[str] = Counter()
    total_bytes = 0

    for path in sorted(p for p in game_root.rglob("*") if p.is_file()):
        relative_path = path.relative_to(game_root)
        top_dir = game_top_dir(relative_path)
        ext = path.suffix.lower() or "<none>"
        bundle_kind = classify_game_bundle(path)
        pipeline_action = pipeline_action_for_game_file(path, bundle_kind)
        size = path.stat().st_size
        total_bytes += size
        ext_counts[ext] += 1
        top_counts.setdefault(top_dir, Counter())[ext] += 1
        top_bytes[top_dir] += size
        bundle_counts[bundle_kind] += 1
        action_counts[pipeline_action] += 1
        files.append(
            {
                "relative": relative_path.as_posix(),
                "topDir": top_dir,
                "extension": ext,
                "bundleKind": bundle_kind,
                "pipelineAction": pipeline_action,
                "bytes": size,
                "mtimeUtc": datetime.fromtimestamp(path.stat().st_mtime, timezone.utc)
                .isoformat(timespec="seconds")
                .replace("+00:00", "Z"),
            }
        )

    queue = build_pipeline_queue(files)
    queue_path = Path(args.queue_out) if args.queue_out else None
    if queue_path:
        queue_manifest = {
            "schema": "winters.elden.full_game_extraction_queue.v1",
            "generatedAt": now_utc_iso(),
            "generatedBy": "Tools/EldenAssetPipeline/elden_pipeline.py index-game-root",
            "gameRoot": str(game_root),
            "counts": queue["counts"],
            "records": queue["records"],
            "notes": [
                "Queue is reference-only; source files remain in the UXM loose Game root.",
                "Process witchy-unpack records in priority order, then feed extracted FLVER/TPF/MATBIN/FXR into the existing conversion passes.",
            ],
        }
        write_json(queue_path, queue_manifest, args.pretty)

    inventory = {
        "schema": "winters.elden.full_game_inventory.v1",
        "generatedAt": now_utc_iso(),
        "generatedBy": "Tools/EldenAssetPipeline/elden_pipeline.py index-game-root",
        "gameRoot": str(game_root),
        "resourceRoot": str(resource_root) if resource_root else None,
        "queuePath": str(queue_path) if queue_path else None,
        "diskPolicy": {
            "mode": "reference-only",
            "reason": "Full Game source is large; keep UXM loose Game folder as source-of-truth and avoid duplicate copies.",
            "sourceBytes": total_bytes,
        },
        "counts": {
            "files": len(files),
            "bytes": total_bytes,
            "byExtension": dict(sorted(ext_counts.items())),
            "byTopDir": {
                top_dir: {
                    "files": sum(counter.values()),
                    "bytes": top_bytes[top_dir],
                    "byExtension": dict(sorted(counter.items())),
                }
                for top_dir, counter in sorted(top_counts.items())
            },
            "byBundleKind": dict(sorted(bundle_counts.items())),
            "byPipelineAction": dict(sorted(action_counts.items())),
            "queue": queue["counts"],
        },
        "pipelineStages": [
            "UXM loose Game root",
            "index-game-root reference inventory",
            "WitchyBND unpack queue",
            "MATBIN/FXR/material binding manifests",
            "Blender normalized FBX export",
            "WintersAssetConverter .wmesh/.wmat/.wskel/.wanim",
            "texconv DDS bake",
            "build-resource-catalog editor/runtime catalog",
        ],
        "files": files,
    }
    write_json(Path(args.out), inventory, args.pretty)
    return 0


def safe_filename(text: str) -> str:
    cleaned = SAFE_FILENAME_RE.sub("_", text).strip("._")
    return cleaned or "asset"


def short_stable_hash(text: str) -> str:
    return hashlib.sha1(text.encode("utf-8", errors="replace")).hexdigest()[:10]


def run_process(command: list[str], timeout_seconds: int) -> dict[str, Any]:
    # CREATE_NO_WINDOW gives console children (WitchyBND/PromptPlus) a valid
    # hidden console even when the pipeline runs detached without one;
    # otherwise WitchyBND dies in set_CursorVisible with an invalid handle.
    creationflags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    try:
        completed = subprocess.run(
            command,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout_seconds,
            check=False,
            creationflags=creationflags,
        )
        return {
            "ok": completed.returncode == 0,
            "exitCode": completed.returncode,
            "stdout": completed.stdout,
            "stderr": completed.stderr,
        }
    except subprocess.TimeoutExpired as ex:
        return {
            "ok": False,
            "exitCode": None,
            "stdout": ex.stdout or "",
            "stderr": ex.stderr or "",
            "error": f"TimeoutExpired: {ex}",
        }
    except OSError as ex:
        return {
            "ok": False,
            "exitCode": None,
            "stdout": "",
            "stderr": "",
            "error": f"{type(ex).__name__}: {ex}",
        }


def free_bytes_for_path(path: Path) -> int:
    target = path
    while not target.exists() and target.parent != target:
        target = target.parent
    return shutil.disk_usage(target).free


def write_text_file(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def load_queue_records(queue_path: Path) -> list[dict[str, Any]]:
    data = load_json_optional(queue_path)
    if not isinstance(data, dict) or not isinstance(data.get("records"), list):
        raise ValueError(f"queue JSON must contain records array: {queue_path}")
    return [item for item in data["records"] if isinstance(item, dict)]


def queue_record_matches(args: argparse.Namespace, record: dict[str, Any]) -> bool:
    if record.get("action") != "witchy-unpack":
        return False
    if args.top_dir and str(record.get("topDir", "")).lower() not in {item.lower() for item in args.top_dir}:
        return False
    if args.bundle_kind and str(record.get("bundleKind", "")).lower() not in {
        item.lower() for item in args.bundle_kind
    }:
        return False
    if args.relative_contains:
        lowered_relative = str(record.get("relative", "")).lower()
        if not all(token.lower() in lowered_relative for token in args.relative_contains):
            return False
    if args.max_source_mib > 0 and int(record.get("bytes", 0)) > args.max_source_mib * 1024 * 1024:
        return False
    return True


def select_pipeline_queue_records(args: argparse.Namespace) -> list[dict[str, Any]]:
    records = [record for record in load_queue_records(Path(args.queue)) if queue_record_matches(args, record)]
    records.sort(key=lambda item: (int(item.get("priority", 9999)), str(item.get("relative", ""))))
    if args.offset > 0:
        records = records[args.offset :]
    if args.limit > 0:
        records = records[: args.limit]
    return records


def pipeline_record_id(record: dict[str, Any]) -> str:
    relative = str(record.get("relative", "asset"))
    stem = relative
    for suffix in (".dcx", ".bhd", ".bdt"):
        if stem.lower().endswith(suffix):
            stem = stem[: -len(suffix)]
    cleaned = safe_filename(stem.replace("/", "_").replace("\\", "_"))
    if len(cleaned) <= 48:
        return cleaned
    return f"{cleaned[:36].rstrip('_')}_{short_stable_hash(relative)}"


def source_path_for_queue_record(game_root: Path, record: dict[str, Any]) -> Path:
    return game_root / str(record.get("relative", "")).replace("/", os.sep)


def stage_output_dirs(resource_root: Path, record: dict[str, Any], record_id: str) -> dict[str, Path]:
    top_dir = safe_filename(str(record.get("topDir", "unknown")))
    bundle_kind = safe_filename(str(record.get("bundleKind", "unknown")))
    root = resource_root / "FullGame" / top_dir / bundle_kind / record_id
    return {
        "root": root,
        "model": root / "Model",
        "texture": root / "Texture",
        "material": root / "Material",
        "effect": root / "Effect",
        "animation_raw": root / "AnimationRaw",
        "animation": root / "Animation",
        "raw": root / "Raw",
        "manifest": root / "manifest.json",
    }


def write_flver_to_fbx_script(work_root: Path) -> Path:
    script_path = work_root / "scripts" / "flver_to_fbx_batch_generated.py"
    if not script_path.exists() or script_path.read_text(encoding="utf-8") != FLVER_TO_FBX_BLENDER_SCRIPT:
        write_text_file(script_path, FLVER_TO_FBX_BLENDER_SCRIPT)
    return script_path


def collect_unpacked_pipeline_files(unpack_root: Path) -> dict[str, list[Path]]:
    collected: dict[str, list[Path]] = {
        "model": [],
        "texture": [],
        "texture-container": [],
        "material": [],
        "effect": [],
        "animation-raw": [],
        "xml": [],
        "other": [],
    }
    if not unpack_root.exists():
        return collected
    for path in sorted(item for item in unpack_root.rglob("*") if item.is_file()):
        kind = PIPELINE_COLLECT_EXTS.get(path.suffix.lower(), "other")
        collected.setdefault(kind, []).append(path)
    return collected


def extended_path(path: Path) -> str:
    """Return a \\\\?\\ extended-length path on Windows so deep binder trees survive MAX_PATH."""
    text = os.path.abspath(str(path))
    if os.name == "nt" and not text.startswith("\\\\?\\"):
        return "\\\\?\\" + text
    return text


def remove_tree(path: Path) -> None:
    """Delete a directory tree, tolerating MAX_PATH-deep entries WitchyBND can produce."""
    try:
        shutil.rmtree(path)
        return
    except OSError:
        pass
    if os.name == "nt":
        subprocess.run(
            ["cmd", "/c", "rd", "/s", "/q", extended_path(path)],
            capture_output=True,
            check=False,
        )
    if path.exists():
        shutil.rmtree(path, ignore_errors=True)


def copy_collected_files(files: list[Path], destination: Path, source_root: Path) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    for source in files:
        relative = source.relative_to(source_root)
        output = destination / relative
        os.makedirs(extended_path(output.parent), exist_ok=True)
        shutil.copy2(extended_path(source), extended_path(output))
        records.append(
            {
                "source": str(source),
                "output": str(output),
                "relative": relative.as_posix(),
                "bytes": os.stat(extended_path(output)).st_size,
            }
        )
    return records


def run_witchy_unpack(
    witchy: Path,
    source: Path,
    unpack_root: Path,
    recursive: bool,
    timeout_seconds: int,
) -> dict[str, Any]:
    unpack_root.mkdir(parents=True, exist_ok=True)
    command = [
        str(witchy),
        "--unpack",
        "--passive",
        "--location",
        str(unpack_root),
    ]
    if recursive:
        command.append("--recursive")
    command.append(str(source))
    result = run_process(command, timeout_seconds)
    result["command"] = command
    result["outputCounts"] = count_files_by_extension(unpack_root)
    result["outputBytes"] = sum(path.stat().st_size for path in unpack_root.rglob("*") if path.is_file())
    return result


def run_flver_to_fbx_batch(
    blender: Path,
    script: Path,
    soulstruct_root: Path,
    flver_files: list[Path],
    output_dir: Path,
    work_dir: Path,
    timeout_seconds: int,
    game_root: Path | None = None,
) -> list[dict[str, Any]]:
    if not flver_files:
        return []
    input_json = work_dir / "flver_to_fbx_inputs.json"
    summary_json = work_dir / "flver_to_fbx_summary.json"
    output_dir.mkdir(parents=True, exist_ok=True)
    items = [
        {
            "source": str(source),
            "output": str(output_dir / f"{safe_filename(source.parent.name)}_{safe_filename(source.stem)}.fbx"),
        }
        for source in flver_files
    ]
    write_json(input_json, items, True)
    command = [
        str(blender),
        "--factory-startup",
        "--background",
        "--python",
        str(script),
        "--",
        "--input-json",
        str(input_json),
        "--summary",
        str(summary_json),
        "--soulstruct-root",
        str(soulstruct_root),
    ]
    if game_root is not None:
        command += ["--game-root", str(game_root)]
    result = run_process(command, timeout_seconds)
    records = load_json_optional(summary_json)
    if not isinstance(records, list):
        records = []
    for record in records:
        if isinstance(record, dict):
            record["blenderExitCode"] = result.get("exitCode")
            record["blenderOk"] = result.get("ok")
            record["blenderStderrTail"] = (result.get("stderr") or "")[-2000:]
    return [record for record in records if isinstance(record, dict)]


def convert_fbx_to_winters_binary(
    converter: Path,
    fbx_path: Path,
    dirs: dict[str, Path],
    timeout_seconds: int,
    blender: Path | None = None,
    blender_script: Path | None = None,
    normalized_root: Path | None = None,
    blender_timeout_seconds: int = 0,
) -> dict[str, Any]:
    base = safe_filename(fbx_path.stem)
    wskel = dirs["model"] / f"{base}.wskel"
    wmesh = dirs["model"] / f"{base}.wmesh"
    anim_dir = dirs["animation"]
    record: dict[str, Any] = {
        "fbx": str(fbx_path),
        "wskel": str(wskel),
        "wmesh": str(wmesh),
        "wmat": str(wmesh.with_suffix(".wmat")),
        "animationDir": str(anim_dir),
        "ok": False,
    }

    dirs["model"].mkdir(parents=True, exist_ok=True)
    anim_dir.mkdir(parents=True, exist_ok=True)

    skel_result = run_process([str(converter), "skel", str(fbx_path), "-o", str(wskel)], timeout_seconds)
    record["skel"] = skel_result
    has_skel = bool(skel_result.get("ok")) and wskel.exists() and wskel.stat().st_size > 0

    mesh_command = [str(converter), "mesh", str(fbx_path), "-o", str(wmesh)]
    if has_skel:
        mesh_command = [str(converter), "mesh", str(fbx_path), "--skel", str(wskel), "-o", str(wmesh)]
    mesh_result = run_process(mesh_command, timeout_seconds)
    record["mesh"] = mesh_result
    mesh_used_skel = has_skel
    if has_skel and not mesh_result.get("ok"):
        static_mesh_result = run_process([str(converter), "mesh", str(fbx_path), "-o", str(wmesh)], timeout_seconds)
        record["meshStaticFallback"] = static_mesh_result
        if static_mesh_result.get("ok"):
            mesh_result = static_mesh_result
            mesh_used_skel = False
    record["usedSkel"] = mesh_used_skel
    record["skelAvailable"] = has_skel

    if has_skel:
        anim_result = run_process(
            [str(converter), "anim", str(fbx_path), "--skel", str(wskel), "-o", str(anim_dir)],
            timeout_seconds,
        )
        record["anim"] = anim_result
        record["wanimCount"] = len(list(anim_dir.glob("*.wanim"))) if anim_dir.exists() else 0
    else:
        record["anim"] = {"ok": False, "detail": "skel unavailable; skipped anim conversion"}
        record["wanimCount"] = 0

    record["wskelExists"] = wskel.exists()
    record["wmeshExists"] = wmesh.exists()
    record["wmatExists"] = wmesh.with_suffix(".wmat").exists()
    record["wmeshBytes"] = wmesh.stat().st_size if wmesh.exists() else 0
    record["ok"] = bool(mesh_result.get("ok")) and wmesh.exists() and wmesh.stat().st_size > 0
    info = converter_info(converter, wmesh)
    if info:
        record["info"] = info
    if (
        not record["ok"]
        and blender
        and blender.exists()
        and blender_script
        and blender_script.exists()
        and normalized_root
    ):
        normalized_root.mkdir(parents=True, exist_ok=True)
        normalized_fbx = normalized_root / f"{base}_normalized.fbx"
        normalize_result = run_process(
            [
                str(blender),
                "--factory-startup",
                "--background",
                "--python",
                str(blender_script),
                "--",
                "--input",
                str(fbx_path),
                "--output",
                str(normalized_fbx),
            ],
            blender_timeout_seconds or timeout_seconds,
        )
        retry: dict[str, Any] = {
            "normalizedFbx": str(normalized_fbx),
            "normalize": normalize_result,
            "normalizedExists": normalized_fbx.exists(),
            "normalizedBytes": normalized_fbx.stat().st_size if normalized_fbx.exists() else 0,
        }
        if normalize_result.get("ok") and normalized_fbx.exists() and normalized_fbx.stat().st_size > 0:
            retry_skel = run_process([str(converter), "skel", str(normalized_fbx), "-o", str(wskel)], timeout_seconds)
            retry["skel"] = retry_skel
            retry_has_skel = bool(retry_skel.get("ok")) and wskel.exists() and wskel.stat().st_size > 0
            retry_mesh_command = [str(converter), "mesh", str(normalized_fbx), "-o", str(wmesh)]
            if retry_has_skel:
                retry_mesh_command = [str(converter), "mesh", str(normalized_fbx), "--skel", str(wskel), "-o", str(wmesh)]
            retry_mesh = run_process(retry_mesh_command, timeout_seconds)
            retry["mesh"] = retry_mesh
            retry_mesh_used_skel = retry_has_skel
            if retry_has_skel and not retry_mesh.get("ok"):
                retry_static_mesh = run_process([str(converter), "mesh", str(normalized_fbx), "-o", str(wmesh)], timeout_seconds)
                retry["meshStaticFallback"] = retry_static_mesh
                if retry_static_mesh.get("ok"):
                    retry_mesh = retry_static_mesh
                    retry_mesh_used_skel = False
            retry["usedSkel"] = retry_mesh_used_skel
            retry["skelAvailable"] = retry_has_skel
            if retry_has_skel:
                retry_anim = run_process(
                    [str(converter), "anim", str(normalized_fbx), "--skel", str(wskel), "-o", str(anim_dir)],
                    timeout_seconds,
                )
                retry["anim"] = retry_anim
            retry["wskelExists"] = wskel.exists()
            retry["wmeshExists"] = wmesh.exists()
            retry["wmatExists"] = wmesh.with_suffix(".wmat").exists()
            retry["wmeshBytes"] = wmesh.stat().st_size if wmesh.exists() else 0
            retry["ok"] = bool(retry_mesh.get("ok")) and wmesh.exists() and wmesh.stat().st_size > 0
            if retry["ok"]:
                record["ok"] = True
                record["usedNormalizedRetry"] = True
                record["usedSkel"] = retry_mesh_used_skel
                record["skelAvailable"] = retry_has_skel
                record["wskelExists"] = wskel.exists()
                record["wmeshExists"] = wmesh.exists()
                record["wmatExists"] = wmesh.with_suffix(".wmat").exists()
                record["wmeshBytes"] = wmesh.stat().st_size
                retry_info = converter_info(converter, wmesh)
                if retry_info:
                    record["info"] = retry_info
                record["wanimCount"] = len(list(anim_dir.glob("*.wanim"))) if anim_dir.exists() else 0
        record["normalizedRetry"] = retry
    return record


def convert_pngs_to_dds_in_dir(
    texconv: Path | None,
    texture_dir: Path,
    timeout_seconds: int,
    force: bool,
) -> list[dict[str, Any]]:
    if not texconv or not texconv.exists() or not texture_dir.exists():
        return []
    records: list[dict[str, Any]] = []
    for source_path in sorted(texture_dir.rglob("*.png")):
        output_path = source_path.with_suffix(".dds")
        role, base_stem = infer_texture_role(source_path)
        texture_format = dds_format_for_role(role)
        should_convert = force or not output_path.exists() or source_path.stat().st_mtime > output_path.stat().st_mtime
        result = None
        if should_convert:
            result = run_process(
                [
                    str(texconv),
                    "-nologo",
                    "-y",
                    "-m",
                    "0",
                    "-f",
                    texture_format,
                    "-ft",
                    "dds",
                    "-o",
                    str(source_path.parent),
                    str(source_path),
                ],
                timeout_seconds,
            )
        records.append(
            {
                "source": str(source_path),
                "output": str(output_path),
                "role": role or "unknown",
                "baseStem": base_stem,
                "format": texture_format,
                "converted": should_convert,
                "ok": output_path.exists() and output_path.stat().st_size > 0 and (result is None or result.get("ok")),
                "exitCode": result.get("exitCode") if result else None,
            }
        )
    return records


def find_texture_outputs(root: Path) -> list[Path]:
    if not root.exists():
        return []
    return [
        path
        for path in root.rglob("*")
        if path.is_file() and path.suffix.lower() in {".dds", ".png"}
    ]


def wait_for_texture_outputs(root: Path) -> list[Path]:
    textures = find_texture_outputs(root)
    if textures:
        return textures
    deadline = time.monotonic() + TPF_TEXTURE_SETTLE_SECONDS
    while time.monotonic() < deadline:
        time.sleep(0.05)
        textures = find_texture_outputs(root)
        if textures:
            return textures
    return textures


def run_tpf_to_texture_batch(
    witchy: Path,
    tpf_files: list[Path],
    texture_dir: Path,
    source_root: Path,
    timeout_seconds: int,
    force: bool,
) -> list[dict[str, Any]]:
    if not witchy.exists() or not tpf_files:
        return []
    texture_dir.mkdir(parents=True, exist_ok=True)
    records: list[dict[str, Any]] = []
    for source_path in sorted(tpf_files):
        try:
            relative = source_path.relative_to(source_root)
            relative_text = relative.as_posix()
        except ValueError:
            relative_text = source_path.name
        short_stem = safe_filename(source_path.stem)[:40]
        output_name = f"{short_stem}_{short_stable_hash(relative_text)}"
        output_dir = texture_dir / output_name
        existing_textures = find_texture_outputs(output_dir)
        if existing_textures and not force:
            records.append(
                {
                    "source": str(source_path),
                    "outputDir": str(output_dir),
                    "ok": True,
                    "skipped": True,
                    "textures": len(existing_textures),
                    "dds": sum(1 for path in existing_textures if path.suffix.lower() == ".dds"),
                    "png": sum(1 for path in existing_textures if path.suffix.lower() == ".png"),
                }
            )
            continue

        result = run_witchy_unpack(witchy, source_path, output_dir, False, timeout_seconds)
        generated_textures = wait_for_texture_outputs(output_dir)
        records.append(
            {
                "source": str(source_path),
                "outputDir": str(output_dir),
                "ok": bool(result.get("ok")) and len(generated_textures) > 0,
                "skipped": False,
                "textures": len(generated_textures),
                "dds": sum(1 for path in generated_textures if path.suffix.lower() == ".dds"),
                "png": sum(1 for path in generated_textures if path.suffix.lower() == ".png"),
                "witchy": result,
            }
        )
    for record in records:
        if record.get("ok"):
            continue
        output_dir = Path(str(record.get("outputDir", "")))
        generated_textures = wait_for_texture_outputs(output_dir)
        if not generated_textures:
            continue
        record["ok"] = True
        record["settledAfterBatch"] = True
        record["textures"] = len(generated_textures)
        record["dds"] = sum(1 for path in generated_textures if path.suffix.lower() == ".dds")
        record["png"] = sum(1 for path in generated_textures if path.suffix.lower() == ".png")
    return records


def run_full_pipeline_batch(args: argparse.Namespace, record: dict[str, Any], flver_script: Path) -> dict[str, Any]:
    game_root = Path(args.game_root)
    resource_root = Path(args.resource_root)
    work_root = Path(args.work_root)
    record_id = pipeline_record_id(record)
    source = source_path_for_queue_record(game_root, record)
    dirs = stage_output_dirs(resource_root, record, record_id)
    unpack_root = work_root / "unpacked" / record_id
    batch_work = work_root / "batches" / record_id
    status_path = work_root / "status" / f"{record_id}.json"

    batch: dict[str, Any] = {
        "schema": "winters.elden.full_pipeline_batch.v1",
        "generatedAt": now_utc_iso(),
        "id": record_id,
        "queueRecord": record,
        "source": str(source),
        "unpackRoot": str(unpack_root),
        "resourceRoot": str(dirs["root"]),
        "statusPath": str(status_path),
        "ok": False,
        "stages": {},
    }

    if args.resume and status_path.exists():
        previous = load_json_optional(status_path)
        if isinstance(previous, dict) and previous.get("ok"):
            previous["resumed"] = True
            return previous

    if free_bytes_for_path(work_root) < args.min_free_gib * 1024 * 1024 * 1024:
        batch["stopped"] = "min-free-gib"
        write_json(status_path, batch, True)
        return batch

    if not source.exists():
        batch["error"] = "source missing"
        write_json(status_path, batch, True)
        return batch

    if args.clean_unpack and unpack_root.exists():
        remove_tree(unpack_root)

    witchy_result = run_witchy_unpack(
        Path(args.witchy),
        source,
        unpack_root,
        args.recursive,
        args.witchy_timeout,
    )
    batch["stages"]["witchyUnpack"] = witchy_result
    if not witchy_result.get("ok") and not args.continue_on_error:
        write_json(status_path, batch, True)
        return batch

    collected = collect_unpacked_pipeline_files(unpack_root)
    batch["stages"]["collect"] = {
        key: len(value)
        for key, value in sorted(collected.items())
    }

    copied: dict[str, list[dict[str, Any]]] = {}
    copied["textures"] = copy_collected_files(
        collected.get("texture", []),
        dirs["texture"],
        unpack_root,
    )
    copied["materials"] = copy_collected_files(
        collected.get("material", []) + [p for p in collected.get("xml", []) if "matbin" in p.name.lower()],
        dirs["material"],
        unpack_root,
    )
    copied["effects"] = copy_collected_files(
        collected.get("effect", []),
        dirs["effect"],
        unpack_root,
    )
    copied["animationRaw"] = copy_collected_files(
        collected.get("animation-raw", []),
        dirs["animation_raw"],
        unpack_root,
    )
    copied["raw"] = []
    fallback_payload = collected.get("xml", []) + collected.get("other", [])
    payload_only_unpack = (
        not copied["textures"]
        and not copied["materials"]
        and not copied["effects"]
        and not copied["animationRaw"]
        and not collected.get("model")
        and not collected.get("texture-container")
    )
    if fallback_payload and (str(record.get("bundleKind")) == "map-msb" or payload_only_unpack):
        # MSB layout XML, FMG text, params, scripts, events: keep the decoded
        # payload under Raw instead of dropping it with the temp unpack dir.
        copied["raw"] = copy_collected_files(fallback_payload, dirs["raw"], unpack_root)
    batch["stages"]["copyCollected"] = {
        key: len(value)
        for key, value in copied.items()
    }

    tpf_records = run_tpf_to_texture_batch(
        Path(args.witchy),
        collected.get("texture-container", []),
        dirs["texture"],
        unpack_root,
        args.witchy_timeout,
        args.force_dds,
    )
    batch["stages"]["tpfToTexture"] = {
        "records": tpf_records,
        "ok": sum(1 for item in tpf_records if item.get("ok")),
        "failed": sum(1 for item in tpf_records if not item.get("ok")),
        "dds": sum(int(item.get("dds", 0)) for item in tpf_records),
        "png": sum(int(item.get("png", 0)) for item in tpf_records),
    }

    fbx_records = run_flver_to_fbx_batch(
        Path(args.blender),
        flver_script,
        Path(args.soulstruct_root),
        collected.get("model", []),
        dirs["model"],
        batch_work,
        args.blender_timeout,
        game_root=game_root,
    )
    batch["stages"]["flverToFbx"] = {
        "records": fbx_records,
        "ok": sum(1 for item in fbx_records if item.get("ok")),
        "failed": sum(1 for item in fbx_records if not item.get("ok")),
    }

    binary_records: list[dict[str, Any]] = []
    for item in fbx_records:
        if not item.get("ok"):
            continue
        fbx_path = Path(str(item.get("output")))
        if fbx_path.exists():
            binary_records.append(
                convert_fbx_to_winters_binary(
                    Path(args.converter),
                    fbx_path,
                    dirs,
                    args.converter_timeout,
                    Path(args.blender),
                    Path(args.blender_normalize_script),
                    batch_work / "normalized_fbx",
                    args.blender_timeout,
                )
            )
    batch["stages"]["fbxToWinters"] = {
        "records": binary_records,
        "ok": sum(1 for item in binary_records if item.get("ok")),
        "failed": sum(1 for item in binary_records if not item.get("ok")),
        "wanim": sum(int(item.get("wanimCount", 0)) for item in binary_records),
    }

    dds_records = convert_pngs_to_dds_in_dir(
        Path(args.texconv) if args.texconv else None,
        dirs["texture"],
        args.texconv_timeout,
        args.force_dds,
    )
    batch["stages"]["textureToDds"] = {
        "records": dds_records,
        "ok": sum(1 for item in dds_records if item.get("ok")),
        "failed": sum(1 for item in dds_records if not item.get("ok")),
    }

    raw_animation_count = len(copied["animationRaw"])
    batch["animationStatus"] = (
        "wanim-converted"
        if batch["stages"]["fbxToWinters"]["wanim"] > 0
        else "raw-hkx-tae-collected-converter-needed"
        if raw_animation_count > 0
        else "no-animation-source-found"
    )

    failed_stage_count = (
        batch["stages"]["tpfToTexture"]["failed"]
        + batch["stages"]["flverToFbx"]["failed"]
        + batch["stages"]["fbxToWinters"]["failed"]
        + batch["stages"]["textureToDds"]["failed"]
    )
    produced_any = (
        batch["stages"]["flverToFbx"]["ok"] > 0
        or len(copied["textures"]) > 0
        or len(copied["animationRaw"]) > 0
        or len(copied["materials"]) > 0
        or len(copied["effects"]) > 0
        or len(copied["raw"]) > 0
        or batch["stages"]["tpfToTexture"]["ok"] > 0
    )
    batch["ok"] = bool(witchy_result.get("ok")) and produced_any and failed_stage_count == 0
    batch["failedStageCount"] = failed_stage_count
    write_json(dirs["manifest"], batch, True)
    write_json(status_path, batch, True)

    if args.clean_unpack and unpack_root.exists():
        remove_tree(unpack_root)
        batch["unpackCleaned"] = True
        write_json(status_path, batch, True)
        write_json(dirs["manifest"], batch, True)

    return batch


def run_full_pipeline_command(args: argparse.Namespace) -> int:
    work_root = Path(args.work_root)
    resource_root = Path(args.resource_root)
    work_root.mkdir(parents=True, exist_ok=True)
    resource_root.mkdir(parents=True, exist_ok=True)
    flver_script = write_flver_to_fbx_script(work_root)
    selected = select_pipeline_queue_records(args)
    records: list[dict[str, Any]] = []

    for index, record in enumerate(selected, start=1):
        record_id = pipeline_record_id(record)
        print(f"[{index}/{len(selected)}] {record_id} {record.get('relative')}", flush=True)
        batch = run_full_pipeline_batch(args, record, flver_script)
        records.append(
            {
                "id": record_id,
                "relative": record.get("relative"),
                "ok": batch.get("ok"),
                "animationStatus": batch.get("animationStatus"),
                "statusPath": batch.get("statusPath"),
                "resourceRoot": batch.get("resourceRoot"),
                "stopped": batch.get("stopped"),
            }
        )
        if batch.get("stopped"):
            break
        if not batch.get("ok") and not args.continue_on_error:
            break

    manifest = {
        "schema": "winters.elden.full_pipeline_run.v1",
        "generatedAt": now_utc_iso(),
        "generatedBy": "Tools/EldenAssetPipeline/elden_pipeline.py run-full-pipeline",
        "queue": str(Path(args.queue)),
        "gameRoot": str(Path(args.game_root)),
        "resourceRoot": str(resource_root),
        "workRoot": str(work_root),
        "filters": {
            "topDir": args.top_dir,
            "bundleKind": args.bundle_kind,
            "relativeContains": args.relative_contains,
            "offset": args.offset,
            "limit": args.limit,
            "maxSourceMiB": args.max_source_mib,
        },
        "counts": {
            "selected": len(selected),
            "processed": len(records),
            "ok": sum(1 for item in records if item.get("ok")),
            "failed": sum(1 for item in records if not item.get("ok")),
        },
        "records": records,
    }
    write_json(Path(args.out), manifest, args.pretty)

    if args.rebuild_catalog:
        catalog_args = argparse.Namespace(
            resource_root=str(resource_root),
            game_root=args.game_root,
            witchy_root=str(work_root / "unpacked"),
            work_root=str(work_root),
            converter=args.converter,
            runtime_bone_limit=args.runtime_bone_limit,
            texture_out=str(resource_root / "Manifests" / "eldenring_texture_index.json"),
            out=str(resource_root / "Manifests" / "eldenring_asset_catalog.json"),
            pretty=True,
        )
        build_resource_catalog_command(catalog_args)

    return 0 if manifest["counts"]["failed"] == 0 else 2


def iter_full_game_asset_dirs(resource_root: Path) -> list[Path]:
    full_game_root = resource_root / "FullGame"
    if not full_game_root.exists():
        return []

    asset_dirs: list[Path] = []
    for top_dir in sorted(path for path in full_game_root.iterdir() if path.is_dir()):
        for bundle_dir in sorted(path for path in top_dir.iterdir() if path.is_dir()):
            for asset_dir in sorted(path for path in bundle_dir.iterdir() if path.is_dir()):
                asset_dirs.append(asset_dir)
    return asset_dirs


def file_magic(path: Path, byte_count: int = 4) -> str:
    try:
        with path.open("rb") as handle:
            return handle.read(byte_count).hex()
    except OSError:
        return ""


def has_magic(path: Path, expected: bytes) -> bool:
    try:
        with path.open("rb") as handle:
            return handle.read(len(expected)) == expected
    except OSError:
        return False


def load_full_pipeline_status_records(work_root: Path) -> dict[str, Any]:
    status_root = work_root / "status"
    records: list[dict[str, Any]] = []
    latest_by_relative: dict[str, dict[str, Any]] = {}
    latest_by_id: dict[str, dict[str, Any]] = {}

    if status_root.exists():
        for status_path in sorted(status_root.glob("*.json")):
            data = load_json_optional(status_path)
            if not isinstance(data, dict):
                continue
            queue_record = data.get("queueRecord") if isinstance(data.get("queueRecord"), dict) else {}
            relative = str(queue_record.get("relative") or "")
            current_id = pipeline_record_id(queue_record) if relative else None
            status_id = str(data.get("id") or status_path.stem)
            mtime = status_path.stat().st_mtime
            record = {
                "path": str(status_path),
                "id": status_id,
                "currentId": current_id,
                "relative": relative,
                "topDir": queue_record.get("topDir"),
                "bundleKind": queue_record.get("bundleKind"),
                "ok": data.get("ok"),
                "animationStatus": data.get("animationStatus"),
                "resourceRoot": data.get("resourceRoot"),
                "generatedAt": data.get("generatedAt"),
                "mtime": mtime,
                "data": data,
            }
            records.append(record)
            latest_by_id[status_id] = max(
                (latest_by_id.get(status_id), record),
                key=lambda item: -1 if item is None else float(item["mtime"]),
            )
            if relative:
                latest_by_relative[relative] = max(
                    (latest_by_relative.get(relative), record),
                    key=lambda item: -1 if item is None else float(item["mtime"]),
                )

    superseded = []
    latest_paths = {item["path"] for item in latest_by_relative.values()}
    for record in records:
        if record["relative"] and record["path"] not in latest_paths:
            superseded.append(
                {
                    "path": record["path"],
                    "id": record["id"],
                    "currentId": record["currentId"],
                    "relative": record["relative"],
                }
            )

    id_mismatches = [
        {
            "path": record["path"],
            "id": record["id"],
            "currentId": record["currentId"],
            "relative": record["relative"],
        }
        for record in records
        if record.get("currentId") and record.get("id") != record.get("currentId")
    ]

    return {
        "root": str(status_root),
        "records": records,
        "latestByRelative": latest_by_relative,
        "latestById": latest_by_id,
        "superseded": superseded,
        "idMismatches": id_mismatches,
    }


def index_full_game_resource_dirs(resource_root: Path) -> dict[str, Any]:
    dirs = iter_full_game_asset_dirs(resource_root)
    by_id = {path.name: path for path in dirs}
    by_relative: dict[str, Path] = {}
    manifest_errors: list[dict[str, Any]] = []
    for asset_dir in dirs:
        manifest_path = asset_dir / "manifest.json"
        manifest = load_json_optional(manifest_path)
        if not isinstance(manifest, dict):
            continue
        queue_record = manifest.get("queueRecord")
        if not isinstance(queue_record, dict):
            continue
        relative = str(queue_record.get("relative") or "")
        if relative:
            by_relative[relative] = asset_dir
        expected_id = pipeline_record_id(queue_record)
        if expected_id != asset_dir.name:
            manifest_errors.append(
                {
                    "root": str(asset_dir),
                    "relative": relative,
                    "assetDirId": asset_dir.name,
                    "expectedCurrentId": expected_id,
                }
            )
    return {
        "dirs": dirs,
        "byId": by_id,
        "byRelative": by_relative,
        "manifestIdMismatches": manifest_errors,
    }


def summarize_queue_coverage(
    queue_records: list[dict[str, Any]],
    status_by_relative: dict[str, dict[str, Any]],
    resource_by_relative: dict[str, Path],
    resource_by_id: dict[str, Path],
    sample_limit: int,
) -> dict[str, Any]:
    by_top_dir: Counter[str] = Counter()
    by_bundle_kind: Counter[str] = Counter()
    by_top_bundle: Counter[str] = Counter()
    duplicate_ids: dict[str, list[str]] = {}
    seen_ids: dict[str, str] = {}
    unprocessed_by_top_bundle: Counter[str] = Counter()
    unprocessed_samples: dict[str, list[dict[str, Any]]] = {}
    processed = 0
    status_only = 0
    resource_only = 0
    both = 0

    for record in queue_records:
        top_dir = str(record.get("topDir") or "_unknown")
        bundle_kind = str(record.get("bundleKind") or "_unknown")
        top_bundle = f"{top_dir}/{bundle_kind}"
        by_top_dir[top_dir] += 1
        by_bundle_kind[bundle_kind] += 1
        by_top_bundle[top_bundle] += 1

        record_id = pipeline_record_id(record)
        relative = str(record.get("relative") or "")
        if record_id in seen_ids:
            duplicate_ids.setdefault(record_id, [seen_ids[record_id]]).append(relative)
        else:
            seen_ids[record_id] = relative

        has_status = relative in status_by_relative
        has_resource = relative in resource_by_relative or record_id in resource_by_id
        if has_status or has_resource:
            processed += 1
            if has_status and has_resource:
                both += 1
            elif has_status:
                status_only += 1
            else:
                resource_only += 1
            continue

        unprocessed_by_top_bundle[top_bundle] += 1
        bucket = unprocessed_samples.setdefault(top_bundle, [])
        if len(bucket) < sample_limit:
            bucket.append(
                {
                    "id": record_id,
                    "relative": relative,
                    "bytes": record.get("bytes"),
                    "priority": record.get("priority"),
                }
            )

    return {
        "totalQueueRecords": len(queue_records),
        "processedOrPresent": processed,
        "statusAndResource": both,
        "statusOnly": status_only,
        "resourceOnly": resource_only,
        "unprocessed": len(queue_records) - processed,
        "byTopDir": dict(sorted(by_top_dir.items())),
        "byBundleKind": dict(sorted(by_bundle_kind.items())),
        "byTopBundle": dict(sorted(by_top_bundle.items())),
        "unprocessedByTopBundle": dict(sorted(unprocessed_by_top_bundle.items())),
        "unprocessedSamples": dict(sorted(unprocessed_samples.items())),
        "duplicatePipelineIds": {
            key: values for key, values in sorted(duplicate_ids.items()) if len(values) > 1
        },
    }


def collect_files_with_ext(root: Path, extensions: set[str]) -> list[Path]:
    if not root.exists():
        return []
    return [
        path
        for path in sorted(root.rglob("*"))
        if path.is_file() and path.suffix.lower() in extensions
    ]


def audit_binary_info(
    converter: Path | None,
    path: Path,
    runtime_bone_limit: int,
) -> tuple[dict[str, Any], list[dict[str, Any]], list[dict[str, Any]]]:
    errors: list[dict[str, Any]] = []
    warnings: list[dict[str, Any]] = []
    record: dict[str, Any] = {
        "path": str(path),
        "bytes": path.stat().st_size if path.exists() else 0,
        "magic": file_magic(path),
    }
    if not path.exists() or path.stat().st_size == 0:
        errors.append({"code": "empty-binary", "path": str(path)})
        return record, errors, warnings
    if not has_magic(path, b"WINT"):
        warnings.append({"code": "unexpected-winters-magic", "path": str(path), "magic": record["magic"]})
    info = converter_info(converter, path) if converter else None
    if info:
        record["info"] = info
        if not info.get("ok"):
            issue = {"code": "converter-info-failed", "path": str(path), "output": info.get("output")}
            if path.suffix.lower() == ".wmesh":
                errors.append(issue)
            else:
                warnings.append({**issue, "detail": "converter info is required for WMesh and best-effort for this file type"})
        if path.suffix.lower() == ".wmesh" and isinstance(info.get("bones"), int):
            if int(info["bones"]) > runtime_bone_limit:
                warnings.append(
                    {
                        "code": "runtime-bone-limit",
                        "path": str(path),
                        "bones": info["bones"],
                        "runtimeBoneLimit": runtime_bone_limit,
                    }
                )
    return record, errors, warnings


def audit_texture_file(path: Path) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    errors: list[dict[str, Any]] = []
    role, base_stem = infer_texture_role(path)
    record = {
        "path": str(path),
        "bytes": path.stat().st_size if path.exists() else 0,
        "role": role or "unknown",
        "baseStem": base_stem,
        "magic": file_magic(path),
    }
    if not path.exists() or path.stat().st_size == 0:
        errors.append({"code": "empty-texture", "path": str(path)})
    elif path.suffix.lower() == ".dds" and not has_magic(path, b"DDS "):
        errors.append({"code": "invalid-dds-magic", "path": str(path), "magic": record["magic"]})
    elif path.suffix.lower() == ".png" and not has_magic(path, b"\x89PNG\r\n\x1a\n"):
        errors.append({"code": "invalid-png-magic", "path": str(path), "magic": record["magic"]})
    return record, errors


def audit_asset_dir(
    asset_dir: Path,
    converter: Path | None,
    runtime_bone_limit: int,
    max_path_warning: int,
) -> dict[str, Any]:
    manifest = load_json_optional(asset_dir / "manifest.json")
    queue_record = manifest.get("queueRecord") if isinstance(manifest, dict) else None
    expected_id = pipeline_record_id(queue_record) if isinstance(queue_record, dict) else None
    top_dir = asset_dir.parent.parent.name if asset_dir.parent.parent.exists() else None
    bundle_kind = asset_dir.parent.name
    files = {
        "wmesh": collect_files_with_ext(asset_dir, {".wmesh"}),
        "wmat": collect_files_with_ext(asset_dir, {".wmat"}),
        "wskel": collect_files_with_ext(asset_dir, {".wskel"}),
        "wanim": collect_files_with_ext(asset_dir, {".wanim"}),
        "fbx": collect_files_with_ext(asset_dir, {".fbx"}),
        "dds": collect_files_with_ext(asset_dir, {".dds"}),
        "png": collect_files_with_ext(asset_dir, {".png"}),
        "rawAnimation": collect_files_with_ext(asset_dir, {".hkx", ".hkxpwv", ".tae"}),
        "materialSource": collect_files_with_ext(asset_dir, {".matbin", ".xml"}),
        "effect": collect_files_with_ext(asset_dir, {".fxr"}),
    }
    errors: list[dict[str, Any]] = []
    warnings: list[dict[str, Any]] = []
    binary_samples: dict[str, list[dict[str, Any]]] = {"wmesh": [], "wskel": [], "wanim": []}

    if expected_id and expected_id != asset_dir.name:
        warnings.append(
            {
                "code": "asset-dir-id-not-current",
                "assetDirId": asset_dir.name,
                "expectedCurrentId": expected_id,
            }
        )
    if isinstance(manifest, dict) and manifest.get("ok") is False:
        warnings.append({"code": "batch-status-not-ok", "failedStageCount": manifest.get("failedStageCount")})

    for wmesh in files["wmesh"]:
        wmat = wmesh.with_suffix(".wmat")
        if not wmat.exists():
            errors.append({"code": "wmesh-missing-wmat", "wmesh": str(wmesh), "expectedWmat": str(wmat)})
        elif wmat.stat().st_size == 0:
            errors.append({"code": "empty-wmat", "wmat": str(wmat)})
        elif not has_magic(wmat, b"WINT"):
            warnings.append({"code": "unexpected-wmat-magic", "wmat": str(wmat), "magic": file_magic(wmat)})
        info_record, info_errors, info_warnings = audit_binary_info(converter, wmesh, runtime_bone_limit)
        binary_samples["wmesh"].append(info_record)
        errors.extend(info_errors)
        warnings.extend(info_warnings)

    for ext in ("wskel", "wanim"):
        for path in files[ext]:
            info_record, info_errors, info_warnings = audit_binary_info(converter, path, runtime_bone_limit)
            binary_samples[ext].append(info_record)
            errors.extend(info_errors)
            warnings.extend(info_warnings)

    texture_role_counts: Counter[str] = Counter()
    dds_by_png: list[dict[str, Any]] = []
    for texture_path in files["dds"] + files["png"]:
        texture_record, texture_errors = audit_texture_file(texture_path)
        texture_role_counts[str(texture_record["role"])] += 1
        errors.extend(texture_errors)
        if texture_path.suffix.lower() == ".png":
            expected_dds = texture_path.with_suffix(".dds")
            if not expected_dds.exists():
                dds_by_png.append({"png": str(texture_path), "expectedDds": str(expected_dds)})
    if dds_by_png:
        warnings.append({"code": "png-without-sidecar-dds", "count": len(dds_by_png), "samples": dds_by_png[:20]})

    long_paths = [str(path) for paths in files.values() for path in paths if len(str(path)) >= max_path_warning]
    if long_paths:
        warnings.append(
            {
                "code": "long-path-warning",
                "threshold": max_path_warning,
                "count": len(long_paths),
                "samples": long_paths[:20],
            }
        )
    if files["rawAnimation"] and not files["wanim"]:
        warnings.append(
            {
                "code": "raw-animation-needs-wanim-converter",
                "rawAnimationFiles": len(files["rawAnimation"]),
            }
        )
    if files["wmesh"] and not files["wmat"]:
        errors.append({"code": "asset-has-wmesh-without-any-wmat", "root": str(asset_dir)})
    if not files["wmesh"] and not files["dds"] and not files["png"] and not files["rawAnimation"]:
        warnings.append({"code": "resource-root-has-no-runtime-payload", "root": str(asset_dir)})

    stage_checks: list[dict[str, Any]] = []
    if isinstance(manifest, dict):
        stages = manifest.get("stages") if isinstance(manifest.get("stages"), dict) else {}
        flver_to_fbx = stages.get("flverToFbx") if isinstance(stages.get("flverToFbx"), dict) else {}
        fbx_to_winters = stages.get("fbxToWinters") if isinstance(stages.get("fbxToWinters"), dict) else {}
        tpf_to_texture = stages.get("tpfToTexture") if isinstance(stages.get("tpfToTexture"), dict) else {}
        copy_collected = stages.get("copyCollected") if isinstance(stages.get("copyCollected"), dict) else {}
        stage_checks = [
            {"name": "flverToFbx.ok-vs-fbx", "expected": flver_to_fbx.get("ok"), "actual": len(files["fbx"])},
            {"name": "fbxToWinters.ok-vs-wmesh", "expected": fbx_to_winters.get("ok"), "actual": len(files["wmesh"])},
            {
                "name": "copyCollected.animationRaw-vs-rawAnimation",
                "expected": copy_collected.get("animationRaw"),
                "actual": len(files["rawAnimation"]),
            },
            {
                "name": "tpfToTexture.dds+png-vs-textures",
                "expected": int(tpf_to_texture.get("dds", 0) or 0) + int(tpf_to_texture.get("png", 0) or 0),
                "actual": len(files["dds"]) + len(files["png"]),
            },
        ]
        for check in stage_checks:
            expected = check.get("expected")
            actual = check.get("actual")
            if expected is None:
                continue
            if check["name"] == "tpfToTexture.dds+png-vs-textures":
                if int(actual) < int(expected):
                    errors.append({"code": "stage-count-mismatch", **check})
            elif int(expected) != int(actual):
                warnings.append({"code": "stage-count-drift", **check})

    if files["wmesh"]:
        runtime_status = "binary-extracted-runtime-ready"
    elif files["dds"] or files["png"]:
        runtime_status = "texture-only"
    elif files["rawAnimation"]:
        runtime_status = "raw-animation-collected"
    else:
        runtime_status = "empty-or-metadata-only"

    return {
        "id": asset_dir.name,
        "root": str(asset_dir),
        "topDir": top_dir,
        "bundleKind": bundle_kind,
        "relative": queue_record.get("relative") if isinstance(queue_record, dict) else None,
        "expectedCurrentId": expected_id,
        "manifestOk": manifest.get("ok") if isinstance(manifest, dict) else None,
        "animationStatus": manifest.get("animationStatus") if isinstance(manifest, dict) else None,
        "runtimeStatus": runtime_status,
        "counts": {
            "byExtension": count_files_by_extension(asset_dir),
            "wmesh": len(files["wmesh"]),
            "wmat": len(files["wmat"]),
            "wskel": len(files["wskel"]),
            "wanim": len(files["wanim"]),
            "fbx": len(files["fbx"]),
            "dds": len(files["dds"]),
            "png": len(files["png"]),
            "rawAnimation": len(files["rawAnimation"]),
            "materialSource": len(files["materialSource"]),
            "effect": len(files["effect"]),
            "textureRoles": dict(sorted(texture_role_counts.items())),
        },
        "stageChecks": stage_checks,
        "binarySamples": binary_samples,
        "errors": errors,
        "warnings": warnings,
    }


def audit_global_resource_files(
    resource_root: Path,
    converter: Path | None,
    runtime_bone_limit: int,
    max_path_warning: int,
) -> dict[str, Any]:
    wmeshes = collect_files_with_ext(resource_root, {".wmesh"})
    wmats = collect_files_with_ext(resource_root, {".wmat"})
    wskels = collect_files_with_ext(resource_root, {".wskel"})
    wanims = collect_files_with_ext(resource_root, {".wanim"})
    dds_files = collect_files_with_ext(resource_root, {".dds"})
    png_files = collect_files_with_ext(resource_root, {".png"})
    errors: list[dict[str, Any]] = []
    warnings: list[dict[str, Any]] = []
    role_counts: Counter[str] = Counter()
    wmesh_info_ok = 0
    wmesh_info_failed = 0
    wmesh_with_wmat = 0

    for wmesh in wmeshes:
        expected_wmat = wmesh.with_suffix(".wmat")
        if expected_wmat.exists() and expected_wmat.stat().st_size > 0:
            wmesh_with_wmat += 1
        else:
            errors.append({"code": "global-wmesh-missing-wmat", "wmesh": str(wmesh), "expectedWmat": str(expected_wmat)})
        _, info_errors, info_warnings = audit_binary_info(converter, wmesh, runtime_bone_limit)
        if info_errors:
            wmesh_info_failed += 1
        else:
            wmesh_info_ok += 1
        errors.extend(info_errors)
        warnings.extend(info_warnings)

    for path in wskels + wanims:
        _, info_errors, info_warnings = audit_binary_info(converter, path, runtime_bone_limit)
        errors.extend(info_errors)
        warnings.extend(info_warnings)

    for wmat in wmats:
        if wmat.stat().st_size == 0:
            errors.append({"code": "global-empty-wmat", "wmat": str(wmat)})
        elif not has_magic(wmat, b"WINT"):
            warnings.append({"code": "global-unexpected-wmat-magic", "wmat": str(wmat), "magic": file_magic(wmat)})

    png_without_dds = []
    for texture in dds_files + png_files:
        texture_record, texture_errors = audit_texture_file(texture)
        role_counts[str(texture_record["role"])] += 1
        errors.extend(texture_errors)
        if texture.suffix.lower() == ".png" and not texture.with_suffix(".dds").exists():
            png_without_dds.append({"png": str(texture), "expectedDds": str(texture.with_suffix(".dds"))})

    if png_without_dds:
        warnings.append(
            {
                "code": "global-png-without-sidecar-dds",
                "count": len(png_without_dds),
                "samples": png_without_dds[:50],
            }
        )

    long_paths = [
        str(path)
        for path in wmeshes + wmats + wskels + wanims + dds_files + png_files
        if len(str(path)) >= max_path_warning
    ]
    if long_paths:
        warnings.append(
            {
                "code": "global-long-path-warning",
                "threshold": max_path_warning,
                "count": len(long_paths),
                "samples": long_paths[:50],
            }
        )

    return {
        "counts": {
            "wmesh": len(wmeshes),
            "wmat": len(wmats),
            "wskel": len(wskels),
            "wanim": len(wanims),
            "dds": len(dds_files),
            "png": len(png_files),
            "wmeshWithWmat": wmesh_with_wmat,
            "wmeshMissingWmat": len(wmeshes) - wmesh_with_wmat,
            "wmeshInfoOk": wmesh_info_ok,
            "wmeshInfoFailed": wmesh_info_failed,
            "textureRoles": dict(sorted(role_counts.items())),
            "pngWithoutSidecarDds": len(png_without_dds),
        },
        "errors": errors,
        "warnings": warnings,
    }


def audit_map_assemblies(resource_root: Path) -> dict[str, Any]:
    maps = collect_maps(resource_root)
    records: list[dict[str, Any]] = []
    errors: list[dict[str, Any]] = []
    warnings: list[dict[str, Any]] = []

    for map_record in maps:
        assembly_path = Path(str(map_record["path"]))
        data = load_json_optional(assembly_path)
        if not isinstance(data, dict):
            errors.append({"code": "invalid-map-assembly-json", "path": str(assembly_path)})
            continue
        seed_paths = {
            "worldPartition": assembly_path.with_name("world_partition_seed.json"),
            "sequencer": assembly_path.with_name("sequencer_seed.json"),
            "editorMap": assembly_path.with_name("editor_map_seed.json"),
        }
        seed_status: dict[str, Any] = {}
        for name, path in seed_paths.items():
            seed_data = load_json_optional(path)
            seed_status[name] = {
                "path": str(path),
                "exists": path.exists(),
                "validJson": isinstance(seed_data, dict),
                "bytes": path.stat().st_size if path.exists() else 0,
            }
            if not path.exists() or not isinstance(seed_data, dict):
                errors.append({"code": "missing-or-invalid-map-seed", "mapAssembly": str(assembly_path), "seed": name})

        counts = map_record.get("counts", {})
        if int(counts.get("mapPieces", 0) or 0) == 0 and int(counts.get("assetIds", 0) or 0) == 0:
            warnings.append({"code": "map-assembly-has-no-pieces-or-assets", "path": str(assembly_path)})
        if not map_record.get("exactTransformsAvailable"):
            warnings.append({"code": "map-exact-transforms-pending", "path": str(assembly_path)})
        records.append({**map_record, "seedStatus": seed_status})

    return {
        "count": len(records),
        "records": records,
        "errors": errors,
        "warnings": warnings,
    }


def build_next_extraction_targets(
    queue_records: list[dict[str, Any]],
    status_by_relative: dict[str, dict[str, Any]],
    resource_by_relative: dict[str, Path],
    resource_by_id: dict[str, Path],
    sample_limit: int,
) -> dict[str, list[dict[str, Any]]]:
    target_rules = {
        "characterBinders": lambda item: item.get("topDir") == "chr" and item.get("bundleKind") == "character-binder",
        "characterTextures": lambda item: item.get("topDir") == "chr" and item.get("bundleKind") == "character-texture-binder",
        "characterAnimations": lambda item: item.get("topDir") == "chr" and item.get("bundleKind") == "character-animation-binder",
        "limgraveMapBinders": lambda item: item.get("topDir") == "map"
        and item.get("bundleKind") == "map-binder"
        and "m60_42_36_00" in str(item.get("relative") or ""),
        "limgraveTextureBinders": lambda item: item.get("topDir") == "map"
        and item.get("bundleKind") == "texture-binder"
        and "m60_42_36_00" in str(item.get("relative") or ""),
        "assetGeometry": lambda item: item.get("topDir") == "asset" and item.get("bundleKind") == "asset-geometry-binder",
        "parts": lambda item: item.get("topDir") == "parts",
        "menuUi": lambda item: item.get("topDir") == "menu",
        "sfx": lambda item: item.get("topDir") == "sfx",
    }
    targets: dict[str, list[dict[str, Any]]] = {key: [] for key in target_rules}
    for record in queue_records:
        relative = str(record.get("relative") or "")
        record_id = pipeline_record_id(record)
        if relative in status_by_relative or relative in resource_by_relative or record_id in resource_by_id:
            continue
        for name, predicate in target_rules.items():
            if len(targets[name]) >= sample_limit:
                continue
            if predicate(record):
                targets[name].append(
                    {
                        "id": record_id,
                        "relative": relative,
                        "bytes": record.get("bytes"),
                        "priority": record.get("priority"),
                    }
                )
    return targets


def audit_catalog_consistency(
    catalog_path: Path | None,
    resource_asset_dirs: list[Path],
) -> dict[str, Any]:
    if not catalog_path:
        return {"path": None, "exists": False, "errors": [], "warnings": [{"code": "catalog-not-provided"}]}
    catalog = load_json_optional(catalog_path)
    errors: list[dict[str, Any]] = []
    warnings: list[dict[str, Any]] = []
    if not isinstance(catalog, dict):
        errors.append({"code": "catalog-missing-or-invalid", "path": str(catalog_path)})
        return {"path": str(catalog_path), "exists": catalog_path.exists(), "errors": errors, "warnings": warnings}

    catalog_full_game = catalog.get("fullGameAssets") if isinstance(catalog.get("fullGameAssets"), list) else []
    catalog_ids = {str(item.get("id")) for item in catalog_full_game if isinstance(item, dict) and item.get("id")}
    resource_ids = {path.name for path in resource_asset_dirs}
    missing_in_catalog = sorted(resource_ids - catalog_ids)
    catalog_without_resource = sorted(catalog_ids - resource_ids)
    if missing_in_catalog:
        errors.append({"code": "resource-assets-missing-from-catalog", "count": len(missing_in_catalog), "ids": missing_in_catalog[:100]})
    if catalog_without_resource:
        warnings.append(
            {
                "code": "catalog-assets-without-resource-root",
                "count": len(catalog_without_resource),
                "ids": catalog_without_resource[:100],
            }
        )

    summaries = catalog.get("summaries") if isinstance(catalog.get("summaries"), dict) else {}
    summary_count = summaries.get("fullGameAssetCount")
    if summary_count is not None and int(summary_count) != len(catalog_full_game):
        warnings.append(
            {
                "code": "catalog-summary-fullgame-count-drift",
                "summary": summary_count,
                "actualCatalogRecords": len(catalog_full_game),
            }
        )

    return {
        "path": str(catalog_path),
        "exists": catalog_path.exists(),
        "schema": catalog.get("schema"),
        "generatedAt": catalog.get("generatedAt"),
        "fullGameAssetCount": len(catalog_full_game),
        "resourceAssetDirCount": len(resource_asset_dirs),
        "missingInCatalog": missing_in_catalog,
        "catalogWithoutResource": catalog_without_resource,
        "errors": errors,
        "warnings": warnings,
    }


def audit_full_pipeline_command(args: argparse.Namespace) -> int:
    queue_path = Path(args.queue)
    resource_root = Path(args.resource_root)
    work_root = Path(args.work_root)
    catalog_path = Path(args.catalog) if args.catalog else None
    converter = Path(args.converter) if args.converter else None

    queue_records = [
        record
        for record in load_queue_records(queue_path)
        if isinstance(record, dict) and record.get("action") == "witchy-unpack"
    ]
    status_index = load_full_pipeline_status_records(work_root)
    resource_index = index_full_game_resource_dirs(resource_root)
    resource_asset_dirs = resource_index["dirs"]

    coverage = summarize_queue_coverage(
        queue_records,
        status_index["latestByRelative"],
        resource_index["byRelative"],
        resource_index["byId"],
        args.sample_limit,
    )
    asset_audits = [
        audit_asset_dir(
            asset_dir,
            converter,
            args.runtime_bone_limit,
            args.max_path_warning,
        )
        for asset_dir in resource_asset_dirs
    ]
    global_resources = audit_global_resource_files(
        resource_root,
        converter,
        args.runtime_bone_limit,
        args.max_path_warning,
    )
    map_assemblies = audit_map_assemblies(resource_root)
    catalog = audit_catalog_consistency(catalog_path, resource_asset_dirs)
    next_targets = build_next_extraction_targets(
        queue_records,
        status_index["latestByRelative"],
        resource_index["byRelative"],
        resource_index["byId"],
        args.sample_limit,
    )

    asset_errors = [issue for item in asset_audits for issue in item["errors"]]
    asset_warnings = [issue for item in asset_audits for issue in item["warnings"]]
    audit_errors = (
        asset_errors
        + global_resources["errors"]
        + map_assemblies["errors"]
        + catalog["errors"]
    )
    audit_warnings = (
        asset_warnings
        + global_resources["warnings"]
        + map_assemblies["warnings"]
        + catalog["warnings"]
        + [
            {"code": "status-id-mismatch", **item}
            for item in status_index["idMismatches"]
        ]
        + [
            {"code": "superseded-status", **item}
            for item in status_index["superseded"]
        ]
        + [
            {"code": "resource-manifest-id-mismatch", **item}
            for item in resource_index["manifestIdMismatches"]
        ]
    )

    runtime_counts = Counter(item["runtimeStatus"] for item in asset_audits)
    top_bundle_counts = Counter(f"{item.get('topDir')}/{item.get('bundleKind')}" for item in asset_audits)
    character_queue_total = sum(
        1
        for record in queue_records
        if record.get("topDir") == "chr" and record.get("bundleKind") == "character-binder"
    )
    character_resource_ready = sum(
        1
        for item in asset_audits
        if item.get("topDir") == "chr"
        and item.get("bundleKind") == "character-binder"
        and item.get("runtimeStatus") == "binary-extracted-runtime-ready"
    )
    character_resource_partial = sum(
        1
        for item in asset_audits
        if item.get("topDir") == "chr"
        and item.get("bundleKind") == "character-binder"
        and item.get("runtimeStatus") != "binary-extracted-runtime-ready"
    )
    character_unprocessed = max(
        0,
        character_queue_total - character_resource_ready - character_resource_partial,
    )

    completeness_warnings: list[dict[str, Any]] = []
    if int(coverage["unprocessed"]) > 0:
        completeness_warnings.append(
            {
                "code": "queue-not-fully-processed",
                "processedOrPresent": coverage["processedOrPresent"],
                "totalQueueRecords": coverage["totalQueueRecords"],
                "unprocessed": coverage["unprocessed"],
            }
        )
    if character_unprocessed > 0 or character_resource_partial > 0:
        completeness_warnings.append(
            {
                "code": "character-binders-not-fully-runtime-ready",
                "queueTotal": character_queue_total,
                "runtimeReady": character_resource_ready,
                "partialOrRaw": character_resource_partial,
                "unprocessed": character_unprocessed,
            }
        )
    if global_resources["counts"].get("wmeshMissingWmat", 0) == 0 and global_resources["counts"].get("wmesh", 0) > 0:
        material_mapping_status = "wmat-sidecars-verified"
    else:
        material_mapping_status = "wmat-sidecars-incomplete"

    report = {
        "schema": "winters.elden.full_pipeline_audit.v1",
        "generatedAt": now_utc_iso(),
        "generatedBy": "Tools/EldenAssetPipeline/elden_pipeline.py audit-full-pipeline",
        "roots": {
            "queue": str(queue_path),
            "resourceRoot": str(resource_root),
            "workRoot": str(work_root),
            "catalog": str(catalog_path) if catalog_path else None,
            "converter": str(converter) if converter else None,
        },
        "successCriteria": [
            "Every queued binder that matters for the target scene has a status JSON or FullGame resource root.",
            "Every WMesh has a sidecar WMat, passes WintersAssetConverter info, and every DDS has a valid DDS magic header.",
            "Every map assembly has World Partition, Sequencer, and Editor seed JSON files visible to the catalog/editor loader.",
        ],
        "summaries": {
            "errors": len(audit_errors),
            "warnings": len(audit_warnings),
            "fullGameResourceRoots": len(resource_asset_dirs),
            "runtimeStatusCounts": dict(sorted(runtime_counts.items())),
            "resourceRootsByTopBundle": dict(sorted(top_bundle_counts.items())),
            "characterBinderQueueTotal": character_queue_total,
            "characterBinderRuntimeReady": character_resource_ready,
            "characterBinderPartialOrRaw": character_resource_partial,
            "characterBinderUnprocessed": character_unprocessed,
            "materialMappingStatus": material_mapping_status,
            "globalResourceCounts": global_resources["counts"],
            "mapAssemblyCount": map_assemblies["count"],
        },
        "completeness": {
            "fullQueueComplete": int(coverage["unprocessed"]) == 0,
            "fullGamePipelineComplete": len(resource_asset_dirs) == len(queue_records),
            "allCharacterBindersRuntimeReady": character_queue_total > 0
            and character_resource_ready == character_queue_total,
            "materialSidecarsComplete": global_resources["counts"].get("wmeshMissingWmat", 0) == 0,
            "pngSidecarDdsComplete": global_resources["counts"].get("pngWithoutSidecarDds", 0) == 0,
            "mapAssembliesPresent": map_assemblies["count"] > 0,
        },
        "coverage": coverage,
        "statusIndex": {
            "root": status_index["root"],
            "records": len(status_index["records"]),
            "latestByRelative": len(status_index["latestByRelative"]),
            "idMismatches": status_index["idMismatches"],
            "superseded": status_index["superseded"],
        },
        "catalog": catalog,
        "globalResources": global_resources,
        "mapAssemblies": map_assemblies,
        "fullGameAssets": asset_audits,
        "nextExtractionTargets": next_targets,
        "knownLimits": [
            "This audit proves the current extracted set, not that all 71k queue records have been converted.",
            "HKX/TAE animation binders are collected as raw sources unless a dedicated HKX/TAE to WAnim converter exists.",
            "WMat sidecar existence verifies runtime material payloads; exact MATBIN texture-slot binding still requires a resolver pass.",
            "Map assembly seeds prove editor-visible composition data; exact FromSoftware MSB transforms remain a separate parser-quality gate.",
        ],
    }
    report["globalResources"]["warnings"].extend(completeness_warnings)
    report["summaries"]["warnings"] = len(audit_warnings) + len(completeness_warnings)
    write_json(Path(args.out), report, args.pretty)

    print(
        "audit-full-pipeline: "
        f"errors={len(audit_errors)} warnings={len(audit_warnings) + len(completeness_warnings)} "
        f"fullGameRoots={len(resource_asset_dirs)} "
        f"queueProcessedOrPresent={coverage['processedOrPresent']}/{coverage['totalQueueRecords']}",
        flush=True,
    )
    return 0 if len(audit_errors) == 0 else 2


def load_retry_targets(summary_path: Path, retry_success: bool) -> list[dict[str, Any]]:
    data = load_json_optional(summary_path)
    if not isinstance(data, list):
        raise ValueError(f"summary must be a JSON array: {summary_path}")

    targets: list[dict[str, Any]] = []
    for item in data:
        if not isinstance(item, dict):
            continue
        if item.get("ok") and not retry_success:
            continue
        fbx = item.get("fbx")
        wmesh = item.get("wmesh")
        if not fbx or not wmesh:
            continue
        targets.append(item)
    return targets


def retry_missing_wmesh_command(args: argparse.Namespace) -> int:
    summary_path = Path(args.summary)
    blender = Path(args.blender)
    blender_script = Path(args.blender_script)
    converter = Path(args.converter)
    normalized_root = Path(args.normalized_root)
    normalized_root.mkdir(parents=True, exist_ok=True)

    targets = load_retry_targets(summary_path, args.retry_success)
    if args.limit > 0:
        targets = targets[: args.limit]

    records: list[dict[str, Any]] = []
    for index, item in enumerate(targets, start=1):
        asset_id = str(item.get("id") or Path(str(item.get("fbx"))).stem)
        source_fbx = Path(str(item["fbx"]))
        output_wmesh = Path(str(item["wmesh"]))
        normalized_fbx = normalized_root / f"{safe_filename(asset_id)}_normalized.fbx"

        record: dict[str, Any] = {
            "index": index,
            "count": len(targets),
            "id": asset_id,
            "sourceFbx": str(source_fbx),
            "normalizedFbx": str(normalized_fbx),
            "wmesh": str(output_wmesh),
            "wmat": str(item.get("wmat") or output_wmesh.with_suffix(".wmat")),
            "sourceExists": source_fbx.exists(),
            "ok": False,
        }
        print(f"[{index}/{len(targets)}] retry {asset_id}", flush=True)

        if not source_fbx.exists():
            record["detail"] = "source FBX missing"
            records.append(record)
            continue

        if args.skip_normalize and normalized_fbx.exists():
            normalize_result = {
                "ok": True,
                "exitCode": 0,
                "stdout": "skipped existing normalized FBX",
                "stderr": "",
            }
        else:
            normalize_result = run_process(
                [
                    str(blender),
                    "--factory-startup",
                    "--background",
                    "--python",
                    str(blender_script),
                    "--",
                    "--input",
                    str(source_fbx),
                    "--output",
                    str(normalized_fbx),
                ],
                args.blender_timeout,
            )
        record["normalize"] = normalize_result
        record["normalizedExists"] = normalized_fbx.exists()
        record["normalizedBytes"] = normalized_fbx.stat().st_size if normalized_fbx.exists() else 0

        if not normalize_result.get("ok") or not normalized_fbx.exists() or normalized_fbx.stat().st_size == 0:
            record["detail"] = "Blender normalized export failed"
            records.append(record)
            continue

        output_wmesh.parent.mkdir(parents=True, exist_ok=True)
        convert_result = run_process(
            [str(converter), "mesh", str(normalized_fbx), "-o", str(output_wmesh)],
            args.converter_timeout,
        )
        record["convert"] = convert_result
        record["wmeshExists"] = output_wmesh.exists()
        record["wmeshBytes"] = output_wmesh.stat().st_size if output_wmesh.exists() else 0
        wmat_path = Path(str(record["wmat"]))
        record["wmatExists"] = wmat_path.exists()
        record["wmatBytes"] = wmat_path.stat().st_size if wmat_path.exists() else 0

        if convert_result.get("ok") and output_wmesh.exists() and output_wmesh.stat().st_size > 0:
            record["info"] = converter_info(converter, output_wmesh)
            record["ok"] = True
            record["detail"] = "normalized retry converted"
        else:
            record["detail"] = "WintersAssetConverter retry failed"
        records.append(record)

    manifest = {
        "schema": "winters.elden.retry_missing_wmesh.v1",
        "generatedAt": now_utc_iso(),
        "generatedBy": "Tools/EldenAssetPipeline/elden_pipeline.py retry-missing-wmesh",
        "summary": str(summary_path),
        "normalizedRoot": str(normalized_root),
        "counts": {
            "targets": len(targets),
            "ok": sum(1 for item in records if item.get("ok")),
            "failed": sum(1 for item in records if not item.get("ok")),
        },
        "records": records,
    }
    write_json(Path(args.out), manifest, args.pretty)
    return 0 if manifest["counts"]["failed"] == 0 else 2


SKEL_INFO_HASH_RE = re.compile(r"\bhash=(0x[0-9a-fA-F]+)")
SKEL_INFO_BONES_RE = re.compile(r"\[Skel\] bones=(\d+)")
MESH_INFO_BONES_RE = re.compile(r"\[Mesh\] submeshes=\d+ bones=(\d+)")
WMAT_ENTRY_SIZE = 596
WMAT_NAME_BYTES = 64
WMAT_PATH_WCHARS = 260


def read_wmat_entries(path: Path) -> tuple[bytes, list[dict[str, Any]]]:
    """Parse a .wmat file: 16B WintersFileHeader + 8B MaterialMetaHeader + 596B entries."""
    blob = path.read_bytes()
    if len(blob) < 24 or blob[0:4] != b"WINT":
        raise ValueError(f"not a Winters file: {path}")
    if blob[16:20] != b"WMAT":
        raise ValueError(f"not a WMAT payload: {path}")
    count = int.from_bytes(blob[20:24], "little")
    entries: list[dict[str, Any]] = []
    offset = 24
    for index in range(count):
        raw = blob[offset : offset + WMAT_ENTRY_SIZE]
        if len(raw) < WMAT_ENTRY_SIZE:
            break
        name = raw[12 : 12 + WMAT_NAME_BYTES].split(b"\x00", 1)[0].decode("utf-8", "replace")
        path_bytes = raw[12 + WMAT_NAME_BYTES : WMAT_ENTRY_SIZE]
        diffuse = path_bytes.decode("utf-16-le", "replace").split("\x00", 1)[0]
        entries.append({"offset": offset, "index": index, "name": name, "diffuse": diffuse})
        offset += WMAT_ENTRY_SIZE
    return blob, entries


def write_wmat_diffuse(blob: bytearray, entry_offset: int, diffuse_path: str) -> None:
    encoded = diffuse_path.encode("utf-16-le")[: (WMAT_PATH_WCHARS - 1) * 2]
    field = encoded + b"\x00" * (WMAT_PATH_WCHARS * 2 - len(encoded))
    start = entry_offset + 12 + WMAT_NAME_BYTES
    blob[start : start + WMAT_PATH_WCHARS * 2] = field


def parse_part_xml(path: Path) -> dict[str, Any] | None:
    try:
        root = ET.parse(path).getroot()
    except ET.ParseError:
        return None

    def vec(name: str, default: float) -> dict[str, float]:
        elem = child(root, name)
        if elem is None:
            return {"x": default, "y": default, "z": default}
        return {
            "x": float(child_text(elem, "X", str(default)) or default),
            "y": float(child_text(elem, "Y", str(default)) or default),
            "z": float(child_text(elem, "Z", str(default)) or default),
        }

    return {
        "name": child_text(root, "Name", path.stem),
        "model": child_text(root, "ModelName", ""),
        "position": vec("Position", 0.0),
        "rotationDeg": vec("Rotation", 0.0),
        "scale": vec("Scale", 1.0),
    }


def resolve_placement_wmesh(
    model: str,
    kind: str,
    resource_root: Path,
    repo_root: Path,
    tile: str,
) -> str | None:
    candidates: list[Path] = []
    if kind == "MapPiece":
        # MSB model m423604 -> mapbnd record map_m60_<tile>_<tile>_423604.mapbnd
        digits = re.sub(r"\D", "", model)[-6:]
        area = tile.split("_", 1)[0]
        binder = resource_root / "FullGame" / "map" / "map-binder" / f"map_{area}_{tile}_{tile}_{digits}.mapbnd" / "Model"
        candidates.extend(sorted(binder.glob("*.wmesh")))
    elif kind == "Asset":
        static_root = resource_root / "Assets" / "LimgraveStatic"
        if static_root.exists():
            primary = [d for d in sorted(static_root.glob(f"{model}_*")) if not d.name.endswith("_1")]
            for directory in primary + sorted(static_root.glob(f"{model}_*")):
                candidates.extend(sorted((directory / "Model").glob("*.wmesh")))
        lower = model.lower()
        family = lower.split("_", 1)[0]
        binder = (
            resource_root / "FullGame" / "asset" / "asset-geometry-binder"
            / f"asset_aeg_{family}_{lower}.geombnd" / "Model"
        )
        preferred = [p for p in sorted(binder.glob("*.wmesh")) if not p.stem.endswith("_1")]
        candidates.extend(preferred + sorted(binder.glob("*.wmesh")))
    elif kind == "Enemy":
        runtime = resource_root / "Runtime" / "Character" / model.lower() / f"{model.lower()}.wmesh"
        if runtime.exists():
            candidates.append(runtime)

    for candidate in candidates:
        if candidate.exists():
            try:
                return candidate.resolve().relative_to(repo_root.resolve()).as_posix()
            except ValueError:
                return str(candidate).replace("\\", "/")
    return None


# Stem may itself contain brackets (C[c3000]_...), so split on pipes only.
WMAT_MATBIN_TOKEN_RE = re.compile(r"\[\s*\d+\s*\|\s*([^|]+?)\s*\|")


def rewrite_wmat_from_matbin_command(args: argparse.Namespace) -> int:
    """Fill empty .wmat diffuse paths via MATBIN: wmat material name carries the
    MATBIN stem (Soulstruct naming `name [slot | matbin | model]`); serialize the
    matbin with WitchyBND, parse its albedo sampler, resolve against extracted
    AET/map-tile DDS, and write the repo-relative path back into the wmat."""
    resource_root = Path(args.resource_root)
    repo_root = Path(args.repo_root) if args.repo_root else Path.cwd()
    work_root = Path(args.work_root)
    xml_cache = work_root / "matbin_xml_cache"
    xml_cache.mkdir(parents=True, exist_ok=True)

    matbin_root = Path(args.matbin_root) if args.matbin_root else (
        resource_root / "FullGame" / "material" / "dcx" / "material_allmaterial.matbinbnd" / "Material"
    )
    print("indexing matbin stems...", flush=True)
    matbin_index: dict[str, Path] = {}
    for path in matbin_root.rglob("*.matbin"):
        matbin_index.setdefault(path.stem.lower(), path)
    print(f"matbin stems: {len(matbin_index)}", flush=True)

    print("indexing texture stems...", flush=True)
    texture_roots = [Path(p) for p in args.texture_root] or [resource_root / "FullGame" / "asset" / "dcx"]
    texture_index = build_texture_index(texture_roots)
    print(f"texture stems: {len(texture_index)}", flush=True)

    # Collect target wmat files from placement TXT files (wmesh -> sibling wmat).
    wmats: list[Path] = []
    seen: set[str] = set()
    for placement in args.placement:
        for line in Path(placement).read_text(encoding="utf-8").splitlines():
            if not line or line.startswith("#"):
                continue
            fields = line.split("|")
            if len(fields) < 4 or not fields[3]:
                continue
            wmat = (repo_root / fields[3]).with_suffix(".wmat")
            key = str(wmat).lower()
            if key not in seen and wmat.exists():
                seen.add(key)
                wmats.append(wmat)
    for extra in args.wmat:
        wmat = Path(extra)
        if wmat.exists() and str(wmat).lower() not in seen:
            wmats.append(wmat)
    print(f"target wmats: {len(wmats)}", flush=True)

    def matbin_xml_for(stem: str) -> Path | None:
        source = None
        for candidate in [stem] + normalized_token_candidates(stem):
            source = matbin_index.get(candidate.lower())
            if source is not None:
                break
        if source is None:
            return None
        cached = xml_cache / f"{source.name}.xml"
        if cached.exists():
            return cached
        staged = xml_cache / source.name
        if not staged.exists():
            shutil.copy2(source, staged)
        run_process([str(Path(args.witchy)), "--unpack", "--passive", str(staged)], args.witchy_timeout)
        produced = xml_cache / f"{source.name}.xml"
        return produced if produced.exists() else None

    parsed_cache: dict[str, dict[str, Any] | None] = {}
    records: list[dict[str, Any]] = []
    total_resolved = total_unresolved = 0
    for index, wmat in enumerate(wmats, start=1):
        blob = bytearray(wmat.read_bytes())
        _, entries = read_wmat_entries(wmat)
        resolved = unresolved = skipped = 0
        for entry in entries:
            if entry["diffuse"] and not args.force:
                skipped += 1
                continue
            match = WMAT_MATBIN_TOKEN_RE.search(entry["name"])
            if not match:
                unresolved += 1
                continue
            stem = match.group(1).strip()
            if stem not in parsed_cache:
                xml_path = matbin_xml_for(stem)
                parsed_cache[stem] = parse_matbin_xml(xml_path, texture_index) if xml_path else None
            material = parsed_cache[stem]
            chosen = None
            if material:
                primary = material.get("primary", {})
                chosen = primary.get("albedo") or primary.get("diffuse")
            if chosen and Path(chosen).exists():
                rel = Path(chosen).resolve().relative_to(repo_root.resolve()).as_posix()
                write_wmat_diffuse(blob, entry["offset"], rel)
                resolved += 1
            else:
                unresolved += 1
        if resolved:
            wmat.write_bytes(bytes(blob))
        total_resolved += resolved
        total_unresolved += unresolved
        records.append({"wmat": str(wmat), "resolved": resolved, "unresolved": unresolved, "kept": skipped})
        if index % 25 == 0:
            print(f"[{index}/{len(wmats)}] resolved={total_resolved} unresolved={total_unresolved}", flush=True)

    manifest = {
        "schema": "winters.elden.wmat_matbin_rewrite.v1",
        "generatedAt": now_utc_iso(),
        "counts": {
            "wmats": len(wmats),
            "resolved": total_resolved,
            "unresolved": total_unresolved,
        },
        "records": records,
    }
    write_json(Path(args.out), manifest, True)
    print(f"DONE resolved={total_resolved} unresolved={total_unresolved}", flush=True)
    return 0


def build_map_placement_command(args: argparse.Namespace) -> int:
    """Generate winters.elden.map_placement.v1 (JSON + engine-friendly TXT) from MSB Part XMLs."""
    resource_root = Path(args.resource_root)
    repo_root = Path(args.repo_root) if args.repo_root else Path.cwd()
    tile = args.map_tile
    raw_root = (
        resource_root / "FullGame" / "map" / "map-msb"
        / f"map_mapstudio_{tile}.msb" / "Raw" / f"{tile}-msb-dcx" / "Part"
    )
    if not raw_root.exists():
        raise FileNotFoundError(f"MSB Part XML root not found: {raw_root}")

    parts: list[dict[str, Any]] = []
    unresolved: list[dict[str, Any]] = []
    kinds = ("MapPiece", "Asset", "Enemy", "Player")
    for kind in kinds:
        kind_dir = raw_root / kind
        if not kind_dir.exists():
            continue
        for xml_path in sorted(kind_dir.glob("*.xml")):
            part = parse_part_xml(xml_path)
            if part is None or not part["model"]:
                continue
            part["kind"] = kind
            if kind == "Player":
                part["wmesh"] = None
                parts.append(part)
                continue
            wmesh = resolve_placement_wmesh(part["model"], kind, resource_root, repo_root, tile)
            if wmesh is None:
                unresolved.append(part)
                continue
            part["wmesh"] = wmesh
            parts.append(part)

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    manifest = {
        "schema": "winters.elden.map_placement.v1",
        "generatedAt": now_utc_iso(),
        "generatedBy": "Tools/EldenAssetPipeline/elden_pipeline.py build-map-placement",
        "mapTile": tile,
        "counts": {
            "placed": len(parts),
            "unresolved": len(unresolved),
            "byKind": dict(Counter(part["kind"] for part in parts)),
        },
        "parts": parts,
        "unresolved": unresolved,
    }
    write_json(out_dir / "map_placement.json", manifest, True)

    # Engine-friendly flat text: kind|name|model|wmesh|px py pz|rx ry rz|sx sy sz
    lines = ["# winters.elden.map_placement.v1 " + tile]
    for part in parts:
        p, r, s = part["position"], part["rotationDeg"], part["scale"]
        lines.append(
            f"{part['kind']}|{part['name']}|{part['model']}|{part['wmesh'] or ''}|"
            f"{p['x']:.6f} {p['y']:.6f} {p['z']:.6f}|"
            f"{r['x']:.6f} {r['y']:.6f} {r['z']:.6f}|"
            f"{s['x']:.6f} {s['y']:.6f} {s['z']:.6f}"
        )
    (out_dir / "map_placement.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"placed={len(parts)} unresolved={len(unresolved)} byKind={manifest['counts']['byKind']}")
    return 0


def cook_runtime_character_command(args: argparse.Namespace) -> int:
    """Re-cook FullGame chrbnd output into the LoL-convention runtime layout:
    Runtime/Character/<id>/{<id>.wmesh,<id>.wskel,<id>.wmat,anims/*.wanim,textures/*.dds}
    with skel-driven bone tables and repo-relative texture paths."""
    resource_root = Path(args.resource_root)
    runtime_root = Path(args.runtime_root) if args.runtime_root else resource_root / "Runtime"
    converter = Path(args.converter)
    repo_root = Path(args.repo_root) if args.repo_root else Path.cwd()

    records: list[dict[str, Any]] = []
    for character in args.character:
        binder_root = resource_root / "FullGame" / "chr" / "character-binder" / f"chr_{character}.chrbnd"
        model_dir = binder_root / "Model"
        anim_src = binder_root / "Animation"
        dst = runtime_root / "Character" / character
        record: dict[str, Any] = {"character": character, "source": str(binder_root), "runtimeDir": str(dst), "ok": False}
        records.append(record)

        fbx = next(iter(sorted(model_dir.glob("*.fbx"))), None)
        if fbx is None:
            record["detail"] = "cooked FBX missing; run run-full-pipeline for this chrbnd first"
            continue

        dst.mkdir(parents=True, exist_ok=True)
        wskel = dst / f"{character}.wskel"
        wmesh = dst / f"{character}.wmesh"
        wmat = dst / f"{character}.wmat"

        skel_result = run_process([str(converter), "skel", str(fbx), "-o", str(wskel)], args.converter_timeout)
        record["skel"] = {"ok": skel_result.get("ok"), "exitCode": skel_result.get("exitCode")}
        if not skel_result.get("ok") or not wskel.exists():
            record["detail"] = "skel conversion failed"
            continue
        # Parse bone count/hash from the writer output first: `info` runs the
        # loader, which refuses >512-bone skeletons and would report nothing.
        write_text = (skel_result.get("stdout") or "") + (skel_result.get("stderr") or "")
        skel_bones = int(match.group(1)) if (match := re.search(r"\bbones=(\d+)", write_text)) else 0
        skel_hash = match.group(1) if (match := SKEL_INFO_HASH_RE.search(write_text)) else None
        if not skel_bones or not skel_hash:
            skel_text = converter_info_text(converter, wskel, args.converter_timeout)
            skel_bones = skel_bones or (int(match.group(1)) if (match := SKEL_INFO_BONES_RE.search(skel_text)) else 0)
            skel_hash = skel_hash or (match.group(1) if (match := SKEL_INFO_HASH_RE.search(skel_text)) else None)
        record["skelBones"] = skel_bones
        record["skelHash"] = skel_hash

        if skel_bones > args.max_bones:
            record["detail"] = f"bone count {skel_bones} exceeds runtime limit {args.max_bones}; skinned mesh blocked"
            record["blocked"] = "bone-limit"
        else:
            mesh_result = run_process(
                [str(converter), "mesh", str(fbx), "--skel", str(wskel), "-o", str(wmesh)],
                args.converter_timeout,
            )
            record["mesh"] = {"ok": mesh_result.get("ok"), "exitCode": mesh_result.get("exitCode"),
                              "stderrTail": (mesh_result.get("stderr") or "")[-300:]}
            if mesh_result.get("ok") and wmesh.exists():
                mesh_text = converter_info_text(converter, wmesh, args.converter_timeout)
                mesh_bones = int(match.group(1)) if (match := MESH_INFO_BONES_RE.search(mesh_text)) else -1
                record["meshInfo"] = mesh_text.splitlines()[0] if mesh_text else ""
                record["meshBones"] = mesh_bones
                record["boneCountsMatch"] = mesh_bones == skel_bones
            else:
                record["detail"] = "skel-driven mesh conversion failed"

        anim_dst = dst / "anims"
        anim_dst.mkdir(exist_ok=True)
        copied_anims = 0
        anim_hash_ok = None
        if anim_src.exists():
            for wanim in sorted(anim_src.glob("*.wanim")):
                shutil.copy2(wanim, anim_dst / wanim.name)
                copied_anims += 1
            first = next(iter(sorted(anim_dst.glob("*.wanim"))), None)
            if first is not None and skel_hash:
                anim_text = converter_info_text(converter, first, args.converter_timeout)
                anim_match = WANIM_INFO_RE.search(anim_text)
                anim_hash_ok = bool(anim_match and anim_match.group("skel_hash") == skel_hash)
        record["wanimCopied"] = copied_anims
        record["wanimSkelHashMatches"] = anim_hash_ok

        tex_dst = dst / "textures"
        tex_dst.mkdir(exist_ok=True)
        copied_textures = 0
        for variant in (f"chr_{character}_h.texbnd", f"chr_{character}_l.texbnd"):
            tex_root = resource_root / "FullGame" / "chr" / "character-texture-binder" / variant / "Texture"
            if not tex_root.exists():
                continue
            for dds in sorted(tex_root.rglob("*.dds")):
                target = tex_dst / dds.name
                if not target.exists():
                    shutil.copy2(dds, target)
                    copied_textures += 1
            if copied_textures:
                break
        record["texturesCopied"] = copied_textures

        resolved = unresolved = 0
        if wmat.exists():
            texture_index = build_texture_index([tex_dst])
            blob = bytearray(wmat.read_bytes())
            _, entries = read_wmat_entries(wmat)
            for entry in entries:
                token = extract_material_token(entry["name"])
                candidates = normalized_token_candidates(token)
                found = resolve_role_from_suffix("albedo", candidates, texture_index)
                if found:
                    rel = Path(found).resolve().relative_to(repo_root.resolve()).as_posix()
                    write_wmat_diffuse(blob, entry["offset"], rel)
                    resolved += 1
                else:
                    unresolved += 1
            wmat.write_bytes(bytes(blob))
        record["wmatDiffuseResolved"] = resolved
        record["wmatDiffuseUnresolved"] = unresolved

        record["ok"] = bool(
            record.get("blocked") == "bone-limit"
            or (record.get("mesh", {}).get("ok") and record.get("boneCountsMatch"))
        )
        if record["ok"] and not record.get("detail"):
            record["detail"] = "cooked"

    manifest = {
        "schema": "winters.elden.runtime_character_cook.v1",
        "generatedAt": now_utc_iso(),
        "generatedBy": "Tools/EldenAssetPipeline/elden_pipeline.py cook-runtime-character",
        "runtimeRoot": str(runtime_root),
        "counts": {
            "characters": len(records),
            "ok": sum(1 for item in records if item.get("ok")),
            "failed": sum(1 for item in records if not item.get("ok")),
        },
        "records": records,
    }
    write_json(Path(args.out), manifest, args.pretty)
    return 0 if manifest["counts"]["failed"] == 0 else 2
WANIM_INFO_RE = re.compile(r"channels=(?P<channels>\d+).*?skel_hash=(?P<skel_hash>0x[0-9a-fA-F]+)")


def converter_info_text(converter: Path, binary_path: Path, timeout_seconds: int) -> str:
    result = run_process([str(converter), "info", str(binary_path)], timeout_seconds)
    return ((result.get("stdout") or "") + (result.get("stderr") or "")).strip()


def convert_hkx_anim_command(args: argparse.Namespace) -> int:
    """Sample HKX animation pipeline: anibnd HKX -> Soulstruct Blender import -> FBX -> .wanim."""
    game_root = Path(args.game_root)
    resource_root = Path(args.resource_root)
    work_root = Path(args.work_root)
    converter = Path(args.converter)
    script_path = work_root / "scripts" / "hkx_anim_to_fbx_generated.py"
    if not script_path.exists() or script_path.read_text(encoding="utf-8") != HKX_ANIM_TO_FBX_BLENDER_SCRIPT:
        write_text_file(script_path, HKX_ANIM_TO_FBX_BLENDER_SCRIPT)

    jobs: list[dict[str, Any]] = []
    records: list[dict[str, Any]] = []
    for character in args.character:
        binder_root = resource_root / "FullGame" / "chr" / "character-binder" / f"chr_{character}.chrbnd"
        model_dir = binder_root / "Model"
        fbx = next(iter(sorted(model_dir.glob("*.fbx"))), None)
        wskel = next(iter(sorted(model_dir.glob("*.wskel"))), None)
        anibnd = game_root / "chr" / f"{character}.anibnd.dcx"
        record: dict[str, Any] = {
            "character": character,
            "fbx": path_for_json(fbx),
            "wskel": path_for_json(wskel),
            "anibnd": path_for_json(anibnd) if anibnd.exists() else None,
            "animationDir": str(binder_root / "Animation"),
            "ok": False,
        }
        if fbx is None or wskel is None:
            record["detail"] = "cooked FBX/wskel missing; run run-full-pipeline for this chrbnd first"
            records.append(record)
            continue
        if not anibnd.exists():
            record["detail"] = "anibnd missing in game root"
            records.append(record)
            continue
        jobs.append(
            {
                "model": character,
                "fbx": str(fbx),
                "anibnd": str(anibnd),
                "output": str(work_root / "anim_batches" / character / f"{character}_hkx_anims.fbx"),
                "animFilter": args.anim_filter,
                "maxAnims": args.max_anims,
            }
        )
        records.append(record)

    blender_records: list[dict[str, Any]] = []
    if jobs:
        batch_dir = work_root / "anim_batches"
        input_json = batch_dir / "hkx_anim_inputs.json"
        summary_json = batch_dir / "hkx_anim_summary.json"
        write_json(input_json, jobs, True)
        blender_result = run_process(
            [
                str(Path(args.blender)),
                "--factory-startup",
                "--background",
                "--python",
                str(script_path),
                "--",
                "--input-json",
                str(input_json),
                "--summary",
                str(summary_json),
                "--soulstruct-root",
                str(Path(args.soulstruct_root)),
            ],
            args.blender_timeout,
        )
        loaded = load_json_optional(summary_json)
        blender_records = loaded if isinstance(loaded, list) else []
        for record in records:
            record["blenderExitCode"] = blender_result.get("exitCode")

    by_model = {str(item.get("model")): item for item in blender_records if isinstance(item, dict)}
    for record in records:
        blender_record = by_model.get(str(record["character"]))
        if record.get("detail") or blender_record is None:
            continue
        record["blender"] = {
            key: blender_record.get(key)
            for key in ("ok", "actions", "animsRequested", "animsImported", "failedEntries", "detail", "bytes")
        }
        if not blender_record.get("ok"):
            record["detail"] = "Blender HKX import/export failed"
            continue

        anim_fbx = Path(str(blender_record.get("output")))
        anim_dir = Path(record["animationDir"])
        anim_dir.mkdir(parents=True, exist_ok=True)
        convert_result = run_process(
            [str(converter), "anim", str(anim_fbx), "--skel", str(record["wskel"]), "-o", str(anim_dir)],
            args.converter_timeout,
        )
        record["convert"] = {
            "ok": convert_result.get("ok"),
            "exitCode": convert_result.get("exitCode"),
            "stderrTail": (convert_result.get("stderr") or "")[-500:],
        }
        wanims = sorted(anim_dir.glob("*.wanim"))
        record["wanimCount"] = len(wanims)
        if not wanims:
            record["detail"] = "converter produced no .wanim"
            continue

        skel_text = converter_info_text(converter, Path(str(record["wskel"])), args.converter_timeout)
        skel_hash = match.group(1) if (match := SKEL_INFO_HASH_RE.search(skel_text)) else None
        first_anim_text = converter_info_text(converter, wanims[0], args.converter_timeout)
        anim_match = WANIM_INFO_RE.search(first_anim_text)
        record["validation"] = {
            "wskelHash": skel_hash,
            "firstWanim": wanims[0].name,
            "firstWanimInfo": first_anim_text.splitlines()[0] if first_anim_text else "",
            "wanimSkelHash": anim_match.group("skel_hash") if anim_match else None,
            "channels": int(anim_match.group("channels")) if anim_match else None,
            "skelHashMatches": bool(skel_hash and anim_match and skel_hash == anim_match.group("skel_hash")),
        }
        record["ok"] = bool(convert_result.get("ok")) and record["validation"]["skelHashMatches"]
        if not record["ok"] and not record.get("detail"):
            record["detail"] = "wanim produced but skel hash validation failed"

    manifest = {
        "schema": "winters.elden.hkx_anim_conversion.v1",
        "generatedAt": now_utc_iso(),
        "generatedBy": "Tools/EldenAssetPipeline/elden_pipeline.py convert-hkx-anim",
        "gameRoot": str(game_root),
        "resourceRoot": str(resource_root),
        "animFilter": args.anim_filter,
        "maxAnims": args.max_anims,
        "counts": {
            "characters": len(records),
            "ok": sum(1 for item in records if item.get("ok")),
            "failed": sum(1 for item in records if not item.get("ok")),
        },
        "records": records,
    }
    write_json(Path(args.out), manifest, args.pretty)
    return 0 if manifest["counts"]["failed"] == 0 else 2


def write_json(path: Path, data: Any, pretty: bool) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    text = json.dumps(data, ensure_ascii=False, indent=2 if pretty else None)
    path.write_text(text + "\n", encoding="utf-8")


def add_common_json_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--out", required=True, help="Output JSON path.")
    parser.add_argument("--pretty", action="store_true", default=True, help="Pretty-print JSON.")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Elden asset pipeline manifest tools.")
    sub = parser.add_subparsers(dest="command", required=True)

    matbin = sub.add_parser("parse-matbin", help="Parse WitchyBND MATBIN XML to a material manifest.")
    matbin.add_argument("--input", required=True, help="MATBIN XML file or directory.")
    matbin.add_argument("--texture-root", action="append", default=[], help="Texture search root. Can repeat.")
    add_common_json_args(matbin)
    matbin.set_defaults(func=parse_matbin_command)

    fxr = sub.add_parser("parse-fxr", help="Parse WitchyBND FXR XML to a normalized FX manifest.")
    fxr.add_argument("--input", required=True, help="FXR XML file or directory.")
    add_common_json_args(fxr)
    fxr.set_defaults(func=parse_fxr_command)

    resolve_fxr = sub.add_parser(
        "resolve-fxr",
        help="Resolve FXR integer candidates to local SFX texture/model resources.",
    )
    resolve_fxr.add_argument("--fxr-manifest", required=True, help="Output of parse-fxr.")
    resolve_fxr.add_argument("--resource-root", action="append", default=[], help="SFX resource root. Can repeat.")
    resolve_fxr.add_argument(
        "--min-resource-id",
        type=int,
        default=1000,
        help="Ignore integer candidates below this absolute value.",
    )
    resolve_fxr.add_argument(
        "--max-locations",
        type=int,
        default=8,
        help="Maximum FXR action locations to keep per resolved value.",
    )
    add_common_json_args(resolve_fxr)
    resolve_fxr.set_defaults(func=resolve_fxr_resources_command)

    bindings = sub.add_parser("build-bindings", help="Build Blender material bindings from material names.")
    bindings.add_argument("--materials-json", required=True, help="JSON from blender_apply_materials.py --inspect-json.")
    bindings.add_argument("--matbin-manifest", help="Output of parse-matbin.")
    bindings.add_argument("--fxr-resource-manifest", help="Output of resolve-fxr.")
    bindings.add_argument("--texture-root", action="append", default=[], help="Texture search root. Can repeat.")
    add_common_json_args(bindings)
    bindings.set_defaults(func=build_bindings_command)

    catalog = sub.add_parser(
        "build-resource-catalog",
        help="Scan cooked Elden Ring Resource outputs into a unified editor/runtime catalog.",
    )
    catalog.add_argument("--resource-root", required=True, help="Cooked Resource/EldenRing root.")
    catalog.add_argument("--game-root", help="UXM loose ELDEN RING/Game root.")
    catalog.add_argument("--witchy-root", help="WitchyBND extraction work root.")
    catalog.add_argument("--work-root", help="Pipeline work root containing conversion summaries.")
    catalog.add_argument("--converter", help="WintersAssetConverter.exe path for binary info probes.")
    catalog.add_argument(
        "--runtime-bone-limit",
        type=int,
        default=512,
        help="Current runtime WMesh loader bone limit used to mark extracted-but-blocked meshes.",
    )
    catalog.add_argument("--texture-out", help="Optional separate texture index JSON path.")
    add_common_json_args(catalog)
    catalog.set_defaults(func=build_resource_catalog_command)

    sync_ui = sub.add_parser(
        "sync-ui-menu",
        help="Copy UXM loose menu UI files into Resource/EldenRing/UI and write a raw UI manifest.",
    )
    sync_ui.add_argument("--source-menu-root", required=True, help="Loose ELDEN RING/Game/menu root.")
    sync_ui.add_argument("--output-root", required=True, help="Output Resource UI root.")
    sync_ui.add_argument(
        "--ext",
        action="append",
        default=[".gfx", ".dcx", ".tpfbdt", ".tpfbhd"],
        help="Extension to copy. Can repeat.",
    )
    sync_ui.add_argument("--force", action="store_true", help="Copy even when output is already current.")
    add_common_json_args(sync_ui)
    sync_ui.set_defaults(func=sync_ui_menu_command)

    sync_source = sub.add_parser(
        "sync-source-bundles",
        help="Copy UXM loose source binders for repeatable Elden Ring extraction passes.",
    )
    sync_source.add_argument("--game-root", required=True, help="UXM loose ELDEN RING/Game root.")
    sync_source.add_argument("--output-root", required=True, help="Output Resource/EldenRing/SourceBundles root.")
    sync_source.add_argument(
        "--map-tile",
        default="m60_42_36_00",
        help="Map tile directory to stage from Game/map/<area>/<tile>.",
    )
    sync_source.add_argument(
        "--character-id",
        action="append",
        help="Extra character ID to stage in addition to the default Limgrave target set.",
    )
    sync_source.add_argument(
        "--static-manifest",
        help="Cooked static manifest used to recover source AEG geombnd binders.",
    )
    sync_source.add_argument("--include-parts", action="store_true", help="Stage all Game/parts item/equipment binders.")
    sync_source.add_argument("--ui-menu-manifest", help="Existing UI/MenuRaw manifest to link, not duplicate.")
    sync_source.add_argument("--asset-catalog", help="Existing cooked asset catalog to link.")
    sync_source.add_argument("--force", action="store_true", help="Copy even when output is already current.")
    add_common_json_args(sync_source)
    sync_source.set_defaults(func=sync_source_bundles_command)

    dds = sub.add_parser(
        "convert-textures-dds",
        help="Convert extracted PNG textures to side-by-side DDS files through texconv.",
    )
    dds.add_argument("--texture-root", required=True, help="Root directory to scan for PNG textures.")
    dds.add_argument("--texconv", required=True, help="texconv.exe path.")
    dds.add_argument(
        "--format",
        help="Override DDS format for every texture. Defaults to BC7 sRGB for color and BC7 linear otherwise.",
    )
    dds.add_argument("--mip-levels", type=int, default=0, help="Mip levels passed to texconv -m. 0 means full chain.")
    dds.add_argument("--timeout", type=int, default=120, help="Per-texture texconv timeout in seconds.")
    dds.add_argument("--limit", type=int, default=0, help="Limit PNG count for smoke testing.")
    dds.add_argument("--force", action="store_true", help="Rebuild DDS outputs even when current.")
    add_common_json_args(dds)
    dds.set_defaults(func=convert_textures_dds_command)

    editor_seeds = sub.add_parser(
        "build-editor-map-seeds",
        help="Split a map assembly into World Partition, Sequencer, and Editor seed JSON files.",
    )
    editor_seeds.add_argument("--map-assembly", required=True, help="Input map_assembly.json path.")
    editor_seeds.add_argument("--out-dir", required=True, help="Directory for editor seed JSON outputs.")
    editor_seeds.add_argument("--asset-catalog", help="Catalog path to link from editor_map_seed.json.")
    editor_seeds.add_argument("--source-bundles", help="SourceBundles manifest path to link from editor_map_seed.json.")
    editor_seeds.add_argument("--pretty", action="store_true", default=True, help="Pretty-print JSON.")
    editor_seeds.set_defaults(func=build_editor_map_seeds_command)

    game_index = sub.add_parser(
        "index-game-root",
        help="Index the full UXM loose ELDEN RING/Game folder and build a reference extraction queue.",
    )
    game_index.add_argument("--game-root", required=True, help="UXM loose ELDEN RING/Game root.")
    game_index.add_argument("--resource-root", help="Resource/EldenRing root to record as the pipeline target.")
    game_index.add_argument("--queue-out", help="Optional extraction queue JSON path.")
    add_common_json_args(game_index)
    game_index.set_defaults(func=index_game_root_command)

    full = sub.add_parser(
        "run-full-pipeline",
        help="Run a resumable chunk of the full Elden pipeline: Witchy unpack, collect, FBX/WMesh/WAnim, TPF/PNG->DDS, catalog.",
    )
    full.add_argument("--queue", required=True, help="eldenring_full_extraction_queue.json path.")
    full.add_argument("--game-root", required=True, help="UXM loose ELDEN RING/Game root.")
    full.add_argument("--resource-root", required=True, help="Resource/EldenRing output root.")
    full.add_argument("--work-root", required=True, help="Pipeline work root for temp unpack/status files.")
    full.add_argument("--witchy", required=True, help="WitchyBND.exe path.")
    full.add_argument("--blender", required=True, help="blender.exe path.")
    full.add_argument(
        "--soulstruct-root",
        default=r"C:\Users\tnest\Downloads\io_soulstruct-2.5.0 (1)",
        help="Soulstruct Blender add-on root used for FLVER import.",
    )
    full.add_argument(
        "--blender-normalize-script",
        default="Tools/EldenAssetPipeline/blender_apply_materials.py",
        help="Blender script used to normalized re-export FBX files that Assimp rejects.",
    )
    full.add_argument("--converter", required=True, help="WintersAssetConverter.exe path.")
    full.add_argument("--texconv", help="texconv.exe path for PNG->DDS bake after WitchyBND TPF unpack.")
    full.add_argument("--top-dir", action="append", help="Only process queue records from this top-level Game dir.")
    full.add_argument("--bundle-kind", action="append", help="Only process this classified bundle kind.")
    full.add_argument(
        "--relative-contains",
        action="append",
        help="Only process records whose relative path contains this token. Can repeat; all must match.",
    )
    full.add_argument("--offset", type=int, default=0, help="Skip this many filtered records.")
    full.add_argument("--limit", type=int, default=1, help="Maximum records to process in this run.")
    full.add_argument("--max-source-mib", type=int, default=0, help="Skip source files larger than this MiB.")
    full.add_argument("--min-free-gib", type=float, default=8.0, help="Stop before a batch if free disk is below this GiB.")
    full.add_argument("--witchy-timeout", type=int, default=600, help="Per-file WitchyBND timeout in seconds.")
    full.add_argument("--blender-timeout", type=int, default=900, help="Per-batch Blender FLVER import timeout in seconds.")
    full.add_argument("--converter-timeout", type=int, default=240, help="Per-FBX converter timeout in seconds.")
    full.add_argument("--texconv-timeout", type=int, default=120, help="Per-texture texconv timeout in seconds.")
    full.add_argument("--runtime-bone-limit", type=int, default=512, help="Catalog runtime bone limit.")
    full.add_argument("--recursive", action="store_true", help="Use WitchyBND recursive unpack.")
    full.add_argument("--resume", action="store_true", help="Skip records with successful status JSON.")
    full.add_argument("--clean-unpack", action="store_true", help="Delete temporary unpack output after cooking.")
    full.add_argument("--force-dds", action="store_true", help="Rebuild DDS files even when current.")
    full.add_argument("--continue-on-error", action="store_true", help="Continue to the next queue record after failures.")
    full.add_argument("--rebuild-catalog", action="store_true", help="Rebuild eldenring_asset_catalog.json after this run.")
    add_common_json_args(full)
    full.set_defaults(func=run_full_pipeline_command)

    audit = sub.add_parser(
        "audit-full-pipeline",
        help="Cross-check queue/status/resource/catalog outputs for the full Elden pipeline.",
    )
    audit.add_argument("--queue", required=True, help="eldenring_full_extraction_queue.json path.")
    audit.add_argument("--resource-root", required=True, help="Resource/EldenRing output root.")
    audit.add_argument("--work-root", required=True, help="Pipeline work root containing status files.")
    audit.add_argument("--catalog", help="Unified asset catalog JSON path to cross-check.")
    audit.add_argument("--converter", help="WintersAssetConverter.exe path for binary info probes.")
    audit.add_argument(
        "--runtime-bone-limit",
        type=int,
        default=512,
        help="Current runtime WMesh loader bone limit used to mark extracted-but-blocked meshes.",
    )
    audit.add_argument(
        "--max-path-warning",
        type=int,
        default=240,
        help="Warn when generated runtime paths approach the classic Windows MAX_PATH boundary.",
    )
    audit.add_argument(
        "--sample-limit",
        type=int,
        default=32,
        help="Maximum sample records to keep per unprocessed bucket.",
    )
    add_common_json_args(audit)
    audit.set_defaults(func=audit_full_pipeline_command)

    retry_wmesh = sub.add_parser(
        "retry-missing-wmesh",
        help="Normalize failed FBX files through Blender and retry Winters .wmesh conversion.",
    )
    retry_wmesh.add_argument("--summary", required=True, help="Previous WMesh conversion summary JSON.")
    retry_wmesh.add_argument("--blender", required=True, help="blender.exe path.")
    retry_wmesh.add_argument(
        "--blender-script",
        default="Tools/EldenAssetPipeline/blender_apply_materials.py",
        help="Blender helper script used for FBX import/export normalization.",
    )
    retry_wmesh.add_argument("--converter", required=True, help="WintersAssetConverter.exe path.")
    retry_wmesh.add_argument("--normalized-root", required=True, help="Directory for normalized FBX outputs.")
    retry_wmesh.add_argument("--retry-success", action="store_true", help="Retry successful records too.")
    retry_wmesh.add_argument("--skip-normalize", action="store_true", help="Reuse existing normalized FBX files.")
    retry_wmesh.add_argument("--limit", type=int, default=0, help="Limit retry count for smoke testing.")
    retry_wmesh.add_argument("--blender-timeout", type=int, default=240, help="Per-asset Blender timeout in seconds.")
    retry_wmesh.add_argument(
        "--converter-timeout",
        type=int,
        default=120,
        help="Per-asset WintersAssetConverter timeout in seconds.",
    )
    add_common_json_args(retry_wmesh)
    retry_wmesh.set_defaults(func=retry_missing_wmesh_command)

    hkx_anim = sub.add_parser(
        "convert-hkx-anim",
        help="Sample HKX animation pipeline: anibnd HKX -> Soulstruct Blender import -> FBX -> .wanim for cooked FullGame characters.",
    )
    hkx_anim.add_argument("--game-root", required=True, help="UXM loose ELDEN RING/Game root.")
    hkx_anim.add_argument("--resource-root", required=True, help="Resource/EldenRing root with cooked FullGame chrbnd output.")
    hkx_anim.add_argument("--work-root", required=True, help="Pipeline work root for scripts/anim batches.")
    hkx_anim.add_argument("--blender", required=True, help="blender.exe path.")
    hkx_anim.add_argument(
        "--soulstruct-root",
        default=r"C:\Users\tnest\Downloads\io_soulstruct-2.5.0 (1)",
        help="Soulstruct Blender add-on root used for HKX animation import.",
    )
    hkx_anim.add_argument("--converter", required=True, help="WintersAssetConverter.exe path.")
    hkx_anim.add_argument("--character", action="append", required=True, help="Character id like c2010. Can repeat.")
    hkx_anim.add_argument("--anim-filter", default="", help="Regex filter applied to anibnd HKX entry names.")
    hkx_anim.add_argument("--max-anims", type=int, default=8, help="Maximum HKX animations per character. 0 means all.")
    hkx_anim.add_argument("--blender-timeout", type=int, default=1800, help="Blender batch timeout in seconds.")
    hkx_anim.add_argument("--converter-timeout", type=int, default=300, help="Converter timeout in seconds.")
    add_common_json_args(hkx_anim)
    hkx_anim.set_defaults(func=convert_hkx_anim_command)

    cook = sub.add_parser(
        "cook-runtime-character",
        help="Re-cook FullGame chrbnd output into the LoL-convention runtime layout (same-stem wmesh/wskel/wmat + anims/ + textures/).",
    )
    cook.add_argument("--resource-root", required=True, help="Resource/EldenRing root with FullGame output.")
    cook.add_argument("--runtime-root", help="Runtime layout root. Default <resource-root>/Runtime.")
    cook.add_argument("--converter", required=True, help="WintersAssetConverter.exe path.")
    cook.add_argument("--repo-root", help="Repo root used to compute resource-relative texture paths. Default CWD.")
    cook.add_argument("--character", action="append", required=True, help="Character id like c2010. Can repeat.")
    cook.add_argument("--max-bones", type=int, default=512, help="Skinned mesh bone limit (engine loader cap).")
    cook.add_argument("--converter-timeout", type=int, default=300, help="Converter timeout in seconds.")
    add_common_json_args(cook)
    cook.set_defaults(func=cook_runtime_character_command)

    placement = sub.add_parser(
        "build-map-placement",
        help="Generate map placement JSON+TXT (model -> wmesh + PRS) from cooked MSB Part XMLs.",
    )
    placement.add_argument("--resource-root", required=True, help="Resource/EldenRing root.")
    placement.add_argument("--repo-root", help="Repo root for resource-relative wmesh paths. Default CWD.")
    placement.add_argument("--map-tile", default="m60_42_36_00", help="Map tile id.")
    placement.add_argument("--out-dir", required=True, help="Output directory for map_placement.json/.txt.")
    placement.set_defaults(func=build_map_placement_command)

    wmat_fix = sub.add_parser(
        "rewrite-wmat-from-matbin",
        help="Fill empty wmat diffuse paths by resolving MATBIN stems to extracted AET/map DDS textures.",
    )
    wmat_fix.add_argument("--resource-root", required=True, help="Resource/EldenRing root.")
    wmat_fix.add_argument("--repo-root", help="Repo root for resource-relative texture paths. Default CWD.")
    wmat_fix.add_argument("--work-root", required=True, help="Work root for the matbin XML cache.")
    wmat_fix.add_argument("--witchy", required=True, help="WitchyBND.exe path.")
    wmat_fix.add_argument("--matbin-root", help="Root containing extracted .matbin files. Default allmaterial Raw tree.")
    wmat_fix.add_argument("--texture-root", action="append", default=[], help="Texture DDS search root. Can repeat.")
    wmat_fix.add_argument("--placement", action="append", default=[], help="map_placement.txt whose wmesh entries get their wmat rewritten. Can repeat.")
    wmat_fix.add_argument("--wmat", action="append", default=[], help="Extra wmat file to rewrite. Can repeat.")
    wmat_fix.add_argument("--force", action="store_true", help="Rewrite entries that already have a diffuse path.")
    wmat_fix.add_argument("--witchy-timeout", type=int, default=60, help="Per-matbin WitchyBND timeout in seconds.")
    wmat_fix.add_argument("--out", required=True, help="Output JSON path.")
    wmat_fix.set_defaults(func=rewrite_wmat_from_matbin_command)

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
