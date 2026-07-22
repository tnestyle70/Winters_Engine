from __future__ import annotations

import argparse
import copy
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "ChampionData"))

import build_champion_game_data as legacy  # noqa: E402


SLOT_NAMES = ("basic_attack", "q", "w", "e", "r")
AI_SLOT_MAP = {
    "BasicAttack": "BasicAttack",
    "Q": "Q",
    "W": "W",
    "E": "E",
    "R": "R",
}
AI_TARGET_MODES = {
    "TargetEntity",
    "AwayFromTarget",
    "WardBehindTarget",
    "LastOwnWard",
    "SylasHijackTarget",
    "SylasStolenUltimateTarget",
    "Self",
}
AI_PROFILE_FLOAT_FIELDS = (
    "preferredRange",
    "championScanRange",
    "minionScanRange",
    "structureScanRange",
    "leashRange",
    "aggression",
    "kiteBias",
    "retreatHpRatio",
    "reengageHpRatio",
    "minionPressureWeight",
    "turretRiskWeight",
    "lastHitWeight",
    "siegeWeight",
)
TARGET_MAP = {
    "Self": ("Self", "Direct"),
    "UnitTarget": ("Unit", "Direct"),
    "GroundTarget": ("Ground", "Direct"),
    "Direction": ("Direction", "Direct"),
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
    "maxStacks": "MaxStacks",
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
    "targetHealthThresholdRatio": "TargetHealthThresholdRatio",
    "acquireRange": "AcquireRange",
    "lifetimeSec": "LifetimeSec",
    "respawnSec": "RespawnSec",
    "sideDotThreshold": "SideDotThreshold",
    "targetMaxHpRatio": "TargetMaxHpRatio",
    "challengeDurationSec": "ChallengeDurationSec",
    "healDurationSec": "HealDurationSec",
    "healRadius": "HealRadius",
    "healIntervalSec": "HealIntervalSec",
    "healAmount": "HealAmount",
    "minHealthAmount": "MinHealthAmount",
    "healBaseAmount": "HealBaseAmount",
    "healAmountPerRank": "HealAmountPerRank",
    "rectLength": "RectLength",
    "rectLengthPerRank": "RectLengthPerRank",
    "rectWidth": "RectWidth",
    "formationDelaySec": "FormationDelaySec",
    "damagePerSpear": "DamagePerSpear",
    "halfWidth": "HalfWidth",
    "disarmDurationSec": "DisarmDurationSec",
    "tornadoSpeed": "TornadoSpeed",
    "tornadoDurationSec": "TornadoDurationSec",
    "tornadoRadius": "TornadoRadius",
    "tornadoDamage": "TornadoDamage",
    "dashAreaRadius": "DashAreaRadius",
    "dashAreaDamage": "DashAreaDamage",
    "bonusAd": "BonusAd",
    "bonusAttackSpeed": "BonusAttackSpeed",
    "totalAdRatio": "TotalAdRatio",
    "bonusAdRatio": "BonusAdRatio",
    "apRatio": "ApRatio",
    "nonEpicBaseDamage": "NonEpicBaseDamage",
    "nonEpicDamagePerRank": "NonEpicDamagePerRank",
    "cooldownRefundSec": "CooldownRefundSec",
    "manaRestoreFlat": "ManaRestoreFlat",
    "castTimeSec": "CastTimeSec",
    "manaCostPerRank": "ManaCostPerRank",
    "cooldownReductionPerRank": "CooldownReductionPerRank",
    "halfAngleCos": "HalfAngleCos",
    "radius": "Radius",
    "shieldDurationSec": "ShieldDurationSec",
    "shieldBaseAmount": "ShieldBaseAmount",
    "shieldAmountPerRank": "ShieldAmountPerRank",
    "shieldArmorPerRank": "ShieldArmorPerRank",
    "healDamageRatio": "HealDamageRatio",
}

SKILL_EFFECT_PARAM_MAX = 16
RANKED_DAMAGE_PARAM_KEYS = {
    ("skill.yasuo.q", "tornadoDamage"),
    ("skill.yasuo.q", "dashAreaDamage"),
    ("skill.leesin.q", "baseDamage"),
    ("skill.kalista.e", "damagePerSpear"),
    ("skill.ezreal.r", "nonEpicBaseDamage"),
}
DAMAGE_TYPES = {
    "Physical": "Physical",
    "Magic": "Magic",
    "True": "True",
}

ITEM_ACTIVE_KIND_MAP = {
    "Ward": "Ward",
    "Stasis": "Stasis",
    "Cleanse": "Cleanse",
    "KalistaOathsworn": "KalistaOathsworn",
}
DAMAGE_FLAGS = {
    "CanCrit": "DamageFlag_CanCrit",
    "CanLifesteal": "DamageFlag_CanLifesteal",
    "OnHit": "DamageFlag_OnHit",
}
DAMAGE_RANK_FIELDS = (
    "flatByRank",
    "totalAdRatioByRank",
    "bonusAdRatioByRank",
    "apRatioByRank",
    "targetMaxHpRatioByRank",
    "targetMissingHpRatioByRank",
)

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

ECONOMY_XP_CURVE_LENGTH = 17

ECONOMY_CHAMPION_KILL_FIELDS = (
    ("killerGold", 0.0),
    ("assistGold", 0.0),
    ("firstBloodBonusGold", 0.0),
    ("victimNextLevelXPFactor", 0.0),
    ("shareRadius", 0.0),
)

ECONOMY_MINION_KINDS = ("melee", "ranged", "siege", "super")

ECONOMY_MINION_FIELDS = (
    ("soloXP", 0.0),
    ("sharedXP", 0.0),
    ("gold", 0.0),
    ("maxGold", 0.0),
    ("growthAmount", 0.0),
    ("growthIntervalSec", 0.0),
)

ECONOMY_JUNGLE_FIELDS = (
    ("smallCampGold", 0.0),
    ("smallCampXP", 0.0),
    ("epicGold", 0.0),
    ("epicXP", 0.0),
    ("baronGold", 0.0),
    ("baronXP", 0.0),
)

ECONOMY_OBJECTIVE_FLOAT_FIELDS = (
    ("teamGoldPerChampion", 2000.0),
    ("buffDurationSec", 300.0),
    ("baronRecallDurationMultiplier", 0.5),
    ("baronAuraRadius", 12.0),
    ("baronMinionHpMultiplier", 3.0),
    ("baronMinionAttackDamageMultiplier", 2.0),
    ("baronMinionScaleMultiplier", 2.0),
    ("elderAttackDamageMultiplier", 1.7),
    ("elderBurnDurationSec", 3.0),
    ("elderBurnTickIntervalSec", 1.0),
    ("elderBurnTargetMaxHpRatioPerTick", 0.01),
    ("elderExecuteThresholdRatio", 0.2),
    ("blueManaRegenPerSec", 10.0),
    ("redHealthRegenPerSec", 10.0),
    ("redBurnDurationSec", 3.0),
    ("redBurnTickIntervalSec", 1.0),
    ("redBurnDamagePerTick", 10.0),
)

ECONOMY_TIMER_FIELDS = (
    ("assistCreditWindowSec", 0.0),
    ("recallDurationSec", 0.0),
)

# ItemStatModifier 멤버명과 1:1 (0이 아닌 필드만 JSON 에 기재).
ITEM_STAT_FIELDS = (
    "flatAd",
    "flatAp",
    "flatHealth",
    "flatMana",
    "flatArmor",
    "flatMr",
    "bonusAttackSpeed",
    "critChance",
    "critDamageBonus",
    "abilityHaste",
    "percentMoveSpeed",
    "armorPenPercent",
    "bonusArmorPenPercent",
    "lethality",
    "magicPenPercent",
    "flatMagicPen",
    "flatMoveSpeed",
    "lifeSteal",
)


def validate_skill_effect_param_domain(param_name: str, value: float, path: str) -> float:
    if param_name == "halfAngleCos":
        if value < -1.0 or value > 1.0:
            fail(f"{path} must be in [-1, 1]")
        return value

    if value < 0.0 or value > 1_000_000.0:
        fail(f"{path} must be in [0, 1000000]")
    return value


def validate_summon_policy_param_domain(param_name: str, value: float, path: str) -> float:
    if value < 0.0 or value > 1_000_000.0:
        fail(f"{path} must be in [0, 1000000]")
    if param_name in {"roleType", "lane"} and (value > 255.0 or value != float(int(value))):
        fail(f"{path} must be an integer-like value in [0, 255]")
    return value

JUNGLE_FIELDS = (
    ("maxHp", 1500.0),
    ("radius", 1.0),
    ("attackRange", 1.7),
    ("attackDamage", 45.0),
    ("attackCooldown", 1.4),
    ("moveSpeed", 4.0),
    ("baseArmor", 20.0),
    ("baseMr", 20.0),
    ("aggroRange", 3.0),
    ("leashRange", 9.0),
    ("respawnDelaySec", 30.0),
)

MINION_FIELDS = (
    ("moveSpeed", 4.0),
    ("attackRange", 1.5),
    ("sightRange", 12.0),
    ("attackDamage", 40.0),
    ("attackCooldownMax", 1.0),
    ("maxHp", 450.0),
)

MINION_WAVE_RANGED_PROJECTILE_FIELDS = (
    ("speed", 14.0),
    ("hitRadius", 0.45),
    ("forwardOffset", 0.45),
    ("spawnHeight", 0.85),
    ("maxDistancePadding", 2.0),
)

MINION_BEHAVIOR_FLOAT_FIELDS = (
    ("pathAgentRadius", 0.0),
    ("laneClearanceRadius", 0.0),
    ("softSeparationRadiusScale", 0.0),
    ("softSeparationWeight", 0.0),
    ("defaultSeparationWeight", 0.0),
    ("softSeparationMaxStep", 0.0),
    ("lanePathRebuildIntervalSec", 0.0),
    ("chasePathRebuildIntervalSec", 0.0),
    ("pathTargetRefreshDistanceSq", 0.0),
    ("pathWaypointArriveRadius", 0.0),
    ("flowFieldProgressSlackSq", 0.0),
    ("structureAcquireRangePadding", 0.0),
    ("targetScanIntervalSec", 0.0),
    ("attackExitRangePadding", 0.0),
    ("meleeAttackWindupSec", 0.0),
    ("rangedAttackWindupSec", 0.0),
    ("attackRecoverySec", 0.0),
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
    respawn_delay = legacy.as_float(
        source.get("respawnDelaySec", 0.0), "spawnLoadout.respawnDelaySec")
    respawn_by_level = source.get("respawnDelaySecByLevel")
    if respawn_by_level is None:
        respawn_by_level = [respawn_delay] * 18
    if not isinstance(respawn_by_level, list) or len(respawn_by_level) != 18:
        fail("spawnLoadout.respawnDelaySecByLevel must contain 18 values")
    respawn_by_level = [
        legacy.as_float(value, f"spawnLoadout.respawnDelaySecByLevel[{index}]")
        for index, value in enumerate(respawn_by_level)
    ]
    return {
        "startGold": legacy.as_int(source.get("startGold", 0), "spawnLoadout.startGold"),
        "startLevel": legacy.as_int(source.get("startLevel", 0), "spawnLoadout.startLevel"),
        "startRune": rune,
        "startRuneCount": legacy.as_int(source.get("startRuneCount", 0), "spawnLoadout.startRuneCount"),
        "respawnDelaySec": respawn_delay,
        "respawnDelaySecByLevel": respawn_by_level,
    }


def normalize_spawn_object_root(root: dict) -> dict:
    root = require_object(root, "spawnObject")

    structure = require_object(root.get("structure", {}), "structure")
    turret_ai = require_object(structure.get("turretAI", {}), "structure.turretAI")
    default_jungle_camp = normalize_float_fields(
        require_object(root.get("defaultJungleCamp", {}), "defaultJungleCamp"),
        JUNGLE_FIELDS,
        "defaultJungleCamp",
    )
    if default_jungle_camp["respawnDelaySec"] <= 0.0:
        fail("defaultJungleCamp.respawnDelaySec must be > 0")

    jungle_camps = []
    seen_jungle = set()
    for index, item in enumerate(require_array(root.get("jungleCamps", []), "jungleCamps")):
        item = require_object(item, f"jungleCamps[{index}]")
        sub_kind = legacy.as_int(item.get("subKind", 0), f"jungleCamps[{index}].subKind")
        if sub_kind in seen_jungle:
            fail(f"duplicated jungle subKind: {sub_kind}")
        seen_jungle.add(sub_kind)
        camp = {
            "subKind": sub_kind,
            **normalize_float_fields(item, JUNGLE_FIELDS, f"jungleCamps[{index}]"),
        }
        if camp["respawnDelaySec"] <= 0.0:
            fail(f"jungleCamps[{index}].respawnDelaySec must be > 0")
        jungle_camps.append(camp)

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

    behavior_source = require_object(root.get("minionBehavior", {}), "minionBehavior")
    minion_behavior = normalize_float_fields(
        behavior_source, MINION_BEHAVIOR_FLOAT_FIELDS, "minionBehavior")
    for field, value in minion_behavior.items():
        if value < 0.0:
            fail(f"minionBehavior.{field} must be non-negative")
    minion_behavior.update({
        "pathBuildBudgetPerTick": legacy.as_int(
            behavior_source.get("pathBuildBudgetPerTick"), "minionBehavior.pathBuildBudgetPerTick"),
        "blockedFramesBeforeRepath": legacy.as_int(
            behavior_source.get("blockedFramesBeforeRepath"), "minionBehavior.blockedFramesBeforeRepath"),
        "flowFieldStallFramesBeforePathFallback": legacy.as_int(
            behavior_source.get("flowFieldStallFramesBeforePathFallback"),
            "minionBehavior.flowFieldStallFramesBeforePathFallback"),
        "targetScanStaggerBuckets": legacy.as_int(
            behavior_source.get("targetScanStaggerBuckets"), "minionBehavior.targetScanStaggerBuckets"),
        "rangedRoleType": legacy.as_int(
            behavior_source.get("rangedRoleType"), "minionBehavior.rangedRoleType"),
    })
    if minion_behavior["pathBuildBudgetPerTick"] < 1 or minion_behavior["targetScanStaggerBuckets"] < 1:
        fail("minionBehavior budgets and stagger buckets must be >= 1")
    for field in ("blockedFramesBeforeRepath", "flowFieldStallFramesBeforePathFallback", "rangedRoleType"):
        if minion_behavior[field] < 0 or minion_behavior[field] > 255:
            fail(f"minionBehavior.{field} must be in [0, 255]")

    minion_wave_source = require_object(root.get("minionWave", {}), "minionWave")
    formation_slots = []
    for index, raw_slot in enumerate(require_array(minion_wave_source.get("formationSlots", []), "minionWave.formationSlots")):
        slot = require_object(raw_slot, f"minionWave.formationSlots[{index}]")
        role_type = legacy.as_int(slot.get("roleType"), f"minionWave.formationSlots[{index}].roleType")
        if role_type < 0 or role_type > 255:
            fail(f"minionWave.formationSlots[{index}].roleType must be in [0, 255]")
        formation_slots.append({
            "roleType": role_type,
            "forwardOffset": legacy.as_float(slot.get("forwardOffset"), f"minionWave.formationSlots[{index}].forwardOffset"),
            "sideOffset": legacy.as_float(slot.get("sideOffset"), f"minionWave.formationSlots[{index}].sideOffset"),
        })
    if not formation_slots or len(formation_slots) > 6:
        fail("minionWave.formationSlots must contain 1..6 entries")
    siege_source = require_object(minion_wave_source.get("siegeSlot"), "minionWave.siegeSlot")
    siege_role = legacy.as_int(siege_source.get("roleType"), "minionWave.siegeSlot.roleType")
    if siege_role < 0 or siege_role > 255:
        fail("minionWave.siegeSlot.roleType must be in [0, 255]")
    minion_wave = {
        "waveIntervalTicks": legacy.as_int(
            minion_wave_source.get("waveIntervalTicks", 900), "minionWave.waveIntervalTicks"),
        "initialDelayTicks": legacy.as_int(
            minion_wave_source.get("initialDelayTicks", 300), "minionWave.initialDelayTicks"),
        "perMinionDelayTicks": legacy.as_int(
            minion_wave_source.get("perMinionDelayTicks", 10), "minionWave.perMinionDelayTicks"),
        "siegeWavePeriod": legacy.as_int(
            minion_wave_source.get("siegeWavePeriod", 3), "minionWave.siegeWavePeriod"),
        "timeGrowthPerMinute": legacy.as_float(
            minion_wave_source.get("timeGrowthPerMinute", 0.025), "minionWave.timeGrowthPerMinute"),
        "timeGrowthCapMinutes": legacy.as_int(
            minion_wave_source.get("timeGrowthCapMinutes", 30), "minionWave.timeGrowthCapMinutes"),
        "rangedProjectile": normalize_float_fields(
            require_object(minion_wave_source.get("rangedProjectile", {}), "minionWave.rangedProjectile"),
            MINION_WAVE_RANGED_PROJECTILE_FIELDS,
            "minionWave.rangedProjectile",
        ),
        "corpseDeathTimerSec": legacy.as_float(
            minion_wave_source.get("corpseDeathTimerSec", 1.5), "minionWave.corpseDeathTimerSec"),
        "startX": legacy.as_float(minion_wave_source.get("startX"), "minionWave.startX"),
        "formationSlots": formation_slots,
        "siegeSlot": {
            "roleType": siege_role,
            "forwardOffset": legacy.as_float(siege_source.get("forwardOffset"), "minionWave.siegeSlot.forwardOffset"),
            "sideOffset": legacy.as_float(siege_source.get("sideOffset"), "minionWave.siegeSlot.sideOffset"),
        },
    }
    if minion_wave["waveIntervalTicks"] < 1:
        fail("minionWave.waveIntervalTicks must be >= 1")
    if minion_wave["siegeWavePeriod"] < 1:
        fail("minionWave.siegeWavePeriod must be >= 1")

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
        "defaultJungleCamp": default_jungle_camp,
        "jungleCamps": sorted(jungle_camps, key=lambda item: item["subKind"]),
        "defaultMinion": normalize_float_fields(
            require_object(root.get("defaultMinion", {}), "defaultMinion"),
            MINION_FIELDS,
            "defaultMinion",
        ),
        "minions": sorted(minions, key=lambda item: item["roleType"]),
        "minionBehavior": minion_behavior,
        "minionWave": minion_wave,
    }


def validate_economy_number(value: float, path: str) -> float:
    if value < 0.0 or value > 1_000_000.0:
        fail(f"{path} must be in [0, 1000000]")
    return value


def normalize_economy_root(root: dict) -> dict:
    root = require_object(root, "economy")

    curve_source = require_array(root.get("xpCurve", []), "xpCurve")
    if len(curve_source) != ECONOMY_XP_CURVE_LENGTH:
        fail(f"xpCurve must have {ECONOMY_XP_CURVE_LENGTH} entries (levels 1..17)")
    xp_curve = [
        validate_economy_number(legacy.as_float(value, f"xpCurve[{index}]"), f"xpCurve[{index}]")
        for index, value in enumerate(curve_source)
    ]

    def normalized_group(name: str, fields: tuple[tuple[str, float], ...]) -> dict:
        record = normalize_float_fields(require_object(root.get(name, {}), name), fields, name)
        for key, value in record.items():
            validate_economy_number(value, f"{name}.{key}")
        return record

    minions_source = require_object(root.get("minions", {}), "minions")
    minions = {}
    for kind in ECONOMY_MINION_KINDS:
        record = normalize_float_fields(
            require_object(minions_source.get(kind, {}), f"minions.{kind}"),
            ECONOMY_MINION_FIELDS,
            f"minions.{kind}",
        )
        for key, value in record.items():
            validate_economy_number(value, f"minions.{kind}.{key}")
        minions[kind] = record

    passive_source = require_object(root.get("passiveGold", {}), "passiveGold")
    passive_gold = {
        "startTick": legacy.as_int(passive_source.get("startTick", 0), "passiveGold.startTick"),
        "intervalTicks": legacy.as_int(passive_source.get("intervalTicks", 1), "passiveGold.intervalTicks"),
        "perGrant": legacy.as_int(passive_source.get("perGrant", 0), "passiveGold.perGrant"),
    }
    for key, value in passive_gold.items():
        validate_economy_number(float(value), f"passiveGold.{key}")
    if passive_gold["intervalTicks"] < 1:
        fail("passiveGold.intervalTicks must be >= 1")

    objectives = normalized_group("objectives", ECONOMY_OBJECTIVE_FLOAT_FIELDS)
    objective_source = require_object(root.get("objectives", {}), "objectives")
    objectives["teamLevelGrant"] = legacy.as_int(
        objective_source.get("teamLevelGrant", 3), "objectives.teamLevelGrant"
    )
    if objectives["teamLevelGrant"] < 0 or objectives["teamLevelGrant"] > 18:
        fail("objectives.teamLevelGrant must be in [0, 18]")

    return {
        "schemaVersion": legacy.as_int(root.get("schemaVersion", 1), "economy.schemaVersion"),
        "dataVersion": legacy.as_int(root.get("dataVersion", 1), "economy.dataVersion"),
        "xpCurve": xp_curve,
        "championKill": normalized_group("championKill", ECONOMY_CHAMPION_KILL_FIELDS),
        "minions": minions,
        "turretGold": validate_economy_number(
            legacy.as_float(root.get("turretGold", 0.0), "turretGold"), "turretGold"
        ),
        "turretTeamGold": validate_economy_number(
            legacy.as_float(root.get("turretTeamGold", 0.0), "turretTeamGold"),
            "turretTeamGold",
        ),
        "jungle": normalized_group("jungle", ECONOMY_JUNGLE_FIELDS),
        "objectives": objectives,
        "passiveGold": passive_gold,
        "timers": normalized_group("timers", ECONOMY_TIMER_FIELDS),
    }


def validate_item_stat_number(value: float, path: str) -> float:
    if value < -1_000_000.0 or value > 1_000_000.0:
        fail(f"{path} must be in [-1000000, 1000000]")
    return value


def _resolve_local_schema_ref(root_schema: dict, reference: str) -> dict:
    if not reference.startswith("#/"):
        fail(f"unsupported non-local schema reference: {reference}")
    current: object = root_schema
    for raw_token in reference[2:].split("/"):
        token = raw_token.replace("~1", "/").replace("~0", "~")
        if not isinstance(current, dict) or token not in current:
            fail(f"unresolved schema reference: {reference}")
        current = current[token]
    if not isinstance(current, dict):
        fail(f"schema reference is not an object: {reference}")
    return current


def _schema_type_matches(value: object, expected: str) -> bool:
    if expected == "object":
        return isinstance(value, dict)
    if expected == "array":
        return isinstance(value, list)
    if expected == "integer":
        return isinstance(value, int) and not isinstance(value, bool)
    if expected == "number":
        return isinstance(value, (int, float)) and not isinstance(value, bool)
    if expected == "string":
        return isinstance(value, str)
    if expected == "boolean":
        return isinstance(value, bool)
    fail(f"unsupported schema type: {expected}")


def _validate_schema_value(
        rule: dict,
        root_schema: dict,
        value: object,
        path: str,
        errors: list[str]) -> None:
    if "$ref" in rule:
        _validate_schema_value(
            _resolve_local_schema_ref(root_schema, rule["$ref"]),
            root_schema,
            value,
            path,
            errors)

    expected = rule.get("type")
    if expected is not None and not _schema_type_matches(value, expected):
        errors.append(f"{path} must be {expected}")
        return

    if "const" in rule and value != rule["const"]:
        errors.append(f"{path} must equal {rule['const']!r}")
    if "enum" in rule and value not in rule["enum"]:
        errors.append(f"{path} must be one of {rule['enum']!r}")

    if isinstance(value, dict):
        properties = rule.get("properties", {})
        for key in rule.get("required", []):
            if key not in value:
                errors.append(f"{path} missing required field: {key}")
        for key, child_value in value.items():
            child_rule = properties.get(key)
            if child_rule is not None:
                _validate_schema_value(
                    child_rule, root_schema, child_value, f"{path}.{key}", errors)
            elif rule.get("additionalProperties") is False:
                errors.append(f"{path} unknown field: {key}")
            elif isinstance(rule.get("additionalProperties"), dict):
                _validate_schema_value(
                    rule["additionalProperties"],
                    root_schema,
                    child_value,
                    f"{path}.{key}",
                    errors)

    if isinstance(value, list):
        if len(value) < rule.get("minItems", 0):
            errors.append(f"{path} has too few entries")
        if "maxItems" in rule and len(value) > rule["maxItems"]:
            errors.append(f"{path} has too many entries")
        item_rule = rule.get("items")
        if isinstance(item_rule, dict):
            for index, item in enumerate(value):
                _validate_schema_value(
                    item_rule, root_schema, item, f"{path}[{index}]", errors)

    if isinstance(value, (int, float)) and not isinstance(value, bool):
        if "minimum" in rule and value < rule["minimum"]:
            errors.append(f"{path} is below minimum {rule['minimum']}")
        if "maximum" in rule and value > rule["maximum"]:
            errors.append(f"{path} is above maximum {rule['maximum']}")
        if "exclusiveMinimum" in rule and value <= rule["exclusiveMinimum"]:
            errors.append(
                f"{path} must be greater than {rule['exclusiveMinimum']}")

    if isinstance(value, str) and len(value) < rule.get("minLength", 0):
        errors.append(f"{path} is shorter than minLength {rule['minLength']}")

    for child_rule in rule.get("allOf", []):
        _validate_schema_value(child_rule, root_schema, value, path, errors)

    if "anyOf" in rule:
        branch_errors = []
        for child_rule in rule["anyOf"]:
            candidate_errors: list[str] = []
            _validate_schema_value(
                child_rule, root_schema, value, path, candidate_errors)
            branch_errors.append(candidate_errors)
        if all(candidate_errors for candidate_errors in branch_errors):
            errors.append(f"{path} does not satisfy anyOf")

    if "if" in rule:
        condition_errors: list[str] = []
        _validate_schema_value(
            rule["if"], root_schema, value, path, condition_errors)
        selected_rule = rule.get("then") if not condition_errors else rule.get("else")
        if isinstance(selected_rule, dict):
            _validate_schema_value(
                selected_rule, root_schema, value, path, errors)


def validate_source_schema(workspace_root: Path, source: Path, value: object) -> None:
    schema_path = workspace_root / "Data" / "LoL" / "Schemas" / (source.name + ".schema.json")
    if not schema_path.exists():
        fail(f"missing schema: {schema_path.relative_to(workspace_root)}")
    schema = json.loads(schema_path.read_text(encoding="utf-8"))
    if schema.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
        fail(f"unsupported schema declaration: {schema_path.name}")

    errors: list[str] = []
    _validate_schema_value(schema, schema, value, source.name, errors)
    if errors:
        fail(f"schema validation failed: {errors[0]}")


def normalize_damage_formula(source: object, path: str) -> dict | None:
    if source is None:
        return None
    source = require_object(source, path)
    damage_type = source.get("type", "Physical")
    if damage_type not in DAMAGE_TYPES:
        fail(f"{path}.type must be one of {sorted(DAMAGE_TYPES)}")

    flags_source = require_array(source.get("flags", []), f"{path}.flags")
    flags = []
    for index, flag in enumerate(flags_source):
        if flag not in DAMAGE_FLAGS:
            fail(f"{path}.flags[{index}] must be one of {sorted(DAMAGE_FLAGS)}")
        if flag in flags:
            fail(f"{path}.flags contains duplicate {flag}")
        flags.append(flag)

    rank_count = 0
    normalized: dict[str, object] = {
        "type": damage_type,
        "cppType": DAMAGE_TYPES[damage_type],
        "flags": flags,
        "cppFlags": [DAMAGE_FLAGS[flag] for flag in flags],
    }
    for field in DAMAGE_RANK_FIELDS:
        values_source = require_array(source.get(field, []), f"{path}.{field}")
        if values_source:
            if rank_count == 0:
                rank_count = len(values_source)
            elif len(values_source) != rank_count:
                fail(f"{path}.{field} rank count mismatch")
        normalized[field] = [
            legacy.as_float(value, f"{path}.{field}[{index}]")
            for index, value in enumerate(values_source)
        ]

    if rank_count < 1 or rank_count > 5:
        fail(f"{path} must define 1..5 ranked values")
    for field in DAMAGE_RANK_FIELDS:
        if not normalized[field]:
            normalized[field] = [0.0] * rank_count
    normalized["rankCount"] = rank_count
    return normalized


def normalize_items_root(root: dict) -> dict:
    root = require_object(root, "items")

    records = []
    seen_ids = set()
    for index, item in enumerate(require_array(root.get("items", []), "items")):
        item = require_object(item, f"items[{index}]")

        item_id = legacy.as_int(item.get("itemId", 0), f"items[{index}].itemId")
        if item_id < 1 or item_id > 65535:
            fail(f"items[{index}].itemId must be in [1, 65535] (u16 wire type)")
        if item_id in seen_ids:
            fail(f"duplicated itemId: {item_id}")
        seen_ids.add(item_id)

        price = legacy.as_int(item.get("price", 0), f"items[{index}].price")
        if price < 0 or price > 65535:
            fail(f"items[{index}].price must be in [0, 65535] (u16 wire type)")

        name = item.get("name")
        if not isinstance(name, str) or not name:
            fail(f"items[{index}].name must be a non-empty string")

        stats_source = require_object(item.get("stats", {}), f"items[{index}].stats")
        for key in stats_source:
            if key not in ITEM_STAT_FIELDS:
                fail(f"items[{index}].stats unknown field: {key}")
        stats = {
            key: validate_item_stat_number(
                legacy.as_float(stats_source[key], f"items[{index}].stats.{key}"),
                f"items[{index}].stats.{key}",
            )
            for key in ITEM_STAT_FIELDS
            if key in stats_source
        }

        on_hit_damage = normalize_damage_formula(
            item.get("onHitDamage"),
            f"items[{index}].onHitDamage",
        )

        purchasable = item.get("purchasable", True)
        if not isinstance(purchasable, bool):
            fail(f"items[{index}].purchasable must be boolean")

        spellblade = None
        if item.get("spellblade") is not None:
            source = require_object(item["spellblade"], f"items[{index}].spellblade")
            spellblade = {
                "cooldownSec": validate_item_stat_number(
                    legacy.as_float(source.get("cooldownSec", 0.0),
                                    f"items[{index}].spellblade.cooldownSec"),
                    f"items[{index}].spellblade.cooldownSec"),
                "baseAdRatio": validate_item_stat_number(
                    legacy.as_float(source.get("baseAdRatio", 0.0),
                                    f"items[{index}].spellblade.baseAdRatio"),
                    f"items[{index}].spellblade.baseAdRatio"),
                "critChanceFlatScale": validate_item_stat_number(
                    legacy.as_float(source.get("critChanceFlatScale", 0.0),
                                    f"items[{index}].spellblade.critChanceFlatScale"),
                    f"items[{index}].spellblade.critChanceFlatScale"),
                "manaRestoreRatio": validate_item_stat_number(
                    legacy.as_float(source.get("manaRestoreRatio", 0.0),
                                    f"items[{index}].spellblade.manaRestoreRatio"),
                    f"items[{index}].spellblade.manaRestoreRatio"),
            }
            if any(value < 0.0 for value in spellblade.values()):
                fail(f"items[{index}].spellblade values must be non-negative")

        manaflow = None
        if item.get("manaflow") is not None:
            source = require_object(item["manaflow"], f"items[{index}].manaflow")
            manaflow = {
                "rechargeSec": legacy.as_float(
                    source.get("rechargeSec", 0.0),
                    f"items[{index}].manaflow.rechargeSec"),
                "maxCharges": legacy.as_int(
                    source.get("maxCharges", 0),
                    f"items[{index}].manaflow.maxCharges"),
                "manaPerTrigger": legacy.as_int(
                    source.get("manaPerTrigger", 0),
                    f"items[{index}].manaflow.manaPerTrigger"),
                "championMultiplier": legacy.as_int(
                    source.get("championMultiplier", 1),
                    f"items[{index}].manaflow.championMultiplier"),
                "maxBonusMana": legacy.as_int(
                    source.get("maxBonusMana", 0),
                    f"items[{index}].manaflow.maxBonusMana"),
                "transformItemId": legacy.as_int(
                    source.get("transformItemId", 0),
                    f"items[{index}].manaflow.transformItemId"),
            }
            if manaflow["rechargeSec"] <= 0.0:
                fail(f"items[{index}].manaflow.rechargeSec must be positive")
            if manaflow["maxCharges"] < 1 or manaflow["maxCharges"] > 255:
                fail(f"items[{index}].manaflow.maxCharges must be in [1, 255]")
            if manaflow["championMultiplier"] < 1 or manaflow["championMultiplier"] > 255:
                fail(f"items[{index}].manaflow.championMultiplier must be in [1, 255]")
            for field in ("manaPerTrigger", "maxBonusMana", "transformItemId"):
                if manaflow[field] < 1 or manaflow[field] > 65535:
                    fail(f"items[{index}].manaflow.{field} must be in [1, 65535]")

        lightshield_strike = None
        if item.get("lightshieldStrike") is not None:
            source = require_object(
                item["lightshieldStrike"],
                f"items[{index}].lightshieldStrike")
            lightshield_strike = {
                field: validate_item_stat_number(
                    legacy.as_float(
                        source.get(field, 0.0),
                        f"items[{index}].lightshieldStrike.{field}"),
                    f"items[{index}].lightshieldStrike.{field}")
                for field in (
                    "cooldownSec",
                    "critDamageMultiplier",
                    "healBaseAdRatio",
                    "healMissingHealthRatio",
                )
            }
            if lightshield_strike["cooldownSec"] <= 0.0:
                fail(f"items[{index}].lightshieldStrike.cooldownSec must be positive")
            if lightshield_strike["critDamageMultiplier"] < 1.0:
                fail(f"items[{index}].lightshieldStrike.critDamageMultiplier must be >= 1")
            if lightshield_strike["healBaseAdRatio"] < 0.0 or \
                    lightshield_strike["healMissingHealthRatio"] < 0.0:
                fail(f"items[{index}].lightshieldStrike heal ratios must be non-negative")

        active = None
        if item.get("active") is not None:
            source = require_object(item["active"], f"items[{index}].active")
            kind = source.get("kind")
            if kind not in ITEM_ACTIVE_KIND_MAP:
                fail(f"items[{index}].active.kind must be one of {sorted(ITEM_ACTIVE_KIND_MAP)}")
            active = {
                "kind": kind,
                "cooldownSec": validate_item_stat_number(
                    legacy.as_float(
                        source.get("cooldownSec", 0.0),
                        f"items[{index}].active.cooldownSec"),
                    f"items[{index}].active.cooldownSec"),
                "durationSec": validate_item_stat_number(
                    legacy.as_float(
                        source.get("durationSec", 0.0),
                        f"items[{index}].active.durationSec"),
                    f"items[{index}].active.durationSec"),
            }
            if active["cooldownSec"] < 0.0 or active["durationSec"] < 0.0:
                fail(f"items[{index}].active cooldown/duration must be non-negative")
            if kind == "Stasis" and active["durationSec"] <= 0.0:
                fail(f"items[{index}].active Stasis durationSec must be positive")

        max_mana_bonus_ad_ratio = validate_item_stat_number(
            legacy.as_float(
                item.get("maxManaBonusAdRatio", 0.0),
                f"items[{index}].maxManaBonusAdRatio"),
            f"items[{index}].maxManaBonusAdRatio")
        if max_mana_bonus_ad_ratio < 0.0:
            fail(f"items[{index}].maxManaBonusAdRatio must be non-negative")

        records.append(
            {
                "itemId": item_id,
                "price": price,
                "purchasable": purchasable,
                "name": name,
                "stats": stats,
                "onHitDamage": on_hit_damage,
                "spellblade": spellblade,
                "manaflow": manaflow,
                "lightshieldStrike": lightshield_strike,
                "active": active,
                "maxManaBonusAdRatio": max_mana_bonus_ad_ratio,
            }
        )

    if not records:
        fail("items[] must not be empty")

    return {
        "schemaVersion": legacy.as_int(root.get("schemaVersion", 1), "items.schemaVersion"),
        "dataVersion": legacy.as_int(root.get("dataVersion", 1), "items.dataVersion"),
        "sourcePatch": str(root.get("sourcePatch", "")),
        "sourceMapId": legacy.as_int(root.get("sourceMapId", 0), "items.sourceMapId"),
        "items": sorted(records, key=lambda record: record["itemId"]),
    }


def normalize_rune_root(root: dict) -> dict:
    records = []
    seen_keys: set[str] = set()
    seen_ids: set[int] = set()
    for index, value in enumerate(require_array(root.get("runes", []), "runes")):
        item = require_object(value, f"runes[{index}]")
        key = item.get("key")
        if not isinstance(key, str) or not key:
            fail(f"runes[{index}].key must be a non-empty string")
        legacy_id = legacy.as_int(item.get("legacyRuneId"), f"runes[{index}].legacyRuneId")
        if not key.startswith("rune."):
            fail(f"runes[{index}].key must start with rune.")
        if key in seen_keys or legacy_id in seen_ids:
            fail(f"duplicate rune key/id: {key}/{legacy_id}")
        if legacy_id <= 0 or legacy_id > 255:
            fail(f"runes[{index}].legacyRuneId must be in [1, 255]")
        max_stacks = legacy.as_int(item.get("maxStacks", 0), f"runes[{index}].maxStacks")
        if max_stacks < 0 or max_stacks > 255:
            fail(f"runes[{index}].maxStacks must be in [0, 255]")
        seen_keys.add(key)
        seen_ids.add(legacy_id)
        enabled = item.get("enabled", False)
        if not isinstance(enabled, bool):
            fail(f"runes[{index}].enabled must be a bool")
        records.append(
            {
                "key": key,
                "definitionKey": definition_key(key),
                "legacyRuneId": legacy_id,
                "enabled": enabled,
                "maxStacks": max_stacks,
            }
        )
    if not records:
        fail("runes must not be empty")
    return {
        "schemaVersion": legacy.as_int(root.get("schemaVersion", 1), "runes.schemaVersion"),
        "dataVersion": legacy.as_int(root.get("dataVersion", 1), "runes.dataVersion"),
        "runes": sorted(records, key=lambda record: record["legacyRuneId"]),
    }


def normalize_ai_profile(value: object, path: str, valid_champions: set[str], allow_default: bool) -> dict:
    item = require_object(value, path)
    champion = item.get("champion")
    allowed = {"END"} if allow_default else valid_champions
    if champion not in allowed:
        fail(f"{path}.champion must be one of {sorted(allowed)}")
    result = {"champion": champion}
    for field in AI_PROFILE_FLOAT_FIELDS:
        number = legacy.as_float(item.get(field), f"{path}.{field}")
        if number < 0.0 or number > 1_000_000.0:
            fail(f"{path}.{field} must be in [0, 1000000]")
        result[field] = number
    for field in ("retreatHpRatio", "reengageHpRatio"):
        if result[field] > 1.0:
            fail(f"{path}.{field} must be in [0, 1]")
    rules = []
    for index, raw_rule in enumerate(require_array(item.get("skillRules", []), f"{path}.skillRules")):
        rule = require_object(raw_rule, f"{path}.skillRules[{index}]")
        slot = rule.get("slot")
        if slot not in AI_SLOT_MAP:
            fail(f"{path}.skillRules[{index}].slot must be one of {sorted(AI_SLOT_MAP)}")
        min_range = legacy.as_float(rule.get("minRange"), f"{path}.skillRules[{index}].minRange")
        score = legacy.as_float(rule.get("score"), f"{path}.skillRules[{index}].score")
        if min_range < 0.0 or score < 0.0:
            fail(f"{path}.skillRules[{index}] values must be non-negative")
        rules.append({"slot": slot, "minRange": min_range, "score": score})
    if len(rules) > 4:
        fail(f"{path}.skillRules supports at most 4 entries")
    result["skillRules"] = rules
    return result


def normalize_ai_root(root: dict, valid_champions: set[str]) -> dict:
    root = require_object(root, "championAI")
    default_profile = normalize_ai_profile(root.get("defaultProfile"), "defaultProfile", valid_champions, True)
    profiles = []
    seen = set()
    for index, value in enumerate(require_array(root.get("profiles", []), "profiles")):
        profile = normalize_ai_profile(value, f"profiles[{index}]", valid_champions, False)
        champion = profile["champion"]
        if champion in seen:
            fail(f"duplicate AI profile: {champion}")
        seen.add(champion)
        profiles.append(profile)
    if seen != valid_champions:
        fail(f"AI profile coverage mismatch; missing={sorted(valid_champions - seen)}, extra={sorted(seen - valid_champions)}")

    combos = []
    combo_champions = set()
    for index, raw_plan in enumerate(require_array(root.get("comboPlans", []), "comboPlans")):
        plan = require_object(raw_plan, f"comboPlans[{index}]")
        champion = plan.get("champion")
        if champion != "END" and champion not in valid_champions:
            fail(f"comboPlans[{index}].champion is invalid: {champion}")
        if champion in combo_champions:
            fail(f"duplicate AI combo plan: {champion}")
        combo_champions.add(champion)
        steps = []
        for step_index, raw_step in enumerate(require_array(plan.get("steps", []), f"comboPlans[{index}].steps")):
            step = require_object(raw_step, f"comboPlans[{index}].steps[{step_index}]")
            slot = step.get("slot")
            target_mode = step.get("targetMode", "TargetEntity")
            if slot not in AI_SLOT_MAP:
                fail(f"comboPlans[{index}].steps[{step_index}].slot is invalid")
            if target_mode not in AI_TARGET_MODES:
                fail(f"comboPlans[{index}].steps[{step_index}].targetMode is invalid")
            normalized_step = {
                "slot": slot,
                "itemId": legacy.as_int(step.get("itemId", 0), f"comboPlans[{index}].steps[{step_index}].itemId"),
                "minRange": legacy.as_float(step.get("minRange"), f"comboPlans[{index}].steps[{step_index}].minRange"),
                "maxRange": legacy.as_float(step.get("maxRange"), f"comboPlans[{index}].steps[{step_index}].maxRange"),
                "selfHpMinRatio": legacy.as_float(step.get("selfHpMinRatio"), f"comboPlans[{index}].steps[{step_index}].selfHpMinRatio"),
                "selfHpMaxRatio": legacy.as_float(step.get("selfHpMaxRatio", 1.0), f"comboPlans[{index}].steps[{step_index}].selfHpMaxRatio"),
                "enemyHpMaxRatio": legacy.as_float(step.get("enemyHpMaxRatio"), f"comboPlans[{index}].steps[{step_index}].enemyHpMaxRatio"),
                "targetMode": target_mode,
            }
            if normalized_step["itemId"] < 0 or normalized_step["itemId"] > 65535:
                fail(f"comboPlans[{index}].steps[{step_index}].itemId must be in [0, 65535]")
            if normalized_step["minRange"] < 0.0 or normalized_step["maxRange"] < normalized_step["minRange"]:
                fail(f"comboPlans[{index}].steps[{step_index}] range is invalid")
            if (not 0.0 <= normalized_step["selfHpMinRatio"] <= normalized_step["selfHpMaxRatio"] <= 1.0 or
                    not 0.0 <= normalized_step["enemyHpMaxRatio"] <= 1.0):
                fail(f"comboPlans[{index}].steps[{step_index}] HP ratios must be in [0, 1]")
            steps.append(normalized_step)
        if len(steps) > 10:
            fail(f"comboPlans[{index}] supports at most 10 steps")
        combos.append({"champion": champion, "steps": steps})
    if "END" not in combo_champions:
        fail("comboPlans must contain the END default plan")
    return {
        "schemaVersion": legacy.as_int(root.get("schemaVersion", 1), "championAI.schemaVersion"),
        "dataVersion": legacy.as_int(root.get("dataVersion", 1), "championAI.dataVersion"),
        "defaultProfile": default_profile,
        "profiles": sorted(profiles, key=lambda item: item["champion"]),
        "comboPlans": combos,
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
            visible_when_alive = state.get("visibleWhenAlive", not visible_when_destroyed)
            if not isinstance(visible_when_alive, bool):
                fail(
                    f"structures[{index}].visibilityStates[{state_index}].visibleWhenAlive "
                    "must be a bool"
                )
            states.append(
                {
                    "name": str(state.get("name", "")),
                    "submeshIndex": submesh_index,
                    "visibleWhenDestroyed": visible_when_destroyed,
                    "visibleWhenAlive": visible_when_alive,
                }
            )

        # S035: kVisualSubmeshStateCount(LoLVisualDefinitionPack.h)와 정렬 — 포탑 7상태 수용.
        if len(states) > 8:
            fail(f"structures[{index}].visibilityStates has too many entries: {len(states)} > 8")

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

        visual_scale_multiplier = legacy.as_float(
            item.get("visualScaleMultiplier", 1.0),
            f"jungles[{index}].visualScaleMultiplier",
        )
        if visual_scale_multiplier <= 0.0:
            fail(f"jungles[{index}].visualScaleMultiplier must be greater than zero")

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

        jungle_record = {
            "key": key,
            "subKind": sub_kind,
            "mesh": mesh.replace("\\", "/"),
            "shader": shader.replace("\\", "/"),
        }
        if visual_scale_multiplier != 1.0:
            jungle_record["visualScaleMultiplier"] = visual_scale_multiplier
        jungle_record["textureOverrides"] = texture_overrides
        jungles.append(jungle_record)

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

        visual_scale_multiplier = legacy.as_float(
            item.get("visualScaleMultiplier", 1.0),
            f"minions[{index}].visualScaleMultiplier")
        if visual_scale_multiplier <= 0.0:
            fail(f"minions[{index}].visualScaleMultiplier must be > 0")
        record = {
            "key": key,
            "type": minion_type,
            "team": team,
            "mesh": mesh.replace("\\", "/"),
            "shader": shader.replace("\\", "/"),
            "textureAllMeshes": texture_all.replace("\\", "/"),
        }
        if visual_scale_multiplier != 1.0:
            record["visualScaleMultiplier"] = visual_scale_multiplier
        minions.append(record)

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
    required_variant_params = {
        "skill.yasuo.q": {"tornadoDamage", "dashAreaDamage"},
        "skill.kalista.e": {"damagePerSpear"},
        "skill.leesin.q": {"baseDamage"},
        "skill.ezreal.r": {"nonEpicBaseDamage"},
    }

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

        if "damage" not in item:
            fail(f"skillEffects[{index}].damage is required")
        damage = normalize_damage_formula(
            item["damage"],
            f"skillEffects[{index}].damage",
        )

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
            param_path = f"skillEffects[{index}].params.{param_name}"
            raw_value = params_source[param_name]
            if (key, param_name) in RANKED_DAMAGE_PARAM_KEYS:
                values_source = require_array(raw_value, param_path)
                if len(values_source) != damage["rankCount"]:
                    fail(
                        f"{param_path} must contain exactly "
                        f"{damage['rankCount']} ranked values"
                    )
                values = [
                    validate_skill_effect_param_domain(
                        param_name,
                        legacy.as_float(value, f"{param_path}[{rank_index}]"),
                        f"{param_path}[{rank_index}]",
                    )
                    for rank_index, value in enumerate(values_source)
                ]
            else:
                values = [
                    validate_skill_effect_param_domain(
                        param_name,
                        legacy.as_float(raw_value, param_path),
                        param_path,
                    )
                ]
            params.append(
                {
                    "id": param_name,
                    "cppId": SKILL_EFFECT_PARAM_IDS[param_name],
                    "values": values,
                }
            )

        if len(params) > SKILL_EFFECT_PARAM_MAX:
            fail(f"skillEffects[{index}] has too many params: {len(params)} > {SKILL_EFFECT_PARAM_MAX}")

        missing_variant_params = required_variant_params.get(key, set()) - seen_params
        if missing_variant_params:
            fail(
                f"skillEffects[{index}] is missing required damage variant params: "
                f"{sorted(missing_variant_params)}"
            )

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
            param_path = f"skillEffects[{index}].summonPolicy.{param_name}"
            value = legacy.as_float(summon_source[param_name], param_path)
            summon_params.append(
                {
                    "id": param_name,
                    "cppId": SUMMON_POLICY_PARAM_IDS[param_name],
                    "value": validate_summon_policy_param_domain(
                        param_name,
                        value,
                        param_path,
                    ),
                }
            )

        if len(summon_params) > SUMMON_POLICY_PARAM_MAX:
            fail(
                f"skillEffects[{index}] has too many summon policy params: "
                f"{len(summon_params)} > {SUMMON_POLICY_PARAM_MAX}"
            )

        records.append(
            {
                "key": key,
                "params": params,
                "summonPolicyParams": summon_params,
                "damage": damage,
            }
        )

    missing_keys = sorted(valid_skill_keys - seen_keys)
    if missing_keys:
        fail(f"missing skill effect definitions: {missing_keys}")

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
        skill["damageFormula"] = record.get("damage")


def compute_definition_pack_hash(
    data: dict,
    summoner_spell_data: dict,
    spawn_object_data: dict,
    skill_effect_data: dict,
    economy_data: dict,
    item_data: dict,
    rune_data: dict,
    ai_data: dict,
    champion_visual_data: dict,
    object_visual_data: dict,
    champion_asset_visual_data: dict) -> int:
    stable = json.dumps(
        {
            "championGameplay": data,
            "summonerSpellGameplay": summoner_spell_data,
            "spawnObjectGameplay": spawn_object_data,
            "skillEffectGameplay": skill_effect_data,
            "economyGameplay": economy_data,
            "itemGameplay": item_data,
            "runeGameplay": rune_data,
            "championAI": ai_data,
            "championVisual": champion_visual_data,
            "objectVisual": object_visual_data,
            "championAssetVisual": champion_asset_visual_data,
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
            target_shapes = []
            target_policies = []
            for stage_index, stage in enumerate(skill["stages"]):
                target_mode = stage.get("targetMode", skill["targetMode"])
                shape, policy = TARGET_MAP.get(target_mode, (None, None))
                if shape is None:
                    fail(
                        f"unsupported targetMode {target_mode} in "
                        f"{skill_key}.stages[{stage_index}]"
                    )
                target_shapes.append(shape)
                target_policies.append(policy)
            resolve_policy = (
                "StageDependent"
                if len(set(target_shapes)) > 1
                else target_policies[0]
            )
            skills.append(
                {
                    **skill,
                    "ownerChampion": champion["champion"],
                    "ownerKey": key,
                    "canonicalKey": skill_key,
                    "definitionKey": definition_key(skill_key),
                    "targetShape": target_shapes[0],
                    "targetShapes": target_shapes,
                    "resolvePolicy": resolve_policy,
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
    target = {
        "shape": record["targetShape"],
        "resolvePolicy": record["resolvePolicy"],
    }
    if len(set(record["targetShapes"])) > 1:
        target["shapes"] = record["targetShapes"]

    result = {
        "key": record["canonicalKey"],
        "definitionKey": record["definitionKey"],
        "defId": record["defId"],
        "ownerChampionDefId": champion_ids[record["ownerKey"]],
        "slot": record["slot"],
        "input": {"activation": record["activationMode"]},
        "target": target,
        "cost": {"manaByRank": record["manaCostByRank"]},
        "cooldown": {"secondsByRank": record["cooldownSecByRank"]},
        "range": {"maximum": record["rangeMax"]},
        "stage": {
            "count": record["stageCount"],
            "windowSeconds": record["stageWindowSec"],
            "lockSeconds": [stage["lockDurationSec"] for stage in record["stages"]],
            "commandLockSeconds": [stage["commandLockSec"] for stage in record["stages"]],
            "movePolicies": [stage["movePolicy"] for stage in record["stages"]],
            "createsActionState": [stage["createsActionState"] for stage in record["stages"]],
            "presentationLoopWhileActive": [
                stage["presentationLoopWhileActive"] for stage in record["stages"]
            ],
        },
        "effect": {
            "scalingTableId": record["scalingTableId"],
            "gameplayPolicyId": record["gameplayPolicyId"],
            "replicatedCueId": record["visualCueId"],
            "params": [
                {
                    "id": param["id"],
                    ("value" if len(param["values"]) == 1 else "values"):
                        (param["values"][0]
                         if len(param["values"]) == 1
                         else param["values"]),
                }
                for param in record.get("effectParams", [])
            ],
        },
    }

    if record.get("damageFormula") is not None:
        result["effect"]["damage"] = damage_formula_json(record["damageFormula"])

    if record.get("charge") is not None:
        result["charge"] = record["charge"]

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
                stage_path = f"{skill_key}.stages[{stage_index}]"
                playback_speed = legacy.as_float(
                    stage.get("animationPlaybackSpeed", 1.0),
                    f"{stage_path}.animationPlaybackSpeed",
                )
                cast_frame = legacy.as_float(
                    stage.get("castFrame", 0.0),
                    f"{stage_path}.castFrame",
                )
                recovery_frame = legacy.as_float(
                    stage.get("recoveryFrame", 0.0),
                    f"{stage_path}.recoveryFrame",
                )
                if playback_speed <= 0.0:
                    fail(f"{stage_path}.animationPlaybackSpeed must be positive")
                if cast_frame < 0.0:
                    fail(f"{stage_path}.castFrame must be non-negative")
                if recovery_frame < cast_frame:
                    fail(f"{stage_path}.recoveryFrame must be >= castFrame")
                stages.append(
                    {
                        "animationPlaybackSpeed": playback_speed,
                        "castFrame": cast_frame,
                        "recoveryFrame": recovery_frame,
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

        missing_slots = set(range(len(SLOT_NAMES))) - seen_slots
        if missing_slots:
            fail(f"{key}.skills is missing slots: {sorted(missing_slots)}")

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
            param["id"]: (
                param["values"][0]
                if len(param["values"]) == 1
                else param["values"]
            )
            for param in record["params"]
        },
    }
    summon_policy_params = record.get("summonPolicyParams", [])
    if summon_policy_params:
        result["summonPolicy"] = {
            param["id"]: param["value"]
            for param in summon_policy_params
        }
    if record.get("damage") is not None:
        result["damage"] = damage_formula_json(record["damage"])
    return result


def damage_formula_json(formula: dict) -> dict:
    return {
        "type": formula["type"],
        "flags": formula["flags"],
        **{field: formula[field] for field in DAMAGE_RANK_FIELDS},
    }


def emit_damage_formula_cpp(lines: list[str], target: str, formula: dict | None) -> None:
    if formula is None:
        return
    lines.append(f"        {target}.bValid = true;")
    lines.append(f"        {target}.rankCount = {formula['rankCount']}u;")
    lines.append(f"        {target}.type = eDamageType::{formula['cppType']};")
    flag_expression = " | ".join(formula["cppFlags"]) if formula["cppFlags"] else "DamageFlag_None"
    lines.append(f"        {target}.flags = {flag_expression};")
    for field in DAMAGE_RANK_FIELDS:
        for index, value in enumerate(formula[field]):
            lines.append(f"        {target}.{field}[{index}] = {cpp_float(value)};")


def emit_ai_profile_cpp(profile: dict, indent: str) -> list[str]:
    lines = [f"{indent}ChampionAIProfile{{", f"{indent}    eChampion::{profile['champion']},"]
    for field in AI_PROFILE_FLOAT_FIELDS:
        lines.append(f"{indent}    {cpp_float(profile[field])},")
    lines.append(f"{indent}    {{")
    for rule in profile["skillRules"]:
        lines.append(
            f"{indent}        ChampionAISkillRule{{ static_cast<u8_t>(eSkillSlot::{AI_SLOT_MAP[rule['slot']]}), "
            f"{cpp_float(rule['minRange'])}, {cpp_float(rule['score'])} }},"
        )
    lines.extend([f"{indent}    }},", f"{indent}    {len(profile['skillRules'])}u", f"{indent}}}"])
    return lines


def emit_ai_combo_cpp(plan: dict, indent: str) -> list[str]:
    lines = [f"{indent}ChampionAIComboPlan{{", f"{indent}    {{"]
    for step in plan["steps"]:
        lines.append(
            f"{indent}        ChampionAIComboStep{{ static_cast<u8_t>(eSkillSlot::{AI_SLOT_MAP[step['slot']]}), "
            f"{step['itemId']}u, {cpp_float(step['minRange'])}, {cpp_float(step['maxRange'])}, "
            f"{cpp_float(step['selfHpMinRatio'])}, {cpp_float(step['selfHpMaxRatio'])}, "
            f"{cpp_float(step['enemyHpMaxRatio'])}, "
            f"static_cast<u8_t>(eChampionAIComboTargetMode::{step['targetMode']}) }},"
        )
    lines.extend([f"{indent}    }},", f"{indent}    {len(plan['steps'])}u", f"{indent}}}"])
    return lines


def emit_ai_inl(ai_data: dict) -> str:
    default_combo = next(plan for plan in ai_data["comboPlans"] if plan["champion"] == "END")
    overrides = [plan for plan in ai_data["comboPlans"] if plan["champion"] != "END"]
    lines = ["// Generated by Tools/LoLData/Build-LoLDefinitionPack.py. Do not edit.", "namespace ChampionAIDataGenerated", "{"]
    lines.append("    constexpr ChampionAIProfile kDefaultProfile =")
    lines.extend(emit_ai_profile_cpp(ai_data["defaultProfile"], "    "))
    lines[-1] += ";"
    lines.extend(["", "    constexpr ChampionAIProfile kProfiles[] =", "    {"])
    for profile in ai_data["profiles"]:
        profile_lines = emit_ai_profile_cpp(profile, "        ")
        profile_lines[-1] += ","
        lines.extend(profile_lines)
    lines.extend(["    };", "", "    struct ComboEntry", "    {", "        eChampion champion;", "        ChampionAIComboPlan plan;", "    };", "", "    constexpr ChampionAIComboPlan kDefaultComboPlan ="])
    lines.extend(emit_ai_combo_cpp(default_combo, "    "))
    lines[-1] += ";"
    lines.extend(["", "    constexpr ComboEntry kComboPlans[] =", "    {"])
    for plan in overrides:
        lines.append(f"        ComboEntry{{ eChampion::{plan['champion']},")
        combo_lines = emit_ai_combo_cpp(plan, "            ")
        combo_lines[-1] += " },"
        lines.extend(combo_lines)
    lines.extend(["    };", "}", ""])
    return "\n".join(lines)


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


def emit_client_visual_cpp(
    visual_data: dict,
    object_visual_data: dict,
    champion_asset_visual_data: dict,
    spawn_object_data: dict,
    item_data: dict,
    build_hash: int,
) -> str:
    lines = [
        '#include "Client/Private/Data/LoLVisualDefinitionPack.h"',
        '#include "Client/Private/Data/RuntimeVisualDefinitionOverlay.h"',
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
            visible_alive = "true" if state["visibleWhenAlive"] else "false"
            lines.append(
                f"        def.submeshStates[{state_index}].submeshIndex = {state['submeshIndex']}u;"
            )
            lines.append(
                f"        def.submeshStates[{state_index}].bVisibleWhenDestroyed = {visible};"
            )
            lines.append(
                f"        def.submeshStates[{state_index}].bVisibleWhenAlive = {visible_alive};"
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
            ]
        )
        if record.get("visualScaleMultiplier", 1.0) != 1.0:
            lines.append(
                f"        def.visualScaleMultiplier = {cpp_float(record['visualScaleMultiplier'])};"
            )
        lines.append(
            f"        def.textureOverrideCount = static_cast<u8_t>({len(record['textureOverrides'])}u);"
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
        if record.get("visualScaleMultiplier", 1.0) != 1.0:
            lines.append(
                f"        def.visualScaleMultiplier = {cpp_float(record['visualScaleMultiplier'])};"
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

    for record in item_data["items"]:
        lines.extend(
            [
                f"    ClientData::ShopItemPresentationDefinition MakeShopItem_{record['itemId']}()",
                "    {",
                "        ClientData::ShopItemPresentationDefinition def{};",
                f"        def.itemId = {record['itemId']}u;",
                f"        def.price = {record['price']}u;",
            ]
        )
        for key, value in record["stats"].items():
            lines.append(f"        def.stats.{key} = {cpp_float(value)};")
        lines.extend(
            [
                f"        def.displayName = {cpp_string(record['name'])};",
                "        return def;",
                "    }",
                "",
            ]
        )

    for record in spawn_object_data["minions"]:
        lines.extend(
            [
                f"    ClientData::LocalSmokeMinionCombatDefinition MakeLocalSmokeMinion_{record['roleType']}()",
                "    {",
                "        ClientData::LocalSmokeMinionCombatDefinition def{};",
                f"        def.roleType = static_cast<u8_t>({record['roleType']}u);",
            ]
        )
        for key, _ in MINION_FIELDS:
            lines.append(f"        def.combat.{key} = {cpp_float(record[key])};")
        lines.extend(["        return def;", "    }", ""])

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
            "",
            "    const ClientData::ShopItemPresentationDefinition kShopItemPresentations[] =",
            "    {",
        ]
    )
    lines.extend(
        f"        MakeShopItem_{record['itemId']}(),"
        for record in item_data["items"]
    )
    lines.extend(
        [
            "    };",
            "",
            "    const ClientData::LocalSmokeMinionCombatDefinition kLocalSmokeMinionCombatDefinitions[] =",
            "    {",
        ]
    )
    lines.extend(
        f"        MakeLocalSmokeMinion_{record['roleType']}(),"
        for record in spawn_object_data["minions"]
    )
    lines.extend(["    };"])
    lines.extend(
        [
            "}",
            "",
            "namespace ClientData",
            "{",
            "    u32_t GetLoLClientVisualDefinitionBuildHash()",
            "    {",
            f"        return 0x{build_hash:08X}u;",
            "    }",
            "",
            "    const ChampionVisualDefinition* FindChampionVisualDefinition(eChampion champion)",
            "    {",
            "        if (const ChampionVisualDefinition* runtime = FindRuntimeChampionVisualDefinition(champion))",
            "            return runtime;",
            "        for (const ChampionVisualDefinition& definition : kChampionVisuals)",
            "        {",
            "            if (definition.legacyChampion == champion)",
            "                return &definition;",
            "        }",
            "        return nullptr;",
            "    }",
            "",
            "    const ChampionVisualDefinition* FindChampionVisualDefinition(DefinitionKey key)",
            "    {",
            "        if (const ChampionVisualDefinition* runtime = FindRuntimeChampionVisualDefinition(key))",
            "            return runtime;",
            "        for (const ChampionVisualDefinition& definition : kChampionVisuals)",
            "        {",
            "            if (definition.key == key)",
            "                return &definition;",
            "        }",
            "        return nullptr;",
            "    }",
            "",
            "    eChampion ResolveChampionFromDefinitionKey(DefinitionKey key)",
            "    {",
            "        const ChampionVisualDefinition* definition = FindChampionVisualDefinition(key);",
            "        return definition ? definition->legacyChampion : eChampion::END;",
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
            "        if (const ChampionModelVisualPack* runtime = GetRuntimeChampionModelVisualPack())",
            "            return *runtime;",
            "        return kChampionModelVisualPack;",
            "    }",
            "",
            "    const ChampionModelVisualDefinition* FindChampionModelVisualDefinition(eChampion champion)",
            "    {",
            "        if (const ChampionModelVisualDefinition* runtime = FindRuntimeChampionModelVisualDefinition(champion))",
            "            return runtime;",
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
            "        if (const ChampionUiVisualDefinition* runtime = FindRuntimeChampionUiVisualDefinition(champion))",
            "            return runtime;",
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
            "    const ShopItemPresentationDefinition* FindShopItemPresentationDefinition(u16_t itemId)",
            "    {",
            "        for (const ShopItemPresentationDefinition& definition : kShopItemPresentations)",
            "        {",
            "            if (definition.itemId == itemId)",
            "                return &definition;",
            "        }",
            "        return nullptr;",
            "    }",
            "",
            "    const MinionCombatDef* FindLocalSmokeMinionCombatDefinition(u8_t roleType)",
            "    {",
            "        for (const LocalSmokeMinionCombatDefinition& definition : kLocalSmokeMinionCombatDefinitions)",
            "        {",
            "            if (definition.roleType == roleType)",
            "                return &definition.combat;",
            "        }",
            "        return nullptr;",
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
            *[
                f"        def.respawnDelaySecByLevel[{index}u] = {cpp_float(value)};"
                for index, value in enumerate(loadout["respawnDelaySecByLevel"])
            ],
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

    minion_behavior = spawn_object_data["minionBehavior"]
    lines.extend(["    MinionBehaviorDef MakeMinionBehaviorDef()", "    {", "        MinionBehaviorDef def{};"])
    for key, _ in MINION_BEHAVIOR_FLOAT_FIELDS:
        lines.append(f"        def.{key} = {cpp_float(minion_behavior[key])};")
    lines.extend(
        [
            f"        def.pathBuildBudgetPerTick = {minion_behavior['pathBuildBudgetPerTick']}u;",
            f"        def.blockedFramesBeforeRepath = static_cast<u8_t>({minion_behavior['blockedFramesBeforeRepath']}u);",
            f"        def.flowFieldStallFramesBeforePathFallback = static_cast<u8_t>({minion_behavior['flowFieldStallFramesBeforePathFallback']}u);",
            f"        def.targetScanStaggerBuckets = {minion_behavior['targetScanStaggerBuckets']}u;",
            f"        def.rangedRoleType = static_cast<u8_t>({minion_behavior['rangedRoleType']}u);",
            "        return def;",
            "    }",
            "",
        ]
    )

    minion_wave = spawn_object_data["minionWave"]
    lines.extend(
        [
            "    MinionWaveDef MakeMinionWaveDef()",
            "    {",
            "        MinionWaveDef def{};",
            f"        def.waveIntervalTicks = {minion_wave['waveIntervalTicks']}ull;",
            f"        def.initialDelayTicks = {minion_wave['initialDelayTicks']}ull;",
            f"        def.perMinionDelayTicks = {minion_wave['perMinionDelayTicks']}ull;",
            f"        def.siegeWavePeriod = {minion_wave['siegeWavePeriod']}u;",
            f"        def.timeGrowthCapMinutes = {minion_wave['timeGrowthCapMinutes']}u;",
            f"        def.timeGrowthPerMinute = {cpp_float(minion_wave['timeGrowthPerMinute'])};",
            f"        def.corpseDeathTimerSec = {cpp_float(minion_wave['corpseDeathTimerSec'])};",
            f"        def.startX = {cpp_float(minion_wave['startX'])};",
        ]
    )
    for key, _ in MINION_WAVE_RANGED_PROJECTILE_FIELDS:
        lines.append(
            f"        def.rangedProjectile.{key} = {cpp_float(minion_wave['rangedProjectile'][key])};")
    for index, slot in enumerate(minion_wave["formationSlots"]):
        lines.append(
            f"        def.formationSlots[{index}] = MinionSpawnSlotDef{{ static_cast<u8_t>({slot['roleType']}u), "
            f"{cpp_float(slot['forwardOffset'])}, {cpp_float(slot['sideOffset'])} }};")
    siege_slot = minion_wave["siegeSlot"]
    lines.extend(
        [
            f"        def.formationSlotCount = static_cast<u8_t>({len(minion_wave['formationSlots'])}u);",
            f"        def.siegeSlot = MinionSpawnSlotDef{{ static_cast<u8_t>({siege_slot['roleType']}u), "
            f"{cpp_float(siege_slot['forwardOffset'])}, {cpp_float(siege_slot['sideOffset'])} }};",
            "        return def;",
            "    }",
            "",
        ]
    )

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


def append_economy_cpp(lines: list[str], economy_data: dict) -> None:
    lines.extend(
        [
            "    EconomyGameplayDef MakeEconomyDef()",
            "    {",
            "        EconomyGameplayDef def{};",
        ]
    )
    for index, value in enumerate(economy_data["xpCurve"], start=1):
        lines.append(f"        def.xpRequiredForNextLevel[{index}] = {cpp_float(value)};")
    champion_kill = economy_data["championKill"]
    for key, _ in ECONOMY_CHAMPION_KILL_FIELDS:
        lines.append(f"        def.championKill.{key} = {cpp_float(champion_kill[key])};")
    for kind in ECONOMY_MINION_KINDS:
        record = economy_data["minions"][kind]
        for key, _ in ECONOMY_MINION_FIELDS:
            lines.append(f"        def.{kind}.{key} = {cpp_float(record[key])};")
    lines.append(f"        def.turretGold = {cpp_float(economy_data['turretGold'])};")
    lines.append(
        f"        def.turretTeamGold = {cpp_float(economy_data['turretTeamGold'])};"
    )
    jungle = economy_data["jungle"]
    for key, _ in ECONOMY_JUNGLE_FIELDS:
        lines.append(f"        def.jungle.{key} = {cpp_float(jungle[key])};")
    objectives = economy_data["objectives"]
    for key, _ in ECONOMY_OBJECTIVE_FLOAT_FIELDS:
        lines.append(f"        def.objectives.{key} = {cpp_float(objectives[key])};")
    lines.append(f"        def.objectives.teamLevelGrant = {objectives['teamLevelGrant']}u;")
    passive_gold = economy_data["passiveGold"]
    timers = economy_data["timers"]
    lines.extend(
        [
            f"        def.passiveGoldStartTick = {passive_gold['startTick']}ull;",
            f"        def.passiveGoldIntervalTicks = {passive_gold['intervalTicks']}ull;",
            f"        def.passiveGoldPerGrant = {passive_gold['perGrant']}u;",
            f"        def.assistCreditWindowSec = {cpp_float(timers['assistCreditWindowSec'])};",
            f"        def.recallDurationSec = {cpp_float(timers['recallDurationSec'])};",
            "        def.bValid = true;",
            "        return def;",
            "    }",
            "",
        ]
    )


def append_items_cpp(lines: list[str], item_data: dict) -> None:
    for record in item_data["items"]:
        lines.extend(
            [
                f"    ItemDef MakeItem_{record['itemId']}()",
                "    {",
                "        ItemDef def{};",
                f"        def.itemId = {record['itemId']}u;",
                f"        def.price = {record['price']}u;",
            ]
        )
        if not record["purchasable"]:
            lines.append("        def.bPurchasable = false;")
        for key, value in record["stats"].items():
            lines.append(f"        def.stats.{key} = {cpp_float(value)};")
        emit_damage_formula_cpp(lines, "def.onHitDamage", record.get("onHitDamage"))
        spellblade = record.get("spellblade")
        if spellblade:
            lines.extend(
                [
                    "        def.spellblade.bValid = true;",
                    f"        def.spellblade.cooldownSec = {cpp_float(spellblade['cooldownSec'])};",
                    f"        def.spellblade.baseAdRatio = {cpp_float(spellblade['baseAdRatio'])};",
                    f"        def.spellblade.critChanceFlatScale = {cpp_float(spellblade['critChanceFlatScale'])};",
                    f"        def.spellblade.manaRestoreRatio = {cpp_float(spellblade['manaRestoreRatio'])};",
                ]
            )
        manaflow = record.get("manaflow")
        if manaflow:
            lines.extend(
                [
                    "        def.manaflow.bValid = true;",
                    f"        def.manaflow.rechargeSec = {cpp_float(manaflow['rechargeSec'])};",
                    f"        def.manaflow.maxCharges = {manaflow['maxCharges']}u;",
                    f"        def.manaflow.manaPerTrigger = {manaflow['manaPerTrigger']}u;",
                    f"        def.manaflow.championMultiplier = {manaflow['championMultiplier']}u;",
                    f"        def.manaflow.maxBonusMana = {manaflow['maxBonusMana']}u;",
                    f"        def.manaflow.transformItemId = {manaflow['transformItemId']}u;",
                ]
            )
        lightshield_strike = record.get("lightshieldStrike")
        if lightshield_strike:
            lines.extend(
                [
                    "        def.lightshieldStrike.bValid = true;",
                    f"        def.lightshieldStrike.cooldownSec = {cpp_float(lightshield_strike['cooldownSec'])};",
                    f"        def.lightshieldStrike.critDamageMultiplier = {cpp_float(lightshield_strike['critDamageMultiplier'])};",
                    f"        def.lightshieldStrike.healBaseAdRatio = {cpp_float(lightshield_strike['healBaseAdRatio'])};",
                    f"        def.lightshieldStrike.healMissingHealthRatio = {cpp_float(lightshield_strike['healMissingHealthRatio'])};",
                ]
            )
        active = record.get("active")
        if active:
            lines.extend(
                [
                    "        def.active.bValid = true;",
                    f"        def.active.kind = eItemActiveKind::{ITEM_ACTIVE_KIND_MAP[active['kind']]};",
                    f"        def.active.cooldownSec = {cpp_float(active['cooldownSec'])};",
                    f"        def.active.durationSec = {cpp_float(active['durationSec'])};",
                ]
            )
        if record.get("maxManaBonusAdRatio", 0.0) != 0.0:
            lines.append(
                f"        def.maxManaBonusAdRatio = {cpp_float(record['maxManaBonusAdRatio'])};")
        lines.extend(
            [
                f"        def.displayName = {cpp_string(record['name'])};",
                "        return def;",
                "    }",
                "",
            ]
        )
    lines.extend(["    const ItemDef kItemDefs[] =", "    {"])
    lines.extend(f"        MakeItem_{record['itemId']}()," for record in item_data["items"])
    lines.extend(["    };", ""])


def append_runes_cpp(lines: list[str], rune_data: dict) -> None:
    for record in rune_data["runes"]:
        lines.extend(
            [
                f"    RuneGameplayDef MakeRune_{record['legacyRuneId']}()",
                "    {",
                "        RuneGameplayDef def{};",
                f"        def.key = 0x{record['definitionKey']:08X}u;",
                f"        def.legacyRuneId = static_cast<eRuneId>({record['legacyRuneId']}u);",
                f"        def.bEnabled = {'true' if record['enabled'] else 'false'};",
                f"        def.maxStacks = {record['maxStacks']}u;",
                "        return def;",
                "    }",
                "",
            ]
        )
    lines.extend(["    const RuneGameplayDef kRuneDefs[] =", "    {"])
    lines.extend(
        f"        MakeRune_{record['legacyRuneId']}(),"
        for record in rune_data["runes"]
    )
    lines.extend(["    };", ""])


def emit_cpp(
    data: dict,
    spawn_object_data: dict,
    economy_data: dict,
    item_data: dict,
    rune_data: dict,
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
            if field == "resourceKind":
                lines.append(
                    f"        def.stats.resourceKind = eChampionResourceKind::{value};")
                continue
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
                f"        def.input.activation = eSkillInputActivation::{record['activationMode']};",
                "        def.target.bValid = true;",
                f"        def.target.resolvePolicy = eTargetResolvePolicy::{record['resolvePolicy']};",
                f"        def.cost.rankCount = {len(record['manaCostByRank'])}u;",
                f"        def.cooldown.rankCount = {len(record['cooldownSecByRank'])}u;",
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
        for rank, value in enumerate(record["manaCostByRank"]):
            lines.append(f"        def.cost.manaCostByRank[{rank}] = {cpp_float(value)};")
        for rank, value in enumerate(record["cooldownSecByRank"]):
            lines.append(f"        def.cooldown.cooldownSecByRank[{rank}] = {cpp_float(value)};")
        emit_damage_formula_cpp(lines, "def.effect.damage", record.get("damageFormula"))
        effect_params = record.get("effectParams", [])
        if effect_params:
            lines.append(f"        def.effect.paramCount = static_cast<u8_t>({len(effect_params)}u);")
            for param_index, param in enumerate(effect_params):
                lines.append(
                    f"        def.effect.params[{param_index}].id = eSkillEffectParamId::{param['cppId']};"
                )
                lines.append(
                    f"        def.effect.params[{param_index}].rankCount = "
                    f"static_cast<u8_t>({len(param['values'])}u);"
                )
                for rank_index, value in enumerate(param["values"]):
                    lines.append(
                        f"        def.effect.params[{param_index}].valueByRank[{rank_index}] = "
                        f"{cpp_float(value)};"
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
        charge = record.get("charge")
        if charge is not None:
            lines.append("        def.charge.bEnabled = true;")
            lines.append(
                f"        def.charge.bAutoRelease = {'true' if charge['autoRelease'] else 'false'};")
            lines.append(f"        def.charge.maxHoldSec = {cpp_float(charge['maxHoldSec'])};")
            lines.append(f"        def.charge.minRangeScale = {cpp_float(charge['rangeScale'][0])};")
            lines.append(f"        def.charge.maxRangeScale = {cpp_float(charge['rangeScale'][1])};")
            lines.append(f"        def.charge.minDamageScale = {cpp_float(charge['damageScale'][0])};")
            lines.append(f"        def.charge.maxDamageScale = {cpp_float(charge['damageScale'][1])};")
            lines.append(f"        def.charge.minStunSec = {cpp_float(charge['stunSeconds'][0])};")
            lines.append(f"        def.charge.maxStunSec = {cpp_float(charge['stunSeconds'][1])};")
        for stage_index, stage in enumerate(record["stages"]):
            target_shape = record["targetShapes"][stage_index]
            lines.append(f"        def.target.shape[{stage_index}] = eTargetShape::{target_shape};")
            lines.append(f"        def.stage.lockDurationSec[{stage_index}] = {cpp_float(stage['lockDurationSec'])};")
            lines.append(f"        def.stage.commandLockSec[{stage_index}] = {cpp_float(stage['commandLockSec'])};")
            lines.append(
                f"        def.stage.movePolicy[{stage_index}] = "
                f"eSkillActionMovePolicy::{stage['movePolicy']};")
            lines.append(
                f"        def.stage.bCreatesActionState[{stage_index}] = "
                f"{'true' if stage['createsActionState'] else 'false'};")
            lines.append(
                f"        def.stage.bPresentationLoopWhileActive[{stage_index}] = "
                f"{'true' if stage['presentationLoopWhileActive'] else 'false'};")
            lines.append(
                f"        def.facing.mode[{stage_index}] = "
                f"eSkillFacingMode::{stage['facingMode']};")
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
    append_economy_cpp(lines, economy_data)
    append_items_cpp(lines, item_data)
    append_runes_cpp(lines, rune_data)

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
            "        static const EconomyGameplayDef economyDef = MakeEconomyDef();",
            "        static const GameplayDefinitionPack pack =",
            "        {",
            f"            {{ {data['schemaVersion']}u, {max(c['dataVersion'] for c in champions)}u, kBuildHash, 0u, eDataPackVisibility::ServerPrivate }},",
            "            kChampions,",
            "            sizeof(kChampions) / sizeof(kChampions[0]),",
            "            kSkills,",
            "            sizeof(kSkills) / sizeof(kSkills[0]),",
            "            kSummonerSpells,",
            "            sizeof(kSummonerSpells) / sizeof(kSummonerSpells[0]),",
            "            &economyDef,",
            "            kItemDefs,",
            "            sizeof(kItemDefs) / sizeof(kItemDefs[0]),",
            "            kRuneDefs,",
            "            sizeof(kRuneDefs) / sizeof(kRuneDefs[0]),",
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
            "            MakeMinionBehaviorDef(),",
            "            MakeMinionWaveDef(),",
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


def canonical_source_table(root: Path) -> dict[str, Path]:
    relative_paths = (
        "Data/Gameplay/ChampionGameData/champions.json",
        "Data/LoL/ServerPrivate/Gameplay/SummonerSpellGameplayDefs.json",
        "Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json",
        "Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json",
        "Data/LoL/ServerPrivate/Gameplay/EconomyGameplayDefs.json",
        "Data/LoL/ServerPrivate/Gameplay/ItemGameplayDefs.json",
        "Data/LoL/ServerPrivate/Gameplay/RuneGameplayDefs.json",
        "Data/LoL/ServerPrivate/AI/ChampionAIGameplayDefs.json",
        "Data/LoL/ClientPublic/Visual/ChampionVisualDefs.json",
        "Data/LoL/ClientPublic/Visual/ObjectVisualDefs.json",
        "Data/LoL/ClientPublic/Visual/ChampionAssetVisualDefs.json",
    )
    return {relative_path: root / Path(relative_path) for relative_path in relative_paths}


def parse_draft_build_hash(value: object) -> int:
    if isinstance(value, bool):
        fail("draft.baseBuildHash must be an unsigned 32-bit integer or hexadecimal string")
    if isinstance(value, int):
        parsed = value
    elif isinstance(value, str):
        try:
            parsed = int(value, 0)
        except ValueError:
            fail("draft.baseBuildHash must be an unsigned 32-bit integer or hexadecimal string")
    else:
        fail("draft.baseBuildHash must be an unsigned 32-bit integer or hexadecimal string")
    if parsed < 0 or parsed > 0xFFFFFFFF:
        fail("draft.baseBuildHash is outside the unsigned 32-bit range")
    return parsed


def require_draft_base_hash(draft: dict, expected_build_hash: int) -> None:
    if "baseBuildHash" not in draft:
        fail("draft.baseBuildHash is required")
    actual_build_hash = parse_draft_build_hash(draft["baseBuildHash"])
    if actual_build_hash != expected_build_hash:
        fail(
            "stale draft: baseBuildHash "
            f"0x{actual_build_hash:08X} does not match active 0x{expected_build_hash:08X}"
        )


def decode_json_pointer(path: object) -> list[str]:
    if not isinstance(path, str) or not path.startswith("/"):
        fail("draft edit path must be a non-empty JSON Pointer")
    return [token.replace("~1", "/").replace("~0", "~") for token in path[1:].split("/")]


def resolve_existing_child(container: object, token: str, path: str) -> object:
    if isinstance(container, dict):
        if token not in container:
            fail(f"draft edit path does not exist: {path}")
        return container[token]
    if isinstance(container, list):
        if not token.isdigit():
            fail(f"draft array index must be numeric: {path}")
        index = int(token)
        if index >= len(container):
            fail(f"draft array index is out of range: {path}")
        return container[index]
    fail(f"draft edit traverses a scalar value: {path}")


def apply_json_pointer_edit(document: object, path: object, value: object) -> None:
    tokens = decode_json_pointer(path)
    if tokens[0] in {"schemaVersion", "dataVersion", "buildHash"}:
        fail(f"draft cannot edit metadata field: {tokens[0]}")
    cursor = document
    for index, token in enumerate(tokens[:-1]):
        cursor = resolve_existing_child(cursor, token, "/" + "/".join(tokens[: index + 1]))
    last = tokens[-1]
    if isinstance(cursor, dict):
        if last not in cursor:
            fail(f"draft edit path does not exist: {path}")
        cursor[last] = copy.deepcopy(value)
        return
    if isinstance(cursor, list):
        if not last.isdigit() or int(last) >= len(cursor):
            fail(f"draft array index is out of range: {path}")
        cursor[int(last)] = copy.deepcopy(value)
        return
    fail(f"draft edit parent is a scalar value: {path}")


def find_definition_nodes(value: object, definition_key_value: object) -> list[dict]:
    matches: list[dict] = []
    if isinstance(value, dict):
        if value.get("key") == definition_key_value or value.get("definitionKey") == definition_key_value:
            matches.append(value)
        for child in value.values():
            matches.extend(find_definition_nodes(child, definition_key_value))
    elif isinstance(value, list):
        for child in value:
            matches.extend(find_definition_nodes(child, definition_key_value))
    return matches


def apply_row_draft(document: dict, draft: dict) -> None:
    definition_key_value = draft.get("definitionKey")
    field = draft.get("field")
    if not isinstance(definition_key_value, (str, int)) or isinstance(definition_key_value, bool):
        fail("draft.definitionKey must be a string or integer")
    if not isinstance(field, str) or not field or field.startswith(".") or field.endswith("."):
        fail("draft.field must be a dotted field path")
    matches = find_definition_nodes(document, definition_key_value)
    if len(matches) != 1:
        fail(f"draft.definitionKey must resolve exactly once; found {len(matches)}")
    pointer = "/" + "/".join(field.split("."))
    if "index" in draft:
        index = draft["index"]
        if isinstance(index, bool) or not isinstance(index, int) or index < 0:
            fail("draft.index must be a non-negative integer")
        pointer += f"/{index}"
    apply_json_pointer_edit(matches[0], pointer, draft.get("value"))


def apply_visual_timing_overrides(document: dict, overrides: object) -> None:
    champion_records = require_array(document.get("champions"), "ChampionVisualDefs.champions")
    champion_lookup = {
        str(record.get("key", "")).removeprefix("champion.").lower(): record
        for record in champion_records
        if isinstance(record, dict)
    }
    slot_names = {"ba": "basic_attack", "q": "q", "w": "w", "e": "e", "r": "r"}
    seen: set[tuple[str, str]] = set()
    for index, raw_override in enumerate(require_array(overrides, "draft.overrides")):
        override = require_object(raw_override, f"draft.overrides[{index}]")
        if set(override) != {"champion", "slot", "stages"}:
            fail(f"draft.overrides[{index}] must contain only champion, slot and stages")
        champion_name = str(override["champion"]).lower()
        slot_name = str(override["slot"]).lower()
        if champion_name not in champion_lookup or slot_name not in slot_names:
            fail(f"draft.overrides[{index}] references an unknown champion or slot")
        identity = (champion_name, slot_name)
        if identity in seen:
            fail(f"draft.overrides[{index}] duplicates {champion_name}.{slot_name}")
        seen.add(identity)
        skill_key = f"skill.{champion_name}.{slot_names[slot_name]}"
        skills = require_array(champion_lookup[champion_name].get("skills"), f"{champion_name}.skills")
        matches = [skill for skill in skills if isinstance(skill, dict) and skill.get("key") == skill_key]
        if len(matches) != 1:
            fail(f"draft override skill must resolve exactly once: {skill_key}")
        matches[0]["stages"] = copy.deepcopy(override["stages"])


def apply_draft_to_document(draft: dict, source_relative: str, original: dict) -> dict:
    result = copy.deepcopy(original)
    modes = [
        "document" in draft,
        "edits" in draft,
        "overrides" in draft,
        all(key in draft for key in ("definitionKey", "field", "value")),
    ]
    if sum(modes) != 1:
        fail("draft must contain exactly one of document, edits, overrides, or a definition row")
    if "document" in draft:
        result = copy.deepcopy(require_object(draft["document"], "draft.document"))
    elif "edits" in draft:
        for index, raw_edit in enumerate(require_array(draft["edits"], "draft.edits")):
            edit = require_object(raw_edit, f"draft.edits[{index}]")
            if set(edit) != {"path", "value"}:
                fail(f"draft.edits[{index}] must contain only path and value")
            apply_json_pointer_edit(result, edit["path"], edit["value"])
    elif "overrides" in draft:
        if source_relative != "Data/LoL/ClientPublic/Visual/ChampionVisualDefs.json":
            fail("draft.overrides is supported only for ChampionVisualDefs.json")
        apply_visual_timing_overrides(result, draft["overrides"])
    else:
        apply_row_draft(result, draft)
    return result


def diff_json_paths(before: object, after: object, path: str = "") -> set[str]:
    if type(before) is not type(after):
        return {path or "/"}
    if isinstance(before, dict):
        if set(before) != set(after):
            return {path or "/"}
        result: set[str] = set()
        for key in before:
            result.update(diff_json_paths(before[key], after[key], f"{path}/{key}"))
        return result
    if isinstance(before, list):
        if len(before) != len(after):
            return {path or "/"}
        result: set[str] = set()
        for index, value in enumerate(before):
            result.update(diff_json_paths(value, after[index], f"{path}/{index}"))
        return result
    return set() if before == after else {path or "/"}


def atomic_write_text(path: Path, content: str) -> None:
    temporary_path = path.with_name(path.name + ".tmp")
    try:
        temporary_path.write_text(content, encoding="utf-8", newline="\n")
        temporary_path.replace(path)
    finally:
        temporary_path.unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--check", action="store_true")
    draft_group = parser.add_mutually_exclusive_group()
    draft_group.add_argument("--verify-draft", type=Path)
    draft_group.add_argument("--apply-draft", type=Path)
    draft_group.add_argument("--test-draft-roundtrip", action="store_true")
    args = parser.parse_args()

    if args.check and (args.verify_draft or args.apply_draft or args.test_draft_roundtrip):
        fail("--check cannot be combined with a draft operation")

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
    economy_source = root / "Data" / "LoL" / "ServerPrivate" / "Gameplay" / "EconomyGameplayDefs.json"
    if not economy_source.exists():
        fail(f"missing source: {economy_source}")
    item_source = root / "Data" / "LoL" / "ServerPrivate" / "Gameplay" / "ItemGameplayDefs.json"
    if not item_source.exists():
        fail(f"missing source: {item_source}")
    rune_source = root / "Data" / "LoL" / "ServerPrivate" / "Gameplay" / "RuneGameplayDefs.json"
    if not rune_source.exists():
        fail(f"missing source: {rune_source}")
    ai_source = root / "Data" / "LoL" / "ServerPrivate" / "AI" / "ChampionAIGameplayDefs.json"
    if not ai_source.exists():
        fail(f"missing source: {ai_source}")
    champion_visual_source = root / "Data" / "LoL" / "ClientPublic" / "Visual" / "ChampionVisualDefs.json"
    if not champion_visual_source.exists():
        fail(f"missing source: {champion_visual_source}")
    object_visual_source = root / "Data" / "LoL" / "ClientPublic" / "Visual" / "ObjectVisualDefs.json"
    if not object_visual_source.exists():
        fail(f"missing source: {object_visual_source}")
    champion_asset_visual_source = root / "Data" / "LoL" / "ClientPublic" / "Visual" / "ChampionAssetVisualDefs.json"
    if not champion_asset_visual_source.exists():
        fail(f"missing source: {champion_asset_visual_source}")

    data_raw = json.loads(source.read_text(encoding="utf-8"))
    validate_source_schema(root, source, data_raw)
    data = legacy.normalize_root(data_raw)
    summoner_spell_raw = json.loads(summoner_spell_source.read_text(encoding="utf-8"))
    validate_source_schema(root, summoner_spell_source, summoner_spell_raw)
    summoner_spell_raw.pop("buildHash", None)
    summoner_spell_data = normalize_summoner_spell_root(summoner_spell_raw)
    champions, skills, spells = make_records(data, summoner_spell_data)
    champion_visual_raw = json.loads(champion_visual_source.read_text(encoding="utf-8"))
    validate_source_schema(root, champion_visual_source, champion_visual_raw)
    champion_visual_raw.pop("buildHash", None)
    champion_visual_data = normalize_client_visual_root(
        champion_visual_raw,
        {canonical_champion(champion["champion"]): champion["champion"] for champion in data["champions"]},
        {record["canonicalKey"] for record in skills},
    )
    spawn_object_raw = json.loads(spawn_object_source.read_text(encoding="utf-8"))
    validate_source_schema(root, spawn_object_source, spawn_object_raw)
    spawn_object_raw.pop("buildHash", None)
    spawn_object_data = normalize_spawn_object_root(spawn_object_raw)
    object_visual_raw = json.loads(object_visual_source.read_text(encoding="utf-8"))
    validate_source_schema(root, object_visual_source, object_visual_raw)
    object_visual_raw.pop("buildHash", None)
    object_visual_data = normalize_object_visual_root(object_visual_raw)
    champion_asset_visual_raw = json.loads(champion_asset_visual_source.read_text(encoding="utf-8"))
    validate_source_schema(root, champion_asset_visual_source, champion_asset_visual_raw)
    champion_asset_visual_raw.pop("buildHash", None)
    champion_asset_visual_data = normalize_champion_asset_visual_root(
        champion_asset_visual_raw,
        {champion["champion"] for champion in data["champions"]},
    )
    skill_effect_raw = json.loads(skill_effect_source.read_text(encoding="utf-8"))
    validate_source_schema(root, skill_effect_source, skill_effect_raw)
    skill_effect_raw.pop("buildHash", None)
    skill_effect_data = normalize_skill_effect_root(
        skill_effect_raw,
        {record["canonicalKey"] for record in skills},
    )
    apply_skill_effect_params(skills, skill_effect_data)
    economy_raw = json.loads(economy_source.read_text(encoding="utf-8"))
    validate_source_schema(root, economy_source, economy_raw)
    economy_raw.pop("buildHash", None)
    economy_data = normalize_economy_root(economy_raw)
    item_raw = json.loads(item_source.read_text(encoding="utf-8"))
    validate_source_schema(root, item_source, item_raw)
    item_raw.pop("buildHash", None)
    item_data = normalize_items_root(item_raw)
    rune_raw = json.loads(rune_source.read_text(encoding="utf-8"))
    validate_source_schema(root, rune_source, rune_raw)
    rune_raw.pop("buildHash", None)
    rune_data = normalize_rune_root(rune_raw)
    ai_raw = json.loads(ai_source.read_text(encoding="utf-8"))
    validate_source_schema(root, ai_source, ai_raw)
    ai_raw.pop("buildHash", None)
    ai_data = normalize_ai_root(ai_raw, {champion["champion"] for champion in data["champions"]})
    build_hash = compute_definition_pack_hash(
        data,
        summoner_spell_data,
        spawn_object_data,
        skill_effect_data,
        economy_data,
        item_data,
        rune_data,
        ai_data,
        champion_visual_data,
        object_visual_data,
        champion_asset_visual_data)
    champion_ids = {record["canonicalKey"]: record["defId"] for record in champions}
    skill_ids = {record["canonicalKey"]: record["defId"] for record in skills}

    if args.verify_draft or args.apply_draft:
        draft_path = (args.verify_draft or args.apply_draft).resolve()
        if not draft_path.exists():
            fail(f"missing draft: {draft_path}")
        draft = require_object(json.loads(draft_path.read_text(encoding="utf-8")), "draft")
        require_draft_base_hash(draft, build_hash)
        source_relative = draft.get("source")
        source_table = canonical_source_table(root)
        if not isinstance(source_relative, str) or source_relative not in source_table:
            fail("draft.source must name one of the 11 canonical authoring sources")
        target_source = source_table[source_relative]
        target_raw = require_object(json.loads(target_source.read_text(encoding="utf-8")), target_source.name)
        candidate_raw = apply_draft_to_document(draft, source_relative, target_raw)
        validate_source_schema(root, target_source, candidate_raw)
        candidate_for_normalize = copy.deepcopy(candidate_raw)
        candidate_for_normalize.pop("buildHash", None)

        if target_source == source:
            candidate_data = legacy.normalize_root(candidate_for_normalize)
            _, candidate_skills, _ = make_records(candidate_data, summoner_spell_data)
            candidate_champions = {champion["champion"] for champion in candidate_data["champions"]}
            candidate_champion_map = {
                canonical_champion(champion): champion for champion in candidate_champions
            }
            candidate_skill_keys = {record["canonicalKey"] for record in candidate_skills}
            normalize_ai_root(copy.deepcopy(ai_raw), candidate_champions)
            normalize_client_visual_root(
                copy.deepcopy(champion_visual_raw), candidate_champion_map, candidate_skill_keys)
            normalize_champion_asset_visual_root(
                copy.deepcopy(champion_asset_visual_raw), candidate_champions)
            normalize_skill_effect_root(copy.deepcopy(skill_effect_raw), candidate_skill_keys)
        elif target_source == summoner_spell_source:
            normalize_summoner_spell_root(candidate_for_normalize)
        elif target_source == spawn_object_source:
            normalize_spawn_object_root(candidate_for_normalize)
        elif target_source == skill_effect_source:
            normalize_skill_effect_root(candidate_for_normalize, {record["canonicalKey"] for record in skills})
        elif target_source == economy_source:
            normalize_economy_root(candidate_for_normalize)
        elif target_source == item_source:
            normalize_items_root(candidate_for_normalize)
        elif target_source == rune_source:
            normalize_rune_root(candidate_for_normalize)
        elif target_source == ai_source:
            normalize_ai_root(candidate_for_normalize, {champion["champion"] for champion in data["champions"]})
        elif target_source == champion_visual_source:
            normalize_client_visual_root(
                candidate_for_normalize,
                {canonical_champion(champion["champion"]): champion["champion"] for champion in data["champions"]},
                {record["canonicalKey"] for record in skills},
            )
        elif target_source == object_visual_source:
            normalize_object_visual_root(candidate_for_normalize)
        elif target_source == champion_asset_visual_source:
            normalize_champion_asset_visual_root(
                candidate_for_normalize, {champion["champion"] for champion in data["champions"]})

        if args.verify_draft:
            print(f"Verified draft for {source_relative} at base 0x{build_hash:08X}")
            return 0

        atomic_write_text(target_source, json_text(candidate_raw))
        generation = subprocess.run(
            [sys.executable, str(Path(__file__).resolve()), "--root", str(root)],
            cwd=root,
            check=False,
        )
        if generation.returncode != 0:
            fail(f"draft was validated but regeneration failed with exit code {generation.returncode}")
        manifest_path = root / "Data" / "LoL" / "SharedContract" / "DefinitionManifest.json"
        regenerated_hash = int(json.loads(manifest_path.read_text(encoding="utf-8"))["buildHash"])
        for canonical_path in canonical_source_table(root).values():
            canonical_document = require_object(
                json.loads(canonical_path.read_text(encoding="utf-8")), canonical_path.name)
            if "buildHash" not in canonical_document:
                continue
            canonical_document["buildHash"] = regenerated_hash
            atomic_write_text(canonical_path, json_text(canonical_document))
        print(
            f"Applied draft to {source_relative} and regenerated LoL definition pack "
            f"0x{regenerated_hash:08X}"
        )
        return 0

    if args.test_draft_roundtrip:
        test_source_relative = "Data/LoL/ServerPrivate/Gameplay/RuneGameplayDefs.json"
        test_source = canonical_source_table(root)[test_source_relative]
        original = require_object(json.loads(test_source.read_text(encoding="utf-8")), test_source.name)
        runes = require_array(original.get("runes"), "RuneGameplayDefs.runes")
        if not runes or not isinstance(runes[0].get("maxStacks"), int):
            fail("round-trip fixture requires RuneGameplayDefs.runes[0].maxStacks")
        replacement = runes[0]["maxStacks"] + 1
        draft = {
            "version": 1,
            "source": test_source_relative,
            "baseBuildHash": f"0x{build_hash:08X}",
            "edits": [{"path": "/runes/0/maxStacks", "value": replacement}],
        }
        serialized_draft = json.dumps(draft, sort_keys=True)
        parsed_draft = require_object(json.loads(serialized_draft), "roundTripDraft")
        require_draft_base_hash(parsed_draft, build_hash)
        candidate = apply_draft_to_document(parsed_draft, test_source_relative, original)
        validate_source_schema(root, test_source, candidate)
        candidate_for_normalize = copy.deepcopy(candidate)
        candidate_for_normalize.pop("buildHash", None)
        normalize_rune_root(candidate_for_normalize)
        changed_paths = diff_json_paths(original, candidate)
        if changed_paths != {"/runes/0/maxStacks"}:
            fail(f"round-trip changed unexpected paths: {sorted(changed_paths)}")
        stale_draft = copy.deepcopy(parsed_draft)
        stale_draft["baseBuildHash"] = build_hash ^ 1
        try:
            require_draft_base_hash(stale_draft, build_hash)
        except SystemExit:
            pass
        else:
            fail("round-trip stale hash fixture was not rejected")
        if original["runes"][0]["maxStacks"] == replacement:
            fail("round-trip test mutated the canonical document")
        with tempfile.TemporaryDirectory(prefix="winters_lol_draft_") as temporary_directory:
            temporary_root = Path(temporary_directory)
            for relative_path, canonical_path in canonical_source_table(root).items():
                copied_path = temporary_root / Path(relative_path)
                copied_path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(canonical_path, copied_path)
            shutil.copytree(
                root / "Data" / "LoL" / "Schemas",
                temporary_root / "Data" / "LoL" / "Schemas",
            )
            draft_path = temporary_root / "valid-draft.json"
            atomic_write_text(draft_path, json_text(draft))
            apply_result = subprocess.run(
                [
                    sys.executable,
                    str(Path(__file__).resolve()),
                    "--root",
                    str(temporary_root),
                    "--apply-draft",
                    str(draft_path),
                ],
                cwd=root,
                capture_output=True,
                text=True,
                check=False,
            )
            if apply_result.returncode != 0:
                fail(
                    "temporary draft apply failed: "
                    + (apply_result.stderr.strip() or apply_result.stdout.strip())
                )
            applied_rune_path = temporary_root / Path(test_source_relative)
            applied_rune = require_object(
                json.loads(applied_rune_path.read_text(encoding="utf-8")), applied_rune_path.name)
            if applied_rune["runes"][0]["maxStacks"] != replacement:
                fail("temporary draft apply did not publish the intended field")
            temporary_manifest_path = (
                temporary_root / "Data" / "LoL" / "SharedContract" / "DefinitionManifest.json")
            temporary_hash = int(
                json.loads(temporary_manifest_path.read_text(encoding="utf-8"))["buildHash"])
            if temporary_hash == build_hash:
                fail("temporary draft apply did not regenerate the definition-pack hash")

            stale_draft_path = temporary_root / "stale-draft.json"
            atomic_write_text(stale_draft_path, json_text(stale_draft))
            applied_text_before_stale_check = applied_rune_path.read_text(encoding="utf-8")
            stale_result = subprocess.run(
                [
                    sys.executable,
                    str(Path(__file__).resolve()),
                    "--root",
                    str(temporary_root),
                    "--apply-draft",
                    str(stale_draft_path),
                ],
                cwd=root,
                capture_output=True,
                text=True,
                check=False,
            )
            if stale_result.returncode == 0:
                fail("temporary stale draft apply unexpectedly succeeded")
            if applied_rune_path.read_text(encoding="utf-8") != applied_text_before_stale_check:
                fail("stale draft changed the temporary canonical source")
        if json.loads(test_source.read_text(encoding="utf-8")) != original:
            fail("round-trip integration test changed the workspace canonical source")
        print(
            "Draft round-trip PASS: temporary apply regenerated a new hash, stale hash was rejected, "
            "workspace canonical source remained unchanged"
        )
        return 0

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
        root / "Data" / "LoL" / "SharedContract" / "DefinitionManifest.json": json_text(
            manifest_json(data, champions, skills, spells, build_hash)
        ),
        root / "Server" / "Private" / "Data" / "Generated" / "LoLGameplayDefinitions.generated.cpp": emit_cpp(
            data,
            spawn_object_data,
            economy_data,
            item_data,
            rune_data,
            champions,
            skills,
            spells,
            build_hash,
        ),
        root / "Client" / "Private" / "Data" / "Generated" / "LoLVisualDefinitions.generated.cpp": emit_client_visual_cpp(
            champion_visual_data,
            object_visual_data,
            champion_asset_visual_data,
            spawn_object_data,
            item_data,
            build_hash,
        ),
        root / "Shared" / "GameSim" / "Generated" / "ChampionAIPolicyData.generated.inl": emit_ai_inl(ai_data),
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
        atomic_write_text(path, content)

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
