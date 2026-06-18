# map11_brush_volumes.csv -> map11_brush_volumes.wbrush (v1) 쿡 스크립트.
# 포맷: u32 magic 'WBSH' | u32 version=1 | u32 count | u32 reserved
#       entry[count]: u32 bushId | f32 worldX | f32 worldZ | f32 radius
# TODO(wmap): Stage 데이터 .wmap 통합 시 BushEntry(07_STAGE6_WMAP.md)로 승격.
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CSV = ROOT / "Client/Bin/Resource/Texture/MAP/Map11_Rebuild/manifest/map11_brush_volumes.csv"
OUT = ROOT / "Client/Bin/Resource/Texture/MAP/Map11_Rebuild/cooked/map11_brush_volumes.wbrush"

MAGIC = 0x48534257  # 'WBSH'
VERSION = 1

entries = []
for line in CSV.read_text(encoding="utf-8").splitlines():
    line = line.strip()
    if not line or line.startswith("#"):
        continue
    parts = [p.strip() for p in line.split(",")]
    if len(parts) != 4:
        raise SystemExit(f"bad row: {line!r}")
    bush_id = int(parts[0])
    x, z, radius = (float(parts[1]), float(parts[2]), float(parts[3]))
    if radius <= 0.0:
        raise SystemExit(f"bad radius: {line!r}")
    entries.append((bush_id, x, z, radius))

payload = struct.pack("<IIII", MAGIC, VERSION, len(entries), 0)
for bush_id, x, z, radius in entries:
    payload += struct.pack("<Ifff", bush_id, x, z, radius)

OUT.write_bytes(payload)
print(f"cooked {len(entries)} brush volumes -> {OUT} ({len(payload)} bytes)")
