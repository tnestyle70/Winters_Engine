Session - 야스오 AI 판단 근거와 실행 명령을 Snapshot/DebugDraw/AIDebugPanel로 노출한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h

`struct ChampionAIComponent` 안의 아래 기존 코드 바로 아래에 추가:

```cpp
	u32_t debugAvailableActionMask = 0;
	u32_t debugAvailableSkillMask = 0;
```

아래에 추가:

```cpp
	u8_t debugLastCommandKind = 0;
	u8_t debugLastCommandSlot = 0;
	EntityID debugLastCommandTarget = NULL_ENTITY;
	u8_t debugYasuoQStage = 1u;
	bool_t bDebugYasuoEActive = false;
	EntityID debugYasuoAirborneTarget = NULL_ENTITY;
	EntityID debugYasuoDashMinion = NULL_ENTITY;
```

`struct ChampionAIDebugComponent` 안의 아래 기존 코드 바로 아래에 추가:

```cpp
	u32_t availableActionMask = 0;
	u32_t availableSkillMask = 0;
```

아래에 추가:

```cpp
	u8_t lastCommandKind = 0;
	u8_t lastCommandSlot = 0;
	u32_t lastCommandTargetNetId = 0;
	u8_t yasuoQStage = 1u;
	bool_t bYasuoEActive = false;
	u32_t yasuoAirborneTargetNetId = 0;
	u32_t yasuoDashMinionNetId = 0;
```

`struct ChampionAIDebugComponent` 안의 아래 기존 코드 바로 아래에 추가:

```cpp
	f32_t fAttackRange = 1.5f;
```

아래에 추가:

```cpp
	f32_t fChampionScanRange = 0.f;
	f32_t fMinionScanRange = 0.f;
	f32_t fStructureScanRange = 0.f;
	f32_t fLeashRange = 0.f;
```

목적:
- 서버 AI가 마지막으로 발행한 명령과 야스오 전용 판단 재료를 클라이언트 Debug UI에서 볼 수 있게 한다.

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

`CommandName(...)` 함수 바로 아래에 추가:

```cpp
    void RecordChampionAICommandDebug(
        ChampionAIComponent& ai,
        EntityID target,
        eCommandKind kind,
        u8_t slot)
    {
        ai.debugLastCommandKind = static_cast<u8_t>(kind);
        ai.debugLastCommandSlot = slot;
        ai.debugLastCommandTarget = target;
    }
```

`EmitMoveCommand(...)` 안의 아래 기존 코드 바로 아래에 추가:

```cpp
        outCommands.push_back(move);
```

아래에 추가:

```cpp
        RecordChampionAICommandDebug(ai, NULL_ENTITY, move.kind, move.slot);
```

`EmitBasicAttackCommand(...)` 안의 아래 기존 코드 바로 아래에 추가:

```cpp
        outCommands.push_back(cmd);
```

아래에 추가:

```cpp
        RecordChampionAICommandDebug(ai, target, cmd.kind, cmd.slot);
```

`EmitSkillCommand(...)` 안의 아래 기존 코드 바로 아래에 추가:

```cpp
        outCommands.push_back(cmd);
```

아래에 추가:

```cpp
        RecordChampionAICommandDebug(ai, target, cmd.kind, cmd.slot);
```

`EmitRecall(...)` 안의 아래 기존 코드 바로 아래에 추가:

```cpp
        outCommands.push_back(recall);
```

아래에 추가:

```cpp
        RecordChampionAICommandDebug(ai, NULL_ENTITY, recall.kind, recall.slot);
```

`BuildChampionAIContext(...)` 안의 야스오 context 수집 블록 끝, 아래 기존 코드 바로 위에 추가:

```cpp
        return ctx;
```

아래에 추가:

```cpp
        ai.debugYasuoQStage = ctx.yasuoQStage;
        ai.bDebugYasuoEActive = ctx.bYasuoEActive;
        ai.debugYasuoAirborneTarget = ctx.airborneChampion;
        ai.debugYasuoDashMinion = ctx.yasuoDashMinion;
```

주의:
- 위 추가 위치는 `return ctx;` 바로 위여야 한다.
- 야스오가 아닌 챔피언은 기본값이 유지된다.

1-3. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Snapshot.fbs

`table EntitySnapshot` 안의 아래 기존 코드 바로 아래에 추가:

```fbs
    abilityHaste:float;
```

아래에 추가:

```fbs
    aiDebugFlags:uint;
    aiDebugChampionScore:float;
    aiDebugFarmScore:float;
    aiDebugStructureScore:float;
    aiDebugSelfHpRatio:float;
    aiDebugEnemyHpRatio:float;
    aiDebugEnemyDistance:float;
    aiDebugAttackRange:float;
    aiDebugTurretDanger:float;
    aiDebugPostComboBATimer:float;
    aiDebugChampionScanRange:float;
    aiDebugMinionScanRange:float;
    aiDebugStructureScanRange:float;
    aiDebugLeashRange:float;
    aiDebugLastCommandKind:ubyte;
    aiDebugLastCommandSlot:ubyte;
    aiDebugLastCommandTargetNet:uint;
    aiDebugYasuoQStage:ubyte;
    aiDebugYasuoEActive:bool;
    aiDebugYasuoAirborneTargetNet:uint;
    aiDebugYasuoDashMinionNet:uint;
```

목적:
- `stateFlags` packing을 더 압박하지 않고 AI 판단 근거를 명시 필드로 내린다.

1-4. C:/Users/tnest/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

`if (world.HasComponent<ChampionAIComponent>(entity))` 블록 바로 위에 추가:

```cpp
        u32_t aiDebugFlags = 0u;
        f32_t aiDebugChampionScore = 0.f;
        f32_t aiDebugFarmScore = 0.f;
        f32_t aiDebugStructureScore = 0.f;
        f32_t aiDebugSelfHpRatio = 1.f;
        f32_t aiDebugEnemyHpRatio = 1.f;
        f32_t aiDebugEnemyDistance = 999.f;
        f32_t aiDebugAttackRange = 1.5f;
        f32_t aiDebugTurretDanger = 0.f;
        f32_t aiDebugPostComboBATimer = 0.f;
        f32_t aiDebugChampionScanRange = 0.f;
        f32_t aiDebugMinionScanRange = 0.f;
        f32_t aiDebugStructureScanRange = 0.f;
        f32_t aiDebugLeashRange = 0.f;
        u8_t aiDebugLastCommandKind = 0u;
        u8_t aiDebugLastCommandSlot = 0u;
        u32_t aiDebugLastCommandTargetNet = 0u;
        u8_t aiDebugYasuoQStage = 1u;
        bool_t aiDebugYasuoEActive = false;
        u32_t aiDebugYasuoAirborneTargetNet = 0u;
        u32_t aiDebugYasuoDashMinionNet = 0u;
```

`if (world.HasComponent<ChampionAIComponent>(entity))` 블록 안의 target 계산 아래에 추가:

```cpp
            aiDebugChampionScore = ai.fChampionDecisionScore;
            aiDebugFarmScore = ai.fFarmDecisionScore;
            aiDebugStructureScore = ai.fStructureDecisionScore;
            aiDebugSelfHpRatio = ai.fDecisionSelfHpRatio;
            aiDebugEnemyHpRatio = ai.fDecisionEnemyHpRatio;
            aiDebugEnemyDistance = ai.fDecisionEnemyDistance;
            aiDebugAttackRange = ai.fDecisionAttackRange;
            aiDebugTurretDanger = ai.fDecisionTurretDanger;
            aiDebugPostComboBATimer = ai.fPostComboBATimer;
            aiDebugChampionScanRange = ai.championScanRange;
            aiDebugMinionScanRange = ai.minionScanRange;
            aiDebugStructureScanRange = ai.structureScanRange;
            aiDebugLeashRange = ai.leashRange;
            aiDebugLastCommandKind = ai.debugLastCommandKind;
            aiDebugLastCommandSlot = ai.debugLastCommandSlot;
            aiDebugLastCommandTargetNet = entityMap.ToNet(ai.debugLastCommandTarget);
            aiDebugYasuoQStage = ai.debugYasuoQStage;
            aiDebugYasuoEActive = ai.bDebugYasuoEActive;
            aiDebugYasuoAirborneTargetNet = entityMap.ToNet(ai.debugYasuoAirborneTarget);
            aiDebugYasuoDashMinionNet = entityMap.ToNet(ai.debugYasuoDashMinion);
```

`Shared::Schema::CreateEntitySnapshot(...)` 호출 인자 끝에 아래 필드들을 추가:

```cpp
                aiDebugFlags,
                aiDebugChampionScore,
                aiDebugFarmScore,
                aiDebugStructureScore,
                aiDebugSelfHpRatio,
                aiDebugEnemyHpRatio,
                aiDebugEnemyDistance,
                aiDebugAttackRange,
                aiDebugTurretDanger,
                aiDebugPostComboBATimer,
                aiDebugChampionScanRange,
                aiDebugMinionScanRange,
                aiDebugStructureScanRange,
                aiDebugLeashRange,
                aiDebugLastCommandKind,
                aiDebugLastCommandSlot,
                aiDebugLastCommandTargetNet,
                aiDebugYasuoQStage,
                aiDebugYasuoEActive,
                aiDebugYasuoAirborneTargetNet,
                aiDebugYasuoDashMinionNet
```

확인 필요:
- `Snapshot.fbs` 변경 후 `Shared/Schemas/run_codegen.bat`를 먼저 실행하고, 생성된 `CreateEntitySnapshot` 인자 순서에 맞춰 정확히 붙인다.

1-5. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

`auto& debug = world.GetComponent<ChampionAIDebugComponent>(e);` 블록 안의 아래 기존 코드 바로 아래에 추가:

```cpp
                debug.availableSkillMask =
                    (es->stateFlags() & kChampionAIAvailableSkillMask) >> kChampionAIAvailableSkillShift;
```

아래에 추가:

```cpp
                debug.bCanAttackChampion = es->aiDebugChampionScore() > 0.f;
                debug.bPostComboBAAllowed = es->aiDebugPostComboBATimer() > 0.f;
                debug.fChampionDecisionScore = es->aiDebugChampionScore();
                debug.fFarmDecisionScore = es->aiDebugFarmScore();
                debug.fStructureDecisionScore = es->aiDebugStructureScore();
                debug.fSelfHpRatio = es->aiDebugSelfHpRatio();
                debug.fEnemyHpRatio = es->aiDebugEnemyHpRatio();
                debug.fEnemyDistance = es->aiDebugEnemyDistance();
                debug.fAttackRange = es->aiDebugAttackRange();
                debug.fTurretDanger = es->aiDebugTurretDanger();
                debug.fPostComboBATimer = es->aiDebugPostComboBATimer();
                debug.fChampionScanRange = es->aiDebugChampionScanRange();
                debug.fMinionScanRange = es->aiDebugMinionScanRange();
                debug.fStructureScanRange = es->aiDebugStructureScanRange();
                debug.fLeashRange = es->aiDebugLeashRange();
                debug.lastCommandKind = es->aiDebugLastCommandKind();
                debug.lastCommandSlot = es->aiDebugLastCommandSlot();
                debug.lastCommandTargetNetId = es->aiDebugLastCommandTargetNet();
                debug.yasuoQStage = es->aiDebugYasuoQStage();
                debug.bYasuoEActive = es->aiDebugYasuoEActive();
                debug.yasuoAirborneTargetNetId = es->aiDebugYasuoAirborneTargetNet();
                debug.yasuoDashMinionNetId = es->aiDebugYasuoDashMinionNet();
```

1-6. C:/Users/tnest/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

`m_bDbgShowMinionMovement` 바로 아래에 추가:

```cpp
    bool_t m_bDbgShowChampionAIText = true;
    bool_t m_bDbgShowChampionAIRanges = true;
```

public getter/setter 영역의 debug draw getter/setter 묶음에 추가:

```cpp
    bool_t IsDbgShowChampionAIText() const { return m_bDbgShowChampionAIText; }
    void SetDbgShowChampionAIText(bool_t b) { m_bDbgShowChampionAIText = b; }
    bool_t IsDbgShowChampionAIRanges() const { return m_bDbgShowChampionAIRanges; }
    void SetDbgShowChampionAIRanges(bool_t b) { m_bDbgShowChampionAIRanges = b; }
```

1-7. C:/Users/tnest/Desktop/Winters/Client/Private/UI/RenderDebug.cpp

아래 기존 코드 바로 아래에 추가:

```cpp
            bool bm = pScene->IsDbgShowMinionMovement();
            if (ImGui::Checkbox("Minion cells / move vectors", &bm)) pScene->SetDbgShowMinionMovement(bm);
```

아래에 추가:

```cpp
            bool ba = pScene->IsDbgShowChampionAIText();
            if (ImGui::Checkbox("Champion AI text", &ba)) pScene->SetDbgShowChampionAIText(ba);

            bool br = pScene->IsDbgShowChampionAIRanges();
            if (ImGui::Checkbox("Champion AI ranges", &br)) pScene->SetDbgShowChampionAIRanges(br);
```

`All On` 버튼 처리 아래에 추가:

```cpp
            pScene->SetDbgShowChampionAIText(true);
            pScene->SetDbgShowChampionAIRanges(true);
```

확인 필요:
- `All Off` 버튼이 같은 파일 아래에 있으면 거기에도 `false` 두 줄을 추가한다.

1-8. C:/Users/tnest/Desktop/Winters/Client/Public/UI/DebugDrawSystem.h

아래 기존 코드 바로 아래에 추가:

```cpp
        static void DrawMinionMovement(CWorld& w, CScene_InGame* s, ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
```

아래에 추가:

```cpp
        static void DrawChampionAIDebug(CWorld& w, CScene_InGame* s, ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
```

1-9. C:/Users/tnest/Desktop/Winters/Client/Private/UI/DebugDrawSystem.cpp

파일 상단 include 묶음에 추가:

```cpp
#include "Shared/GameSim/Components/ChampionAIComponent.h"
```

`CDebugDrawSystem::Render(...)` 안의 아래 기존 코드 바로 아래에 추가:

```cpp
        if (pScene->IsDbgShowMinionMovement()) DrawMinionMovement(world, pScene, pDraw, mVP);
```

아래에 추가:

```cpp
        if (pScene->IsDbgShowChampionAIText() || pScene->IsDbgShowChampionAIRanges())
            DrawChampionAIDebug(world, pScene, pDraw, mVP);
```

`DrawMinionMovement(...)` 함수 끝 바로 아래에 추가:

```cpp
    void CDebugDrawSystem::DrawChampionAIDebug(CWorld& w, CScene_InGame* s, ImDrawList* pDraw, const DirectX::XMMATRIX& mVP)
    {
        w.ForEach<ChampionAIDebugComponent, ChampionComponent, TransformComponent>(
            [&](EntityID, ChampionAIDebugComponent& debug, ChampionComponent& champion, TransformComponent& tf)
            {
                if (!debug.bPresent)
                    return;

                const Vec3 pos = tf.GetPosition();

                if (s->IsDbgShowChampionAIRanges())
                {
                    DrawWireCylinder(pDraw, mVP, pos, debug.fChampionScanRange, 0.05f, 0x8046A0FFu, 48);
                    DrawWireCylinder(pDraw, mVP, Vec3{ pos.x, pos.y + 0.06f, pos.z }, debug.fLeashRange, 0.05f, 0x808080FFu, 48);
                    DrawWireCylinder(pDraw, mVP, Vec3{ pos.x, pos.y + 0.12f, pos.z }, debug.fAttackRange, 0.05f, 0x8000FF80u, 32);
                }

                if (!s->IsDbgShowChampionAIText())
                    return;

                ImVec2 labelPos{};
                if (!WorldToScreen(mVP, Vec3{ pos.x + 1.6f, pos.y + 2.6f, pos.z }, labelPos))
                    return;

                char label[256]{};
                if (champion.id == eChampion::YASUO)
                {
                    sprintf_s(
                        label,
                        "AI %u I=%u A=%u cmd=%u/%u tgt=%u\nC/F/S %.2f/%.2f/%.2f hp %.2f/%.2f d=%.1f\nY q=%u e=%u air=%u dashM=%u",
                        debug.netId,
                        static_cast<u32_t>(debug.intent),
                        static_cast<u32_t>(debug.action),
                        static_cast<u32_t>(debug.lastCommandKind),
                        static_cast<u32_t>(debug.lastCommandSlot),
                        debug.lastCommandTargetNetId,
                        debug.fChampionDecisionScore,
                        debug.fFarmDecisionScore,
                        debug.fStructureDecisionScore,
                        debug.fSelfHpRatio,
                        debug.fEnemyHpRatio,
                        debug.fEnemyDistance,
                        static_cast<u32_t>(debug.yasuoQStage),
                        static_cast<u32_t>(debug.bYasuoEActive ? 1u : 0u),
                        debug.yasuoAirborneTargetNetId,
                        debug.yasuoDashMinionNetId);
                }
                else
                {
                    sprintf_s(
                        label,
                        "AI %u I=%u A=%u cmd=%u/%u tgt=%u\nC/F/S %.2f/%.2f/%.2f hp %.2f/%.2f d=%.1f",
                        debug.netId,
                        static_cast<u32_t>(debug.intent),
                        static_cast<u32_t>(debug.action),
                        static_cast<u32_t>(debug.lastCommandKind),
                        static_cast<u32_t>(debug.lastCommandSlot),
                        debug.lastCommandTargetNetId,
                        debug.fChampionDecisionScore,
                        debug.fFarmDecisionScore,
                        debug.fStructureDecisionScore,
                        debug.fSelfHpRatio,
                        debug.fEnemyHpRatio,
                        debug.fEnemyDistance);
                }

                pDraw->AddText(labelPos, 0xFFFFFFFFu, label);
            });
    }
```

1-10. C:/Users/tnest/Desktop/Winters/Client/Private/UI/AIDebugPanel.cpp

선택된 AI 상세 영역에서 아래 기존 코드 근처에 추가:

```cpp
		ImGui::Text("State / Intent / Action: %s / %s / %s",
```

아래에 추가:

```cpp
        ImGui::SeparatorText("Server Evidence");
        ImGui::Text("Scores C/F/S: %.2f / %.2f / %.2f",
            debug.fChampionDecisionScore,
            debug.fFarmDecisionScore,
            debug.fStructureDecisionScore);
        ImGui::Text("HP self/enemy: %.2f / %.2f  enemyDist=%.2f",
            debug.fSelfHpRatio,
            debug.fEnemyHpRatio,
            debug.fEnemyDistance);
        ImGui::Text("Ranges scan/minion/structure/leash/attack: %.1f / %.1f / %.1f / %.1f / %.1f",
            debug.fChampionScanRange,
            debug.fMinionScanRange,
            debug.fStructureScanRange,
            debug.fLeashRange,
            debug.fAttackRange);
        ImGui::Text("Last cmd kind/slot/target: %u / %u / %u",
            static_cast<u32_t>(debug.lastCommandKind),
            static_cast<u32_t>(debug.lastCommandSlot),
            debug.lastCommandTargetNetId);
        if (selectedAI.champion.id == eChampion::YASUO)
        {
            ImGui::Text("Yasuo qStage=%u eActive=%u airborne=%u dashMinion=%u",
                static_cast<u32_t>(debug.yasuoQStage),
                static_cast<u32_t>(debug.bYasuoEActive ? 1u : 0u),
                debug.yasuoAirborneTargetNetId,
                debug.yasuoDashMinionNetId);
        }
```

2. 검증

정적 확인:
- `Shared/Schemas/run_codegen.bat`
- `rg -n "aiDebugYasuoQStage|aiDebugLastCommandKind|aiDebugChampionScanRange" Shared/Schemas/Generated/cpp/Snapshot_generated.h`
- `rg -n "RecordChampionAICommandDebug|debugYasuoQStage|debugLastCommandKind" Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp Shared/GameSim/Components/ChampionAIComponent.h`
- `rg -n "DrawChampionAIDebug|Champion AI text|Champion AI ranges" Client/Private/UI Client/Public/UI Client/Public/Scene`
- `git diff --check`

빌드:
- `"C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" Winters.sln /p:Configuration=Debug /p:Platform=x64 /m`

런타임 확인:
- Render Debug master가 켜진 상태에서 Champion AI text/ranges 토글을 확인한다.
- 야스오 적 봇 오른쪽에 intent/action/last command/Q stage/E active/airborne/dash minion이 표시되는지 확인한다.
- 감지 범위 원이 BA 사거리와 구분되어 보이는지 확인한다.
- 야스오가 `yasuo-airborne-r`, `yasuo-e-engage-champion`, `yasuo-e-minion-gapclose`, `yasuo-q-farm-minion` 명령을 낼 때 텍스트의 last command가 바뀌는지 확인한다.

다음 후속 후보:
- 같은 대상 E 재사용 제한은 위 디버그로 반복 문제가 확인된 뒤 `YasuoGameSim` 서버 상태로 추가한다.
- 튜닝 슬라이더는 `SliderFloat` 매 프레임 전송 대신 release/apply 방식으로 구현한다.
