# 17. 애니메이션 / 물리 / 오디오 — 면접 대비 세션

> 근거 문서: `.md/이력서/WINTERS_DOMAIN_HONEST_MAP_2026-06-26.md` "### 17." (working, 약점이되 통제됨)
> 코드 근거: `Engine/Private/Resource/{Animation,Animator,Skeleton}.cpp`, `Engine/Private/Physics3D/HitVolume.cpp`, `Engine/Private/Sound/Sound_Manager.cpp`
> 계획 근거: `.md/문서/04_Ch4_Animation.md`, `05_Ch5_Physics.md`, `06_Ch6_Audio.md`, `.md/plan/physics/00_PHYSICS_PLAN_INDEX.md`

---

## 0. 한 줄 본질 + 현재 상태

**한 줄 본질**: 캐릭터를 "움직이게(애니메이션) · 부딪히게(물리) · 들리게(오디오)" 만드는 세 가지 런타임 — 즉 정적 메시 덩어리를 시간축 위에서 살아 있는 게임 오브젝트로 바꾸는 계층이다.

**현재 성숙도(정직하게)**: 세 도메인 모두 *기반 수학/재생 경로는 working, 게임 통합·고도화는 미완*인 혼재 상태다.
- **애니메이션**: 스켈레탈 애니 **재생 런타임은 working** (본 계층 누적 + 키프레임 보간 + 매 프레임 평가, 26개 모듈에서 소비). 단 **단일 클립 재생만** — 블렌딩/StateMachine/IK는 **planned**.
- **물리**: **SAT 기반 충돌 *수학 라이브러리*는 working** (OBB/Sphere/AABB overlap + 프레임 윈도우). 단 강체/솔버/CCD는 **0줄**, 게임 전투 통합도 아직 아님(유일 소비처가 EldenRing 에디터 디버그 패널).
- **오디오**: **순수 2D FMOD 래퍼는 working** (채널 재생/볼륨/폴더 로드). 3D 공간화/DSP/submix는 **planned**.

> 면접 톤: "이 도메인은 제 약점이지만 *통제된 약점*입니다. 어디까지 됐고 어디부터 계획인지를 챕터 문서와 7단계 물리 계획으로 스스로 그어 뒀고, MOBA 우선순위에 맞춰 의도적으로 뒤로 미룬 부분을 구분해 설명드릴 수 있습니다."

---

## 1. 핵심 개념 (본질)

### 1.1 스켈레탈 애니메이션 — "왜 본 계층과 행렬 곱이 필요한가"

근본 문제: 캐릭터 메시는 수만 개의 정점이다. 이걸 프레임마다 직접 옮기면 데이터·연산이 폭발한다. 해결책은 **소수의 본(bone)에만 변환을 정의하고, 각 정점이 본에 가중치로 매달리게(skinning)** 하는 것이다. 정점 수천 개 대신 본 50~150개만 애니메이션하면 된다.

이를 위한 1차 원리 세 가지:

1. **본은 트리(계층) 구조다.** 손은 팔에, 팔은 어깨에, 어깨는 척추에 매달린다. 자식 본의 월드 변환 = `Local(자식) × Global(부모)`. 그래서 부모를 먼저 계산해야 자식을 계산할 수 있고, 본 배열을 **부모가 자식보다 먼저 오도록 정렬(topological order)** 해 두면 한 번의 선형 순회로 전체 계층을 누적할 수 있다.

2. **최종 스키닝 행렬 = Offset × Global × GlobalInverseRoot.**
   - `Offset(=inverse bind pose)`: 정점을 모델 공간에서 **본 로컬 공간으로** 끌어내린다. "이 정점이 바인드 포즈에서 이 본 기준으로 어디 있었나".
   - `Global`: 현재 애니메이션된 본의 월드 변환. 본 로컬 → 현재 월드.
   - `GlobalInverseRoot`: 루트 노드의 전역 변환을 상쇄(DCC 툴마다 루트에 박아 넣는 좌표계 보정 제거).
   - 이 곱셈 규약은 Assimp 표준이며, 한 곳이라도 순서가 틀리면 캐릭터가 폭발(스파게티)한다. **곱셈 *순서*를 정확히 말할 수 있느냐가 이 개념을 진짜 이해했는지의 리트머스다.**

3. **키프레임 보간 — 시간 → 포즈.** 애니메이터는 모든 프레임을 저장하지 않고 **키프레임(특정 시각의 본 변환)** 만 저장한다. 임의 시각 t의 포즈는 인접 두 키 사이를 보간한다.
   - **위치/스케일은 lerp**(선형 보간). 직선 경로면 충분.
   - **회전은 반드시 slerp**(구면 선형 보간). 쿼터니언을 단순 lerp하면 회전 속도가 불균일해지고 정규화가 깨진다. slerp는 단위 구면 위 최단 호를 따라 등속으로 돈다. **"회전을 왜 lerp 아닌 slerp로 하나"는 단골 질문이다.**

### 1.2 충돌 판정 — "SAT가 무엇이고 왜 존재하는가"

근본 문제: 두 볼록 도형이 겹쳤는가? 구-구는 중심거리 vs 반지름합으로 끝이지만, 회전된 박스(OBB)는 단순 좌표 비교가 안 통한다.

**분리축 정리(Separating Axis Theorem, SAT)**: 두 볼록 도형이 겹치지 *않는다면*, 둘을 한 직선(축)에 투영했을 때 투영 구간이 겹치지 않는 축이 **반드시 하나 이상 존재한다**. 역으로, 검사해야 할 후보 축 전부에서 투영이 겹치면 → 두 도형은 겹친다. 박스의 후보 축은 각 박스의 면 법선들(2D yaw-OBB면 각 박스의 X·Z축, 총 4개)이다.

왜 SAT인가: GJK/EPA보다 구현이 단순하고, **MOBA처럼 박스/구 같은 단순 프리미티브만 쓰는 경우 가장 가성비가 좋다**. 임의 볼록다면체까지 가면 GJK가 유리하지만, 격투형 히트박스는 박스/구로 충분하다.

**히트박스 프레임 윈도우 — 격투 게임 1차 원리**: 충돌 "수학"만으로는 전투가 안 된다. *언제* 그 충돌이 유효한지가 핵심이다. 한 공격 모션은 (a) **telegraph**(예고, 회피 가능 구간 시작) → (b) **active**(타격 판정 ON) → (c) 종료의 프레임 구간으로 나뉜다. 데미지는 active 윈도우 안에서 overlap이 났을 때만 발생한다. dodge 윈도우(telegraph~active 사이)는 "이 안에 회피 입력을 넣으면 피한다"는 무적/회피 프레임이다. 이게 소울/격겜 전투의 본질이다.

### 1.3 오디오 — "오디오는 신호 그래프다(이론) vs 채널 재생(현재)"

근본 모델(UE5/Wwise 수준): 오디오는 **소스 → DSP 노드 그래프 → 출력 디바이스**다. 각 소스(재생 중인 소리 1개 = 1 voice)가 submix 버스로 흘러가고, 버스마다 reverb/EQ/compressor 같은 DSP가 직렬로 걸리고, 3D 위치는 spatializer가 좌우 채널 + 감쇠(attenuation) + 도플러로 변환한다. voice 수에는 물리적 한계(64~256)가 있어 초과 시 priority/거리/age로 voice stealing을 한다.

**현재 내 구현의 위치**: 나는 이 그래프 모델의 **가장 바닥 — "소스를 디바이스에 그냥 재생"** 까지만 했다. FMOD `System`을 초기화하고 채널에 사운드를 얹어 볼륨을 거는 수준이다. submix/DSP/3D/voice stealing은 전부 FMOD가 *제공할 수 있지만 내가 안 켰다*. 이걸 정확히 구분해 말하는 게 정직성의 핵심이다.

---

## 2. 왜 이 선택인가 — 기술 스택 선택 이유 + Trade-off

### 2.1 애니메이션: 자체 구현 + 단일 클립 우선

| 선택지 | 장점 | 단점 | 내 결정 이유 |
|---|---|---|---|
| **자체 스켈레탈 런타임(택함)** | DirectXMath 직접, 본 계층/스키닝 행렬 규약을 내가 통제, RHI 스키닝과 직결 | AnimGraph/블렌딩 전부 직접 작성 비용 | 엔진 프로그래머 핵심 역량 증명 + 외부 미들웨어 없음이 프로젝트 정체성 |
| 단일 클립 재생만 우선(택함) | MOBA는 idle/run/attack 정도면 캐릭터가 "살아 보인다", 빠르게 end-to-end 검증 | castFrame 하드코딩, 이동중 공격 블렌딩 불가 | **150챔프 가면 무너지는 걸 알지만**, 지금은 재생 경로 검증이 우선. Ch4 문서에 한계를 선제 기록 |
| 애니 블렌딩/StateMachine 선구현 | 룩이 부드러움 | 재생 경로도 안 굳었는데 그래프부터 짜면 과설계 | Karpathy 가드레일(단순성 우선) — 단일 클립이 안정된 뒤 그래프로 리팩터가 순서 |

**근본 Trade-off**: 단일 클립 재생은 "castFrame 숫자를 챔프 `_Skills.cpp`에 박는" 확장 한계를 낳는다(LoLVisualDefinitions에 95회 등장). 이걸 montage notify 트랙으로 데이터화하는 게 Ch4의 핵심 부채 상환이다. **나는 이 부채를 숨기지 않고 챕터 문서에 명시**했다.

### 2.2 물리: 외부 엔진 미통합 + SAT 충돌 라이브러리 우선

| 선택지 | 장점 | 단점 | 내 결정 이유 |
|---|---|---|---|
| Jolt/PhysX 통합 | 검증된 강체/솔버/CCD 즉시 | LoL MOBA엔 **오버스펙**, 결정론 보장 까다로움, 학습 가치 0 | MOBA 핵심은 가벼운 스킬샷 판정. 강체 시뮬 불필요 |
| **SAT 충돌 *수학* 자체 구현(택함)** | 충돌 이론 직접 이해, NaN sanitize/프레임 윈도우까지 통제, 서버 결정론 친화 | 강체 역학/조인트 없음(라이브러리지 엔진 아님) | MOBA에 실제로 필요한 건 overlap query. 그것만 정확히 만듦 |
| 자체 풀 물리 엔진 | 100% 통제, 포트폴리오 임팩트 | 1~2 인년 비용 | 신입 1인 범위 초과. 7단계 계획으로 *로드맵화*하되 지금은 Stage 1만 |

**근본 Trade-off**: "물리 엔진을 만들었다"고 말하면 즉사한다(강체/솔버/CCD 0줄). 정확한 표현은 **"SAT 기반 충돌 판정 라이브러리 + 격투형 프레임 윈도우"**. MOBA 특성상 챔피언은 물리로 안 움직이고(서버 권위 그리드 이동), 히트박스는 "겹침 이벤트"만 필요하므로 trigger overlap 라이브러리면 충분하다는 게 `00_PHYSICS_PLAN_INDEX.md`의 명시적 판단이다.

### 2.3 오디오: FMOD 래퍼 + 2D 우선

| 선택지 | 장점 | 단점 | 내 결정 이유 |
|---|---|---|---|
| **FMOD 2D 래퍼(택함)** | BGM/효과음이 즉시 들림, 채널 분리, 폴더 자동 로드 | 3D/공간감/믹스 없음 | 탑다운 MOBA는 카메라가 고정 부감 — 3D 오디오 체감 이득이 낮음 |
| FMOD 3D 풀 통합 | 거리 감쇠/도플러 | 탑다운에선 ROI 낮음, listener 관리 추가 | 우선순위 후순위. Ch6 Stage 2로 계획 |
| Wwise | 디자이너 툴링 1급 | 라이선스 비쌈, 빌드 통합 복잡 | 라이브 서비스 진입 시 검토(Ch6 Stage 8) |

**근본 Trade-off**: 탑다운 MOBA의 카메라 특성상 3D 공간 오디오의 체감 가치가 낮다. 그래서 의도적으로 2D에 머물렀다. 단 "3D 공간 오디오를 구현했다"는 절대 금지 — `m_pSystem->init(1024, FMOD_INIT_NORMAL)` + `createSound(..., FMOD_DEFAULT, ...)`로 **3D 플래그도 listener도 설정 안 했다**.

---

## 3. 실제 구현 (코드 근거)

### 3.1 애니메이션 재생 파이프라인

**호출 경로**: `CModel::LoadCookedAnimations` (Model.cpp:1124) → `anims/*.wanim` 디렉토리 정렬 로드 → `CAnimator::Create` (Model.cpp:1049) → 매 프레임 `CAnimator::Update`.

핵심 자료구조와 알고리즘:

1. **키프레임 이진탐색** — `Animation.cpp:9-59`의 `FindVectorKeySegment`/`FindQuatKeySegment`. 정렬된 키 배열에서 시각 t를 포함하는 세그먼트를 O(log n)로 찾는다. 앞/뒤 경계(`t <= front`, `t >= back`)를 먼저 처리해 클램프한다.

2. **포즈 평가** — `CAnimation::Evaluate` (Animation.cpp:84-103). **채널 없는 본을 Identity가 아니라 `matRestLocal`(Rest Pose)로 초기화**하는 게 핵심(주석 라인 88: "Identity가 아님!"). 채널 있는 본만 위치(lerp)·회전(slerp)·스케일(lerp)로 덮어쓴다.
   - 회전 보간: `InterpolateRotation` (Animation.cpp:120-134) → `XMQuaternionSlerp`. 위치/스케일은 `XMVectorLerp`. **slerp/lerp를 정확히 구분**해 적용.
   - 보간 계수 `f`는 `[0,1]`로 클램프(Animation.cpp:116, 131, 147).

3. **본 계층 누적** — `CSkeleton::ComputeFinalTransformsWithScratch` (Skeleton.cpp:49-77). 본 배열을 **선형 순회**(부모가 자식보다 앞에 있다는 전제)하며:
   ```
   global[i] = parent>=0 ? local[i] × global[parent] : local[i]
   final[i]  = offset[i] × global[i] × globalInverseRoot   // Assimp 표준 (Skeleton.cpp:72-73)
   ```
   `final` 배열이 RHI 스키닝(StructuredBuffer)으로 올라간다.

4. **시간 갱신/루프** — `CAnimator::Update` (Animator.cpp:42-88). `m_dCurrentTime += dt × ticksPerSecond × playSpeed`. duration 초과 시 loop면 `fmod`, 아니면 클램프 후 정지. 음수 방향 재생(역재생)도 처리.

5. **scratch 버퍼 재사용** — `CAnimator::Create`에서 `m_vecGlobalScratch`를 본 수만큼 미리 resize(Animator.cpp:30), 매 프레임 할당 없이 재사용. → GC/할당 압력 제거.

**DLL 경계 처리(비자명)**: `GetCurrentFrame`/`HasFramePassed`/`SetPlaySpeed`를 **헤더 인라인**으로 둬서 `WINTERS_ENGINE` dllexport 없이 Client TU가 직접 호출(Animator.h:33-48). Client는 `ModelRenderer::GetAnimator()`로 받은 포인터로 호출 → DLL 경계 비용 회피.

### 3.2 충돌 수학 라이브러리

**파일**: `Engine/Private/Physics3D/HitVolume.cpp` (392줄), 공개 API `HitVolume.h`.

핵심:
1. **NaN/Inf sanitize 우선** — 모든 진입점이 `SanitizeVolume`(HitVolume.cpp:83-91)을 먼저 통과. `SanitizeScalar`(비유한 → 0), `ClampExtent`(음수/비유한 extent → 0), `NormalizeRadians`. **외부 데이터(스킬 정의)가 더러워도 충돌 함수가 폭발하지 않게** 입력을 정화한다.

2. **yaw-OBB SAT** — `OverlapYawBox`(HitVolume.cpp:179-195). Y축은 단순 구간 겹침으로 빠르게 reject(line 181-183), XZ 평면은 **4개 축(a.X, a.Z, b.X, b.Z)** 각각에 SAT 투영(`OverlapOnAxisXZ`, line 171-177). 투영 반지름은 `ProjectRadiusXZ`(line 164-169). 하나라도 분리되면 false.

3. **Sphere-Box closest point** — `OverlapSphereBox`(HitVolume.cpp:197-211). 구 중심을 박스 로컬축에 투영 → 각 축에서 박스 밖으로 삐져나온 거리(`max(|local|-half, 0)`)의 제곱합 vs 반지름². 정석 closest-point 기법.

4. **shape 디스패치** — `Overlap`(HitVolume.cpp:373-391): Sphere-Sphere → 거리², Sphere-Box → closest point, AABB-AABB → 구간겹침, 그 외 → OBB SAT.

5. **프레임 윈도우** — `MakeActiveWindow`/`IsFrameActive`/`DodgeWindowLengthFrames`(HitVolume.cpp:268-309). telegraph ≤ activeStart ≤ activeEnd 불변식을 강제(clamp)하고, `IsFrameActive`로 "이 프레임에 타격 판정 ON?"을, `DodgeWindowLengthFrames`로 회피 가능 프레임 수를 계산.

**정직성 포인트**: 이 라이브러리의 **유일한 실제 소비처는 `EldenRingEditor/Private/EldenRingEditorScene.cpp`**(grep 확인). LoL 전투 판정에는 아직 안 물려 있다. "working 라이브러리, prototype 통합".

### 3.3 오디오 래퍼

**파일**: `Engine/Private/Sound/Sound_Manager.cpp` (188줄).
1. **FMOD 초기화** — `Initialize`(line 36-46): `System_Create` → `init(1024, FMOD_INIT_NORMAL, nullptr)`. **3D 플래그 없음**.
2. **폴더 재귀 로드** — `LoadSoundFolderRecursive`(line 146-186): exe 옆 `Resource/Sound/`를 재귀 순회, wstring 경로를 **FMOD 요구 UTF-8로 변환**(`WideCharToMultiByte`, line 172) 후 `createSound(..., FMOD_DEFAULT, ...)`. 상대경로를 키로 맵에 저장.
3. **채널 모델** — 고정 채널(`PlaySoundOn`, 같은 채널 재생 중이면 stop 후 교체, line 56-71) vs 자동 채널(`PlayEffect`, 겹침 허용, line 73-82) vs BGM(`FMOD_LOOP_NORMAL`, line 84-97). 전부 **2D 평면 재생** — worldPosition/attenuation/listener 없음.

---

## 4. 검증 — 동작을 어떻게 증명했나

> 정직성: 이 도메인은 **자동 골든/유닛 테스트가 빈약**하다. "측정으로 증명"보다 "시각 확인 + sanitize 가드 + 통합 게이트"가 실상이다. 이걸 숨기지 않는다.

- **애니메이션**:
  - **F5 인게임 Idle/Run 재생 체크리스트**(에셋 파이프라인 도메인과 공유) — 캐릭터가 바인드 포즈로 폭발하지 않고 루프 재생되는지 *육안 확인*. 곱셈 순서 버그는 즉시 스파게티로 드러나므로 시각이 강한 신호다.
  - **`.wanim` 로드 시 `skelHash` 교차검증**(Model.cpp:1144) — 스켈레톤과 애니의 본 카운트/이름 해시 불일치면 거부. 잘못된 애니를 잘못된 스켈레톤에 적용하는 사고 차단.
  - **본 카운트 가드**(Model.cpp:1066) — >256이면 cbuffer overflow 경고.
  - 한계: 포즈 값을 골든으로 박은 회귀 테스트는 없음("측정 예정").
- **물리(충돌)**:
  - **sanitize 가드 자체가 1차 검증** — NaN/음수 extent를 0으로 정화하므로 "더러운 입력에 죽지 않는다"가 코드로 보장됨.
  - `ToDebugString`(HitVolume.cpp:311-344)로 volume/window 상태를 문자열 덤프 → 에디터 디버그 패널에서 시각 확인.
  - 한계: overlap 정답을 박은 단위테스트는 없음. 에디터에서 도형을 겹쳐 보며 수동 확인.
- **오디오**: `OutputDebugStringW`로 로드/스캔 로그(line 142, 180), 인게임에서 *귀로* 확인. 자동 검증 없음.

---

## 5. 최적화

**실제로 한 것**:
1. **키프레임 이진탐색** (Animation.cpp) — 선형 스캔 대신 O(log n) 세그먼트 탐색. 키 많은 클립에서 평가 비용 절감.
2. **scratch 버퍼 사전 할당·재사용** (Animator.cpp:30) — 매 프레임 본 행렬 배열 재할당 제거.
3. **DLL 경계 인라인화** (Animator.h:33-48) — 핫한 프레임 조회 함수를 export 함수 호출이 아닌 인라인으로 → 경계 비용/간접 호출 제거.
4. **SAT early-out** — Y축 구간 겹침으로 먼저 reject(HitVolume.cpp:181-183), 분리축 하나 찾으면 즉시 false. 불필요한 축 검사 회피.
5. **Sphere 빠른 경로** — Sphere-Sphere를 SAT 안 거치고 거리² 비교로 직행(HitVolume.cpp:378-379).

**계획 중(정량 수치 없음 — 측정 예정)**:
- 애니 평가를 JobSystem fan-out으로 병렬화(본 계층 누적은 순차 의존이라 *클립/엔티티 단위* 병렬이 자연스러움).
- RHI StructuredBuffer 스키닝과의 본 행렬 업로드 배칭.
- 물리 broad phase(SAP→Dynamic AABB Tree)로 N² overlap 회피(`00_PHYSICS_PLAN_INDEX.md` Stage 3).

> **정직성**: 이 도메인엔 프로파일러 캡처 수치를 아직 안 박았다. 정량 최적화 서사는 성능/측정 인프라 도메인(#12)에 있고, 여기는 "알고리즘적으로 옳은 선택(이진탐색/scratch 재사용/early-out)"까지다.

---

## 6. 구현 예정 (Planned) — 동일한 깊이로

> 사용자는 실제로 구현할 것이므로, "안 했다"로 끝내지 않고 **무엇을·왜·어떻게·Trade-off·검증**까지 구현된 부분과 같은 깊이로 적는다.

### 6.1 애니메이션: AnimGraph (Ch4)

**무엇을**: 단일 클립 재생 → 포즈를 매 프레임 합성하는 노드 그래프. 각 노드가 `Evaluate(out pose)`를 구현하는 `IAnimNode` 인터페이스(Ch4 §4.2).

**왜(어떤 문제)**: (a) 이동 중 공격(walk+attack 블렌딩), (b) 상·하체 분리(달리며 활쏘기), (c) castFrame 하드코딩 부채(현재 `_Skills.cpp`에 숫자 박힘) 해소.

**어떻게(단계)**:
1. **Animator를 1-노드 그래프로 리팩터** — 현재 `CAnimator`가 사실상 단일 ClipNode다. 이걸 `IAnimNode::Evaluate(AnimPoseContext&)`로 추출.
2. **StateMachine 노드** — 상태 + transition condition. Locomotion↔Attack↔Dead.
3. **Montage + Section + Notify** (데이터 우선) — 스킬/공격을 montage 트랙으로. **notify 트랙이 핵심**: `t=0.3 [GameplayEvent: hitFrame]`이 곧 castFrame을 대체. 서버는 actionSeq cue만 보내고 클라가 notify로 FxCue/AudioCue를 트리거(damage는 서버 권위 유지).
4. BlendSpace(2D 입력→블렌딩) → LayeredBoneBlend(상하체) → IK → Inertialization.

**Trade-off 예상**: AnimGraph 평가는 워커 스레드로 보내야 하는데(게임 스레드 분리), **데이터 race에 매우 엄격**해진다. 단순 단일 클립 대비 복잡도가 급증하므로 *MOBA에 실제 필요한 Stage(1,3,5)만* 먼저. 전부 다 하면 과설계.

**검증 방법**: `--anim-dump-graph=<champ>`로 그래프 구조 덤프(Ch4 §5) + 블렌딩 결과 육안 확인 + 향후 포즈 골든.

**불변식(중요)**: AI는 montage를 직접 재생하지 않는다. AI→`GameCommand`→server `TryActivate`→성공 시 actionSeq broadcast→클라가 montage 재생. notify는 클라 visual/서버 damage tick이지 AI 관심사가 아니다(Ch4 §4.4).

### 6.2 물리: 자체 강체/구속/CCD (Phase D, Stage 1~7)

**무엇을**: 충돌 수학 라이브러리 → broad/narrow phase + 강체 역학 + 구속 솔버 + PBD + CCD를 갖춘 진짜 물리 시스템.

**왜**: (a) 스킬샷 투사체가 얇은 벽을 통과하는 tunneling 방지(CCD 필수), (b) 다수 히트박스 overlap의 N² 회피(broad phase), (c) 엘든링 모작용 천/로프(PBD), (d) 충돌·적분·구속 이론을 실제 구현한 이력.

**어떻게(의존성 순서, `00_PHYSICS_PLAN_INDEX.md`)**:
```
Stage1 Primitives(AABB/Sphere/Ray)  ← 현재 capsule 비교를 정식 raycast/sweep으로 wrap
  → Stage3 Broad Phase(SAP→Dynamic AABB Tree)  ← N² 먼저 제거
  → Stage2 Narrow Phase(SAT 확장, GJK+EPA)
  → Hitbox/Hurtbox 통합  ← 현재 라이브러리를 LoL 전투에 실제 연결
  → Stage4 Rigid Body(Semi-implicit Euler)
  → Stage5 Constraint(Sequential Impulse)
  → Stage6 PBD/XPBD(Cloth/Rope)
  → Stage7 CCD(TOI)
```

**Trade-off 예상**: (1) **결정론 vs 성능** — 서버 gameplay 물리는 fixed-dt·결정적이어야 하고(스냅샷 복제 모델이라 lockstep까진 불필요), visual 물리(cloth/ragdoll)는 클라 only·비결정. 이 둘을 섞으면 50ms ping 차이로 다른 결과를 보는 사고(Ch5 §3.4). (2) **구속 솔버 iteration** — 늘리면 안정, 줄이면 빠름. MOBA는 강체가 거의 없으니 Stage 4~6은 과잉일 수 있어 **MOBA는 Stage 1~3+CCD만 필수, PBD는 엘든링/포트폴리오용**으로 명시 분리.

**검증 방법**: `--phys-debug --phys-show-aabb`로 broadphase/island/contact 로그(Ch5 §5) + DebugDraw 시각화 + 결정론 골든(SimLab에 물리 스텝 해시 추가).

### 6.3 오디오: AudioEngine + 3D + DSP (Ch6)

**무엇을**: 2D 래퍼 → Voice/Submix/DSP 그래프 + 3D spatialize + occlusion + 데이터 분기 사운드 cue.

**왜**: (a) 거리 감쇠/방향감, (b) submix별 reverb/EQ + 전투 시 음악 ducking, (c) 벽 너머 소리 차단(occlusion), (d) actionSeq→audio cue를 single source of truth로(Ch4 notify와 합류).

**어떻게**: FMOD 위에 `CAudioEngine` wrap → `PlayOneShot/PlayAttached` + `SetListener` → submix 트리 + `AddEffect` → occlusion은 **Ch5 raycast 재사용**(LowPass cutoff) → `.wsound` 데이터 분기(Metasound 등가).

**Trade-off 예상**: 탑다운 MOBA는 3D 체감 이득이 낮아 **Stage 1~3(3D+submix+reverb)이 LoL 한계 효용점**. voice stealing(64 voice 초과 시 priority/거리/age)이 동시 이펙트 많은 한타에서 필요해짐. Wwise는 라이브 서비스 진입 전엔 ROI 부족.

**검증 방법**: `--audio-trace --audio-show-voices`로 submix 트리/voice spawn/occlusion cutoff 로그(Ch6 §5).

---

## 7. 면접 예상 질문 & 모범 답변 (12개)

**Q1. (기본) 스켈레탈 애니메이션에서 본 계층을 왜 트리로 두고, 최종 본 행렬은 어떻게 만드나요?**
A. 자식 본이 부모를 따라 움직여야 하기 때문입니다. 본을 부모-먼저 순서로 정렬해 두고 선형 순회하면서 `global[i] = local[i] × global[parent]`로 누적합니다. 최종 스키닝 행렬은 `Offset × Global × GlobalInverseRoot`입니다. Offset(=inverse bind pose)이 정점을 본 로컬로 끌어내리고, Global이 현재 월드로 보내고, GlobalInverseRoot가 DCC 루트 보정을 상쇄합니다. Assimp 표준이고, `Skeleton.cpp:72-73`에 그대로 있습니다. 순서가 하나만 틀려도 캐릭터가 폭발해서, 사실상 곱셈 순서가 self-test입니다.

**Q2. (기본) 위치는 lerp인데 회전은 왜 slerp인가요?**
A. 쿼터니언을 선형 보간하면 (a) 회전 각속도가 불균일해지고 (b) 중간 결과가 단위 쿼터니언에서 벗어나 정규화가 깨집니다. slerp는 단위 구면 위 최단 호를 등속으로 따라가서 둘 다 해결합니다. 그래서 `InterpolateRotation`만 `XMQuaternionSlerp`, 위치/스케일은 `XMVectorLerp`로 나눠 뒀습니다(`Animation.cpp:120-148`).

**Q3. (기본) 채널 없는 본을 Identity로 안 두고 Rest Pose로 초기화하는 이유는?**
A. 애니메이션이 모든 본에 키를 주지 않습니다. 키 없는 본을 Identity로 두면 그 본이 원점으로 끌려가 메시가 찢어집니다. 바인드 포즈의 로컬 변환(`matRestLocal`)으로 초기화해야 "이 본은 그냥 가만히 있어라"가 됩니다(`Animation.cpp:88-90`, 주석에도 "Identity가 아님!"으로 박아 뒀습니다).

**Q4. (설계) SAT가 뭐고, 왜 GJK 대신 SAT를 골랐나요?**
A. 분리축 정리입니다 — 두 볼록 도형이 안 겹치면 투영이 분리되는 축이 반드시 존재한다는 정리로, 후보 축(yaw-OBB면 두 박스의 X·Z, 4개)을 전부 검사해 하나라도 분리되면 충돌 아님입니다. GJK/EPA보다 단순하고, MOBA 히트박스는 박스/구만 쓰므로 임의 볼록다면체용 GJK는 오버스펙입니다. `OverlapYawBox`에 Y축 빠른 reject + XZ 4축 SAT로 구현했습니다(`HitVolume.cpp:179-195`).

**Q5. (설계) 충돌 함수가 NaN 입력에 어떻게 견디나요?**
A. 모든 진입점이 `SanitizeVolume`을 먼저 통과합니다. 비유한 스칼라는 0, 음수/비유한 extent는 0, yaw는 정규화합니다(`HitVolume.cpp:83-91`). 스킬 정의 데이터가 더러워도 충돌 수학이 폭발하지 않게 입력단에서 막는 설계입니다. 이게 라이브러리의 1차 검증 역할도 합니다.

**Q6. (압박/레드플래그) 이거 "물리 엔진"이라고 부를 수 있나요? 강체 시뮬은 어디 있죠?**
A. 아니요, 물리 엔진이 아닙니다. **SAT 기반 충돌 판정 라이브러리 + 격투형 프레임 윈도우**입니다. 강체/구속 솔버/CCD는 0줄이고, 그건 `00_PHYSICS_PLAN_INDEX.md`의 Stage 4~7 계획입니다. 의도적입니다 — MOBA 챔피언은 물리로 안 움직이고(서버 권위 그리드 이동) 히트박스는 "겹침 이벤트"만 필요해서, 지금 필요한 overlap query만 정확히 만들었습니다. 강체는 엘든링 모작·포트폴리오용으로 뒤에 뒀습니다.

**Q7. (압박/레드플래그) 그 충돌 라이브러리, 실제로 LoL 전투 판정에 쓰이나요?**
A. 솔직히 아직 아닙니다. 현재 유일한 실제 소비처는 EldenRing 에디터의 디버그 패널입니다(`EldenRingEditorScene.cpp`). "working 라이브러리, prototype 통합" 상태입니다. LoL 전투는 아직 거리 기반 단순 판정을 쓰고, 이 라이브러리를 Hitbox/Hurtbox로 연결하는 게 Phase D의 "통합" 스테이지입니다. 라이브러리 자체는 SAT/closest-point가 정확하고 sanitize까지 돼 있어서, 연결만 남았습니다.

**Q8. (압박/레드플래그) "3D 오디오" 됩니까?**
A. 안 됩니다. 순수 2D FMOD 래퍼입니다. `init(1024, FMOD_INIT_NORMAL)` + `createSound(..., FMOD_DEFAULT)`로 **3D 플래그도 listener도 attenuation도 설정 안 했습니다**(`Sound_Manager.cpp:41, 176`). 의도적인데, 탑다운 MOBA는 카메라가 고정 부감이라 3D 공간감 체감 이득이 낮습니다. 3D spatialize는 Ch6 Stage 2 계획이고, occlusion은 Ch5 raycast를 재사용하도록 설계해 뒀습니다.

**Q9. (압박/레드플래그) 애니메이션 블렌딩이나 StateMachine은요? 이동하면서 공격은 어떻게 하죠?**
A. 지금은 못 합니다 — 단일 클립 재생만입니다. 이동중 공격 블렌딩, 상하체 분리, IK 전부 Ch4의 AnimGraph 계획입니다. 그리고 castFrame을 챔프 코드에 숫자로 박은 부채가 있습니다(LoLVisualDefinitions에 95회). 이걸 montage notify 트랙으로 데이터화하는 게 Ch4 Stage 3입니다. 단순 클립 재생이 안정된 뒤 그래프로 리팩터하는 게 순서라고 봤고, 한계를 챕터 문서에 선제 기록했습니다.

**Q10. (심화) AnimGraph를 워커 스레드로 평가한다고 했는데 위험 요소는?**
A. 데이터 race입니다. AnimGraph evaluate는 게임 스레드와 분리해 워커에서 돌리되, 입력 상태 갱신(`NativeUpdateAnimation` 등가)과 포즈 평가(`Evaluate_AnyThread` 등가)를 엄격히 분리해야 합니다. 평가 중에는 그래프 입력이 불변이어야 하고, 본 행렬 출력 버퍼는 더블버퍼링하거나 프레임 경계로 동기화합니다. 제 JobSystem은 Chase-Lev work-stealing이라 클립/엔티티 단위 fan-out으로 붙이기 좋습니다 — 본 계층 누적 자체는 순차 의존이라 *엔티티 간* 병렬이 자연스럽습니다.

**Q11. (심화) 서버 권위 멀티플레이에서 물리/애니/오디오를 어떻게 분리하나요?**
A. 핵심은 **gameplay(서버 권위·결정적) vs visual(클라 only·비결정적) 분리**입니다. 서버는 capsule sweep/raycast 같은 단순·결정적 판정과 결과(damage, death cue)만 보냅니다. 클라는 스냅샷으로 위치를 보간하고 그 위에 cloth/ragdoll/오디오/FX를 얹습니다. 이 분리가 깨지면 ping 차이로 플레이어마다 다른 결과를 봅니다. 애니의 notify도 같은 원칙 — damage는 서버 권위, FxCue/AudioCue는 클라 visual입니다. AI는 montage/사운드/물리를 전혀 모르고 `GameCommand`만 발행합니다.

**Q12. (심화) 자체 물리를 만든다면 broad phase를 왜 narrow phase보다 먼저 하나요?**
A. N² 회피가 먼저라서입니다. narrow phase(정밀 SAT/GJK)는 비싸니까, broad phase(Dynamic AABB Tree나 SAP)로 "겹칠 가능성 있는 후보 쌍"만 O(N log N)으로 추려 narrow phase 호출 횟수를 줄여야 합니다. `00_PHYSICS_PLAN_INDEX.md`의 의존성 순서도 Stage1(프리미티브)→Stage3(broad)→Stage2(narrow)로, 정밀도보다 성능 구조를 먼저 잡습니다. broad phase의 AABB Tree는 Path Tracing의 BVH와 수학을 공유해서 재사용 이득도 있습니다.

---

## 8. 30초 엘리베이터 피치

"애니메이션·물리·오디오 — 솔직히 제 엔진에서 제일 얇은 도메인이지만, *통제된 얇음*입니다. 외부 미들웨어 없이 스켈레탈 애니 재생을 직접 만들었어요. 본 계층 누적에 Offset×Global×GlobalInverseRoot 규약, 키프레임 이진탐색에 회전은 slerp·위치는 lerp까지. 충돌은 SAT 기반으로 yaw-OBB랑 sphere-box closest point를 NaN sanitize까지 넣어 짰고, telegraph·active·dodge 프레임 윈도우로 격투형 판정을 표현합니다. 오디오는 2D FMOD 래퍼고요. 여기서 핵심은 — 저는 '물리 엔진 만들었다', '3D 오디오 된다'고 말하지 않습니다. 강체·블렌딩·3D 공간음은 Ch4~6 챕터 문서와 7단계 물리 계획에 의존성 순서까지 적어 뒀고, MOBA 우선순위에 맞춰 *의도적으로 뒤로 미룬 것*과 *아직 못 한 것*을 구분합니다. 어디까지 했고 어디부터 왜 계획인지를 정확히 그을 수 있다는 게 이 약한 도메인에서 제가 보여드릴 강점입니다."
