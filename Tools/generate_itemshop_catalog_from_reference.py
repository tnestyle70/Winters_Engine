from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
UI_TEXTURE_DIR = ROOT / "Client" / "Bin" / "Resource" / "Texture" / "UI"
ITEM_ICON_DIR = UI_TEXTURE_DIR / "Items"
OUTPUT_LUA = ROOT / "Client" / "Bin" / "Resource" / "UI" / "Lua" / "itemshop_catalog.lua"
ITEM_DEF = ROOT / "Shared" / "GameSim" / "Definitions" / "ItemDef.h"
EXTRA_CATALOG_ITEMS = {
    3153: ("legendary", "Legendary", 452, 773),
}


@dataclass(frozen=True)
class Sample:
    ref_size: tuple[int, int]
    section: str
    section_name: str
    row: int
    col: int
    x: int
    y: int
    price: int
    order: int
    slot_order: int


@dataclass
class Match:
    sample: Sample
    item_id: int
    icon_name: str
    score: float
    margin: float


def find_reference_by_size(size: tuple[int, int]) -> Path:
    for path in UI_TEXTURE_DIR.glob("*.png"):
        try:
            with Image.open(path) as image:
                if image.size == size:
                    return path
        except OSError:
            continue
    raise FileNotFoundError(f"reference image with size {size[0]}x{size[1]} was not found")


def build_samples() -> list[Sample]:
    samples: list[Sample] = []
    order = 0
    slot_order = 0
    slot_orders: dict[tuple[str, int, int], int] = {}

    def add_row(ref_size: tuple[int, int], section: str, section_name: str,
                row: int, x0: int, y: int, prices: list[int]) -> None:
        nonlocal order, slot_order
        for col, price in enumerate(prices):
            key = (section, row, col)
            if key not in slot_orders:
                slot_orders[key] = slot_order
                slot_order += 1
            samples.append(Sample(
                ref_size, section, section_name, row, col,
                x0 + col * 56, y, price, order, slot_orders[key]))
            order += 1

    ref1 = (1165, 736)
    ref2 = (1132, 729)

    add_row(ref1, "starter", "Starter", 0, 172, 201, [400, 450, 450, 450, 0, 0, 0])
    add_row(ref1, "basic", "Basic", 0, 172, 315, [250, 250, 300, 350, 400, 400, 875, 1300])
    add_row(ref1, "epic", "Epic", 0, 172, 430, [700, 775, 800, 800, 800, 800, 850, 900, 900, 1050])
    add_row(ref1, "epic", "Epic", 1, 172, 506, [1100, 1100, 1150, 1200, 1200, 1300])
    add_row(ref1, "legendary", "Legendary", 0, 172, 621, [2800, 2900, 2900, 2900, 2900, 3000, 3000, 3000, 3000, 3000])

    add_row(ref2, "basic", "Basic", 0, 155, 151, [250, 250, 300, 350, 400, 400, 875, 1300])
    add_row(ref2, "epic", "Epic", 0, 155, 266, [700, 775, 800, 800, 800, 800, 850, 900, 900, 1050])
    add_row(ref2, "epic", "Epic", 1, 155, 342, [1100, 1100, 1150, 1200, 1200, 1300])
    add_row(ref2, "legendary", "Legendary", 0, 155, 456, [2800, 2900, 2900, 2900, 2900, 3000, 3000, 3000, 3000, 3000])
    add_row(ref2, "legendary", "Legendary", 1, 155, 532, [3100, 3100, 3100, 3100, 3200, 3200, 3200, 3200, 3300, 3300])
    add_row(ref2, "legendary", "Legendary", 2, 155, 608, [3300, 3300, 3300, 3333, 3400])

    return samples


def feature(image: Image.Image) -> np.ndarray:
    patch = image.convert("RGB").crop((3, 3, 37, 37)).resize((32, 32), Image.Resampling.LANCZOS)
    arr = np.asarray(patch, dtype=np.float32).reshape(-1, 3)
    mean = arr.mean(axis=0)
    std = arr.std(axis=0) + 1e-5
    return ((arr - mean) / std).reshape(-1)


def load_icon_features() -> list[tuple[int, str, np.ndarray]]:
    icons: list[tuple[int, str, np.ndarray]] = []
    for path in sorted(ITEM_ICON_DIR.glob("*.png")):
        match = re.match(r"(\d+)", path.name)
        if not match:
            continue
        with Image.open(path) as image:
            resized = image.convert("RGB").resize((40, 40), Image.Resampling.LANCZOS)
            icons.append((int(match.group(1)), path.name, feature(resized)))
    if not icons:
        raise FileNotFoundError(f"no item icons found in {ITEM_ICON_DIR}")
    return icons


def parse_registered_items() -> dict[int, tuple[int, str]]:
    text = ITEM_DEF.read_text(encoding="utf-8", errors="ignore")
    registered: dict[int, tuple[int, str]] = {}
    pattern = re.compile(r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*ItemStatModifier\{.*?\}\s*,\s*\"([^\"]+)\"\s*\}", re.DOTALL)
    for item_id, price, name in pattern.findall(text):
        registered[int(item_id)] = (int(price), name)
    return registered


def match_sample(sample: Sample, references: dict[tuple[int, int], Image.Image],
                 icons: list[tuple[int, str, np.ndarray]],
                 registered: dict[int, tuple[int, str]]) -> Match:
    ref = references[sample.ref_size]
    patch = ref.crop((sample.x, sample.y, sample.x + 40, sample.y + 40))
    sample_feature = feature(patch)
    scored = []
    for item_id, icon_name, icon_feature in icons:
        score = float(np.mean((sample_feature - icon_feature) ** 2))
        reg = registered.get(item_id)
        if reg and reg[0] != sample.price:
            score += 0.35 + min(abs(reg[0] - sample.price) / 3000.0, 1.0) * 0.4
        scored.append((score, item_id, icon_name))
    scored.sort(key=lambda entry: entry[0])
    best = scored[0]
    second = scored[1] if len(scored) > 1 else (best[0], best[1], best[2])
    return Match(sample, best[1], best[2], best[0], second[0] - best[0])


def select_best_slot_matches(matches: list[Match]) -> list[Match]:
    selected: dict[tuple[str, int, int], Match] = {}
    for match in matches:
        key = (match.sample.section, match.sample.row, match.sample.col)
        current = selected.get(key)
        if current is None or (match.score, -match.margin) < (current.score, -current.margin):
            selected[key] = match
    return sorted(selected.values(), key=lambda match: match.sample.slot_order)


def lua_quote(text: str) -> str:
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def display_name_from_icon(icon_name: str) -> str:
    stem = Path(icon_name).stem
    stem = re.sub(r"^\d+_?", "", stem)
    return stem.replace("_", " ").replace("-", " ").title()


def logical_slot_position(sample: Sample) -> tuple[int, int]:
    section_y = {
        "starter": 201,
        "basic": 315,
        "epic": 430,
        "legendary": 621,
    }
    return 172 + sample.col * 56, section_y[sample.section] + sample.row * 76


def find_icon_name(icons: list[tuple[int, str, np.ndarray]], item_id: int) -> str | None:
    for icon_item_id, icon_name, _ in icons:
        if icon_item_id == item_id:
            return icon_name
    return None


def write_catalog(matches: list[Match],
                  registered: dict[int, tuple[int, str]],
                  icons: list[tuple[int, str, np.ndarray]]) -> None:
    lines = [
        "WintersItemShopCatalog = {",
        "    columns = 10,",
        "    sections = {",
        "        { section = \"starter\", name = \"Starter\", y = 174 },",
        "        { section = \"basic\", name = \"Basic\", y = 289 },",
        "        { section = \"epic\", name = \"Epic\", y = 404 },",
        "        { section = \"legendary\", name = \"Legendary\", y = 595 },",
        "    },",
        "    items = {",
    ]

    written_item_ids: set[int] = set()

    for match in matches:
        reg = registered.get(match.item_id)
        price = reg[0] if reg else match.sample.price
        name = reg[1] if reg else display_name_from_icon(match.icon_name)
        purchasable = "true" if reg else "false"
        needs_review = "true" if match.score > 0.95 or match.margin < 0.12 else "false"
        icon_path = f"Resource/Texture/UI/Items/{match.icon_name}"
        x, y = logical_slot_position(match.sample)

        lines.append(
            "        { "
            f"id = {match.item_id}, "
            f"price = {price}, "
            f"section = {lua_quote(match.sample.section)}, "
            f"sectionName = {lua_quote(match.sample.section_name)}, "
            f"name = {lua_quote(name)}, "
            f"icon = {lua_quote(icon_path)}, "
            f"x = {x}, "
            f"y = {y}, "
            f"purchasable = {purchasable}, "
            f"matchScore = {match.score:.4f}, "
            f"needsReview = {needs_review} "
            "},"
        )
        written_item_ids.add(match.item_id)

    for item_id, (section, section_name, x, y) in EXTRA_CATALOG_ITEMS.items():
        if item_id in written_item_ids:
            continue

        reg = registered.get(item_id)
        icon_name = find_icon_name(icons, item_id)
        if not reg or not icon_name:
            continue

        lines.append(
            "        { "
            f"id = {item_id}, "
            f"price = {reg[0]}, "
            f"section = {lua_quote(section)}, "
            f"sectionName = {lua_quote(section_name)}, "
            f"name = {lua_quote(reg[1])}, "
            f"icon = {lua_quote(f'Resource/Texture/UI/Items/{icon_name}')}, "
            f"x = {x}, "
            f"y = {y}, "
            "purchasable = true, "
            "matchScore = 0.0000, "
            "needsReview = false "
            "},"
        )

    lines.extend([
        "    },",
        "}",
        "",
    ])
    OUTPUT_LUA.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_LUA.write_text("\n".join(lines), encoding="utf-8")


def write_verify_images(matches: list[Match], references: dict[tuple[int, int], Image.Image]) -> None:
    output_dir = OUTPUT_LUA.parent
    grouped: dict[tuple[int, int], list[Match]] = {}
    for match in matches:
        grouped.setdefault(match.sample.ref_size, []).append(match)

    for size, group in grouped.items():
        image = references[size].copy()
        draw = ImageDraw.Draw(image)
        for match in group:
            color = (70, 230, 80) if match.score <= 0.95 and match.margin >= 0.12 else (255, 207, 72)
            x, y = match.sample.x, match.sample.y
            draw.rectangle((x, y, x + 40, y + 40), outline=color, width=2)
            draw.text((x, y - 12), str(match.item_id), fill=color)
        image.save(output_dir / f"itemshop_catalog_verify_{size[0]}x{size[1]}.png")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()

    samples = build_samples()
    refs = {size: Image.open(find_reference_by_size(size)).convert("RGB") for size in {(s.ref_size) for s in samples}}
    icons = load_icon_features()
    registered = parse_registered_items()
    matches = [match_sample(sample, refs, icons, registered) for sample in samples]
    matches.sort(key=lambda match: match.sample.order)
    selected = select_best_slot_matches(matches)
    write_catalog(selected, registered, icons)

    if args.verify:
        write_verify_images(matches, refs)
        reviewed = sum(1 for match in selected if match.score > 0.95 or match.margin < 0.12)
        print(f"wrote {OUTPUT_LUA}")
        print(f"matched slots: {len(selected)} from samples: {len(matches)}, needs review: {reviewed}")
        for match in selected:
            flag = " REVIEW" if match.score > 0.95 or match.margin < 0.12 else ""
            print(
                f"{match.sample.section:9s} order={match.sample.order:02d} "
                f"id={match.item_id:5d} price={match.sample.price:4d} "
                f"score={match.score:.3f} margin={match.margin:.3f}{flag} {match.icon_name}"
            )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
