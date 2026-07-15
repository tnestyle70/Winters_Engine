from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
UI_TEXTURE_DIR = ROOT / "Client" / "Bin" / "Resource" / "Texture" / "UI"
ITEM_ICON_DIR = UI_TEXTURE_DIR / "Items"
ITEM_DEF = ROOT / "Shared" / "GameSim" / "Definitions" / "ItemDef.h"
REGISTRY_SOURCE = ROOT / "Client" / "Private" / "GamePlay" / "LoLUIContentRegistry.cpp"
RUNTIME_CATALOG = ROOT / "Client" / "Bin" / "Resource" / "UI" / "Lua" / "itemshop_catalog.lua"
ATLAS_MANIFEST = ROOT / "Client" / "Bin" / "Resource" / "UI" / "itemshop_atlas_manifest.json"

EXPECTED_REFERENCE_SIZES = {(1165, 736), (1132, 729)}
# 34-item SR-only catalog: starters -> boots -> components -> legendaries.
# Verified purchasable on map 11 against Data Dragon 16.13.1 (2026-07-14).
EXPECTED_CATALOG = (
    (1055, "1055_marksman_t1_doransblade.png"),
    (1056, "1056_mage_t1_doransring.png"),
    (1054, "1054_tank_t1_doransshield.png"),
    (1001, "1001_class_t1_bootsofspeed.png"),
    (3006, "3006_class_t2_berserkersgreaves.png"),
    (3020, "3020_class_t2_sorcerersshoes.png"),
    (3047, "3047_class_t2_ninjatabi.png"),
    (3111, "3111_class_t2_mercurystreads.png"),
    (3158, "3158_class_t2_ionianbootsoflucidity.png"),
    (1036, "1036_class_t1_longsword.png"),
    (1042, "1042_base_t1_dagger.png"),
    (1043, "1043_base_t2_recurvebow.png"),
    (1052, "1052_mage_t2_amptome.png"),
    (1053, "1053_fighter_t2_vampiricscepter.png"),
    (1028, "1028_base_t1_rubycrystal.png"),
    (1029, "1029_base_t1_clotharmor.png"),
    (1031, "1031_base_t2_chainvest.png"),
    (1033, "1033_base_t1_magicmantle.png"),
    (1057, "1057_tank_t2_negatroncloak.png"),
    (1011, "1011_class_t2_giantsbelt.png"),
    (1018, "1018_base_t1_cloakagility.png"),
    (1026, "1026_mage_t1_blastingwand.png"),
    (1027, "1027_base_t1_saphirecrystal.png"),
    (1037, "1037_class_t1_pickaxe.png"),
    (1058, "1058_mage_t1_largerod.png"),
    (1038, "1038_marksman_t1_bfsword.png"),
    (3031, "3031_marksman_t3_infinityedge.png"),
    (3072, "3072_fighter_t3_bloodthirster.png"),
    (3078, "3078_fighter_t4_trinityforce.png"),
    (3153, "3153_fighter_t3_bladeoftheruinedking.png"),
    (3089, "3089_mage_t3_deathcap.png"),
    (3157, "3157_mage_t3_zhonyashourglass.png"),
    (3065, "3065_tank_t3_spiritvisage.png"),
    (3742, "3742_tank_t3_deadmansplate.png"),
)


class ValidationError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValidationError(message)


def parse_registered_items() -> dict[int, tuple[int, str]]:
    text = ITEM_DEF.read_text(encoding="utf-8", errors="ignore")
    pattern = re.compile(
        r'\{\s*(\d+)\s*,\s*(\d+)\s*,\s*ItemStatModifier\{.*?\}\s*,\s*"([^"]+)"\s*\}',
        re.DOTALL,
    )
    return {
        int(item_id): (int(price), name)
        for item_id, price, name in pattern.findall(text)
    }


def parse_runtime_catalog_entries() -> list[tuple[int, str, str, str, str]]:
    text = REGISTRY_SOURCE.read_text(encoding="utf-8")
    begin = text.find("constexpr LoLShopCatalogEntry kLoLShopCatalog[]")
    end = text.find("void RegisterLoLShopItems", begin)
    require(begin >= 0 and end > begin, "kLoLShopCatalog source block was not found")

    pattern = re.compile(
        r'\{\s*(\d+)\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*L"([^"]+)"\s*,\s*"([^"]+)"\s*\}'
    )
    return [
        (int(item_id), asset_key, section, icon_path, sprite)
        for item_id, asset_key, section, icon_path, sprite in pattern.findall(text[begin:end])
    ]


def find_reference_sizes() -> set[tuple[int, int]]:
    sizes: set[tuple[int, int]] = set()
    for path in UI_TEXTURE_DIR.glob("*.png"):
        try:
            with Image.open(path) as image:
                if image.size in EXPECTED_REFERENCE_SIZES:
                    sizes.add(image.size)
        except OSError:
            continue
    return sizes


def validate() -> list[str]:
    expected_ids = [item_id for item_id, _ in EXPECTED_CATALOG]
    expected_keys = [asset_key for _, asset_key in EXPECTED_CATALOG]
    registered = parse_registered_items()
    entries = parse_runtime_catalog_entries()

    require(len(entries) == len(EXPECTED_CATALOG), f"catalog entry count is {len(entries)}, expected {len(EXPECTED_CATALOG)}")
    require([entry[0] for entry in entries] == expected_ids, "catalog item order differs from the curated order")
    require([entry[1] for entry in entries] == expected_keys, "catalog asset-key order differs from the curated list")
    require(len(set(expected_ids)) == len(expected_ids), "curated item IDs are not unique")
    require(set(expected_ids) == set(registered), "ItemDef IDs and curated catalog IDs differ")
    require(all(entry[2] == "legacy" for entry in entries), "catalog entries must use the flat legacy layout")

    manifest = json.loads(ATLAS_MANIFEST.read_text(encoding="utf-8"))
    sprites = manifest.get("sprites", {})
    for item_id, asset_key, _, icon_path, sprite in entries:
        require(item_id in registered, f"item {item_id} is not registered in ItemDef")
        require(icon_path == f"Resource/Texture/UI/Items/{asset_key}", f"item {item_id} icon path does not match its asset key")
        require((ITEM_ICON_DIR / asset_key).is_file(), f"item {item_id} icon is missing: {asset_key}")
        require(sprite == f"item:{asset_key}", f"item {item_id} atlas sprite key is invalid")
        require(sprite in sprites, f"item {item_id} atlas sprite is missing: {sprite}")
        require(sprites[sprite].get("texture") == "items", f"item {item_id} atlas sprite uses the wrong texture")

    require(find_reference_sizes() == EXPECTED_REFERENCE_SIZES, "상점1/상점2 reference dimensions were not both found")

    lua = RUNTIME_CATALOG.read_text(encoding="utf-8")
    required_lua = (
        "local items = UI.GetShopItems()",
        "item.x = 172 + (zeroBased % 10) * 56",
        "item.y = 201 + math.floor(zeroBased / 10) * 114",
        "items = items",
    )
    for token in required_lua:
        require(token in lua, f"runtime catalog is missing: {token}")
    require("{ id =" not in lua, "runtime catalog contains hardcoded item rows")

    return [
        f"{item_id}:{registered[item_id][0]}:{registered[item_id][1]}:{asset_key}"
        for item_id, asset_key in EXPECTED_CATALOG
    ]


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate the 15-item server-authoritative Winters shop catalog without writing runtime files."
    )
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Compatibility alias; validation is always performed and no files are written.",
    )
    parser.parse_args()

    try:
        rows = validate()
    except (OSError, json.JSONDecodeError, ValidationError) as error:
        print(f"item shop catalog validation failed: {error}", file=sys.stderr)
        return 1

    count = len(EXPECTED_CATALOG)
    print("item shop catalog validation passed")
    print(
        f"entries={count} uniqueIds={count} authoritativeDefs={count} "
        f"atlasSprites={count} referenceImages=2")
    for row in rows:
        print(row)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
