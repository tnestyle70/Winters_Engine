#!/usr/bin/env python3
"""Build deterministic champion VO slot maps from LoL Wwise metadata.

The localized WPK owns media payloads, while skin0.bin owns human-readable
event names and the VO events BNK owns Event -> Action -> Container -> WEM
relationships.  This tool joins those three sources without speech-to-text.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable


@dataclass(frozen=True)
class ChampionSpec:
    name: str
    asset: str
    skill_patterns: dict[str, tuple[str, ...]]
    skill_event_ids: dict[str, tuple[int, ...]] = field(default_factory=dict)


CHAMPIONS: tuple[ChampionSpec, ...] = (
    ChampionSpec("Annie", "annie", {
        "skillQ": (r"_AnnieQ_cast3D$",),
        "skillW": (),
        "skillE": (),
        "skillR": (r"_AnnieR_cast3D$",),
    }),
    ChampionSpec("Ashe", "ashe", {
        "skillQ": (r"_AsheQ_cast3D$",),
        "skillW": (r"_Volley(?:Rank[2-5])?_cast3D$",),
        "skillE": (r"_AsheSpiritOfTheHawk_cast3D$",),
        "skillR": (),
    }, {
        # Current skin0.bin retains a stale EnchantedCrystalArrow name hash.
        # Both live BNK events resolve to the same five localized R lines.
        "skillR": (1546256697, 2074303445),
    }),
    ChampionSpec("Ezreal", "ezreal", {
        "skillQ": (r"_EzrealQ_cast3D$",),
        "skillW": (r"_EzrealW_cast3D$",),
        "skillE": (r"_EzrealE_cast3D$",),
        "skillR": (r"_EzrealR_cast3D$",),
    }),
    ChampionSpec("Fiora", "fiora", {
        "skillQ": (r"_Spell3DQHit$",),
        "skillW": (r"_FioraW(?:_Jade)?_cast3D$", r"_Spell3DW2Cast$"),
        "skillE": (),
        "skillR": (r"_FioraR(?:_Jade)?_cast3D$",),
    }),
    ChampionSpec("Garen", "garen", {
        "skillQ": (r"_GarenQ(?:Attack)?_cast3D$",),
        "skillW": (r"_GarenW_cast3D$",),
        "skillE": (r"_GarenE_cast3D$",),
        "skillR": (r"_GarenR_cast3D$",),
    }),
    ChampionSpec("Irelia", "irelia", {
        "skillQ": (r"_IreliaQ_cast3D$",),
        "skillW": (r"_IreliaW2?_cast3D$",),
        "skillE": (r"_IreliaE2?_cast3D$",),
        "skillR": (r"_IreliaR_cast3D$",),
    }),
    ChampionSpec("Jax", "jax", {
        "skillQ": (r"_JaxQ_cast3D$",),
        "skillW": (r"_JaxWAttack_cast3D$",),
        "skillE": (r"_JaxE_cast3D$",),
        "skillR": (r"_JaxR_cast3D$",),
    }),
    ChampionSpec("Kalista", "kalista", {
        "skillQ": (r"_KalistaMysticShot_cast3D$",),
        "skillW": (r"_KalistaW_cast3D$",),
        "skillE": (r"_KalistaExpunge_cast3D$",),
        "skillR": (r"_KalistaRMis_cast3D$",),
    }),
    ChampionSpec("Kindred", "kindred", {
        "skillQ": (r"_KindredQ_cast3D$",),
        "skillW": (r"_KindredW_cast3D$",),
        "skillE": (r"_KindredE_cast3D$",),
        "skillR": (r"_KindredR_cast3D$",),
    }),
    ChampionSpec("LeeSin", "leesin", {
        "skillQ": (r"_LeeSinQ(?:One|Two)_cast3D$",),
        "skillW": (r"_LeeSinWTwo_cast3D$",),
        "skillE": (r"_LeeSinE(?:One|Two)_cast3D$",),
        "skillR": (r"_LeeSinR_cast3D$",),
    }),
    ChampionSpec("MasterYi", "masteryi", {
        "skillQ": (r"_AlphaStrike_cast3D$",),
        "skillW": (r"_Meditate_cast3D$",),
        "skillE": (r"_WujuStyle_cast3D$",),
        "skillR": (r"_Highlander_cast3D$",),
    }),
    ChampionSpec("Riven", "riven", {
        "skillQ": (r"_Spell3DQ[123]Cast$",),
        "skillW": (r"_RivenMartyr_cast3D$",),
        "skillE": (r"_RivenFeint_cast3D$",),
        "skillR": (r"_Riven(?:FengShuiEngine|IzunaBlade)_cast3D$",),
    }),
    ChampionSpec("Sylas", "sylas", {
        "skillQ": (r"_SylasQ_cast3D$",),
        "skillW": (r"_SylasW_cast3D$",),
        "skillE": (r"_SylasE_cast3D$",),
        "skillR": (r"_SylasR_hit3D$", r"_SylasR2_cast$"),
    }),
    ChampionSpec("Viego", "viego", {
        "skillQ": (r"_ViegoQ_cast3D$",),
        "skillW": (r"_Spell3DW2Cast$",),
        "skillE": (r"_ViegoE_cast3D$",),
        "skillR": (r"_ViegoR_cast3D$",),
    }),
    ChampionSpec("Yasuo", "yasuo", {
        "skillQ": (r"_YasuoQ(?:1|2|3Wrapper)_cast3D$",),
        "skillW": (r"_YasuoW_cast3D$",),
        "skillE": (r"_YasuoEDash_cast3D$",),
        "skillR": (r"_YasuoR_cast3D$",),
    }),
    ChampionSpec("Yone", "yone", {
        "skillQ": (r"_YoneQ3?_cast3D$",),
        "skillW": (r"_YoneW_cast3D$",),
        "skillE": (r"_YoneE_cast3D$", r"_Spell3DEEnd$"),
        "skillR": (r"_YoneR_cast3D$",),
    }),
    ChampionSpec("Zed", "zed", {
        "skillQ": (r"_ZedQ_cast3D$",),
        "skillW": (r"_ZedW2?_cast3D$",),
        "skillE": (r"_ZedE_cast3D$",),
        "skillR": (r"_ZedR_cast3D$",),
    }),
)

SLOT_ORDER = ("move", "basicAttack", "skillQ", "skillW", "skillE", "skillR", "death")
BASE_PATTERNS: dict[str, tuple[str, ...]] = {
    "move": (r"_Move2D(?:First|Long|Standard)$",),
    "basicAttack": (r"_(?:[^_]*BasicAttack[^_]*|[^_]*CritAttack[^_]*)_cast3D$",),
    "death": (r"_Death3D$",),
}
EXPECTED_EMPTY_SLOTS = {
    ("Annie", "skillW"),
    ("Annie", "skillE"),
    ("Ashe", "basicAttack"),
    ("Fiora", "skillE"),
}


def parse_args() -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=repo_root)
    parser.add_argument("--league-root", type=Path, default=Path(r"C:\Riot Games\League of Legends"))
    parser.add_argument(
        "--probe",
        type=Path,
        default=repo_root / "Tools/External/LeagueToolkitProbe/bin/Debug/net10.0/LeagueToolkitProbe.exe",
    )
    parser.add_argument(
        "--wwiser",
        type=Path,
        default=None,
    )
    parser.add_argument(
        "--sound-stage-root",
        type=Path,
        default=repo_root / "Tools/Bin/Intermediate/LoLSoundBanks",
    )
    parser.add_argument(
        "--map-stage-root",
        type=Path,
        default=repo_root / "Tools/Bin/Intermediate/LoLVoiceMap",
    )
    parser.add_argument(
        "--voice-root",
        type=Path,
        default=repo_root / "Client/Bin/Resource/Sound/LoL/Champions",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=repo_root / "Data/LoL/Sound/ChampionVoiceMap.json",
    )
    parser.add_argument("--locale", default="ko_KR")
    return parser.parse_args()


def run_checked(command: list[str], label: str) -> str:
    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if result.returncode != 0:
        raise RuntimeError(
            f"{label} failed ({result.returncode})\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    return result.stdout


def fnv1_lower(text: str) -> int:
    value = 2166136261
    for byte in text.lower().encode("utf-8"):
        value = ((value * 16777619) & 0xFFFFFFFF) ^ byte
    return value


def extract_bin_strings(probe: Path, bin_path: Path, prefix: str) -> list[str]:
    stdout = run_checked([str(probe), "bin-strings", str(bin_path), prefix], f"bin-strings {bin_path}")
    values: set[str] = set()
    for line in stdout.splitlines():
        marker = "|value="
        if marker not in line:
            continue
        value = line.split(marker, 1)[1]
        if value.startswith(prefix):
            values.add(value)
    return sorted(values)


class WwiseGraph:
    def __init__(self, xml_path: Path) -> None:
        root = ET.parse(xml_path).getroot()
        objects: dict[int, ET.Element] = {}
        for obj in root.findall(".//object"):
            if not obj.attrib.get("name", "").startswith("CAk"):
                continue
            id_field = obj.find("./field[@name='ulID']")
            if id_field is None:
                continue
            objects[int(id_field.attrib["value"])] = obj

        self.sources: dict[int, int] = {}
        self.children: dict[int, list[int]] = {}
        self.action_targets: dict[int, int] = {}
        self.event_actions: dict[int, list[int]] = {}

        for object_id, obj in objects.items():
            kind = obj.attrib.get("name", "")
            if kind == "CAkSound":
                source_field = obj.find(".//field[@name='sourceID']")
                if source_field is not None:
                    self.sources[object_id] = int(source_field.attrib["value"])

            child_ids = [
                int(field.attrib["value"])
                for field_name in ("ulChildID", "ulNodeID")
                for field in obj.findall(f".//field[@name='{field_name}']")
            ]
            if child_ids:
                self.children[object_id] = child_ids

            if kind.startswith("CAkAction"):
                target_field = obj.find(".//field[@name='idExt']")
                if target_field is not None:
                    self.action_targets[object_id] = int(target_field.attrib["value"])

            if kind == "CAkEvent":
                self.event_actions[object_id] = [
                    int(field.attrib["value"])
                    for field in obj.findall(".//field[@name='ulActionID']")
                ]

    def _collect_node_sources(self, object_id: int, seen: set[int]) -> set[int]:
        if object_id in seen:
            return set()
        seen.add(object_id)

        result: set[int] = set()
        if object_id in self.sources:
            result.add(self.sources[object_id])
        for child_id in self.children.get(object_id, ()):
            result.update(self._collect_node_sources(child_id, seen))
        return result

    def event_sources(self, event_id: int) -> list[int]:
        result: set[int] = set()
        for action_id in self.event_actions.get(event_id, ()):
            target_id = self.action_targets.get(action_id)
            if target_id is not None:
                result.update(self._collect_node_sources(target_id, set()))
        return sorted(result)


def matches_any(value: str, patterns: Iterable[str]) -> bool:
    return any(re.search(pattern, value, re.IGNORECASE) is not None for pattern in patterns)


def build_champion(
    args: argparse.Namespace,
    spec: ChampionSpec,
) -> tuple[dict[str, object], list[str]]:
    champion_stage = args.map_stage_root / spec.name
    skin_relative = f"data/characters/{spec.asset}/skins/skin0.bin"
    skin_path = champion_stage / Path(skin_relative)
    main_wad = args.league_root / "Game/DATA/FINAL/Champions" / f"{spec.name}.wad.client"

    champion_stage.mkdir(parents=True, exist_ok=True)
    run_checked(
        [str(args.probe), "wad-extract", str(main_wad), str(champion_stage), skin_relative],
        f"skin0 extract {spec.name}",
    )
    if not skin_path.is_file():
        raise FileNotFoundError(f"Missing extracted skin BIN: {skin_path}")

    event_names = extract_bin_strings(args.probe, skin_path, f"Play_vo_{spec.name}")
    event_relative = (
        f"assets/sounds/wwise2016/vo/en_us/characters/{spec.asset}/skins/base/"
        f"{spec.asset}_base_vo_events.bnk"
    )
    event_bank = args.sound_stage_root / "Champions" / spec.name / "Voice" / args.locale / event_relative
    if not event_bank.is_file():
        localized_wad = (
            args.league_root / "Game/DATA/FINAL/Champions" / f"{spec.name}.{args.locale}.wad.client"
        )
        event_root = args.sound_stage_root / "Champions" / spec.name / "Voice" / args.locale
        run_checked(
            [str(args.probe), "wad-extract", str(localized_wad), str(event_root), event_relative],
            f"VO event bank extract {spec.name}",
        )
    if not event_bank.is_file():
        raise FileNotFoundError(f"Missing VO events bank: {event_bank}")

    xml_base = champion_stage / f"{spec.asset}_base_vo_events"
    xml_path = Path(str(xml_base) + ".xml")
    if xml_path.exists():
        xml_path.unlink()
    run_checked(
        [sys.executable, str(args.wwiser), str(event_bank), "-d", "xml", "-dn", str(xml_base)],
        f"wwiser {spec.name}",
    )
    if not xml_path.is_file():
        raise FileNotFoundError(f"wwiser did not create {xml_path}")

    graph = WwiseGraph(xml_path)
    slot_patterns = dict(BASE_PATTERNS)
    slot_patterns.update(spec.skill_patterns)
    voice_dir = args.voice_root / spec.name / "Voice"
    locale_stem = args.locale.lower()

    voices: dict[str, list[str]] = {}
    audit: dict[str, list[dict[str, object]]] = {}
    warnings: list[str] = []
    for slot in SLOT_ORDER:
        patterns = slot_patterns.get(slot, ())
        matching_events = [name for name in event_names if matches_any(name, patterns)]
        source_ids: set[int] = set()
        event_audit: list[dict[str, object]] = []
        for event_name in matching_events:
            event_id = fnv1_lower(event_name)
            event_sources = graph.event_sources(event_id)
            source_ids.update(event_sources)
            event_audit.append({
                "name": event_name,
                "eventId": event_id,
                "wemCount": len(event_sources),
            })
            if not event_sources:
                warnings.append(f"{spec.name}:{slot}: event has no WEM sources: {event_name}")

        for event_id in spec.skill_event_ids.get(slot, ()):
            event_sources = graph.event_sources(event_id)
            source_ids.update(event_sources)
            event_audit.append({
                "name": f"BNK_EVENT_{event_id}",
                "eventId": event_id,
                "wemCount": len(event_sources),
            })
            if not event_sources:
                warnings.append(f"{spec.name}:{slot}: explicit event has no WEM sources: {event_id}")

        paths: list[str] = []
        for source_id in sorted(source_ids):
            filename = f"{spec.asset}_base_vo_{locale_stem}_{source_id}.wav"
            source_path = voice_dir / filename
            if not source_path.is_file():
                warnings.append(f"{spec.name}:{slot}: missing WAV {source_path}")
                continue
            paths.append(f"LoL/Champions/{spec.name}/Voice/{filename}")

        voices[slot] = paths
        audit[slot] = event_audit
        if not paths and (spec.name, slot) not in EXPECTED_EMPTY_SLOTS:
            warnings.append(f"{spec.name}:{slot}: required slot resolved no WAV files")

    return ({
        "champion": spec.name,
        "voices": voices,
        "audit": audit,
    }, warnings)


def main() -> int:
    args = parse_args()
    args.repo_root = args.repo_root.resolve()
    args.league_root = args.league_root.resolve()
    args.probe = args.probe.resolve()
    if args.wwiser is None:
        wwiser_candidates = (
            args.repo_root / "Tools/External/wwiser/wwiser.py",
            Path(tempfile.gettempdir()) / "winters-wwiser/wwiser.py",
        )
        args.wwiser = next((path for path in wwiser_candidates if path.is_file()), wwiser_candidates[0])
    args.wwiser = args.wwiser.resolve()
    args.sound_stage_root = args.sound_stage_root.resolve()
    args.map_stage_root = args.map_stage_root.resolve()
    args.voice_root = args.voice_root.resolve()
    args.output = args.output.resolve()

    for required in (args.probe, args.wwiser, args.league_root, args.voice_root):
        if not required.exists():
            raise FileNotFoundError(required)

    champions: list[dict[str, object]] = []
    warnings: list[str] = []
    for spec in CHAMPIONS:
        champion, champion_warnings = build_champion(args, spec)
        champions.append(champion)
        warnings.extend(champion_warnings)
        counts = champion["voices"]
        summary = ", ".join(f"{slot}={len(counts[slot])}" for slot in SLOT_ORDER)
        print(f"{spec.name}: {summary}")

    result = {
        "version": 2,
        "locale": args.locale,
        "voiceVolume": 0.9,
        "moveDelayMinSec": 8.0,
        "moveDelayMaxSec": 10.0,
        "champions": champions,
        "warnings": warnings,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"WROTE {args.output}")
    print(f"warnings={len(warnings)}")
    return 0 if not warnings else 2


if __name__ == "__main__":
    raise SystemExit(main())
