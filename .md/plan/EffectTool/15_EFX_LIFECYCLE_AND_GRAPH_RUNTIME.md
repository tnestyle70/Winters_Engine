# EFX Lifecycle + Graph Runtime — 상태머신 + Tick 순서 + Worker-Safe + Hot Reload

**작성일**: 2026-05-07
**상태**: ✅ v3 보강 — Effect 라이프사이클 + 그래프 런타임 실행 흐름 박제
**선행**: [`13_EFFECT_TOOL_V3_MASTER.md`](13_EFFECT_TOOL_V3_MASTER.md), [`14_NIAGARA_REFERENCE_DEEP_MAP.md`](14_NIAGARA_REFERENCE_DEEP_MAP.md)
**범위**: EFX-2 박제 시 라이프사이클 상태머신 + EFX-5 박제 시 그래프 실행 흐름 + Hot Reload sequence

---

## §1. Lifecycle 상태머신 — 5 state (EFX-2)

### 1.1 State enum

```cpp
// Engine/Public/FX/FxLifecycle.h (EFX-2 박제 예정)
enum class eFxLifecycleState : u8_t
{
    Inactive,        // 컴포넌트 존재만, system 측 슬롯 없음
    Active,          // tick 받음, spawn/update 실행
    Completing,      // spawn 중지, 잔여 particle 소멸 대기
    Complete,        // 모든 particle 소멸, system 측 cleanup 대기
    PoolReturned,    // pool slot 반환, 컴포넌트 제거 대기
};
```

### 1.2 상태 전이 다이어그램

```
                        SpawnFromAsset()
                              ↓
                          [Inactive]
                              ↓ (다음 Phase 11 Tick)
                          [Active] ←─── (loop = true 시 자동 재진입)
                              │
              ┌───────────────┼───────────────┐
              │               │               │
       (lifetime 도달)   (RequestComplete   (Loop 종료 명시)
       (loop = false)    명시 호출)
              ↓               ↓               ↓
                        [Completing]
                          (spawn 중지)
                              ↓
                  (모든 particle.age >= lifetime)
                              ↓
                         [Complete]
                              ↓
              (CFxSimulationSystem cleanup)
                              ↓
                       [PoolReturned]
                              ↓
              (다음 Component cleanup phase)
                              ↓
                    [Component 제거 + Entity destroy]
```

### 1.3 8 단계 상세 흐름

**Step 1 — SpawnFromAsset 호출**

```cpp
EntityID CFxSimulationSystem::SpawnFromAsset(CWorld& world, FxAssetHandle h, ...)
{
    // 1.1 자산 검증
    const FxAsset* pAsset = CFxAssetRegistry::Instance().Find(h);
    if (!pAsset) return NULL_ENTITY;

    // 1.2 새 entity 생성
    EntityID e = world.CreateEntity();

    // 1.3 FxInstanceComponent 부착
    FxInstanceComponent fic{};
    fic.hAsset = h;
    fic.hAttachTo = (attachTo != NULL_ENTITY)
                       ? world.MakeEntityHandle(attachTo)
                       : NULL_ENTITY_HANDLE;
    fic.vAttachOffset = vOffset;
    fic.fLifetime = ComputeLongestLifetime(*pAsset);
    fic.bLoop = pAsset->bLoop;   // FxAsset 에 bLoop 추가 필요 (v3 신규)
    fic.state = eFxLifecycleState::Inactive;
    fic.uInstanceSlot = FX_INVALID_INSTANCE;
    world.AddComponent(e, std::move(fic));

    return e;
}
```

**Step 2 — 첫 Phase 11 Tick (Inactive → Active)**

```cpp
void CFxSimulationSystem::Execute(CWorld& world, f32_t fDeltaTime)
{
    // Step 2.1: Inactive 컴포넌트 찾아서 슬롯 할당 + Active 전이
    world.ForEach<FxInstanceComponent>(
        [&](EntityID e, FxInstanceComponent& fic)
        {
            if (fic.state == eFxLifecycleState::Inactive)
            {
                // 슬롯 할당 (재사용 또는 신규)
                u32_t slot;
                if (!m_vecFreeSlots.empty())
                {
                    slot = m_vecFreeSlots.back();
                    m_vecFreeSlots.pop_back();
                }
                else
                {
                    slot = static_cast<u32_t>(m_vecInstances.size());
                    m_vecInstances.emplace_back();
                }
                fic.uInstanceSlot = slot;

                // CFxSystemInstance 초기화
                m_vecInstances[slot].Initialize(fic.hAsset, e, ++m_uNextRngSeed);

                fic.state = eFxLifecycleState::Active;
            }
        });

    // Step 2.2: Active 컴포넌트 tick
    // ... (Step 3)
}
```

**Step 3 — 매 frame Tick (state == Active)**

```cpp
world.ForEach<FxInstanceComponent, TransformComponent>(
    [&](EntityID e, FxInstanceComponent& fic, const TransformComponent& tx)
    {
        if (fic.state != eFxLifecycleState::Active) return;

        CFxSystemInstance& inst = m_vecInstances[fic.uInstanceSlot];

        // Tick
        inst.Tick(fDeltaTime, tx, world.GetFxParameterMap());

        // Age 갱신
        fic.fAge += fDeltaTime;

        // Lifetime 체크
        if (!fic.bLoop && fic.fAge >= fic.fLifetime)
        {
            inst.RequestComplete();
            fic.state = eFxLifecycleState::Completing;
        }
    });
```

**Step 4 — Completing (spawn 중지, 잔여 particle 소멸 대기)**

```cpp
void CFxSystemInstance::RequestComplete()
{
    for (auto& emitter : m_vecEmitters)
        emitter.RequestComplete();   // bSpawnEnabled = false

    m_State = eFxLifecycleState::Completing;
}

// CFxEmitterInstance::RequestComplete
void CFxEmitterInstance::RequestComplete()
{
    m_bSpawnEnabled = false;
    // 기존 particle 은 자연 소멸 (Update_Phase 가 age 갱신 + Cull_Phase 가 kill)
}
```

**Step 5 — Completing → Complete 전이 검사**

매 frame Tick 마지막에:
```cpp
world.ForEach<FxInstanceComponent>(
    [&](EntityID e, FxInstanceComponent& fic)
    {
        if (fic.state != eFxLifecycleState::Completing) return;

        const CFxSystemInstance& inst = m_vecInstances[fic.uInstanceSlot];
        if (inst.IsAllParticlesDead())
        {
            fic.state = eFxLifecycleState::Complete;
        }
    });
```

**Step 6 — Complete → PoolReturned (시스템 cleanup)**

```cpp
world.ForEach<FxInstanceComponent>(
    [&](EntityID e, FxInstanceComponent& fic)
    {
        if (fic.state != eFxLifecycleState::Complete) return;

        // 슬롯 반환
        m_vecInstances[fic.uInstanceSlot].Reset();
        m_vecFreeSlots.push_back(fic.uInstanceSlot);
        fic.uInstanceSlot = FX_INVALID_INSTANCE;
        fic.state = eFxLifecycleState::PoolReturned;
    });
```

**Step 7 — PoolReturned → Component 제거**

```cpp
// Phase 11 의 마지막 단계 또는 별도 cleanup phase
std::vector<EntityID> toRemove;
world.ForEach<FxInstanceComponent>(
    [&](EntityID e, FxInstanceComponent& fic)
    {
        if (fic.state == eFxLifecycleState::PoolReturned)
            toRemove.push_back(e);
    });

for (EntityID e : toRemove)
{
    world.RemoveComponent<FxInstanceComponent>(e);
    // 외부 owner 가 attached entity 면 destroy 안 함 (e 가 attach 된 entity 의 entity-of-owned-effect)
    // 호출자가 spawn 한 ephemeral entity (FxOnly entity) 면 destroy
    if (fic.bAutoDestroyEntity)   // FxInstanceComponent 에 추가 필요
        world.DestroyEntity(e);
}
```

**Step 8 — Loop 케이스 (state == Active, bLoop == true)**

```cpp
// Step 3 의 lifetime 체크에서:
if (fic.bLoop && fic.fAge >= fic.fLifetime)
{
    // Loop 재시작 — particle 모두 kill, age reset, spawn 재개
    inst.LoopRestart();
    fic.fAge = 0.f;
    // state 는 Active 유지
}

void CFxSystemInstance::LoopRestart()
{
    for (auto& emitter : m_vecEmitters)
    {
        emitter.GetPool().Reset();   // 모든 particle 제거
        emitter.SetSpawnEnabled(true);
    }
    m_LocalParams.Set<f32_t>(eFxNamespace::Emitter, "LoopedAge", 0.f);
}
```

### 1.4 상태별 invariant

| state | 보장 |
|---|---|
| `Inactive` | `uInstanceSlot == FX_INVALID_INSTANCE` |
| `Active` | `uInstanceSlot != FX_INVALID_INSTANCE` && `m_vecInstances[slot].IsActive()` && spawn enabled |
| `Completing` | `m_vecInstances[slot].IsCompleting()` && spawn disabled |
| `Complete` | `m_vecInstances[slot].IsAllParticlesDead()` |
| `PoolReturned` | `uInstanceSlot == FX_INVALID_INSTANCE` (다시) |

### 1.5 외부 호출 API

```cpp
// 즉시 정지 (스킬 취소 시)
void CFxSimulationSystem::DeactivateInstance(EntityID e)
{
    auto* pFic = world.TryGetComponent<FxInstanceComponent>(e);
    if (!pFic || pFic->state != eFxLifecycleState::Active) return;

    m_vecInstances[pFic->uInstanceSlot].RequestComplete();
    pFic->state = eFxLifecycleState::Completing;
}

// 즉시 강제 종료 (Scene 전환 시)
void CFxSimulationSystem::KillInstance(EntityID e)
{
    auto* pFic = world.TryGetComponent<FxInstanceComponent>(e);
    if (!pFic) return;

    if (pFic->uInstanceSlot != FX_INVALID_INSTANCE)
    {
        m_vecInstances[pFic->uInstanceSlot].Reset();
        m_vecFreeSlots.push_back(pFic->uInstanceSlot);
    }
    pFic->state = eFxLifecycleState::PoolReturned;
}
```

---

## §2. Tick 순서 — Per-frame Execution Order (EFX-2)

### 2.1 Phase 순서 (CLAUDE.md §3 기준)

```
Phase 0    Manager/RHI 갱신
Phase 1    Core (Timer/Input)
Phase 2    Structure
Phase 3    Renderer 준비
Phase 4    Editor (ImGui)
Phase 5    ECS Vision (Track 4 B-13)
Phase 6    ECS Turret AI
Phase 7    ECS Spatial Hash
Phase 8    ECS BehaviorTree
Phase 9    ECS (예약)
Phase 10   ECS MCTS
Phase 11   ★ EFX FxSimulationSystem (신규)
Phase 12   Collision
Phase 13   AI / Other
```

**왜 Phase 11?**
- AI/BT/MCTS 후 → AI 가 결정한 action (skill cast 등) 의 effect spawn 결과를 같은 frame 에 반영
- Collision 이전 → effect 가 collision 결과 사용 X (collision 노드는 EFX-7+ 진입 시 Phase 12 후)
- Render 보다 충분히 앞 → frame 내 spawn → 즉시 render 가능

### 2.2 CFxSimulationSystem::Execute 내부 순서

```cpp
void CFxSimulationSystem::Execute(CWorld& world, f32_t fDeltaTime)
{
    // Step A: System.* parameter 갱신 (1회)
    auto& sysParams = world.GetFxParameterMap();
    sysParams.Set<f32_t>(eFxNamespace::System, "DeltaTime", fDeltaTime);
    sysParams.Set<f32_t>(eFxNamespace::System, "WorldTime",
                          sysParams.Get<f32_t>(eFxNamespace::System, "WorldTime", 0.f) + fDeltaTime);

    // Step B: Inactive → Active 전이 (Step 2 in §1.3)
    PromoteInactive(world);

    // Step C: Active 의 Tick (Step 3 in §1.3)
    TickActive(world, fDeltaTime);

    // Step D: Completing → Complete 검사 (Step 5)
    CheckCompletingToComplete(world);

    // Step E: Complete → PoolReturned cleanup (Step 6)
    CleanupComplete(world);

    // Step F: PoolReturned → Component 제거 (Step 7)
    RemovePoolReturned(world);
}
```

### 2.3 CFxSystemInstance::Tick 내부 순서

```cpp
void CFxSystemInstance::Tick(f32_t fDeltaTime,
                              const TransformComponent& tx,
                              CFxParameterMap& worldParams)
{
    if (m_State != eFxLifecycleState::Active &&
        m_State != eFxLifecycleState::Completing) return;

    // Step T1: Emitter.* parameter 갱신 (Position 추종)
    m_LocalParams.Set<Vec3>(eFxNamespace::Emitter, "Position", tx.vWorldPos);
    m_LocalParams.Set<Vec4>(eFxNamespace::Emitter, "Rotation", tx.qRotation.AsVec4());
    m_LocalParams.Set<f32_t>(eFxNamespace::Emitter, "LoopedAge", m_fAge);

    // Step T2: User.* parameter 동기 (worldParams → m_LocalParams)
    SyncUserParams(worldParams);

    // Step T3: 각 emitter tick (Spawn → Update → Cull 순서)
    for (auto& emitter : m_vecEmitters)
        emitter.Tick(fDeltaTime, m_LocalParams);

    m_fAge += fDeltaTime;
}
```

### 2.4 CFxEmitterInstance::Tick 내부 순서 (Spawn → Update → Cull)

```cpp
void CFxEmitterInstance::Tick(f32_t fDeltaTime, CFxParameterMap& sysParams)
{
    // Step E1: Spawn Phase
    if (m_bSpawnEnabled)
    {
        f32_t fSpawnRate = sysParams.Get<f32_t>(eFxNamespace::Emitter, "SpawnRate",
                                                  m_pDesc->spawnRate);
        m_fSpawnAccumulator += fSpawnRate * fDeltaTime;

        u32_t uSpawnCount = static_cast<u32_t>(m_fSpawnAccumulator);
        m_fSpawnAccumulator -= uSpawnCount;   // 분수 보존

        for (u32_t i = 0; i < uSpawnCount; ++i)
        {
            u32_t idx = m_Pool.Spawn();
            if (idx == FX_INVALID_PARTICLE) break;   // pool full

            // Spawn bytecode 실행 (EFX-5)
            m_VM.Execute(m_pDesc->spawnBytecode, sysParams, m_Pool, idx);
        }
    }

    // Step E2: Update Phase (모든 active particle)
    f32_t* pAge = m_Pool.GetAgeColumn();
    for (u32_t i = 0; i < m_Pool.GetActiveCount(); ++i)
    {
        // Update bytecode 실행
        m_VM.Execute(m_pDesc->updateBytecode, sysParams, m_Pool, i);

        pAge[i] += fDeltaTime;
    }

    // Step E3: Cull Phase (age >= lifetime swap-back kill)
    f32_t* pLifetime = m_Pool.GetLifetimeColumn();
    pAge = m_Pool.GetAgeColumn();   // 다시 가져옴 (vector 재할당 가능성)
    for (i32_t i = static_cast<i32_t>(m_Pool.GetActiveCount()) - 1; i >= 0; --i)
    {
        if (pAge[i] >= pLifetime[i])
        {
            m_Pool.KillSwapBack(static_cast<u32_t>(i));
        }
    }
}
```

**왜 Cull 이 Update 후?**
- Update 가 새 age 갱신 → 그 frame 의 마지막 visual update 가 보임 (트레일/페이드의 마지막 frame)
- Cull 이 Update 전이면 마지막 frame 의 visual 누락 → 시각 깜빡임

**왜 역순 iteration?**
- `KillSwapBack(i)` 가 i 번째 자리에 마지막 particle 을 옮김 → 정순 iteration 이면 마지막 particle 의 update 가 두 번 실행 (재방문)
- 역순 = 안전

---

## §3. Worker-Safe Spawn — CommandBuffer 통합

### 3.1 문제 — Fiber JobSystem 안에서 spawn

```cpp
// 기존 (worker thread 안전 X):
void CSomeJob::Execute(CWorld& world)
{
    if (HitDetected())
    {
        CFxSimulationSystem::Instance().SpawnFromAsset(world, hHitFx, vPos);
        // ★ world.CreateEntity() 가 thread-safe X — race
    }
}
```

### 3.2 해결 — CCommandBuffer 통합

```cpp
// CFxSimulationSystem 의 신규 API (EFX-2 박제):
void CFxSimulationSystem::DeferSpawnFromAsset(CCommandBuffer& cb,
                                               FxAssetHandle h,
                                               const Vec3& pos,
                                               EntityID attachTo)
{
    cb.Enqueue([h, pos, attachTo](CWorld& world)
    {
        CFxSimulationSystem::Instance().SpawnFromAsset(world, h, pos, attachTo);
    });
}
```

**worker thread 안의 호출**:
```cpp
void CSomeJob::Execute(CCommandBuffer& cb)
{
    if (HitDetected())
    {
        // ★ thread-safe — CommandBuffer 가 mutex 보호
        CFxSimulationSystem::DeferSpawnFromAsset(cb, hHitFx, vPos);
    }
}
```

**메인 thread 의 commit**:
```cpp
// Frame 끝에:
cb.Flush(world);   // 모든 enqueued lambda 실행 (메인 thread 안전)
```

### 3.3 Fiber JobSystem 진입 (NEXTGEN §1 4 후)

Phase 5-A → 5-B-pre 전환 후:
- `CSystemAccessBuilder` 가 SystemAccess 명시 (EFX-2 의 `DescribeAccess` 박제)
- Fiber M1 yield + WaitForCounter 안정 시 → Spawn 도 Fiber 안에서 가능
- 단 그래도 **CWorld 변경은 메인 thread + CommandBuffer 패턴** 유지 (P-19 회피)

---

## §4. Hot Reload Sequence Diagram (EFX-4)

### 4.1 외부 에디터 변경 → 시각 반영 (5 step)

```
[t=0]   VFX artist: notepad++ 로 .wfx 편집 + 저장
        파일 mtime: 2026-05-07 14:23:45.123

[t=0~0.5]
   ↓ (CScene_EffectTool::OnUpdate 매 frame 호출, 0.5s 누적 시 검사)

[t=0.5]
CScene_EffectTool::DetectAssetChanges()
   │
   ├─ for each h in m_vecLoadedAssets:
   │     auto path = registry.GetAssetPath(h);
   │     auto mtime = std::filesystem::last_write_time(path);
   │     if (mtime > m_AssetMTimes[h])
   │         ↓
   │
   ├─ CFxAssetRegistry::ReloadFromFile(h)
   │     │
   │     ├─ Slot* pSlot = ResolveSlot(h);
   │     ├─ LoadFromFile(pSlot->path);        // RegisterOrReplaceByName 같은 이름이면 덮어씀
   │     │     ↓
   │     │  Slot.asset = 새 FxAsset (generation 변경 X)
   │     │
   │     └─ return true
   │
   ├─ CFxSimulationSystem::OnAssetReloaded(h)
   │     │
   │     └─ for each instance in m_vecInstances:
   │            if (instance.GetAssetHandle() == h)
   │                ↓
   │            instance.Reset();              // 모든 particle kill, m_State = Inactive
   │            instance.Initialize(h, ...);   // 새 자산으로 재초기화
   │
   └─ m_AssetMTimes[h] = mtime;

[t=0.5+]
   ↓ (다음 Phase 11 Tick)

CFxSimulationSystem::Execute()
   ↓
   PromoteInactive() — instance.m_State 가 Inactive → Active 전이
   ↓
   TickActive() — 새 자산으로 spawn 시작

[t=0.5+ε]
   ↓ Render

화면에 새 자산 시각화 즉시 반영
```

### 4.2 Inspector Apply → 시각 반영 (4 step)

```
[t=0]   Inspector 패널: 사용자가 spawn_rate 슬라이더 60 → 120 변경
        ↓
        FxAsset* pMutableAsset = registry.FindMutable(hSelected);
        pMutableAsset->emitters[0].spawnRate = 120.f;

[t=0]   "Apply" 버튼 클릭
        ↓
        SaveAssetToWfx(pMutableAsset, registry.GetAssetPath(hSelected));   // .wfx 덮어쓰기
        ↓
        ReloadFromFile(hSelected);   // ★ §4.1 의 reload 흐름 재진입
        ↓
        OnAssetReloaded(hSelected);
        ↓
        시각 반영
```

### 4.3 Save 흐름 (Inspector / Asset Browser save)

```cpp
// Engine/Public/FX/FxAssetSerializer.h (EFX-1 후반 박제)
namespace FxAssetSerializer
{
    bool_t SaveToWfx(const FxAsset& asset, const wstring_t& path);
    bool_t LoadFromWfx(const wstring_t& path, FxAsset& outAsset);
}
```

**Save 본체**:
```cpp
bool_t FxAssetSerializer::SaveToWfx(const FxAsset& asset, const wstring_t& path)
{
    nlohmann::json j;
    j["version"] = "1.0";
    j["name"] = asset.strName;

    // user_params 직렬화
    for (auto& [key, val] : asset.initialUserParams.GetAllValues())
    {
        // FxValue (variant) → JSON
        std::visit([&](auto& v) { j["user_params"][key] = v; }, val);
    }

    // emitters 직렬화
    for (auto& em : asset.emitters)
    {
        nlohmann::json je;
        je["name"] = em.strName;
        je["render_type"] = RenderTypeToString(em.renderType);
        je["max_particles"] = em.maxParticles;
        je["spawn_rate"] = em.spawnRate;
        je["blend_mode"] = BlendPresetToString(em.blendMode);
        je["material_path"] = w2u8(em.strTexturePath);
        je["lifetime"] = em.fLifetime;
        // ... 모든 필드
        j["emitters"].push_back(std::move(je));
    }

    std::ofstream ofs(path);
    if (!ofs) return false;
    ofs << j.dump(2);
    return true;
}
```

---

## §5. Graph Runtime — Node Execution Flow (EFX-5)

### 5.1 컴파일 → 실행 분리

```
[디자이너 단계 — Editor]
    그래프 노드 추가/연결
         ↓
    CFxNodeCompiler::CompileEmitter()
         ↓
    spawnBytecode + updateBytecode (FxEmitterDesc 안 박제)
         ↓
    .wfx 저장

[런타임 단계 — Game]
    .wfx 로드
         ↓
    FxAsset 안의 bytecode 그대로 사용 (재컴파일 X)
         ↓
    CFxExpressionVM::Execute() 가 bytecode 실행
```

### 5.2 컴파일 (CFxNodeCompiler)

```cpp
bool_t CFxNodeCompiler::CompileEmitter(const FxEmitterDesc& emitter,
                                       std::vector<u8_t>& outSpawnBytecode,
                                       std::vector<u8_t>& outUpdateBytecode)
{
    // Step 1: 노드를 mode 별 분리
    std::vector<u32_t> spawnNodes;
    std::vector<u32_t> updateNodes;

    for (u32_t i = 0; i < emitter.nodes.size(); ++i)
    {
        const auto& node = emitter.nodes[i];
        if (IsSpawnNode(node.strType))   // "InitPosition", "InitColor", "InitVelocity*", ...
            spawnNodes.push_back(i);
        else if (IsUpdateNode(node.strType))   // "UpdateGravity", "UpdateLifetime", ...
            updateNodes.push_back(i);
    }

    // Step 2: 각 mode 별 위상 정렬
    std::vector<u32_t> spawnOrder, updateOrder;
    if (!TopologicalSort(emitter.nodes, spawnNodes, spawnOrder)) return false;
    if (!TopologicalSort(emitter.nodes, updateNodes, updateOrder)) return false;

    // Step 3: 각 노드를 bytecode chunk 로 변환
    outSpawnBytecode.clear();
    for (u32_t idx : spawnOrder)
        EmitNode(emitter.nodes[idx], outSpawnBytecode);
    EmitOp(outSpawnBytecode, eFxOp::Return);

    outUpdateBytecode.clear();
    for (u32_t idx : updateOrder)
        EmitNode(emitter.nodes[idx], outUpdateBytecode);
    EmitOp(outUpdateBytecode, eFxOp::Return);

    return true;
}
```

### 5.3 EmitNode — 노드 → bytecode

```cpp
void CFxNodeCompiler::EmitNode(const FxNodeDesc& node, std::vector<u8_t>& out)
{
    if (node.strType == "InitPositionAtPoint")
    {
        // inputs[0] = Vec3 (location)
        // outputs[0] = Particle.Position

        // bytecode:
        // PushConstVec3 <inputs[0] 값>
        // StoreParticleAttr Position
        EmitOp(out, eFxOp::PushConstVec3);
        EmitVec3(out, ParseVec3(node.inputs[0]));
        EmitOp(out, eFxOp::StoreParticleAttr);
        EmitU8(out, static_cast<u8_t>(eParticleAttr::Position));
    }
    else if (node.strType == "UpdateGravity")
    {
        // inputs[0] = Vec3 gravity (default {0, -9.8, 0})
        // affects Particle.Velocity

        // bytecode:
        // LoadParticleAttr Velocity
        // PushConstVec3 gravity
        // PushConstFloat dt
        // MulV3F             // gravity * dt
        // AddV3              // velocity + gravity*dt
        // StoreParticleAttr Velocity
        EmitOp(out, eFxOp::LoadParticleAttr);
        EmitU8(out, static_cast<u8_t>(eParticleAttr::Velocity));
        EmitOp(out, eFxOp::PushConstVec3);
        EmitVec3(out, ParseVec3(node.inputs[0]));
        EmitOp(out, eFxOp::LoadParam);
        EmitParamID(out, FxParameterID::Make(eFxNamespace::System, "DeltaTime", eFxParameterType::Float));
        EmitOp(out, eFxOp::MulV3F);
        EmitOp(out, eFxOp::AddV3);
        EmitOp(out, eFxOp::StoreParticleAttr);
        EmitU8(out, static_cast<u8_t>(eParticleAttr::Velocity));
    }
    // ... 30+ 노드
}
```

### 5.4 VM Execute (CFxExpressionVM)

```cpp
void CFxExpressionVM::Execute(const std::vector<u8_t>& bytecode,
                              CFxParameterMap& params,
                              CParticlePool& pool,
                              u32_t uParticleIdx)
{
    m_Stack.clear();
    m_PC = 0;

    while (m_PC < bytecode.size())
    {
        eFxOp op = static_cast<eFxOp>(bytecode[m_PC++]);

        switch (op)
        {
        case eFxOp::PushConstFloat:
        {
            f32_t v = ReadF32(bytecode, m_PC);
            m_PC += sizeof(f32_t);
            m_Stack.push_back(v);
            break;
        }
        case eFxOp::LoadParticleAttr:
        {
            u8_t attr = bytecode[m_PC++];
            FxValue v = LoadParticleAttrFromPool(pool, uParticleIdx, attr);
            m_Stack.push_back(std::move(v));
            break;
        }
        case eFxOp::AddV3:
        {
            Vec3 b = std::get<Vec3>(m_Stack.back()); m_Stack.pop_back();
            Vec3 a = std::get<Vec3>(m_Stack.back()); m_Stack.pop_back();
            m_Stack.push_back(a + b);
            break;
        }
        case eFxOp::Return:
            return;
        // ... 50+ opcode
        }
    }
}
```

### 5.5 위상 정렬 (Topological Sort)

DAG 검사 + 실행 순서 결정:

```cpp
bool_t CFxNodeCompiler::TopologicalSort(const std::vector<FxNodeDesc>& nodes,
                                        const std::vector<u32_t>& subset,
                                        std::vector<u32_t>& outOrder)
{
    // Kahn's algorithm
    std::unordered_map<u32_t, u32_t> inDegree;     // nodeIdx → 의존 개수
    std::unordered_map<u32_t, std::vector<u32_t>> dependents;

    // Edge 구성: node A 의 outputs[i] = "Particle.X" → node B 의 inputs[j] = "Particle.X" → A → B
    for (u32_t a : subset)
    for (u32_t b : subset)
    {
        if (a == b) continue;
        for (auto& outName : nodes[a].outputs)
        for (auto& inName  : nodes[b].inputs)
        {
            if (outName == inName)
            {
                ++inDegree[b];
                dependents[a].push_back(b);
            }
        }
    }

    std::queue<u32_t> readyQ;
    for (u32_t i : subset)
        if (inDegree[i] == 0)
            readyQ.push(i);

    outOrder.clear();
    while (!readyQ.empty())
    {
        u32_t cur = readyQ.front(); readyQ.pop();
        outOrder.push_back(cur);

        for (u32_t dep : dependents[cur])
        {
            if (--inDegree[dep] == 0)
                readyQ.push(dep);
        }
    }

    if (outOrder.size() != subset.size())
        return false;   // ★ 순환 있음 — 컴파일 실패

    return true;
}
```

**순환 검출 시**: Editor 가 Inspector 패널에 "Cyclic dependency: NodeA → NodeB → NodeA" 표시 + Apply 차단.

---

## §6. Render Snapshot — Sim 과 Render 분리 (EFX-3 후반)

### 6.1 RenderWorldSnapshot 패턴

PITFALLS P-19 (Render/Sim 결합) 회피 — Render 패스가 ECS World 직접 의존 X.

```cpp
// CFxSimulationSystem 의 BuildRenderSnapshot
struct FxRenderSnapshot
{
    std::vector<FxBillboardComponent>      billboards;
    std::vector<FxRibbonRenderState>       ribbons;       // VB 데이터 ready
    std::vector<FxBeamRenderState>         beams;
    std::vector<FxGroundDecalComponent>    decals;
    std::vector<FxMeshParticleRenderState> meshParticles;
    std::vector<FxShockwaveComponent>      shockwaves;
};

void CFxSimulationSystem::BuildRenderSnapshot(FxRenderSnapshot& out) const
{
    out.billboards.clear();
    // 모든 active instance 의 billboard particle 들을 vector 에 push
    for (const auto& inst : m_vecInstances)
    {
        if (!inst.IsActive()) continue;
        for (const auto& emitter : inst.GetEmitters())
        {
            if (emitter.GetDesc().renderType != eFxRenderType::Billboard) continue;
            const CParticlePool& pool = emitter.GetPool();
            // SoA → AoS 변환 (Render 측은 AoS 가 편함)
            for (u32_t i = 0; i < pool.GetActiveCount(); ++i)
            {
                FxBillboardComponent rs{};
                rs.vWorldPos = pool.GetPosColumn()[i];
                rs.vColor = pool.GetColorColumn()[i];
                // ... 등 변환
                out.billboards.push_back(rs);
            }
        }
    }
    // ribbon / beam / decal / mesh / shockwave 도 동일 패턴
}
```

### 6.2 Render Pass

```cpp
// Renderer 측 (FxSystem.cpp 등)
void CFxSystem::Render(const FxRenderSnapshot& snapshot, const CDynamicCamera& cam)
{
    // ECS World 직접 의존 X — snapshot 만 read
    Mat4 viewProj = cam.GetViewProj();

    // 1. Decal 먼저 (depth write off, 지면 위)
    m_DecalRenderer.Render(snapshot.decals, viewProj);

    // 2. Mesh particle (depth write on)
    m_MeshParticleRenderer.Render(snapshot.meshParticles, viewProj);

    // 3. Billboard / Ribbon / Beam (depth write off, additive/alpha)
    m_BillboardRenderer.Render(snapshot.billboards, viewProj);
    m_RibbonRenderer.Render(snapshot.ribbons, viewProj);
    m_BeamRenderer.Render(snapshot.beams, viewProj);

    // 4. Shockwave (마지막 — 알파 누적 회피)
    m_ShockwaveRenderer.Render(snapshot.shockwaves, viewProj);
}
```

### 6.3 EFX-2 시점 단순화

EFX-2 에선 BuildRenderSnapshot 박제 보류 가능 — `FxSystem::Render` 가 직접 `world.ForEach<FxBillboardComponent>` 사용 (현 패턴 유지). EFX-3 진입 시 Snapshot 패턴 박제 + RenderGraph 통합 (NEXTGEN §1 5 후).

---

## §7. Determinism 보장

### 7.1 Deterministic Effect (판정 FX)

**용도**: 서버/클라 일치 검증 필요 — Class & Servant 의 PvPvE 권위 검증.

**박제**:
```cpp
// FxAsset.h 신규 필드 (v3 박제 예정)
struct FxAsset
{
    // ...
    bool_t bDeterministic = false;   // ★ 신규
};
```

```cpp
// CFxSystemInstance::Initialize
void CFxSystemInstance::Initialize(FxAssetHandle h, EntityID owner, u64_t rngSeed)
{
    const FxAsset* pAsset = CFxAssetRegistry::Instance().Find(h);
    m_hAsset = h;
    m_OwnerEntity = owner;

    if (pAsset->bDeterministic)
    {
        // ★ deterministic seed = (network frame # × prime) ^ owner entity
        m_Rng.Seed(ComputeDeterministicSeed(owner));
    }
    else
    {
        m_Rng.Seed(rngSeed);   // 시각용 — 클라마다 다름 OK
    }

    // ...
}
```

### 7.2 Visual-only Effect (먼지/트레일)

**용도**: 시각 효과만, 클라마다 달라도 무관 — 성능 우선.

**박제**: 기본값 `bDeterministic = false`. RNG seed = global counter (매번 다름).

### 7.3 검증 시나리오

```
[Server simulation]
- frame N, owner=42, asset bDeterministic=true
- Spawn → seed = (N × 0x9E3779B9) ^ 42
- Particle Position 결과 = deterministic_pos[N]

[Client simulation]
- 동일 frame N, 동일 owner
- 동일 seed → 동일 Position 결과

[검증]
- Server snapshot 의 hash == Client snapshot 의 hash → OK
```

---

## §8. 디버깅 / 진단

### 8.1 Lifecycle 디버그 표시

```cpp
// EFX-4 Inspector 패널 신규 섹션:
void CInspectorPanel::DrawLifecycleDebug(EntityID e, const FxInstanceComponent& fic)
{
    ImGui::Text("State: %s", LifecycleStateToString(fic.state));
    ImGui::Text("Slot: %u", fic.uInstanceSlot);
    ImGui::Text("Age: %.2f / %.2f", fic.fAge, fic.fLifetime);
    ImGui::Text("Loop: %s", fic.bLoop ? "true" : "false");

    if (fic.uInstanceSlot != FX_INVALID_INSTANCE)
    {
        const auto& inst = CFxSimulationSystem::Instance().GetInstance(fic.uInstanceSlot);
        ImGui::Text("Active particles: %u", inst.GetTotalActiveParticleCount());

        for (u32_t i = 0; i < inst.GetEmitterCount(); ++i)
        {
            const auto& em = inst.GetEmitter(i);
            ImGui::BulletText("Emitter %u: %u/%u particles", i,
                              em.GetPool().GetActiveCount(),
                              em.GetPool().GetCapacity());
        }
    }
}
```

### 8.2 Tick 시간 측정

```cpp
// CFxSimulationSystem::Execute 안에 Profiler scope
{
    PROFILER_SCOPE("FxSimulation::PromoteInactive");
    PromoteInactive(world);
}
{
    PROFILER_SCOPE("FxSimulation::TickActive");
    TickActive(world, fDeltaTime);
}
// ...
```

### 8.3 Snapshot 검증 (Hot Reload 후)

```cpp
// Hot reload 후 시각적으로 동일한지 검증:
void CScene_EffectTool::ValidateAfterReload(FxAssetHandle h)
{
    // Reload 직전 snapshot 저장
    FxRenderSnapshot before;
    CFxSimulationSystem::Instance().BuildRenderSnapshot(before);

    // Reload
    ReloadFromFile(h);

    // 즉시 비교 (deterministic seed 같으면 결과 동일)
    FxRenderSnapshot after;
    CFxSimulationSystem::Instance().BuildRenderSnapshot(after);

    // particle count 비교, position diff 등
    // 차이 있으면 WARN log
}
```

---

## §9. 합격 기준 (15번 문서 종합)

- [ ] **5 state enum** 박제 (`FxLifecycle.h`)
- [ ] **8 단계 라이프사이클** 박제 + 각 단계 invariant 검증
- [ ] **Phase 11 Tick 순서** (PromoteInactive → TickActive → CheckCompleting → CleanupComplete → RemovePoolReturned)
- [ ] **Spawn → Update → Cull** 매 emitter Tick 순서 준수
- [ ] **CommandBuffer Worker-Safe Spawn** 박제 + Fiber JobSystem 안 race 0
- [ ] **Hot Reload sequence** (.wfx 변경 → 0.5s 검출 → ReloadFromFile → OnAssetReloaded → 즉시 시각 반영)
- [ ] **Topological sort** + 순환 검출 + Apply 차단
- [ ] **Render Snapshot 패턴** (Sim/Render 분리) — EFX-3 진입 후
- [ ] **Determinism flag** (bDeterministic) 박제 + RNG seed 분기
- [ ] **Lifecycle 디버그 패널** Inspector 안 박제

---

**END OF EFX LIFECYCLE AND GRAPH RUNTIME**
