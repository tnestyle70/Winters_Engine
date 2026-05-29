# 미니언 군중 Collision 해소 — 구현 계획서 (다음 세션 진입점)

**작성일**: 2026-04-28 (세션 종료 시 stub)
**전제**:
- Worker-safety + Minion combat 통합 (`MINION_COMBAT_AND_WORKER_SAFETY.md` v1) M0~M7 완료
- 미니언 적 감지 → Chase → Attack → death 풀 사이클 동작 검증 완료
- 마지막 fix: `Spawn_Minion` 에서 `VelocityComponent` 명시 초기화 (`vDirection={0,0,0}, fSpeed=0`)
- 모든 worker-safety 패치 production-safe

**다음 세션 진입 즉시 시작점**: 본 계획서 §0 표 + §3 부터 진입 가능

---

## 0. 문제 — 미니언이 같은 cell 에 겹침 + 적 한 마리에 다굴 시 한 점에 모임

LoL 원작 동작:
- 미니언끼리: **soft collision** (약하게 서로 밀침, 통과 가능)
- 미니언 vs 챔프: **hard collision** (막힘, 우회)
- 미니언 vs 구조물 (포탑/벽): **완전 막힘**
- 같은 타겟 다굴 시 미니언이 타겟 주위에 부채꼴로 둘러쌈 (한 점 X)

현재 Winters 동작:
- NavigationSystem 이 path 따라 이동 → 같은 적 미니언 향해 N 마리가 같은 cell 로 수렴
- 충돌 처리 0 → 시각적으로 한 점에 겹침
- 라인전 시 미니언 군집이 한 덩어리로 보임

---

## 1. 솔루션 옵션 비교

| 옵션 | 작업량 | 품질 | 정합 |
|---|---|---|---|
| **A) Soft Separation Force (Boids)** | 1~2일 | 중 | 즉시 적용 가능, 자체 코드 |
| **B) Spatial Hash + Push-out** | 2~3일 | 중상 | 자체 spatial hash 자료구조 필요 |
| **C) NavGrid 동적 마킹** | 1일 | 하 | path 자체에 미니언 점유 반영 — 단 매 프레임 mark/unmark race |
| **D) Jolt Physics 도입** | 5~7일 | 상 (정석) | CLAUDE.md 기술 스택 명시. 단 Phase 7 Physics 영역 |

**권장 진입 순서**:
- **1차 (B-10c-3 정도)**: A) Soft Separation Force — 즉시 시각 개선
- **2차 (Phase 7 직전)**: D) Jolt Physics 정식 도입 — 챔프/투사체/구조물 통합

---

## 2. 1차 = Soft Separation Force (Boids 의 Separation 규칙)

### 2.1 원리

각 미니언이 일정 반경 안의 이웃 미니언으로부터 멀어지는 force 추가:

```cpp
Vec3 separationForce = {0, 0, 0};
i32_t neighborCount = 0;
for (otherMinion in nearby) {
    if (other == self) continue;
    Vec3 delta = self.pos - other.pos;
    f32_t dist = length(delta);
    if (dist < kSeparationRadius && dist > 0.001f) {
        separationForce += normalize(delta) * (kSeparationRadius - dist) / kSeparationRadius;
        ++neighborCount;
    }
}
if (neighborCount > 0) {
    separationForce /= neighborCount;
    vel.vDirection = normalize(vel.vDirection + separationForce * kSeparationWeight);
}
```

### 2.2 파라미터 (튜닝 대상 — ImGui 슬라이더 노출 의무 — gotchas #14)

| 파라미터 | 초기값 | 의미 |
|---|---|---|
| `kSeparationRadius` | 1.0 unit | 이웃 인식 반경 (미니언 반경의 ~2배) |
| `kSeparationWeight` | 0.5 | nav direction 대비 separation force 비중 |
| `kMaxNeighbors` | 8 | 성능 — 가장 가까운 N 마리만 |

### 2.3 구현 위치 — 새 ECS System (Phase 1.5)

`Engine/Public/ECS/Systems/MinionSeparationSystem.h/.cpp` 신규.
- Phase 1.5 (NavigationSystem L1 → MinionSeparationSystem L1.5 → MinionAISystem L2)
- ForEach<MinionComponent, TransformComponent, VelocityComponent>
- O(N²) — 미니언 100 마리면 10,000 비교. 충분히 빠름. 1000+ 시 spatial hash 필요

### 2.4 Worker-Safety 적용

- self entity write 만 (vel.vDirection 갱신) — `MINION_COMBAT_AND_WORKER_SAFETY.md` 정책 (4)
- 다른 미니언 read-only 순회 (TransformComponent.GetLocalPosition)
- worker thread 에서 안전. Set_JobSystem 활성 가능

---

## 3. 마일스톤 (다음 세션 진입 즉시 시작)

```
M0  현재 라인전 상태에서 미니언 군집 시각 캡처 (before)
M1  ImGui 패널에 SeparationSettings 추가 (kSeparationRadius/Weight/MaxNeighbors)
M2  CMinionSeparationSystem.h/.cpp 신규 (Phase 1.5)
M3  Scene_InGame.cpp Scheduler.RegisterSystem 등록 (Nav 와 MinionAI 사이)
M4  ForEach 기반 O(N²) 1차 구현 (worker-safe)
M5  ImGui 슬라이더 튜닝 — Radius / Weight 조정해 시각 적정값 찾기
M6  worker 병렬 활성 — Set_JobSystem(pJS)
M7  F5 검증 — 미니언 16+ 라인전 시 부채꼴 분포 + 한 점 겹침 0
M8  성능 검증 — 100+ 미니언 시 frame time 영향
M9  스크린샷 (after) + before 비교
```

**예상 시간**: 1~2일

---

## 4. 다음 후속 — Phase 7 Jolt Physics 정식

본 계획서 외부. CLAUDE.md `07. Physics — Jolt Physics 래퍼` 영역.
- 챔프/투사체/구조물/미니언 통합
- Capsule 충돌 + RigidBody
- Sweep test (투사체)
- Soft separation 은 Jolt 의 character controller 로 자연 흡수

---

## 5. 추가 후속 후보 (미니언 collision 해결 후)

| 사이클 | 내용 |
|---|---|
| M6 의 ranged minion projectile | `MINION_COMBAT_AND_WORKER_SAFETY.md` §10 |
| 06 v4 B-10c 잔여 (worker slot stress) | 이미 흡수됨 |
| 06 v4 B-10d-pre PlayerTransformAdapter | Riven 향 1.5일 |
| 06 v4 B-10d Riven Pure ECS | 2~3일 |
| Phase 1b JobSystem Fiber 강화 | CLAUDE.md L11~18 |
| Phase 2 RenderGraph + Deferred | CLAUDE.md L11~18 |

---

## 6. 진입 직전 체크리스트

- [ ] 빌드 성공 (Debug x64) — 본 세션 마지막 상태에서 시작
- [ ] 미니언 적 감지/Chase/Attack/death 풀 사이클 동작 확인 (F5)
- [ ] Profiler counter 살아있음 (`MinionAI::Candidates/Chase/Attack`)
- [ ] `MINION_COMBAT_AND_WORKER_SAFETY.md` v1 읽기
- [ ] §3 M1 부터 진입 → MinionSeparationSystem 구현

---

## 7. 한 줄

**Soft Separation Force (Boids) 1차 → ImGui 튜닝 → worker 병렬 → Jolt Physics 정식 (Phase 7). 다음 세션 §3 M1 부터 즉시 진입 가능.**
