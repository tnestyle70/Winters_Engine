from __future__ import annotations

import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
FX_ROOT = ROOT / "Data" / "LoL" / "FX"

PROTECTED_TINTS = {
    "Champions/LeeSin/w1_cast.wfx": {
        "w1_floor_gold_teal_ring": [0.72, 1.0, 0.78, 0.38],
        "w1_shield_main_bubble": [0.68, 1.0, 0.84, 0.55],
        "w1_shield_outer_glow": [0.36, 0.98, 1.0, 0.44],
        "w1_gold_outline_pop": [1.0, 0.78, 0.24, 0.58],
        "w1_scroll_interior": [0.92, 0.82, 0.44, 0.46],
    },
    "Champions/Riven/Riven_E_Shield.wfx": {
        "E_ShieldMesh": [0.62, 2.45, 0.48, 0.68],
        "E_ShieldMainBubble": [0.50, 2.20, 0.40, 0.52],
        "E_ShieldOuterGlow": [0.70, 2.55, 0.60, 0.38],
        "E_BlueFlare": [0.70, 2.55, 0.60, 0.54],
        "E_Rune": [0.72, 2.30, 0.52, 0.52],
        "E_GroundFlash": [0.36, 1.85, 0.30, 0.46],
    },
}


def fail(message: str) -> None:
    raise SystemExit(f"[WfxTintRegression] FAIL: {message}")


def load_wfx(path: Path) -> dict:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        fail(f"{path.relative_to(ROOT)}: {error}")
    if not isinstance(document.get("emitters"), list):
        fail(f"{path.relative_to(ROOT)}: emitters must be an array")
    return document


def validate_color(path: Path, emitter_name: str, color: object) -> None:
    if not isinstance(color, list) or len(color) != 4:
        fail(f"{path.relative_to(ROOT)}:{emitter_name}: color must have RGBA")
    if not all(
        isinstance(value, (int, float))
        and not isinstance(value, bool)
        and math.isfinite(value)
        for value in color
    ):
        fail(f"{path.relative_to(ROOT)}:{emitter_name}: color must be finite numeric RGBA")


def main() -> int:
    wfx_files = sorted(FX_ROOT.rglob("*.wfx"))
    if not wfx_files:
        fail("no WFX files found")

    documents: dict[str, dict] = {}
    emitter_count = 0
    for path in wfx_files:
        document = load_wfx(path)
        key = path.relative_to(FX_ROOT).as_posix()
        documents[key] = document
        for emitter in document["emitters"]:
            if not isinstance(emitter, dict):
                fail(f"{path.relative_to(ROOT)}: every emitter must be an object")
            emitter_name = emitter.get("name", "<unnamed>")
            if "color" in emitter:
                validate_color(path, str(emitter_name), emitter["color"])
            emitter_count += 1

    protected_count = 0
    for relative_path, expected_emitters in PROTECTED_TINTS.items():
        document = documents.get(relative_path)
        if document is None:
            fail(f"missing protected asset: {relative_path}")
        actual_emitters: dict[str, dict] = {}
        for emitter in document["emitters"]:
            emitter_name = emitter.get("name")
            if not isinstance(emitter_name, str):
                continue
            if emitter_name in actual_emitters:
                fail(f"{relative_path}: duplicate protected emitter {emitter_name}")
            actual_emitters[emitter_name] = emitter
        for emitter_name, expected_color in expected_emitters.items():
            emitter = actual_emitters.get(emitter_name)
            if emitter is None:
                fail(f"{relative_path}: missing protected emitter {emitter_name}")
            actual_color = emitter.get("color")
            validate_color(FX_ROOT / relative_path, emitter_name, actual_color)
            if actual_color != expected_color:
                fail(
                    f"{relative_path}:{emitter_name}: "
                    f"expected {expected_color}, actual {actual_color}"
                )
            protected_count += 1

    print(
        f"[WfxTintRegression] PASS: {len(wfx_files)} files / "
        f"{emitter_count} emitters valid; {protected_count} protected tints exact"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
