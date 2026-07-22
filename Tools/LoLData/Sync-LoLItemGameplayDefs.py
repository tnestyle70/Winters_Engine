#!/usr/bin/env python3
"""Synchronize Winters item base stats with a pinned Riot Data Dragon patch."""

from __future__ import annotations

import argparse
import html
import json
import re
import sys
import urllib.request
from collections import Counter
from pathlib import Path


DEFAULT_PATCH = "16.14.1"
DEFAULT_MAP_ID = 11
RUNTIME_ONLY_ITEM_IDS = {3042, 3340, 3599}
PRESERVED_GAMEPLAY_FIELDS = (
    "onHitDamage",
    "spellblade",
    "manaflow",
    "lightshieldStrike",
    "active",
    "maxManaBonusAdRatio",
)

STAT_ORDER = (
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

DDRAGON_STAT_FIELDS = {
    "FlatPhysicalDamageMod": "flatAd",
    "FlatMagicDamageMod": "flatAp",
    "FlatHPPoolMod": "flatHealth",
    "FlatMPPoolMod": "flatMana",
    "FlatArmorMod": "flatArmor",
    "FlatSpellBlockMod": "flatMr",
    "PercentAttackSpeedMod": "bonusAttackSpeed",
    "FlatCritChanceMod": "critChance",
    "PercentMovementSpeedMod": "percentMoveSpeed",
    "FlatMovementSpeedMod": "flatMoveSpeed",
    "PercentLifeStealMod": "lifeSteal",
}

PERCENT_LABELS = {
    "Attack Speed": "bonusAttackSpeed",
    "Critical Strike Chance": "critChance",
    "Critical Strike Damage": "critDamageBonus",
    "Life Steal": "lifeSteal",
    "Armor Penetration": "armorPenPercent",
}

UNSUPPORTED_DIRECT_STAT_LABELS = {
    "Base Health Regen",
    "Base Mana Regen",
    "Heal and Shield Power",
    "Omnivamp",
    "Tenacity",
    "Gold Per 10 Seconds",
}


def fetch_json(url: str) -> dict:
    request = urllib.request.Request(url, headers={"User-Agent": "Winters-LoLDataSync/1.0"})
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)


def is_supported_shop_item(item_id: int, item: dict, map_id: int) -> bool:
    gold = item.get("gold", {})
    maps = item.get("maps", {})
    is_shop_item = (
        0 < item_id <= 0xFFFF
        and gold.get("purchasable") is True
        and int(gold.get("total", 0)) > 0
        and maps.get(str(map_id)) is True
        and item.get("inStore") is not False
        and item.get("hideFromAll") is not True
    )
    is_runtime_only = (
        item_id in RUNTIME_ONLY_ITEM_IDS
        and 0 < item_id <= 0xFFFF
    )
    return is_shop_item or is_runtime_only


def parse_stats_block(description: str) -> list[tuple[float, bool, str]]:
    match = re.search(r"<stats>(.*?)</stats>", description or "", flags=re.IGNORECASE | re.DOTALL)
    if not match:
        return []

    rows: list[tuple[float, bool, str]] = []
    for fragment in re.split(r"<br\s*/?>", match.group(1), flags=re.IGNORECASE):
        plain = html.unescape(re.sub(r"<[^>]+>", "", fragment)).strip()
        value_match = re.fullmatch(r"([+-]?\d+(?:\.\d+)?)\s*(%)?\s+(.+?)", plain)
        if not value_match:
            continue
        rows.append(
            (
                float(value_match.group(1)),
                value_match.group(2) == "%",
                value_match.group(3).strip(),
            )
        )
    return rows


def assign_description_stat(
    stats: dict[str, float],
    value: float,
    is_percent: bool,
    label: str,
) -> bool:
    if label in PERCENT_LABELS and is_percent:
        stats[PERCENT_LABELS[label]] = value / 100.0
        return True
    if label == "Ability Haste" and not is_percent:
        stats["abilityHaste"] = value
        return True
    if label == "Lethality" and not is_percent:
        stats["lethality"] = value
        return True
    if label == "Move Speed":
        stats["percentMoveSpeed" if is_percent else "flatMoveSpeed"] = (
            value / 100.0 if is_percent else value
        )
        return True
    if label == "Magic Penetration":
        stats["magicPenPercent" if is_percent else "flatMagicPen"] = (
            value / 100.0 if is_percent else value
        )
        return True
    return label in {
        "Attack Damage",
        "Ability Power",
        "Health",
        "Mana",
        "Armor",
        "Magic Resist",
    }


def build_item_stats(item: dict, unsupported: Counter[str]) -> dict[str, float]:
    stats: dict[str, float] = {}
    for data_dragon_name, value in item.get("stats", {}).items():
        winters_name = DDRAGON_STAT_FIELDS.get(data_dragon_name)
        if winters_name is not None and float(value) != 0.0:
            stats[winters_name] = float(value)

    for value, is_percent, label in parse_stats_block(item.get("description", "")):
        if assign_description_stat(stats, value, is_percent, label):
            continue
        unsupported[label] += 1

    return {name: stats[name] for name in STAT_ORDER if stats.get(name, 0.0) != 0.0}


def load_preserved_gameplay(path: Path) -> dict[int, dict]:
    if not path.exists():
        return {}
    current = json.loads(path.read_text(encoding="utf-8"))
    preserved: dict[int, dict] = {}
    for item in current.get("items", []):
        fields = {
            field: item[field]
            for field in PRESERVED_GAMEPLAY_FIELDS
            if item.get(field) is not None
        }
        if fields:
            preserved[int(item["itemId"])] = fields
    return preserved


def build_document(payload: dict, patch: str, map_id: int, output: Path) -> tuple[dict, Counter[str]]:
    preserved_gameplay = load_preserved_gameplay(output)
    unsupported: Counter[str] = Counter()
    records = []
    for raw_id, item in payload.get("data", {}).items():
        item_id = int(raw_id)
        if not is_supported_shop_item(item_id, item, map_id):
            continue
        record = {
            "itemId": item_id,
            "price": int(item["gold"]["total"]),
            "name": item["name"],
            "stats": build_item_stats(item, unsupported),
            "onHitDamage": None,
        }
        record.update(preserved_gameplay.get(item_id, {}))
        if item["gold"].get("purchasable") is not True or item.get("inStore") is False:
            record["purchasable"] = False
        records.append(record)

    return (
        {
            "schemaVersion": 1,
            "dataVersion": 2,
            "sourcePatch": patch,
            "sourceMapId": map_id,
            "items": sorted(records, key=lambda record: record["itemId"]),
        },
        unsupported,
    )


def icon_ids(icon_dir: Path) -> set[int]:
    ids: set[int] = set()
    if not icon_dir.is_dir():
        return ids
    for path in icon_dir.glob("*.png"):
        match = re.match(r"(\d+)", path.stem)
        if match:
            ids.add(int(match.group(1)))
    return ids


def download_missing_icons(document: dict, patch: str, icon_dir: Path) -> int:
    icon_dir.mkdir(parents=True, exist_ok=True)
    existing = icon_ids(icon_dir)
    downloaded = 0
    for item in document["items"]:
        item_id = int(item["itemId"])
        if item_id in existing:
            continue
        destination = icon_dir / f"{item_id}_ddragon.png"
        url = f"https://ddragon.leagueoflegends.com/cdn/{patch}/img/item/{item_id}.png"
        request = urllib.request.Request(url, headers={"User-Agent": "Winters-LoLDataSync/1.0"})
        with urllib.request.urlopen(request, timeout=30) as response:
            destination.write_bytes(response.read())
        existing.add(item_id)
        downloaded += 1
    return downloaded


def canonical_json(document: dict) -> str:
    return json.dumps(document, ensure_ascii=False, indent=2) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--patch", default=DEFAULT_PATCH)
    parser.add_argument("--map-id", type=int, default=DEFAULT_MAP_ID)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--download-missing-icons", action="store_true")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[2]
    output = root / "Data/LoL/ServerPrivate/Gameplay/ItemGameplayDefs.json"
    icon_dir = root / "Client/Bin/Resource/Texture/UI/Items"
    url = f"https://ddragon.leagueoflegends.com/cdn/{args.patch}/data/en_US/item.json"
    document, unsupported = build_document(fetch_json(url), args.patch, args.map_id, output)
    expected = canonical_json(document)

    if args.check:
        actual = output.read_text(encoding="utf-8") if output.exists() else ""
        if actual != expected:
            print(f"FAIL: {output} differs from Riot Data Dragon {args.patch}", file=sys.stderr)
            return 1
    else:
        output.write_text(expected, encoding="utf-8", newline="\n")

    downloaded = 0
    if args.download_missing_icons:
        downloaded = download_missing_icons(document, args.patch, icon_dir)

    unsupported_summary = ", ".join(
        f"{label}={count}" for label, count in sorted(unsupported.items())
        if label in UNSUPPORTED_DIRECT_STAT_LABELS
    )
    print(
        f"PASS: patch={args.patch} map={args.map_id} items={len(document['items'])} "
        f"iconsDownloaded={downloaded}"
    )
    if unsupported_summary:
        print("Not projected (runtime stat system has no matching field): " + unsupported_summary)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
