Session - Elden 복원 계획을 Atom/Probe/Report 세 원자로 축소한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Tools/EldenAssetPipeline/elden_pipeline.py

`convert_hkx_anim_command` 함수 바로 아래에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```python
    write_json(Path(args.out), manifest, args.pretty)
    return 0 if manifest["counts"]["failed"] == 0 else 2
```

아래에 추가:

```python
def FirstWmesh(value: Any) -> str:
    if isinstance(value, str):
        return value if value.lower().endswith(".wmesh") else ""
    if isinstance(value, dict):
        for item in value.values():
            found = FirstWmesh(item)
            if found:
                return found
    if isinstance(value, list):
        for item in value:
            found = FirstWmesh(item)
            if found:
                return found
    return ""


def BuildPayloadIndex(catalog: Any) -> dict[str, str]:
    index: dict[str, str] = {}

    def Visit(value: Any) -> None:
        if isinstance(value, dict):
            wmesh = FirstWmesh(value)
            if wmesh:
                raw_id = str(value.get("id") or value.get("model") or value.get("character") or "")
                for token in re.findall(r"(AEG\d{3}_\d{3}|c\d{4})", raw_id):
                    index.setdefault(token.lower(), wmesh)
                if raw_id:
                    index.setdefault(raw_id.lower(), wmesh)
            for item in value.values():
                Visit(item)
        elif isinstance(value, list):
            for item in value:
                Visit(item)

    Visit(catalog)
    return index


def DetectRestorationDomain(source_id: str) -> str:
    if source_id.startswith("AEG"):
        return "StaticProp"
    if re.fullmatch(r"c\d{4}", source_id):
        return "Character"
    return "Unknown"


def WalkRestorationSources(value: Any) -> Iterable[tuple[str, int]]:
    if isinstance(value, dict):
        source_id = value.get("model") or value.get("sourceId") or value.get("character")
        if isinstance(source_id, str) and source_id:
            try:
                yield source_id, int(value.get("count") or value.get("sourceCount") or 1)
            except (TypeError, ValueError):
                yield source_id, 1
        for item in value.values():
            yield from WalkRestorationSources(item)
    elif isinstance(value, list):
        for item in value:
            yield from WalkRestorationSources(item)


def CountFiles(root: Path, suffixes: set[str]) -> int:
    if not root.exists():
        return 0
    lowered = {suffix.lower() for suffix in suffixes}
    return sum(1 for path in root.rglob("*") if path.is_file() and path.suffix.lower() in lowered)


def MakeRestorationAtom(domain: str, source_id: str, count: int, payload: str, open_reason: str) -> dict[str, Any]:
    return {
        "domain": domain,
        "sourceId": source_id,
        "count": count,
        "payload": payload,
        "openReason": open_reason,
        "closed": bool(payload) and not open_reason,
    }


def BuildRestorationSummary(atoms: list[dict[str, Any]]) -> dict[str, Any]:
    by_domain: Counter[str] = Counter(str(atom.get("domain") or "Unknown") for atom in atoms)
    by_reason: Counter[str] = Counter(
        str(atom.get("openReason") or "Closed")
        for atom in atoms
    )
    return {
        "atoms": len(atoms),
        "closed": sum(1 for atom in atoms if atom.get("closed")),
        "open": sum(1 for atom in atoms if not atom.get("closed")),
        "byDomain": dict(sorted(by_domain.items())),
        "byReason": dict(sorted(by_reason.items())),
    }


def BuildRestorationClosureCommand(args: argparse.Namespace) -> int:
    resource_root = Path(args.resource_root)
    payload_index = BuildPayloadIndex(load_json_optional(Path(args.catalog)))
    counts: Counter[tuple[str, str]] = Counter()

    for source_path in args.source:
        source = load_json_optional(Path(source_path))
        for source_id, count in WalkRestorationSources(source):
            domain = DetectRestorationDomain(source_id)
            if domain != "Unknown":
                counts[(domain, source_id)] += count

    atoms: list[dict[str, Any]] = []
    for (domain, source_id), count in sorted(counts.items()):
        payload = payload_index.get(source_id.lower(), "")
        reason = "" if payload else "MissingRuntimePayload"
        atoms.append(MakeRestorationAtom(domain, source_id, count, payload, reason))

    if args.include_raw_domains:
        collision_count = CountFiles(resource_root / "FullGame" / "map", {".hkxbhd", ".hkxbdt", ".nvmhktbnd"})
        atoms.append(MakeRestorationAtom("CollisionNav", "RawCollisionNav", collision_count, "", "NoRuntimePayload"))

        event_count = CountFiles(resource_root / "FullGame" / "event", {".emevd", ".xml"})
        script_count = CountFiles(resource_root / "FullGame" / "script", {".lua", ".luagnl", ".xml"})
        param_count = CountFiles(resource_root / "FullGame" / "param", {".param", ".xml"})
        atoms.append(
            MakeRestorationAtom(
                "EventAiGameplay",
                "RawEventAiGameplay",
                event_count + script_count + param_count,
                "",
                "NoRuntimePayload",
            )
        )

    manifest = {
        "schema": "winters.elden.restoration_closure.v1",
        "generatedAt": now_utc_iso(),
        "generatedBy": "Tools/EldenAssetPipeline/elden_pipeline.py build-restoration-closure",
        "essence": "Atom -> Payload -> Closed",
        "counts": BuildRestorationSummary(atoms),
        "atoms": atoms,
    }
    write_json(Path(args.out), manifest, args.pretty)
    return 0
```

`build_parser` 함수에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```python
    audit.set_defaults(func=audit_full_pipeline_command)
```

아래에 추가:

```python
    closure = sub.add_parser(
        "build-restoration-closure",
        help="Reduce Elden restoration to Atom -> Payload -> Closed records.",
    )
    closure.add_argument("--resource-root", required=True, help="Resource/EldenRing root.")
    closure.add_argument("--catalog", required=True, help="eldenring_resource_catalog.json path.")
    closure.add_argument("--source", action="append", required=True, help="Source manifest with model/count records.")
    closure.add_argument(
        "--include-raw-domains",
        action="store_true",
        help="Add raw-only CollisionNav and EventAiGameplay atoms.",
    )
    add_common_json_args(closure)
    closure.set_defaults(func=BuildRestorationClosureCommand)
```

2. 검증

검증 명령:
- `python -m py_compile Tools/EldenAssetPipeline/elden_pipeline.py`
- `python Tools/EldenAssetPipeline/elden_pipeline.py build-restoration-closure --resource-root Client/Bin/Resource/EldenRing --catalog Client/Bin/Resource/EldenRing/Manifests/eldenring_resource_catalog.json --source Client/Bin/Resource/EldenRing/Manifests/limgrave_unresolved_cook_candidates.json --source Client/Bin/Resource/EldenRing/Maps/StartingCave/starting_cave_vertical_slice_manifest.json --include-raw-domains --out Client/Bin/Resource/EldenRing/Manifests/elden_restoration_closure.json --pretty`
- `git diff --check -- Tools/EldenAssetPipeline/elden_pipeline.py Plan/S01_ELDEN_RING_RESTORATION_ESSENCE_PLAN.md`

확인 필요:
- `Atom -> Payload -> Closed` 밖의 정보가 필요해지면 S01에 넣지 말고 S02로 분리한다.
- `CollisionNav`와 `EventAiGameplay`은 S01에서 raw-only atom으로만 남긴다.
- Client ImGui 표시는 S01의 본질이 아니므로 추가하지 않는다.
