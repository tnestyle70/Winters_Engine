# map11 앰비언트 프롭(새/오리/반딧불) 배치 추출 + 쿡 스크립트.
# 1) LeagueToolkitProbe levelprops로 base.materials.bin에서 LevelProp 배치(LoL 좌표) 추출
# 2) manifest/map11_ambient_props.csv (문서용) + cooked/map11_ambient_props.wamb (런타임용) 생성
#    .wamb v1: u32 'WAMB' | u32 version=1 | u32 count | u32 reserved
#              entry: u32 kind(0=bird,1=duck,2=firefly) | f32 lolX,lolY,lolZ | f32 lolYaw(rad) | f32 scale
#    좌표는 LoL 공간 그대로 저장 — 클라이언트가 canonical Stage 좌표로 변환한다.
# 3) cooked/ambient/<name>/<name>.wmesh + anims/ 레이아웃으로 복사 (CModel의 anims/ 규약)
import math
import shutil
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROBE = ROOT / "Tools/External/LeagueToolkitProbe/bin/Debug/net10.0/LeagueToolkitProbe.exe"
MATERIALS_BIN = ROOT / "Client/Bin/Resource/Texture/MAP/data/maps/mapgeometry/map11/base.materials.bin"
REBUILD = ROOT / "Client/Bin/Resource/Texture/MAP/Map11_Rebuild"
CSV_OUT = REBUILD / "manifest/map11_ambient_props.csv"
BIN_OUT = REBUILD / "cooked/map11_ambient_props.wamb"

KINDS = {
    "levelprop_sru_bird": 0,
    "levelprop_sru_duck": 1,
    "audio-emitter_sru_insects": 2,
}

out = subprocess.run(
    [str(PROBE), "levelprops", str(MATERIALS_BIN), "sru_bird,sru_duck,sru_insects"],
    capture_output=True, text=True, check=True).stdout

entries = []
for line in out.splitlines():
    parts = line.strip().split(",")
    if len(parts) != 13:
        continue
    name = parts[0]
    key = next((k for k in KINDS if name.lower().startswith(k)), None)
    if key is None:
        continue
    tx, ty, tz = (float(parts[1]), float(parts[2]), float(parts[3]))
    m11, m12, m13, m21, m22, m23, m31, m32, m33 = (float(v) for v in parts[4:13])
    yaw = math.atan2(m31, m33)
    scale = (math.hypot(m11, m13) + math.hypot(m31, m33)) * 0.5
    if scale < 0.05:
        scale = 1.0
    entries.append((name, KINDS[key], tx, ty, tz, yaw, scale))

if not entries:
    raise SystemExit("no ambient prop placements extracted")

entries.sort(key=lambda e: (e[1], e[0]))

CSV_OUT.write_text(
    "# schema: name,kind(0=bird/1=duck/2=firefly),lolX,lolY,lolZ,lolYawRad,scale\n"
    "# source: data/maps/mapgeometry/map11/base.materials.bin (LevelProp/Audio-Emitter transforms)\n"
    + "".join(
        f"{n},{k},{x:.3f},{y:.3f},{z:.3f},{yaw:.4f},{s:.4f}\n"
        for n, k, x, y, z, yaw, s in entries),
    encoding="utf-8")

payload = struct.pack("<IIII", 0x424D4157, 1, len(entries), 0)  # 'WAMB'
for _, k, x, y, z, yaw, s in entries:
    payload += struct.pack("<Ifffff", k, x, y, z, yaw, s)
BIN_OUT.write_bytes(payload)

# 에셋 레이아웃: <name>/<name>.{wmesh,wskel,wmat} + <name>/anims/*.wanim
for name in ("sru_bird", "sru_duck", "chemtech_firefly_animated"):
    src = REBUILD / "cooked/ambient"
    dst = src / name
    (dst / "anims").mkdir(parents=True, exist_ok=True)
    for ext in (".wmesh", ".wskel", ".wmat"):
        shutil.copy2(src / f"{name}{ext}", dst / f"{name}{ext}")
    for anim in (src / f"{name}_anim").glob("*.wanim"):
        shutil.copy2(anim, dst / "anims" / anim.name)

print(f"cooked {len(entries)} ambient props -> {BIN_OUT} ({len(payload)} bytes)")
for n, k, x, y, z, yaw, s in entries:
    print(f"  kind={k} {n} lol=({x:.0f},{y:.0f},{z:.0f}) yaw={math.degrees(yaw):.0f}deg scale={s:.2f}")
