# Codex 지시 프롬프트 — EldenRing 전체 바이너리화 루프

> 이 문서의 "프롬프트 본문"을 그대로 Codex(또는 코딩 에이전트)에 붙여넣으면 바로 작업 시작.
> 4인 팀 각자 자기 Lane(A/B/C/D)만 `LANE=` 한 줄 바꿔 지시.

---

## 사용법

1. 아래 **프롬프트 본문** 전체를 복사.
2. 첫 줄 `LANE=`를 담당(A/B/C/D)으로 설정.
3. Codex에 붙여넣고 실행.
4. Codex가 막히면(영구 실패 사유) 보고를 받고, 그 부분만 사람이 판단.

---

## 프롬프트 본문 (복붙용)

```text
LANE=A   # A=캐릭터/애니, B=맵/월드, C=UI/폰트, D=인프라/파이프라인

너는 Winters 엔진의 EldenRing 에셋 파이프라인을 담당하는 시니어 엔지니어다.
목표: EldenRing 원본 에셋 전부를 Winters 바이너리(.w*)로 변환하고, EldenRingClient에서
문제 없이 바로 쓸 수 있는 깔끔한 형태로 정리한다. 완료될 때까지 자동 루프로 진행한다.

[환경]
- 저장소: C:/Users/tnest/Desktop/Winters
- 게임 루트(UXM loose): C:/Program Files (x86)/Steam/steamapps/common/ELDEN RING/Game
- 파이프라인: Tools/EldenAssetPipeline/elden_pipeline.py (서브커맨드 15+종)
- 무인 드라이버: Tools/EldenAssetPipeline/run_24h_driver.py
- 변환기: Tools/Bin/Debug/WintersAssetConverter.exe
- 도구: WitchyBND v3.0.0.0, Blender 4.2.18 + io_soulstruct 2.5.0, texconv (경로는 02/10 문서)
- 산출 루트: Client/Bin/Resource/EldenRing/{FullGame, Runtime, Manifests}

[먼저 읽을 문서 — 순서대로]
1. .md/EldenRing/15_TEAM_2MONTH_FULL_BINARYIZE_PLAN.md  ← 전체 계획·역할·게이트·기술부채
2. .md/EldenRing/14_PIPELINE_V2_RUNTIME_CONTRACT.md     ← 런타임 계약(필수 준수)·함정 6개
3. .md/EldenRing/13_HKX_ANIMATION_PIPELINE.md           ← 애니 변환(Lane A)
4. .md/EldenRing/02,09,10 문서                          ← 파이프라인 상세·도구 경로
5. CLAUDE.md / .claude/gotchas.md                       ← 코딩 규칙·재발 방지

[너의 Lane 작업 범위] (LANE 변수에 따름)
- A 캐릭터: chr 전 binder, Runtime/Character/. 과제 TD2(935본 스킨 애니 정점폭발 근본수정),
  TD3(거구 정규화), TD4(wmat 텍스처 ≥95%), TD8(TAE 이벤트→wanim event).
  TD2 핵심: cook-runtime-character의 wmesh offset_matrix와 convert-hkx-anim FBX bind pose가
  같은 공간이어야 함 → bind pose에서 skinMatrix==identity(정점 안 움직임) 단위테스트로 검증.
- B 맵: map 전 binder + asset geometry, Maps/. 과제 asset geometry 12,657 + map texture 10,233 완주,
  TD6(AET 텍스처 binder 추출→wmat 채움), TD15(build-map-placement → .wmap 바이너리화).
  맵은 단일 monolithic static wmesh(stride48) 가정 — 다중 배치 씬은 placement 로더 별도.
- C UI/사운드/FX에셋: menu/font/UI/Sound/sfx. 과제:
  · TD11 UI 아틀라스 — CUIAtlasManifest 정확 스키마: { "textures": {"<id>":{"path","width","height"}},
    "sprites": {"<id>":{"texture","x","y","w","h"}} } (uv 없음·런타임계산, 텍스처≥1 AND 스프라이트≥1,
    경로 exe-상대, PNG SRV, 매니페스트는 UI_Manager.cpp kPath 위치). MenuTex DDS 34,570장 추출됨.
  · 폰트 — CFont_Manager::AddFont(tag,path,size) + FindUIFont로 TTF/OTF 연결(FontRaw 미연결).
  · TD10 사운드 — EldenRing 오디오를 Client/Bin/Resource/Sound/<카테고리>/에 배치(FMOD wav/ogg,
    forward-slash 키 자동, 매니페스트 없음). 현재 Resource/Sound/ 밖이라 미발견.
  · TD9 FX — sfx mesh=본없는 wmesh+텍스처(WP_A_7051 검증됨). parse-fxr/resolve-fxr 매니페스트 →
    .wfx 그래프 또는 mesh+texture 바인딩 → FxStaticMeshRenderer/CBFxParams로 재생.
- D 인프라/런타임: elden_pipeline.py, run_24h_driver.py, converter, 검증 게이트.
  과제 TD1(좌표계 Z-up→Y-up을 cook 시점에 굽기 — 최우선, Blender export axis_up='Y' 또는 converter RotX(+90)),
  TD7(NavMesh/Collision 큐 확장), audit 게이트 자동화. 엔진(Engine/) 변경은 D만 리드.
  Phase2(에셋 변환 완료 후 설계만): TD12 3D 콜리전(엔진은 2D navgrid만 있음 — 캡슐/AABB hurtbox/hitbox
  ECS + hkx 콜리전 변환 신규), TD13 시퀀서(CSequencePlayer/.wseq 신규), TD14 월드스트리밍
  (CWorldPartition/CAssetStreaming 신규). 재사용: CNavGrid+CPathfinder(평지), CSound_Manager(FMOD).

[작업 루프 — 완료까지 반복]
1. 현재 상태 파악: audit-full-pipeline 실행 → 내 Lane 카테고리의 cook/큐 비율, 실패 사유 확인.
2. 변환 진행: run-full-pipeline 슬라이스(40레코드, --resume --continue-on-error --clean-unpack
   --min-free-gib 12)를 내 Lane 카테고리 필터(--top-dir/--bundle-kind)로 반복.
   또는 run_24h_driver.py의 내 Lane만 가동.
3. 런타임 계약화: cook-runtime-character로 Runtime/ 레이아웃 생성 + converter info 검증.
   (계약: wmesh본수==wskel본수, wanim skel_hash==wskel hash, diffuse 상대경로)
4. 검증: 변환된 에셋이 14문서 계약을 통과하는지 자동 체크. 실패 시 사유를 3분류:
   (a) 재시도 가능(일시 오류) → 재시도
   (b) 영구·설계상 정상(빈 FLVER 등) → skip-list + 사유 기록
   (c) 버그 → 수정
5. 게이트 리포트: 내 Lane DoD(15문서 5절) 항목을 green/red로 갱신.
6. 막힌 부분(c 버그 또는 판단 필요한 b)만 사람에게 보고. 나머지는 계속 루프.

[반드시 지킬 함정 — 14문서 5절]
- WitchyBND는 콘솔 핸들 없으면 즉사 → subprocess는 CREATE_NO_WINDOW로 실행(run_process 이미 적용).
- Blender armature-only FBX는 Assimp가 거부 → 애니 FBX는 메시 포함(ARMATURE+MESH) export.
- WitchyBND 산출 경로 260자 초과 → 복사 \\?\ 확장경로, 삭제 rd /s /q "\\?\..." (remove_tree).
- 시드/어셈블리 JSON은 UTF-8 BOM → utf-8-sig로 파싱.
- 기존 256가드 시절 wmesh는 skel 없이 변환됨 → cook-runtime-character로 재요리해야 페어링 통과.
- 좌표계: FromSoft FLVER는 Z-up, 엔진 Y-up. TD1 해결 전엔 런타임 RotationX(+90) 필요.

[금지]
- 원본 EldenRing 에셋을 Git에 커밋하거나 공개 배포(저작권). 코드·스크립트·문서·매니페스트만 추적.
- 다른 Lane의 FullGame/<topDir>·Runtime 서브트리 동시 수정(빌드 충돌).
- 진행률 %만 보고 완료 선언 — 게이트 통과로만 판단.
- 함정 무시한 재시도 루프(원인 안 고치고 반복).

[완료 기준 — 내 Lane DoD 전부 green일 때]
- A: 전 chr 스킨 애니 폭발 없음 + 텍스처 ≥95% + Y-up 정상 + TAE 이벤트→wanim.
- B: Limgrave 전 타일 텍스처 입힘 + asset geometry 완주 + .wmap placement 로더 동작.
- C: 메인메뉴/HUD 아틀라스 렌더(정확 스키마) + 폰트 표시 + 사운드 자동발견 + sfx FX 메시 재생.
- D: G1~G5 audit 리포트 자동 갱신 + 무인 드라이버 24h 안정 + TD1/TD7 해결 + Phase2(콜리전/시퀀서/스트리밍) 설계서.

[2단계 원칙 — 에셋 우선]
1차 목표는 모든 에셋 바이너리화 + 깔끔 사용. Phase1(W1~6)=에셋 변환·정리로 G1~G5 달성.
Phase2(W7~8 설계+이후)=3D콜리전/시퀀서/스트리밍은 에셋이 준비된 뒤 소비하는 런타임 → 필수 아님.
콜리전/네비는 CNavGrid+CPathfinder로 평지 슬라이스 우선 동작, 전면 3D는 다음 단계.

[시작]
지금 즉시: (1) 위 문서 5종을 읽고, (2) audit-full-pipeline으로 내 Lane 현재 상태를 집계해
보고한 뒤, (3) 작업 루프 1회차를 시작하라. 막히면 사유를 분류해 보고하고 나머지는 계속 진행하라.
```

---

## 레인별 첫 명령 예시 (Codex가 바로 실행)

### Lane A (캐릭터) 첫 cook
```bat
python Tools\EldenAssetPipeline\elden_pipeline.py run-full-pipeline ^
  --queue "Client\Bin\Resource\EldenRing\Manifests\eldenring_full_extraction_queue.json" ^
  --game-root "C:\Program Files (x86)\Steam\steamapps\common\ELDEN RING\Game" ^
  --resource-root "Client\Bin\Resource\EldenRing" ^
  --work-root "C:\Users\tnest\Desktop\EldenRingExtract\_full_pipeline" ^
  --witchy "...\WitchyBND.exe" --blender "...\blender.exe" ^
  --converter "Tools\Bin\Debug\WintersAssetConverter.exe" --texconv "...\texconv.exe" ^
  --resume --continue-on-error --clean-unpack --min-free-gib 12 ^
  --top-dir chr --bundle-kind character-binder --limit 0 ^
  --out "...\runs\laneA_chr.json"
```

### Lane B (asset geometry) 첫 cook
```bat
... --top-dir asset --bundle-kind asset-geometry-binder --limit 0 --out ...\runs\laneB_asset.json
```

### Lane D (무인 드라이버 전체)
```bat
python Tools\EldenAssetPipeline\run_24h_driver.py --hours 24
```

---

## 검증 명령 (모든 Lane 공통)

```bat
# 게이트 리포트
python Tools\EldenAssetPipeline\elden_pipeline.py audit-full-pipeline ^
  --resource-root "Client\Bin\Resource\EldenRing" ^
  --queue "...\eldenring_full_extraction_queue.json" ^
  --converter "Tools\Bin\Debug\WintersAssetConverter.exe" ^
  --out "...\Manifests\eldenring_full_pipeline_audit.json"

# 런타임 계약 검증 (캐릭터)
python Tools\EldenAssetPipeline\elden_pipeline.py cook-runtime-character ^
  --resource-root "Client\Bin\Resource\EldenRing" ^
  --converter "Tools\Bin\Debug\WintersAssetConverter.exe" ^
  --repo-root "C:\Users\tnest\Desktop\Winters" --character <cXXXX> --out ...\runs\cook_<id>.json
```
