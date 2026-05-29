#!/usr/bin/env python3
"""Elden Ring asset manifest tooling for Winters.

The parser consumes WitchyBND XML outputs rather than raw FromSoftware binaries.
This keeps the pipeline deterministic and lets WitchyBND own binary format drift.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import xml.etree.ElementTree as ET
from collections import Counter
from pathlib import Path
from typing import Any


SUPPORTED_TEXTURE_EXTS = (".png", ".dds", ".tif", ".tiff", ".tga", ".jpg", ".jpeg")
SUPPORTED_MODEL_EXTS = (".fbx", ".flver", ".obj")

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

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
