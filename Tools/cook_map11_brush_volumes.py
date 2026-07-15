# Map11 centered brush authoring CSV -> research-only canonical WBRUSH v1.
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CSV = ROOT / "Client/Bin/Resource/Texture/MAP/Map11_Rebuild/manifest/map11_brush_volumes.csv"
OUT = ROOT / "Client/Bin/Resource/Texture/MAP/Map11_Rebuild/cooked/map11_brush_volumes.wbrush"

WBRUSH_MAGIC = 0x48534257  # 'WBSH'
WBRUSH_VERSION = 1
MAP11_STAGE_CENTER_X = 104.50


def read_entries():
    entries = []
    for line in CSV.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = [part.strip() for part in line.split(",")]
        if len(parts) != 4:
            raise SystemExit(f"bad row: {line!r}")
        bush_id = int(parts[0])
        local_x, local_z, radius = (float(parts[1]), float(parts[2]), float(parts[3]))
        if bush_id <= 0 or radius <= 0.0:
            raise SystemExit(f"bad brush record: {line!r}")
        entries.append((bush_id, local_x + MAP11_STAGE_CENTER_X, local_z, radius))
    if not entries:
        raise SystemExit("no Map11 brush records")
    return entries


def main():
    entries = read_entries()
    payload = struct.pack("<IIII", WBRUSH_MAGIC, WBRUSH_VERSION, len(entries), 0)
    for bush_id, world_x, world_z, radius in entries:
        payload += struct.pack("<Ifff", bush_id, world_x, world_z, radius)
    OUT.write_bytes(payload)
    print(f"cooked {len(entries)} research brush volumes -> {OUT} ({len(payload)} bytes)")


if __name__ == "__main__":
    main()
