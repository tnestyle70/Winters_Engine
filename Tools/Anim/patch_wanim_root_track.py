"""Patch zeroed root-bone position tracks in cooked .wanim clips.

A cooked clip that ships all-(0,0,0) position keys for a root-family bone
collapses the model to the mesh origin while that clip plays (2026-07-15
Yasuo spell1_wind ground-sink). CAnimation::Evaluate applies channel keys
verbatim over the rest pose, so the repair is to rewrite the degenerate keys
with the bone's .wskel rest translation.

Usage:
  # patch one clip (writes <file>.bak first)
  python Tools/Anim/patch_wanim_root_track.py <clip.wanim> <skeleton.wskel>

  # dry-run: report only, no write
  python Tools/Anim/patch_wanim_root_track.py <clip.wanim> <skeleton.wskel> --dry-run

  # audit every champion under the Character root, report only
  python Tools/Anim/patch_wanim_root_track.py --audit Client/Bin/Resource/Texture/Character
"""

import os
import struct
import sys

ROOT_BONES = ("Root", "Root_Upper", "Root_Lower", "Pelvis", "root", "hips")
ZERO_EPS = 1e-4
REST_MIN_LEN = 1.0  # only treat as defect when rest translation is meaningful


def read_file(path):
    with open(path, "rb") as f:
        return f.read()


def parse_wskel(path):
    d = read_file(path)
    magic, _vmaj, _vmin, _flags, _csize = struct.unpack_from("<4sHHII", d, 0)
    assert magic == b"WINT", (path, magic)
    off = 16
    smagic, bone_count, _socket_count = struct.unpack_from("<4sII", d, off)
    assert smagic == b"WSKL", (path, smagic)
    off += 32
    bones = {}
    for _ in range(bone_count):
        name_hash, = struct.unpack_from("<Q", d, off)
        name = d[off + 8:off + 8 + 64].split(b"\0")[0].decode("ascii", "replace")
        rest = struct.unpack_from("<16f", d, off + 76)
        # row-major local rest matrix: translation = m30 m31 m32
        bones[name_hash] = (name, (rest[12], rest[13], rest[14]))
        off += 256
    return bones


def parse_wanim_channels(data):
    magic, _vmaj, _vmin, _flags, csize = struct.unpack_from("<4sHHII", data, 0)
    assert magic == b"WINT", magic
    payload_base = 16
    amagic, channel_count = struct.unpack_from("<4sI", data, payload_base)
    assert amagic == b"WANM", amagic
    chan_base = payload_base + 32
    chans = []
    for i in range(channel_count):
        bone_hash, pos_n, pos_off = struct.unpack_from(
            "<QII", data, chan_base + 40 * i)
        chans.append((bone_hash, pos_n, pos_off))
    key_block_base = chan_base + 40 * channel_count
    return chans, key_block_base, csize


def find_zeroed_root_channels(data, bones):
    chans, key_block_base, _ = parse_wanim_channels(data)
    hits = []
    for bone_hash, pos_n, pos_off in chans:
        name, rest = bones.get(bone_hash, ("?%016x" % bone_hash, (0.0, 0.0, 0.0)))
        if name not in ROOT_BONES or pos_n == 0:
            continue
        all_zero = True
        for k in range(pos_n):
            _t, x, y, z = struct.unpack_from(
                "<4f", data, key_block_base + pos_off + 16 * k)
            if abs(x) > ZERO_EPS or abs(y) > ZERO_EPS or abs(z) > ZERO_EPS:
                all_zero = False
                break
        rest_len = (rest[0] ** 2 + rest[1] ** 2 + rest[2] ** 2) ** 0.5
        if all_zero and rest_len >= REST_MIN_LEN:
            hits.append((name, pos_n, pos_off, rest, key_block_base))
    return hits


def patch_file(wanim_path, wskel_path, dry_run):
    bones = parse_wskel(wskel_path)
    data = bytearray(read_file(wanim_path))
    hits = find_zeroed_root_channels(data, bones)
    if not hits:
        print(f"[ok] no zeroed root track: {wanim_path}")
        return 0
    for name, pos_n, pos_off, rest, key_block_base in hits:
        print(f"[patch] {os.path.basename(wanim_path)} ch={name} posKeys={pos_n} "
              f"-> rest=({rest[0]:.3f}, {rest[1]:.3f}, {rest[2]:.3f})")
        if dry_run:
            continue
        for k in range(pos_n):
            key_off = key_block_base + pos_off + 16 * k
            struct.pack_into("<3f", data, key_off + 4, rest[0], rest[1], rest[2])
    if dry_run:
        print(f"[dry-run] {len(hits)} channel(s) would be patched")
        return len(hits)
    bak = wanim_path + ".bak"
    if not os.path.exists(bak):
        with open(bak, "wb") as f:
            f.write(read_file(wanim_path))
    with open(wanim_path, "wb") as f:
        f.write(bytes(data))
    print(f"[done] patched {len(hits)} channel(s), backup: {bak}")
    return len(hits)


def audit(character_root):
    total = 0
    for champ in sorted(os.listdir(character_root)):
        champ_dir = os.path.join(character_root, champ)
        anims_dir = os.path.join(champ_dir, "anims")
        if not os.path.isdir(anims_dir):
            continue
        wskels = [f for f in os.listdir(champ_dir) if f.endswith(".wskel")]
        if not wskels:
            continue
        bones = parse_wskel(os.path.join(champ_dir, wskels[0]))
        for clip in sorted(os.listdir(anims_dir)):
            if not clip.endswith(".wanim"):
                continue
            path = os.path.join(anims_dir, clip)
            try:
                hits = find_zeroed_root_channels(read_file(path), bones)
            except (AssertionError, struct.error) as e:
                print(f"[skip] unparsable {path}: {e}")
                continue
            for name, pos_n, _po, rest, _kb in hits:
                total += 1
                print(f"[defect] {champ}/{clip} ch={name} posKeys={pos_n} "
                      f"rest=({rest[0]:.2f}, {rest[1]:.2f}, {rest[2]:.2f})")
    print(f"[audit] zeroed-root channels found: {total}")
    return total


def main(argv):
    if len(argv) >= 2 and argv[0] == "--audit":
        audit(argv[1])
        return 0
    if len(argv) < 2:
        print(__doc__)
        return 2
    dry = "--dry-run" in argv[2:]
    patch_file(argv[0], argv[1], dry)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
