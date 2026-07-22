Session - 억제기 파괴 후 살아 있는 모델이 남는 시각 버그 수정
좌표: 신규 좌표 후보 · 축 C3 공유 원본/산출물 동시 편집 충돌, C8 검증 병목
관련: 2026-07-19_INHIBITOR_DESTROYED_VISUAL_STATE_FIX_PLAN/RESULT

# 1. 결정 기록

- 문제·제약: Blue/Red 억제기 2종에서 체력 0 이후 살아 있는 모델이 남는다. 다른 세션이 definition-pack 입력·산출물을 수정 중이므로 전체 생성기를 실행하면 6개 공유 파일을 덮어쓸 수 있다.
- 추진 증거: 서버 스냅샷과 `SnapshotApplier`는 체력 0을 `StructureComponent.hp`로 전달한다. `Structure_Manager`도 `hp <= 0`이면 destroyed 마스크를 선택한다. 실제 WMesh/WMat은 `0=일반`, `1=Destroyed`인데 JSON/생성 C++만 반대로 선언돼 있다.
- 메커니즘: canonical JSON과 현재 Client 런타임이 소비하는 생성 C++의 Blue/Red 억제기 인덱스만 같은 값으로 교정한다. 공통 렌더·네트워크 코드는 유지한다.
- 대조: 전체 생성기를 지금 실행하는 안은 정확한 최종 cook이지만 타 세션의 서버/manifest 산출물까지 갱신하므로 이번 세션에서는 금지한다. 런타임 special-case 하드코딩은 canonical 원인을 남기므로 사용하지 않는다.
- 대가: global pack hash 재계산은 다른 세션 종료 후 전체 recook 시 수행된다. 이번 수정은 기존 stale 파일 목록을 늘리지 않고, 다음 recook에서도 canonical JSON에 의해 보존된다.

# 2. 반영해야 하는 코드

## 2-1. C:/Users/user/Desktop/Winters/Data/LoL/ClientPublic/Visual/ObjectVisualDefs.json

`structure.inhibitor.blue`의 기존 코드:

```json
"visibilityStates": [
  {
    "name": "Destroyed",
    "submeshIndex": 0,
    "visibleWhenDestroyed": true
  },
  {
    "name": "Alive",
    "submeshIndex": 1,
    "visibleWhenDestroyed": false
  }
]
```

아래로 교체:

```json
"visibilityStates": [
  {
    "name": "Alive",
    "submeshIndex": 0,
    "visibleWhenDestroyed": false
  },
  {
    "name": "Destroyed",
    "submeshIndex": 1,
    "visibleWhenDestroyed": true
  }
]
```

`structure.inhibitor.red`의 동일한 기존 블록도 위 교체 블록으로 교체한다.

## 2-2. C:/Users/user/Desktop/Winters/Client/Private/Data/Generated/LoLVisualDefinitions.generated.cpp

`MakeStructureVisual_STRUCTURE_INHIBITOR_BLUE`와 `MakeStructureVisual_STRUCTURE_INHIBITOR_RED` 각각의 기존 코드:

```cpp
def.submeshStates[0].submeshIndex = 0u;
def.submeshStates[0].bVisibleWhenDestroyed = true;
def.submeshStates[0].bVisibleWhenAlive = false;
def.submeshStates[1].submeshIndex = 1u;
def.submeshStates[1].bVisibleWhenDestroyed = false;
def.submeshStates[1].bVisibleWhenAlive = true;
```

아래로 교체:

```cpp
def.submeshStates[0].submeshIndex = 0u;
def.submeshStates[0].bVisibleWhenDestroyed = false;
def.submeshStates[0].bVisibleWhenAlive = true;
def.submeshStates[1].submeshIndex = 1u;
def.submeshStates[1].bVisibleWhenDestroyed = true;
def.submeshStates[1].bVisibleWhenAlive = false;
```

다른 세션이 이미 변경한 파일 끝의 global build hash와 그 밖의 생성 정의는 보존한다.

# 3. 검증

예언:

- Blue/Red 모두 hp > 0이면 WMesh 0번 일반 억제기만, hp <= 0이면 WMesh 1번 Destroyed 모델만 렌더된다.
- 변경 전후 `Build-LoLDefinitionPack.py --check`의 stale 파일 집합은 현재 기준 6개로 동일하며, 이번 세션이 공유 산출물 충돌을 확대하지 않는다.
- Client Debug가 컴파일·링크된다.

검증 명령:

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
msbuild Winters.sln /t:Client /p:Configuration=Debug /p:Platform=x64 /m
git diff --check
```

추가 정적 계약 검증:

- WMesh 헤더를 읽어 Blue/Red 모두 `submesh_count == 2`인지 확인한다.
- WMat material 0은 일반 억제기 텍스처, material 1은 `Destroyed`인지 확인한다.
- JSON과 생성 C++가 모두 `alive=0`, `destroyed=1`인지 확인한다.

미검증:

- 자동 조작 가능한 구조물 파괴 smoke가 없으므로 실제 인게임 육안 검증을 수행하지 못하면 RESULT에 명시한다. 그 경우 자동/빌드 검증 완료와 육안 검증 미완료를 분리한다.

# 4. 서브 에이전트 비평

비평 주체: `/root/plan_critic` (read-only)

- P0/BLOCKER — 전체 생성기는 global hash와 서버/manifest 등 6개 파일을 갱신하며 현재 타 세션 dirty와 충돌한다: 수용. 현재 `--check` stale 6개를 baseline으로 기록하고 전체 생성기를 금지했다. canonical JSON과 Client 생성 파일의 정확한 두 함수만 좁혀 동기화하며 기존 hash hunk를 보존한다.
- P1 — WMesh/WMat 의미를 자동 검증하지 않는다: 부분 수용. 이 핫픽스에서는 바이너리 헤더/WMat/JSON/생성 C++를 한 번에 대조하는 read-only 계약 검증을 수행한다. 영구 generator validator 추가는 별도 데이터 파이프라인 변경이므로 비범위다.
- P1 — 육안 검증 없이는 visual 완료를 주장할 수 없다: 수용. 가능하면 인게임 확인하며, 불가능하면 RESULT와 최종 인계에서 명시한다.
- P2 — Network/Snapshot/Structure_Manager를 건드리지 않는 최소 수정이 맞다: 수용. 해당 파일은 수정하지 않는다.

# 5. 성공 기준과 비범위

- 원본과 런타임 생성 정의에서 Blue/Red 억제기 상태가 `alive=0`, `destroyed=1`이다.
- 공통 구조물 렌더, 스냅샷, 서버/GameSim 파일은 수정하지 않는다.
- Client Debug 빌드와 `git diff --check`가 통과한다.
- 기존 타 세션의 생성 파일 build hash hunk를 보존한다.
- 비범위: 억제기 파괴 애니메이션/FX·부활, 넥서스/포탑 상태, 전체 definition pack recook.

# 6. 2026-07-19 Release 실플레이 실패 보정

## 6-1. 기존 판정의 폐기와 확정 원인

- 사용자 합격 기준은 "억제기용 Destroyed 텍스처 변형"이 아니라 **현재 포탑 파괴 때 보이는 잔해 모델을 억제기 위치에 표시**하는 것이다. 기존 계획은 이 요구를 잘못 축소했다.
- 현재 Blue/Red 억제기 WMesh의 0/1번 서브메시는 서로 다른 정점·인덱스 집합이지만 AABB가 거의 동일하고, 같은 억제기 자산 안에서 일반/Destroyed 재질을 쓰는 변형이다. 따라서 0/1 가시성만 뒤집어도 포탑 잔해 WMesh로 교체되지 않는다.
- 포탑 WMesh는 8개 서브메시를 가지며 현재 `ObjectVisualDefs.json`의 포탑 destroyed state는 3번 `Stage3Stump`만 표시한다. 사용자가 말한 "포탑 파괴 잔해"의 현재 런타임 정의는 이 상태다. 억제기 파괴 시 같은 팀 포탑 visual definition과 같은 destroyed mask를 재사용해야 실제 포탑과 동일한 결과가 나온다.
- `SnapshotBuilder -> SnapshotApplier -> StructureComponent.hp -> BuildStructureVisibilityMask`는 정상이다. 억제기 스냅샷은 `hp == 0`을 클라이언트 `StructureComponent.hp`에 기록하고, normal DX11과 RHI snapshot 양쪽 모두 `CStructure_Manager`를 통과한다.
- 이전 RESULT는 변경 TU의 Debug 컴파일까지만 확인했고 Release 링크·실플레이를 하지 않았다. 따라서 Release 적용 완료 판정은 성립하지 않는다. 현재 파일 시각 기준 Release 실행 파일/오브젝트는 이후 재빌드됐지만, 이것도 포탑 잔해 교체 요구 자체를 구현하지는 않는다.
- Docker, 서버 구조물 상태, generated cook, config별 리소스 복사 문제가 아니다. 런타임 리소스는 규칙대로 `Client/Bin/Resource`에서 존재하고, 남은 결함은 Client presentation이 억제기 renderer 하나만 소유한다는 점이다.

## 6-2. 최소 구현 방향

기존 억제기 renderer는 alive 표시와 fallback으로 유지한다. 억제기 엔티티마다 같은 팀 포탑 WMesh로 초기화한 destroyed renderer를 하나 더 소유하고, `hp <= 0`일 때만 renderer와 visual definition을 함께 같은 팀 포탑 쪽으로 선택한다.

```text
Inhibitor hp > 0
-> inhibitor renderer
-> inhibitor alive mask

Inhibitor hp <= 0
-> same-team turret renderer
-> same-team turret destroyed mask
-> 현재 포탑 파괴와 동일한 Stage3Stump 표시
```

- 포탑 잔해의 서브메시 번호를 억제기 코드에 다시 하드코딩하지 않는다. 포탑 canonical visual definition의 destroyed mask를 재사용해 둘이 갈라지지 않게 한다.
- 파괴 renderer 초기화 실패 시 억제기 자체 스폰까지 실패시키지 않고 기존 억제기 destroyed 변형으로 fallback한다. 정상 Release 합격 기준에서는 fallback이 아니라 포탑 renderer 선택을 육안 확인한다.
- normal DX11 `Render`와 `--rhi-scene-only`의 `AppendRenderSnapshotMeshes`를 함께 고친다. 한 경로만 고치면 백엔드별 회귀가 된다.
- 서버/GameSim/Snapshot, ObjectVisualDefs schema, generated pack, 런타임 리소스 파일은 이번 보정에서 수정하지 않는다.

## 6-3. 반영해야 하는 코드

### 6-3-1. C:/Users/user/Desktop/Winters/Client/Public/Manager/Structure_Manager.h

기존 코드:

```cpp
    std::vector<EntityID>                                        m_vecEntities;
    std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>> m_mapRenderers;
    std::vector<std::string>                                     m_vecNames;
```

아래로 교체:

```cpp
    std::vector<EntityID>                                        m_vecEntities;
    std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>> m_mapRenderers;
    std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>> m_mapInhibitorDestroyedRenderers;
    std::vector<std::string>                                     m_vecNames;
```

### 6-3-2. C:/Users/user/Desktop/Winters/Client/Private/Manager/Structure_Manager.cpp — 상태별 renderer/definition 선택

anonymous namespace의 `BuildStructureVisibilityMask` 아래에 다음 helper를 추가한다. 기존 `RenderComponent::pRenderer`는 alive renderer이고, destroyed map에 성공적으로 cook된 같은 팀 포탑 renderer가 있을 때만 교체한다.

```cpp
    struct StructureRenderSelection
    {
        ModelRenderer* pRenderer = nullptr;
        const ClientData::StructureVisualDefinition* pVisual = nullptr;
    };

    StructureRenderSelection ResolveStructureRenderSelection(
        EntityID entity,
        const StructureComponent& structure,
        ModelRenderer* pAliveRenderer,
        const std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>>& destroyedRenderers)
    {
        const auto kind = static_cast<Winters::Map::eObjectKind>(structure.kind);
        StructureRenderSelection selection{
            pAliveRenderer,
            ClientData::FindStructureVisualDefinition(kind, structure.team)
        };

        if (kind != Winters::Map::eObjectKind::Structure_Inhibitor || structure.hp > 0.f)
            return selection;

        const auto it = destroyedRenderers.find(entity);
        const ClientData::StructureVisualDefinition* pTurretVisual =
            ClientData::FindStructureVisualDefinition(
                Winters::Map::eObjectKind::Structure_Turret,
                structure.team);
        if (it != destroyedRenderers.end() && it->second && pTurretVisual)
        {
            selection.pRenderer = it->second.get();
            selection.pVisual = pTurretVisual;
        }
        return selection;
    }
```

`CStructure_Manager::Render`의 lambda 시작부와 renderer 사용부는 `rc.pRenderer`를 직접 쓰지 않고 아래 선택 결과를 사용하도록 교체한다. `candidateCount`/FoW 판정은 기존 순서를 보존한다.

```cpp
            if (!rc.bVisible || !rc.pRenderer)
                return;

            ++candidateCount;
            if (!UI::IsRenderableForLocal(*m_pWorld, id, localTeam, bIgnoreFogOfWar))
            {
                ++fowSkippedCount;
                return;
            }

            const StructureRenderSelection selection = ResolveStructureRenderSelection(
                id,
                structure,
                rc.pRenderer,
                m_mapInhibitorDestroyedRenderers);
            if (!selection.pRenderer)
                return;

            ++visibleCount;
            selection.pRenderer->SetAmbientOcclusionSRV(pAmbientOcclusionSRV);
            selection.pRenderer->UpdateCamera(matViewProj, vCameraWorld);
            selection.pRenderer->UpdateTransform(xform.GetWorldMatrix());
            const bool_t bNexus =
                structure.kind == static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Nexus);
            if (bNexus || (selection.pVisual && selection.pVisual->submeshStateCount > 0u))
            {
                const VisibilityMask mask =
                    BuildStructureVisibilityMask(structure, selection.pVisual);
                selection.pRenderer->RenderWithVisibility(mask);
            }
            else
            {
                selection.pRenderer->RenderFrustumCulled(matViewProj);
            }
```

`CStructure_Manager::AppendRenderSnapshotMeshes`의 기존 lambda 본문:

```cpp
            if (!rc.bVisible || !rc.pRenderer)
                return;

            ++candidateCount;
            if (!UI::IsRenderableForLocal(*m_pWorld, id, localTeam, bIgnoreFogOfWar))
            {
                ++fowSkippedCount;
                return;
            }

            ++visibleCount;
            rc.pRenderer->UpdateTransform(xform.GetWorldMatrix());

            // S035: visibilityStates를 가진 구조물(포탑)도 상태 마스크 경로를 탄다.
            const auto kind = static_cast<Winters::Map::eObjectKind>(structure.kind);
            const ClientData::StructureVisualDefinition* pVisual =
                ClientData::FindStructureVisualDefinition(kind, structure.team);
            const bool_t bNexus =
                structure.kind == static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Nexus);
            if (bNexus || (pVisual && pVisual->submeshStateCount > 0u))
            {
                const VisibilityMask mask = BuildStructureVisibilityMask(structure, pVisual);
                appendedCount += rc.pRenderer->AppendRenderSnapshotMeshes(snapshot, mask);
            }
            else
            {
                appendedCount += rc.pRenderer->AppendRenderSnapshotMeshesFrustumCulled(snapshot, matViewProj);
            }
```

아래로 교체:

```cpp
            if (!rc.bVisible || !rc.pRenderer)
                return;

            ++candidateCount;
            if (!UI::IsRenderableForLocal(*m_pWorld, id, localTeam, bIgnoreFogOfWar))
            {
                ++fowSkippedCount;
                return;
            }

            const StructureRenderSelection selection = ResolveStructureRenderSelection(
                id,
                structure,
                rc.pRenderer,
                m_mapInhibitorDestroyedRenderers);
            if (!selection.pRenderer)
                return;

            ++visibleCount;
            selection.pRenderer->UpdateTransform(xform.GetWorldMatrix());

            const bool_t bNexus =
                structure.kind == static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Nexus);
            if (bNexus || (selection.pVisual && selection.pVisual->submeshStateCount > 0u))
            {
                const VisibilityMask mask =
                    BuildStructureVisibilityMask(structure, selection.pVisual);
                appendedCount += selection.pRenderer->AppendRenderSnapshotMeshes(snapshot, mask);
            }
            else
            {
                appendedCount += selection.pRenderer->AppendRenderSnapshotMeshesFrustumCulled(
                    snapshot,
                    matViewProj);
            }
```

### 6-3-3. C:/Users/user/Desktop/Winters/Client/Private/Manager/Structure_Manager.cpp — destroyed renderer 수명

`Spawn_FromEntry`의 기존 anchor:

```cpp
    auto pRenderer = std::unique_ptr<ModelRenderer>(new ModelRenderer());
    if (!pRenderer->Initialize(pVisual->mesh.resourceRelativePath, pVisual->shader.runtimePath))
        return NULL_ENTITY;
```

위 anchor 바로 아래, 엔티티 생성 전에 다음을 추가한다.

```cpp
    std::unique_ptr<ModelRenderer> pInhibitorDestroyedRenderer;
    if (kind == Winters::Map::eObjectKind::Structure_Inhibitor)
    {
        const ClientData::StructureVisualDefinition* pTurretVisual =
            ClientData::FindStructureVisualDefinition(
                Winters::Map::eObjectKind::Structure_Turret,
                team);
        if (pTurretVisual &&
            pTurretVisual->mesh.resourceRelativePath &&
            pTurretVisual->shader.runtimePath)
        {
            pInhibitorDestroyedRenderer = std::make_unique<ModelRenderer>();
            if (!pInhibitorDestroyedRenderer->Initialize(
                    pTurretVisual->mesh.resourceRelativePath,
                    pTurretVisual->shader.runtimePath))
            {
                pInhibitorDestroyedRenderer.reset();
            }
        }
    }
```

기존 코드:

```cpp
    m_mapRenderers[id] = std::move(pRenderer);
    m_vecEntities.push_back(id);
```

아래로 교체:

```cpp
    m_mapRenderers[id] = std::move(pRenderer);
    if (pInhibitorDestroyedRenderer)
        m_mapInhibitorDestroyedRenderers[id] = std::move(pInhibitorDestroyedRenderer);
    m_vecEntities.push_back(id);
```

`Remove_At`의 `m_mapRenderers.erase(id);` 바로 아래에 다음을 추가한다.

```cpp
    m_mapInhibitorDestroyedRenderers.erase(id);
```

`Clear`의 기존 코드:

```cpp
    m_mapRenderers.clear();
    m_uAutoNumber = 0;
```

아래로 교체:

```cpp
    m_mapRenderers.clear();
    m_mapInhibitorDestroyedRenderers.clear();
    m_uAutoNumber = 0;
```

## 6-4. 검증과 완료선

정적/자동 검증:

```powershell
& .\Tools\Bin\Release\WintersAssetConverter.exe info .\Client\Bin\Resource\Texture\Object\Inhibitor\inhibitor_textured.wmesh
& .\Tools\Bin\Release\WintersAssetConverter.exe info .\Client\Bin\Resource\Texture\Object\Turret\turret_textured.wmesh
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\Client\Include\Client.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1 /nr:false /v:minimal
git diff --check
```

예언:

- 억제기 WMesh는 2개, 포탑 WMesh는 8개 서브메시이며 Release 런타임은 둘 다 `Client/Bin/Resource`에서 읽는다.
- Blue/Red 억제기 alive 상태는 기존 팀별 억제기 renderer를 사용한다.
- Blue/Red 억제기 `hp <= 0` 상태는 같은 팀 포탑 renderer와 포탑 destroyed mask를 사용한다.
- 파괴 뒤 alive 억제기 renderer는 한 프레임에도 함께 제출되지 않는다.
- inhibitor respawn 또는 practice refill로 `hp > 0`이 되면 별도 상태 저장 없이 alive renderer로 되돌아간다.

필수 실플레이 검증:

1. 새로 링크한 `Client/Bin/Release/WintersGame.exe`의 수정 시각이 `Structure_Manager.cpp`와 Release obj 이후인지 확인한다.
2. Release normal DX11에서 Blue/Red 억제기를 각각 파괴한다.
3. 파괴 순간 살아 있는 억제기 본체가 사라지고, 현재 같은 팀 포탑 파괴 때 보이는 잔해만 같은 위치에 남는지 확인한다.
4. 가능하면 `--rhi-scene-only`에서도 한 번 확인한다. 실행하지 못하면 RESULT에 normal DX11 합격과 RHI 미검증을 분리한다.

완료선:

- Release 링크 PASS만으로 완료를 주장하지 않는다. 최소 1팀 Release 실플레이 캡처가 필요하며, Blue/Red 대칭은 코드/리소스 계약과 가능하면 양팀 육안으로 닫는다.
- 기존 §4 비평은 0/1 submesh 교정 계획에 대한 것이므로 이 새 renderer delta에는 자동 승계되지 않는다. 소스 수정 전 독립 델타 비평에서 P0/P1 잔존 0을 다시 기록한다.

## 6-5. 독립 델타 비평

- `/root/replay_plan_critique`: residual P0 0 / P1 0, PASS.
- normal DX11/RHI snapshot selection, 팀별 turret renderer와 canonical destroyed mask, respawn 전환, `Remove_At`/`Clear` 수명, fallback과 Release 완료선을 실제 코드/자산과 재대조했다.
- 최종 source-edit gate는 PASS다.

## 7. 2026-07-19 사용자 추가 범위 — 억제기 5분 재생성

### 7-1. 결정과 완료선

- 억제기는 파괴된 첫 권위 틱부터 `300.0초` 재생성 카운트다운을 시작한다.
- 카운트다운은 Server가 실행하는 Shared/GameSim `CDeathSystem`에서만 진행한다. Client가 시간을 추측하거나 모델을 직접 부활시키지 않는다.
- 파괴 중에는 HP 0, `bIsDead=true`, `TargetableTag` 없음 상태를 유지한다.
- 만료 틱에는 `HealthComponent`와 `StructureComponent`를 최대 체력으로 함께 복구하고 `TargetableTag`를 되살린다.
- Client는 다음 스냅샷의 `hp > 0`을 받으면 기존 구현대로 포탑 잔해 renderer 제출을 멈추고 원래 팀별 억제기 renderer와 체력바를 다시 표시한다.
- 카운트다운 상태는 checkpoint/replay rewind에서도 결정적으로 복원되어야 한다.

### 7-2. 변경 미리보기

#### `Shared/GameSim/Components/GameplayComponents.h`

기존 `InhibitorTag`/static assert 아래에 추가한다.

```cpp
struct InhibitorRespawnComponent
{
    f32_t fRespawnDelaySec = 300.f;
    f32_t fRespawnTimerSec = 0.f;
    bool_t bRespawnPending = false;
    u8_t reserved[3]{};
};

static_assert(sizeof(InhibitorRespawnComponent) == 12u);
static_assert(std::is_trivially_copyable_v<InhibitorRespawnComponent>);
```

#### `Shared/GameSim/Core/Checkpoint/WorldKeyframe.cpp`

`InhibitorTag` 등록 바로 아래에 추가한다.

```cpp
reg.Register<InhibitorRespawnComponent>("InhibitorRespawnComponent");
```

#### `Server/Private/Game/GameRoomSpawn.cpp`

기존 코드:

```cpp
if (bInhibitor)
    m_world.AddComponent<InhibitorTag>(entity);
```

아래로 교체한다.

```cpp
if (bInhibitor)
{
    m_world.AddComponent<InhibitorTag>(entity);
    m_world.AddComponent<InhibitorRespawnComponent>(
        entity,
        InhibitorRespawnComponent{});
}
```

#### `Shared/GameSim/Systems/Death/DeathSystem.cpp`

`shouldBeDead` 분기에서 구조물 사망 정리 전에 다음 상태 전이를 적용한다. 실제 편집에서는 같은 함수 안 중복 없이 통합한다.

```cpp
if (shouldBeDead &&
    world.HasComponent<InhibitorTag>(entity) &&
    world.HasComponent<InhibitorRespawnComponent>(entity))
{
    auto& respawn = world.GetComponent<InhibitorRespawnComponent>(entity);
    if (!respawn.bRespawnPending)
    {
        respawn.bRespawnPending = true;
        respawn.fRespawnTimerSec = (std::max)(0.f, respawn.fRespawnDelaySec);
    }
    else
    {
        respawn.fRespawnTimerSec = (std::max)(
            0.f,
            respawn.fRespawnTimerSec - tc.fDt);
    }

    if (respawn.fRespawnTimerSec <= 0.f && health.fMaximum > 0.f)
    {
        health.fCurrent = health.fMaximum;
        health.bIsDead = false;
        auto& structure = world.GetComponent<StructureComponent>(entity);
        structure.hp = health.fCurrent;
        structure.maxHp = health.fMaximum;
        respawn.bRespawnPending = false;
        if (!world.HasComponent<TargetableTag>(entity))
            world.AddComponent<TargetableTag>(entity);
        continue;
    }
}
```

또한 죽은 구조물의 `TargetableTag` 제거는 damage pipeline이 이미 `bIsDead`를 세운 경우에도 실행되도록 전이 조건 밖으로 둔다. 외부 조작으로 HP가 먼저 복구되면 pending timer를 초기화하고 기존 alive 복구 분기를 사용한다.

#### `Tools/SimLab/main.cpp`

기존 `RunStructureDestructionRemnantProbe()`에 inhibitor fixture를 추가해 다음을 검증한다.

```cpp
InhibitorRespawnComponent respawn{};
world.AddComponent<InhibitorTag>(inhibitor);
world.AddComponent<InhibitorRespawnComponent>(inhibitor, respawn);
// first dead tick: pending=true, delay/timer=300, untargetable
// shortened remaining timer expiry: full HP mirrors to StructureComponent,
// bIsDead=false, pending=false, TargetableTag restored
```

### 7-3. 검증

```powershell
& $msbuild .\Tools\SimLab\SimLab.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1 /nologo /v:minimal
& .\Tools\Bin\Release\SimLab.exe
& $msbuild .\Server\Include\Server.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1 /nologo /v:minimal
& $msbuild .\Client\Include\Client.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1 /nologo /v:minimal
git diff --check
```

수동 Release 완료선은 파괴 직후 포탑 잔해만 남는 것, 5분 뒤 잔해가 사라지면서 원래 억제기와 최대 체력 체력바가 같은 스냅샷 전이로 돌아오는 것이다.

### 7-4. 추가분 독립 비평과 보정

- 독립 비평 1차: P0 0 / P1 2.
- 수용 P1-1: DamagePipeline이 사망 틱에 이미 `bIsDead=true`를 기록하므로, 구조물 `TargetableTag` 제거를 `!bIsDead` 전이 조건 밖으로 옮기는 전체 함수 교체안을 아래에 확정했다. 외부 HP 복구도 pending/timer와 `StructureComponent` mirror를 함께 초기화한다.
- 수용 P1-2: 중간 countdown keyframe 저장 → 원본 진행 → restore → 동일 remaining/동일 respawn tick을 검증하는 round-trip을 자동 완료선에 추가했다.

`Shared/GameSim/Systems/Death/DeathSystem.cpp`의 `<algorithm>` include를 추가하고 `CDeathSystem::Execute` 전체를 아래로 교체한다.

```cpp
void CDeathSystem::Execute(CWorld& world, const TickContext& tc)
{
    const auto entities = DeterministicEntityIterator<HealthComponent>::CollectSorted(world);

    for (EntityID entity : entities)
    {
        auto& health = world.GetComponent<HealthComponent>(entity);
        const bool_t shouldBeDead = health.fCurrent <= 0.f;

        if (shouldBeDead)
        {
            health.fCurrent = 0.f;

            if (world.HasComponent<InhibitorTag>(entity) &&
                world.HasComponent<InhibitorRespawnComponent>(entity))
            {
                auto& respawn = world.GetComponent<InhibitorRespawnComponent>(entity);
                if (!respawn.bRespawnPending)
                {
                    respawn.bRespawnPending = true;
                    respawn.fRespawnTimerSec = (std::max)(
                        0.f,
                        respawn.fRespawnDelaySec);
                }
                else
                {
                    respawn.fRespawnTimerSec = (std::max)(
                        0.f,
                        respawn.fRespawnTimerSec - tc.fDt);
                }

                if (respawn.fRespawnTimerSec <= 0.f && health.fMaximum > 0.f)
                {
                    health.fCurrent = health.fMaximum;
                    health.bIsDead = false;
                    if (world.HasComponent<StructureComponent>(entity))
                    {
                        auto& structure = world.GetComponent<StructureComponent>(entity);
                        structure.hp = health.fCurrent;
                        structure.maxHp = health.fMaximum;
                    }
                    respawn.bRespawnPending = false;
                    if (!world.HasComponent<TargetableTag>(entity))
                        world.AddComponent<TargetableTag>(entity);
                    continue;
                }
            }

            const bool_t bFirstDeathTransition = !health.bIsDead;
            health.bIsDead = true;
            if (bFirstDeathTransition)
            {
                if (world.HasComponent<MoveTargetComponent>(entity))
                    world.GetComponent<MoveTargetComponent>(entity).bHasTarget = false;

                if (world.HasComponent<SkillStateComponent>(entity))
                {
                    auto& skillState = world.GetComponent<SkillStateComponent>(entity);
                    for (u8_t i = 0; i < 5; ++i)
                        skillState.slots[i].currentStage = 0;
                }
                if (world.HasComponent<SkillChargeStateComponent>(entity))
                    world.RemoveComponent<SkillChargeStateComponent>(entity);
            }

            if (world.HasComponent<StructureComponent>(entity) &&
                world.HasComponent<TargetableTag>(entity))
            {
                world.RemoveComponent<TargetableTag>(entity);
            }
            continue;
        }

        if (world.HasComponent<InhibitorRespawnComponent>(entity))
        {
            auto& respawn = world.GetComponent<InhibitorRespawnComponent>(entity);
            respawn.bRespawnPending = false;
            respawn.fRespawnTimerSec = 0.f;
            if (world.HasComponent<StructureComponent>(entity))
            {
                auto& structure = world.GetComponent<StructureComponent>(entity);
                structure.hp = health.fCurrent;
                structure.maxHp = health.fMaximum;
            }
        }

        if (health.bIsDead)
        {
            health.bIsDead = false;
            if (world.HasComponent<StructureComponent>(entity) &&
                !world.HasComponent<TargetableTag>(entity))
            {
                world.AddComponent<TargetableTag>(entity);
            }
        }
    }
}
```

`Tools/SimLab/main.cpp`의 inhibitor fixture는 `fRespawnDelaySec == 300.f`를 확인한 뒤 짧은 remaining time을 설정한다. countdown 중 `SaveWorldKeyframe`, 원본 만료 tick 기록, restore 후 동일 tick 재실행을 수행하고 다음을 모두 assert한다.

```cpp
const f32_t savedRemaining =
    world.GetComponent<InhibitorRespawnComponent>(inhibitor).fRespawnTimerSec;
std::vector<u8_t> checkpoint;
if (!SimCheckpoint::SaveWorldKeyframe(
        world, rng, entityMap, countdownTick.tickIndex, checkpoint))
    return false;

// 원본과 restore 분기 각각 같은 TickContext 두 번을 실행한다.
// 두 분기 모두 첫 틱에는 dead/pending, 두 번째 틱에는 full HP/alive가 되어야 한다.
// restore 직후 remaining == savedRemaining 이어야 한다.
```

재비평 pass line은 이 보정 뒤 accepted/held P0/P1 0이다.
