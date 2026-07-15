# 05. 리소스 · 에셋 파이프라인 (Resource & Asset Pipeline)

> 면접 대본. 코드 문법이 아니라 "왜 이렇게 설계했고, 뭘 포기했는가"를 말하는 챕터.
> 모든 경로는 repo-relative. 인용은 실제 코드에서 검증한 것만 남겼다.

---

## ① 한 줄 정의

"Winters의 리소스 파이프라인은 **'런타임은 검증된 불변 바이너리만 읽는다'**는 원칙 위에 세웠습니다.
FBX는 별도 CLI 툴(WintersAssetConverter)이 오프라인에서 `.wmesh/.wskel/.wanim/.wmat` 자체 포맷으로 쿠킹(cooking)하고,
런타임 로더는 고정 레이아웃 POD를 zero-copy로 읽되 파일을 신뢰 경계(trust boundary)로 취급해 상한/정합성 검증을 전부 통과한 데이터만 GPU에 올립니다.
그 위에 경로 정규화 캐시와 flyweight 공유로 수명·중복을 관리합니다."

---

## ② 구조와 데이터 흐름

```text
[오프라인 — Tools 경계 (Assimp는 여기만 링크)]
  FBX / GLB
    │  WintersAssetConverter.exe  (Engine/Private/Tools/AssetConverter/main.cpp)
    │  서브커맨드: skel / mesh / material / anim / info
    │  Assimp postprocess: Triangulate | ConvertToLeftHanded | GenNormals
    │                      | CalcTangentSpace | LimitBoneWeights | JoinIdenticalVertices
    ▼
  .wskel ──(본 인덱스 맵 --skel)──▶ .wmesh      .wmat      anims/*.wanim (+ .wanim.stamp)
    │        2-pass: skel 먼저, mesh가 그 인덱스에 정합
    │  배치: Tools/convert_all_assets.ps1 (source+converter mtime 기반 incremental)
    ▼
[런타임 — Engine.dll (Assimp 의존 없음)]
  CWMeshLoader / CWSkelLoader / CWAnimLoader / CWMaterialLoader
    │  16B WintersFileHeader('WINT') → payload magic(WMSH/WSKL/WANM/WMAT)
    │  상한/정합 검증 → 헤더 복사, 정점/인덱스 blob은 zero-copy 포인터
    ▼
  CModel  ── 정적: BuildCombinedStaticMesh (단일 VB/IB + range)
          ── 스키닝: submesh별 CMesh + CSkeleton(BuildSkeletonFromStage3)
    ▼
  CResourceCache (경로 정규화 dedup, 텍스처=unique_ptr 소유/raw 반환, 모델=shared_ptr 공유)
    ▼
  ModelRenderer 인스턴스들 (모델·스켈레톤 공유 / 애니 시간·상수버퍼·본 SRV만 인스턴스별)

[별도 데이터 축]
  EntityBlueprintRegistry (씬 스코프 프로토타입 → Clone_Entity)
  Stage .dat (CMapDataIO — 'WSTG' magic, version 5, min-compat 3)
```

핵심 파일:
- 포맷 정의: `Engine/Public/AssetFormat/Common/WintersFileHeader.h`, `Engine/Public/AssetFormat/Mesh/WMeshFormat.h`
- 로더: `Engine/Private/AssetFormat/Mesh/WMeshLoader.cpp`, `Engine/Private/AssetFormat/Anim/WAnimLoader.cpp`
- 쿠커: `Engine/Private/Tools/AssetConverter/main.cpp`, `Tools/convert_all_assets.ps1`
- 소비: `Engine/Private/Resource/Model.cpp`, `Engine/Private/Resource/ResourceCache.cpp`, `Engine/Private/Renderer/ModelRenderer.cpp`

---

## ③ 핵심 설계 결정과 트레이드오프

### 결정 1 — 런타임 FBX 파싱 대신 오프라인 쿠킹

- **왜**: FBX/GLB는 파싱이 무겁고(트라이앵글화, 탄젠트 생성, 좌표계 변환) 포맷이 런타임에 신뢰하기 어렵다. 매 실행마다 같은 변환을 반복하는 건 낭비다.
- **대안**: (a) 런타임에 Assimp로 직접 로드, (b) 캐시 파일을 런타임이 자동 생성하는 lazy cooking, (c) 오프라인 CLI 쿠킹.
- **선택 이유**: (c). 좌수 좌표계 변환·본 가중치 4개 제한·탄젠트 생성 같은 정규화를 전부 오프라인으로 밀었다 (`main.cpp` ReadScene의 postprocess 플래그, 66-72행). 그 결과 **Engine.dll은 Assimp 의존이 아예 없다** — `WMeshWriter.h` 6행 주석: "Assimp 전방선언 (Engine.dll 은 Assimp 의존 없음 — Tools 프로젝트에서만 링크)". 임포터는 툴 경계로 격리하고 로더/포맷 정의만 Engine에 뒀다.
- **감수한 비용**: 에셋을 고칠 때마다 재쿠킹 단계가 생긴다. 이 비용은 `Tools/convert_all_assets.ps1`의 incremental 쿠킹으로 줄였다 — `Test-OutputsCurrent`가 소스 FBX의 mtime뿐 아니라 **컨버터 exe 자체의 mtime**도 출력물과 비교해서(40-45행), 컨버터 코드가 바뀌면 소스가 그대로여도 재쿠킹을 강제한다. 애니메이션은 산출물 개수가 가변이라 `anims/.wanim.stamp` 파일로 완료를 표시한다.

실측 근거: 세션 기록(2026-04-24) 기준, Profiler(스코프/카운터 계측)로 병목(스키닝 갱신)을 특정한 뒤 같은 세션에서 정적 엔티티 bAnimated 스킵과 함께 27개 모델 `.wmesh` 전수 변환 + 런타임 쿠킹본 통합을 적용해 프레임 타임을 **17.8ms → 9ms**로 회복했다 — 이 챕터의 기여분은 로드 경로 쪽이다. 면접에서는 "숫자를 계측 인프라와 함께 만들었다"고 말한다.

### 결정 2 — `#pragma pack(1)` POD + `static_assert`로 잠근 wire 레이아웃, zero-copy 파싱

- **왜**: 직렬화가 "구조체를 그대로 쓰고 읽는" 수준으로 단순해야 로더가 빠르고 검증 가능하다.
- **대안**: protobuf/FlatBuffers 같은 직렬화 라이브러리, 또는 필드 단위 수동 직렬화.
- **선택 이유**: 에셋은 스키마 진화 빈도가 낮고 로드 속도가 중요하다. 모든 헤더/엔트리를 `#pragma pack(push,1)`로 패킹하고 크기를 컴파일 타임에 못박았다 — `WMeshFormat.h`: `MeshMetaHeader==36`, `SubMeshDesc==48`, `BoneEntry==128`, `VertexSkinned==76`, `VertexStatic==48`이 전부 `static_assert`다. 덕분에 로더는 파일을 통째로 메모리에 올린 뒤 헤더는 `Read<T>()`로 복사하고, 대용량 정점/인덱스 blob은 `Peek()+Skip()`으로 **포인터만 잡는다**(`WMeshLoader.cpp` 65-77행). blob 수명은 `WMeshLoaded::m_vRawFile` 벡터가 소유하고 `pVertexBlob/pIndexBlob`는 그 안을 가리킨다(`WMeshLoader.h` 18-24행 주석). 정점 stride도 `STRIDE_STATIC(48)/STRIDE_SKINNED(76)` 두 값만 허용해 InputLayout과 정합을 강제했다.
- **감수한 비용**: 포맷 변경이 곧 바이너리 호환성 문제다. 이를 위해 16바이트 공통 `WintersFileHeader`에 `version_major/minor`, `flags`(LZ4/SHA256 예약), `content_size`를 뒀고, 지금은 `version_major==1`, `flags==WF_NONE`만 허용한다(`WintersFileHeader.h`). 확장점은 예약하되 미구현 조합은 로드 거부 — "모르는 파일은 읽지 않는다"가 규칙이다.

### 결정 3 — 파일을 신뢰하지 않는 로더: 상한선 + 정합성 교차 검증

- **왜**: 쿠킹된 파일이라도 손상·구버전·악의적 변조 가능성이 있다. malformed 파일이 크래시가 아니라 "로드 실패"로 떨어져야 한다.
- **선택 이유**: `WMeshLoader.cpp`는 다층 검증이다: magic 확인 + `MAX_VERTICES(1000만)/MAX_SUBMESHES(2048)/MAX_BONES(1024)` 상한, index_stride는 2/4만, `VF_BoneWeight` 플래그와 stride의 교차 검증(27-29행), blob마다 `r.Remaining()` 비교로 오버런 차단, bone `parent_index` 범위 검증(86-88행), 전체를 try/catch로 감싸 `CBinaryReader`의 bounds 예외를 false로 흡수. `WAnimLoader`도 동일 패턴 — `MAX_ANIM_CHANNELS/KEYS` 상한과 `SpanFits<T>`로 키 offset+count가 keyBlock 안에 들어오는지 확인한다(34-39, 118-120행).
- **감수한 비용**: 로더 코드가 길어지고 "그냥 읽으면 되는" 케이스에도 검증 비용이 붙는다. 헤더 단계 검증이라 실측 비용은 무시 가능하고, 얻는 건 "파싱 크래시가 원천적으로 없는 런타임"이다.

### 결정 4 — 본 데이터의 파일 분리와 해시 기반 호환성 가드

- **왜**: 스켈레톤 계층은 여러 메시/애니가 공유할 수 있는 데이터고, inverse bind(offset matrix)는 메시 바인드포즈에 종속된 데이터다. 그리고 "엉뚱한 스켈레톤용 애니가 붙는 사고"는 조용히 이상한 포즈로 나타나서 디버깅이 지옥이다.
- **선택 이유**:
  - `.wmesh`의 `BoneEntry`는 offset_matrix를, `.wskel`의 `BoneNode`는 hierarchy(parent_index, child_count)+rest_transform+GlobalInverseRoot를 담고, 런타임 `BuildSkeletonFromStage3`가 둘을 결합한다(`Model.cpp` 1155-1182행).
  - `.wanim`은 끝에 `WAnimTrailer{skel_hash}`를 갖고, skelHash는 `WSkelWriter`가 모든 본 name_hash를 FNV1a로 fold한 값이다(`WSkelWriter.cpp` 43-53행). `LoadAsAnimation`은 trailer가 기대 해시와 다르면 **즉시 로드 거부**(`WAnimLoader.cpp` 101-103행).
  - 애니 채널은 bone_index가 아니라 **bone_name_hash**로 저장 → 로드 시 현재 스켈레톤에서 역탐색(`ResolveBoneByHash`), 없는 본은 skip. 본 순서가 달라도 재바인딩 가능하다.
  - 본 저장 순서는 DFS pre-order 불변식으로 writer/loader 양쪽에 계약을 박았다 — `WSkelWriter.cpp` 30행: "★ Model.cpp::LoadSkeleton 과 동일 DFS pre-order". 부모 인덱스 < 자식 인덱스가 보장되므로 런타임 `CSkeleton::ComputeFinalTransformsWithScratch`는 정렬/재귀 없이 인덱스 오름차순 **단일 루프**로 `global = local * global[parent]`를 계산한다(`Skeleton.cpp` 63-73행, Final = Offset × Global × GlobalInverseRoot).
- **감수한 비용**: 파일이 하나 더 생기고(스키닝 모델당 .wskel), `.wmesh/.wskel` 본 이름 배열 동일성까지 교차 확인해야 한다(`Model.cpp`의 `WMeshAndWSkelNamesMatch` 호출, 993-994행). 검증이 늘어난 대신 잘못된 조합이 로드 단계에서 걸러진다.

### 결정 5 — 리소스 캐시의 이원 수명 정책: 텍스처 raw-ptr vs 모델 shared_ptr

- **왜**: 텍스처와 모델은 공유 패턴이 다르다. 텍스처는 "캐시가 살아있는 동안 항상 유효"로 충분하고, 모델은 여러 렌더러 인스턴스가 참조를 들고 있다.
- **선택 이유**: `ResourceCache.cpp` — 키는 백슬래시→슬래시 + 소문자 정규화(`NormalizePath`)로 같은 파일의 중복 로드를 막고, sRGB 무시 텍스처는 `|ignore-srgb` 접미로 별도 키(17-22행). **텍스처는 캐시가 unique_ptr로 소유하고 raw `CTexture*`를 반환**(호출자 비소유, 41-44행), **모델은 shared_ptr 반환**으로 인스턴스들이 참조카운트를 공유한다(67-69행). 모델 경로는 `ToCookedModelPath`가 확장자를 `.wmesh`로 치환해 원본 FBX 경로로 요청해도 쿠킹본을 찾는다(110-116행) — 호출부가 쿠킹 여부를 몰라도 된다.
- **감수한 비용**: 두 정책이 섞여 있어 "이건 소유인가 참조인가"를 API 주석으로 설명해야 한다. 또 `ResourceCache.h` 39행에 스스로 남긴 부채가 있다: "이것도 CName 해싱으로 string 전부 교체하기!!! - CDPR 방식" — string 키 비교 비용은 알고 있고, 해시 네임 도입은 향후 과제로 명시해 뒀다.

### 결정 6 — 정적 메시 결합(combined static) + 런타임 range 병합 / 스키닝은 개별 유지

- **왜**: 맵 같은 정적 지오메트리는 submesh가 수백 개라 개별 draw가 병목이다.
- **선택 이유**: `Model.cpp` — 정적 경로는 모든 submesh를 material_index로 stable_sort한 뒤 **하나의 VB/IB로 결합**하고 `CombinedSubmeshRange`만 유지(218-232행). 렌더 시 인접 range가 같은 텍스처+연속 인덱스면 `FlushPending` 람다로 병합해 draw call과 material bind를 줄인다(565-613행). 인덱스는 정점 65535 초과거나 원본이 32bit일 때만 32bit로 승격(208-209행). 스키닝 모델은 본 팔레트 갱신이 인스턴스별이라 submesh별 개별 CMesh를 유지했다 — 결합의 이득이 본 관리 복잡도를 못 이긴다.
- **감수한 비용**: 결합 메시는 submesh 단위 컬링 정밀도가 떨어질 수 있다. 그래서 컬링을 계층화했다: 쿠킹 시 submesh AABB를 미리 저장(has_bounding)하고 로드 시 authored 우선/정점 재계산 폴백(`BuildSubmeshBounds`, 411-431행), 렌더 전 로컬 AABB 8코너를 clip 공간으로 변환해 테스트, submesh 128개 이상이면 NDC 화면 점유 0.0035 미만 초소형까지 tiny-cull(773-774행). 반대로 맵처럼 submesh 512개 이상(`kCombinedStaticCoarseCullSubmeshThreshold`)이면 per-submesh 컬링 자체를 bypass한다(25, 753-763행) — 컬링 비용이 이득을 넘는 지점을 상수로 못박고 profiler 카운터(`Model::ClipBypassLargeCombinedStatic`)로 관측한다.
- **검증**: `Model::VisibleMeshCalls / CombinedDrawCalls / MaterialBinds` 카운터가 렌더 루프에 박혀 있어(615-617행) 병합 효과를 수치로 확인할 수 있다.

### 결정 7 — 텍스처: sRGB는 셰이더 로컬 처리, 밉은 첫 Bind에서 지연 생성

- **왜**: WIC/DDS 로더는 기본적으로 1-mip만 만들고, 밉 생성은 immediate context가 필요해 스레드 제약이 있다.
- **선택 이유**: `Texture.cpp` — Create는 1-mip SRV만 만들고 `m_bMipsPending=true`(235-236행), 첫 Bind 시점(렌더 스레드)에 `EnsureMipsOnBind`가 GENERATE_MIPS 텍스처로 CopySubresourceRegion→GenerateMips로 밉체인을 생성·교체한다(86-156행). "immediate context를 사용하므로 반드시 렌더 스레드(Bind 시점)에서만 호출한다"를 주석 계약으로 박았다. 이미 밉이 있거나(DDS) MSAA/1x1/포맷 미지원이면 skip. sRGB는 머티리얼 텍스처를 `ShaderLocalSRGB`(WIC_LOADER_IGNORE_SRGB + FORCE_RGBA32)로 로드해 감마를 셰이더에서 직접 다루기로 했다(205-219행). 월드 텍스처 샘플러는 경사면 minification 울렁임 방지로 Anisotropic 필수, UI/Decal은 Clamp+Linear(238-252행 주석 포함).
- **감수한 비용**: 첫 바인드 프레임에 밉 생성 비용이 온다. 로딩 시점 일괄 생성 대비 "실제로 그려지는 텍스처만 밉을 만든다"는 이득과 교환했다.

---

## ④ 어려웠던 점과 해결 (war stories)

### 1) Riot FBX의 제멋대로인 텍스처 경로 — 휴리스틱 스코어링

외부 FBX는 텍스처 경로가 절대경로/파일명만/임베디드(`*N`)/아예 없음으로 제각각이었다. `WMaterialWriter`가 다단계로 해석한다: BASE_COLOR→DIFFUSE 순 조회 → 절대경로면 그대로, 없으면 modelDir/파일명 재탐색, 임베디드는 `textures/embedded/`로 추출 저장. 머티리얼에 텍스처가 아예 없으면 modelDir을 스캔해 `ScoreDiffuseCandidate`로 점수를 매긴다 — 파일명이 머티리얼 토큰 포함 +100, base/body +60, basecolor/albedo/diffuse/_cm/_tx +20 (`WMaterialWriter.cpp` 228-264행). 최종 경로는 `ToRuntimePath`가 `/client/bin/resource/` 마커 기준 런타임 상대경로로 정규화한다(48-58행). "완벽한 규칙이 없으면 점수화하고, 결과는 쿠킹 산출물에 박제해서 런타임은 고민하지 않게 한다"가 결론이었다.

### 2) Layer 오버레이 노드로 인한 z-fighting — 쿠킹 단계에서 원천 제거

일부 FBX는 이름이 `Layer`로 시작하는 오버레이 노드(중복/반투명 오버레이 메시)를 포함해 그대로 구우면 z-fighting과 중복 렌더가 났다(넥서스/바론 텍스처 진단 때 겪은 "Destroyed 중첩 메시" 계열 문제). `WMeshWriter`의 `CollectMeshIndicesFromNode`가 prefix `Layer` 노드를 재귀적으로 skip하고 스킵 통계를 OutputDebugStringA로 남긴다(36-131행). 필터 결과가 비면 전체 메시로 안전 폴백(113-118행), 필요하면 `--include-layers` 옵션으로 되살릴 수 있다. **런타임에서 픽셀 단위로 싸우지 말고 에셋 단계에서 데이터를 고친다**는 원칙의 사례다.

### 3) Elden 에셋의 불완전한 스켈레톤 — 명시적 정적 폴백

FromSoftware 추출 맵 에셋은 본 데이터는 있는데 `.wskel`이 없거나 불일치하는 경우가 있었다. 일반 챔피언이라면 `.wskel` 누락은 E_FAIL이지만, `ShouldFallbackSkinnedMeshToStatic`이 경로에 `/assets/limgravestatic/`, `/fullgame/asset/`, `/fullgame/map/`이 포함되면 스키닝을 포기하고 combined static으로 강등해 화면에는 뜨게 했다(`Model.cpp` 154-164, 978-1007행). 중요한 건 폴백 경로도 OutputDebugStringW로 흔적을 남긴다는 것 — **관용은 허용하되 침묵은 허용하지 않는다.**

### 4) 조용한 실패 경로의 전사적 fail-fast 전환

씬 전환 `OnEnter()` 실패가 무시되고 무조건 S_OK를 반환하던 것을, sceneID를 담은 로그 + E_FAIL 반환으로 바꿨다(`Scene_Manager.cpp` 30-36행). 같은 방향으로 `RHITextureLoader.cpp`는 WIC 각 단계(CoCreateInstance/CreateDecoder/GetFrame/GetSize/FormatConverter/CopyPixels)마다 stage명+HRESULT+경로를 찍는 `LogTextureLoadFailure`를 달았다. "텍스처가 안 보인다"는 증상에서 어느 단계가 죽었는지 즉시 알 수 있게 된 것 — 에러 처리 정책 리팩터링(실패 즉시 가시화)의 리소스 파이프라인 적용분이다.

### 5) 성능 회복은 계측과 함께 — 17.8ms → 9ms

프레임이 17.8ms까지 밀렸던 시기에, Profiler 스코프/카운터를 먼저 깔아(`WINTERS_PROFILE_SCOPE("Model::WMeshLoad")`, `Model::VisibleMeshCalls` 등) 병목(스키닝 갱신)을 특정했고, 같은 세션에서 정적 엔티티 bAnimated 스킵과 함께 27개 모델 `.wmesh` 전수 변환 + 런타임 쿠킹본 통합(이 챕터의 기여분 = 로드 경로)을 진행해 9ms로 회복했다(2026-04-24 세션 기록). `CProfilerOverlay`는 1초 간격 샘플링/freeze로 안정화된 뷰를 ImGui에 그리고 `CaptureToJson`으로 저장한다(`Engine/Public/Manager/Profiler/ProfilerOverlay.h`). 면접 포인트: **"빨라졌다"가 아니라 "어떤 카운터가 어떻게 변했다"로 말할 수 있는 상태**를 먼저 만들었다.

---

## ⑤ 향후 개선 방향

1. **압축/무결성**: `WintersFileHeader.flags`에 `WF_LZ4`, `WF_HAS_SHA256`이 예약돼 있고 현재는 `WF_NONE`만 허용(`WintersFileHeader.h` 8행 주석 — "Stage 1 SHA256 + Stage 9 Ed25519 서명은 MVP 이후"). 헤더 확장 없이 플래그만 켜면 되는 구조로 미리 파 놨다.
2. **캐시 키의 CName 해싱**: string 키를 해시 네임으로 교체하는 계획을 `ResourceCache.h` 39행에 명시. 캐시 룩업 비용과 문자열 중복 저장을 줄인다.
3. **씬 스코프 언로드**: `CGameInstance::Clear_Resources`는 현재 BlueprintRegistry의 씬 스코프만 정리하고 "추후 ResourceCache::Unload_Scene 연계 예정" 주석으로 부채를 박제해 뒀다(`GameInstance.cpp` 202-208행). 지금은 리소스 해제가 전체 Clear에 의존한다.
4. **RHI 스키닝 경로**: `ModelRenderer::AppendRenderSnapshotMeshes`는 `HasSkeleton()`이면 조기 return 0 — "RenderWorldSnapshot/RHISceneRenderer currently has no skinned vertex path or bone palette"(`ModelRenderer.cpp` 664-667행). 정적 메시만 새 RHI 스냅샷 렌더러로 넘어갔고 캐릭터는 레거시 경로에 남아 있다. 미완 경계를 코드+주석으로 명시해 사고를 막는 방식.
5. **번들 로딩**: 로더가 `LoadFromMemory`를 이미 노출한다(`WMeshLoader.h` 33-34행 "메모리 버퍼 직행 (번들 대비)") — 파일 단위에서 아카이브/번들 단위 로딩으로 갈 확장점.
6. **소켓(장착점)**: `.wskel`의 SocketEntry는 구조만 정의되고 `socket_count=0`(`WSkelWriter.cpp` 79행) — 무기 장착점 등 확장 예약.

---

## ⑥ 면접 Q&A

### Q1. "자체 에셋 포맷을 왜 만들었나요? Assimp를 런타임에 쓰면 되지 않나요?"

**답변 골격**: 세 가지 이유. (1) 성능 — FBX 파싱/탄젠트 생성/좌표계 변환을 매 실행 반복할 이유가 없어 오프라인으로 밀었다. 17.8ms→9ms 복구 세션의 로드 경로 기여분이 이 쿠킹본 통합이었다. (2) 의존성 — Assimp는 Tools exe에만 링크하고 Engine.dll은 의존이 없다. 배포 바이너리가 가벼워지고 임포터 버그가 런타임에 못 들어온다. (3) 신뢰 경계 — 런타임은 상한/정합 검증을 통과한 불변 바이너리만 읽는다.
**꼬리질문 대비**: "포맷 버전 관리는?" → 공통 16B 헤더의 version_major/minor + 미지원 flags 조합 로드 거부. Stage `.dat`도 `STAGE_VERSION=5 / MIN_COMPAT=3`으로 하위 호환 범위를 명시한다(`Shared/GameSim/Definitions/MapDataFormats.h`).

### Q2. "직렬화는 어떻게 구현했나요?"

**답변 골격**: 고정 레이아웃 POD를 `#pragma pack(1)` + `static_assert(sizeof==N)`으로 컴파일 타임에 잠그고, 로더는 헤더만 `Read<T>()`로 복사, 정점/인덱스 blob은 파일 메모리 포인터를 그대로 잡는 zero-copy다. blob 수명은 `WMeshLoaded::m_vRawFile`이 소유한다 — "포인터가 가리키는 메모리를 누가 소유하는가"를 구조체 주석으로 명시했다.
**꼬리질문 대비**: "엔디안/플랫폼 이식성은?" → 현재 타깃이 Windows x64 단일이라 리틀엔디안 고정을 전제했고, 이식 시 헤더 버전으로 구분 가능하다고 답한다(정직하게 스코프를 밝힘).

### Q3. "손상된 파일이 들어오면 어떻게 되나요?"

**답변 골격**: 크래시가 아니라 로드 실패로 떨어진다. magic/버전/flags → 개수 상한(정점 1000만, submesh 2048, 본 1024) → stride와 포맷 플래그 교차 검증 → blob마다 Remaining 비교로 오버런 차단 → parent_index 범위 검증, 전체 try/catch. 애니는 `SpanFits`로 키 span이 블록 안에 있는지까지 본다.
**꼬리질문 대비**: "왜 예외를 삼키나?" → 로더 경계에서 실패는 '이 파일을 못 쓴다'는 단일 의미라 bool로 접었고, 원인 로그는 호출부(CModel)에서 경로와 함께 남긴다.

### Q4. "애니메이션이 다른 스켈레톤에 잘못 붙는 사고는 어떻게 막았나요?"

**답변 골격**: 이중 가드. (1) `.wanim` trailer의 skel_hash — 스켈레톤 전체 본 이름을 FNV1a로 fold한 해시를 쿠킹 시 박고, 로드 시 기대 해시와 다르면 즉시 거부. (2) 채널은 본 인덱스가 아니라 본 이름 해시로 저장 → 현재 스켈레톤에서 역탐색해 바인딩하므로 본 순서가 달라도 안전하고, 없는 본은 skip. 추가로 `.wmesh/.wskel` 본 이름 배열 동일성도 로드 시 교차 확인한다.
**꼬리질문 대비**: "해시 충돌은?" → FNV1a 64bit, 본 수십 개 스케일에서 충돌 확률은 무시 가능하지만, 충돌해도 이름 문자열이 파일에 함께 있어 진단 가능하다.

### Q5. "같은 챔피언을 10개 그리면 메모리는 어떻게 되나요?"

**답변 골격**: flyweight. 메시/스켈레톤/애니 키 데이터는 ResourceCache의 `shared_ptr<CModel>` 하나를 공유하고, 인스턴스별로는 상수버퍼(cbPerFrame/cbPerObject), 본 팔레트 SRV, 인스턴스 애니메이터(공유 스켈레톤 포인터 + 개별 시간축), 텍스처 오버라이드만 둔다. Shutdown에서 `pSharedModel.reset()`은 참조카운트만 줄이고 실제 파괴는 ResourceCache가 관장한다.
**꼬리질문 대비**: "텍스처는 왜 shared_ptr이 아닌가?" → 캐시가 unique_ptr로 소유하고 raw 반환 — 캐시 수명이 디바이스 수명과 같아 dangling 위험이 없고, 참조카운트 오버헤드를 뺐다. 수명 정책을 리소스 종류별로 의도적으로 다르게 갔다.

### Q6. "드로우콜 최적화는 어떻게 했나요?"

**답변 골격**: 정적 지오메트리는 쿠킹된 submesh들을 material 정렬 후 단일 VB/IB로 결합하고, 렌더 시 같은 텍스처+연속 인덱스 range를 `FlushPending`으로 병합해 draw call/material bind를 줄였다. 효과는 `Model::CombinedDrawCalls / MaterialBinds / VisibleMeshCalls` profiler 카운터로 수치 검증했다. 컬링은 authored AABB(정점 폴백) + clip 공간 8코너 테스트 + 128개 이상 메시의 tiny-cull, 단 512 submesh 이상 대형 결합 메시는 컬링 비용이 이득을 넘어 통째로 bypass한다.
**꼬리질문 대비**: "왜 스키닝은 결합 안 했나?" → 본 팔레트가 인스턴스별 갱신이고 submesh 수도 적어 결합 이득이 관리 복잡도를 못 이긴다.

### Q7. "텍스처 로딩에서 신경 쓴 부분은?"

**답변 골격**: (1) 지연 밉 — 로드 시 1-mip만 만들고 첫 Bind(렌더 스레드)에서 GenerateMips로 체인 생성·교체. immediate context 제약을 "Bind 시점에서만 호출"이라는 주석 계약으로 관리. (2) sRGB — 머티리얼 텍스처는 IGNORE_SRGB+FORCE_RGBA32로 로드해 감마를 셰이더 로컬에서 처리. (3) 샘플러 — 월드는 Anisotropic 필수(경사면 minification 울렁임), UI/Decal은 Clamp+Linear. (4) 실패 가시화 — WIC 단계별 stage명+HRESULT+경로 로그.
**꼬리질문 대비**: "왜 로딩 스레드에서 밉을 안 만들었나?" → DX11 immediate context는 단일 스레드 제약이 있고, deferred context/스테이징 복잡도 대비 첫 바인드 1회 비용이 저렴했다.

### Q8. "에셋이 수백 개인데 반복 작업(iteration)은 어떻게 빠르게 유지했나요?"

**답변 골격**: incremental 쿠킹. 배치 스크립트가 소스 FBX mtime뿐 아니라 컨버터 exe의 mtime도 출력물과 비교한다 — 컨버터 코드가 바뀌면 전체 재쿠킹이 자동으로 강제되므로 "구버전 쿠커 산출물이 섞이는" 사고가 없다. 애니처럼 산출물 개수가 가변인 것은 `.wanim.stamp`로 완료를 표시한다. skel→mesh→anim 순서 의존은 함수로 캡슐화하고 실패는 카운터로 집계해 요약 exit한다.
**꼬리질문 대비**: "내용 해시 기반이 더 정확하지 않나?" → 맞다. mtime은 touch에 취약하지만 구현 비용 대비 현재 스케일(수백 파일)에서 충분했고, 해시 기반은 SHA256 플래그 도입과 함께 갈 수 있다.

### Q9. "게임 데이터(맵/엔티티)는 코드와 어떻게 분리했나요?"

**답변 골격**: 두 축. (1) 엔티티 정의 — `CEntityBlueprintRegistry`가 씬 스코프로 blueprint를 등록하고 `Clone_Entity(sceneID, key, world, pArg)`로 인스턴스화한다. 씬 종료 시 `Clear_Scene`으로 해당 스코프만 파기. (2) 맵 배치 — 에디터에서 편집한 구조물/정글 배치를 `CMapDataIO::Save_Stage/Load_Stage`가 `Data/StageN.dat` 바이너리로 저장한다. 포맷은 'WSTG' magic + version 5 + 하위 호환 하한(min-compat 3)을 가진 고정 레이아웃 엔트리(`StructureEntry`에 tier/lane/team까지)다. 포맷 구조체가 `Shared/GameSim/Definitions/`에 있어서 서버도 같은 정의로 스테이지를 읽는다.
**꼬리질문 대비**: "왜 JSON이 아니라 바이너리?" → 에디터 저장물은 필드가 고정적이고 파싱 신뢰성이 중요했다. 버전 필드로 진화 여지를 뒀고, 사람이 읽을 필요가 있는 진단은 converter의 info 서브커맨드처럼 역파싱 도구로 해결한다.

### Q10. "미완성인 부분을 솔직히 말해 달라."

**답변 골격**: 세 가지를 코드에 박제해 뒀다. (1) 스키닝 모델은 아직 RHI 스냅샷 렌더러에 못 올라간다 — 본 팔레트 경로가 없어 조기 return + 이유 주석. (2) 씬별 리소스 언로드 미구현 — Clear_Resources가 Blueprint 스코프만 정리하고 ResourceCache 연계는 주석으로 예약. (3) 레거시 DX11 경로와 RHI 경로가 이중으로 공존한다 — CMesh는 지오메트리당 GPU 버퍼를 두 벌 만들고, RHI 쪽 인덱스는 32bit로 통일해 추상 경로를 단순화했다(`Mesh.cpp` 130-147행). 과도기 비용을 알고 감수하는 중이며, 경계마다 "왜 여기서 멈췄는지"를 주석으로 남겨 후속 작업자가 사고 없이 이어갈 수 있게 했다.
**꼬리질문 대비**: "이중 버퍼 메모리 낭비 아닌가?" → 맞다, 과도기 비용이다. 이관 완료 시 레거시 버퍼 제거가 종착점이고, 그 전까지는 정적 메시부터 RHI 스냅샷으로 검증하며 옮기는 전략이다.

---

## ⑦ 다른 챕터와의 연결

- **렌더링/RHI 챕터**: 여기서 만든 `CombinedSubmeshRange`/VisibilityMask가 렌더 루프의 draw 병합·컬링 입력이 되고, 레거시 DX11 ↔ RHI 스냅샷 이중 경로의 경계(스키닝 미이관)가 렌더러 이관 로드맵과 직결된다.
- **씬/게임 루프 챕터**: `CScene_Manager`의 결정적 전환 순서(OnExit → Clear_Resources → Safe_Reset → OnEnter fail-fast)가 리소스 수명의 상위 트리거다. 영속 static scene + 교체 current scene 이중 슬롯 구조 포함.
- **애니메이션 챕터**: DFS pre-order 본 불변식, skelHash 가드, 인스턴스 애니메이터(flyweight)가 애니 시스템의 입력 계약이다.
- **에러 처리 철학 챕터**: 로더 상한선/fail-fast/폴백-with-로그 패턴은 `.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md`의 "실패 즉시 가시화" 원칙의 리소스 도메인 적용이다.
- **cpp 세트 연결**:
  - `.md/interview/cpp/02_compile_link_dll.md` — Engine.dll이 Assimp 의존을 끊은 툴/런타임 링크 경계.
  - `.md/interview/cpp/03_memory_lifetime_raii.md` — `m_vRawFile`이 blob 수명을 소유하는 zero-copy 패턴의 수명 규칙.
  - `.md/interview/cpp/04_pointers_smart_pointers.md` — 텍스처 raw-ptr vs 모델 shared_ptr 수명 정책의 근거.
  - `.md/interview/cpp/12_network_serialization.md` — pack(1) POD + 검증 로더 패턴은 네트워크 패킷 직렬화와 같은 계열의 설계다.
