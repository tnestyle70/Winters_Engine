---
name: debug-pipeline
description: >
  WintersEngine 의 렌더/이펙트/스킬 버그 디버깅 사이클.
  "안 보임", "그려지지 않음", "스폰 됐는데 화면 비어있음", "한 케이스만 깨짐",
  "호출은 되는데 픽셀이 안 나옴" 같은 증상에 트리거.
  CPU vs GPU 경계 분기 + 셰이더 우선 Read + 데이터 직접 계측을 강제.
---

# Skill: Debug Pipeline — WintersEngine

## 핵심 원칙

**증상 → 실행 경로 분해 → 실패 지점 이분 탐색 → 데이터 직접 계측 → 최소 수정**

**Why**: 코드 흐름 추론만 누적하면 픽셀 단위에서 죽는 GPU 클래스 버그를 못 잡는다. 셰이더 코드 + 텍스처 데이터 + UV 좌표를 CPU 코드와 동급으로 다뤄야 함.

## 트리거 키워드 → 1차 분기

| 사용자 표현 | 1차 의심 | 즉시 할 일 |
|-----------|---------|-----------|
| "안 보임" / "그려지지 않음" / "스폰 안 됨" | **GPU 픽셀 단계** | 셰이더 Read |
| "Render 호출은 됐는데..." | **GPU 픽셀 단계** | 셰이더 Read + 데이터 계측 |
| "한 케이스만 깨짐 / 옆은 정상" | **데이터 차이** | 비교 대조군 표 |
| "카메라 움직이면 사라짐" | RS/DepthState | 파이프라인 상태 |
| "첫 프레임만 / 처음에만" | cbuffer 초기화 | Create/Init BP |
| "Output 창에 메시지 0개" | DIAG 로그 미연결 | 셰이더 Read 우선 |

"호출은 되는데 안 보임" 패턴은 **정의상 GPU 픽셀 단계**. CPU 가설부터 세우지 말 것.

## 실행 순서

### 1. 셰이더 우선 Read (의무)

GPU 의심 시 즉시:
- `Shaders/Mesh3D.hlsl` (정적 메쉬)
- `Shaders/Skinned3D.hlsl` (스키닝)
- `Shaders/Plane.hlsl` (있으면)
- `Shaders/Default3D.hlsl`

검색 키워드: `clip(`, `discard`, `texture.Sample`, `return float4`, `SV_TARGET`, `alpha`

`clip(texColor.a - 0.05f)` 같은 alpha test 라인 = **PNG 알파가 그 임계 미만이면 픽셀 전체 버려짐**. CPU 단계 정상 + 화면 0 = 이 패턴.

### 2. 데이터 직접 계측

#### 2.1 PNG 알파 분포 (Bash 한 줄)
```bash
python -c "from PIL import Image; im=Image.open('PATH').convert('RGBA'); a=im.split()[3]; print('size=',im.size,'alpha_bbox=',a.getbbox(),'alpha_max=',max(a.getdata()))"
```
- `alpha_bbox=None` → 전체 투명. 텍스처 자체 문제
- `alpha_bbox` 가 이미지 일부 → mesh UV 가 그 외 영역 가리키면 clip

#### 2.2 .wmesh / FBX UV bbox
- `Tools/WintersAssetConverter` 로 FBX → .wmesh 변환 후 헤더 + 정점 dump
- 또는 Assimp 직접 호출 Python 스크립트로 정점 첫 N 개 출력
- UV bbox 와 PNG alpha bbox 가 겹치는지 비교

#### 2.3 FBX 바이너리 키워드 (HasSkeleton 검사)
```powershell
Select-String -Path "PATH.fbx" -Pattern "Skeleton|Deformer|Cluster|Geometry|AnimStack" -Encoding byte
```
- `Skeleton/Deformer/Cluster` 발견 = 스키닝 FBX. `CFxStaticMeshRenderer` HasSkeleton reject 가능

#### 2.4 DIAG 로그 (Model.cpp 가 자동 출력)
VS Output 창 검색:
```
[CModel] === <path> ===
[CModel] Loaded: meshes=N materials=M animations=K bones=B
[CModel DIAG] aiMaterials=N
[CModel DIAG] aiMeshes=N verts=...
[CModel DIAG] rootChildren=N
[CFxStaticMeshRenderer] Model load fail / Skinned FBX rejected / Texture load fail
```

`meshes=0` 또는 `verts=0` → FBX 파싱 자체 실패.

### 3. 비교 대조군 표

"한 케이스만 깨짐" 이면 표 채우기 전에 코드 수정 금지:

| 차원 | 정상 | 깨짐 |
|------|------|------|
| 호출 경로 | | |
| 파일 (FBX/PNG) | | |
| 스케일/회전/위치 | | |
| 정점 수 / UV bbox | | |
| 텍스처 alpha bbox | | |
| 셰이더/파이프라인/블렌드 | | |

차원 1개로 좁혀지면 그게 원인.

### 4. 가설 → 즉시 falsify

가설 1개 세우면 데이터로 검증 후 다음. 추론으로 가설만 누적하지 말 것:
- "노드 transform 미적용" → DIAG `verts=` 와 .wmesh UV bbox 측정
- "스케일 너무 작음" → 슬라이더로 100배 시도
- "본 있는 FBX 라 reject" → Skeleton 키워드 grep

falsify 안 되면 다음 가설로 넘어가지 말고 도구 바꿔 측정.

### 5. 최소 수정 (큰 패치 금지)

데이터 계측 끝나기 전:
- ❌ 신규 로더 분기 / aiProcess 옵션 / 셰이더 신규 / ResourceCache 우회
- ✅ 텍스처 경로 1개 교체, 멤버 1개 추가, 슬라이더 클램프 확장

원인 확정 후 인프라 변경 검토.

### 6. 사용자 위임 전 자체 시도

다음은 read-only 권한 안에서도 가능:
- Bash → Python 한 줄 (PIL, struct, hashlib)
- PowerShell `Select-String -Encoding byte`
- AssetConverter / 프로젝트 내장 도구
- 셰이더 Read
- DIAG 로그 grep

"BP 찍어주세요" / "슬라이더 시도" 권장 전에 위 도구로 자체 진단 1회 시도.

## 사이클 종료 후 (★ 의무)

원인 확정되면 메모리 업데이트:
1. CLAUDE.md Gotchas 한 줄 추가 (재발 방지 도메인 사실)
2. `memory/feedback_*.md` 신규 작성 (구체 사례)
3. **놓친 사고 흐름** 이 있으면 본 SKILL.md 또는 `memory/feedback_debugging_pipeline.md` 보강 (사이클 자체 개선)
4. `MEMORY.md` 인덱스 1줄

## 참고 사례

### 2026-04-26 이렐리아 E sword 안 보임
- **증상**: SpawnPlaced/PreloadMesh/Render 모두 호출됨. 화면 0 픽셀
- **CPU 가설들 (전부 틀림)**: ProcessNode 노드 transform 미적용, vScale 0.01 작음, vRotation 0 누움, FBX 단위 cm
- **정답**: `Mesh3D.hlsl` 의 `clip(texColor.a - 0.05f)` + `render/irelia_base_e_blade.png` 의 알파 분포가 mesh UV 영역 밖. 즉 sprite 캡처 PNG 와 mesh UV 도메인 미스매치
- **잡은 방법**: .wmesh UV bbox (`u=0.075~0.875, v=0.806~0.977`) + PNG 알파 bbox 직접 측정. 두 영역 비교
- **수정**: 텍스처 1개 교체 (`render/irelia_base_e_blade.png` → `irelia_base_blades_passive_4_texture.png`)
- **소요**: CPU 추론 1.5시간 → 데이터 계측 30분
- **상세**: `memory/feedback_lol_fx_texture_pattern.md`, `memory/feedback_debugging_pipeline.md`

### 핵심 교훈
- 같은 도구 권한이었는데 도구 사용 의지 + 도메인 분기 + 데이터 계측 셋이 차이를 만들었다
- "셰이더를 안 읽는 습관" 이 픽셀 단계 버그의 가장 큰 적
- "사용자에게 BP 찍어달라" 는 자체 시도 다 한 후의 마지막 수단

## 슬래시 명령어

`/debug-pipeline` 으로 명시 호출 가능 — `.claude/commands/debug-pipeline.md` 가 본 SKILL 의 단계별 절차를 즉시 컨텍스트에 로드.
