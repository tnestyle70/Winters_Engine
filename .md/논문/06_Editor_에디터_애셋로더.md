# 06. 에디터·애셋 파이프라인 — 박사 연구 심화

> 이 문서는 `00_PHD_Paper_Guide.md`의 개념 틀(특히 §1 "구현 vs 기여", §3 thesis statement, §4 구조, §7 평가)을 전제로 한다.
> 이 분야는 그래픽스·물리처럼 "더 예쁜 픽셀"을 다투는 분야가 아니다. **Systems / Software Engineering** 성격이 압도적으로 강하다. 측정 대상은 화질이 아니라 **빌드 시간(build latency), 스트리밍 지연·대역폭(streaming hitch / bandwidth), 메모리 예산(memory budget), 재현성(determinism / reproducibility), 그리고 개발자 생산성(edit-to-see latency)**이다.
> 그래서 top venue도 SIGGRAPH가 아니라 **SOSP / OSDI / EuroSys / ASPLOS / FAST(스토리지) / ICSE / FSE**다. 게임 엔진 도구는 "그래픽스 논문의 부속물"로 취급되어 학술적으로 과소연구된(under-studied) 영역이고, 바로 그 점이 박사 기회다.

---

## 0. 이 분야를 박사로 본다는 것 (구현 vs 기여 + top venue)

### 0.1 "에셋 파이프라인을 만들었다"는 박사가 아니다

가이드 §1로 즉시 돌아가자.

> ❌ "Unreal의 UnrealPak·DDC(Derived Data Cache)·AsyncLoading2·Hot Reload·Niagara 에디터를 다 따라 구현했다" → 이것은 **엔지니어링 결과물(industrial artifact)**이다.

Unreal Engine, Unity Addressables/AssetBundle, Bazel, Frostbite의 빌드팜은 이미 존재하고 산업 표준에 가깝다. 그것을 재현하는 것은 훌륭한 시니어 엔진 작업이지만 **인류 지식에 새 명제를 더하지 않는다.** 에디터·파이프라인 박사의 기여는 다음 중 하나여야 한다.

- **새 알고리즘**: 같은 정확도(=올바른 산출물)를 더 적은 재빌드로 얻는 **정밀 무효화(fine-grained invalidation)**, 또는 같은 메모리 예산에서 hitch를 줄이는 **예측 prefetch** 정책.
- **새 자료구조**: content-addressable store의 **분산 캐시 일관성**을 더 적은 RPC로 보장하는 인덱스, 또는 핫 리로드 reference fixup을 O(변경분)으로 처리하는 GUID 인덱스.
- **새 시스템/아키텍처**: 기존 도구로 불가능하던 조합 — 예: "결정론적·증분 쿠킹"과 "분산 빌드팜 캐시 공유"를 **비트 단위 재현성을 깨지 않고** 동시에 만족시키는 빌드 시스템.
- **새 이론/모델**: 핫 리로드 시 **타입 변경(schema migration)의 안전성**을 정적으로 보장하는 타입 시스템, 또는 협업 편집의 **수렴(convergence) 증명**(CRDT의 SEC, Strong Eventual Consistency).
- **새 경험적 발견 / 벤치마크**: "실제 게임 콘텐츠 변경의 P%는 단일 파라미터 수정인데 현존 쿠커는 그 중 Q%에서 전체 의존 그래프를 재빌드한다"는 측정과, 이를 재는 **공개 벤치마크**. 가이드 §5-6에서 과소평가되지만 강력하다고 한 유형.

### 0.2 이 분야의 심장: "변경의 전파(propagation of change)"

렌더링의 심장이 빛 수송이고 프로파일러의 심장이 observer effect라면, 에디터·파이프라인의 심장은 단 하나다.

> **무언가 하나가 바뀌었을 때, 정확히 무엇을 다시 해야 하고 무엇은 하지 않아도 되는가 (what must be recomputed, and what must not)?**

이 한 질문이 네 세부 주제를 관통한다.

- **§1 쿠킹**: source asset 하나가 바뀌면 어떤 cooked 산출물을 다시 굽는가? (의존 그래프 무효화)
- **§2 스트리밍**: 카메라가 움직이면 어떤 셀/번들을 디스크에서 올리고 어떤 것을 버리는가? (공간적 변경 전파)
- **§3 핫 리로드**: 애셋이 디스크에서 바뀌면 런타임의 어떤 객체·참조·GPU 리소스를 갱신하는가? (라이브 상태 전파)
- **§4 에디터**: 사용자가 한 번 클릭하면(undo 단위) 씬의 어떤 상태가 바뀌고, 협업자에게 어떻게 전파하는가? (편집 의도 전파)

"전부 다시 한다"는 항상 정답이지만(over-approximation), 항상 느리다. "필요한 것만 다시 한다"는 빠르지만, **놓치면(under-approximation) 조용히 틀린 결과**를 낳는다. 박사 명제는 거의 항상 **"이 정밀도/안전성 곡선을 새 기법으로 한 칸 옮긴다"**의 형태다 — 더 정밀하게 무효화하되 절대 놓치지 않거나, 더 공격적으로 prefetch하되 메모리 예산을 깨지 않거나.

### 0.3 Determinism이 모든 것의 하한이다

게임 파이프라인이 일반 빌드 시스템(컴파일러)과 결정적으로 다른 점: **산출물이 거대하고(GB~TB), 입력이 부동소수·텍스처 압축·메시 최적화처럼 비결정적 도구를 거친다.** 같은 입력에 같은 cooked 바이트가 나오지 않으면(non-reproducible build):

- 분산 캐시가 무력화된다(해시가 매번 달라 cache miss).
- 패치 diff가 폭발한다(안 바뀐 애셋도 바이트가 달라 재다운로드).
- 협업·핫 리로드에서 "누구의 산출물이 진짜인가"를 판단할 수 없다.

그래서 이 분야의 평가에는 항상 **재현성 축**이 들어간다(가이드 §7). "재현 가능한 빌드(reproducible builds)"는 시스템 SE의 독립 연구 주제(Lamb & Zacchiroli, *Reproducible Builds: Increasing the Integrity of Software Supply Chains*, IEEE Software 2022)이며, 게임 쿠킹은 그 가장 가혹한 워크로드다.

### 0.4 Top Venue

| 성격 | Top Venue |
|------|-----------|
| 빌드 시스템·증분·캐시·재현성 | **OSDI, SOSP, EuroSys, ASPLOS, USENIX ATC**; 빌드 SE는 **ICSE, FSE, ASE** |
| 스토리지·비동기 I/O·스트리밍 | **FAST**(USENIX Conf. on File and Storage Tech.), ATC, EuroSys; SSD/NVMe는 HotStorage |
| 협업 편집·일관성·CRDT/OT | **PODC, DISC**(이론), **CSCW, UIST**(시스템·HCI), EuroSys |
| 리플렉션·직렬화·라이브 프로그래밍 | **PLDI, OOPSLA, ‹Programming›, ICSE**; live programming은 LIVE 워크숍 |
| 산업(비학술, 인용용) | **GDC**(Naughty Dog/Insomniac/Frostbite의 빌드·스트리밍 발표), CppCon |

> **주의(가이드 §9):** Unreal/Unity 문서, GDC 발표, Bazel/Buck2 엔지니어링 블로그는 강력한 근거지만 동료심사 학술 출판이 **아니다.** 영향력·사례 근거로 인용하되, 1차 기여 주장은 학술 venue에 건다. 반대로 빌드 시스템·CRDT·재현성은 의외로 탄탄한 학술 문헌(Mokhov et al., Shapiro et al., Lamb & Zacchiroli)이 있으므로 그것을 baseline으로 정면으로 비교해야 한다.

---

## 1. 애셋 쿠킹(Cooking) 파이프라인과 결정론적·증분 빌드

### 1.1 핵심 원리

게임 애셋은 두 세계로 나뉜다.

```text
source asset (저작용)            cooked asset (런타임용)
  .fbx .png .psd .wav      ──cook──►  .wmesh .wtex .wsound (...)
  - 사람이 편집              - 기계가 즉시 로드
  - 크다, 느리다, 무손실      - 작다, 빠르다, 플랫폼 최적
  - DCC 도구 종속            - 엔진 메모리 레이아웃 그대로
```

쿠킹(cooking, = baking, = importing)은 source → cooked 변환이다. 핵심 원리는 셋이다.

**(1) Cooked는 "메모리 이미지"여야 한다.** 좋은 cooked 포맷은 런타임이 `read()` 한 번 후 거의 그대로 쓸 수 있도록 **이미 정렬·이미 압축·이미 엔디안 정리·포인터 대신 오프셋**으로 만들어 둔다. 파싱 비용을 빌드 타임에 한 번 지불하고 로드 타임엔 0에 가깝게 만든다. Winters의 `.wmesh`가 `vertex_stride=76`처럼 고정 스트라이드를 강제하는 것(02 §6)이 정확히 이 원리다 — 런타임이 vertex 레이아웃을 추론할 필요가 없다.

**(2) 빌드는 의존 그래프(DAG) 위의 함수다.** 각 cooked 산출물은 입력들(source 파일 + 변환 파라미터 + 도구 버전)의 **순수 함수(pure function)**여야 한다. 같은 입력 → 같은 출력(결정론). 그러면 빌드는 DAG 위상정렬(topological sort) 후 노드를 평가하는 문제로 환원된다.

**(3) 증분(incremental)은 "안 바뀐 노드를 건너뛰기"다.** 입력이 그대로면 출력도 그대로이므로(순수 함수 + 결정론), 다시 굽지 않는다. 이를 위해 각 노드의 입력을 식별하는 키가 필요하다 — timestamp(약함) 또는 **content hash(강함)**.

**(4) Content-Addressable Storage (CAS).** 산출물을 "경로"가 아니라 "내용의 해시"로 저장한다. `store[sha256(bytes)] = bytes`. 같은 내용이면 같은 주소 → 자동 중복 제거(dedup), 자동 캐시 키, 자동 무결성 검증. Git의 object store가 정확히 이 구조다.

### 1.2 대표 기존 연구/시스템

- **Make** (Feldman, 1979): 최초의 의존 그래프 빌드. timestamp 기반 무효화. 결함: timestamp는 "내용 변화"가 아니라 "수정 시각 변화"만 봄 → touch만 해도 재빌드, 클럭 스큐에 취약, 분산 캐시 불가.
- **Build Systems à la Carte** (Mokhov, Mitchell, Peyton Jones, ICFP 2018 / JFP 2020): 빌드 시스템을 **이론적으로 분해**한 기념비적 논문. 빌드 시스템을 두 축으로 정의 — *스케줄러*(topological / restarting / suspending)와 *rebuilder*(dirty bit / verifying trace / constructive trace / deep constructive trace). Make=topological+dirty bit, Bazel=topological+deep constructive trace, Shake=suspending+verifying trace. **에디터 파이프라인 박사라면 반드시 이 프레임워크로 자기 시스템을 위치시켜야 한다.**
- **Bazel / Buck2** (Google / Meta): hermetic(밀폐) + 결정론 빌드 + 원격 실행/캐시(Remote Execution API). 모든 액션을 입력 해시로 키잉해 빌드팜이 결과를 공유. 게임 빌드팜의 직접 조상.
- **Vesta** (Heydon et al., *Software Configuration Management with Vesta*, 2006): 불변(immutable) 저장소 + 완전한 의존 추적으로 재현 가능 빌드를 한 시스템 차원의 고전.
- **Unreal DDC (Derived Data Cache)**: cooked 파생물을 키-값으로 공유(로컬→공유→클라우드 계층). 키는 (source GUID + 변환 버전 + 플랫폼). 산업 사실표준이나 학술 출판 아님.
- **Reproducible Builds 프로젝트 / Lamb & Zacchiroli (IEEE Software 2022)**: 비결정성의 출처(타임스탬프, 파일 순서, 비결정 스레드, 절대경로, 난수 시드)를 체계적으로 제거하는 방법론. 게임 쿠킹의 비결정성(텍스처 인코더의 멀티스레드 비결정, FBX importer의 부동소수)에 그대로 적용된다.

### 1.3 자료구조/알고리즘(의사코드)

**핵심 알고리즘: content-hash 기반 증분 쿠킹 (verifying/constructive trace).**

각 노드의 캐시 키는 입력들의 해시를 합성한다. 키가 store에 있으면 굽지 않고 가져온다.

```text
# 노드 = 하나의 cooked 산출물을 만드는 변환
struct CookNode:
    inputs:   list<AssetRef>      # source 파일들 + 의존 cooked 산출물들
    recipe:   ToolId + Params     # 어떤 변환을, 어떤 파라미터로
    tool_ver: Version             # 변환 도구 바이너리 버전

function cook_key(node) -> Hash:
    h = Hasher()
    h.update(node.recipe.tool_id)
    h.update(node.tool_ver)             # 도구가 바뀌면 결과도 바뀜 → 키에 포함
    h.update(canonical(node.recipe.params))   # 파라미터 정규 직렬화
    for inp in sorted(node.inputs):     # 순서 독립성: 정렬 후 해시
        h.update(content_hash(inp))     # ★ timestamp가 아니라 "내용"
    return h.final()

function cook(node, cas: ContentAddressableStore) -> Hash:
    key = cook_key(node)
    if cas.has(key):                    # ── 증분: cache hit
        return cas.resolve_output(key)  #     이미 같은 입력으로 구운 적 있음
    # cache miss: 실제로 굽는다
    out_bytes = run_tool(node.recipe, materialize(node.inputs))
    out_addr  = sha256(out_bytes)
    cas.put(out_addr, out_bytes)
    cas.bind(key, out_addr)             # 입력키 → 출력주소 (constructive trace)
    return out_addr

# 전체 빌드: DAG 위상정렬 후 평가. 의존 산출물의 주소가
# 다시 상위 노드의 content_hash(inp)로 들어가므로 "변경 전파"가 자동.
function build(graph):
    for node in topo_sort(graph):       # suspending 스케줄러면 동적 발견
        cook(node, global_cas)
```

여기서 **§0.2의 심장**이 드러난다. `cook_key`에 들어가는 입력이 정밀할수록 무효화가 정밀하다. 셰이더 파일 전체 해시 대신 "이 머티리얼이 실제 참조한 #include + 키워드"만 키에 넣으면, 주석 한 줄 고쳤다고 셰이더 변형 수천 개를 재컴파일하지 않는다 — 이것이 **fine-grained invalidation** open problem의 핵심.

**Bloom filter 기반 빠른 "안 바뀜" 판정(대규모 그래프 가속):**

```text
# 수십만 노드에서 매번 전체 해시는 비싸다.
# 1) mtime으로 "확실히 안 바뀐" 다수를 빠르게 거른다(cheap, 약함).
# 2) mtime이 바뀐 소수만 content hash로 검증(expensive, 강함).
function maybe_dirty(node):
    if node.mtime == cached_mtime[node]:   # 빠른 경로
        return false                       # 거의 안 바뀜 (timestamp 신뢰)
    if content_hash(node) == cached_hash[node]:  # touch만 된 경우 구제
        cached_mtime[node] = node.mtime
        return false
    return true                            # 진짜 바뀜
```

### 1.4 박사급 novel 각도 (open problems)

1. **정밀 무효화(fine-grained invalidation).** 파일 단위가 아니라 **애셋 내부 필드/파라미터 단위** 의존 추적. "머티리얼의 roughness 슬라이더 하나"가 바뀌면 그 파라미터를 읽는 셰이더 변형만 무효화. 현존 쿠커 대부분은 파일 단위라 over-rebuild가 심하다. *기여 후보: 변환 도구가 "내가 입력의 어느 바이트/필드를 실제로 읽었는가"를 자기보고(self-reporting)하게 하는 추적 빌드(traced build) — Memoize/Fabricate(Hoyt)식 syscall 추적의 게임 애셋판.*
2. **분산 캐시 일관성(distributed cache consistency).** 빌드팜이 여러 머신에서 동시에 같은 노드를 구울 때, **결정론이 깨지면 캐시가 오염**된다(같은 키, 다른 바이트). open problem: 비결정 도구를 쓰면서도 캐시 일관성을 보장하는 정규화(canonicalization) 또는 "첫 산출물 고정(first-writer-wins + 검증)" 프로토콜.
3. **재현성 자동 복원(auto-repair of non-determinism).** 어떤 도구가 왜 비결정적인지(스레드 순서? 시드? 타임스탬프?)를 **자동 진단**하고 정규화 패스를 삽입. ICSE/FSE 색이 강한 기여.
4. **증분과 정확성의 형식적 보장.** "증분 빌드가 from-scratch 빌드와 비트 단위 동일함"을 **검증 가능하게(verifiable)** 만들기 — 무작위 더티 시퀀스에 대한 differential testing 또는 경량 정형기법.

### 1.5 Thesis statement 예시

> "변환 도구가 입력 애셋에서 실제로 소비한 필드 집합을 런타임에 자기보고하게 하는 **소비 추적(consumption-traced) 증분 쿠킹**은, 파일 단위 무효화 대비 동일한 비트-정확 산출물을 보장하면서 전형적 콘텐츠 편집 워크로드에서 재쿠킹 작업량을 X% 줄인다."

falsifiability: "줄인다"가 아니라 "동일 산출물(=정확성 불변) + 재쿠킹 작업량 X% ↓ + 추적 오버헤드 Y% 이내"가 명제. baseline = 파일 단위 해시 쿠커(=Bazel식). ablation = 추적 끄기/켜기.

### 1.6 평가 방법

- **Build latency**: from-scratch vs warm-cache vs 단일 파라미터 변경 후 incremental의 wall-clock(중앙값·P95, 여러 시드의 더티 시퀀스).
- **Rebuild work (정밀도)**: 변경 1건당 *실제로 재실행된 노드 수 / 정답(=의미상 영향받는 노드 수)*. over-rebuild 비율을 핵심 metric으로.
- **Reproducibility**: 같은 입력을 N머신·N회 굽고 cooked 바이트의 해시 일치율(100% 목표). 불일치 시 어느 도구가 원인인지 분해.
- **Cache effectiveness**: 분산 빌드팜에서 hit rate, 전송 바이트, RPC 수.
- **Memory/Disk**: CAS의 dedup 이득(논리 크기 대 물리 크기).
- **Threats to validity**: 워크로드 대표성(실제 편집 로그 vs 합성), 도구 버전 고정, 캐시 워밍 상태.

### 1.7 Winters 연결점

Winters에는 **실재하는 쿠킹 testbed**가 있다.

- **Source → cooked 변환 체인**: `.fbx/.png/.dds → WitchyBND/Blender 정규화 → WintersAssetConverter → .wskel/.wmesh/.wmat/.wanim` (`.md/EldenRing/02_ASSET_EXTRACTION_TO_WINTERS_BINARY_PIPELINE.md` §전체 파이프라인, `10_ASSET_PIPELINE_TOOLING.md`). 이미 다단계 DAG다 — skel이 mesh의 입력(`--skel`)이고, mesh가 다시 런타임 로드의 입력.
- **결정론 검증 게이트가 이미 존재**: `02 §6 info 검증`이 `wanim.skel_hash == wskel.hash`, `wmesh.bone_count == wskel.bone_count`를 강제한다. 이것은 곧 **content hash 기반 의존 일관성 검사의 원형**이다. 박사 기여는 이 hash를 캐시 키로 승격해 증분 빌드를 구축하는 것.
- **Tooling이 Python+Blender+native exe 혼합**: `elden_pipeline.py`(MATBIN/FXR 파서), `blender_apply_materials.py`, `WintersAssetConverter.exe`. 비결정성의 출처(Blender 부동소수 export, FBX importer)가 그대로 있어 **재현성 연구의 살아있는 워크로드**.
- **번들 목표 `.winters`**(07 §Winters Binary 우선순위, 02 §9): CAS 기반 콘텐츠 주소 번들로 설계하면 §1.4-2(분산 캐시)와 직결.
- 즉시 만들 측정 인프라: `WintersAssetConverter`에 `--trace-inputs`(소비 필드 로깅)와 `--emit-cas-key`를 추가하면 §1.5 명제를 바로 검증 가능.

---

## 2. 비동기 스트리밍과 로딩 (Async Streaming)

### 2.1 핵심 원리

오픈월드(Elden Ring 스타일)는 전 콘텐츠를 메모리에 담을 수 없다. 스트리밍의 원리는 **"보일/필요할 것을 미리, 안 보일 것을 버려, 고정 메모리 예산 안에서 끊김 없이(hitch-free)"**다. 네 축이 긴장한다.

**(1) 비동기 I/O — 메인 스레드를 블록하지 마라.** 동기 `read()`는 디스크 지연(NVMe도 수십~수백 μs, HDD는 ms) 동안 프레임을 멈춘다 → hitch. 따라서 I/O를 **워커/완료 큐**로 분리한다. Windows는 **IOCP(I/O Completion Ports)**, Linux는 **io_uring**(Axboe, 2019)이 현대 표준 — 둘 다 "완료를 통지받는" 비동기 모델로 syscall 수와 컨텍스트 스위치를 줄인다.

**(2) 우선순위 — 무엇을 먼저 올릴 것인가.** 플레이어 발밑 콜리전 > 가까운 메시 > 먼 HLOD > 장식 FX. 요청을 우선순위 큐로 관리한다(07 §Request의 priority 1000=player … 100=far HLOD).

**(3) Eviction & Prefetch — 무엇을 버리고 무엇을 당겨올 것인가.** 메모리 예산이 차면 LRU 등으로 축출(evict). 동시에 **이동 방향·속도를 보고 곧 필요할 것을 prefetch**. 여기서 §0.2의 심장이 공간 형태로 나타난다: "카메라가 움직였다" → "어떤 셀을 올리고 어떤 셀을 버리나".

**(4) Virtual File System / 번들(pak).** 수십만 개 loose 파일을 열면 파일시스템 메타데이터·열기 비용이 폭발한다. 그래서 애셋을 **큰 번들(.pak/.winters)로 묶고**, VFS가 "논리 경로 → (번들, 오프셋, 길이)"로 매핑. 한 번 mmap/열기 후 오프셋 read만 한다 → seek 최소화, OS page cache 친화.

**(5) GPU 업로드의 특수성.** 디스크에서 읽은 바이트는 아직 GPU 리소스가 아니다. DX11/12는 리소스 생성이 디바이스 컨텍스트와 얽혀 워커 스레드에서 끝내기 어렵다. 그래서 "워커=파싱/스테이징, 메인/렌더 스레드=GPU 생성, 프레임당 업로드 예산"으로 나눈다(07 §GPU Upload Queue: 프레임당 4 job / 16MB / blocking 5ms).

### 2.2 대표 기존 연구/시스템

- **io_uring** (Jens Axboe, 2019): 링버퍼 기반 비동기 syscall. submission/completion 큐로 배치·폴링. Linux 고성능 I/O의 현재. FAST/ATC에서 활발히 분석됨.
- **Unreal AsyncLoading2 / EventDrivenLoader** (Epic): IoStore 컨테이너(.utoc/.ucas) + 비동기 그래프 기반 로딩. 산업 사실표준, 학술 출판 아님.
- **DirectStorage / RTX IO** (Microsoft, NVIDIA): NVMe→VRAM **직접 경로 + GPU 압축 해제(GDeflate)**로 CPU·복사 단계를 우회. 콘솔(PS5 I/O complex, Cerny의 GDC 발표)에서 출발한 아이디어의 PC판. open problem의 하드웨어적 토대.
- **Sony PS5 I/O / Kraken**: 전용 압축 해제 하드웨어로 스트리밍 대역폭을 수 GB/s로. "로딩 화면 제거"라는 목표를 하드웨어로 푼 사례.
- **OS page cache / mmap, LRU·LIRS·ARC**: 캐시 교체 알고리즘의 고전. LRU의 약점(scan 한 번에 캐시 오염)을 보완한 **LIRS**(Jiang & Zhang, SIGMETRICS 2002), **ARC**(Megiddo & Modha, FAST 2003)는 스트리밍 캐시 정책의 직접 이론 기반.
- **Prefetching 일반론**: OS·DB의 prefetch(순차 탐지, Markov prefetcher)는 게임의 공간적 prefetch로 번역 가능. 게임에서는 "플레이어 궤적 예측"이 곧 prefetch 신호.

### 2.3 자료구조/알고리즘(의사코드)

**우선순위 스트리밍 루프 + 메모리 예산 하 eviction:**

```text
# 매 틱: (1) 우선순위 갱신 (2) 예산 내 업로드 (3) 초과분 축출
priority_queue PQ          # max-heap, key = dynamic priority
lru_list      RESIDENT     # 적재된 애셋, 최근 사용 순
budget_bytes  BUDGET

function on_streaming_source_moved(cam):
    for cell in cells_within(cam.pos, radius):
        pr = base_priority(cell)
        pr += visibility_bonus(cell, cam)        # 보이면 ↑
        pr += predicted_bonus(cell, cam.vel)     # 곧 진입 예측 시 ↑ (prefetch)
        if not resident(cell): PQ.push(cell, pr)
        else                 : RESIDENT.touch(cell)

function stream_tick():
    on_streaming_source_moved(current_camera)
    spent = 0
    while not PQ.empty() and spent < FRAME_UPLOAD_BUDGET:
        cell = PQ.pop_max()
        issue_async_read(cell)        # IOCP/io_uring 비동기 제출
        spent += cell.est_upload_bytes

    # I/O 완료된 것들을 GPU 업로드 큐로 (프레임 예산 분할)
    for job in completed_reads(max=MAX_JOBS_PER_FRAME):
        gpu_upload(job)               # 메인/렌더 스레드 phase
        RESIDENT.add(job.cell)

    # 예산 초과 시 축출 (단, 우선순위 높은 건 보호)
    while resident_bytes() > BUDGET:
        victim = RESIDENT.lru_not_pinned()
        if victim == NULL: break       # 전부 pinned면 예산 초과 허용 or 강등
        evict(victim)
```

**예측 prefetch — 궤적 외삽 + 신뢰도 게이팅(open problem의 한 접근):**

```text
# 단순 LRU/반경 prefetch의 문제: 방향 무관하게 다 당겨 메모리 낭비.
# 개선: 미래 위치를 외삽하되, 예측 신뢰도가 낮으면 당기지 않는다.
function predicted_bonus(cell, vel):
    horizon = PREFETCH_SECONDS
    future  = camera.pos + vel * horizon          # 1차 외삽
    conf    = trajectory_confidence(history)       # 최근 궤적의 직진성/일관성
    if conf < CONF_MIN: return 0                   # 못 믿으면 prefetch 안 함
    d = distance(cell.center, future)
    return conf * gaussian(d, sigma=cell.size)     # 가까울수록·믿을수록 ↑
```

여기서 prefetch는 **투기적(speculative)**이다 — 맞으면 hitch 제거, 틀리면 대역폭·메모리 낭비. 박사 기여는 이 **투기 정확도/비용 곡선**을 옮기는 것.

### 2.4 박사급 novel 각도 (open problems)

1. **학습 기반 예측 prefetch.** 플레이어 행동 로그로 "다음에 갈 셀"을 학습(경량 Markov/시퀀스 모델)하고, **예측 정확도와 메모리/대역폭 비용의 파레토 곡선**을 naive 반경 prefetch 대비 개선. CHI PLAY적 행동 데이터 + 시스템적 평가의 교차.
2. **메모리 예산 하 끊김 없는 보장.** "예산 B, 이동 속도 v_max에서 hitch 없음을 보장하는 prefetch lead time의 하한"을 **정량 모델/증명**으로. 즉 "언제 무엇을 미리 올려야 절대 끊기지 않는가"의 형식적 조건.
3. **DirectStorage/GDeflate 시대의 파이프라인 재설계.** GPU 압축 해제가 CPU 디코드 단계를 없앨 때, 우선순위·예산·캐시 정책이 어떻게 바뀌어야 하는가? CPU-bound 가정으로 짠 기존 스케줄러가 NVMe→VRAM 직접 경로에서 최적이 아님을 보이고 새 정책 제시.
4. **번들 레이아웃 최적화.** 함께 로드되는 애셋을 같은 번들·인접 오프셋에 배치하면 seek·열기 비용↓. "접근 동시발생(co-access) 그래프"로 번들 패킹을 최적화하는 알고리즘(그래프 분할 문제).

### 2.5 Thesis statement 예시

> "플레이어 궤적의 단기 시퀀스 모델로 셀 적재를 투기적으로 선행하는 **신뢰도 게이팅 예측 prefetch**는, 고정 VRAM 예산 하에서 반경 기반 prefetch 대비 스트리밍 hitch(프레임 P99 spike)를 X% 줄이면서 추가 대역폭을 Y% 이내로 유지한다."

baseline = 반경 prefetch + LRU. SOTA 비교 = ARC/LIRS 캐시. ablation = 예측 끄기 / 신뢰도 게이팅 끄기. metric = P99 frame time + bandwidth.

### 2.6 평가 방법

- **Hitch / latency**: 셀 전환 시 frame time 분포(중앙값·P99·최대), "끊김 횟수/분". 결정적 카메라 궤적(replay)으로 재현.
- **Bandwidth**: 초당 디스크 read 바이트, prefetch 적중률(prefetched & used / prefetched).
- **Memory**: 상주 바이트가 예산을 넘는 시간 비율(0이어야 함), eviction 빈도.
- **Scalability**: 월드 크기·이동 속도·동시 스트리밍 소스(멀티플레이) 수 대비 곡선.
- **하드웨어 매트릭스**: HDD / SATA SSD / NVMe / DirectStorage 각각에서 — 정책이 매체에 어떻게 의존하는지.
- **Threats**: 궤적 대표성(실제 플레이 vs 스크립트), 디스크 캐시 워밍, 백그라운드 I/O 간섭.

### 2.7 Winters 연결점

- **스트리밍 런타임 스펙이 이미 설계됨**: `.md/EldenRing/07_ASSET_LOADER_AND_STREAMING_RUNTIME.md`의 `CAssetStreamingSystem` 계층(RequestQueue, AsyncFileLoader, DecodeWorkers, GpuUploadQueue, StreamingBudget)과 우선순위 표·프레임 업로드 예산(4 job/16MB/5ms)이 그대로 §2.3 의사코드의 구현 골격이다.
- **World Partition과의 연결**: `08_WorldPartition_월드파티션.md`의 cell streaming이 §2의 "공간적 변경 전파"의 상위 시스템. 셀 적재 = 애셋 요청 묶음(07 §World Partition 연동: required/optional 분리)이라는 설계가 §2.4-2(끊김 없는 보장)의 분석 단위.
- **Fallback 정책이 이미 명세**(07 §Fallback): mesh→placeholder, texture→gray/normal, anim→bind pose. 이는 "준비 안 됨" 상태에서도 끊기지 않게 하는 **graceful degradation** 측정 대상.
- **번들 우선순위**(07): `.winters` > loose `.w*` > FBX. 번들 레이아웃 최적화(§2.4-4)의 testbed.
- **즉시 측정 인프라**: 07 §Debug Panel(`Asset Streaming`: queued/loading/failed/upload bytes/cache memory/refcount/dependency tree)이 곧 §2.6 metric 수집기. CLAUDE.md의 "inspectable overlay" 문화와 정확히 부합.

---

## 3. 핫 리로드(Hot Reload)와 라이브 편집

### 3.1 핵심 원리

핫 리로드는 "런타임을 끄지 않고 디스크의 애셋/코드 변경을 살아있는 상태에 반영"하는 것이다. edit-to-see 지연을 분 단위(재빌드·재실행)에서 초 단위로 줄여 **개발자 생산성**을 극적으로 바꾼다. 원리는 §0.2의 심장이 "라이브 상태" 형태로 나타난 것이다.

**(1) 참조 고정 / Reference fixup (GUID).** 애셋을 경로가 아니라 **안정적 GUID**로 참조한다. 새 버전을 로드해도 GUID는 같으므로, 그 GUID를 가리키던 모든 런타임 참조가 자동으로 새것을 본다. 경로 참조는 리네임·이동에 깨지지만 GUID는 살아남는다.

**(2) Atomic swap + refcount.** 새 애셋을 백그라운드에서 완성한 뒤, **한 순간에** 핸들이 가리키는 대상을 교체한다. 이전 버전은 아직 참조 중인 곳이 있을 수 있으므로 **refcount가 0이 될 때까지 살려둔다**(07 §Hot Reload: keep old alive → atomic swap → release after no refs). 렌더 스레드가 이전 GPU 리소스를 쓰는 도중 free하면 크래시 → swap은 동기화 지점에서.

**(3) 상태 보존 마이그레이션 (state-preserving migration).** 데이터만 바뀌면(슬라이더 값) 단순 교체로 충분하다. 그러나 **스키마(타입)가 바뀌면**(필드 추가/삭제/타입 변경) 이전 인스턴스의 데이터를 새 레이아웃으로 옮겨야 한다. 이것이 핫 리로드의 가장 어려운 부분이자 §0.2가 가장 날카롭게 드러나는 곳: "타입이 바뀌었을 때, 살아있는 데이터를 어떻게 안전하게 이주하는가".

**(4) 코드/스크립트 리로드.** 데이터 너머 **로직**을 바꾸려면 DLL 교체(C++ hot-reload, Unreal Live Coding) 또는 스크립트 VM 리로드. C++은 vtable·함수 포인터·정적 상태가 얽혀 위험; 스크립트(Lua/C#)는 상대적으로 쉽다. 안전성이 핵심 — 부분적으로 바뀐 코드가 일관성 없는 상태를 만들면 안 됨.

### 3.2 대표 기존 연구/시스템

- **Erlang hot code swapping** (Armstrong, *Making reliable distributed systems in the presence of software errors*, 2003): **라이브 코드 교체의 고전.** 두 버전 공존(old/new) + 다음 외부 호출에서 new로 전환 + `code_change/3` 콜백으로 상태 마이그레이션. 무중단 시스템의 원형으로, 게임 핫 리로드의 상태 마이그레이션 사고틀을 제공.
- **Smalltalk / Lisp 이미지 기반 라이브 프로그래밍**: 살아있는 이미지에서 클래스 재정의 + 기존 인스턴스 마이그레이션(`instanceVariableNames:` 변경 시). 리플렉션 + 라이브 편집의 뿌리.
- **Dynamic Software Updating (DSU)** — **Ginseng**(Neamtiu, Hicks et al., PLDI 2006), **Kitsune**(Hayden et al., OOPSLA 2012): 실행 중 C 프로그램을 안전하게 갱신. **타입 변경 시 데이터 마이그레이션의 안전성**(타입 전이 함수, 갱신 안전 지점 quiescence)을 정면으로 다룬 학술 계보. 게임 핫 리로드 박사라면 반드시 인용·비교.
- **Unreal Hot Reload / Live Coding**: C++ obj/dll 패치. 산업 표준, 학술 아님. 알려진 한계(vtable·UObject 레이아웃 변경 시 불안정)가 곧 §3.4-1의 동기.
- **Niagara / Material Editor live edit**: 파라미터 변경이 즉시 프리뷰에 반영. Winters의 WFX 에디터가 직접 겨냥하는 워크플로우.
- **GUID 기반 애셋 시스템**(Unity .meta GUID, Unreal FName/asset registry): reference fixup의 산업 구현.

### 3.3 자료구조/알고리즘(의사코드)

**GUID 핸들 + atomic swap + refcount 기반 데이터 핫 리로드:**

```text
struct AssetHandle:          # 런타임 참조는 항상 이 핸들을 통함
    guid: Guid
    # 실제 객체는 registry에서 guid로 간접 조회 → swap 시 참조처 무수정

registry: map<Guid, { live: AssetPtr, refcount: int, version: int }>

function on_file_changed(path):
    guid = guid_of(path)
    enqueue_reload(guid)                 # 디스크 변경 감지(file watcher)

# 백그라운드 워커
function reload_worker(guid):
    new_obj = load_and_cook_if_needed(path_of(guid))   # §1 쿠킹과 연결
    if new_obj == ERROR:
        log_compile_error(); return       # ★ 실패 시 기존 유지 (안전)
    enqueue_to_main(guid, new_obj)        # GPU/일관성 위해 메인에서 swap

# 메인/렌더 동기화 지점
function apply_swap(guid, new_obj):
    slot = registry[guid]
    old  = slot.live
    slot.live = new_obj                   # ★ atomic: 이후 조회는 new를 봄
    slot.version += 1
    defer_release(old)                    # refcount 0 될 때 GPU 리소스 해제
    notify_dependents(guid)               # 이 애셋을 쓰는 프리뷰/머티리얼 리셋
```

**스키마 변경 시 상태 보존 마이그레이션(open problem의 핵심):**

```text
# 타입 v_old → v_new. 살아있는 인스턴스의 데이터를 잃지 않고 옮긴다.
function migrate_instance(inst, schema_old, schema_new):
    out = alloc(schema_new)
    for field_new in schema_new.fields:
        if field_new.name in schema_old:           # 1) 이름 일치: 값 이전
            v = inst.read(field_new.name)
            out.write(field_new.name, coerce(v, field_new.type))  # 타입 강제
        elif rename_map.has(field_new):            # 2) 명시적 리네임 규칙
            out.write(field_new.name, inst.read(rename_map[field_new]))
        else:                                      # 3) 신규 필드: 기본값
            out.write(field_new.name, field_new.default)
    # schema_old에만 있던 필드(삭제됨)는 버린다 — 단, 로그로 데이터 손실 경고
    return out
```

여기서 안전성 문제: `coerce`가 무손실인가? 삭제된 필드의 데이터 손실을 사용자가 의도했는가? 마이그레이션 함수 자체가 틀리면? — **이것이 §3.4가 푸는 지점.**

### 3.4 박사급 novel 각도 (open problems)

1. **타입 변경 마이그레이션의 안전성 보장.** 스키마 v_old→v_new의 마이그레이션을 **정적으로 검증**하거나(타입 시스템이 데이터 손실/잘못된 강제를 컴파일 타임 거부), 변경 diff에서 마이그레이션 함수를 **자동 합성**. DSU(Kitsune/Ginseng)의 게임 애셋·ECS 컴포넌트판. *기여 후보: 리플렉션 스키마 diff → 안전한 마이그레이션 또는 "위험" 경고를 산출하는 알고리즘 + 형식적 안전성 정의(no-silent-data-loss).*
2. **GPU 리소스 라이프타임의 race-free 핫 스왑.** 렌더 스레드가 이전 텍스처/PSO를 쓰는 동안 안전하게 교체하는 동기화 프로토콜의 **최소 stall** 버전. EFX-4 2차의 "DX12 PSO rebuild가 render thread와 race 나면 안 된다"(06_EFX4 §7 주의)가 정확히 이 문제.
3. **부분 일관성(partial consistency) 중 안전성.** 의존 애셋 여러 개를 핫 리로드할 때, 일부만 새 버전이면 일시적 비일관 상태가 생긴다. "원자적 묶음 스왑(transactional reload set)"으로 **전부-또는-전무**를 보장하는 메커니즘.
4. **라이브 게임 상태 위 핫 리로드.** 프리뷰가 아니라 **진행 중인 gameplay**에서 FX/밸런스를 바꿀 때(06_EFX4 §7: "실제 gameplay active FX hot reload는 EFX-8에서"), 결정론·네트워크 동기화를 깨지 않고 적용하는 문제(10_Server lockstep과 교차).

### 3.5 Thesis statement 예시

> "리플렉션 스키마의 구조적 diff로부터 상태 마이그레이션 함수를 합성하는 **diff-driven 마이그레이션**은, 데이터 컴포넌트의 타입 변경을 런타임 재시작 없이 적용하면서 무손실 이전(no-silent-data-loss)을 정적으로 보장하고, 수작업 마이그레이션 대비 개발자 작성 코드를 X% 제거한다."

baseline = 재시작(=마이그레이션 없음) / 수작업 마이그레이션 콜백. metric = edit-to-see 지연, 데이터 손실 사고 수(0이어야 함), 개발자가 작성한 마이그레이션 LOC.

### 3.6 평가 방법

- **Edit-to-see latency**: 저장→프리뷰 반영까지 wall-clock(06_EFX4 §7 완료 기준 "Save 후 200ms 이내"가 그대로 metric).
- **Safety / correctness**: 마이그레이션 후 상태가 from-scratch 로드와 동일한가(differential testing); 무작위 스키마 변경 시퀀스에서 데이터 손실/크래시 0건.
- **Stall**: 핫 스왑이 유발한 프레임 spike(ms); race-free 프로토콜의 추가 동기화 비용.
- **Developer productivity**: 마이그레이션 수작업 코드량, 핫 리로드로 절감한 재시작 횟수(사례 연구/로그).
- **Robustness**: 컴파일 실패 시 기존 애셋 유지(06_EFX4 §7 "compile fail 시 기존 material 유지")가 항상 성립하는지.
- **Threats**: 코드(DLL) vs 데이터(애셋) 리로드의 일반화 한계, 타입 변경의 대표성.

### 3.7 Winters 연결점

- **WFX 에디터가 직접적 testbed**: `.md/TODO/05-07/EffectTool/06_EFX4_EDITOR_PREVIEW_AND_HOT_RELOAD.md`가 manual reload(1차: dirty flag → save `.wmi` → reload → preview reset)에서 async reload(2차: file watcher → compile queue → worker load+shader compile → game thread enqueue → registry replace → preview reset)로 가는 **점진적 핫 리로드 로드맵**을 이미 명세. §3.3 의사코드의 `reload_worker`/`apply_swap`이 그대로 대응.
- **실재 코드 경로**: `CFxEditSession`(dirty flag), `CFxMaterialInstanceJsonLoader`(load), `PreviewController`(reset), `EffectToolPanel`(UI). working tree에 `Client/Private/UI/WfxEffectToolPanel.cpp`, `Client/Private/GameObject/FX/WfxDocument.cpp` 등이 존재 — 살아있는 코드.
- **안전성 명세가 이미 의식됨**: 06_EFX4 §7 "DX12 PSO rebuild는 render thread와 race가 나면 안 된다", "compile fail 시 기존 material 유지", "첫 구현은 active preview instance만 reset" — §3.4-2(race-free swap)·§3.4-4(라이브 gameplay)의 문제의식이 문서에 박혀 있다.
- **07 §Hot Reload의 atomic swap + refcount 정책**이 §3.3의 라이프타임 관리와 직결.
- **즉시 측정**: 06_EFX4 §4 Compile Log 패널(JSON parse error / shader compile error / last reload time)이 §3.6의 latency·safety 수집기.

---

## 4. 에디터 아키텍처: Undo/Redo·리플렉션·협업 편집

### 4.1 핵심 원리

에디터는 "사용자 의도 → 데이터 변경 → 영속화·전파"의 기계다. 세 기둥.

**(1) Command 패턴 Undo/Redo.** 모든 편집을 **되돌릴 수 있는 명령 객체**로 표현한다. 각 command는 `execute()`/`undo()`를 갖고, undo 스택에 쌓인다. 핵심 설계 선택:
- *메멘토(memento, 상태 스냅샷)*: 변경 전/후 값을 통째로 저장. 단순하지만 큰 상태에 메모리 폭발.
- *역연산(inverse op)*: "이 값을 A→B로" command가 자기 역(B→A)을 안다. 메모리 적지만 모든 연산에 역을 정의해야.
실무는 보통 작은 상태는 메멘토, 큰 상태는 역연산 + transient 그룹핑(드래그 한 번 = 한 undo).

**(2) 리플렉션 / 직렬화 메타데이터.** 에디터가 임의 타입의 프로퍼티를 **자동으로** 인스펙터에 그리고·직렬화하려면, 타입의 필드 목록·이름·타입·메타(범위, 표시명)를 런타임에 알아야 한다. C++엔 내장 리플렉션이 없으므로 매크로/코드젠(Unreal UPROPERTY, Unity SerializeField)으로 메타데이터를 생성한다. 리플렉션은 §1의 정밀 무효화, §3의 스키마 마이그레이션, §4의 인스펙터·직렬화·undo를 **모두 떠받치는 하부구조**다.

**(3) 협업 편집(collaborative editing).** 여러 사용자가 같은 씬을 동시에 편집할 때 충돌을 어떻게 해소하고 **최종 수렴(convergence)**을 보장하는가. 두 학파:
- **OT (Operational Transformation)**: 동시 연산을 서로의 문맥에 맞게 *변환*(Ellis & Gibbs, SIGMOD 1989; Google Docs/Jupiter). 구현이 미묘하고 변환 함수의 정확성 증명이 어렵기로 악명.
- **CRDT (Conflict-free Replicated Data Type)** (Shapiro, Preguiça, Baquero, Zawirski, *Conflict-free Replicated Data Types*, SSS 2011): 연산/상태를 **교환·결합·멱등**(commutative/associative/idempotent)하게 설계해, 어떤 순서로 병합해도 같은 결과(Strong Eventual Consistency, SEC)에 **수렴함을 수학적으로 보장**. 중앙 변환 서버 없이 P2P 병합 가능.

여기서 §0.2의 심장이 "편집 의도 전파" 형태로: "사용자 A가 한 변경을, B의 동시 변경과 모순 없이 모두에게 어떻게 반영하는가".

### 4.2 대표 기존 연구/시스템

- **Command 패턴**: GoF, *Design Patterns* (1994). undo/redo의 표준 구조.
- **OT 계보**: Ellis & Gibbs (SIGMOD 1989, GROVE); Nichols et al. **Jupiter** (UIST 1995, Google Docs의 조상); Sun & Ellis (CSCW 1998, OT 정확성 분석).
- **CRDT 계보**: Shapiro et al. (SSS 2011, 정의/SEC 증명); **Treedoc/Logoot/RGA**(시퀀스 CRDT); **Yjs / Automerge**(현대 구현, 텍스트·JSON·트리). Kleppmann & Beresford, *A Conflict-Free Replicated JSON Datatype* (IEEE TPDS 2017)은 **트리/JSON 협업 편집**(=씬 그래프와 가장 가까움)을 정면으로 다룸.
- **리플렉션 시스템**: Unreal UObject/UPROPERTY(코드젠 UHT), Unity SerializedProperty, RTTR(C++ 라이브러리), Qt MOC. 게임 에디터 메타데이터의 산업 사례.
- **Live link / 협업 게임 에디터**: Unreal Multi-User Editing(동시 편집), Roblox Team Create, Horizon Worlds. 대규모 씬 동시 편집의 산업 시도(학술 출판은 드묾 → §4.4의 기회).

### 4.3 자료구조/알고리즘(의사코드)

**Command 스택 + transient 그룹핑(드래그=1 undo):**

```text
stack undo_stack, redo_stack

interface Command:
    apply()           # 정방향
    revert()          # 역방향 (메멘토 복원 또는 역연산)
    merge_with(prev) -> bool   # 연속 같은 종류면 병합 (슬라이더 드래그)

function do(cmd):
    if undo_stack.top() and undo_stack.top().merge_with(cmd):
        pass                       # 드래그 중 수백 개 변경을 1개로 합침
    else:
        cmd.apply(); undo_stack.push(cmd)
    redo_stack.clear()             # 새 편집 시 redo 무효

function undo(): c=undo_stack.pop(); c.revert(); redo_stack.push(c)
function redo(): c=redo_stack.pop(); c.apply();  undo_stack.push(c)
```

**씬 그래프 협업 편집을 위한 트리 CRDT(개념, Kleppmann-style):**

```text
# 각 노드는 전역 고유 ID. 부모 포인터를 "마지막-writer-우선 + 인과시계"로 병합.
struct CrdtNode:
    id: GlobalId                    # (replica_id, counter) — 충돌 없는 생성
    parent: Register<GlobalId>      # LWW-Register w/ 인과 타임스탬프
    props: map<Key, LwwRegister>    # 각 속성도 LWW

function apply_remote_op(op):       # 원격 변경 수신
    node = nodes[op.target]
    node.props[op.key].merge(op.value, op.timestamp)   # 멱등·교환 병합
    # 부모 변경 시 사이클 방지: move 연산은 사이클 만들면 무시 규칙
    if op.kind == MOVE and would_create_cycle(op):
        record_conflict(op)         # 동시 move의 고전적 충돌
    else:
        node.parent.merge(op.new_parent, op.timestamp)

# 성질: 모든 replica가 같은 op 집합을 (임의 순서로) 받으면 같은 트리에 수렴 (SEC).
```

여기서 **씬 그래프 특유의 어려움**: 두 사용자가 같은 노드를 **서로 다른 부모로 동시에 이동**하면(concurrent move) 사이클·중복이 생길 수 있다 — 텍스트 CRDT엔 없는 트리 고유 충돌. 이것이 §4.4의 핵심.

### 4.4 박사급 novel 각도 (open problems)

1. **대규모 씬 그래프의 충돌 해소 + 결정론.** 텍스트 CRDT는 성숙했으나 **계층적 씬 그래프**(부모-자식, 트랜스폼 상속, 컴포넌트 배열)의 동시 이동·재부모화 충돌은 미해결. *기여 후보: 게임 씬 그래프 의미론(트랜스폼 상속·prefab override)을 보존하면서 SEC를 만족하는 트리 CRDT + 동시 move 충돌의 결정적·의도보존 해소.*
2. **CRDT 메타데이터 오버헤드.** CRDT는 인과시계·tombstone으로 메모리가 부푼다(수만 객체 씬에서 치명적). open problem: 씬 그래프 규모에서 메타데이터를 압축/GC하면서 SEC 유지.
3. **undo와 협업의 상호작용.** 협업 중 "내 변경만 undo"(selective/local undo)는 OT/CRDT에서 악명 높게 어렵다. 다중 사용자 undo의 일관된 의미론.
4. **에디터 데이터와 런타임/네트워크 결정론의 일관성.** 에디터에서 저장한 데이터가 클라/서버에서 **비트 동일하게** 해석되는가(06_FX §결정: visual-only는 로컬 시드, gameplay-affecting은 서버 event id+seed). §1 재현성·§3 핫 리로드·§4 협업이 만나는 지점.
5. **리플렉션 기반 범용 인프라.** 하나의 리플렉션 메타데이터로 인스펙터·직렬화·undo·정밀 무효화·스키마 마이그레이션을 **동시에** 구동하는 통합 설계 — 중복 없이. (PLDI/OOPSLA 색)

### 4.5 Thesis statement 예시

> "트랜스폼 상속과 prefab override 의미론을 보존하는 **씬 그래프 전용 트리 CRDT**는, 다중 편집자의 동시 재부모화 충돌을 의도보존적으로 결정적으로 해소하면서 Strong Eventual Consistency를 만족하고, 일반 트리 CRDT 대비 동시 move 충돌에서 사용자 의도 보존율을 X% 높인다."

baseline = OT 중앙 서버 / 범용 JSON CRDT(Automerge). metric = 수렴(SEC 위반 0), 의도 보존율(사용자 연구 + 합성 충돌), 메타데이터 메모리, 병합 지연.

### 4.6 평가 방법

- **수렴(correctness)**: 무작위 동시 연산 시퀀스를 임의 순서로 모든 replica에 적용 후 상태 동일성(SEC 위반 0건) — model-based / property testing.
- **충돌 의도 보존**: 동시 move/edit 시나리오에서 결과가 사용자 기대와 일치하는 비율(전문가 평가 + 합성 케이스 카탈로그).
- **성능/메모리**: 객체 수·편집자 수 대비 메타데이터 메모리, 병합 지연, undo/redo 응답 시간.
- **Undo correctness**: 임의 do/undo/redo 시퀀스 후 상태가 정의대로(재현 테스트).
- **Developer/User study**: 다중 사용자 편집 태스크 완료 시간·오류·만족도(IRB, 가이드 §10).
- **Threats**: 충돌 시나리오의 대표성, 합성 vs 실제 협업 로그, 씬 규모 일반화.

### 4.7 Winters 연결점

- **에디터 패널 군이 이미 설계됨**: `06_FX_GRAPH_SEQUENCER_EDITOR.md`가 FxGraphEditor·SequencerPanel·BossPatternEditor·HitboxTimelineEditor·WorldPartitionEditor·AssetBrowser를 명세. 각각 undo/redo·인스펙터·직렬화가 필요한 **리플렉션 기반 에디터의 구체 사례**.
- **JSON → binary 에디터 포맷 전략**(06_FX §저장 포맷: 초기 `.json`(diff 가능) → 중기 `.wfx/.wseq/.wboss/.whitbox` → 후기 `Content.winters`)이 §4.2 직렬화·§1 쿠킹과 직결. JSON 단계가 **협업 diff/merge 실험에 이상적**(텍스트라 CRDT/3-way merge 바로 적용 가능).
- **에디터=런타임 동일 프로세스**(06_FX §에디터 모드: `Scene_EldenEditor`의 play/simulate/edit; 06_EFX4 §결정: 1차는 Client ImGui 패널)이라 §3 핫 리로드와 §4 편집이 **같은 런타임에서 즉시 검증** — 측정·재현이 쉽다.
- **에디터 데이터의 네트워크 결정론 의식**(06_FX §FX Runtime 결정: visual-only=로컬 시드, gameplay-affecting=서버 event id+seed, raid telegraph=서버 action state)이 §4.4-4(에디터↔런타임↔네트워크 일관성)의 살아있는 제약. 10_Server·07_FX와 교차.
- **working tree 증거**: `WfxEffectToolPanel.cpp`, `WfxDocument.cpp`, `FxCuePlayer.cpp`, `LegacyFxAdapter.cpp` 등 FX 에디터/직렬화 코드가 활발히 수정 중 — testbed가 가설이 아니라 실재.

---

## 종합. 통합 학위논문 구조 예시

가이드 §4 "Three Papers Make a Thesis" 모델을 이 분야에 적용한다. 네 세부 주제를 다 하면 scope 폭주(가이드 §11)다. **하나의 thesis statement 아래 인접한 3개 문제**를 고른다. 이 분야는 §0.2의 심장("변경의 전파")이 모든 주제를 묶으므로, 그것을 한 문장으로 만들 수 있다.

**통합 명제(예):**
> "게임 콘텐츠 변경의 전파를 **소비 추적 리플렉션(consumption-traced reflection)**이라는 단일 하부구조 위에서 다루면, 빌드(증분 쿠킹)·런타임(핫 리로드)·협업(동시 편집)에서 변경의 영향 범위를 비트-정확·무손실·수렴 보장 하에 최소화할 수 있다."

```text
┌─ Ch 1. 서론
│   1.3 Thesis: "소비 추적 리플렉션으로 변경 전파를 빌드·런타임·협업에서
│               비트-정확/무손실/수렴 보장 하에 최소화한다."
│   1.4 기여: (a) 소비 추적 증분 쿠킹  (b) diff-driven 핫 리로드 마이그레이션
│            (c) 씬 그래프 트리 CRDT  + (공통) 통합 리플렉션 메타데이터
│
├─ Ch 2. 배경: 빌드 시스템(à la Carte 분류), CAS, DSU, OT/CRDT, 리플렉션
│         gap: 셋이 제각기 다른 메타데이터를 중복 사용; 변경 전파가 파일 단위로 과대
│
├─ Ch 3. [논문1, ICSE/EuroSys] 소비 추적 증분 쿠킹 (§1)
│         도구 self-reporting → 필드 단위 무효화 → over-rebuild X%↓, 비트 재현성
│
├─ Ch 4. [논문2, OOPSLA/EuroSys] diff-driven 상태 마이그레이션 핫 리로드 (§3)
│         리플렉션 스키마 diff → 마이그레이션 합성 → 무손실 정적 보장, 200ms 이내
│
├─ Ch 5. [논문3, CSCW/DISC] 씬 그래프 트리 CRDT (§4)
│         트랜스폼/prefab 의미 보존 + 동시 move 충돌 해소 → SEC, 의도 보존율 X%↑
│
├─ Ch 6. 종합 평가: Winters Elden 파이프라인을 단일 testbed로
│         - 빌드: 실제 편집 로그의 over-rebuild·재현성
│         - 스트리밍: World Partition 셀 전환 hitch (§2는 보조/응용 장)
│         - 핫 리로드: WFX 에디터 edit-to-see·무손실
│         - 협업: 다중 편집자 씬 그래프 수렴·사용자 연구
│         - ablation: 추적 끄기 / 마이그레이션 합성 끄기 / CRDT→OT
│
├─ Ch 7. 논의: 통합 리플렉션의 일반성과 한계, threats to validity
└─ Ch 8. 결론: "단일 변경-전파 하부구조" 명제가 세 영역에서 증명됨
```

> 대안 구성(스트리밍 중심): §2를 1차 기여로 올리면 "메모리 예산 하 끊김 없는 오픈월드 스트리밍"이 명제가 되고, Ch3=예측 prefetch(EuroSys/FAST), Ch4=DirectStorage 재설계(FAST/ATC), Ch5=번들 레이아웃 최적화(ATC)로 묶인다. Winters는 어느 구성이든 같은 testbed.

핵심(가이드 §11): **하나의 명제, 인접한 셋, 각 챕터는 동료심사 통과 가능한 논문 1편.** "에디터를 다 만들었다"가 아니라 "변경 전파라는 하나의 문제를 세 각도로 한 칸 밀었다".

---

## 참고문헌

> 확실한 것만 인용(가이드 §10). 학술 출판과 산업 자료(GDC/엔진 문서)를 구분 표기.

**빌드 시스템 · 증분 · 재현성**
- A. Mokhov, N. Mitchell, S. Peyton Jones. *Build Systems à la Carte*. ICFP 2018 / Journal of Functional Programming 30 (2020).
- S. Feldman. *Make — A Program for Maintaining Computer Programs*. Software: Practice and Experience, 1979.
- A. Heydon, R. Levin, T. Mann, Y. Yu. *Software Configuration Management with Vesta*. Springer, 2006.
- C. Lamb, S. Zacchiroli. *Reproducible Builds: Increasing the Integrity of Software Supply Chains*. IEEE Software, 2022.
- (산업) Google Bazel; Meta Buck2; Epic Games, *Derived Data Cache (DDC)* documentation.

**스트리밍 · I/O · 캐시**
- J. Axboe. *Efficient IO with io_uring*. Kernel documentation / LWN, 2019.
- S. Jiang, X. Zhang. *LIRS: An Efficient Low Inter-reference Recency Set Replacement Policy*. SIGMETRICS 2002.
- N. Megiddo, D. Modha. *ARC: A Self-Tuning, Low Overhead Replacement Cache*. USENIX FAST 2003.
- (산업) Microsoft, *DirectStorage* API; NVIDIA RTX IO; M. Cerny, *The Road to PS5* (GDC/공개 강연); Epic Games, *AsyncLoading2 / IoStore* documentation.

**핫 리로드 · 동적 업데이트 · 라이브 프로그래밍**
- J. Armstrong. *Making Reliable Distributed Systems in the Presence of Software Errors*. PhD thesis, KTH, 2003. (Erlang hot code swapping)
- I. Neamtiu, M. Hicks, G. Stoyle, M. Oriol. *Practical Dynamic Software Updating for C* (Ginseng). PLDI 2006.
- C. Hayden, E. Smith, M. Denchev, M. Hicks, J. Foster. *Kitsune: Efficient, General-purpose Dynamic Software Updating for C*. OOPSLA 2012.
- (산업) Epic Games, *Live Coding / Hot Reload* documentation.

**협업 편집 · 일관성 · CRDT/OT**
- C. Ellis, S. Gibbs. *Concurrency Control in Groupware Systems*. SIGMOD 1989.
- D. Nichols, P. Curtis, M. Dixon, J. Lamping. *High-Latency, Low-Bandwidth Windowing in the Jupiter Collaboration System*. UIST 1995.
- M. Shapiro, N. Preguiça, C. Baquero, M. Zawirski. *Conflict-free Replicated Data Types*. SSS 2011 (and INRIA RR-7687, 2011).
- M. Kleppmann, A. Beresford. *A Conflict-Free Replicated JSON Datatype*. IEEE TPDS 2017.

**패턴 · 리플렉션**
- E. Gamma, R. Helm, R. Johnson, J. Vlissides. *Design Patterns: Elements of Reusable Object-Oriented Software*. 1994. (Command 패턴)
- (산업) Unreal Engine *UObject/UPROPERTY* reflection; Unity *Serialization*; RTTR C++ reflection library.

**Winters 내부 testbed 문서(1차 자료)**
- `.md/EldenRing/02_ASSET_EXTRACTION_TO_WINTERS_BINARY_PIPELINE.md` — source→cooked 변환 체인, info 검증 게이트.
- `.md/EldenRing/07_ASSET_LOADER_AND_STREAMING_RUNTIME.md` — `CAssetStreamingSystem`, 우선순위, GPU 업로드 예산, hot reload, 번들.
- `.md/EldenRing/10_ASSET_PIPELINE_TOOLING.md` — `elden_pipeline.py`, Blender 자동 매핑, 재현성 워크로드.
- `.md/EldenRing/06_FX_GRAPH_SEQUENCER_EDITOR.md` — 에디터 패널, JSON→binary 포맷, 에디터=런타임.
- `.md/TODO/05-07/EffectTool/06_EFX4_EDITOR_PREVIEW_AND_HOT_RELOAD.md` — WFX manual/async 핫 리로드 로드맵.
- `.md/논문/08_WorldPartition_월드파티션.md` — 스트리밍 상위 시스템(§2 연계).
