# 면접 대비 — 도메인 10: 에셋 파이프라인 / Winters 바이너리 포맷

> 근거 문서: `.md/이력서/WINTERS_DOMAIN_HONEST_MAP_2026-06-26.md` "### 10." (상태: **working**)
> 핵심 코드: `Engine/Public/AssetFormat/`, `Engine/Private/AssetFormat/`, `Engine/Private/Tools/AssetConverter/main.cpp`, `Engine/Private/Resource/Model.cpp`
> 계획 문서: `.md/plan/WintersFormat/00_WINTERS_FORMAT_INDEX.md`, `.md/guide/CHAMPION_WMESH_PIPELINE_GUIDE.md`

---

## 0. 한 줄 본질 + 현재 상태

**한 줄 본질**: "런타임에 FBX를 파싱하면 챔피언 1체당 50~200ms가 사라진다 — 그래서 오프라인에서 한 번만 Assimp로 변환해 두고, 런타임은 디스크 바이트를 거의 그대로 GPU 정점 레이아웃으로 읽는 자체 바이너리 포맷(`.wmesh`/`.wskel`/`.wanim`/`.wmat`)을 설계·구현한 에셋 cook→load 파이프라인."

**현재 성숙도(정직하게)**:
- **working (end-to-end 동작)**: 4종 포맷의 writer/loader 본문이 실제 로직이고, `WintersAssetConverter.exe`로 FBX→`.w*` cook → `CWMeshLoader`/`CWSkelLoader`/`CWAnimLoader`가 런타임 로드 → `Model.cpp`가 실제 메시·애니로 재생까지 닫혀 있음. 디스크 산출 바이트가 코드 POD와 1:1 일치.
- **planned-only (코드 0줄)**: SHA256 무결성, Ed25519 서명, LZ4 압축, `.wtex`/`.wmap`/`.winters` 번들. 헤더에 `flags`·`WF_LZ4`·`WF_HAS_SHA256` enum 자리만 있고, 모든 로더는 `flags != WF_NONE`이면 **거부(false 반환)**하는 스텁이다.

즉 "MVP는 zero-parse 로드 한 가지에 집중했고, 무결성·압축·번들·텍스처는 헤더에 자리만 잡고 설계만 끝낸 상태"가 정확한 자기 진단이다.

---

## 1. 핵심 개념 (본질)

### 1.1 왜 런타임 FBX 파싱이 문제인가 (first principles)
FBX/glTF 같은 DCC 교환 포맷은 **저작 도구 간 호환**이 목적이다. 그 대가로:
- 노드 그래프·머티리얼 그래프·복수 좌표계·다양한 정점 속성 조합을 **범용 파서(Assimp)**가 런타임마다 재해석해야 한다.
- 정점 데이터가 GPU가 원하는 메모리 레이아웃과 다르다 → 파싱 후 재배열(remap)·재계산(노멀/탱젠트)·본 가중치 정규화가 매 로드마다 발생.
- Assimp는 무거운 DLL 의존성이고 예외/실패 경로가 많다.

결과적으로 챔피언 1체 로드에 50~200ms. 인게임 진입·스킨 교체 때마다 체감되는 hitch.

### 1.2 자체 바이너리 포맷이 푸는 것: "파싱을 빌드 타임으로 옮긴다"
핵심 통찰은 **"파싱은 한 번만 하면 된다"**이다. 게임 에셋은 빌드 후 불변이므로, 비싼 해석(Assimp 임포트, 좌표계 변환, 탱젠트 계산, 본 인덱스 packing)을 **오프라인 cook 단계로 1회 이동**시키고, 런타임은 이미 GPU가 원하는 모양으로 직렬화된 바이트를 읽기만 한다. 이것이 Unreal `.uasset`, Source `.mdl`, id Tech `.bsp`가 공통으로 하는 일이다.

### 1.3 zero-copy / "memcpy만으로 업로드 가능한 레이아웃"
가장 중요한 설계 원리. 런타임 정점 블롭의 byte layout을 **GPU Input Layout(IL)의 `AlignedByteOffset`과 byte 단위로 동일**하게 cook해 둔다. 그러면 로더는:
1. 파일을 메모리에 한 번 로드(`std::vector<uint8_t>`),
2. 헤더만 읽어 offset/count를 알아낸 뒤,
3. 정점/인덱스 블롭은 **파일 메모리의 포인터를 그대로 가리킨다(`Peek()`)** — 복사·재배열 없음.

`WMeshLoaded`(`WMeshLoader.h:11`)가 이 zero-copy를 표현한다: `pVertexBlob`/`pIndexBlob`은 원본 파일 메모리를 가리키는 포인터이고, 그 수명은 같은 구조체의 `m_vRawFile`이 소유한다. CMesh 생성 시 이 포인터를 그대로 `CreateBuffer`에 넘긴다.

### 1.4 POD ABI 계약 (byte offset 매칭) — 이 도메인의 심장
`VertexSkinned`(`WMeshFormat.h:73`)는 `#pragma pack(push,1)` + `static_assert(sizeof==76)`로 76B 고정. 이 76B 레이아웃이 셰이더 `Skinned3D.hlsl`의 IL `AlignedByteOffset`과 정확히 맞아야 한다:

| 필드 | offset | semantic |
|---|---|---|
| pos[3] | 0 | POSITION |
| nrm[3] | 12 | NORMAL |
| uv[2] | 24 | TEXCOORD |
| tan[3] | 32 | TANGENT |
| indices[4] (uint32×4) | 44 | BLENDINDICES |
| weights[4] | 60 | BLENDWEIGHT |

여기에 필드 하나라도 추가/패딩이 끼면 GPU가 다음 필드를 1byte씩 밀어 읽어 정점이 NaN/0이 되고 **메시가 소리 없이 사라진다**(애니·Transform 로그는 정상이라 진단이 어렵다). 그래서 `static_assert`로 ABI를 컴파일 타임에 못 박았다.

### 1.5 교차 검증 해시 (cook과 load의 정합성)
세 파일(`.wmesh`/`.wskel`/`.wanim`)은 따로 cook되지만 한 캐릭터에 묶여야 한다. 본 인덱스가 어긋나면 스키닝이 폭발하므로:
- `.wskel`이 **본 DFS 순서의 권위**이고 FNV-1a `skel_hash`를 가진다.
- `.wmesh`는 그 skel 순서로 정점 본 인덱스를 pack한다(`mesh --skel`).
- `.wanim`은 trailer에 `skel_hash`를 박아 둔다(`WAnimTrailer`, `WAnimFormat.h:77`).
- 런타임에 hash/본 카운트가 안 맞으면 fast-path를 버리고 Assimp fallback으로 자동 전환.

---

## 2. 왜 이 선택인가 — Trade-off

| 선택지 | 장점 | 단점 | 내 결정 이유 |
|---|---|---|---|
| **자체 바이너리 vs 런타임 FBX** | 파싱 0, GPU 직행 레이아웃 | cook 단계·툴 유지보수 필요, 포맷 버저닝 부담 | 50~200ms hitch 제거가 인게임 체감 1순위. cook은 1회성 |
| **POD `#pragma pack(1)` + memcpy vs FlatBuffers/protobuf 역직렬화** | 진짜 zero-copy, 의존성 0 | 스키마 진화 취약(필드 추가=깨짐), 엔디언 고정 | 메시/정점은 거대 blob이라 역직렬화 오버헤드가 곧 비용. 정점은 스키마가 거의 안 변함 |
| **`.wmesh`/`.wskel`/`.wanim` 분리 vs 단일 파일** | skel만 재생성, anim 독립 교체, 본 권위 명확 | 3파일 정합(hash) 관리 필요 | 애니는 수십 개라 메시와 수명이 다름. 분리가 작업 단위와 일치 |
| **오프라인 codegen(CLI) vs 런타임 변환** | 런타임 슬림, 실패를 빌드 타임에 격리 | 에셋 추가 시 변환 절차 필요 | 게임 에셋은 불변. 비싼 해석을 빌드로 미는 게 원칙 |
| **Assimp fallback 유지 vs fast-path 전용** | 미변환/깨진 에셋도 화면엔 나옴(개발 편의) | 두 경로 유지, fallback이 fast-path 실패를 가림 | 1인 개발 중 변환 누락은 흔함. 안전망 가치 > 코드 중복 비용 |
| **stride 화이트리스트(48/76)만 허용 vs 임의 stride** | 잘못된 IL 매칭 조기 거부 | 새 정점 포맷마다 화이트리스트 갱신 | 정점 포맷이 2종뿐이라 화이트리스트가 곧 ABI 게이트 |

**"왜 신입 1인 프로젝트에서 합리적인가"**: 무결성·압축·번들까지 한 번에 만들면 검증할 표면이 폭발한다. 그래서 *측정 가능한 단일 가치(zero-parse 로드)*만 끝까지 닫고, 나머지는 헤더에 enum 자리(`WF_LZ4`/`WF_HAS_SHA256`)만 예약해 두고 "지금은 `WF_NONE`만 지원, 그 외 플래그는 명시적 거부"로 경계를 그었다. 이게 미래 확장을 막지 않으면서 현재 검증 범위를 좁히는 방법이다.

---

## 3. 실제 구현 (코드 근거)

### 3.1 공통 헤더 — 16B 고정
`WintersFileHeader`(`WintersFileHeader.h:18`): `magic[4]="WINT"` + `version_major/minor` + `flags` + `content_size`, `static_assert(sizeof==16)`. 모든 `.w*` 파일 선두 16B. 그 뒤에 포맷별 sub-magic(`WMSH`/`WSKL`/`WANM`/`WMAT`)이 온다 → 이중 매직으로 "Winters 파일인가 + 어떤 종류인가"를 분리 판정.

### 3.2 BinaryReader/Writer — 직렬화 유틸
- `CBinaryReader`(`BinaryReader.h:12`): 커서 기반 read-only. `Read<T>()`는 `is_trivially_copyable` static_assert로 POD만, `Peek()`로 zero-copy 포인터 반환, `Skip()`으로 blob 건너뜀. `LoadFileToMemory`가 파일→`vector<uint8_t>`.
- `CBinaryWriter::SaveToFile`(`BinaryWriter.cpp:14`): 16B 헤더를 자동으로 앞에 붙이고 페이로드를 쓴다. `content_size = m_vBuf.size()`. SHA256 append는 **아직 없음**(MVP).

### 3.3 `.wmesh` writer — cook의 핵심 (`WMeshWriter.cpp`)
`CWMeshWriter::WriteFromAssimp`(`:351`):
1. `CollectMeshIndicesForWrite`(`:97`): 노드 트리 순회, `Layer*` overlay 노드는 옵션으로 skip(EldenRing FLVER 레이어 메시 필터).
2. 본 존재 여부로 skinned/static 결정 → stride 76/48.
3. `CollectBones`(`:134`): `--skel`이 주어지면 **wskel 인덱스 순서를 권위로** BoneEntry를 채우고(`pSkelNameToIdx`), offset_matrix는 Assimp `mOffsetMatrix`를 `ConvertAndTranspose`(`:26`, 행/열 전치 — CLAUDE.md gotcha 준수)로 변환. skel 없으면 mesh-local 순서로 발견순 등록.
4. `AppendVertices`(`:245`): byte offset을 **명시적 `memcpy(p+0/+12/+24/+32/+44/+60,...)`**로 채운다 → IL과 1:1. 본 가중치는 vertex당 4슬롯(`LimitBoneWeights`), 슬롯 0개면 `weight=1, index=0` 기본값(rigid fallback). bone>1024면 FATAL.
5. 인덱스는 정점 수 >65535면 uint32, 아니면 uint16으로 다운캐스트(`:414`).
6. `MeshMetaHeader` 채우고 sub테이블→정점블롭→인덱스블롭→본테이블→bounds 순으로 직렬화 후 `SaveToFile(WF_NONE)`.

### 3.4 `.wmesh` loader — zero-copy load (`WMeshLoader.cpp`)
`CWMeshLoader::LoadFromMemory`(`:41`):
1. `WintersFileHeader` 읽고 magic/`version_major==1`/`flags==WF_NONE`/`content_size<=Remaining` 검증(`:48~53`).
2. `MeshMetaHeader` → `ValidateMeta`(`:13`): sub-magic, MAX 상한(악성 파일 방어: 정점 1천만/서브메시 2048/본 1024), `index_stride∈{2,4}`, **stride 화이트리스트(48/76)**, 그리고 `VF_BoneWeight` 플래그와 stride 정합(skinned면 반드시 76).
3. 정점/인덱스 블롭은 `Peek()`로 **포인터만 잡고 Skip**(`:69`, `:77`) — 복사 0.
4. 본 테이블은 `ReadBytes`로 vector 채우고 `parent_index` 범위 검증.
5. 전 과정 `try/catch`로 감싸 손상 파일에 nullptr/false 반환.

### 3.5 `.wanim` loader — 교차검증 + ECS 애니 변환 (`WAnimLoader.cpp`)
`LoadAsAnimation`(`:61`): 헤더 검증 후 채널 테이블·key 블록·event·trailer를 분리. **trailer의 `skel_hash != expectedSkelHash`면 즉시 nullptr**(`:102`) — 잘못 짝지어진 anim 차단. 각 채널의 `bone_name_hash`를 런타임 `CSkeleton`에서 `ResolveBoneByHash`(`:41`, FNV1a 역참조)로 이름/인덱스 복원 후 pos/rot/scl 키를 `BoneChannel`로 빌드. `SpanFits`(`:34`)로 offset+count가 key 블록 경계를 넘지 않는지 OOB 검증.

### 3.6 런타임 통합 — fast-path vs Assimp fallback (`Model.cpp`)
`ReplaceExtToWMesh`(`:72`)로 FBX 경로를 `.wmesh`로 바꿔 자동 탐색. 흐름:
- `.wmesh` 없으면 → Assimp fallback(`:962`).
- skinned인데 `.wskel` 없거나 `WMeshAndWSkelNamesMatch`(`:100`, 본 카운트+name_hash 비교) 실패 → fallback(`:994`).
- 통과 시 `BuildMeshesFromWMesh`(`:114`)가 zero-copy 포인터(`pVertexBlob+vertex_offset`)로 CMesh 생성, `LoadCookedAnimations`로 anim 로드.
- EldenRing static 에셋 경로(`/limgravestatic/` 등)는 `ShouldFallbackSkinnedMeshToStatic`(`:154`)로 skinned→static 강등 후 combined 메시 빌드.

### 3.7 CLI (`main.cpp`)
`WintersAssetConverter.exe {mesh|skel|material|anim|info}`. `ReadScene`(`:64`)이 Assimp postflags(Triangulate/LeftHanded/GenNormals/CalcTangentSpace/LimitBoneWeights) 고정. `mesh`는 `--skel`로 본 권위 주입, `anim`은 scene의 N개 애니를 각각 `.wanim`으로 출력. **`info`**(`:323`)가 헤더 spot-check 도구 — 다음 §4의 검증 게이트.

---

## 4. 검증 — 동작을 어떻게 증명했나

자동 골든 테스트는 **없다**(정직하게). 검증은 3계층 수동/도구 게이트:

1. **`info` 헤더 게이트** (`main.cpp:323`): 변환 직후 3파일을 `info`로 덤프해 통과 기준 3개 동시 만족 확인 —
   - `wmesh.stride == 76`(Skinned3D IL 계약),
   - `wmesh.bone_count == wskel.bone_count`,
   - `wanim.skel_hash == wskel.hash`.
   하나라도 어긋나면 변환 순서 위반(`skel→mesh --skel→anim`) 또는 POD 변경을 의미하므로 즉시 진단된다.

2. **런타임 fallback 로그 게이트** (`Model.cpp`): fast-path 진입 시 `[CModel] cooked load OK`, 실패 시 `wmesh/wskel mismatch`/`build failed` 로그. fallback 로그가 뜨면 "fast-path가 깨졌다"는 신호 — 조용히 느려지는 걸 로그로 가시화.

3. **F5 인게임 시각 체크리스트** (`CHAMPION_WMESH_PIPELINE_GUIDE.md §3.3`): bind-pose 탈출, Idle/Run 무한 재생, 우클릭 이동 시 run, BA/QWER 애니, **스키닝 폭발 0**. byte offset 미스매치는 "fast-path 로그 정상인데 화면 안 보임"으로 나타나므로 시각 확인이 최종 게이트.

4. **구조적 무결성 검증**: `static_assert`가 모든 POD 크기를 컴파일 타임에 고정(`VertexSkinned==76`, `MeshMetaHeader==36`, `BoneEntry==128` 등) → ABI가 깨지면 빌드가 실패한다. ValidateMeta의 MAX 상한이 손상/악성 파일을 거부.

**측정한 것**: "디스크 산출 바이트 == 코드 POD" 정합성(라운드트립 writer→loader), 본 인덱스 정합(hash), 화면 정상 재생. **측정 안 한 것(정직하게)**: 로드 시간 정량 수치 — §5 참조.

---

## 5. 최적화

### 실제로 한 것
- **zero-copy 정점 블롭**: 정점/인덱스를 복사 없이 파일 메모리 포인터로 GPU에 직행(`WMeshLoader.cpp:69`). 메시가 클수록(수만 정점) 이득이 커지는 구조.
- **인덱스 stride 적응**: 65535 이하면 uint16로 저장(`WMeshWriter.cpp:414`) → 인덱스 버퍼 절반.
- **stride/본 상한 화이트리스트**: 잘못된 파일을 파싱 전에 거부해 런타임 방어.

### 정량 수치 (정직성 지도 준수)
"로드 1ms"는 **설계 목표치이고 실측하지 않았다.** 어필할 때는 수치 대신 **구조적 근거**로 말한다: "Assimp 임포트·좌표 변환·탱젠트 계산·본 packing을 빌드 타임으로 옮겨 런타임에서 파싱 단계 자체를 제거했다. 그래서 런타임 비용은 파일 read + 헤더 파싱 + GPU 업로드뿐이다." 정량화는 "측정 예정"으로 둔다 — 프로파일러(도메인 12)의 `Model::WMeshLoad` 스코프(`Model.cpp:969`)가 이미 박혀 있어 캡처만 하면 된다.

### 계획 중인 최적화
- **JobSystem 멀티스레드 로드**: cook된 `.wmesh`는 의존성 없는 독립 파일이라 워커 풀에 fan-out 가능(INDEX 로드맵).
- **`.wtex`(BC7) 도입 시 Assimp 완전 제거**: 현재 텍스처 경로만 FBX 머티리얼에 의존해 챔프 로드 시 Assimp를 30~50ms 호출한다(`PIPELINE_GUIDE §0`). `.wtex`로 분리하면 `ReadFile` 자체를 없애 챔프 로드 <5ms 목표.

---

## 6. 구현 예정 (Planned) — 동일한 깊이

### 6.1 SHA256 무결성 (Stage 9 / Security Level 1)
- **무엇**: 페이로드 SHA256 32B를 파일 끝에 append, 로드 시 재계산 비교.
- **왜**: 치트 메시 교체(히트박스 키운 모델 등) 방어 1차선. 현재 `flags`에 `WF_HAS_SHA256` enum 자리만 있고 로더는 이 플래그가 켜지면 거부한다 — 즉 "설계 자리만 예약".
- **어떻게**: writer `SaveToFile`에 옵션 분기 추가 → 페이로드 해시 후 trailer 32B write, `flags|=WF_HAS_SHA256`. loader는 `content_size` 뒤 32B를 읽어 `VerifySHA256()`. CryptoAPI(`BCryptHashData`) 또는 단일 헤더 SHA256.
- **Trade-off**: 로드마다 전체 페이로드 해시 = O(파일크기) CPU. 큰 메시엔 무시 못 함 → "민감 에셋만" 또는 "개발 빌드만" 검증 전략 필요.
- **검증**: 1바이트 변조 파일이 거부되는지, 정상 파일이 통과하는지 라운드트립 테스트.

### 6.2 LZ4 압축 (`WF_LZ4`)
- **무엇**: 페이로드를 LZ4로 압축, 로드 시 해제 후 zero-copy.
- **왜**: 디스크/번들 크기 + I/O 시간 절감.
- **어떻게**: writer에서 페이로드 압축 후 원본 크기를 헤더에 기록. loader는 `WF_LZ4`면 별도 버퍼에 decompress → `WMeshLoaded.m_vRawFile`이 그 해제 버퍼를 소유(포인터 수명 모델 그대로 재사용).
- **Trade-off**: **zero-copy 깨짐** — 해제 버퍼가 별도 할당이라 mmap 직참조 불가. "압축 vs zero-copy"는 정면 충돌하므로, 핫 에셋은 비압축·콜드 에셋만 압축하는 선택형으로 갈 것.
- **검증**: 압축률·해제 시간 측정해 zero-copy 손실 대비 이득 비교.

### 6.3 `.winters` 번들 (Stage 8)
- **무엇**: Valve VPK + Unreal PAK 스타일. `[Header][TOC][압축 블록들][서명]`, TOC가 `name_hash→offset/size/sha256` 매핑.
- **왜**: 수천 파일 배포 → 단일 번들 + open 핸들 1개. EldenRing Limgrave는 placement 3,862개라 파일 핸들/seek가 폭증.
- **어떻게**: `CWintersBundle::Open` → mmap, `ExtractAsset(nameHash)`로 TOC 이진탐색 후 블록 슬라이스 반환. 기존 `CWMeshLoader::LoadFromMemory`(`:34`)가 이미 **메모리 직행 진입점**을 노출해 둔 게 이 번들 대비 설계다.
- **Trade-off**: 번들 빌드 단계 추가, 부분 갱신 비용(에셋 1개 바뀌면 재패키징). 개발 중엔 loose 파일, 배포만 번들로 이원화.
- **검증**: 번들 추출 == loose 파일 로드 결과 동일(바이트 비교).

### 6.4 `.wtex` (BC7 + 밉맵) / `.wmap`
- **`.wtex`**: DirectXTex로 PNG→BC7 압축+밉맵 cook. Assimp 텍스처 의존 제거가 목적(§5). GPU 네이티브 BC 포맷이라 디코드 없이 직행.
- **`.wmap`**: 현재 `Stage1.dat` flat POD dump(버전 없음)를 헤더+NavGrid bits 포함 포맷으로 승격. 두 포맷 동시 로드 지원으로 점진 이관.

### 6.5 Ed25519 서명 (번들 전용)
- 번들 전체 해시를 개발 키로 서명, Release에 공개키 임베드, 불일치=변조=종료+서버 신고. Security Phase 2 이후. **현재 코드 0줄, 문서 설계만.**

---

## 7. 면접 예상 질문 & 모범 답변

**Q1 (기본). 왜 FBX를 그냥 안 쓰고 자체 포맷을 만들었나요?**
> FBX는 DCC 툴 간 교환용이라 범용 파서가 런타임마다 노드 그래프·좌표계·머티리얼을 재해석해야 합니다. 챔피언 1체에 50~200ms hitch가 났습니다. 게임 에셋은 빌드 후 불변이니, 비싼 해석을 오프라인 cook으로 1회 옮기고 런타임은 GPU가 원하는 레이아웃의 바이트를 그대로 읽게 했습니다. Assimp DLL 런타임 의존도 줄였습니다.

**Q2 (기본). "zero-copy 로드"가 구체적으로 뭔가요?**
> 정점 블롭의 byte layout을 GPU Input Layout의 AlignedByteOffset과 byte 단위로 동일하게 cook해 둡니다. 그래서 로더는 파일을 메모리에 한 번 올리고 헤더만 읽은 뒤, 정점/인덱스는 그 메모리 포인터(`Peek()`)를 그대로 CMesh에 넘깁니다. 재배열·재계산 복사가 없습니다. `WMeshLoaded`의 `pVertexBlob`이 원본 버퍼를 가리키고 수명은 `m_vRawFile`이 소유합니다.

**Q3 (설계). 왜 mesh/skel/anim을 세 파일로 나눴나요? 합치는 게 로드는 빠르지 않나요?**
> 수명과 작업 단위가 다릅니다. 애니메이션은 캐릭터당 수십 개이고 메시와 독립적으로 교체·추가됩니다. skel은 본 인덱스의 권위라 한 번 정하면 mesh와 anim이 그걸 따라야 합니다. 합치면 anim 하나 바꿀 때 전체를 재cook해야 합니다. 대신 정합성은 FNV `skel_hash`로 묶어, 잘못 짝지어지면 런타임이 거부하게 했습니다.

**Q4 (설계). 본 인덱스 정합은 어떻게 보장하나요?**
> `.wskel`이 본 DFS 순서의 단일 권위입니다. mesh는 `--skel`을 받아 그 순서로 정점 본 인덱스를 pack하고, anim은 trailer에 skel_hash를 박습니다. 변환은 반드시 `skel → mesh --skel → anim` 순서여야 합니다. 런타임에서 `WMeshAndWSkelNamesMatch`가 본 카운트와 name_hash를 비교하고, anim 로더는 trailer hash가 안 맞으면 nullptr를 반환합니다.

**Q5 (심화). 정점 포맷에 필드를 하나 추가하면 무슨 일이 일어나나요?**
> 76B stride가 깨지고 GPU가 그 뒤 필드를 1byte씩 밀어 읽어 정점이 NaN/0이 됩니다. 메시가 소리 없이 사라지는데, 애니·Transform 로그는 정상이라 진단이 가장 어려운 버그입니다. 그래서 `static_assert(sizeof(VertexSkinned)==76)`로 컴파일 타임에 못 박고, 로더의 ValidateMeta가 stride를 48/76 화이트리스트로만 허용합니다. 새 포맷은 IL부터 offset을 읽어 POD를 설계하는 순서로 갑니다.

**Q6 (adversarial). SHA256 무결성·Ed25519 서명으로 치트 메시를 막는다고 하셨는데, 그거 실제로 구현돼 있나요?**
> 아니요, 거기까지는 안 했습니다. `flags`에 `WF_HAS_SHA256`/`WF_LZ4` enum 자리만 예약돼 있고, 현재 모든 로더는 `flags != WF_NONE`이면 명시적으로 거부합니다. 무결성·압축·서명은 설계만 끝낸 상태입니다. MVP를 zero-parse 로드 한 가지에 집중시키려고 의도적으로 범위를 좁혔고, 헤더에 자리만 잡아 나중에 확장이 깨지지 않게 했습니다. 구현 시엔 페이로드 해시를 trailer로 append하고 로드 시 재계산 비교하는 방식이고, 큰 메시엔 해시 비용이 O(파일크기)라 민감 에셋·개발 빌드만 검증하는 전략을 쓸 겁니다.

**Q7 (adversarial). LZ4 압축 했다고 README에 쓸 수 있나요? 로딩 1ms는요?**
> 둘 다 못 씁니다. LZ4는 코드 0줄, `WF_LZ4` enum 자리뿐입니다. 그리고 LZ4를 넣으면 해제 버퍼가 별도 할당이라 제 zero-copy 모델과 정면 충돌합니다 — 그건 trade-off로 인지하고 있고, 핫 에셋은 비압축으로 둘 계획입니다. "1ms"도 설계 목표치이고 실측 안 했습니다. 어필한다면 수치 대신 "파싱 단계 자체를 빌드 타임으로 제거했다"는 구조적 근거로 말하고, 프로파일러에 `Model::WMeshLoad` 스코프가 이미 박혀 있으니 캡처해서 보여드릴 수 있습니다.

**Q8 (adversarial). 자동 테스트는 있나요? 어떻게 "됐다"를 증명하죠?**
> 자동 골든 테스트는 없습니다, 정직하게. 대신 3계층 게이트가 있습니다. 첫째, 변환 직후 `info` CLI로 stride==76, mesh.bone_count==skel.bone_count, anim.skel_hash==skel.hash 세 조건을 확인합니다. 둘째, 런타임 fallback 로그 — fast-path 깨지면 mismatch 로그가 떠서 조용한 성능 저하를 가시화합니다. 셋째, F5 인게임에서 스키닝 폭발 0과 Idle/Run 재생을 눈으로 확인합니다. 그리고 모든 POD가 `static_assert`로 크기 고정이라 ABI가 깨지면 빌드 자체가 실패합니다. 자동화는 다음 단계로, info 출력을 파싱하는 회귀 스크립트가 가장 싼 자동화 진입점입니다.

**Q9 (adversarial). 손상되거나 악의적인 파일을 주면요?**
> ValidateMeta가 막습니다. sub-magic 불일치, 정점 1천만/서브메시 2048/본 1024 상한 초과, index_stride가 2/4가 아님, stride가 48/76이 아님, 플래그-stride 불일치를 모두 거부합니다. 정점/인덱스 블롭 크기가 남은 바이트를 넘으면 false, anim은 `SpanFits`로 key offset+count가 블록 경계를 넘는 OOB를 검증합니다. 전체가 try/catch로 감싸여 nullptr/false로 안전하게 떨어집니다. 다만 SHA256이 없어 "내용이 의미상 올바른지"는 검증 못 하고 "구조가 안전한지"까지입니다.

**Q10 (확장). EldenRing 3,862개 placement는 어떻게 로드하나요? 번들 없이?**
> 현재는 loose `.wmesh` 파일들을 MSB placement를 source of truth로 자동 로드합니다. 그래서 파일 핸들·seek가 많아 번들이 다음 우선순위입니다. `.winters` 번들은 TOC가 name_hash→offset 매핑이고 open 핸들 1개로 처리합니다. 이걸 대비해서 `CWMeshLoader`에 `LoadFromMemory`라는 메모리 직행 진입점을 이미 분리해 뒀습니다 — 번들이 슬라이스 포인터만 넘기면 됩니다.

**Q11 (확장/날카로운). Assimp fallback이 있으면, fast-path가 실제로 항상 도는지 어떻게 알죠? fallback이 실패를 숨기지 않나요?**
> 정확한 지적입니다. fallback은 양날의 검입니다 — 변환 누락이나 ABI 깨짐을 화면상으론 정상으로 보이게 가립니다. 그래서 fast-path/fallback 진입을 각각 다른 로그(`cooked load OK` vs `mismatch fallback`)로 구분하고, 가이드 문서에 "fallback 로그 = 즉시 진단" 규칙을 박았습니다. 다만 로그를 사람이 봐야 한다는 한계가 있어, fallback 진입 카운터를 프로파일러에 노출하거나 CI에서 fallback 발생 시 실패시키는 게 다음 보강입니다.

---

## 8. 30초 엘리베이터 피치

> "런타임에 FBX를 파싱하면 챔피언 1체당 50~200ms가 날아갑니다. 그래서 비싼 파싱을 오프라인 cook으로 한 번만 하고, 런타임은 디스크 바이트를 GPU 정점 레이아웃 그대로 읽는 자체 바이너리 포맷 `.wmesh`/`.wskel`/`.wanim`/`.wmat`을 설계·구현했습니다. 정점 stride를 76byte로 고정하고 셰이더 Input Layout과 byte 단위로 맞춰 zero-copy 업로드를 달성했고, `static_assert`로 ABI를 컴파일 타임에 못 박았습니다. Assimp 컨버터부터 런타임 로더, 실제 인게임 재생까지 end-to-end로 동작합니다. 중요한 건, SHA256 무결성·LZ4 압축·번들은 헤더에 자리만 예약하고 의도적으로 안 만들었다는 겁니다 — MVP를 'zero-parse 로드' 한 가지에 집중시키고, 어디까지 했고 어디부터 계획인지 `flags` enum과 검증 게이트로 스스로 선을 그어 둔 게 이 작업의 핵심입니다."
