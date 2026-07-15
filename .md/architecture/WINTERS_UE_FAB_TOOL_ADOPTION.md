# Winters × UE/Fab 툴 생태계 이식 설계

작성일: 2026-07-09. 언리얼 엔진의 툴 체계와 Fab 마켓플레이스 자산을 Winters Engine에 "어떻게 설계하고 적용할 것인가"를 고정하는 방향 문서. 상위 원칙은 `WINTERS_DESIGN_PHILOSOPHY.md`(P1~P4), 에디터 계층 방향은 `WINTERS_CODEBASE_COMPASS.md`의 Editor 섹션, 데이터 소유권은 `WINTERS_DATA_ARCHITECTURE.md`를 따른다.

## 0. 한 줄 결론

UE에서 가져올 것은 **개별 툴이 아니라 "자산이 검증된 런타임 계약으로만 게임에 들어간다"는 파이프라인 규율**이다. Fab 자산은 전부 "외부 원료"로 취급해 `Import 스테이징 → Winters cook(.w* 바이너리) → Validator 게이트 → 런타임 계약` 단일 경로로만 수용하고, UE 툴들은 기능 복제가 아니라 **Winters의 기존 소유권 경계 위에 등가물을 얹는 방식**으로 매핑한다.

## 1. UE 툴 생태계 분해 — 무엇이 강한가

UE가 강한 이유를 툴 단위가 아니라 계약 단위로 분해하면 Winters가 복제해야 할 본질이 보인다:

| UE 요소 | 본질 계약 | Winters 대응물 (현존/방향) |
|---|---|---|
| Content Browser | 모든 자산은 카탈로그에 등록되고 참조가 추적된다 | Editor shell의 Asset Catalog + manifest (`EldenAssetPipeline` manifests가 시초) |
| Import/Reimport (FBX/glTF/USD) | 원본→엔진 포맷 변환은 자동·재현 가능하다 | `WintersAssetConverter` (FBX→.wmesh/.wskel/.wanim/.wmat) + `convert_all_assets` |
| DataAsset/DataTable + 에디터 | 게임 수치는 코드가 아니라 데이터이고, 툴에서 편집한다 | GameplayDefinitionPack + generated pack (python cook) — 편집 UI는 미래 과제 |
| Niagara | VFX는 노드 그래프 자산이고 런타임은 컴파일된 plan을 실행 | WFX(.wfx) + `CFxGraphCompiler`/`CFxGraphValidator` + `WfxEffectToolPanel` — 이미 같은 구조 |
| Sequencer | 컷신/연출은 트랙 자산 | `.wseq` + `CSequenceAsset/CSequencePlayer` (Engine/Cinematic, 골격 존재) |
| World Partition | 월드는 스트리밍 셀 단위 데이터 | `CWorldPartitionSystem` + EldenRingEditor의 `CWorldCellDocument`(winters.world.cell.v1) |
| Control Rig/Retarget | 스켈레톤 차이를 데이터로 흡수 | .wskel 리타겟 테이블 (미구현 — Fab 애님팩 수용의 핵심 선결 과제) |
| Blueprint | 디자이너가 로직을 조립 | **채택 안 함** (§5) — generated pack + GameplayHookRegistry + Lua(UI 한정)로 대체 |
| Fab | 자산·툴의 외부 공급망 | 이 문서 §3의 ingestion 경로 |

## 2. 대원칙: 툴이 지켜야 하는 Winters 불변식

1. **툴은 런타임 계약의 생산자다.** 툴 산출물이 게임에 들어가는 유일한 경로는 cook된 `.wmesh/.wskel/.wanim/.wtex/.wmat/.wmap/.wfx/.wseq` + 정의 팩이다. 런타임 프레임 코드는 JSON/FBX/원본 포맷을 절대 읽지 않는다 (DataDriven 경계).
2. **Validator가 게이트다 (P1).** import→cook의 각 단계는 실패를 즉시·가시적으로 보고한다(무엇이, 어느 파일이, 왜). "조용히 기본값으로 폴백"은 금지 — 폴백은 카운터+로그와 함께만. 검증 실패 자산은 카탈로그에 '불합격' 상태로 남아야지, 사라지면 안 된다.
3. **툴은 gameplay truth를 만들 수 없다 (P2).** 에디터가 아무리 자라도 서버 권위 값(스탯/타이밍/판정)은 authoring 소스 → python/cook → ServerPrivate pack 경로로만 들어간다. 에디터의 라이브 튜닝(ImGui 슬라이더)은 로컬 실험 값이며, 저장은 authoring 소스로 역기록 후 재cook한다.
4. **normal F5 런타임을 우회하지 않는다.** 에디터 전용 기능이 게임 런타임 경로를 숨기거나 대체하면 안 된다 (compass Editor 규칙).
5. **에디터는 Engine SDK 소비자다.** 툴은 EldenRingEditor처럼 Engine 위의 별도 실행 파일/씬으로 서고, Engine 내부(Private)나 제품 클라이언트 코드에 손대지 않는다. ImGui는 툴/튜너 전용이고 런타임 HUD가 아니다 (UI 파이프라인 문서 규칙).

## 3. Fab 자산 클래스별 수용 파이프라인

Fab에서 실제로 사게 되는 것과 각각의 Winters 수용 경로. 공통 뼈대:

```text
Fab 다운로드 (원본: FBX/glTF/PNG/EXR/WAV/uasset)
  → Tools/AssetStaging/<vendor>/<pack>/   (원본 보존, 레포 외부 또는 LFS — 라이선스 메타 필수)
  → import manifest 작성 (무엇을, 어떤 설정으로, 어디로)
  → WintersAssetConverter / 전용 컨버터 cook → Client/Bin/Resource 이하 .w* 바이너리
  → Validator (포맷/본 수/텍스처 채널/네이밍) → Asset Catalog 등록
  → 런타임은 카탈로그/정의 팩 경유로만 참조
```

### 3-1. 스켈레탈 메시/캐릭터 (FBX/glTF)
- 현행 파이프라인 그대로: Assimp import → `.wmesh/.wskel/.wmat` cook. 이미 검증된 경로(LoL champion, Elden 자산).
- 주의(기존 gotcha 반영): 본 forward axis가 팩마다 다르다 → 캐릭터별 yaw offset은 `GetDefaultChampionVisualYawOffset` 계열 데이터로 흡수, 코드 산탄 +PI 금지. 본 수 256 제한(cbuffer) 검증을 Validator 단계로 승격(현재는 런타임 경고).
- uasset 전용 팩은 사지 않는다 — UE 에디터 없이 추출 불가. Fab에서 "Source files included(FBX)" 표기 팩만 채택.

### 3-2. 애니메이션 팩 (가장 가치 높고, 가장 선결 과제가 큰 클래스)
- Fab 애님팩(로코모션/전투/리액션)은 UE 표준 스켈레톤(UE4/UE5 Mannequin) 기준이 대부분 → **리타겟이 없으면 무용지물**.
- 설계: `.wskel`에 리타겟 프로필(본 매핑 테이블 + 리스트 포즈 보정)을 추가하고, AssetConverter에 `--retarget=<profile>` cook 단계를 넣는다. Control Rig 수준의 런타임 IK는 후순위; cook 시점 리타겟(오프라인 베이크)이 Winters 결정론/성능 원칙에 맞는다.
- 애님 키 네이밍 규약(champion animPrefix + key)은 import manifest에서 강제 — `[AnimKeyMiss]` 런타임 경고에 의존하지 말고 cook 시점에 잡는다 (P1: 실패를 가장 이른 지점으로).

### 3-3. 머티리얼/텍스처 (Quixel Megascans 포함)
- Megascans류는 PBR 텍스처 세트(albedo/normal/roughness/AO) → `.wtex` cook + `CMaterialPBR` 채널 매핑. UE 머티리얼 그래프(.uasset)는 수용 불가 — Material Resolver(에디터 계층)가 "채널 조합 → 우리 셰이더 파라미터" 매핑을 담당한다.
- 과거 사고 반영(gotcha 2026-04-26): render/*.png 스프라이트 캡처를 diffuse로 오인한 사고 → import manifest에 텍스처 역할(diffuse/mult/sprite)을 명시하고 Validator가 UV-alpha 정합을 검사.

### 3-4. VFX
- Niagara 자산은 직접 이식 불가(그래프+HLSL 커스텀). 접근: **참조용으로 사서 WFX로 재작성**. WFX 그래프/노드가 Niagara의 emitter-module 구조와 동형이므로, 자주 쓰는 모듈(burst, velocity over life, size curve, ribbon)을 WFX 노드로 보강하는 것이 Fab VFX "이식"의 실체다.
- 스프라이트 시트/플립북 텍스처는 그대로 .wtex로 수용 가능 — atlas manifest 경유.

### 3-5. 오디오
- WAV/OGG는 FMOD가 직접 소비 → 가장 저항 없는 클래스. 수용 규칙은 네이밍/카탈로그 등록과 라이선스 메타뿐.

### 3-6. 툴/플러그인 (에디터 확장, 코드 플러그인)
- UE 코드 플러그인은 이식 대상이 아니라 **사양서**다: 잘 팔리는 툴(스탯 관리, 대화 시스템, 인벤토리)은 "디자이너가 원하는 워크플로"의 증거이므로, 기능 목록을 참고해 Winters 에디터 패널/정의 팩 스키마를 설계한다.
- 범용 라이브러리형(포맷 파서, 압축 등)은 ThirdPartyLib 절차(`THIRDPARTY_INTEGRATION_GUIDE.md`)로만 수용.

### 3-7. 라이선스/출처 관리 (전 클래스 공통, 협업 필수)
- staging manifest에 필수 필드: 출처 URL, 라이선스 종류(Fab Standard/Personal), 구매 계정, 재배포 가능 여부. 원본 추출 자산(Elden 등)은 로컬 검증 전용이라는 기존 경계 유지 — 공개 빌드/포트폴리오는 대체 자산으로.

## 4. Winters 에디터 툴 아키텍처 (UE 개념의 착지 지점)

compass의 Editor shell 6계층에 UE 등가물을 매핑한 실행 설계:

```text
Editor shell (EldenRingEditor 스캐폴드 확장)
├── Content Browser  ← 카탈로그 = cook manifest의 집계. 파일시스템 스캔이 아니라 manifest가 진실.
│                       썸네일/검색/참조 추적. 불합격 자산도 상태와 사유가 보인다 (P1).
├── Importer/Converter/Validator ← AssetConverter를 라이브러리화해 에디터에서 호출.
│                       실패는 스테이지+파일+원인 로그 (RHITextureLoader 패턴 준용).
├── Material Resolver ← 텍스처 세트 → CMaterialPBR 파라미터 매핑 UI. 결과는 .wmat.
├── WFX Graph        ← 현 WfxEffectToolPanel 승격. Validator/Compiler 이미 존재. Save는 .wfx.
├── Sequencer        ← .wseq 트랙 편집. CSequencePlayer 골격 위. 이벤트 싱크는 FX cue 이름 재사용.
└── World Partition  ← CWorldCellDocument(JSON authoring) → .wmap cook. 셀 단위 스트리밍 검증.
```

원칙 적용 세부:
- **트랜잭션/언두**: EldenRingEditor `CEditorTransaction` 패턴을 전 패널 공통 기반으로 (P2: 편집 실패가 문서를 오염시키지 않도록 문서 단위 트랜잭션).
- **데이터 에디터** (UE DataTable 등가): 정의 팩의 authoring 소스(JSON/py 테이블)를 편집하는 패널. 저장 = authoring 소스에 기록 → cook 실행 → build hash 갱신. 런타임 pack을 직접 편집하는 UI는 만들지 않는다.
- **라이브 튜닝과 박제의 분리**: ImGui 튜너(EffectTuner/ChampionTuner/MapTuner 계열)는 실험 전용. "Save Preset"은 authoring 소스로 역기록하는 명시적 버튼이며, 게임 재시작 후에도 살아 있는 값은 오직 cook을 거친 값뿐이다.
- **에디터 빌드 편입**: EldenRingEditor가 sln에 없어 썩는 문제(의존성 지도 §1)를 먼저 해소해야 툴 투자가 안전하다 — vcxproj 추가 또는 CI에서 CMake 타깃 강제.

## 5. 채택하지 않는 것과 그 이유

- **Blueprint 런타임 VM**: 서버 권위 30Hz 결정론 tick에 인터프리터 로직을 넣으면 SimLab 해시 검증 체계가 무너진다. 대체: 수치는 generated pack, 분기 로직은 `CGameplayHookRegistry`(champion×variant 함수 테이블), UI 연출만 Lua.
- **UObject/GC/리플렉션 전면 도입**: Winters ECS의 trivially-copyable 컴포넌트 + 스냅샷 복제 모델과 충돌. 리플렉션이 필요한 곳(에디터 프로퍼티 그리드)은 정의 팩 스키마에서 필드 메타를 생성하는 코드젠으로 한정.
- **uasset 파이프라인 호환**: 유지비가 이득을 압도. Fab 구매 기준을 "소스 포맷 포함"으로 고정하는 쪽이 싸다.

## 6. 도입 순서 (기존 로드맵과의 접합)

1. **지금 즉시 가치**: Fab/Megascans 텍스처·오디오 수용 (파이프라인 이미 존재, staging+manifest 규약만 추가) / 애님팩은 리타겟 프로필 설계 완료 전까지 구매 보류.
2. **단기**: AssetConverter 라이브러리화 + Validator 게이트 승격(본 수/텍스처 역할/애님 키), Content Browser MVP(=manifest 뷰어), EldenRingEditor 빌드 편입.
3. **중기**: 데이터 에디터 패널(정의 팩 authoring 편집→cook), .wskel 리타겟 cook, WFX 노드 확충(Niagara 모듈 등가).
4. **장기**: Sequencer/World Partition 툴 완성 — Elden 수직 슬라이스 요구와 동기화 (`.md/plan/EldenRingEditor/` 세션 문서들).

각 단계의 완료 게이트: 빌드 PASS + 해당 자산 클래스의 실제 Fab(또는 등가) 팩 1개를 끝까지 통과시켜 F5 런타임에서 확인 + Validator가 의도적 불량 입력을 잡는 네거티브 테스트 1개.
