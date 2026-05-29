# Winters Engine — AAA 엔진 리버스 연구 인덱스

> **목표**: 상용 AAA 엔진(REDengine, Unreal, RAGE, Dantelion/FromSoft, Decima, id Tech, ND ICE, Source 2 등)의
> 아키텍처를 분석해 **Winters Engine 에 이식 가치가 있는 패턴**을 선별하고 적용한다.
> **범위**: 학습 목적 · 본인 소유 빌드 한정. 재배포 · 상용 게임 공격 용도 금지.
> **연계**: [Red Team 문서](../plan/security/06_RedTeam_SelfAttack_Plan.md) 의 권한/경계 원칙과 동일.
> **작성**: 2026-04-19

---

## 접근 계층

| 계층 | 자료 | 얻는 것 | Winters 활용 |
|---|---|---|---|
| **A** — 정식 공개 | Unreal(EULA), Godot, O3DE, Bevy, id Tech 4, GDC talks | 구현 전체, 설계 의도 | 1차 교본 |
| **B** — 커뮤니티 RE SDK | RED4ext.SDK, UE4SS, FiveM, DSMapStudio, SoulsModding | 구조체 오프셋, vtable, RTTI 덤프 | 패턴 확인 |
| **C** — 바이너리 분석 | IDA Free, Ghidra, x64dbg + 본인 소유 게임 exe | 실제 알고리즘, 인라인 최적화 | 특정 질문 답 |

---

## 연구 대상 엔진

### 1. REDengine 4 (CDPR — Cyberpunk 2077, Witcher 3 NG)
- **공개 수준**: B + C. CDPR 공식 소스 비공개.
- **핵심 자원**:
  - [RED4ext.SDK](https://github.com/WopsS/RED4ext.SDK) — 구조체 오프셋 + 함수 AOB 해시
  - [RED4ext Runtime](https://github.com/WopsS/RED4ext) — 플러그인 로더
  - [RedLib](https://github.com/jackhumbert/RedLib) — C++ 헬퍼
  - [CyberCAT](https://github.com/WolvenKit/CyberCAT) — 세이브 파일 파서
  - [WolvenKit](https://github.com/WolvenKit/WolvenKit) — 에셋 언팩
- **주요 서브시스템**:
  - `RTTI` — 런타임 타입 정보. 모든 게임 오브젝트가 CClass 참조
  - `CName` — 64-bit FNV1a64 해시 문자열 (CNamePool)
  - `Handle<T>` / `WeakHandle<T>` — intrusive refcount
  - `TweakDB` — 밸런스 DB (무기 수치, 스탯). 런타임 조회
  - `REDscript` — 커스텀 스크립트 언어, 바이트코드
  - `IScriptable` — 스크립트에서 접근 가능한 C++ 객체 기반
  - `JobQueue` — 병렬 작업
  - `Memory/` — 커스텀 얼로케이터
- **Winters 이식 후보**: CName, Handle, RTTI, TweakDB ← **Top 우선순위**

### 2. Unreal Engine 5 (Epic)
- **공개 수준**: A (EULA 동의 후 전체 소스 접근: https://github.com/EpicGames/UnrealEngine)
- **추가 자원**:
  - [UE4SS](https://github.com/UE4SS-RE/RE-UE4SS) — 서드파티 게임 후킹
  - [UnrealCLR](https://github.com/nxrighthere/UnrealCLR) — .NET 브릿지
  - GDC Vault: Billy Khan, Brian Karis (Nanite), Daniel Wright (Lumen) 발표
- **주요 서브시스템**:
  - `UObject` + `UCLASS / UPROPERTY / UFUNCTION` 매크로 → UHT (Unreal Header Tool) 가 reflection 보일러플레이트 생성
  - `FName` — 해시 문자열 (UE 의 CName 해당)
  - `TSharedPtr / TWeakPtr / TSharedRef` — non-intrusive + intrusive UObject GC
  - `TaskGraph` — DAG 기반 병렬 작업
  - Actor / Component 상속 계층 + `AActor::Tick`
  - `World Partition` — streaming tile
  - `Enhanced Input` — 선언적 입력 매핑
  - `Gameplay Ability System (GAS)` — 네트워크 복제 가능한 스킬 시스템 ★ LoL 스킬에 직접 참고 가치
  - Blueprint VM — C++ 함수를 스크립트화
- **Winters 이식 후보**: FName 스타일 해시, UHT 스타일 생성 파이프라인, GAS 아키텍처 연구

### 3. RAGE (Rockstar — GTA5, RDR2)
- **공개 수준**: B + C. 공식 소스 비공개.
- **핵심 자원**:
  - [FiveM / CitizenFX](https://github.com/citizenfx/fivem) — GTA5 멀티플레이 클라이언트 (MIT/GPL 혼합)
  - [ScriptHookV](http://www.dev-c.com/gtav/scripthookv/) — 프로프리어타리 로더 (기술 참고만)
  - dev-c.com — 클래스/함수 오프셋 문서
  - [NativeDB](https://nativedb.dotindustries.dev/) — 6000+ native 함수 DB
- **주요 서브시스템**:
  - `atHashValue` — 해시 문자열
  - `atArray<T>` / `atMap` — 커스텀 컨테이너
  - `fwEntity / fwArchetype` — 인스턴스 + 템플릿 분리
  - `fwScene / fwGameInterface` — 월드 매니저
  - `rage::scrProgram` — YSC 바이트코드 가상머신 (미션 로직)
  - `RSC7` resource format — 비동기 스트리밍용 페이지 압축
  - Pool allocator (`rage::atPoolBase`) — 풀 기반 오브젝트 관리
  - `CPedFactory / CVehicleFactory / CObjectFactory` 싱글턴 팩토리
- **Winters 이식 후보**: atHashValue 패턴, fwEntity+fwArchetype 분리, Pool allocator

### 4. Dantelion 2 (FromSoft — ER, DS3, Sekiro, AC6)
- **공개 수준**: B + C. 엔진 문서 거의 없음, 모드 커뮤니티가 대부분 RE.
- **핵심 자원**:
  - [SoulsModding Wiki](http://soulsmodding.wikidot.com/)
  - [DSMapStudio](https://github.com/soulsmods/DSMapStudio) — 맵/파라미터 에디터
  - [Yabber](https://github.com/JKAnderson/Yabber) — 포맷 언팩
  - [SoulsFormats](https://github.com/JKAnderson/SoulsFormats) — 포맷 라이브러리
  - [FLVER2](https://github.com/soulsmods/FLVER2) — 메시 포맷
- **주요 서브시스템**:
  - `MSB` (Map Studio Binary) — 지형 + 엔티티 + 스크립트 트리거 + 라이트 통합 포맷
  - `PARAM` — 테이블 기반 게임 밸런스 DB (WeaponParam, MagicParam, NpcParam 등 100+ 테이블)
  - `FMG` — 해시 키 기반 문자열 테이블
  - `FLVER` — 스키닝 메시 + 애니메이션
  - `HKS` (Havok Script) — Lua 5.0 기반 AI
  - `ESD` — 상태머신 정의 (이벤트 → 상태 전이)
  - `EMEVD` — 이벤트 스크립트 (퀘스트/트리거)
  - `ChrAsm` — 캐릭터 장비 조합 데이터
  - Havok Physics 통합
  - 글로벌 매니저 싱글턴: `WorldChrMan`, `MapItemMan`, `GameMan`, `SoloParamRepository`
- **Winters 이식 후보**: PARAM 테이블 DB, MSB 통합 맵 포맷, FMG 스타일 문자열 DB, HKS Lua AI 브릿지

### 5. Decima (Guerrilla — Horizon, Death Stranding)
- **공개 수준**: A (GDC talks 풍부) + B 일부
- **핵심 자원**:
  - GDC Vault: Hermen Hulst, Michiel van der Leeuw, Tim Verweij 발표
  - ["Decima Engine: Advances in Lighting and AA"](https://www.guerrilla-games.com/read/decima-engine) 공식 PDF
  - Horizon GDC: terrain streaming, world gen
  - Death Stranding GDC: 절차적 식생, topography-based physics
- **주요 서브시스템**:
  - 절차적 월드 생성 (Horizon ML — Machine Learning assisted)
  - GPU 중심 컬링
  - Streaming tile 기반 거대 월드
  - 데이터드리븐 컴포넌트
- **Winters 이식 후보**: GPU 컬링 패턴 (Phase 3 GPU-Driven), streaming tile (엘든링 모작 시)

### 6. id Tech 7 (id Software — Doom Eternal, The Dark Ages)
- **공개 수준**: A (GDC talks) + B 부분 툴
- **핵심 자원**:
  - GDC: Billy Khan, Axel Gneiting ("Mega-texture Beyond" etc)
  - id Tech 4 는 GPL (Doom 3 BFG 포함) — 베이스 구조 학습 가능
- **주요 서브시스템**:
  - Vulkan + DX12 듀얼 추상화
  - 정적 지오메트리 + 동적 라이팅
  - 60fps 고정 철학
  - 액터 + 스크립팅
- **Winters 이식 후보**: DX12 전환 시 RHI 추상 참고

### 7. Naughty Dog ICE (ND — TLoU, Uncharted)
- **공개 수준**: A (GDC talks 다수) + 일부 RE
- **핵심 자원**:
  - Christian Gyrling ["Parallelizing the Naughty Dog Engine Using Fibers"](https://www.gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine) ★ 필수
  - Andrew Maximov: data-oriented design
  - [GNM 문서 — 리크](https://github.com/gnmetc) 일부
- **주요 서브시스템**:
  - **Fiber Job System** (Phase 1b 의 직접 모델)
  - Data-Oriented Design 엄수 — SoA 레이아웃
  - GNM (PS4 커스텀 렌더 API)
- **Winters 이식 후보**: **Phase 1b JobSystem 의 직접 참조**

### 8. Source 2 (Valve — CS2, Dota 2, HL:Alyx)
- **공개 수준**: B (VPK 툴, 커뮤니티 분석)
- **핵심 자원**:
  - [Source 2 Viewer](https://github.com/ValveResourceFormat/ValveResourceFormat)
  - Valve 공식 SDK (Dota 2 Workshop Tools)
- **주요 서브시스템**:
  - ECS 기반 Entity/Component
  - Live reload (에디터 → 게임 핫리로드)
  - VPK 리소스 패킹
  - FGD 엔티티 정의
- **Winters 이식 후보**: 에디터 ↔ 게임 핫리로드 (Phase 2 이후)

### 9. Tiger (Bungie — Destiny 1/2)
- **공개 수준**: A (GDC talks 다수)
- **핵심 자원**:
  - Luis Anton-Canalis GDC talks (activity/world streaming)
  - ["Destiny's Multithreaded Rendering Architecture"](https://www.gdcvault.com/play/1022064/) (Natalya Tatarchuk)
  - ["Destiny Shader Pipeline"](https://www.gdcvault.com/play/1026812/)
- **주요 서브시스템**:
  - Activity 기반 월드 인스턴싱
  - 다중 스레드 렌더링
  - 네트워크 샤드 (인스턴스 풀)
- **Winters 이식 후보**: Phase 4 (네트워크/IOCP) 설계 참고

### 10. "Blackspace" — **확인 필요**
- 용어 불명확. 혹시:
  - Bungie 구 엔진 "Blam!" ?
  - Blackbird Interactive (Hardspace: Shipbreaker) 엔진 ?
  - PlatinumGames 내부 엔진 ?
  - 다른 의미 ?
- 사용자 확인 후 섹션 작성.

---

## Winters 이식 패턴 우선순위 (재요약)

| 우선도 | 패턴 | 원조 | 적용 지점 | 선행 조건 |
|---|---|---|---|---|
| ★★★★★ | CName (64-bit hash string) | REDengine / UE / RAGE | Timer tag, Event key, Asset name | 없음 — 즉시 가능 |
| ★★★★★ | Handle\<T\> intrusive refcount | REDengine / UE / RAGE | 엔티티 참조, 스킬 타겟 | 없음 |
| ★★★★☆ | RTTI / Reflection | REDengine / UE / Godot | ECS Component 자동 편집/직렬화 | CName 선행 |
| ★★★★☆ | TweakDB / PARAM | REDengine / FromSoft | LoL 챔피언 스탯 DB | RTTI 선행 권장 |
| ★★★★☆ | Fiber JobSystem | ND | Phase 1b 이미 계획 | — |
| ★★★☆☆ | Pool Allocator | 전체 | JobSystem task, 프레임 scope | Phase 1b |
| ★★★☆☆ | MSB 통합 맵 포맷 | FromSoft | Stage.dat 차기 버전 | Stage.dat 정착 후 |
| ★★★☆☆ | Scripting Bridge | UE / REDengine / FromSoft | Lua 5.4 (이미 계획) | RTTI 선행 |
| ★★☆☆☆ | Generated Header (UHT-style) | UE / REDengine | .wcomp → .h 자동 생성 | RTTI 정착 후 |
| ★☆☆☆☆ | Unified Base Object | UE / RAGE | **비권장 — ECS와 충돌** | 도입 안 함 |

---

## 연구 로드맵 (4 Phase)

### Phase R-1 — Layer A 탐독 (1~2주)
- Unreal Engine 소스 (Core/Public 위주 — FName/TSharedPtr/TaskGraph)
- Godot ClassDB (reflection 의 단순 구현)
- Christian Gyrling Fiber 논문 정독
- 산출물: [R-1_Reading_Notes.md](R-1_Reading_Notes.md) — 각 엔진에서 배운 요약

### Phase R-2 — Layer B SDK 분석 (1주)
- RED4ext.SDK 주요 서브시스템 5개 헤더 정독 (RTTI, CName, Handle, TweakDB, JobQueue)
- DSMapStudio 의 PARAM / MSB 리더 코드 분석
- UE4SS 의 후킹 인프라 분석 (Winters 안티치트 Phase 에도 유용)
- 산출물: 각 엔진별 `EngineName_Analysis.md`

### Phase R-3 — Winters 첫 이식 (2~3주)
- **CName 시스템**을 `Engine/Public/Core/CName.h` 로 구현 (FNV1a64 + 해시 충돌 테이블)
- Timer_Manager / Event / Asset 에서 `wstring_t` 키 → `CName` 로 점진 교체
- 측정: 문자열 비교 빈도 높은 코드 경로의 프로파일 전후 비교
- 산출물: `Engine/Public/Core/CName.h/.cpp` + [R-3_CName_Adoption.md](R-3_CName_Adoption.md)

### Phase R-4 — Layer C 학습 (필요 시)
- 본인 소유 게임에서 특정 패턴 확인용 (예: FromSoft PARAM 레이아웃 검증)
- Ghidra + 본인 소유 DS3/ER/GTA5 로 제한
- 재배포/공격 도구 생성 금지
- 산출물: 특정 질문에 대한 답만 기록, 바이너리 자체는 커밋 안 함

---

## 서브 문서 플랜 (생성 예정)

- `01_REDengine_Analysis.md` — CName/Handle/RTTI/TweakDB 심층
- `02_Unreal_Reflection_UHT.md` — UCLASS 매크로가 어떻게 UHT로 전개되는가
- `03_Fiber_JobSystem_NaughtyDog.md` — Phase 1b 직접 참조 문서
- `04_RAGE_Entity_Factory.md` — fwEntity + Pool
- `05_FromSoft_PARAM_MSB.md` — PARAM 테이블 스키마 + MSB 구조
- `06_Winters_Pattern_Adoption_Plan.md` — 각 패턴을 Winters 에 언제/어떻게 넣을지 타임라인

---

## 원칙 (Red Team 문서와 동일)

- 본인 소유 빌드 / 합법적으로 획득한 게임 한정
- 상용 게임 실시간 서버 공격·치트 배포 금지
- RE 분석 결과는 **패턴 학습** 목적 — 남의 IP 를 Winters 에 그대로 복사 금지 (구조체 필드명 표절 금지, 알고리즘 재구현 OK)
- 발견 내용 저장소 외부 반출 금지 (본 `.md/research/` 는 private 유지)
