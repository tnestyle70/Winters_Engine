from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "Data" / "Gameplay" / "ChampionGameData" / "champions.json"
GENERATOR = ROOT / "Tools" / "LoLData" / "Build-LoLDefinitionPack.py"


def load_generator():
    spec = importlib.util.spec_from_file_location("build_lol_definition_pack", GENERATOR)
    if spec is None or spec.loader is None:
        raise SystemExit(f"unable to load {GENERATOR}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def validate(module, document: dict) -> None:
    module.validate_source_schema(ROOT, SOURCE, document)
    module.legacy.normalize_root(copy.deepcopy(document))


def expect_rejected(module, name: str, document: dict) -> None:
    try:
        validate(module, document)
    except SystemExit:
        return
    raise SystemExit(f"[ChampionSchemaContract] {name} was accepted")


def find_skill(document: dict, champion: str, slot: int) -> dict:
    champion_data = next(
        item for item in document["champions"] if item["champion"] == champion)
    return next(item for item in champion_data["skills"] if item["slot"] == slot)


def main() -> int:
    module = load_generator()
    canonical = json.loads(SOURCE.read_text(encoding="utf-8"))
    validate(module, canonical)

    by_rank_only = copy.deepcopy(canonical)
    ezreal_q = find_skill(by_rank_only, "EZREAL", 1)
    if "cooldownSec" in ezreal_q or "manaCost" in ezreal_q:
        raise SystemExit("[ChampionSchemaContract] EZREAL Q is no longer ByRank-only")
    validate(module, by_rank_only)

    unknown_stage_field = copy.deepcopy(canonical)
    find_skill(unknown_stage_field, "IRELIA", 2)["stages"][0]["unexpected"] = 1
    expect_rejected(module, "nested unknown stage field", unknown_stage_field)

    invalid_two_stage = copy.deepcopy(canonical)
    invalid_w = find_skill(invalid_two_stage, "IRELIA", 2)
    invalid_w["stages"] = invalid_w["stages"][:1]
    invalid_w["activationMode"] = "Press"
    expect_rejected(module, "invalid two-stage shape/activation", invalid_two_stage)

    missing_charge = copy.deepcopy(canonical)
    find_skill(missing_charge, "IRELIA", 2).pop("charge")
    expect_rejected(module, "PressRelease without charge", missing_charge)

    mismatched_target = copy.deepcopy(canonical)
    find_skill(mismatched_target, "IRELIA", 2)["targetMode"] = "Self"
    expect_rejected(module, "top-level/stage target mismatch", mismatched_target)

    print(
        "[ChampionSchemaContract] PASS: canonical + ByRank accepted; "
        "nested/stage/charge/target mutations rejected")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
