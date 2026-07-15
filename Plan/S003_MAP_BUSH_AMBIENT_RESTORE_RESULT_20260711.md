Session - Map11 부쉬·환경 오브젝트 복구 반영 결과와 비실행/화면 검증 경계를 고정한다.

RETIRED - crossed-card WMesh는 PNG 카드가 여러 장 겹친 평면성 때문에 부쉬의 체적·실루엣·존재감을 만들지 못했다. ABI/빌드 성공과 시각 성공은 별개이며, S004에서 Stage B0로 normal F5 경로를 폐기한다. 곤충 앰비언트는 폐기 대상이 아니다.

## 1. 결론

- 원본 리소스에는 독립된 완성형 부쉬 3D 메시가 없었다.
- `sru_brush.png`는 256x256 불투명 텍스처 아틀라스이며, 한 장을 그대로 카메라 빌보드로 띄우면 사각 오버레이처럼 보일 가능성이 높다.
- 대신 남아 있던 35 vertex windgrass 지오메트리를 0/60/120도 세 방향으로 교차시킨 `map11_bush_cluster.wmesh`를 생성했다.
- 이 메시를 기존 `ModelRenderer`와 `Mesh3D.hlsl` 경로에 연결하여 월드 깊이 검사, 프러스텀 컬링, 양면 지오메트리와 기존 텍스처 UV를 사용한다.
- 따라서 현재 선택은 `순수 카메라 빌보드`가 아니라 `월드 공간 crossed-card foliage mesh`이다. 순수 빌보드는 소스 지오메트리마저 없을 때의 fallback으로만 남긴다.

## 2. 실제 반영

### 2-1. 부쉬 데이터와 렌더

- `Tools/build_map11_bush_cluster.py`
  - 기존 GLB의 position/UV/index를 읽어 세 방향 교차 카드와 reverse face를 생성한다.
  - OBJ/MTL을 만든 뒤 `WintersAssetConverter`로 WMesh/WMat을 생성한다.
- `Tools/cook_map11_brush_volumes.py`
  - centered local CSV의 X에 `+104.50`을 적용해 canonical Stage 좌표로 변환한다.
  - `WBRUSH v1`과 `Stage1.dat v5`의 bush block을 동일 입력으로 재생성한다.
  - 재실행해도 bush block이 중복 추가되지 않는다.
- `CBush_Manager`
  - `RenderKind::Mesh`를 `ModelRenderer`로 초기화한다.
  - legacy render와 RHI snapshot 양쪽에 동일 메시를 제출한다.
  - kind/path/visible/scale 변경을 renderer와 동기화한다.
  - mesh bush에는 overlay용 `FxBillboardComponent`를 만들지 않는다.
- `CConcealmentVolumeIndex`
  - ECS entity 번호보다 비영(非零) `volumeId`를 우선 반환한다.
  - 동일 `bushId`의 두 원형 볼륨을 하나의 부쉬 영역으로 판정할 수 있다.

### 2-2. 새·오리·벌레 계열 환경 오브젝트

- `Tools/cook_map11_ambient_props.py`
  - `base.materials.bin`의 bird 5, duck 1 배치를 유지한다.
  - `Audio-Emitter_SRU_Insects*` 8개 transform을 추출해 firefly visual proxy로 연결한다.
  - firefly mesh/skeleton/material/idle animation을 런타임의 중첩 asset layout으로 복사한다.
- `ObjectVisualDefs.json`과 생성된 visual definition pack
  - `ambient.chemtech_firefly`를 등록한다.
- `CAmbientProp_Manager`
  - LoL 좌표를 Stage 좌표로 아래처럼 변환한다.

```text
stageX = (lolX + lolZ) * 0.01 / sqrt(2)
stageZ = (lolX - lolZ) * 0.01 / sqrt(2)
```

  - 지면 투영 뒤 firefly만 1.4m 올리고 녹색 material override와 idle animation을 적용한다.
  - legacy render와 RHI snapshot 양쪽에 제출한다.

## 3. 실행 없이 확정한 검증 결과

### 3-1. 데이터 ABI와 개수

```text
Stage1.dat
  magic       = 0x47545357 (WSTG)
  version     = 5
  structures  = 30
  jungle      = 12
  waypoints   = 27
  bushes      = 64
  parsed size = 21,540 / 21,540 bytes

Bush block
  bushId          = 1..32
  multiplicity    = 각 ID당 2
  renderKind      = Mesh 64
  X range         = 10.500..198.500
  Z range         = -95.000..95.000
  unique meshPath = 1

WAMB
  version   = 1
  count     = 14
  kind 0    = bird 5
  kind 1    = duck 1
  kind 2    = firefly 8
```

### 3-2. 생성 에셋

```text
map11_bush_cluster.wmesh
  submeshes = 1
  bones     = 0
  vertices  = 210
  indices   = 864
  stride    = 48

map11_bush_cluster.wmat
  materials = 2
```

### 3-3. 재현성과 회귀 방지

```text
Stage1.dat SHA-256
14AF613D8D13C6ADB0409C00C913C4902A75F1D9F8E8F5CF9EB27208C13F11FE

Stage1.navgrid SHA-256
B2E0DCA448ABFA9425B076BEF65D0B2047BC84BCB85494385784A03ACE9127A9
```

- bush cook를 다시 실행해도 Stage hash가 유지됐다.
- bush migration 동안 `Stage1.navgrid`는 변경되지 않았다.

## 4. 통과한 검증

- `python Tools/build_map11_bush_cluster.py`: PASS
- `python Tools/cook_map11_brush_volumes.py --migrate-stage`: PASS
- brush cook idempotence/hash: PASS
- `python Tools/cook_map11_ambient_props.py`: PASS
- Python syntax 및 JSON parse: PASS
- `python Tools/LoLData/Build-LoLDefinitionPack.py --check`: PASS
  - pack `0x94A01989`, champions 17, skills 85
- `Tools/Harness/Check-SharedBoundary.ps1`: PASS
- `git diff --check`: PASS, 기존 line-ending 안내만 존재
- Client Debug x64: PASS, `Client/Bin/Debug/WintersGame.exe`
- Server Debug x64: PASS, `Server/Bin/Debug/WintersServer.exe`
- 자동 클라이언트 smoke: 18초 동안 프로세스 alive/responding, 강제 종료 전 비정상 종료 없음

## 5. 화면 실행이 필요한 마지막 확인

다음 항목은 데이터나 빌드만으로 최종 픽셀을 증명할 수 없다.

- 불투명 atlas와 교차 지오메트리 조합이 실제 카메라 거리에서 덩어리처럼 자연스럽게 보이는지
- 부쉬 scale과 밀도가 맵 지형 경계에 맞는지
- 반딧불의 1.4m 높이, 녹색 tint, idle animation이 과하거나 약하지 않은지
- 새/오리의 보정 좌표가 지형 위에 자연스럽게 놓이는지

직접 게임을 플레이할 필요는 없다. 정상 맵에 한 번 진입해 중앙·상단·하단 부쉬와 강가 firefly를 보는 약 30초 확인이면 충분하다. 이 확인 전에는 미술 수치를 더 튜닝하지 않는다.

검증 명령은 다음과 같다.

```powershell
Set-Location 'C:\Users\user\Desktop\Winters'
.\Client\Bin\Debug\WintersGame.exe --banpick-smoke --smoke-slot=0 --smoke-champion=YONE --smoke-start --smoke-start-min-humans=1 --smoke-full-map --smoke-no-skill
```

## 6. 완료로 표현하면 안 되는 범위

- 현재 brush CSV는 기존 64개 authored approximation을 canonical Stage 좌표로 복구한 것이다. Riot 원본 brush polygon의 정밀 재추출 결과가 아니다.
- 서버는 아직 `StageData::bushes`를 권위 데이터로 소비하거나 세션별 snapshot visibility를 필터링하지 않는다.
- 이번 S003은 `클라이언트 표시 + 클라이언트 concealment identity` 복구까지다. LAN에서 적을 부쉬 밖 클라이언트에 숨기는 서버 권위 기능은 별도 세션으로 구현·검증해야 한다.
- 수동 화면 확인에서 카드의 사각감이 보이면 다음 수정은 무작정 billboard로 되돌리는 것이 아니라, bush 전용 alpha mask/soft edge/wind shader를 현재 crossed-card 경로에 추가하는 것이다.
