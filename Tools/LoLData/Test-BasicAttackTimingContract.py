from __future__ import annotations

import json
import math
import struct
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
RESOURCE_ROOT = ROOT / "Client" / "Bin" / "Resource"
GAMEPLAY_PATH = ROOT / "Data" / "Gameplay" / "ChampionGameData" / "champions.json"
VISUAL_PATH = ROOT / "Data" / "LoL" / "ClientPublic" / "Visual" / "ChampionVisualDefs.json"
ASSET_PATH = ROOT / "Data" / "LoL" / "ClientPublic" / "Visual" / "ChampionAssetVisualDefs.json"
DRAGON_MESH_PATH = (
    RESOURCE_ROOT / "Texture" / "Object" / "Jungle" / "Dragon" / "air" /
    "dragon_air_textured.wmesh")
DRAGON_ATTACK_PATH = (
    DRAGON_MESH_PATH.parent / "anims" / "sru_dragon_flying_attack1.wanim")
JUNGLE_MANAGER_PATH = ROOT / "Client" / "Private" / "Manager" / "Jungle_Manager.cpp"
FLOAT_TOLERANCE = 0.00001
ZERO_MARKER_FALLBACKS = {"YASUO"}
WINT_HEADER_SIZE = 16
WSKEL_META_SIZE = 32
WSKEL_BONE_SIZE = 256
WSKEL_GLOBAL_ROOT_SIZE = 128
WSKEL_SOCKET_SIZE = 128
WANIM_META_SIZE = 32
WANIM_CHANNEL_SIZE = 40
WANIM_EVENT_SIZE = 32
WANIM_TRAILER_SIZE = 8
FNV64_OFFSET = 0xCBF29CE484222325
FNV64_PRIME = 0x100000001B3


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def read_wint_payload(path: Path) -> bytes:
    data = path.read_bytes()
    if len(data) < WINT_HEADER_SIZE:
        raise ValueError(f"truncated WINT header: {path}")
    magic, major, _minor, flags, content_size = struct.unpack_from(
        "<4sHHII", data, 0)
    if magic != b"WINT" or major != 1 or flags != 0:
        raise ValueError(
            f"invalid WINT header magic={magic!r} major={major} flags={flags}: {path}")
    if content_size > len(data) - WINT_HEADER_SIZE:
        raise ValueError(f"WINT content exceeds file: {path}")
    return data[WINT_HEADER_SIZE:WINT_HEADER_SIZE + content_size]


def read_wskel_hash(path: Path) -> int:
    payload = read_wint_payload(path)
    if len(payload) < WSKEL_META_SIZE:
        raise ValueError(f"truncated WSKL meta: {path}")
    magic, bone_count, socket_count = struct.unpack_from("<4sII", payload, 0)
    if magic != b"WSKL" or bone_count == 0 or bone_count > 1024 or socket_count > 256:
        raise ValueError(f"invalid WSKL meta: {path}")
    required = (
        WSKEL_META_SIZE + bone_count * WSKEL_BONE_SIZE +
        WSKEL_GLOBAL_ROOT_SIZE + socket_count * WSKEL_SOCKET_SIZE)
    if required > len(payload):
        raise ValueError(f"truncated WSKL payload: {path}")

    skeleton_hash = FNV64_OFFSET
    for bone_index in range(bone_count):
        bone_offset = WSKEL_META_SIZE + bone_index * WSKEL_BONE_SIZE
        name_hash = struct.unpack_from("<Q", payload, bone_offset)[0]
        parent_index = struct.unpack_from("<i", payload, bone_offset + 72)[0]
        if parent_index >= bone_count:
            raise ValueError(f"invalid WSKL parent index: {path}")
        skeleton_hash ^= name_hash
        skeleton_hash = (skeleton_hash * FNV64_PRIME) & 0xFFFFFFFFFFFFFFFF
    return skeleton_hash


def read_valid_wanim(path: Path, expected_skeleton_hash: int) -> tuple[float, float]:
    payload = read_wint_payload(path)
    if len(payload) < WANIM_META_SIZE:
        raise ValueError(f"truncated WANM meta: {path}")
    (
        magic,
        channel_count,
        duration_ticks,
        ticks_per_second,
        total_key_count,
        event_count,
    ) = struct.unpack_from("<4sIffII", payload, 0)
    if (
        magic != b"WANM" or channel_count > 1024 or
        total_key_count > 1_000_000 or
        not math.isfinite(duration_ticks) or duration_ticks < 0.0 or
        not math.isfinite(ticks_per_second) or ticks_per_second <= 0.0
    ):
        raise ValueError(f"invalid WANM meta: {path}")

    channel_bytes = channel_count * WANIM_CHANNEL_SIZE
    channel_end = WANIM_META_SIZE + channel_bytes
    event_bytes = event_count * WANIM_EVENT_SIZE
    if len(payload) < channel_end + event_bytes + WANIM_TRAILER_SIZE:
        raise ValueError(f"truncated WANM payload: {path}")
    key_block_size = len(payload) - channel_end - event_bytes - WANIM_TRAILER_SIZE
    for channel_index in range(channel_count):
        channel_offset = WANIM_META_SIZE + channel_index * WANIM_CHANNEL_SIZE
        (
            _bone_hash,
            pos_count,
            pos_offset,
            rot_count,
            rot_offset,
            scale_count,
            scale_offset,
            _cached_index,
            _reserved,
        ) = struct.unpack_from("<QIIIIIIiI", payload, channel_offset)
        spans = (
            (pos_offset, pos_count, 16),
            (rot_offset, rot_count, 20),
            (scale_offset, scale_count, 16),
        )
        if any(offset > key_block_size or count * stride > key_block_size - offset
               for offset, count, stride in spans):
            raise ValueError(f"WANM key span exceeds block: {path}")

    trailer_hash = struct.unpack_from(
        "<Q", payload, len(payload) - WANIM_TRAILER_SIZE)[0]
    if trailer_hash != expected_skeleton_hash:
        raise ValueError(f"WANM skeleton hash mismatch: {path}")
    return duration_ticks, ticks_per_second


def resolve_basic_attack_clip(model: dict) -> tuple[Path, str, float, float]:
    prefix = str(model.get("animPrefix", ""))
    animation = str(model["basicAttackAnimation"])
    query = animation if animation.startswith(prefix) else prefix + animation
    mesh_path = RESOURCE_ROOT / Path(str(model["mesh"]))
    mesh_parent = mesh_path.parent
    skeleton_hash = read_wskel_hash(mesh_path.with_suffix(".wskel"))
    anim_dir = mesh_parent / "anims"
    candidates = sorted(anim_dir.glob("*.wanim"), key=lambda path: str(path))
    for candidate in candidates:
        try:
            duration_ticks, ticks_per_second = read_valid_wanim(
                candidate, skeleton_hash)
        except ValueError:
            continue
        if query in candidate.stem:
            return candidate, query, duration_ticks, ticks_per_second
    raise ValueError(f"no runtime-valid .wanim contains '{query}' under {anim_dir}")


def validate_dragon_attack_contract(errors: list[str]) -> None:
    try:
        if not DRAGON_MESH_PATH.is_file():
            raise ValueError(f"missing Dragon mesh: {DRAGON_MESH_PATH}")

        skeleton_path = DRAGON_MESH_PATH.with_suffix(".wskel")
        if not skeleton_path.is_file():
            raise ValueError(f"missing Dragon skeleton: {skeleton_path}")
        if not DRAGON_ATTACK_PATH.is_file():
            raise ValueError(f"missing Dragon attack clip: {DRAGON_ATTACK_PATH}")

        skeleton_hash = read_wskel_hash(skeleton_path)
        duration_ticks, ticks_per_second = read_valid_wanim(
            DRAGON_ATTACK_PATH, skeleton_hash)
        if duration_ticks <= 0.0:
            raise ValueError(
                f"Dragon attack clip has no duration: {DRAGON_ATTACK_PATH}")

        attack_payload = read_wint_payload(DRAGON_ATTACK_PATH)
        _, channel_count, _, _, _, event_count = struct.unpack_from(
            "<4sIffII", attack_payload, 0)
        if channel_count == 0:
            raise ValueError(
                f"Dragon attack clip has no animation channels: {DRAGON_ATTACK_PATH}")

        manager_source = JUNGLE_MANAGER_PATH.read_text(encoding="utf-8")
        exact_mapping = (
            'return { "sru_dragon_flying_run", "sru_dragon_flying_run", '
            '"", "" };')
        if exact_mapping not in manager_source:
            raise ValueError(
                "Jungle_Manager no longer keeps Dragon BasicAttack on the "
                "stable flying loop")

        available_clips = sorted(
            path.stem for path in DRAGON_ATTACK_PATH.parent.glob("*.wanim"))
        has_hit_reaction = any(
            token in clip.lower()
            for clip in available_clips
            for token in ("hit", "damage", "react"))
        has_death = any("death" in clip.lower() for clip in available_clips)
        print(
            "[BasicAttackTiming] DRAGON "
            f"clip={DRAGON_ATTACK_PATH.name} channels={channel_count} "
            f"events={event_count} skeletonHash=0x{skeleton_hash:016x} "
            f"TPS={ticks_per_second:.3f} durationTicks={duration_ticks:.3f} "
            f"hitReactionClip={has_hit_reaction} deathClip={has_death}")
    except (OSError, struct.error, ValueError) as exc:
        errors.append(f"DRAGON: {exc}")


def main() -> int:
    gameplay = load_json(GAMEPLAY_PATH)
    visuals = load_json(VISUAL_PATH)
    assets = load_json(ASSET_PATH)

    visual_by_champion = {
        str(entry["key"]).split(".")[-1].upper(): entry
        for entry in visuals["champions"]
    }
    model_by_champion = {
        str(entry["champion"]).upper(): entry
        for entry in assets["models"]
    }

    errors: list[str] = []
    aligned_count = 0
    fallback_count = 0
    observed_fallbacks: set[str] = set()

    for champion in gameplay["champions"]:
        name = str(champion["champion"]).upper()
        try:
            visual = visual_by_champion[name]
            model = model_by_champion[name]
            basic_visual = next(
                skill for skill in visual["skills"]
                if str(skill["key"]).endswith(".basic_attack"))
            stage = basic_visual["stages"][0]
            playback = float(stage["animationPlaybackSpeed"])
            cast_frame = float(stage["castFrame"])
            recovery_frame = float(stage["recoveryFrame"])
            windup = float(champion["stats"]["basicAttackWindupSec"])
            basic_gameplay = next(
                skill for skill in champion["skills"]
                if int(skill["slot"]) == 0)
            lock = float(basic_gameplay["stages"][0]["lockDurationSec"])
            (
                clip_path,
                query,
                duration_ticks,
                ticks_per_second,
            ) = resolve_basic_attack_clip(model)
        except (KeyError, StopIteration, TypeError, ValueError) as exc:
            errors.append(f"{name}: {exc}")
            continue

        if not math.isfinite(playback) or playback <= 0.0:
            errors.append(f"{name}: invalid playback {playback}")
            continue

        if cast_frame > 0.0:
            expected_windup = min(cast_frame, duration_ticks) / ticks_per_second / playback
            if abs(windup - expected_windup) > FLOAT_TOLERANCE:
                errors.append(
                    f"{name}: windup={windup:.6f}, expected={expected_windup:.6f} "
                    f"from cast={cast_frame:.3f}/TPS={ticks_per_second:.3f}/play={playback:.3f}")
            else:
                aligned_count += 1
        else:
            observed_fallbacks.add(name)
            fallback_count += 1
            if name not in ZERO_MARKER_FALLBACKS or windup <= 0.0:
                errors.append(f"{name}: unexpected zero cast marker/fallback windup={windup:.6f}")

        if windup > lock + FLOAT_TOLERANCE:
            errors.append(f"{name}: windup={windup:.6f} exceeds action lock={lock:.6f}")

        if recovery_frame > 0.0:
            recovery = min(recovery_frame, duration_ticks) / ticks_per_second / playback
            if recovery > lock + FLOAT_TOLERANCE:
                errors.append(
                    f"{name}: recovery={recovery:.6f} exceeds action lock={lock:.6f} "
                    "and would be truncated by the client clamp")
        elif name not in ZERO_MARKER_FALLBACKS:
            errors.append(f"{name}: unexpected zero recovery marker")

        print(
            f"[BasicAttackTiming] {name:<9} clip={clip_path.name} query={query} "
            f"TPS={ticks_per_second:.3f} durationTicks={duration_ticks:.3f} "
            f"cast={cast_frame:.3f} windup={windup:.6f} lock={lock:.6f}")

    if observed_fallbacks != ZERO_MARKER_FALLBACKS:
        errors.append(
            f"zero-marker fallback set={sorted(observed_fallbacks)}, "
            f"expected={sorted(ZERO_MARKER_FALLBACKS)}")

    validate_dragon_attack_contract(errors)

    if errors:
        for error in errors:
            print(f"[BasicAttackTiming] FAIL: {error}", file=sys.stderr)
        return 1

    print(
        f"[BasicAttackTiming] PASS: champions={len(gameplay['champions'])} "
        f"asset-marker-aligned={aligned_count} explicit-fallbacks={fallback_count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
