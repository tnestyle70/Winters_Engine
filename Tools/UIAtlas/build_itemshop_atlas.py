"""Rebuild the in-game item shop icon atlas from Summoner's Rift-only icons.

Reads Client/Bin/Resource/Texture/UI/Items/*.png, filters out non-SR icons
(special modes, removed legacy items, dev/test files), packs the survivors
into item_icons_atlas.png (64x64 grid), and rewrites only the "items"-texture
sprites inside itemshop_atlas_manifest.json. The "shop" texture and its
sprites are preserved verbatim.

Filter provenance: repo icon audit (2026-07-14) + Riot Data Dragon 16.13.1
maps["11"] purchasable check. See S032 result report.

Usage: python Tools/UIAtlas/build_itemshop_atlas.py [--root <repo>] [--check]
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path

from PIL import Image

CELL = 64
COLUMNS = 16  # 1024 / 64

# Arena/URF/ARAM/Nexus Blitz/Ornn masterwork/champion-exclusive/soul-juice ids.
EXCLUDED_SPECIAL_IDS = {
    1504, 1507, 1508, 1509, 1510, 1511, 1512, 1524,
    2019, 2051, 2052, 2319, 2420, 2422, 2503, 2508,
    2510, 2512, 2517, 2520, 2522, 2523, 2524, 2525, 2526, 2530,
    3112, 3177, 3513, 3599, 3600, 3683, 3901, 3902, 3903,
    4001, 4003, 4004, 4012, 4013, 4015, 4017, 4403,
    6700, 6701, 6702,
    7000, 7003, 7100, 7101, 7102, 7103, 7105, 7106, 7107, 7108,
    7110, 7111, 7112, 7113,
    220000, 220001, 220002, 220003, 220004, 220005, 220006, 220007,
    223069,
    443080, 443081, 443090,
    447114, 447115, 447116, 447117, 447118, 447119, 447120, 447121,
    447122, 447123,
}

# Removed-from-SR legacy items (wiki/ddragon verified not purchasable on map 11).
EXCLUDED_LEGACY_IDS = {
    96, 1004, 1040, 1402, 2012, 2020, 2022, 2033, 2048, 2049,
    3001, 3002, 3005, 3010, 3012, 3022, 3023, 3032, 3042,
    3054, 3055, 3056, 3058, 3059, 3061, 3062, 3063, 3064,
    3069, 3073, 3095, 3128, 3131, 3144, 3146, 3147, 3181,
    3194, 3348, 3380, 3385, 3430,
    4636, 6630, 6632, 6656, 6664, 6671, 6693,
}

# Per-filename exclusions where an id is shared with a keeper (e.g. 1103 jungle pet).
EXCLUDED_FILENAMES = {
    "1103_testitem2.png",
    "1111_jarvanis.kiwi_15_24_balance.png",
    "34.png",
}

EXCLUDED_IDS = EXCLUDED_SPECIAL_IDS | EXCLUDED_LEGACY_IDS


def leading_id(name: str) -> int | None:
    head = name.split("_", 1)[0].split(".", 1)[0]
    return int(head) if head.isdigit() else None


def collect_icons(items_dir: Path) -> tuple[list[Path], list[str]]:
    kept: list[Path] = []
    dropped: list[str] = []
    for path in sorted(items_dir.iterdir()):
        if path.is_dir():
            dropped.append(path.name + "/ (subfolder: dev/special-mode source)")
            continue
        if path.suffix.lower() != ".png":
            dropped.append(path.name + " (not png)")
            continue
        if path.name in EXCLUDED_FILENAMES:
            dropped.append(path.name + " (test/abnormal filename)")
            continue
        item_id = leading_id(path.name)
        if item_id is None:
            dropped.append(path.name + " (no item id: special asset)")
            continue
        if item_id in EXCLUDED_IDS:
            dropped.append(path.name + f" (id {item_id} not SR-purchasable)")
            continue
        kept.append(path)
    return kept, dropped


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[2]))
    parser.add_argument(
        "--check", action="store_true",
        help="report the filter result without writing outputs")
    args = parser.parse_args()

    root = Path(args.root)
    items_dir = root / "Client/Bin/Resource/Texture/UI/Items"
    manifest_path = root / "Client/Bin/Resource/UI/itemshop_atlas_manifest.json"
    atlas_rel = "Resource/Texture/UI/item_icons_atlas.png"
    atlas_path = root / "Client/Bin" / atlas_rel

    if not items_dir.is_dir() or not manifest_path.is_file():
        print(f"FAIL: missing inputs ({items_dir}, {manifest_path})")
        return 1

    kept, dropped = collect_icons(items_dir)
    rows = (len(kept) + COLUMNS - 1) // COLUMNS
    height = 1
    while height < rows * CELL:
        height *= 2

    print(f"kept {len(kept)} icons, dropped {len(dropped)}, atlas 1024x{height}")
    if args.check:
        for line in dropped:
            print("  drop:", line)
        return 0

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    sprites = {
        key: value
        for key, value in manifest["sprites"].items()
        if value.get("texture") != "items"
    }

    atlas = Image.new("RGBA", (COLUMNS * CELL, height), (0, 0, 0, 0))
    for index, path in enumerate(kept):
        x = (index % COLUMNS) * CELL
        y = (index // COLUMNS) * CELL
        with Image.open(path) as icon:
            icon = icon.convert("RGBA")
            if icon.size != (CELL, CELL):
                icon = icon.resize((CELL, CELL), Image.LANCZOS)
            atlas.paste(icon, (x, y))
        sprites[f"item:{path.name}"] = {
            "texture": "items", "x": x, "y": y, "w": CELL, "h": CELL,
        }

    manifest["textures"]["items"]["width"] = COLUMNS * CELL
    manifest["textures"]["items"]["height"] = height
    manifest["sprites"] = dict(sorted(sprites.items()))

    for target in (atlas_path, manifest_path):
        if target.exists():
            shutil.copy2(target, target.with_suffix(target.suffix + ".bak"))

    atlas.save(atlas_path)
    manifest_path.write_text(
        json.dumps(manifest, indent=1, ensure_ascii=False) + "\n",
        encoding="utf-8")

    drop_log = items_dir.parent / "Items_excluded_from_atlas.txt"
    drop_log.write_text("\n".join(dropped) + "\n", encoding="utf-8")
    print(f"wrote {atlas_path.name}, manifest updated, drop log -> {drop_log}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
