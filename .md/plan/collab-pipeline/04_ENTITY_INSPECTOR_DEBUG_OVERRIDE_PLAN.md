Session - 선택 엔티티의 ECS component를 읽어 보여주는 generic Entity Inspector를 만들고, 편집은 client mutation이 아니라 server debug override 경로로만 흐르게 한다.

1. 반영해야 하는 코드

설계 원칙(북극성 2 + 16 시리즈 S18):
- 인스펙터는 기본 read-only다. component 값을 "보는" 도구다.
- 값 편집은 client가 직접 mutate하지 않고 server debug override command를 보낸다(이 세션에서는 read-only 표시까지 구현, 편집 경로는 아래 "후속"에 고정).
- `Client/Private/Scene/Scene_Editor.cpp`의 Hierarchy/Inspector 패턴을 generic ECS 인스펙터로 일반화한다.

1-1. C:/Users/tnest/Desktop/Winters/Client/Private/UI/EntityInspectorPanel.h

새 파일:

```cpp
#pragma once

#include "ECS/Entity.h"

class CWorld;

// 선택 엔티티의 component를 ImGui로 표시한다(read-only). 편집은 server override 경로(후속).
void DrawEntityInspector(CWorld& world);
```

1-2. C:/Users/tnest/Desktop/Winters/Client/Private/UI/EntityInspectorPanel.cpp

`World.h`의 `ForEach<ChampionComponent>`로 챔피언 목록을 만들고, 선택 엔티티의 component를 read-only로 표시한다. 다른 디버그 패널(`CombatDebugPanel` 등)과 같은 free-function 스타일.

새 파일:

```cpp
#include "UI/EntityInspectorPanel.h"

#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/GoldComponent.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

#include <vector>

namespace
{
    struct InspectableEntity
    {
        EntityID id;
        char label[32];
    };

    void DrawReadOnlyFloat(const char* name, f32_t value)
    {
        ImGui::BeginDisabled();
        ImGui::DragFloat(name, &value);
        ImGui::EndDisabled();
    }
}

void DrawEntityInspector(CWorld& world)
{
    ImGui::Begin("Entity Inspector");

    static EntityID s_selected = NULL_ENTITY;

    std::vector<InspectableEntity> entities;
    world.ForEach<ChampionComponent>([&](EntityID e, ChampionComponent& champ)
    {
        InspectableEntity item{};
        item.id = e;
        std::snprintf(item.label, sizeof(item.label), "Champion %u (id %d)",
            static_cast<unsigned>(e), static_cast<int>(champ.id));
        entities.push_back(item);
    });

    const char* preview = "(none)";
    for (const InspectableEntity& it : entities)
        if (it.id == s_selected)
            preview = it.label;

    if (ImGui::BeginCombo("Entity", preview))
    {
        for (const InspectableEntity& it : entities)
            if (ImGui::Selectable(it.label, it.id == s_selected))
                s_selected = it.id;
        ImGui::EndCombo();
    }

    if (s_selected == NULL_ENTITY || !world.IsAlive(s_selected))
    {
        ImGui::TextUnformatted("선택된 엔티티 없음");
        ImGui::End();
        return;
    }

    if (TransformComponent* tf = world.TryGetComponent<TransformComponent>(s_selected))
    {
        if (ImGui::CollapsingHeader("Transform"))
        {
            const Vec3 pos = tf->GetPosition();
            DrawReadOnlyFloat("Pos X", pos.x);
            DrawReadOnlyFloat("Pos Y", pos.y);
            DrawReadOnlyFloat("Pos Z", pos.z);
        }
    }
    if (HealthComponent* h = world.TryGetComponent<HealthComponent>(s_selected))
    {
        if (ImGui::CollapsingHeader("Health"))
        {
            DrawReadOnlyFloat("Current", h->fCurrent);
            DrawReadOnlyFloat("Maximum", h->fMaximum);
            ImGui::Text("Dead: %s", h->bIsDead ? "true" : "false");
        }
    }
    if (ChampionComponent* c = world.TryGetComponent<ChampionComponent>(s_selected))
    {
        if (ImGui::CollapsingHeader("Champion"))
        {
            ImGui::Text("Level: %u", static_cast<unsigned>(c->level));
            DrawReadOnlyFloat("Move Speed", c->moveSpeed);
            DrawReadOnlyFloat("Mana", c->mana);
        }
    }
    if (StatComponent* s = world.TryGetComponent<StatComponent>(s_selected))
    {
        if (ImGui::CollapsingHeader("Stat"))
        {
            DrawReadOnlyFloat("AD", s->ad);
            DrawReadOnlyFloat("Armor", s->armor);
            DrawReadOnlyFloat("MR", s->mr);
            DrawReadOnlyFloat("Attack Speed", s->attackSpeed);
            DrawReadOnlyFloat("Attack Range", s->attackRange);
        }
    }
    if (GoldComponent* g = world.TryGetComponent<GoldComponent>(s_selected))
    {
        if (ImGui::CollapsingHeader("Gold"))
            ImGui::Text("Gold: %u", static_cast<unsigned>(g->amount));
    }

    ImGui::End();
}
```

확인 필요:
- `ChampionComponent`/`StatComponent`/`GoldComponent` 필드명을 실제 헤더와 대조해 교정(`ad`, `armor`, `mr`, `attackSpeed`, `attackRange`, `amount`).
- `TransformComponent::GetPosition()` 시그니처가 `Scene_Editor.cpp`에서 쓰는 것과 같은지 확인.
- `NULL_ENTITY` 상수가 `ECS/Entity.h`에 있는지 확인(World.h에서 사용 중).
- `Vec3` 필드 접근(`pos.x/y/z`)이 맞는지 확인.

1-3. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 디버그 패널을 그리는 `OnImGui` 경로에서 인스펙터를 호출한다.

기존 디버그 패널 호출 블록(예: `DrawCombatDebugPanel(...)` 등) 근처에 아래를 추가한다(정확한 anchor는 구현 직전 확인):

```cpp
    DrawEntityInspector(*m_pWorld);
```

그리고 파일 상단 include에 추가:

```cpp
#include "UI/EntityInspectorPanel.h"
```

확인 필요:
- `Scene_InGame`이 보유한 `CWorld` 멤버명(`m_pWorld`/`m_world` 등)을 확인.
- 디버그 패널을 그리는 실제 함수/위치를 확인하고 호출 지점을 맞춘다. 디버그 패널 host가 `UI_Manager::OnImGui_Tuner`라면 그쪽으로 옮긴다.

2. 검증

미검증:
- 빌드 미검증
- 인스펙터가 선택 엔티티 component를 정확히 표시하는지 미검증

검증 명령:
- git diff --check
- & "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" .\Client\Include\Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64

수동 확인:
- F5: "Entity Inspector" 창에서 챔피언 선택 -> Transform/Health/Champion/Stat/Gold가 스냅샷 값과 일치하는지.
- 인스펙터에서 어떤 값도 client가 직접 mutate하지 않는지(현재는 read-only, BeginDisabled 적용).

확인 필요:
- 새 `EntityInspectorPanel.h/.cpp`가 Client 빌드 프로젝트에 포함되는지 확인.

후속 (편집 경로, 별도 세션):
- 값 편집은 server debug override로만 흐른다. `Shared/Schemas/Command.fbs`에 debug override command(또는 기존 AIDebugControl 확장)를 추가하고 codegen 후, 인스펙터의 편집 위젯이 그 command를 전송하도록 연결한다.
- release 빌드에서는 override 경로가 비활성이어야 한다(16 시리즈 S18과 정합).
- 이 경로는 schema 변경 + 양 언어 codegen이 필요하므로 본 세션 범위에서 분리한다.
