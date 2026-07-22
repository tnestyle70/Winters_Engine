"""Repair degenerate Yasuo Q3 position tracks in a cooked .wanim clip.

The affected source exports meaningful skeleton joints with all-(0,0,0)
position keys. CAnimation::Evaluate applies those keys verbatim over the rest
pose, collapsing the skinned mesh while Q3 plays. This targeted post-cook
repair writes each affected position track from the matching .wskel rest
translation.

Usage:
  python Tools/Anim/patch_wanim_root_track.py <spell1_wind.wanim> <yasuo.wskel>
  python Tools/Anim/patch_wanim_root_track.py <spell1_wind.wanim> <yasuo.wskel> --dry-run
"""

import hashlib
import math
import os
import struct
import sys

ZERO_EPS = 1e-4
REST_MIN_LEN = 1.0
TARGET_CLIP = "skinned_mesh_yasuo_spell1_wind.wanim"


def read_file(path):
    with open(path, "rb") as stream:
        return stream.read()


def parse_wskel(path):
    data = read_file(path)
    magic, version_major, _version_minor, flags, content_size = struct.unpack_from(
        "<4sHHII", data, 0)
    if magic != b"WINT" or version_major != 1 or flags != 0:
        raise ValueError(f"invalid WSkel header: {path}")
    if content_size > len(data) - 16:
        raise ValueError(f"truncated WSkel payload: {path}")

    offset = 16
    skeleton_magic, bone_count, _socket_count = struct.unpack_from(
        "<4sII", data, offset)
    if skeleton_magic != b"WSKL" or bone_count == 0 or bone_count > 1024:
        raise ValueError(f"invalid WSkel metadata: {path}")
    offset += 32

    bones = {}
    skeleton_hash = 0xCBF29CE484222325
    for _ in range(bone_count):
        if offset + 256 > len(data):
            raise ValueError(f"truncated WSkel bone table: {path}")
        name_hash, = struct.unpack_from("<Q", data, offset)
        name = data[offset + 8:offset + 72].split(b"\0")[0].decode(
            "ascii", "replace")
        rest = struct.unpack_from("<16f", data, offset + 76)
        bones[name_hash] = (name, (rest[12], rest[13], rest[14]))
        skeleton_hash ^= name_hash
        skeleton_hash = (skeleton_hash * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
        offset += 256
    return bones, skeleton_hash


def parse_wanim_channels(data):
    if len(data) < 16 + 32 + 8:
        raise ValueError("truncated WAnim")
    magic, version_major, _version_minor, flags, content_size = struct.unpack_from(
        "<4sHHII", data, 0)
    if magic != b"WINT" or version_major != 1 or flags != 0:
        raise ValueError("invalid WAnim header")
    if content_size > len(data) - 16:
        raise ValueError("truncated WAnim payload")

    payload_base = 16
    anim_magic, channel_count = struct.unpack_from("<4sI", data, payload_base)
    if anim_magic != b"WANM":
        raise ValueError("invalid WAnim metadata")
    channel_base = payload_base + 32
    key_block_base = channel_base + 40 * channel_count
    trailer_offset = payload_base + content_size - 8
    if key_block_base > trailer_offset:
        raise ValueError("invalid WAnim channel table")

    channels = []
    for index in range(channel_count):
        channel = struct.unpack_from(
            "<QIIIIIIII", data, channel_base + 40 * index)
        bone_hash, position_count, position_offset = channel[:3]
        position_end = key_block_base + position_offset + 16 * position_count
        if position_end > trailer_offset:
            raise ValueError("position keys exceed WAnim payload")
        channels.append((bone_hash, position_count, position_offset))
    skeleton_hash, = struct.unpack_from("<Q", data, trailer_offset)
    return channels, key_block_base, skeleton_hash


def find_zeroed_root_channels(data, bones):
    channels, key_block_base, _skeleton_hash = parse_wanim_channels(data)
    hits = []
    for bone_hash, position_count, position_offset in channels:
        bone = bones.get(bone_hash)
        if bone is None or position_count == 0:
            continue
        name, rest = bone
        if not all(math.isfinite(value) for value in rest):
            raise ValueError(f"non-finite rest translation: {name}")
        if math.sqrt(sum(value * value for value in rest)) < REST_MIN_LEN:
            continue
        positions = [
            struct.unpack_from(
                "<4f", data, key_block_base + position_offset + 16 * key)[1:]
            for key in range(position_count)
        ]
        if not all(
                math.isfinite(value)
                for position in positions
                for value in position):
            raise ValueError(f"non-finite position track: {name}")
        if all(
                all(abs(value) <= ZERO_EPS for value in position)
                for position in positions):
            hits.append(
                (name, position_count, position_offset, rest, key_block_base))
    return hits


def patch_file(wanim_path, wskel_path, dry_run):
    if os.path.basename(wanim_path).lower() != TARGET_CLIP:
        raise ValueError("repair is restricted to Yasuo spell1_wind.wanim")

    bones, expected_skeleton_hash = parse_wskel(wskel_path)
    original = read_file(wanim_path)
    data = bytearray(original)
    _channels, _key_block_base, actual_skeleton_hash = parse_wanim_channels(data)
    if actual_skeleton_hash != expected_skeleton_hash:
        raise ValueError(
            f"skeleton hash mismatch: wanim={actual_skeleton_hash:016x} "
            f"wskel={expected_skeleton_hash:016x}")

    hits = find_zeroed_root_channels(data, bones)
    if not hits:
        print(f"[ok] no degenerate position track: {wanim_path}")
        return 0
    for name, position_count, position_offset, rest, key_block_base in hits:
        print(f"[patch] {os.path.basename(wanim_path)} ch={name} "
              f"posKeys={position_count} -> "
              f"rest=({rest[0]:.3f}, {rest[1]:.3f}, {rest[2]:.3f})")
        if dry_run:
            continue
        for key in range(position_count):
            key_offset = key_block_base + position_offset + 16 * key
            struct.pack_into("<3f", data, key_offset + 4, *rest)

    if dry_run:
        print(f"[dry-run] {len(hits)} channel(s) would be patched")
        return len(hits)

    digest = hashlib.sha256(original).hexdigest()[:12]
    backup = f"{wanim_path}.{digest}.bak"
    if not os.path.exists(backup):
        with open(backup, "wb") as stream:
            stream.write(original)
    temporary = f"{wanim_path}.tmp"
    with open(temporary, "wb") as stream:
        stream.write(data)
    os.replace(temporary, wanim_path)
    print(f"[done] patched {len(hits)} channel(s), backup: {backup}")
    return len(hits)


def main(argv):
    if len(argv) not in (2, 3) or (len(argv) == 3 and argv[2] != "--dry-run"):
        print(__doc__)
        return 2
    try:
        patch_file(argv[0], argv[1], len(argv) == 3)
    except (OSError, ValueError, struct.error) as error:
        print(f"[error] {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
