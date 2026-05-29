# Phase D — 자체 Physics 구현 계획 (인덱스)

## 비전

외부 물리 엔진 (Jolt/PhysX/Bullet/Box2D) 의존 없이 **강체·연체·구속·CCD** 를 C++20 로 직접 구현.
MOBA 충돌 판정부터 연습모드 PBD 천/로프 시뮬레이션까지 전부 자체.

## 왜 자체 구현인가

- **포트폴리오 목적** — 충돌 수학 + 수치 적분 + 구속 풀이 이론을 실제로 이해하고 구현한 이력
- **게임 특성 맞춤** — LoL 모작의 핵심은 MOBA 스킬샷 판정 (빠름, 가벼움). Jolt 는 오버스펙
- **Phase E 와 수학 공유** — BVH, 몬테카를로, 확률 분포가 Path Tracing 과 겹침
- **교육적 가치** — 물리 시뮬레이션 직접 디버깅해본 경험 = 엔진 프로그래머 핵심 역량

## Stage 로드맵 (1~7)

| Stage | 내용 | 문서 |
|---|---|---|
| 1 | 프리미티브 충돌 (AABB / Sphere / Ray) | [02_STAGE1_PRIMITIVES.md](02_STAGE1_PRIMITIVES.md) |
| 2 | Narrow Phase (SAT OBB-OBB, GJK+EPA) | [03_STAGE2_NARROW_PHASE.md](03_STAGE2_NARROW_PHASE.md) |
| 3 | Broad Phase (SAP → Dynamic AABB Tree) | [04_STAGE3_BROAD_PHASE.md](04_STAGE3_BROAD_PHASE.md) |
| 4 | Rigid Body Dynamics (Semi-implicit Euler) | [05_STAGE4_RIGID_BODY.md](05_STAGE4_RIGID_BODY.md) |
| 5 | Constraint Solver (Sequential Impulse) | [06_STAGE5_CONSTRAINTS.md](06_STAGE5_CONSTRAINTS.md) |
| 6 | PBD (Cloth / Rope / Soft Body + XPBD) | [07_STAGE6_PBD.md](07_STAGE6_PBD.md) |
| 7 | CCD (TOI — 빠른 공격/투사체) | [08_STAGE7_CCD.md](08_STAGE7_CCD.md) |

## 공통 시스템

| 주제 | 문서 |
|---|---|
| 아키텍처 + ECS 통합 + 디렉토리 | [01_ARCHITECTURE.md](01_ARCHITECTURE.md) |
| Socket / Hitbox / Hurtbox 통합 | [09_INTEGRATION.md](09_INTEGRATION.md) |
| 디버그 시각화 + ImGui 튜너 | [10_DEBUG_TOOLS.md](10_DEBUG_TOOLS.md) |

## 구현 순서 (의존성 기준)

```
Stage 1 Primitives        ← 모든 충돌의 기본
     ↓
Stage 3 Broad Phase        ← 성능: N² 회피 먼저
     ↓
Stage 2 Narrow Phase       ← 정밀 판정
     ↓
Phase C-3 Socket / Hitbox  ← 충돌 쿼리 소비자
     ↓
Stage 4 Rigid Body         ← 강체 역학
     ↓
Stage 5 Constraints        ← 조인트/접촉 풀이
     ↓
Stage 6 PBD                ← 연체 (천/로프)
     ↓
Stage 7 CCD                ← 고속 물체
```

## MOBA 특화 요구사항

LoL 같은 탑다운 MOBA 는 일반 3D 시뮬레이터 대비 요구 다름:

| 요구 | 이유 |
|---|---|
| 2D 기반 판정 위주 | 대부분 y=0 평면. 원기둥 (Capsule) 수직 충돌만 검사 |
| Kinematic (강체 역학 X) | 챔피언은 물리로 안 움직임. 서버 권위 이동 |
| **Trigger 콜라이더 우선** | Hitbox 는 "겹침 이벤트" 만 필요 |
| 지형 충돌은 NavMesh | Phase C-4 Navigation 이 담당 |
| CCD 필수 | 스킬샷 투사체 관통 방지 |
| Raycast 빈번 | 픽킹, 시야, LoS 체크 |

반면 Phase D Stage 4~6 (Rigid Body / Constraint / PBD) 는 LoL 모작엔 과잉일 수 있음.
**Stage 6 PBD 는 엘든링 모작 (Phase B) + 포트폴리오용**. MOBA 에선 Stage 1~3 + CCD 만 필수.

## 참고 문헌

- **Real-Time Collision Detection** — Christer Ericson (공간 쿼리 정석)
- **Game Physics Engine Development** — Ian Millington (Rigid Body + Constraint 입문)
- **Position Based Dynamics** — Matthias Müller 2007 (PBD 원논문)
- **XPBD: Position-Based Simulation of Compliant Constrained Dynamics** — Müller 2016
- **Erin Catto GDC** — Sequential Impulse 발표 (Box2D 저자)
- **Bullet Physics 소스코드** — 오픈소스 참고
- **Box2D 소스코드** — 2D 레퍼런스
- **Baraff "Large Steps in Cloth Simulation"** 1998 (Implicit Euler)

## Phase E 와의 공유

| 수학/자료구조 | Phase D | Phase E |
|---|---|---|
| BVH (SAH 빌드) | Broad Phase | Path Tracing 가속 구조 |
| Ray-AABB 교차 | Raycast | Ray Generation |
| 난수/확률 | PBD (Jakobsen) | 몬테카를로 적분 |
| Kinematic 적분 | Semi-implicit Euler | 모션 벡터 (TAA) |

## 의존성

| 필요 | 상태 |
|---|---|
| ECS 기반 | ✅ Phase 1a |
| Transform 계층 | ✅ |
| DebugDraw | ⏭️ Phase C-2 (필수) |
| Job System 병렬화 | 🔄 Phase 1b (성능 큰 향상) |
| Compute Shader | ✅ DX11 지원 |

## 예상 소요

- Stage 1~3: 2주 (MOBA 판정 작동)
- Stage 4~5: 3주 (강체/조인트 일반 케이스)
- Stage 6: 2주 (Cloth 기본)
- Stage 7: 1주 (CCD TOI)
