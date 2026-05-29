# Ch16. Cross-discipline Collaboration Topology

> 기획자 / 디자이너 / 클라 / 엔진 / 서버 / 백엔드가 부딪히지 않고 같이 일하는 구조.
> 본 챕터는 코드보다 **프로세스 + SSoT + 의존성 방향**을 다룬다.
> 레퍼런스: UE5의 module rule, build.cs dependency, Editor/Runtime/Developer 디렉토리 분리.

---

## 1. 기초 원리 — 게임 개발이 망하는 가장 흔한 이유

기술이 아니다. **공동 작업 인터페이스 불일치**다.

흔한 죽음:
- 기획자가 엑셀에서 챔프 스탯을 바꿨는데 클라엔 안 들어오고 서버엔 들어온다
- 아티스트가 메시 이름 바꿨더니 50군데서 reference 깨짐, 누구도 모름
- 클라가 visual 위치 직접 계산해서 멀티 동기화 안 됨
- 서버 개발자가 Editor를 모르고, Editor 개발자가 서버를 모름
- 빌드 환경이 사람마다 달라서 "내 컴에선 됐는데"

**UE5의 답** (Winters도 같은 방향):
1. **명시적 의존성 그래프** (모듈/디렉토리/build.cs)
2. **데이터의 SSoT** (어디서 진실인지 1곳)
3. **각 디스플린의 entry point** (다른 영역 코드 안 봐도 일 가능)
4. **빌드/Cook이 위반을 강제 차단**

---

## 2. UE5의 디렉토리 분리 = 협업 규칙

`UnrealEngine/Engine/Source/`:

```text
Runtime/      게임 실행 시 필요한 코드
              Editor 모름. PIE / dedicated server / shipped game이 동일하게 쓰는 영역.

Editor/       Editor에서만. 빌드 target에 따라 strip.
              Runtime을 사용하나, Runtime은 Editor를 모름.

Developer/    개발 시 도구 (Profiler, debug viz). shipping 빌드에서 제외 가능.

Programs/     별도 exe (UnrealBuildTool, UnrealHeaderTool, UnrealInsights 등).

ThirdParty/   외부 라이브러리.
```

**의존성 방향**:
```text
Programs   →  (어떤 것도 안 의존, 자체 exe)
Editor     →  Runtime, Developer 의존
Developer  →  Runtime 의존
Runtime    →  Runtime 안에서만 의존 (Editor 절대 모름)
ThirdParty →  의존 없음
```

`Build.cs`가 이 규칙을 강제. Runtime 모듈이 `Editor` 모듈 include 시도하면 **빌드 깨짐**.

Server target 빌드 시:
- Runtime + Programs만
- Editor / Developer / Renderer / Audio / UI strip
- 같은 코드로 dedicated server binary 생산

---

## 3. Winters에 매핑

### 3.1 현재 Winters 디렉토리

```text
Engine/Public, Engine/Private      Runtime 등가
Engine/Public/Editor               Editor 일부 (in-game, 별도 stripping 미정)
Server/                            Runtime + server-only
Client/                            Runtime + client-only (UI/Render/Input)
Shared/                            Runtime 공용 (Server + Client + GameSim)
Services/                          Go backend (게임과 완전 분리)
Tools/                             Programs 등가 (AssetConverter, DX12SmokeHost)
Engine/External, Engine/ThirdPartyLib  ThirdParty
```

좋은 분리. 부족한 것:
- Editor가 in-game ImGui 형태로 Runtime 안에 박혀 있음 (Ch12 Stage B에서 분리해야)
- Developer (Profiler) 별도 영역 없음 (Ch13에서 추가)

### 3.2 Winters 의존성 규칙 (제안)

```text
Tools/         →  (none — 자체 exe)
Editor/        →  Engine/Public, Shared/    (Editor가 Runtime을 알지만 역방향 금지)
Developer/     →  Engine/Public, Shared/    (Profiler 등)
Engine/        →  Engine/ 안에서만, ThirdParty 가능
Client/        →  Engine/Public, Shared/, ThirdParty
Server/        →  Engine/Public(일부), Shared/, ThirdParty  (UI/Render 모듈 exclude)
Shared/        →  Engine/Public 일부 (수학/타입), ThirdParty
Services/      →  완전 분리. 게임 코드 무관. Schemas 통해서만 통신.
```

이걸 `Tools/WintersBuildTool/Module.Build.cs`(Ch13)에서 강제.

### 3.3 위반 사례 (현재 박제됨)

CLAUDE.md 2026-05-12:
> Bot AI는 `Transform`, `Health`, `SkillState`, `MoveTarget` gameplay 결과를 직접 수정하지 않는다.

→ `Shared/GameSim/Systems/BotLaneAISystem.cpp`가 Component를 직접 mutate하면 안 됨. `m_pendingExecCommands` 채워 `CDefaultCommandExecutor`로 흘려야.

이게 정확히 **의존성 방향 위반**. AI가 GameSim의 truth를 직접 만지면, "AI가 게임을 정의"하게 됨. 반대로 가야 함.

---

## 4. 4가지 SSoT (Single Source of Truth) — 본 brief 핵심

| 도메인 | SSoT 위치 | Owner | Consumer |
|--------|----------|-------|----------|
| Gameplay state | `Shared/GameSim/` ECS World | Server / GameSim 개발자 | Client(visual), AI(producer) |
| 데이터 자산 (스탯/스킬/아이템) | `Content/` cook + `Tools/DataEditor` (Ch15) | 기획자 | Client/Server/Engine |
| Animation cue / FX cue | actionSeq → notify track (Ch4 + Ch6 + Ch8) | 애니메이터 + 클라 | Server cue 송신, 클라 재생 |
| Asset binary | `Content/.wmesh` `.wanim` `.wtex` (Ch15) | 아티스트 | AssetCache → 런타임 |

이 4개가 흔들리면 협업이 깨진다. 코드에서 가장 자주 점검해야 할 invariant.

---

## 5. 디스플린별 entry point

### 5.1 기획자 (Game Designer)

```text
in:
  Tools/DataEditor (web 또는 desktop)
  .xlsx / DataTable / CurveTable (Ch15)
  Tools/AbilityEditor (Blueprint Lite — Ch8/Ch12 의존)

out:
  cook → Client/Bin/Resource/Data/
  hot patch → Services/internal/liveops (Ch14)

검증:
  in-editor PIE (Play-In-Editor) — Ch12 Stage B 필수
  test server에서 즉시 확인

도구 의존:
  Ch15 DataTable / DataAsset
  Ch12 Editor Stage B
  Ch8 AbilityEditor
  Ch14 LiveOps push

협업 fault line:
  데이터 변경이 게임 로직과 충돌하지 않게 → Ch15 Data Validation (cook 전 sanity)
  하드코드 금지 — 모든 magic number는 데이터로
```

### 5.2 레벨 디자이너 / 아티스트

```text
in:
  Tools/WintersEditor (World/Sequencer/Effect)
  Blender / Maya → .wmesh / .wanim (Tools/AssetConverter)
  Photoshop / Substance → .png / .wtex
  Tools/EffectTool (Phase G memory 박제)

out:
  AssetRegistry 자동 색인 → Content/
  Source control (Perforce / Git LFS)

검증:
  AssetValidator (Ch13)
  RenderDoc (GPU profile)
  in-game preview

도구 의존:
  Ch12 ContentBrowser / WorldEditor
  Ch3 WorldPartition (큰 월드)
  Ch11 Sequencer (컷씬)
  Ch13 AssetConverter / Validator

협업 fault line:
  Asset 이름 변경 → reference 깨짐 → Ch15 ContentRegistry로 자동 따라옴
  PR/CL 충돌 → External Actor 패턴 (Ch3) + Source Control LFS
```

### 5.3 클라 개발자

```text
in:
  Client/Private/Scene/
  Client/Private/GameObject/
  Client/Private/UI/
  Client/Private/Network/  (snapshot consume)

out:
  Snapshot/Event 소비 → 캐릭터 visual, animation cue 재생, fx, sound
  사용자 Input → GameCommand (Ch7 prediction)

금지:
  gameplay 결과 직접 수정 (HP / Cooldown / Position)
  서버 권위 영역 침범

도구 의존:
  Ch4 Animation
  Ch6 Audio
  Ch10 UI
  Ch11 Cinematic

협업 fault line:
  서버 snapshot이 클라 예측과 충돌 → Ch7 reconciliation 코드 점검
  visual cue 누락 → Ch8 GameplayCue tag 매핑 확인
```

### 5.4 엔진 개발자

```text
in:
  Engine/Public, Engine/Private

out:
  모든 챕터 capability 제공
  RHI / RenderGraph / Animation 등 plugin point

검증:
  SmokeHost (DX12 / Vulkan 등)
  Unit test
  Benchmark (frame budget)
  Profiler (Ch13)

도구 의존:
  Ch1 RHI
  Ch2 RenderGraph
  Ch13 BuildTool / HeaderTool

협업 fault line:
  Engine 변경이 Client/Server 깨뜨림 → 양쪽 build smoke 필수
  DLL boundary 깨짐 → unique_ptr 멤버 + dllexport 규칙 (memory 박제)
  ABI 변경 → reflection / serialization 호환성
```

### 5.5 서버 / GameSim 개발자

```text
in:
  Server/
  Shared/GameSim/
  Shared/Network/

out:
  gameplay truth (HP/MP/cooldown/damage)
  simulation tick (deterministic)
  AOI / replication priority
  snapshot 생성 + event broadcast

금지:
  클라용 visual / animation / sound 직접 호출
  Render / UI module 의존

도구 의존:
  Ch7 Networking (IOCP × Fiber memory 박제)
  Ch8 GAS (서버 권위 ability)
  Ch9 AI (서버 권위 AI)

협업 fault line:
  GameSim 변경이 클라 prediction과 불일치 → Ch7 reconciliation 깨짐
  AI가 GameSim 직접 mutate → CLAUDE.md 박제 위반
```

### 5.6 백엔드 개발자 (Go)

```text
in:
  Services/internal/* (Ch14)

out:
  Auth / Shop / Match / Profile / Inventory / Telemetry / LiveOps

검증:
  Postman, docker-compose
  k6 load test
  Prometheus + Grafana

도구 의존:
  Ch14 Services 전체
  Ch15 데이터 자산 (cook된 asset을 service가 호스팅)

협업 fault line:
  Schema 변경 → 클라/서버/services 동기화 필요 → FlatBuffers/Protobuf 1 SoT
  결제/인벤토리 사기 → antifraud 분리된 영역
```

### 5.7 QA / Live Ops

```text
in:
  Tools/Profiler (Ch13)
  Telemetry dashboard (Ch14)
  Replay 시스템 (Ch7 R0~R3)

out:
  버그 리포트 + replay 첨부
  A/B 테스트 설정 (LiveOps)
  점검 일정 / 패치노트

도구 의존:
  Ch7 Replay
  Ch13 Profiler
  Ch14 LiveOps CMS

협업 fault line:
  버그 재현 어려움 → replay 첨부로 결정적 재현
  사고 발생 시 통계 빠른 lookup → telemetry SQL alert
```

---

## 6. 인터페이스: 디스플린이 만나는 지점

### 6.1 기획자 ↔ 엔진/클라

```text
기획자: "이즈리얼 Q 데미지 50 → 60"
   ↓
DataTable 변경 → cook → 게임 즉시 변화
엔진/클라 코드 변경 0줄

기획자: "Q가 충돌 시 폭발하는 후속 효과 추가"
   ↓
새 GameplayEffect 자산 + 새 AbilityTask 자산
엔진/클라 코드 변경 0줄

기획자: "Q가 이동하면서 시전 가능하게"
   ↓
이건 데이터로 표현 안 됨 → AbilityEditor 노드 그래프에서 task 재배치
엔진 코드 변경 0줄 (단, AbilityTask 종류가 충분해야 함 — Ch8 task library)
```

### 6.2 클라 ↔ 서버

```text
클라: Input → GameCommand → Server
서버: Simulate → Snapshot/Event → Client
클라: Snapshot consume → Visual / Animation / Sound

[잘못된 패턴]
클라가 직접 "내 HP가 50이야" 결정 → 서버 모름
   → 멀티 desync, 치트 가능
   → Ch7 prediction + reconciliation 필수
```

### 6.3 엔진 ↔ 다른 모듈

```text
엔진은 game-agnostic. 챔프 이름을 모름.
게임 코드만이 "Ezreal"을 안다.

새 챔프 추가:
   - 엔진 변경 0 (가능해야 함)
   - 데이터 자산 추가
   - GAS / Animation registry 등록 코드 N줄
```

### 6.4 Services ↔ 게임 서버

```text
인증 흐름:
   클라 → services/auth → JWT token
   클라 → 게임서버 with token → 게임서버가 services/auth/verify
   게임서버는 services 신뢰만

매치 흐름:
   클라 → services/matchmaking → "Ready, server endpoint=X"
   클라 → endpoint X (게임 서버)
   게임 서버 ← services에서 player roster 받음

상점/인벤:
   결제는 services만. 게임 서버는 services에 "this player has skin Y?"만 query.
   게임 서버에 직접 결제 안 박음.
```

---

## 7. PR / 코드 리뷰 / CI 규칙

### 7.1 PR 단위

- 1 PR = 1 챕터의 1 stage 부분
- "Ezreal Q 추가" = 데이터만 PR, 코드 0 (이상적)
- "Animation StateMachine 도입" = Engine + Client PR (큰 변경)

### 7.2 자동 검증 (CI)

```text
on PR:
  ✓ Build Win64 Debug / Release
  ✓ Build Server Debug / Release
  ✓ Smoke: DX12SmokeHost 8초 생존
  ✓ Smoke: WintersServer ping-pong
  ✓ AssetValidator full content
  ✓ unit test (Engine / GameSim)
  ✓ Schema compatibility (.fbs 변경 시 backend 호환)
```

CLAUDE.md 박제 검증 명령들이 CI에 박힌다.

### 7.3 코드 리뷰 매트릭스

```text
변경 영역             owner approval 필요
Engine/Public/        엔진 개발자
Engine/Private/       엔진 개발자 (변경 작음) / 또는 그 영역 전문가
Shared/GameSim/       서버 + GameSim 개발자
Server/               서버 개발자
Client/               클라 개발자
Services/             백엔드 개발자
.md/architecture/     리더 + 영향받는 디스플린
Tools/                Tooling 담당
```

`CODEOWNERS` 파일로 자동 reviewer 할당.

---

## 8. 회의 / 의사소통

| 주기 | 내용 | 참가 |
|------|------|------|
| Daily | 어제/오늘/blocker | 자기 팀 |
| Weekly | 챕터별 진척, fault line | 전 디스플린 |
| Sprint (2주) | 다음 슬라이스 정의, retro | 전체 |
| Quarter | 챕터 roadmap 재조정 | 리더 + 시니어 |

핵심: **fault line은 daily가 아닌 weekly에 다룬다.** 같은 디스플린끼리는 빠른 cycle, 다른 디스플린 간 충돌은 느린 cycle로.

---

## 9. 문서화 규칙 (Winters AGENTS.md 박제와 합치)

- 각 챕터마다 `.md/architecture/CH_*.md` 또는 `.md/문서/*` 1개
- 진척은 `.md/TODO/<date>/` 디렉토리
- 큰 변경은 사전 plan handoff (코드 직접 수정 전)
- `IMPLEMENTATION_HANDOFF_OUTPUT_RULE.md` 형식 준수

Winters의 **handoff 형식** 자체가 협업 인터페이스. 이 brief의 16 챕터 .md가 그 예시.

---

## 10. 게임 규모별 인력 구성 (참고)

| 게임 | 인력 (대략) | 핵심 인력 분포 |
|------|-----|--------------|
| LoL (현재 Winters 목표) | 100~300 | 챔프 / VFX / Engine / Server / Live Ops 균형 |
| 로아 | 300~500 | Content 비중 큼 (콘텐츠 디자이너 다수) |
| 엘든링 | 200~300 | Director 중심, 작은 dev team + 큰 외주 |
| GTA6 | 1000~2000 | 모든 디스플린 다수, multi-region team |

핵심: **인력이 늘수록 협업 인터페이스가 더 명시적이어야** 한다. 100명까지는 구두로 OK. 500명+는 build.cs / CODEOWNERS / CI gate.

---

## 11. Winters 현재 협업 fault line 점검

memory + 박제 기반:

- **Bot AI 권한**: CLAUDE.md 2026-05-12 박제로 잡힘 → 유지
- **Phase B-11d v3.1 Ezreal pending**: 챔프 등록 시 Registry 3종 + hookId 4분할 + ChampionRegistry → Ch8 GAS의 정형화로 이동
- **Profiler thread_local race**: memory `project_session_2026_04_28_minion_combat.md` → Ch13 Profiler 도입 시 thread-safe 강제
- **MinionAI stuck**: memory `project_session_2026_04_28_minion_stuck.md` (Phase swap + path empty fallback) → Ch9 AI에서 정식 BT Task로 흡수
- **컨벤션 문서 누락 사고**: memory `feedback_convention_reading.md` (CLAUDE.md만 읽으면 Manager 재작성) → Ch16 문서 진입 순서 박제

이 사고들이 다시 안 일어나려면 **문서 + build rule + CI**가 동시에 필요. 사람의 메모리만으로는 시간 지나면 잊힘.

---

## 12. 마무리 — 16 챕터의 합

각 챕터는 단독으로 만들 수 없다. 의존 그래프:

```text
Ch13 Tooling (UBT/UHT/Reflection)
  ↓
Ch1 RHI  →  Ch2 RG  →  나머지 렌더
                       ↓
Ch15 Data ────→ Ch12 Editor ──→ 모든 디자이너 도구
                       ↓
Ch4 Anim  Ch6 Audio  Ch5 Phys  Ch8 GAS  Ch9 AI  Ch10 UI  Ch11 Cinematic
                       ↓
                  Ch3 WorldPartition (Open World 필요 시)
                       ↓
                  Ch7 Networking  ← Ch14 Services 연결 지점
                       ↓
                  Ch16 (이 문서) — 모두 묶음
```

Ch16은 **다른 챕터를 가능하게 하는 사회적 인프라**. 코드보다 문서/CI/owner/SSoT/dependency rule이 핵심.

본 16 챕터 brief가 그 인프라의 첫 박제다. 다음 단계는 각 챕터를 1년 단위 sub-plan으로 분해 (Year 1: Ch1/Ch4/Ch7/Ch8 일부, Year 2: ...). 그건 별도 handoff.

---

## 13. 다음 챕터로

본 16 챕터가 brief의 마지막. 위 챕터 중 진입 우선순위가 정해지면 그 챕터의 1년 분 sub-plan을 `.md/문서/Ch?_*_Year1.md`로 박제 가능. 필요 시 요청.
