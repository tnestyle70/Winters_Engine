# Limgrave Vertical Slice Reconstruction

## 목적

림그레이브 복원은 손으로 맵을 다시 배치하는 작업이 아니다.
원작 map binary/MSB가 가진 placement record를 source of truth로 삼고, Winters runtime contract로 cook된 에셋을 자동 로드한 뒤, editor는 검수와 override만 담당한다.

```text
MSB/map binary placement
  -> model id, part kind, position, rotation, scale
  -> cooked .wmesh/.wmat/.wskel/.wanim/DDS lookup
  -> placement JSON/TXT
  -> runtime/editor load
  -> missing asset, transform, material, collision validation
```

## 현재 Vertical Slice 기준

계약 파일:

```text
Client/Bin/Resource/EldenRing/Maps/Limgrave/limgrave_vertical_slice_manifest.json
```

초점 타일:

| Tile | 역할 | 현재 상태 |
|---|---|---|
| `m60_42_36_00` | map assembly seed | placed 307, unresolved 451 |
| `m60_44_35_00` | actor lineup anchor | placed 234, unresolved 514 |
| `m60_42_37_00` | dense static neighbor | placed 460, unresolved 689 |

전체 Limgrave 현재 placement 감사:

| 항목 | 수량 |
|---|---:|
| placed records | 3862 |
| unresolved records | 7353 |
| placed MapPiece | 35 |
| placed Asset | 3787 |
| placed Player refs | 37 |
| placed Enemy refs | 3 |
| missing placed wmesh | 0 |

## 복원 가능 범위

### P0 Visual Geography

가능:
- MapPiece와 static Asset의 원작 위치/회전/스케일 재현
- `.wmesh/.wmat/DDS`가 있는 지형과 소품 로드
- 림그레이브 필드의 큰 실루엣과 공간 구조 검증

현재 상태:
- 핵심 map pieces와 3862개 placement가 loadable하다.
- unresolved 대부분은 AEG801 계열 식생/소품, 일부 몹/세부 에셋이다.

### P1 Animated Actors

가능:
- Enemy/NPC/Boss를 runtime character contract로 cook
- `.wmesh/.wskel/.wmat/anims/*.wanim/textures/*.dds` 조합으로 애니메이션 검증
- showcase actor placement JSON으로 stage에 배치

현재 상태:
- 기존 6종 + 신규 Limgrave mobs 6종, 총 12개 actor placement.
- 신규 animated mobs: `c6100`, `c6060`, `c6010`, `c2271`, `c4200`, `c4100`.
- `c6070`은 skel/mesh bone count mismatch.
- `c4300`은 skel conversion 실패.
- `c6001`, `c4311`은 mesh cook은 됐지만 primary anibnd에서 animation import 실패.

### P2 Material Fidelity

가능:
- diffuse 중심 `.wmat`로 기본 시각 검증
- MATBIN/TPF/AET를 통해 더 정확한 material slot으로 확장

남은 것:
- normal/mask/emissive/roughness 같은 multi-slot `.wmat`
- MATBIN shader parameter 의미 반영
- AET/map texture gap 처리

### P3 Collision And Navigation

가능 방향:
- collision hkxbhd/bdt와 navmesh nvmhktbnd를 Winters collision/nav format으로 변환
- editor overlay로 walkable/collision mismatch 검증

현재 상태:
- raw 수집/큐 인식 수준이며 runtime collision/nav로는 아직 미변환.

### P4 Gameplay Events

가능 방향:
- PARAM/regulation, EMEVD, TAE event, behavior HKX를 읽어 slice용 gameplay trigger로 재해석

현재 상태:
- visual vertical slice 범위 밖이다.
- 원작 전체 gameplay behavior 복원보다, Winters식 action RPG slice로 필요한 데이터만 단계적으로 흡수한다.

## 에디터 역할

에디터는 원작 placement를 대체하지 않는다.
에디터는 다음 작업을 담당한다.

1. Asset Catalog: model id -> cooked resource 상태 표시
2. Map Cell View: tile별 placed/unresolved/missing 분류
3. Transform Inspector: 원본 MSB transform과 Winters axis/unit 보정 분리 표시
4. Missing Cook Queue: unresolved 상위 모델을 run-full/retry-missing-wmesh 대상으로 내보내기
5. Material Preview: diffuse-only, missing texture, multi-slot 상태 표시
6. Collision/Nav Overlay: 시각 mesh와 물리/이동 mesh의 mismatch 표시
7. Override Layer: 수동 수정은 별도 JSON으로 저장하고 source placement는 보존

## 실제 개발팀식 작업 방식

FromSoftware나 GTA급 팀이 하는 일도 본질은 같다.

- Tools engineers: 원본/에디터 binary를 runtime format으로 cook하는 importer, validator, commandlet 제작
- Environment artists: asset library, material, LOD/HLOD, collision, kit 조립 품질 관리
- Level designers: enemy/spawner/route/region/event 배치와 encounter tuning
- Technical artists: shader parameter, foliage/scatter, material instance, streaming budget 관리
- Gameplay designers: PARAM/AI/event/animation notify를 게임 규칙으로 연결
- QA/build pipeline: missing reference, bad transform, collision hole, streaming budget을 자동 리포트

따라서 Winters도 같은 형태로 간다.
원작 data를 자동 배치하고, editor는 사람이 판단해야 하는 품질 문제만 드러낸다.

## 다음 작업

1. `limgrave_vertical_slice_manifest.json`을 editor/runtime panel에서 읽는다.
2. focus tile 3개를 우선 표시하고, 전체 16타일 로드는 선택 모드로 둔다.
3. unresolved 상위 `AEG801_*`는 `retry-missing-wmesh` 또는 `run-full-pipeline` 대상 큐로 보낸다.
4. `Enemy` MSB placement를 Runtime Character lookup에 연결해서 수동 showcase placement 의존을 줄인다.
5. material/collision/nav는 visual slice 검증 뒤 순차적으로 붙인다.
