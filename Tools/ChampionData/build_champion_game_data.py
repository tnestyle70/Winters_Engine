from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path


SKILL_SLOT_COUNT = 5
SKILL_STAGE_MAX = 2
SKILL_RANK_VALUE_MAX = 5
STAT_FIELDS = {
    "baseHp": 600.0,
    "hpPerLevel": 100.0,
    "baseMana": 300.0,
    "manaPerLevel": 50.0,
    "baseAd": 60.0,
    "adPerLevel": 3.5,
    "baseAp": 0.0,
    "apPerLevel": 0.0,
    "baseArmor": 30.0,
    "armorPerLevel": 4.0,
    "baseMr": 30.0,
    "mrPerLevel": 1.25,
    "baseAttackSpeed": 0.60,
    "attackSpeedRatio": 0.60,
    "attackSpeedPerLevel": 0.025,
    "baseAttackRange": 5.5,
    "baseMoveSpeed": 5.0,
    "navArriveRadius": 0.15,
    "spatialRadius": 0.75,
    "sightRange": 19.0,
}

STAGE_FIELDS = {
    "lockDurationSec": 0.6,
}


def fail(message: str) -> None:
    raise SystemExit(f"[ChampionGameData] {message}")


def as_float(value: object, path: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        fail(f"{path} must be a number")
    result = float(value)
    if not math.isfinite(result):
        fail(f"{path} must be finite")
    return result


def as_int(value: object, path: str) -> int:
    if not isinstance(value, int):
        fail(f"{path} must be an integer")
    return value


def as_enum_name(value: object, path: str) -> str:
    if not isinstance(value, str) or not value:
        fail(f"{path} must be a non-empty enum name")
    return value


def cpp_float(value: float) -> str:
    if value == 0.0:
        return "0.f"
    text = f"{value:.8f}".rstrip("0").rstrip(".")
    if "." not in text:
        return f"{text}.f"
    return f"{text}f"


def enum_value(enum_type: str, raw: str) -> str:
    return f"{enum_type}::{raw}"


def normalize_stage(stage: dict, path: str) -> dict:
    if not isinstance(stage, dict):
        fail(f"{path} must be an object")
    return {
        key: as_float(stage.get(key, default), f"{path}.{key}")
        for key, default in STAGE_FIELDS.items()
    }


def normalize_rank_values(
        skill: dict,
        field: str,
        slot: int,
        path: str,
        default: float) -> list[float]:
    expected_count = 1 if slot == 0 else (3 if slot == 4 else 5)
    array_field = f"{field}ByRank"
    if array_field in skill:
        source = skill[array_field]
        if not isinstance(source, list) or len(source) != expected_count:
            fail(f"{path}.{array_field} must contain {expected_count} values")
        values = [
            as_float(value, f"{path}.{array_field}[{index}]")
            for index, value in enumerate(source)
        ]
    else:
        value = as_float(skill.get(field, default), f"{path}.{field}")
        values = [value] * expected_count

    if any(value < 0.0 for value in values):
        fail(f"{path}.{array_field} values must be non-negative")
    return values


def normalize_skill(skill: dict, champion: str, index: int) -> dict:
    path = f"champions[{champion}].skills[{index}]"
    if not isinstance(skill, dict):
        fail(f"{path} must be an object")

    slot = as_int(skill.get("slot", index), f"{path}.slot")
    if slot < 0 or slot >= SKILL_SLOT_COUNT:
        fail(f"{path}.slot must be 0..{SKILL_SLOT_COUNT - 1}")

    stage_count = as_int(skill.get("stageCount", 1), f"{path}.stageCount")
    if stage_count < 1 or stage_count > SKILL_STAGE_MAX:
        fail(f"{path}.stageCount must be 1..{SKILL_STAGE_MAX}")

    raw_stages = skill.get("stages", [])
    if not isinstance(raw_stages, list):
        fail(f"{path}.stages must be an array")

    stages = []
    for stage_index in range(SKILL_STAGE_MAX):
        source = raw_stages[stage_index] if stage_index < len(raw_stages) else {}
        stages.append(normalize_stage(source, f"{path}.stages[{stage_index}]"))

    cooldown_by_rank = normalize_rank_values(
        skill, "cooldownSec", slot, path, 0.0)
    mana_by_rank = normalize_rank_values(
        skill, "manaCost", slot, path, 0.0)

    return {
        "slot": slot,
        "targetMode": as_enum_name(skill.get("targetMode", "Self"), f"{path}.targetMode"),
        "stageCount": stage_count,
        "stageWindowSec": as_float(skill.get("stageWindowSec", 0.0), f"{path}.stageWindowSec"),
        "cooldownSec": cooldown_by_rank[0],
        "cooldownSecByRank": cooldown_by_rank,
        "rangeMax": as_float(skill.get("rangeMax", 0.0), f"{path}.rangeMax"),
        "manaCost": mana_by_rank[0],
        "manaCostByRank": mana_by_rank,
        "skillId": as_int(skill.get("skillId", 0), f"{path}.skillId"),
        "scalingTableId": as_int(skill.get("scalingTableId", 0), f"{path}.scalingTableId"),
        "gameplayPolicyId": as_int(skill.get("gameplayPolicyId", 0), f"{path}.gameplayPolicyId"),
        "visualCueId": as_int(skill.get("visualCueId", 0), f"{path}.visualCueId"),
        "stages": stages,
    }


def normalize_spell(spell: dict, index: int) -> dict:
    path = f"summonerSpells[{index}]"
    if not isinstance(spell, dict):
        fail(f"{path} must be an object")
    return {
        "spellId": as_int(spell.get("spellId", 0), f"{path}.spellId"),
        "rangeMax": as_float(spell.get("rangeMax", 0.0), f"{path}.rangeMax"),
        "cooldownSec": as_float(spell.get("cooldownSec", 0.0), f"{path}.cooldownSec"),
        "gameplayPolicyId": as_int(spell.get("gameplayPolicyId", 0), f"{path}.gameplayPolicyId"),
        "visualCueId": as_int(spell.get("visualCueId", 0), f"{path}.visualCueId"),
    }


def normalize_passive_dash(passive_dash: object, champion: str) -> dict | None:
    if passive_dash is None:
        return None
    path = f"champions[{champion}].passiveDash"
    if not isinstance(passive_dash, dict):
        fail(f"{path} must be an object")
    return {
        "distance": as_float(passive_dash.get("distance", 0.0), f"{path}.distance"),
        "durationSec": as_float(passive_dash.get("durationSec", 0.0), f"{path}.durationSec"),
        "inputGraceSec": as_float(passive_dash.get("inputGraceSec", 0.0), f"{path}.inputGraceSec"),
    }


def normalize_passive_soul(passive_soul: object, champion: str) -> dict | None:
    if passive_soul is None:
        return None
    if not isinstance(passive_soul, dict):
        fail(f"champions[{champion}].passiveSoul must be an object")
    path = f"champions[{champion}].passiveSoul"
    return {
        "lifetimeSec": as_float(passive_soul.get("lifetimeSec"), f"{path}.lifetimeSec"),
        "radius": as_float(passive_soul.get("radius"), f"{path}.radius"),
    }


def normalize_champion(champion: dict, index: int) -> dict:
    if not isinstance(champion, dict):
        fail(f"champions[{index}] must be an object")

    name = as_enum_name(champion.get("champion"), f"champions[{index}].champion")

    stats_source = champion.get("stats", {})
    if not isinstance(stats_source, dict):
        fail(f"champions[{name}].stats must be an object")
    stats = {
        key: as_float(stats_source.get(key, default), f"champions[{name}].stats.{key}")
        for key, default in STAT_FIELDS.items()
    }

    raw_skills = champion.get("skills", [])
    if not isinstance(raw_skills, list):
        fail(f"champions[{name}].skills must be an array")
    skills = [normalize_skill(skill, name, idx) for idx, skill in enumerate(raw_skills)]

    used_slots = set()
    for skill in skills:
        if skill["slot"] in used_slots:
            fail(f"champions[{name}].skills has duplicated slot {skill['slot']}")
        used_slots.add(skill["slot"])

    return {
        "champion": name,
        "dataVersion": as_int(champion.get("dataVersion", 1), f"champions[{name}].dataVersion"),
        "stats": stats,
        "skills": skills,
        "passiveDash": normalize_passive_dash(champion.get("passiveDash"), name),
        "passiveSoul": normalize_passive_soul(champion.get("passiveSoul"), name),
    }


def normalize_root(root: dict) -> dict:
    if not isinstance(root, dict):
        fail("root must be an object")

    champions = root.get("champions", [])
    if not isinstance(champions, list) or not champions:
        fail("champions must be a non-empty array")

    normalized_champions = [normalize_champion(champion, index) for index, champion in enumerate(champions)]
    seen = set()
    for champion in normalized_champions:
        name = champion["champion"]
        if name in seen:
            fail(f"duplicated champion: {name}")
        seen.add(name)

    return {
        "schemaVersion": as_int(root.get("schemaVersion", 1), "schemaVersion"),
        "champions": normalized_champions,
    }


def compute_hash(data: dict) -> int:
    stable = json.dumps(data, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return int(hashlib.sha256(stable).hexdigest()[:8], 16)


def emit_header() -> str:
    return """#pragma once

#include <cstddef>

#include "Shared/GameSim/Definitions/ChampionGameData.h"

namespace ChampionGameDataGenerated
{
    u32_t GetBuildHash();
    std::size_t GetChampionCount();
    const ChampionGameData* GetChampionTable();
    const ChampionGameData* FindChampion(eChampion champion);
}
"""


def append_skill(lines: list[str], skill: dict) -> None:
    slot = skill["slot"]
    lines.append(f"        auto& skill{slot} = data.skills[{slot}];")
    lines.append(f"        skill{slot}.bValid = true;")
    lines.append(f"        skill{slot}.slot = {slot}u;")
    lines.append(f"        skill{slot}.targetMode = {enum_value('eTargetMode', skill['targetMode'])};")
    lines.append(f"        skill{slot}.stageCount = {skill['stageCount']}u;")
    lines.append(f"        skill{slot}.stageWindowSec = {cpp_float(skill['stageWindowSec'])};")
    lines.append(f"        skill{slot}.rankCount = {len(skill['cooldownSecByRank'])}u;")
    for rank, value in enumerate(skill["cooldownSecByRank"]):
        lines.append(f"        skill{slot}.cooldownSecByRank[{rank}] = {cpp_float(value)};")
    for rank, value in enumerate(skill["manaCostByRank"]):
        lines.append(f"        skill{slot}.manaCostByRank[{rank}] = {cpp_float(value)};")
    lines.append(f"        skill{slot}.cooldownSec = {cpp_float(skill['cooldownSec'])};")
    lines.append(f"        skill{slot}.rangeMax = {cpp_float(skill['rangeMax'])};")
    lines.append(f"        skill{slot}.manaCost = {cpp_float(skill['manaCost'])};")
    lines.append(f"        skill{slot}.skillId = {skill['skillId']}u;")
    lines.append(f"        skill{slot}.scalingTableId = {skill['scalingTableId']}u;")
    lines.append(f"        skill{slot}.gameplayPolicyId = {skill['gameplayPolicyId']}u;")
    lines.append(f"        skill{slot}.visualCueId = {skill['visualCueId']}u;")

    for stage_index, stage in enumerate(skill["stages"]):
        lines.append(f"        auto& stage{slot}_{stage_index} = skill{slot}.stages[{stage_index}];")
        lines.append(f"        stage{slot}_{stage_index}.lockDurationSec = {cpp_float(stage['lockDurationSec'])};")


def append_passive_dash(lines: list[str], passive_dash: dict | None) -> None:
    if passive_dash is None:
        return
    lines.append("        data.passiveDash.bValid = true;")
    lines.append(f"        data.passiveDash.distance = {cpp_float(passive_dash['distance'])};")
    lines.append(f"        data.passiveDash.durationSec = {cpp_float(passive_dash['durationSec'])};")
    lines.append(f"        data.passiveDash.inputGraceSec = {cpp_float(passive_dash['inputGraceSec'])};")


def emit_cpp(data: dict, build_hash: int) -> str:
    lines: list[str] = [
        '#include "Shared/GameSim/Generated/ChampionGameData.generated.h"',
        "",
        "namespace",
        "{",
        f"    inline constexpr u32_t kGeneratedChampionGameDataBuildHash = 0x{build_hash:08X}u;",
        "",
    ]

    for champion in data["champions"]:
        name = champion["champion"]
        lines.append(f"    ChampionGameData MakeChampion_{name}()")
        lines.append("    {")
        lines.append("        ChampionGameData data{};")
        lines.append("        data.bValid = true;")
        lines.append(f"        data.champion = {enum_value('eChampion', name)};")
        lines.append(f"        data.dataVersion = {champion['dataVersion']}u;")
        lines.append("        data.authoringHash = kGeneratedChampionGameDataBuildHash;")
        lines.append(f"        data.stats.championId = {enum_value('eChampion', name)};")
        for key, value in champion["stats"].items():
            lines.append(f"        data.stats.{key} = {cpp_float(value)};")
        for skill in champion["skills"]:
            append_skill(lines, skill)
        append_passive_dash(lines, champion["passiveDash"])
        lines.append("        return data;")
        lines.append("    }")
        lines.append("")

    lines.append("    const ChampionGameData kChampionGameDataTable[] =")
    lines.append("    {")
    for champion in data["champions"]:
        lines.append(f"        MakeChampion_{champion['champion']}(),")
    lines.append("    };")
    lines.append("}")
    lines.append("")
    lines.append("namespace ChampionGameDataGenerated")
    lines.append("{")
    lines.append("    u32_t GetBuildHash()")
    lines.append("    {")
    lines.append("        return kGeneratedChampionGameDataBuildHash;")
    lines.append("    }")
    lines.append("")
    lines.append("    std::size_t GetChampionCount()")
    lines.append("    {")
    lines.append("        return sizeof(kChampionGameDataTable) / sizeof(kChampionGameDataTable[0]);")
    lines.append("    }")
    lines.append("")
    lines.append("    const ChampionGameData* GetChampionTable()")
    lines.append("    {")
    lines.append("        return kChampionGameDataTable;")
    lines.append("    }")
    lines.append("")
    lines.append("    const ChampionGameData* FindChampion(eChampion champion)")
    lines.append("    {")
    lines.append("        for (const ChampionGameData& data : kChampionGameDataTable)")
    lines.append("        {")
    lines.append("            if (data.champion == champion)")
    lines.append("            {")
    lines.append("                return &data;")
    lines.append("            }")
    lines.append("        }")
    lines.append("")
    lines.append("        return nullptr;")
    lines.append("    }")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument(
        "--check", action="store_true",
        help="fail if generated files are stale relative to champions.json")
    args = parser.parse_args()

    root = args.root.resolve()
    source = root / "Data" / "Gameplay" / "ChampionGameData" / "champions.json"
    out_dir = root / "Shared" / "GameSim" / "Generated"

    if not source.exists():
        fail(f"missing source file: {source}")

    raw = json.loads(source.read_text(encoding="utf-8"))
    data = normalize_root(raw)
    build_hash = compute_hash(data)

    header_text = emit_header()
    cpp_text = emit_cpp(data, build_hash)

    if args.check:
        header_path = out_dir / "ChampionGameData.generated.h"
        cpp_path = out_dir / "ChampionGameData.generated.cpp"
        for path, expected in ((header_path, header_text), (cpp_path, cpp_text)):
            if not path.exists() or path.read_text(encoding="utf-8") != expected:
                fail(f"stale generated file (rerun build_champion_game_data.py): {path}")
        print(f"Check passed: generated files match champions.json (hash 0x{build_hash:08X})")
        return 0

    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "ChampionGameData.generated.h").write_text(header_text, encoding="utf-8", newline="\n")
    (out_dir / "ChampionGameData.generated.cpp").write_text(cpp_text, encoding="utf-8", newline="\n")

    print(f"Generated ChampionGameData.generated.* from {source}")
    print(f"Champion count: {len(data['champions'])}")
    print(f"Build hash: 0x{build_hash:08X}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
