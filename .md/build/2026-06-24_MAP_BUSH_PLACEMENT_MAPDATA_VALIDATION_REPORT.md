# Session - 2026-06-24 Map Bush Placement MapData Validation Report

## 요약

부쉬 배치 방향을 `NavGrid에 섞는 방식`이 아니라 `StageData BushEntry에 저장하고 Editor/InGame/Vision이 같은 데이터를 읽는 방식`으로 정리했다.

이번 세션은 설계 문서 작성과 현재 코드/자산/빌드 기준 검증까지 수행했다. 런타임 배치 구현은 아직 하지 않았으므로 editor roundtrip과 InGame visual smoke는 다음 구현 단계의 검증 항목으로 남긴다.

## 작성 문서

- `.md/plan/2026-06-24_MAP_BUSH_PLACEMENT_MAPDATA_PLAN.md`

## 핵심 원인 정리

현재 부쉬 관련 데이터는 한 곳에 수렴되어 있지 않다.

- InGame은 `.wbrush`, CSV, hardcoded fallback으로 `BushVolumeComponent`를 seed한다.
- Editor의 Stage `.dat`는 `Structure`, `Jungle`, `MinionWaypoint`만 저장한다.
- Vision은 이미 `BushVolumeComponent`와 `CBushVolumeIndex`로 동작할 준비가 되어 있다.

가장 본질적인 문제는 `부쉬의 원본 데이터가 StageData가 아니라 임시 런타임 seed에 흩어져 있는 것`이다.

해결 방향은 `BushEntry`를 StageData의 유일한 원본으로 두는 것이다. PNG/FBX는 표시 자산이고, CSV는 import 재료이며, NavGrid는 이동 가능성 데이터이므로 부쉬의 원본이 아니다.

## 현재 코드 증거

| 영역 | 확인 내용 |
| --- | --- |
| StageData | `Shared/GameSim/Definitions/MapDataFormats.h`의 현재 `STAGE_VERSION = 4` |
| Client I/O | `Client/Private/Map/MapDataIO.cpp`가 `Structure -> Jungle -> MinionWaypoint`를 저장/로드 |
| Shared loader | `Shared/GameSim/Definitions/StageData.h`가 structures/jungles/minionWaypoints만 보유 |
| Editor | `Client/Private/Scene/Scene_Editor.cpp`에 `TryPickGroundPlane()`, `AddMode::NavGrid` 흐름 존재 |
| Vision | `BushVolumeComponent`, `CBushVolumeIndex`, `VisionSystem`의 부쉬 시야 판정 존재 |
| InGame seed | `Scene_InGameLifecycle.cpp`에 `.wbrush`/CSV/fallback 부쉬 seed 존재 |

## 검증 결과

| 검증 | 명령/대상 | 결과 |
| --- | --- | --- |
| 부쉬 PNG 존재 | `Test-Path .../sru_brush.png` | PASS, `True` |
| brush volume CSV 존재 | `Test-Path .../map11_brush_volumes.csv` | PASS, `True` |
| CSV 후보 수 | non-comment row count | PASS, `64` |
| 부쉬 FBX 후보 | `Client/Bin/Resource`에서 brush/bush/grass/foliage/shrub FBX 검색 | PASS, `NO_MATCH` |
| 계획 문서 whitespace | `git diff --check -- .md/plan/2026-06-24_MAP_BUSH_PLACEMENT_MAPDATA_PLAN.md` | PASS |
| Client Debug x64 build | `MSBuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m` | PASS, warning 76, error 0, 00:00:09.80 |
| Server Debug x64 build | `MSBuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m` | PASS, warning 2, error 0, 00:00:14.04 |

## 빌드 경고

Client와 Server 모두 빌드는 성공했다. 경고는 기존 DLL interface 계열 `C4275`로 확인된다.

- Client: `CNavigationSystem`, `CVisionSystem`, `CMinionAISystem`, `CSpatialHashSystem`, `CBehaviorTreeSystem` 등에서 `ISystem` DLL interface 경고
- Server: `CSpatialHashSystem`, `CTurretAISystem`에서 `ISystem` DLL interface 경고

이번 문서 작업은 C++ 코드를 수정하지 않았으므로 새 빌드 오류는 없다.

## 다음 구현 검증 게이트

1. `BushEntry`를 StageData v5에 추가한다.
2. v4 Stage 파일 로드 시 bushes가 빈 배열로 처리되는지 확인한다.
3. Editor에서 Bush 3개를 배치하고 저장한다.
4. Editor 재진입 후 위치/radius/assetPath가 보존되는지 확인한다.
5. InGame 진입 시 StageData bush count가 로그로 출력되는지 확인한다.
6. visual asset 실패와 무관하게 `BushVolumeComponent`가 생성되는지 확인한다.
7. 캐릭터가 부쉬 안팎으로 이동할 때 `VisibilityComponent::bInBush`, `bushId`가 바뀌는지 확인한다.
8. Server가 같은 StageData bushes를 읽는 단계까지 간 뒤, Client-only 부쉬 판정이 남지 않았는지 확인한다.

## 핸드오프

다음 장비에서는 계획 문서부터 읽으면 된다.

1. `.md/plan/2026-06-24_MAP_BUSH_PLACEMENT_MAPDATA_PLAN.md`
2. 이 보고서
3. `Shared/GameSim/Definitions/MapDataFormats.h`
4. `Client/Private/Map/MapDataIO.cpp`
5. `Client/Private/Scene/Scene_Editor.cpp`
6. `Client/Private/Scene/Scene_InGameLifecycle.cpp`

이 작업의 첫 구현 단위는 visual이 아니라 `StageData v5 + BushEntry 저장/로드`다. 그 다음에 Editor 배치와 InGame visual을 붙이면 된다.
