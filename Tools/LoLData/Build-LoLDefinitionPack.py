from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "ChampionData"))

import build_champion_game_data as legacy  # noqa: E402


SLOT_NAMES = ("basic_attack", "q", "w", "e", "r")
TARGET_MAP = {
    "Self": ("Self", "Direct"),
    "UnitTarget": ("Unit", "Direct"),
    "GroundTarget": ("Ground", "Direct"),
    "Direction": ("Direction", "Direct"),
    "Conditional": ("Self", "Contextual"),
}

RUNE_MAP = {
    "None": "None",
    "LethalTempo": "LethalTempo",
    "Electrocute": "Electrocute",
    "AdaptiveForce": "AdaptiveForce",
}

STRUCTURE_KIND_MAP = {
    "Structure_Nexus": "Structure_Nexus",
    "Structure_Inhibitor": "Structure_Inhibitor",
    "Structure_Turret": "Structure_Turret",
}

TEAM_MAP = {
    "Blue": "Blue",
    "Red": "Red",
}

SKILL_EFFECT_PARAM_IDS = {
    "baseDamage": "BaseDamage",
    "damagePerRank": "DamagePerRank",
    "range": "Range",
    "speed": "Speed",
    "moveSpeedMul": "MoveSpeedMul",
    "stunDurationSec": "StunDurationSec",
    "slowDurationSec": "SlowDurationSec",
    "airborneDurationSec": "AirborneDurationSec",
    "markDurationSec": "MarkDurationSec",
    "stackWindowSec": "StackWindowSec",
    "gap": "Gap",
    "dashDistance": "DashDistance",
    "dashDurationSec": "DashDurationSec",
    "targetDashDurationSec": "TargetDashDurationSec",
    "dashDelaySec": "DashDelaySec",
    "effectDurationSec": "EffectDurationSec",
    "tickIntervalSec": "TickIntervalSec",
    "refreshDurationSec": "RefreshDurationSec",
    "vanishDurationSec": "VanishDurationSec",
    "missingHealthDamageRatio": "MissingHealthDamageRatio",
    "minHealthAmount": "MinHealthAmount",
    "healBaseAmount": "HealBaseAmount",
    "healAmountPerRank": "HealAmountPerRank",
    "rectLength": "RectLength",
    "rectWidth": "RectWidth",
    "halfWidth": "HalfWidth",
    "disarmDurationSec": "DisarmDurationSec",
    "tornadoSpeed": "TornadoSpeed",
    "tornadoDurationSec": "TornadoDurationSec",
    "tornadoRadius": "TornadoRadius",
    "tornadoDamage": "TornadoDamage",
    "dashAreaRadius": "DashAreaRadius",
    "dashAreaDamage": "DashAreaDamage",
    "halfAngleCos": "HalfAngleCos",
    "radius": "Radius",
    "shieldDurationSec": "ShieldDurationSec",
    "shieldBaseAmount": "ShieldBaseAmount",
    "shieldAmountPerRank": "ShieldAmountPerRank",
    "shieldArmorPerRank": "ShieldArmorPerRank",
}

SKILL_EFFECT_PARAM_MAX = 16
SUMMON_POLICY_PARAM_IDS = {
    "durationSec": "DurationSec",
    "moveSpeed": "MoveSpeed",
    "attackRange": "AttackRange",
    "sightRange": "SightRange",
    "attackCooldownSec": "AttackCooldownSec",
    "baseAttackDamage": "BaseAttackDamage",
    "attackDamagePerRank": "AttackDamagePerRank",
    "baseHp": "BaseHp",
    "hpPerRank": "HpPerRank",
    "radius": "Radius",
    "roleType": "RoleType",
    "lane": "Lane",
}
SUMMON_POLICY_PARAM_MAX = 16

JUNGLE_FIELDS = (
    ("maxHp", 1500.0),
    ("radius", 1.0),
    ("attackRange", 1.7),
    ("attackDamage", 45.0),
    ("attackCooldown", 1.4),
    ("moveSpeed", 4.0),
    ("baseArmor", 20.0),
    ("baseMr", 20.0),
)

MINION_FIELDS = (
    ("moveSpeed", 4.0),
    ("attackRange", 1.5),
    ("sightRange", 12.0),
    ("attackDamage", 40.0),
    ("attackCooldownMax", 1.0),
    ("maxHp", 450.0),
)


def fail(message: str) -> None:
    raise SystemExit(f"[LoLDefinitionPack] {message}")


def definition_key(canonical_key: str) -> int:
    value = 2166136261
    for byte in canonical_key.encode("utf-8"):
        value ^= byte
        value = (value * 16777619) & 0xFFFFFFFF
    if value == 0:
        fail(f"reserved zero DefinitionKey generated for {canonical_key}")
    return value


def cpp_float(value: float) -> str:
    return legacy.cpp_float(value)


def cpp_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def cpp_wstring(value: str) -> str:
    return "L" + cpp_string(value)


def cpp_symbol(value: str) -> str:
    result = []
    for ch in value:
        if ch.isalnum():
            result.append(ch.upper())
        else:
            result.append("_")
    symbol = "".join(result).strip("_")
    return symbol if symbol else "VALUE"


def require_object(value: object, path: str) -> dict:
    if not isinstance(value, dict):
        fail(f"{path} must be an object")
    return value


def require_array(value: object, path: str) -> list:
    if not isinstance(value, list):
        fail(f"{path} must be an array")
    return value


def normalize_resource_relative_path(value: object, path: str, allow_empty: bool = False) -> str:
    if not isinstance(value, str):
        fail(f"{path} must be a string")
    if not allow_empty and not value:
        fail(f"{path} must be a non-empty string")
    normalized = value.replace("\\", "/")
    if normalized and "Client/Bin/Resource" in normalized:
        fail(f"{path} must be resource-relative, not rooted")
    return normalized


def normalize_float_fields(source: dict, fields: tuple[tuple[str, float], ...], path: str) -> dict:
    return {
        key: legacy.as_float(source.get(key, default), f"{path}.{key}")
        for key, default in fields
    }


def normalize_spawn_loadout(source: dict) -> dict:
    rune = source.get("startRune", "None")
    if rune not in RUNE_MAP:
        fail(f"spawnLoadout.startRune must be one of {sorted(RUNE_MAP)}")
    return {
        "startGold": legacy.as_int(source.get("startGold", 0), "spawnLoadout.startGold"),
        "startLevel": legacy.as_int(source.get("startLevel", 0), "spawnLoadout.startLevel"),
        "startRune": rune,
        "startRuneCount": legacy.as_int(source.get("startRuneCount", 0), "spawnLoadout.startRuneCount"),
        "respawnDelaySec": legacy.as_float(source.get("respawnDelaySec", 0.0), "spawnLoadout.respawnDelaySec"),
    }


def normalize_spawn_object_root(root: dict) -> dict:
    root = require_object(root, "spawnObject")

    structure = require_object(root.get("structure", {}), "structure")
    turret_ai = require_object(structure.get("turretAI", {}), "structure.turretAI")

    jungle_camps = []
    seen_jungle = set()
    for index, item in enumerate(require_array(root.get("jungleCamps", []), "jungleCamps")):
        item = require_object(item, f"jungleCamps[{index}]")
        sub_kind = legacy.as_int(item.get("subKind", 0), f"jungleCamps[{index}].subKind")
        if sub_kind in seen_jungle:
            fail(f"duplicated jungle subKind: {sub_kind}")
        seen_jungle.add(sub_kind)
        jungle_camps.append(
            {
                "subKind": sub_kind,
                **normalize_float_fields(item, JUNGLE_FIELDS, f"jungleCamps[{index}]"),
            }
        )

    minions = []
    seen_minion = set()
    for index, item in enumerate(require_array(root.get("minions", []), "minions")):
        item = require_object(item, f"minions[{index}]")
        role_type = legacy.as_int(item.get("roleType", 0), f"minions[{index}].roleType")
        if role_type in seen_minion:
            fail(f"duplicated minion roleType: {role_type}")
        seen_minion.add(role_type)
        minions.append(
            {
                "roleType": role_type,
                **normalize_float_fields(item, MINION_FIELDS, f"minions[{index}]"),
            }
        )

    return {
        "schemaVersion": legacy.as_int(root.get("schemaVersion", 1), "spawnObject.schemaVersion"),
        "dataVersion": legacy.as_int(root.get("dataVersion", 1), "spawnObject.dataVersion"),
        "spawnLoadout": normalize_spawn_loadout(
            require_object(root.get("spawnLoadout", {}), "spawnLoadout")
        ),
        "championCollider": {
            "bodyHeight": legacy.as_float(
                require_object(root.get("championCollider", {}), "championCollider").get("bodyHeight", 0.0),
                "championCollider.bodyHeight",
            ),
            "bodyOffsetY": legacy.as_float(
                require_object(root.get("championCollider", {}), "championCollider").get("bodyOffsetY", 0.0),
                "championCollider.bodyOffsetY",
            ),
        },
        "structure": {
            "turretMaxHp": legacy.as_float(structure.get("turretMaxHp", 0.0), "structure.turretMaxHp"),
            "inhibitorMaxHp": legacy.as_float(structure.get("inhibitorMaxHp", 0.0), "structure.inhibitorMaxHp"),
            "nexusMaxHp": legacy.as_float(structure.get("nexusMaxHp", 0.0), "structure.nexusMaxHp"),
            "turretAI": {
                "attackRange": legacy.as_float(turret_ai.get("attackRange", 0.0), "structure.turretAI.attackRange"),
                "attackCooldownMax": legacy.as_float(
                    turret_ai.get("attackCooldownMax", 0.0),
                    "structure.turretAI.attackCooldownMax",
                ),
                "attackDamage": legacy.as_float(turret_ai.get("attackDamage", 0.0), "structure.turretAI.attackDamage"),
                "nexusAttackDamage": legacy.as_float(
                    turret_ai.get("nexusAttackDamage", 0.0),
                    "structure.turretAI.nexusAttackDamage",
                ),
                "projectileSpeed": legacy.as_float(
                    turret_ai.get("projectileSpeed", 0.0),
                    "structure.turretAI.projectileSpeed",
                ),
                "turretSightRange": legacy.as_float(
                    turret_ai.get("turretSightRange", 0.0),
                    "structure.turretAI.turretSightRange",
                ),
                "structureSightRange": legacy.as_float(
                    turret_ai.get("structureSightRange", 0.0),
                    "structure.turretAI.structureSightRange",
                ),
                "bodyHeight": legacy.as_float(turret_ai.get("bodyHeight", 0.0), "structure.turretAI.bodyHeight"),
                "bodyOffsetY": legacy.as_float(turret_ai.get("bodyOffsetY", 0.0), "structure.turretAI.bodyOffsetY"),
            },
        },
        "defaultJungleCamp": normalize_float_fields(
            require_object(root.get("defaultJungleCamp", {}), "defaultJungleCamp"),
            JUNGLE_FIELDS,
            "defaultJungleCamp",
        ),
        "jungleCamps": sorted(jungle_camps, key=lambda item: item["subKind"]),
        "defaultMinion": normalize_float_fields(
            require_object(root.get("defaultMinion", {}), "defaultMinion"),
            MINION_FIELDS,
            "defaultMinion",
        ),
        "minions": sorted(minions, key=lambda item: item["roleType"]),
    }


def normalize_object_visual_root(root: dict) -> dict:
    root = require_object(root, "objectVisual")
    structures = []
    jungles = []
    minions = []
    ambient_props = []
    seen_keys = set()
    seen_pairs = set()

    for index, item in enumerate(require_array(root.get("structures", []), "structures")):
        item = require_object(item, f"structures[{index}]")
        key = item.get("key")
        if not isinstance(key, str) or not key:
            fail(f"structures[{index}].key must be a non-empty string")
        if key in seen_keys:
            fail(f"duplicated structure visual key: {key}")
        seen_keys.add(key)

        kind = item.get("kind")
        if kind not in STRUCTURE_KIND_MAP:
            fail(f"structures[{index}].kind must be one of {sorted(STRUCTURE_KIND_MAP)}")

        team = item.get("team")
        if team not in TEAM_MAP:
            fail(f"structures[{index}].team must be one of {sorted(TEAM_MAP)}")

        pair = (kind, team)
        if pair in seen_pairs:
            fail(f"duplicated structure visual pair: {kind}.{team}")
        seen_pairs.add(pair)

        mesh = item.get("mesh")
        if not isinstance(mesh, str) or not mesh:
            fail(f"structures[{index}].mesh must be a non-empty string")
        if "Client/Bin/Resource" in mesh.replace("\\", "/"):
            fail(f"structures[{index}].mesh must be resource-relative, not rooted")

        shader = item.get("shader", "Shaders/Mesh3D.hlsl")
        if not isinstance(shader, str) or not shader:
            fail(f"structures[{index}].shader must be a non-empty string")
        if "Client/Bin/Resource" in shader.replace("\\", "/"):
            fail(f"structures[{index}].shader must be resource-relative, not rooted")

        states = []
        seen_submeshes = set()
        for state_index, state in enumerate(require_array(item.get("visibilityStates", []), f"structures[{index}].visibilityStates")):
            state = require_object(state, f"structures[{index}].visibilityStates[{state_index}]")
            submesh_index = legacy.as_int(
                state.get("submeshIndex", 0),
                f"structures[{index}].visibilityStates[{state_index}].submeshIndex",
            )
            if submesh_index in seen_submeshes:
                fail(f"duplicated visibility submesh index: {key}.{submesh_index}")
            seen_submeshes.add(submesh_index)
            visible_when_destroyed = state.get("visibleWhenDestroyed", False)
            if not isinstance(visible_when_destroyed, bool):
                fail(
                    f"structures[{index}].visibilityStates[{state_index}].visibleWhenDestroyed "
                    "must be a bool"
                )
            states.append(
                {
                    "name": str(state.get("name", "")),
                    "submeshIndex": submesh_index,
                    "visibleWhenDestroyed": visible_when_destroyed,
                }
            )

        if len(states) > 4:
            fail(f"structures[{index}].visibilityStates has too many entries: {len(states)} > 4")

        structures.append(
            {
                "key": key,
                "kind": kind,
                "team": team,
                "mesh": mesh.replace("\\", "/"),
                "shader": shader.replace("\\", "/"),
                "visibilityStates": states,
            }
        )

    seen_jungle_sub_kinds = set()
    for index, item in enumerate(require_array(root.get("jungles", []), "jungles")):
        item = require_object(item, f"jungles[{index}]")
        key = item.get("key")
        if not isinstance(key, str) or not key:
            fail(f"jungles[{index}].key must be a non-empty string")
        if key in seen_keys:
            fail(f"duplicated object visual key: {key}")
        seen_keys.add(key)

        sub_kind = legacy.as_int(item.get("subKind", 0), f"jungles[{index}].subKind")
        if sub_kind in seen_jungle_sub_kinds:
            fail(f"duplicated jungle visual subKind: {sub_kind}")
        seen_jungle_sub_kinds.add(sub_kind)

        mesh = item.get("mesh")
        if not isinstance(mesh, str) or not mesh:
            fail(f"jungles[{index}].mesh must be a non-empty string")
        if "Client/Bin/Resource" in mesh.replace("\\", "/"):
            fail(f"jungles[{index}].mesh must be resource-relative, not rooted")

        shader = item.get("shader", "Shaders/Mesh3D.hlsl")
        if not isinstance(shader, str) or not shader:
            fail(f"jungles[{index}].shader must be a non-empty string")
        if "Client/Bin/Resource" in shader.replace("\\", "/"):
            fail(f"jungles[{index}].shader must be resource-relative, not rooted")

        texture_overrides = []
        seen_texture_meshes = set()
        for override_index, override in enumerate(require_array(item.get("textureOverrides", []), f"jungles[{index}].textureOverrides")):
            override = require_object(override, f"jungles[{index}].textureOverrides[{override_index}]")
            mesh_index = legacy.as_int(
                override.get("meshIndex", 0),
                f"jungles[{index}].textureOverrides[{override_index}].meshIndex",
            )
            if mesh_index in seen_texture_meshes:
                fail(f"duplicated jungle texture override mesh index: {key}.{mesh_index}")
            seen_texture_meshes.add(mesh_index)

            texture = override.get("texture")
            if not isinstance(texture, str) or not texture:
                fail(f"jungles[{index}].textureOverrides[{override_index}].texture must be a non-empty string")
            if "Client/Bin/Resource" in texture.replace("\\", "/"):
                fail(f"jungles[{index}].textureOverrides[{override_index}].texture must be resource-relative")

            texture_overrides.append(
                {
                    "meshIndex": mesh_index,
                    "texture": texture.replace("\\", "/"),
                }
            )

        if len(texture_overrides) > 4:
            fail(f"jungles[{index}].textureOverrides has too many entries: {len(texture_overrides)} > 4")

        jungles.append(
            {
                "key": key,
                "subKind": sub_kind,
                "mesh": mesh.replace("\\", "/"),
                "shader": shader.replace("\\", "/"),
                "textureOverrides": texture_overrides,
            }
        )

    seen_minion_pairs = set()
    for index, item in enumerate(require_array(root.get("minions", []), "minions")):
        item = require_object(item, f"minions[{index}]")
        key = item.get("key")
        if not isinstance(key, str) or not key:
            fail(f"minions[{index}].key must be a non-empty string")
        if key in seen_keys:
            fail(f"duplicated object visual key: {key}")
        seen_keys.add(key)

        minion_type = legacy.as_int(item.get("type", 0), f"minions[{index}].type")
        team = legacy.as_int(item.get("team", 0), f"minions[{index}].team")
        pair = (minion_type, team)
        if pair in seen_minion_pairs:
            fail(f"duplicated minion visual pair: type={minion_type}, team={team}")
        seen_minion_pairs.add(pair)

        mesh = item.get("mesh")
        if not isinstance(mesh, str) or not mesh:
            fail(f"minions[{index}].mesh must be a non-empty string")
        if "Client/Bin/Resource" in mesh.replace("\\", "/"):
            fail(f"minions[{index}].mesh must be resource-relative, not rooted")

        shader = item.get("shader", "Shaders/Mesh3D.hlsl")
        if not isinstance(shader, str) or not shader:
            fail(f"minions[{index}].shader must be a non-empty string")
        if "Client/Bin/Resource" in shader.replace("\\", "/"):
            fail(f"minions[{index}].shader must be resource-relative, not rooted")

        texture_all = item.get("textureAllMeshes", "")
        if not isinstance(texture_all, str):
            fail(f"minions[{index}].textureAllMeshes must be a string")
        if "Client/Bin/Resource" in texture_all.replace("\\", "/"):
            fail(f"minions[{index}].textureAllMeshes must be resource-relative, not rooted")

        minions.append(
            {
                "key": key,
                "type": minion_type,
                "team": team,
                "mesh": mesh.replace("\\", "/"),
                "shader": shader.replace("\\", "/"),
                "textureAllMeshes": texture_all.replace("\\", "/"),
            }
        )

    ambient_source = require_object(root.get("ambientProps", {}), "ambientProps")
    ambient_placement = ambient_source.get("placement")
    if not isinstance(ambient_placement, str) or not ambient_placement:
        fail("ambientProps.placement must be a non-empty string")
    if "Client/Bin/Resource" in ambient_placement.replace("\\", "/"):
        fail("ambientProps.placement must be resource-relative, not rooted")

    seen_ambient_kinds = set()
    for index, item in enumerate(require_array(ambient_source.get("props", []), "ambientProps.props")):
        item = require_object(item, f"ambientProps.props[{index}]")
        key = item.get("key")
        if not isinstance(key, str) or not key:
            fail(f"ambientProps.props[{index}].key must be a non-empty string")
        if key in seen_keys:
            fail(f"duplicated object visual key: {key}")
        seen_keys.add(key)

        kind = legacy.as_int(item.get("kind", 0), f"ambientProps.props[{index}].kind")
        if kind in seen_ambient_kinds:
            fail(f"duplicated ambient prop visual kind: {kind}")
        seen_ambient_kinds.add(kind)

        mesh = item.get("mesh")
        if not isinstance(mesh, str) or not mesh:
            fail(f"ambientProps.props[{index}].mesh must be a non-empty string")
        if "Client/Bin/Resource" in mesh.replace("\\", "/"):
            fail(f"ambientProps.props[{index}].mesh must be resource-relative, not rooted")

        shader = item.get("shader", "Shaders/Mesh3D.hlsl")
        if not isinstance(shader, str) or not shader:
            fail(f"ambientProps.props[{index}].shader must be a non-empty string")
        if "Client/Bin/Resource" in shader.replace("\\", "/"):
            fail(f"ambientProps.props[{index}].shader must be resource-relative, not rooted")

        idle_animation = item.get("idleAnimation", "")
        if not isinstance(idle_animation, str):
            fail(f"ambientProps.props[{index}].idleAnimation must be a string")

        ambient_props.append(
            {
                "key": key,
                "kind": kind,
                "mesh": mesh.replace("\\", "/"),
                "shader": shader.replace("\\", "/"),
                "idleAnimation": idle_animation,
            }
        )

    map_runtime_source = require_object(root.get("mapRuntime", {}), "mapRuntime")
    base_map_mesh = normalize_resource_relative_path(
        map_runtime_source.get("baseMapMesh"),
        "mapRuntime.baseMapMesh",
    )
    full_layer_map_mesh = normalize_resource_relative_path(
        map_runtime_source.get("fullLayerMapMesh"),
        "mapRuntime.fullLayerMapMesh",
    )
    map_runtime = {
        "baseMapMesh": base_map_mesh,
        "baseMapSurface": normalize_resource_relative_path(
            map_runtime_source.get("baseMapSurface", base_map_mesh),
            "mapRuntime.baseMapSurface",
        ),
        "fullLayerMapMesh": full_layer_map_mesh,
        "fullLayerMapSurface": normalize_resource_relative_path(
            map_runtime_source.get("fullLayerMapSurface", full_layer_map_mesh),
            "mapRuntime.fullLayerMapSurface",
        ),
        "brushVolumeCsv": normalize_resource_relative_path(
            map_runtime_source.get("brushVolumeCsv"),
            "mapRuntime.brushVolumeCsv",
        ),
        "brushVolumeBinary": normalize_resource_relative_path(
            map_runtime_source.get("brushVolumeBinary"),
            "mapRuntime.brushVolumeBinary",
        ),
        "attackRangeTexture": normalize_resource_relative_path(
            map_runtime_source.get("attackRangeTexture"),
            "mapRuntime.attackRangeTexture",
        ),
    }

    fx_mesh_preloads = []
    for index, item in enumerate(require_array(root.get("fxMeshPreloads", []), "fxMeshPreloads")):
        item = require_object(item, f"fxMeshPreloads[{index}]")
        key = item.get("key")
        if not isinstance(key, str) or not key:
            fail(f"fxMeshPreloads[{index}].key must be a non-empty string")
        if key in seen_keys:
            fail(f"duplicated object visual key: {key}")
        seen_keys.add(key)
        fx_mesh_preloads.append(
            {
                "key": key,
                "mesh": normalize_resource_relative_path(
                    item.get("mesh"),
                    f"fxMeshPreloads[{index}].mesh",
                ),
                "texture": normalize_resource_relative_path(
                    item.get("texture"),
                    f"fxMeshPreloads[{index}].texture",
                ),
            }
        )

    return {
        "schemaVersion": legacy.as_int(root.get("schemaVersion", 1), "objectVisual.schemaVersion"),
        "structures": sorted(structures, key=lambda record: record["key"]),
        "jungles": sorted(jungles, key=lambda record: record["subKind"]),
        "minions": sorted(minions, key=lambda record: (record["team"], record["type"])),
        "ambientProps": {
            "placement": ambient_placement.replace("\\", "/"),
            "props": sorted(ambient_props, key=lambda record: record["kind"]),
        },
        "mapRuntime": map_runtime,
        "fxMeshPreloads": sorted(fx_mesh_preloads, key=lambda record: record["key"]),
    }


def normalize_champion_asset_visual_root(root: dict, valid_champions: set[str]) -> dict:
    root = require_object(root, "championAssetVisual")
    models = []
    ui_records = []

    seen_models = set()
    for index, item in enumerate(require_array(root.get("models", []), "models")):
        item = require_object(item, f"models[{index}]")
        champion = item.get("champion")
        if champion not in valid_champions:
            fail(f"models[{index}].champion must be one of {sorted(valid_champions)}")
        if champion in seen_models:
            fail(f"duplicated champion model visual: {champion}")
        seen_models.add(champion)

        display_name = item.get("displayName", champion)
        if not isinstance(display_name, str) or not display_name:
            fail(f"models[{index}].displayName must be a non-empty string")

        anim_prefix = item.get("animPrefix", "")
        idle_animation = item.get("idleAnimation", "Idle1")
        run_animation = item.get("runAnimation", "run")
        basic_attack_animation = item.get("basicAttackAnimation", "attack_01")
        for field_name, field_value in (
            ("animPrefix", anim_prefix),
            ("idleAnimation", idle_animation),
            ("runAnimation", run_animation),
            ("basicAttackAnimation", basic_attack_animation),
        ):
            if not isinstance(field_value, str):
                fail(f"models[{index}].{field_name} must be a string")

        texture_slots = []
        for slot_index, texture in enumerate(require_array(item.get("textureSlots", []), f"models[{index}].textureSlots")):
            texture_slots.append(
                normalize_resource_relative_path(
                    texture,
                    f"models[{index}].textureSlots[{slot_index}]",
                    allow_empty=True,
                )
            )
        if len(texture_slots) > 8:
            fail(f"models[{index}].textureSlots has too many entries: {len(texture_slots)} > 8")

        spawn_position_source = require_array(item.get("spawnPosition", [0.0, 1.0, 0.0]), f"models[{index}].spawnPosition")
        if len(spawn_position_source) != 3:
            fail(f"models[{index}].spawnPosition must have exactly 3 numbers")
        spawn_position = [
            legacy.as_float(value, f"models[{index}].spawnPosition[{axis}]")
            for axis, value in enumerate(spawn_position_source)
        ]

        models.append(
            {
                "key": f"champion.model.{champion.lower()}",
                "champion": champion,
                "displayName": display_name,
                "animPrefix": anim_prefix,
                "idleAnimation": idle_animation,
                "runAnimation": run_animation,
                "basicAttackAnimation": basic_attack_animation,
                "basicAttackRange": legacy.as_float(
                    item.get("basicAttackRange", 6.0),
                    f"models[{index}].basicAttackRange",
                ),
                "mesh": normalize_resource_relative_path(item.get("mesh"), f"models[{index}].mesh"),
                "shader": normalize_resource_relative_path(
                    item.get("shader", "Shaders/Mesh3D.hlsl"),
                    f"models[{index}].shader",
                ),
                "defaultTexture": normalize_resource_relative_path(
                    item.get("defaultTexture"),
                    f"models[{index}].defaultTexture",
                ),
                "textureSlots": texture_slots,
                "spawnPosition": spawn_position,
                "spawnScale": legacy.as_float(item.get("spawnScale", 0.01), f"models[{index}].spawnScale"),
            }
        )

    seen_ui = set()
    for index, item in enumerate(require_array(root.get("ui", []), "ui")):
        item = require_object(item, f"ui[{index}]")
        champion = item.get("champion")
        if champion not in valid_champions:
            fail(f"ui[{index}].champion must be one of {sorted(valid_champions)}")
        if champion in seen_ui:
            fail(f"duplicated champion ui visual: {champion}")
        seen_ui.add(champion)
        ui_records.append(
            {
                "key": f"champion.ui.{champion.lower()}",
                "champion": champion,
                "loadscreen": normalize_resource_relative_path(item.get("loadscreen"), f"ui[{index}].loadscreen"),
                "portrait": normalize_resource_relative_path(item.get("portrait"), f"ui[{index}].portrait"),
            }
        )

    missing_ui = valid_champions - seen_ui
    if missing_ui:
        fail(f"missing champion ui visuals: {sorted(missing_ui)}")

    return {
        "schemaVersion": legacy.as_int(root.get("schemaVersion", 1), "championAssetVisual.schemaVersion"),
        "models": sorted(models, key=lambda record: record["champion"]),
        "ui": sorted(ui_records, key=lambda record: record["champion"]),
    }


def normalize_skill_effect_root(root: dict, valid_skill_keys: set[str]) -> dict:
    root = require_object(root, "skillEffect")
    records = []
    seen_keys = set()

    for index, item in enumerate(require_array(root.get("skillEffects", []), "skillEffects")):
        item = require_object(item, f"skillEffects[{index}]")
        key = item.get("key")
        if not isinstance(key, str) or not key:
            fail(f"skillEffects[{index}].key must be a non-empty string")
        if key not in valid_skill_keys:
            fail(f"skillEffects[{index}].key references unknown skill: {key}")
        if key in seen_keys:
            fail(f"duplicated skill effect key: {key}")
        seen_keys.add(key)

        params_source = require_object(item.get("params", {}), f"skillEffects[{index}].params")
        params = []
        seen_params = set()
        for param_name in sorted(params_source):
            if param_name not in SKILL_EFFECT_PARAM_IDS:
                fail(
                    f"skillEffects[{index}].params.{param_name} must be one of "
                    f"{sorted(SKILL_EFFECT_PARAM_IDS)}"
                )
            if param_name in seen_params:
                fail(f"duplicated skill effect param: {key}.{param_name}")
            seen_params.add(param_name)
            params.append(
                {
                    "id": param_name,
                    "cppId": SKILL_EFFECT_PARAM_IDS[param_name],
                    "value": legacy.as_float(
                        params_source[param_name],
                        f"skillEffects[{index}].params.{param_name}",
                    ),
                }
            )

        if len(params) > SKILL_EFFECT_PARAM_MAX:
            fail(f"skillEffects[{index}] has too many params: {len(params)} > {SKILL_EFFECT_PARAM_MAX}")

        summon_source = require_object(item.get("summonPolicy", {}), f"skillEffects[{index}].summonPolicy")
        summon_params = []
        seen_summon_params = set()
        for param_name in sorted(summon_source):
            if param_name not in SUMMON_POLICY_PARAM_IDS:
                fail(
                    f"skillEffects[{index}].summonPolicy.{param_name} must be one of "
                    f"{sorted(SUMMON_POLICY_PARAM_IDS)}"
                )
            if param_name in seen_summon_params:
                fail(f"duplicated summon policy param: {key}.{param_name}")
            seen_summon_params.add(param_name)
            summon_params.append(
                {
                    "id": param_name,
                    "cppId": SUMMON_POLICY_PARAM_IDS[param_name],
                    "value": legacy.as_float(
                        summon_source[param_name],
                        f"skillEffects[{index}].summonPolicy.{param_name}",
                    ),
                }
            )

        if len(summon_params) > SUMMON_POLICY_PARAM_MAX:
            fail(
                f"skillEffects[{index}] has too many summon policy params: "
                f"{len(summon_params)} > {SUMMON_POLICY_PARAM_MAX}"
            )

        records.append({"key": key, "params": params, "summonPolicyParams": summon_params})

    return {
        "schemaVersion": legacy.as_int(root.get("schemaVersion", 1), "skillEffect.schemaVersion"),
        "dataVersion": legacy.as_int(root.get("dataVersion", 1), "skillEffect.dataVersion"),
        "skillEffects": sorted(records, key=lambda record: record["key"]),
    }


def apply_skill_effect_params(skills: list[dict], skill_effect_data: dict) -> None:
    by_key = {record["key"]: record for record in skill_effect_data["skillEffects"]}
    for skill in skills:
        record = by_key.get(skill["canonicalKey"], {})
        skill["effectParams"] = record.get("params", [])
        skill["summonPolicyParams"] = record.get("summonPolicyParams", [])


def compute_definition_pack_hash(
    data: dict,
    summoner_spell_data: dict,
    spawn_object_data: dict,
    skill_effect_data: dict) -> int:
    stable = json.dumps(
        {
            "championGameplay": data,
            "summonerSpellGameplay": summoner_spell_data,
            "spawnObjectGameplay": spawn_object_data,
            "skillEffectGameplay": skill_effect_data,
        },
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")
    return int(hashlib.sha256(stable).hexdigest()[:8], 16)


def canonical_champion(name: str) -> str:
    return f"champion.{name.lower()}"


def canonical_skill(champion: str, slot: int) -> str:
    return f"skill.{champion.lower()}.{SLOT_NAMES[slot]}"


def canonical_spell(spell_id: int) -> str:
    return "summoner.flash" if spell_id == 4 else f"summoner.{spell_id}"


def normalize_summoner_spell_root(root: dict) -> dict:
    root = require_object(root, "summonerSpellGameplay")
    spells = []
    seen = set()
    for index, spell in enumerate(require_array(root.get("summonerSpells", []), "summonerSpells")):
        spell = require_object(spell, f"summonerSpells[{index}]")
        legacy_spell_id = legacy.as_int(
            spell.get("legacySpellId", spell.get("spellId", 0)),
            f"summonerSpells[{index}].legacySpellId",
        )
        if legacy_spell_id == 0:
            continue
        key = spell.get("key", canonical_spell(legacy_spell_id))
        if not isinstance(key, str) or not key:
            fail(f"summonerSpells[{index}].key must be a non-empty string")
        if key in seen:
            fail(f"duplicated summoner spell key: {key}")
        seen.add(key)
        spells.append(
            {
                "spellId": legacy_spell_id,
                "canonicalKey": key,
                "definitionKey": definition_key(key),
                "rangeMax": legacy.as_float(spell.get("rangeMax", 0.0), f"{key}.rangeMax"),
                "cooldownSec": legacy.as_float(spell.get("cooldownSec", 0.0), f"{key}.cooldownSec"),
                "gameplayPolicyId": legacy.as_int(spell.get("gameplayPolicyId", 0), f"{key}.gameplayPolicyId"),
                "visualCueId": legacy.as_int(
                    spell.get("replicatedCueId", spell.get("visualCueId", 0)),
                    f"{key}.replicatedCueId",
                ),
            }
        )

    return {
        "schemaVersion": legacy.as_int(root.get("schemaVersion", 1), "schemaVersion"),
        "summonerSpells": sorted(spells, key=lambda record: record["canonicalKey"]),
    }


def make_records(data: dict, summoner_spell_data: dict) -> tuple[list[dict], list[dict], list[dict]]:
    champions = []
    skills = []

    for champion in data["champions"]:
        key = canonical_champion(champion["champion"])
        champions.append({**champion, "canonicalKey": key, "definitionKey": definition_key(key)})
        for skill in champion["skills"]:
            skill_key = canonical_skill(champion["champion"], skill["slot"])
            shape, policy = TARGET_MAP.get(skill["targetMode"], (None, None))
            if shape is None:
                fail(f"unsupported targetMode {skill['targetMode']} in {skill_key}")
            skills.append(
                {
                    **skill,
                    "ownerChampion": champion["champion"],
                    "ownerKey": key,
                    "canonicalKey": skill_key,
                    "definitionKey": definition_key(skill_key),
                    "targetShape": shape,
                    "resolvePolicy": policy,
                }
            )

    spells = [{**spell} for spell in summoner_spell_data["summonerSpells"]]

    champions.sort(key=lambda record: record["canonicalKey"])
    skills.sort(key=lambda record: record["canonicalKey"])

    for index, record in enumerate(champions, start=1):
        record["defId"] = index
    for index, record in enumerate(skills, start=1):
        record["defId"] = index
    for index, record in enumerate(spells, start=1):
        record["defId"] = index

    all_records = champions + skills + spells
    keys: dict[int, str] = {}
    for record in all_records:
        value = record["definitionKey"]
        previous = keys.get(value)
        if previous is not None:
            fail(f"DefinitionKey collision: {previous} and {record['canonicalKey']} -> 0x{value:08X}")
        keys[value] = record["canonicalKey"]

    return champions, skills, spells


def server_champion_json(record: dict, skill_ids: dict[str, int]) -> dict:
    result = {
        "key": record["canonicalKey"],
        "definitionKey": record["definitionKey"],
        "defId": record["defId"],
        "dataVersion": record["dataVersion"],
        "stats": record["stats"],
        "skillLoadout": [skill_ids[canonical_skill(record["champion"], slot)] for slot in range(5)],
    }
    if record["passiveDash"] is not None:
        result["passiveDash"] = record["passiveDash"]
    if record.get("passiveSoul") is not None:
        result["passiveSoul"] = record["passiveSoul"]
    return result


def server_skill_json(record: dict, champion_ids: dict[str, int]) -> dict:
    result = {
        "key": record["canonicalKey"],
        "definitionKey": record["definitionKey"],
        "defId": record["defId"],
        "ownerChampionDefId": champion_ids[record["ownerKey"]],
        "slot": record["slot"],
        "target": {"shape": record["targetShape"], "resolvePolicy": record["resolvePolicy"]},
        "cost": {"mana": record["manaCost"]},
        "cooldown": {"seconds": record["cooldownSec"]},
        "range": {"maximum": record["rangeMax"]},
        "stage": {
            "count": record["stageCount"],
            "windowSeconds": record["stageWindowSec"],
            "lockSeconds": [stage["lockDurationSec"] for stage in record["stages"]],
        },
        "effect": {
            "scalingTableId": record["scalingTableId"],
            "gameplayPolicyId": record["gameplayPolicyId"],
            "replicatedCueId": record["visualCueId"],
            "params": [
                {"id": param["id"], "value": param["value"]}
                for param in record.get("effectParams", [])
            ],
        },
    }

    summon_policy_params = record.get("summonPolicyParams", [])
    if summon_policy_params:
        result["summonPolicy"] = {
            "params": [
                {"id": param["id"], "value": param["value"]}
                for param in summon_policy_params
            ],
        }

    return result


def slot_from_skill_key(key: str, path: str) -> int:
    parts = key.split(".")
    if len(parts) != 3 or parts[0] != "skill":
        fail(f"{path}.key must be a canonical skill key")
    slot_name = parts[2]
    if slot_name not in SLOT_NAMES:
        fail(f"{path}.key has unknown skill slot: {slot_name}")
    return SLOT_NAMES.index(slot_name)


def normalize_client_visual_root(root: dict, valid_champions: dict[str, str], valid_skills: set[str]) -> dict:
    root = require_object(root, "championVisual")
    champions = []
    seen_champions = set()
    for champion_index, champion in enumerate(require_array(root.get("champions", []), "champions")):
        champion = require_object(champion, f"champions[{champion_index}]")
        key = champion.get("key")
        if not isinstance(key, str) or key not in valid_champions:
            fail(f"champions[{champion_index}].key must be a known champion key")
        if key in seen_champions:
            fail(f"duplicated champion visual key: {key}")
        seen_champions.add(key)

        skills = []
        seen_slots = set()
        for skill_index, skill in enumerate(require_array(champion.get("skills", []), f"champions[{champion_index}].skills")):
            skill = require_object(skill, f"champions[{champion_index}].skills[{skill_index}]")
            skill_key = skill.get("key")
            if not isinstance(skill_key, str) or skill_key not in valid_skills:
                fail(f"champions[{champion_index}].skills[{skill_index}].key must be a known skill key")
            slot = slot_from_skill_key(skill_key, f"champions[{champion_index}].skills[{skill_index}]")
            if slot in seen_slots:
                fail(f"duplicated visual skill slot: {key}.{slot}")
            seen_slots.add(slot)

            stages = []
            for stage_index, stage in enumerate(require_array(skill.get("stages", []), f"{skill_key}.stages")):
                stage = require_object(stage, f"{skill_key}.stages[{stage_index}]")
                stages.append(
                    {
                        "animationPlaybackSpeed": legacy.as_float(
                            stage.get("animationPlaybackSpeed", 1.0),
                            f"{skill_key}.stages[{stage_index}].animationPlaybackSpeed",
                        ),
                        "castFrame": legacy.as_float(
                            stage.get("castFrame", 0.0),
                            f"{skill_key}.stages[{stage_index}].castFrame",
                        ),
                        "recoveryFrame": legacy.as_float(
                            stage.get("recoveryFrame", 0.0),
                            f"{skill_key}.stages[{stage_index}].recoveryFrame",
                        ),
                    }
                )
            if not stages:
                fail(f"{skill_key}.stages must contain at least one stage")
            if len(stages) > 2:
                fail(f"{skill_key}.stages has too many entries: {len(stages)} > 2")

            skills.append(
                {
                    "key": skill_key,
                    "slot": slot,
                    "replicatedCueId": legacy.as_int(skill.get("replicatedCueId", 0), f"{skill_key}.replicatedCueId"),
                    "stages": stages,
                }
            )

        champions.append(
            {
                "key": key,
                "champion": valid_champions[key],
                "modelYawOffsetRadians": legacy.as_float(
                    champion.get("modelYawOffsetRadians", 0.0),
                    f"{key}.modelYawOffsetRadians",
                ),
                "skills": sorted(skills, key=lambda item: item["slot"]),
            }
        )

    missing = set(valid_champions) - seen_champions
    if missing:
        fail(f"missing champion visual definitions: {sorted(missing)}")

    return {"schemaVersion": legacy.as_int(root.get("schemaVersion", 1), "schemaVersion"), "champions": champions}


def client_visual_json(visual_data: dict) -> dict:
    champions = []
    for champion in visual_data["champions"]:
        skills = []
        for skill in champion["skills"]:
            skills.append(
                {
                    "key": skill["key"],
                    "replicatedCueId": skill["replicatedCueId"],
                    "stages": skill["stages"],
                }
            )
        champions.append(
            {
                "key": champion["key"],
                "modelYawOffsetRadians": champion["modelYawOffsetRadians"],
                "skills": skills,
            }
        )
    return {"schemaVersion": visual_data["schemaVersion"], "champions": champions}


def skill_effect_json(record: dict) -> dict:
    result = {
        "key": record["key"],
        "params": {
            param["id"]: param["value"]
            for param in record["params"]
        },
    }
    summon_policy_params = record.get("summonPolicyParams", [])
    if summon_policy_params:
        result["summonPolicy"] = {
            param["id"]: param["value"]
            for param in summon_policy_params
        }
    return result


def manifest_json(data: dict, champions: list[dict], skills: list[dict], spells: list[dict], build_hash: int) -> dict:
    entries = []
    for kind, records in (("Champion", champions), ("Skill", skills), ("SummonerSpell", spells)):
        for record in records:
            entries.append(
                {
                    "kind": kind,
                    "key": record["canonicalKey"],
                    "definitionKey": record["definitionKey"],
                }
            )
    entries.sort(key=lambda entry: entry["key"])
    return {
        "schemaVersion": data["schemaVersion"],
        "dataVersion": max(champion["dataVersion"] for champion in champions),
        "buildHash": build_hash,
        "buildHashAlgorithm": "sha256-prefix32",
        "definitionKeyHashAlgorithm": "fnv1a32",
        "entries": entries,
    }


def emit_client_visual_cpp(visual_data: dict, object_visual_data: dict, champion_asset_visual_data: dict) -> str:
    lines = [
        '#include "Client/Private/Data/LoLVisualDefinitionPack.h"',
        "",
        "namespace",
        "{",
    ]
    for champion in visual_data["champions"]:
        name = champion["champion"]
        key = definition_key(champion["key"])
        lines.extend(
            [
                f"    ClientData::ChampionVisualDefinition MakeChampionVisual_{name}()",
                "    {",
                "        ClientData::ChampionVisualDefinition def{};",
                f"        def.key = 0x{key:08X}u;",
                f"        def.legacyChampion = eChampion::{name};",
                f"        def.modelYawOffsetRadians = {cpp_float(champion['modelYawOffsetRadians'])};",
            ]
        )
        for skill in champion["skills"]:
            slot = skill["slot"]
            lines.append(f"        def.skills[{slot}].stageCount = {len(skill['stages'])}u;")
            lines.append(f"        def.skills[{slot}].replicatedCueId = {skill['replicatedCueId']}u;")
            for stage_index, stage in enumerate(skill["stages"]):
                lines.append(
                    f"        def.skills[{slot}].stages[{stage_index}].animationPlaybackSpeed = "
                    f"{cpp_float(stage['animationPlaybackSpeed'])};"
                )
                lines.append(
                    f"        def.skills[{slot}].stages[{stage_index}].castFrame = {cpp_float(stage['castFrame'])};"
                )
                lines.append(
                    f"        def.skills[{slot}].stages[{stage_index}].recoveryFrame = "
                    f"{cpp_float(stage['recoveryFrame'])};"
                )
        lines.extend(["        return def;", "    }", ""])

    for record in champion_asset_visual_data["models"]:
        symbol = cpp_symbol(record["champion"])
        key = definition_key(record["key"])
        spawn_position = record["spawnPosition"]
        lines.extend(
            [
                f"    ClientData::ChampionModelVisualDefinition MakeChampionModelVisual_{symbol}()",
                "    {",
                "        ClientData::ChampionModelVisualDefinition def{};",
                f"        def.key = 0x{key:08X}u;",
                f"        def.champion = eChampion::{record['champion']};",
                f"        def.displayName = {cpp_string(record['displayName'])};",
                f"        def.animPrefix = {cpp_string(record['animPrefix'])};",
                f"        def.idleAnimation = {cpp_string(record['idleAnimation'])};",
                f"        def.runAnimation = {cpp_string(record['runAnimation'])};",
                f"        def.basicAttackAnimation = {cpp_string(record['basicAttackAnimation'])};",
                f"        def.basicAttackRange = {cpp_float(record['basicAttackRange'])};",
                f"        def.mesh.resourceRelativePath = {cpp_string(record['mesh'])};",
                f"        def.shader.runtimePath = {cpp_wstring(record['shader'])};",
                f"        def.defaultTexture.resourceRelativePath = {cpp_wstring(record['defaultTexture'])};",
            ]
        )
        for slot_index, texture in enumerate(record["textureSlots"]):
            if texture:
                lines.append(
                    f"        def.textureSlots[{slot_index}].resourceRelativePath = {cpp_wstring(texture)};"
                )
        lines.extend(
            [
                f"        def.spawnPositionX = {cpp_float(spawn_position[0])};",
                f"        def.spawnPositionY = {cpp_float(spawn_position[1])};",
                f"        def.spawnPositionZ = {cpp_float(spawn_position[2])};",
                f"        def.spawnScale = {cpp_float(record['spawnScale'])};",
                "        return def;",
                "    }",
                "",
            ]
        )

    for record in champion_asset_visual_data["ui"]:
        symbol = cpp_symbol(record["champion"])
        key = definition_key(record["key"])
        lines.extend(
            [
                f"    ClientData::ChampionUiVisualDefinition MakeChampionUiVisual_{symbol}()",
                "    {",
                "        ClientData::ChampionUiVisualDefinition def{};",
                f"        def.key = 0x{key:08X}u;",
                f"        def.champion = eChampion::{record['champion']};",
                f"        def.loadscreen.resourceRelativePath = {cpp_wstring(record['loadscreen'])};",
                f"        def.portrait.resourceRelativePath = {cpp_wstring(record['portrait'])};",
                "        return def;",
                "    }",
                "",
            ]
        )

    for record in object_visual_data["structures"]:
        symbol = cpp_symbol(record["key"])
        key = definition_key(record["key"])
        lines.extend(
            [
                f"    ClientData::StructureVisualDefinition MakeStructureVisual_{symbol}()",
                "    {",
                "        ClientData::StructureVisualDefinition def{};",
                f"        def.key = 0x{key:08X}u;",
                f"        def.kind = Winters::Map::eObjectKind::{STRUCTURE_KIND_MAP[record['kind']]};",
                f"        def.team = eTeam::{TEAM_MAP[record['team']]};",
                f"        def.mesh.resourceRelativePath = {cpp_string(record['mesh'])};",
                f"        def.shader.runtimePath = {cpp_wstring(record['shader'])};",
                f"        def.submeshStateCount = static_cast<u8_t>({len(record['visibilityStates'])}u);",
            ]
        )
        for state_index, state in enumerate(record["visibilityStates"]):
            visible = "true" if state["visibleWhenDestroyed"] else "false"
            lines.append(
                f"        def.submeshStates[{state_index}].submeshIndex = {state['submeshIndex']}u;"
            )
            lines.append(
                f"        def.submeshStates[{state_index}].bVisibleWhenDestroyed = {visible};"
            )
        lines.extend(["        return def;", "    }", ""])

    for record in object_visual_data["jungles"]:
        symbol = cpp_symbol(record["key"])
        key = definition_key(record["key"])
        lines.extend(
            [
                f"    ClientData::JungleVisualDefinition MakeJungleVisual_{symbol}()",
                "    {",
                "        ClientData::JungleVisualDefinition def{};",
                f"        def.key = 0x{key:08X}u;",
                f"        def.subKind = {record['subKind']}u;",
                f"        def.mesh.resourceRelativePath = {cpp_string(record['mesh'])};",
                f"        def.shader.runtimePath = {cpp_wstring(record['shader'])};",
                f"        def.textureOverrideCount = static_cast<u8_t>({len(record['textureOverrides'])}u);",
            ]
        )
        for override_index, override in enumerate(record["textureOverrides"]):
            lines.append(
                f"        def.textureOverrides[{override_index}].meshIndex = {override['meshIndex']}u;"
            )
            lines.append(
                f"        def.textureOverrides[{override_index}].resourceRelativePath = {cpp_wstring(override['texture'])};"
            )
        lines.extend(["        return def;", "    }", ""])

    for record in object_visual_data["minions"]:
        symbol = cpp_symbol(record["key"])
        key = definition_key(record["key"])
        texture_all = record["textureAllMeshes"]
        lines.extend(
            [
                f"    ClientData::MinionVisualDefinition MakeMinionVisual_{symbol}()",
                "    {",
                "        ClientData::MinionVisualDefinition def{};",
                f"        def.key = 0x{key:08X}u;",
                f"        def.type = {record['type']}u;",
                f"        def.team = {record['team']}u;",
                f"        def.mesh.resourceRelativePath = {cpp_string(record['mesh'])};",
                f"        def.shader.runtimePath = {cpp_wstring(record['shader'])};",
            ]
        )
        if texture_all:
            lines.append(f"        def.textureAllMeshes.resourceRelativePath = {cpp_wstring(texture_all)};")
        lines.extend(["        return def;", "    }", ""])

    for record in object_visual_data["ambientProps"]["props"]:
        symbol = cpp_symbol(record["key"])
        key = definition_key(record["key"])
        lines.extend(
            [
                f"    ClientData::AmbientPropVisualDefinition MakeAmbientPropVisual_{symbol}()",
                "    {",
                "        ClientData::AmbientPropVisualDefinition def{};",
                f"        def.key = 0x{key:08X}u;",
                f"        def.kind = {record['kind']}u;",
                f"        def.mesh.resourceRelativePath = {cpp_string(record['mesh'])};",
                f"        def.shader.runtimePath = {cpp_wstring(record['shader'])};",
                f"        def.idleAnimation = {cpp_string(record['idleAnimation'])};",
                "        return def;",
                "    }",
                "",
            ]
        )

    map_runtime = object_visual_data["mapRuntime"]
    lines.extend(
        [
            "    ClientData::MapRuntimeVisualDefinition MakeMapRuntimeVisual()",
            "    {",
            "        ClientData::MapRuntimeVisualDefinition def{};",
            f"        def.baseMapMesh.resourceRelativePath = {cpp_string(map_runtime['baseMapMesh'])};",
            f"        def.baseMapSurface.resourceRelativePath = {cpp_wstring(map_runtime['baseMapSurface'])};",
            f"        def.fullLayerMapMesh.resourceRelativePath = {cpp_string(map_runtime['fullLayerMapMesh'])};",
            f"        def.fullLayerMapSurface.resourceRelativePath = {cpp_wstring(map_runtime['fullLayerMapSurface'])};",
            f"        def.brushVolumeCsv.resourceRelativePath = {cpp_wstring(map_runtime['brushVolumeCsv'])};",
            f"        def.brushVolumeBinary.resourceRelativePath = {cpp_wstring(map_runtime['brushVolumeBinary'])};",
            f"        def.attackRangeTexture.resourceRelativePath = {cpp_wstring(map_runtime['attackRangeTexture'])};",
            "        return def;",
            "    }",
            "",
        ]
    )

    for record in object_visual_data["fxMeshPreloads"]:
        symbol = cpp_symbol(record["key"])
        key = definition_key(record["key"])
        lines.extend(
            [
                f"    ClientData::FxMeshPreloadVisualDefinition MakeFxMeshPreloadVisual_{symbol}()",
                "    {",
                "        ClientData::FxMeshPreloadVisualDefinition def{};",
                f"        def.key = 0x{key:08X}u;",
                f"        def.mesh.resourceRelativePath = {cpp_string(record['mesh'])};",
                f"        def.texture.resourceRelativePath = {cpp_wstring(record['texture'])};",
                "        return def;",
                "    }",
                "",
            ]
        )

    lines.extend(["    const ClientData::ChampionVisualDefinition kChampionVisuals[] =", "    {"])
    lines.extend(
        f"        MakeChampionVisual_{champion['champion']}(),"
        for champion in sorted(visual_data["champions"], key=lambda item: item["champion"])
    )
    lines.extend(["    };", "", "    const ClientData::ChampionModelVisualDefinition kChampionModelVisuals[] =", "    {"])
    lines.extend(
        f"        MakeChampionModelVisual_{cpp_symbol(record['champion'])}(),"
        for record in champion_asset_visual_data["models"]
    )
    lines.extend(
        [
            "    };",
            "",
            "    const ClientData::ChampionModelVisualPack kChampionModelVisualPack =",
            "    {",
            "        kChampionModelVisuals,",
            "        static_cast<u32_t>(sizeof(kChampionModelVisuals) / sizeof(kChampionModelVisuals[0]))",
            "    };",
            "",
            "    const ClientData::ChampionUiVisualDefinition kChampionUiVisuals[] =",
            "    {",
        ]
    )
    lines.extend(
        f"        MakeChampionUiVisual_{cpp_symbol(record['champion'])}(),"
        for record in champion_asset_visual_data["ui"]
    )
    lines.extend(["    };", "", "    const ClientData::StructureVisualDefinition kStructureVisuals[] =", "    {"])
    lines.extend(
        f"        MakeStructureVisual_{cpp_symbol(record['key'])}(),"
        for record in object_visual_data["structures"]
    )
    lines.extend(["    };", "", "    const ClientData::JungleVisualDefinition kJungleVisuals[] =", "    {"])
    lines.extend(
        f"        MakeJungleVisual_{cpp_symbol(record['key'])}(),"
        for record in object_visual_data["jungles"]
    )
    lines.extend(["    };", "", "    const ClientData::MinionVisualDefinition kMinionVisuals[] =", "    {"])
    lines.extend(
        f"        MakeMinionVisual_{cpp_symbol(record['key'])}(),"
        for record in object_visual_data["minions"]
    )
    lines.extend(["    };", "", "    const ClientData::AmbientPropVisualDefinition kAmbientPropVisuals[] =", "    {"])
    lines.extend(
        f"        MakeAmbientPropVisual_{cpp_symbol(record['key'])}(),"
        for record in object_visual_data["ambientProps"]["props"]
    )
    lines.extend(
        [
            "    };",
            "",
            "    const ClientData::AmbientPropVisualPack kAmbientPropVisualPack =",
            "    {",
            f"        {{ {cpp_wstring(object_visual_data['ambientProps']['placement'])} }},",
            "        kAmbientPropVisuals,",
            "        static_cast<u32_t>(sizeof(kAmbientPropVisuals) / sizeof(kAmbientPropVisuals[0]))",
            "    };",
            "",
            "    const ClientData::MapRuntimeVisualDefinition kMapRuntimeVisual = MakeMapRuntimeVisual();",
            "",
            "    const ClientData::FxMeshPreloadVisualDefinition kFxMeshPreloadVisuals[] =",
            "    {",
        ]
    )
    lines.extend(
        f"        MakeFxMeshPreloadVisual_{cpp_symbol(record['key'])}(),"
        for record in object_visual_data["fxMeshPreloads"]
    )
    lines.extend(
        [
            "    };",
            "",
            "    const ClientData::FxMeshPreloadVisualPack kFxMeshPreloadVisualPack =",
            "    {",
            "        kFxMeshPreloadVisuals,",
            "        static_cast<u32_t>(sizeof(kFxMeshPreloadVisuals) / sizeof(kFxMeshPreloadVisuals[0]))",
            "    };",
        ]
    )
    lines.extend(
        [
            "}",
            "",
            "namespace ClientData",
            "{",
            "    const ChampionVisualDefinition* FindChampionVisualDefinition(eChampion champion)",
            "    {",
            "        for (const ChampionVisualDefinition& definition : kChampionVisuals)",
            "        {",
            "            if (definition.legacyChampion == champion)",
            "                return &definition;",
            "        }",
            "        return nullptr;",
            "    }",
            "",
            "    f32_t ResolveChampionModelYawOffset(eChampion champion)",
            "    {",
            "        const ChampionVisualDefinition* definition = FindChampionVisualDefinition(champion);",
            "        return definition ? definition->modelYawOffsetRadians : 0.f;",
            "    }",
            "",
            "    const ChampionModelVisualPack& GetChampionModelVisualPack()",
            "    {",
            "        return kChampionModelVisualPack;",
            "    }",
            "",
            "    const ChampionModelVisualDefinition* FindChampionModelVisualDefinition(eChampion champion)",
            "    {",
            "        for (const ChampionModelVisualDefinition& definition : kChampionModelVisuals)",
            "        {",
            "            if (definition.champion == champion)",
            "                return &definition;",
            "        }",
            "        return nullptr;",
            "    }",
            "",
            "    const ChampionUiVisualDefinition* FindChampionUiVisualDefinition(eChampion champion)",
            "    {",
            "        for (const ChampionUiVisualDefinition& definition : kChampionUiVisuals)",
            "        {",
            "            if (definition.champion == champion)",
            "                return &definition;",
            "        }",
            "        return nullptr;",
            "    }",
            "",
            "    const StructureVisualDefinition* FindStructureVisualDefinition(Winters::Map::eObjectKind kind, eTeam team)",
            "    {",
            "        for (const StructureVisualDefinition& definition : kStructureVisuals)",
            "        {",
            "            if (definition.kind == kind && definition.team == team)",
            "                return &definition;",
            "        }",
            "        return nullptr;",
            "    }",
            "",
            "    const JungleVisualDefinition* FindJungleVisualDefinition(u32_t subKind)",
            "    {",
            "        for (const JungleVisualDefinition& definition : kJungleVisuals)",
            "        {",
            "            if (definition.subKind == subKind)",
            "                return &definition;",
            "        }",
            "        return nullptr;",
            "    }",
            "",
            "    const AmbientPropVisualPack& GetAmbientPropVisualPack()",
            "    {",
            "        return kAmbientPropVisualPack;",
            "    }",
            "",
            "    const AmbientPropVisualDefinition* FindAmbientPropVisualDefinition(u32_t kind)",
            "    {",
            "        for (const AmbientPropVisualDefinition& definition : kAmbientPropVisuals)",
            "        {",
            "            if (definition.kind == kind)",
            "                return &definition;",
            "        }",
            "        return nullptr;",
            "    }",
            "",
            "    const MapRuntimeVisualDefinition& GetMapRuntimeVisualDefinition()",
            "    {",
            "        return kMapRuntimeVisual;",
            "    }",
            "",
            "    const FxMeshPreloadVisualPack& GetFxMeshPreloadVisualPack()",
            "    {",
            "        return kFxMeshPreloadVisualPack;",
            "    }",
            "",
            "    const MinionVisualDefinition* FindMinionVisualDefinition(u32_t type, u32_t team)",
            "    {",
            "        for (const MinionVisualDefinition& definition : kMinionVisuals)",
            "        {",
            "            if (definition.type == type && definition.team == team)",
            "                return &definition;",
            "        }",
            "        return nullptr;",
            "    }",
            "}",
            "",
        ]
    )
    return "\n".join(lines)


def append_jungle_def(lines: list[str], variable_name: str, record: dict) -> None:
    lines.extend(
        [
            f"    JungleCampGameDef {variable_name}()",
            "    {",
            "        JungleCampGameDef def{};",
        ]
    )
    for key, _ in JUNGLE_FIELDS:
        lines.append(f"        def.{key} = {cpp_float(record[key])};")
    lines.extend(["        return def;", "    }", ""])


def append_minion_def(lines: list[str], variable_name: str, record: dict) -> None:
    lines.extend(
        [
            f"    MinionCombatDef {variable_name}()",
            "    {",
            "        MinionCombatDef def{};",
        ]
    )
    for key, _ in MINION_FIELDS:
        lines.append(f"        def.{key} = {cpp_float(record[key])};")
    lines.extend(["        return def;", "    }", ""])


def append_spawn_object_cpp(lines: list[str], spawn_object_data: dict) -> None:
    loadout = spawn_object_data["spawnLoadout"]
    collider = spawn_object_data["championCollider"]
    structure = spawn_object_data["structure"]
    turret_ai = structure["turretAI"]

    lines.extend(
        [
            "    SpawnLoadoutPolicyDef MakeSpawnLoadoutPolicy()",
            "    {",
            "        SpawnLoadoutPolicyDef def{};",
            f"        def.startGold = {loadout['startGold']}u;",
            f"        def.startLevel = static_cast<u8_t>({loadout['startLevel']}u);",
            f"        def.startRune = eRuneId::{RUNE_MAP[loadout['startRune']]};",
            f"        def.startRuneCount = static_cast<u8_t>({loadout['startRuneCount']}u);",
            f"        def.respawnDelaySec = {cpp_float(loadout['respawnDelaySec'])};",
            "        return def;",
            "    }",
            "",
            "    ChampionColliderProfileDef MakeChampionColliderProfile()",
            "    {",
            "        ChampionColliderProfileDef def{};",
            f"        def.bodyHeight = {cpp_float(collider['bodyHeight'])};",
            f"        def.bodyOffsetY = {cpp_float(collider['bodyOffsetY'])};",
            "        return def;",
            "    }",
            "",
            "    StructureGameDef MakeStructureGameDef()",
            "    {",
            "        StructureGameDef def{};",
            f"        def.turretMaxHp = {cpp_float(structure['turretMaxHp'])};",
            f"        def.inhibitorMaxHp = {cpp_float(structure['inhibitorMaxHp'])};",
            f"        def.nexusMaxHp = {cpp_float(structure['nexusMaxHp'])};",
            f"        def.turretAI.attackRange = {cpp_float(turret_ai['attackRange'])};",
            f"        def.turretAI.attackCooldownMax = {cpp_float(turret_ai['attackCooldownMax'])};",
            f"        def.turretAI.attackDamage = {cpp_float(turret_ai['attackDamage'])};",
            f"        def.turretAI.nexusAttackDamage = {cpp_float(turret_ai['nexusAttackDamage'])};",
            f"        def.turretAI.projectileSpeed = {cpp_float(turret_ai['projectileSpeed'])};",
            f"        def.turretAI.turretSightRange = {cpp_float(turret_ai['turretSightRange'])};",
            f"        def.turretAI.structureSightRange = {cpp_float(turret_ai['structureSightRange'])};",
            f"        def.turretAI.bodyHeight = {cpp_float(turret_ai['bodyHeight'])};",
            f"        def.turretAI.bodyOffsetY = {cpp_float(turret_ai['bodyOffsetY'])};",
            "        return def;",
            "    }",
            "",
        ]
    )

    append_jungle_def(lines, "MakeDefaultJungleCamp", spawn_object_data["defaultJungleCamp"])
    for record in spawn_object_data["jungleCamps"]:
        append_jungle_def(lines, f"MakeJungleCamp_{record['subKind']}", record)

    append_minion_def(lines, "MakeDefaultMinionCombat", spawn_object_data["defaultMinion"])
    for record in spawn_object_data["minions"]:
        append_minion_def(lines, f"MakeMinionCombat_{record['roleType']}", record)

    lines.extend(["    const JungleCampGameDefEntry kJungleCamps[] =", "    {"])
    lines.extend(
        f"        {{ static_cast<u8_t>({record['subKind']}u), MakeJungleCamp_{record['subKind']}() }},"
        for record in spawn_object_data["jungleCamps"]
    )
    lines.extend(["    };", "", "    const MinionCombatDefEntry kMinions[] =", "    {"])
    lines.extend(
        f"        {{ static_cast<u8_t>({record['roleType']}u), MakeMinionCombat_{record['roleType']}() }},"
        for record in spawn_object_data["minions"]
    )
    lines.extend(["    };", ""])


def emit_cpp(
    data: dict,
    spawn_object_data: dict,
    champions: list[dict],
    skills: list[dict],
    spells: list[dict],
    build_hash: int,
) -> str:
    champion_ids = {record["canonicalKey"]: record["defId"] for record in champions}
    skill_ids = {record["canonicalKey"]: record["defId"] for record in skills}
    lines = [
        '#include "Server/Private/Data/LoLGameplayDefinitionPack.h"',
        "",
        "namespace",
        "{",
        f"    inline constexpr u32_t kBuildHash = 0x{build_hash:08X}u;",
        "",
    ]

    for record in champions:
        name = record["champion"]
        lines.extend(
            [
                f"    ChampionGameplayDef MakeChampion_{name}()",
                "    {",
                "        ChampionGameplayDef def{};",
                f"        def.key = 0x{record['definitionKey']:08X}u;",
                f"        def.id.value = {record['defId']}u;",
                f"        def.legacyChampion = eChampion::{name};",
                f"        def.dataVersion = {record['dataVersion']}u;",
                "        def.authoringHash = kBuildHash;",
            ]
        )
        for field, value in record["stats"].items():
            lines.append(f"        def.stats.{field} = {cpp_float(value)};")
        for slot in range(5):
            lines.append(
                f"        def.skillLoadout[{slot}].value = {skill_ids[canonical_skill(name, slot)]}u;"
            )
        passive = record["passiveDash"]
        if passive is not None:
            lines.extend(
                [
                    "        def.passiveDash.bValid = true;",
                    f"        def.passiveDash.distance = {cpp_float(passive['distance'])};",
                    f"        def.passiveDash.durationSec = {cpp_float(passive['durationSec'])};",
                    f"        def.passiveDash.inputGraceSec = {cpp_float(passive['inputGraceSec'])};",
                ]
            )
        passive_soul = record.get("passiveSoul")
        if passive_soul is not None:
            lines.extend(
                [
                    "        def.passiveSoul.bValid = true;",
                    f"        def.passiveSoul.lifetimeSec = {cpp_float(passive_soul['lifetimeSec'])};",
                    f"        def.passiveSoul.radius = {cpp_float(passive_soul['radius'])};",
                ]
            )
        lines.extend(["        return def;", "    }", ""])

    for record in skills:
        function_name = f"{record['ownerChampion']}_{SLOT_NAMES[record['slot']]}".upper()
        facing = "None"
        if record["targetShape"] == "Unit":
            facing = "TowardsTarget"
        elif record["targetShape"] in ("Ground", "Direction"):
            facing = "TowardsCommandDirection"
        lines.extend(
            [
                f"    SkillGameplayDef MakeSkill_{function_name}()",
                "    {",
                "        SkillGameplayDef def{};",
                f"        def.key = 0x{record['definitionKey']:08X}u;",
                f"        def.id.value = {record['defId']}u;",
                f"        def.ownerChampionId.value = {champion_ids[record['ownerKey']]}u;",
                f"        def.slot = {record['slot']}u;",
                f"        def.legacySkillId = {record['skillId']}u;",
                "        def.target.bValid = true;",
                f"        def.target.resolvePolicy = eTargetResolvePolicy::{record['resolvePolicy']};",
                f"        def.cost.manaCost = {cpp_float(record['manaCost'])};",
                f"        def.cooldown.cooldownSec = {cpp_float(record['cooldownSec'])};",
                f"        def.range.rangeMax = {cpp_float(record['rangeMax'])};",
                f"        def.stage.stageCount = {record['stageCount']}u;",
                f"        def.stage.stageWindowSec = {cpp_float(record['stageWindowSec'])};",
                f"        def.effect.scalingTableId = {record['scalingTableId']}u;",
                f"        def.effect.gameplayPolicyId = {record['gameplayPolicyId']}u;",
                f"        def.effect.replicatedCueId = {record['visualCueId']}u;",
            ]
        )
        effect_params = record.get("effectParams", [])
        if effect_params:
            lines.append(f"        def.effect.paramCount = static_cast<u8_t>({len(effect_params)}u);")
            for param_index, param in enumerate(effect_params):
                lines.append(
                    f"        def.effect.params[{param_index}].id = eSkillEffectParamId::{param['cppId']};"
                )
                lines.append(
                    f"        def.effect.params[{param_index}].value = {cpp_float(param['value'])};"
                )
        summon_policy_params = record.get("summonPolicyParams", [])
        if summon_policy_params:
            lines.append("        def.summonPolicy.bValid = true;")
            lines.append(
                f"        def.summonPolicy.paramCount = static_cast<u8_t>({len(summon_policy_params)}u);"
            )
            for param_index, param in enumerate(summon_policy_params):
                lines.append(
                    f"        def.summonPolicy.params[{param_index}].id = eSummonPolicyParamId::{param['cppId']};"
                )
                lines.append(
                    f"        def.summonPolicy.params[{param_index}].value = {cpp_float(param['value'])};"
                )
        for stage_index, stage in enumerate(record["stages"]):
            lines.append(f"        def.target.shape[{stage_index}] = eTargetShape::{record['targetShape']};")
            lines.append(f"        def.stage.lockDurationSec[{stage_index}] = {cpp_float(stage['lockDurationSec'])};")
            lines.append(f"        def.facing.mode[{stage_index}] = eSkillFacingMode::{facing};")
        lines.extend(["        return def;", "    }", ""])

    for record in spells:
        lines.extend(
            [
                f"    SummonerSpellGameplayDef MakeSummonerSpell_{record['legacySpellId'] if 'legacySpellId' in record else record['spellId']}()",
                "    {",
                "        SummonerSpellGameplayDef def{};",
                f"        def.key = 0x{record['definitionKey']:08X}u;",
                f"        def.id.value = {record['defId']}u;",
                f"        def.legacySpellId = {record['spellId']}u;",
                f"        def.rangeMax = {cpp_float(record['rangeMax'])};",
                f"        def.cooldownSec = {cpp_float(record['cooldownSec'])};",
                f"        def.gameplayPolicyId = {record['gameplayPolicyId']}u;",
                f"        def.replicatedCueId = {record['visualCueId']}u;",
                "        return def;",
                "    }",
                "",
            ]
        )

    append_spawn_object_cpp(lines, spawn_object_data)

    lines.extend(["    const ChampionGameplayDef kChampions[] =", "    {"])
    lines.extend(f"        MakeChampion_{record['champion']}()," for record in champions)
    lines.extend(["    };", "", "    const SkillGameplayDef kSkills[] =", "    {"])
    lines.extend(
        f"        MakeSkill_{record['ownerChampion']}_{SLOT_NAMES[record['slot']]}(),".upper().replace("MAKESKILL", "MakeSkill")
        for record in skills
    )
    lines.extend(["    };", "", "    const SummonerSpellGameplayDef kSummonerSpells[] =", "    {"])
    lines.extend(f"        MakeSummonerSpell_{record['spellId']}()," for record in spells)
    lines.extend(
        [
            "    };",
            "}",
            "",
            "namespace ServerData",
            "{",
            "    const GameplayDefinitionPack& GetLoLGameplayDefinitionPack()",
            "    {",
            "        static const GameplayDefinitionPack pack =",
            "        {",
            f"            {{ {data['schemaVersion']}u, {max(c['dataVersion'] for c in champions)}u, kBuildHash, 0u, eDataPackVisibility::ServerPrivate }},",
            "            kChampions,",
            "            sizeof(kChampions) / sizeof(kChampions[0]),",
            "            kSkills,",
            "            sizeof(kSkills) / sizeof(kSkills[0]),",
            "            kSummonerSpells,",
            "            sizeof(kSummonerSpells) / sizeof(kSummonerSpells[0]),",
            "        };",
            "        return pack;",
            "    }",
            "",
            "    const SpawnObjectDefinitionPack& GetLoLSpawnObjectDefinitionPack()",
            "    {",
            "        static const SpawnObjectDefinitionPack pack =",
            "        {",
            f"            {{ {spawn_object_data['schemaVersion']}u, {spawn_object_data['dataVersion']}u, kBuildHash, 0u, eDataPackVisibility::ServerPrivate }},",
            "            MakeSpawnLoadoutPolicy(),",
            "            MakeChampionColliderProfile(),",
            "            MakeStructureGameDef(),",
            "            MakeDefaultJungleCamp(),",
            "            kJungleCamps,",
            "            sizeof(kJungleCamps) / sizeof(kJungleCamps[0]),",
            "            MakeDefaultMinionCombat(),",
            "            kMinions,",
            "            sizeof(kMinions) / sizeof(kMinions[0]),",
            "        };",
            "        return pack;",
            "    }",
            "}",
            "",
        ]
    )
    return "\n".join(lines)


def json_text(value: object) -> str:
    return json.dumps(value, ensure_ascii=True, indent=2, sort_keys=False) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    root = args.root.resolve()
    source = root / "Data" / "Gameplay" / "ChampionGameData" / "champions.json"
    if not source.exists():
        fail(f"missing source: {source}")
    summoner_spell_source = root / "Data" / "LoL" / "ServerPrivate" / "Gameplay" / "SummonerSpellGameplayDefs.json"
    if not summoner_spell_source.exists():
        fail(f"missing source: {summoner_spell_source}")
    spawn_object_source = root / "Data" / "LoL" / "ServerPrivate" / "Gameplay" / "SpawnObjectGameplayDefs.json"
    if not spawn_object_source.exists():
        fail(f"missing source: {spawn_object_source}")
    skill_effect_source = root / "Data" / "LoL" / "ServerPrivate" / "Gameplay" / "SkillEffectGameplayDefs.json"
    if not skill_effect_source.exists():
        fail(f"missing source: {skill_effect_source}")
    champion_visual_source = root / "Data" / "LoL" / "ClientPublic" / "Visual" / "ChampionVisualDefs.json"
    if not champion_visual_source.exists():
        fail(f"missing source: {champion_visual_source}")
    object_visual_source = root / "Data" / "LoL" / "ClientPublic" / "Visual" / "ObjectVisualDefs.json"
    if not object_visual_source.exists():
        fail(f"missing source: {object_visual_source}")
    champion_asset_visual_source = root / "Data" / "LoL" / "ClientPublic" / "Visual" / "ChampionAssetVisualDefs.json"
    if not champion_asset_visual_source.exists():
        fail(f"missing source: {champion_asset_visual_source}")

    data = legacy.normalize_root(json.loads(source.read_text(encoding="utf-8")))
    summoner_spell_raw = json.loads(summoner_spell_source.read_text(encoding="utf-8"))
    summoner_spell_raw.pop("buildHash", None)
    summoner_spell_data = normalize_summoner_spell_root(summoner_spell_raw)
    champions, skills, spells = make_records(data, summoner_spell_data)
    champion_visual_raw = json.loads(champion_visual_source.read_text(encoding="utf-8"))
    champion_visual_raw.pop("buildHash", None)
    champion_visual_data = normalize_client_visual_root(
        champion_visual_raw,
        {canonical_champion(champion["champion"]): champion["champion"] for champion in data["champions"]},
        {record["canonicalKey"] for record in skills},
    )
    spawn_object_raw = json.loads(spawn_object_source.read_text(encoding="utf-8"))
    spawn_object_raw.pop("buildHash", None)
    spawn_object_data = normalize_spawn_object_root(spawn_object_raw)
    object_visual_raw = json.loads(object_visual_source.read_text(encoding="utf-8"))
    object_visual_raw.pop("buildHash", None)
    object_visual_data = normalize_object_visual_root(object_visual_raw)
    champion_asset_visual_raw = json.loads(champion_asset_visual_source.read_text(encoding="utf-8"))
    champion_asset_visual_raw.pop("buildHash", None)
    champion_asset_visual_data = normalize_champion_asset_visual_root(
        champion_asset_visual_raw,
        {champion["champion"] for champion in data["champions"]},
    )
    skill_effect_raw = json.loads(skill_effect_source.read_text(encoding="utf-8"))
    skill_effect_raw.pop("buildHash", None)
    skill_effect_data = normalize_skill_effect_root(
        skill_effect_raw,
        {record["canonicalKey"] for record in skills},
    )
    apply_skill_effect_params(skills, skill_effect_data)
    build_hash = compute_definition_pack_hash(data, summoner_spell_data, spawn_object_data, skill_effect_data)
    champion_ids = {record["canonicalKey"]: record["defId"] for record in champions}
    skill_ids = {record["canonicalKey"]: record["defId"] for record in skills}

    outputs = {
        root / "Data" / "LoL" / "ServerPrivate" / "Gameplay" / "ChampionGameplayDefs.json": json_text(
            {
                "schemaVersion": data["schemaVersion"],
                "buildHash": build_hash,
                "champions": [server_champion_json(record, skill_ids) for record in champions],
            }
        ),
        root / "Data" / "LoL" / "ServerPrivate" / "Gameplay" / "SkillGameplayDefs.json": json_text(
            {
                "schemaVersion": data["schemaVersion"],
                "buildHash": build_hash,
                "skills": [server_skill_json(record, champion_ids) for record in skills],
            }
        ),
        root / "Data" / "LoL" / "ServerPrivate" / "Gameplay" / "SummonerSpellGameplayDefs.json": json_text(
            {
                "schemaVersion": summoner_spell_data["schemaVersion"],
                "buildHash": build_hash,
                "summonerSpells": [
                    {
                        "key": record["canonicalKey"],
                        "definitionKey": record["definitionKey"],
                        "defId": record["defId"],
                        "legacySpellId": record["spellId"],
                        "rangeMax": record["rangeMax"],
                        "cooldownSec": record["cooldownSec"],
                        "gameplayPolicyId": record["gameplayPolicyId"],
                        "replicatedCueId": record["visualCueId"],
                    }
                    for record in spells
                ],
            }
        ),
        root / "Data" / "LoL" / "ServerPrivate" / "Gameplay" / "SpawnObjectGameplayDefs.json": json_text(
            {
                "schemaVersion": spawn_object_data["schemaVersion"],
                "dataVersion": spawn_object_data["dataVersion"],
                "buildHash": build_hash,
                "spawnLoadout": spawn_object_data["spawnLoadout"],
                "championCollider": spawn_object_data["championCollider"],
                "structure": spawn_object_data["structure"],
                "defaultJungleCamp": spawn_object_data["defaultJungleCamp"],
                "jungleCamps": spawn_object_data["jungleCamps"],
                "defaultMinion": spawn_object_data["defaultMinion"],
                "minions": spawn_object_data["minions"],
            }
        ),
        root / "Data" / "LoL" / "ServerPrivate" / "Gameplay" / "SkillEffectGameplayDefs.json": json_text(
            {
                "schemaVersion": skill_effect_data["schemaVersion"],
                "dataVersion": skill_effect_data["dataVersion"],
                "buildHash": build_hash,
                "skillEffects": [
                    skill_effect_json(record)
                    for record in skill_effect_data["skillEffects"]
                ],
            }
        ),
        root / "Data" / "LoL" / "ClientPublic" / "Visual" / "ChampionVisualDefs.json": json_text(
            client_visual_json(champion_visual_data)
        ),
        root / "Data" / "LoL" / "ClientPublic" / "Visual" / "ObjectVisualDefs.json": json_text(
            object_visual_data
        ),
        root / "Data" / "LoL" / "ClientPublic" / "Visual" / "ChampionAssetVisualDefs.json": json_text(
            champion_asset_visual_data
        ),
        root / "Data" / "LoL" / "SharedContract" / "DefinitionManifest.json": json_text(
            manifest_json(data, champions, skills, spells, build_hash)
        ),
        root / "Server" / "Private" / "Data" / "Generated" / "LoLGameplayDefinitions.generated.cpp": emit_cpp(
            data, spawn_object_data, champions, skills, spells, build_hash
        ),
        root / "Client" / "Private" / "Data" / "Generated" / "LoLVisualDefinitions.generated.cpp": emit_client_visual_cpp(
            champion_visual_data, object_visual_data, champion_asset_visual_data
        ),
    }

    parity = {
        "status": "PASS",
        "source": str(source.relative_to(root)).replace("\\", "/"),
        "buildHash": build_hash,
        "championCount": len(champions),
        "skillCount": len(skills),
        "summonerSpellCount": len(spells),
        "definitionKeyCount": len(champions) + len(skills) + len(spells),
        "definitionKeyCollisionCount": 0,
        "serverVisualOwnershipViolationCount": 0,
        "notes": [
            "DefId is pack-local and never a network or save identifier.",
            "DefinitionKey is the stable SharedContract identifier.",
            "Animation playback and model yaw are emitted only to ClientPublic.",
            "Spawn/object gameplay policy is emitted only to ServerPrivate.",
            "Skill effect scalar params are emitted only to ServerPrivate.",
        ],
    }
    outputs[root / ".md" / "TODO" / "06-22" / "LOL_DEFINITION_PACK_PARITY.json"] = json_text(parity)

    stale = []
    for path, content in outputs.items():
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != content:
                stale.append(path)
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8", newline="\n")

    if stale:
        for path in stale:
            print(f"STALE {path.relative_to(root)}")
        fail("generated definition pack is stale; run without --check")

    verb = "Checked" if args.check else "Generated"
    print(f"{verb} LoL definition pack 0x{build_hash:08X}")
    print(f"Champions: {len(champions)}, skills: {len(skills)}, summoner spells: {len(spells)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
