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
    "halfAngleCos": "HalfAngleCos",
    "radius": "Radius",
    "shieldDurationSec": "ShieldDurationSec",
    "shieldBaseAmount": "ShieldBaseAmount",
    "shieldAmountPerRank": "ShieldAmountPerRank",
    "shieldArmorPerRank": "ShieldArmorPerRank",
    "summonDurationSec": "SummonDurationSec",
    "summonMoveSpeed": "SummonMoveSpeed",
    "summonAttackRange": "SummonAttackRange",
    "summonSightRange": "SummonSightRange",
    "summonAttackCooldownSec": "SummonAttackCooldownSec",
    "summonBaseAttackDamage": "SummonBaseAttackDamage",
    "summonAttackDamagePerRank": "SummonAttackDamagePerRank",
    "summonBaseHp": "SummonBaseHp",
    "summonHpPerRank": "SummonHpPerRank",
    "summonRadius": "SummonRadius",
}

SKILL_EFFECT_PARAM_MAX = 16

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


def require_object(value: object, path: str) -> dict:
    if not isinstance(value, dict):
        fail(f"{path} must be an object")
    return value


def require_array(value: object, path: str) -> list:
    if not isinstance(value, list):
        fail(f"{path} must be an array")
    return value


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

        records.append({"key": key, "params": params})

    return {
        "schemaVersion": legacy.as_int(root.get("schemaVersion", 1), "skillEffect.schemaVersion"),
        "dataVersion": legacy.as_int(root.get("dataVersion", 1), "skillEffect.dataVersion"),
        "skillEffects": sorted(records, key=lambda record: record["key"]),
    }


def apply_skill_effect_params(skills: list[dict], skill_effect_data: dict) -> None:
    by_key = {record["key"]: record["params"] for record in skill_effect_data["skillEffects"]}
    for skill in skills:
        skill["effectParams"] = by_key.get(skill["canonicalKey"], [])


def compute_definition_pack_hash(data: dict, spawn_object_data: dict, skill_effect_data: dict) -> int:
    stable = json.dumps(
        {
            "championGameplay": data,
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


def make_records(data: dict) -> tuple[list[dict], list[dict], list[dict]]:
    champions = []
    skills = []
    spells = []

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

    for spell in data["summonerSpells"]:
        if spell["spellId"] == 0:
            continue
        key = canonical_spell(spell["spellId"])
        spells.append({**spell, "canonicalKey": key, "definitionKey": definition_key(key)})

    champions.sort(key=lambda record: record["canonicalKey"])
    skills.sort(key=lambda record: record["canonicalKey"])
    spells.sort(key=lambda record: record["canonicalKey"])

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
    return result


def server_skill_json(record: dict, champion_ids: dict[str, int]) -> dict:
    return {
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


def client_visual_json(data: dict) -> dict:
    champions = []
    for champion in data["champions"]:
        skills = []
        for skill in champion["skills"]:
            skills.append(
                {
                    "key": canonical_skill(champion["champion"], skill["slot"]),
                    "replicatedCueId": skill["visualCueId"],
                    "stages": [
                        {
                            "animationPlaybackSpeed": stage["animPlaySpeed"],
                            "castFrame": stage["castFrame"],
                            "recoveryFrame": stage["recoveryFrame"],
                        }
                        for stage in skill["stages"][: skill["stageCount"]]
                    ],
                }
            )
        champions.append(
            {
                "key": canonical_champion(champion["champion"]),
                "modelYawOffsetRadians": champion["visualYawOffset"],
                "skills": skills,
            }
        )
    return {"schemaVersion": data["schemaVersion"], "champions": champions}


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


def emit_client_visual_cpp(data: dict) -> str:
    lines = [
        '#include "Client/Private/Data/LoLVisualDefinitionPack.h"',
        "",
        "namespace",
        "{",
    ]
    for champion in data["champions"]:
        name = champion["champion"]
        key = definition_key(canonical_champion(name))
        lines.extend(
            [
                f"    ClientData::ChampionVisualDefinition MakeChampionVisual_{name}()",
                "    {",
                "        ClientData::ChampionVisualDefinition def{};",
                f"        def.key = 0x{key:08X}u;",
                f"        def.legacyChampion = eChampion::{name};",
                f"        def.modelYawOffsetRadians = {cpp_float(champion['visualYawOffset'])};",
            ]
        )
        for skill in champion["skills"]:
            slot = skill["slot"]
            lines.append(f"        def.skills[{slot}].stageCount = {skill['stageCount']}u;")
            lines.append(f"        def.skills[{slot}].replicatedCueId = {skill['visualCueId']}u;")
            for stage_index, stage in enumerate(skill["stages"]):
                lines.append(
                    f"        def.skills[{slot}].stages[{stage_index}].animationPlaybackSpeed = "
                    f"{cpp_float(stage['animPlaySpeed'])};"
                )
                lines.append(
                    f"        def.skills[{slot}].stages[{stage_index}].castFrame = {cpp_float(stage['castFrame'])};"
                )
                lines.append(
                    f"        def.skills[{slot}].stages[{stage_index}].recoveryFrame = "
                    f"{cpp_float(stage['recoveryFrame'])};"
                )
        lines.extend(["        return def;", "    }", ""])

    lines.extend(["    const ClientData::ChampionVisualDefinition kChampionVisuals[] =", "    {"])
    lines.extend(
        f"        MakeChampionVisual_{champion['champion']}(),"
        for champion in sorted(data["champions"], key=lambda item: item["champion"])
    )
    lines.extend(
        [
            "    };",
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
    spawn_object_source = root / "Data" / "LoL" / "ServerPrivate" / "Gameplay" / "SpawnObjectGameplayDefs.json"
    if not spawn_object_source.exists():
        fail(f"missing source: {spawn_object_source}")
    skill_effect_source = root / "Data" / "LoL" / "ServerPrivate" / "Gameplay" / "SkillEffectGameplayDefs.json"
    if not skill_effect_source.exists():
        fail(f"missing source: {skill_effect_source}")

    data = legacy.normalize_root(json.loads(source.read_text(encoding="utf-8")))
    spawn_object_raw = json.loads(spawn_object_source.read_text(encoding="utf-8"))
    spawn_object_raw.pop("buildHash", None)
    spawn_object_data = normalize_spawn_object_root(spawn_object_raw)
    champions, skills, spells = make_records(data)
    skill_effect_raw = json.loads(skill_effect_source.read_text(encoding="utf-8"))
    skill_effect_raw.pop("buildHash", None)
    skill_effect_data = normalize_skill_effect_root(
        skill_effect_raw,
        {record["canonicalKey"] for record in skills},
    )
    apply_skill_effect_params(skills, skill_effect_data)
    build_hash = compute_definition_pack_hash(data, spawn_object_data, skill_effect_data)
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
                "schemaVersion": data["schemaVersion"],
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
                    {
                        "key": record["key"],
                        "params": {
                            param["id"]: param["value"]
                            for param in record["params"]
                        },
                    }
                    for record in skill_effect_data["skillEffects"]
                ],
            }
        ),
        root / "Data" / "LoL" / "ClientPublic" / "Visual" / "ChampionVisualDefs.json": json_text(
            client_visual_json(data)
        ),
        root / "Data" / "LoL" / "SharedContract" / "DefinitionManifest.json": json_text(
            manifest_json(data, champions, skills, spells, build_hash)
        ),
        root / "Server" / "Private" / "Data" / "Generated" / "LoLGameplayDefinitions.generated.cpp": emit_cpp(
            data, spawn_object_data, champions, skills, spells, build_hash
        ),
        root / "Client" / "Private" / "Data" / "Generated" / "LoLVisualDefinitions.generated.cpp": emit_client_visual_cpp(
            data
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
