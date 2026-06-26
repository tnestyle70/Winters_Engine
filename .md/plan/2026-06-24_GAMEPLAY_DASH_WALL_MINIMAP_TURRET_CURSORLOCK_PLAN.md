Session - Dash 벽 끼임, 미니맵 좌표 동기화, 미니맵 원형 초상화, 포탑 이펙트 높이, L키 커서 lock 5개 항목을 반영한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/Move/DashArrival.h

새 파일 (dash 도착 위치를 가장 가까운 walkable 셀로 스냅하는 공통 헬퍼). IWalkableQuery::TryResolveMoveTarget이 내부적으로 CNavGrid::TryFindNearestWalkableCell을 호출하므로 인터페이스 추가 없이 그대로 재사용한다.

cpp

#pragma once

#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Core/World/World.h"

#include "ECS/Components/TransformComponent.h"

// dash 도착 시점에 1회 호출한다. transform의 현재 위치(=도착 위치)가 벽이면
// fromBeforeDash(=dash 시작점, walkable 가정)를 기준으로 가장 가까운 walkable 셀로 스냅한다.
// 전진 중 매 프레임 호출하면 안 된다(도착 분기에서만).
inline void SnapDashArrivalToWalkable(CWorld& world, const TickContext& tc,
    EntityID entity, const Vec3& fromBeforeDash)
{
    if (!tc.pWalkable || !world.HasComponent<TransformComponent>(entity))
        return;

    auto& transform = world.GetComponent<TransformComponent>(entity);
    const Vec3 arrived = transform.GetLocalPosition();

    if (tc.pWalkable->IsWalkableXZ(arrived))
        return;   // 이미 walkable이면 손대지 않는다.

    Vec3 snapped = arrived;
    if (!tc.pWalkable->TryResolveMoveTarget(fromBeforeDash, arrived, snapped))
        return;   // 스냅 실패 시 기존 위치 유지(현행 동작 보존).

    f32_t surfaceY = snapped.y;
    if (tc.pWalkable->TrySampleHeight(snapped.x, snapped.z, surfaceY))
        snapped.y = surfaceY;

    transform.SetPosition(snapped);
}

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/LeeSin/LeeSinGameSim.cpp

include 추가(파일 상단 include 블록):

cpp

#include "Shared/GameSim/Systems/Move/DashArrival.h"

기존 코드:

cpp

        for (EntityID entity : finishedDashes)
            world.RemoveComponent<LeeSinDashComponent>(entity);

아래로 교체:

cpp

        for (EntityID entity : finishedDashes)
        {
            if (world.HasComponent<LeeSinDashComponent>(entity))
                SnapDashArrivalToWalkable(world, tc, entity,
                    world.GetComponent<LeeSinDashComponent>(entity).vStart);
            world.RemoveComponent<LeeSinDashComponent>(entity);
        }

1-3. Sylas/Yone/Yasuo/Viego/Fiora/Jax 동일 패턴

각 *GameSim.cpp 상단에 `#include "Shared/GameSim/Systems/Move/DashArrival.h"` 추가 후, finishedDashes 제거 루프를 위와 동일하게 스냅 추가로 교체한다. dash 시작 필드명만 다르다(Sylas: vStart, 나머지: start). Yone은 SoulReturn(anchorPosition 복귀)만 스냅에서 제외한다.

- Sylas: `SnapDashArrivalToWalkable(world, tc, entity, world.GetComponent<SylasDashComponent>(entity).vStart);`
- Yasuo: `... world.GetComponent<YasuoDashComponent>(entity).start);`
- Viego: `... world.GetComponent<ViegoDashComponent>(entity).start);`
- Fiora: `... world.GetComponent<FioraDashComponent>(entity).start);`
- Jax: `... world.GetComponent<JaxDashComponent>(entity).start);`
- Yone(기존 루프의 SoulReturn else 분기에서만): `SnapDashArrivalToWalkable(world, tc, entity, dash.start);`

1-4. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Irelia/IreliaGameSim.cpp

Irelia는 전용 컴포넌트가 아니라 IreliaSimComponent 필드(dashStartPos)를 쓴다. `if (t >= 1.f || bDashBlocked) { state.bDashActive = false; ... }` 도착 분기에서 transform.SetPosition(guardedPos) 이후 스냅 적용:

cpp

SnapDashArrivalToWalkable(world, tc, entity, state.dashStartPos);

(상단에 DashArrival.h include 추가)

1-5. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.cpp

Kalista passive dash 도착(dash.endPos = guardedPos 확정 지점)에서 스냅 적용 후 transform도 갱신. 상단 DashArrival.h include 추가. 도착 분기에서 `dash.endPos = guardedPos;` 직후 SnapDashArrivalToWalkable(world, tc, entity, dash.startPos) 호출(transform 위치를 스냅값으로 갱신).

1-6. C:/Users/tnest/Desktop/Winters/Client/Public/UI/MinimapPanel.h (미니맵 좌표 동기화)

현재 MinimapProjection 3점이 비직교·비등배(skew)라 world Z축이 1.66배 부풀려져 top이 아래로/bottom이 위로 표시된다. 중심 (104.5, 0), 45° 회전, 직교·등배(반대각 256·√2/2 ≈ 181)로 재교정한다.

기존 코드:

cpp

    struct MinimapProjection
    {
        Vec2 vWorldAtUv00{ 96.59f, 157.20f };
        Vec2 vWorldAtUv10{ 199.28f, 0.04f };
        Vec2 vWorldAtUv01{ 10.51f, 0.98f };
    };

아래로 교체:

cpp

    struct MinimapProjection
    {
        Vec2 vWorldAtUv00{ 104.50f, 181.02f };
        Vec2 vWorldAtUv10{ 285.52f, 0.00f };
        Vec2 vWorldAtUv01{ -76.52f, 0.00f };
    };

1-7. C:/Users/tnest/Desktop/Winters/Client/Private/UI/MinimapPanel.cpp (원형 초상화)

좌표 수정 후 챔피언 마커를 단색 원에서 원형 초상화로 교체한다. UI_Draw_RawImageCircle은 텍스처 SRV를 받으면 자동 원형 크롭하므로 셰이더 불필요. 기존 정사각 초상화 경로 헬퍼(GetRosterChampionPortraitPath)를 재사용한다.

include 추가:

cpp

#include "Scene/LobbyRosterHelpers.h"

익명 namespace에 초상화 텍스처 캐시 + 헬퍼 추가(EnsureMinimapBaseTexture 패턴):

cpp

    std::unordered_map<eChampion, std::unique_ptr<Engine::CTexture>> s_ChampionPortraits;

    Engine::CTexture* EnsureChampionPortrait(eChampion champion)
    {
        if (champion == eChampion::END || champion == eChampion::NONE)
            return nullptr;

        auto it = s_ChampionPortraits.find(champion);
        if (it != s_ChampionPortraits.end())
            return it->second.get();

        CGameInstance* pGameInstance = CGameInstance::Get();
        if (!pGameInstance)
            return nullptr;
        IRHIDevice* pDevice = pGameInstance->Get_RHIDevice();
        if (!pDevice)
            return nullptr;

        const wchar_t* pPath = GetRosterChampionPortraitPath(champion);
        if (!pPath)
            return nullptr;

        auto tex = Engine::CTexture::Create(
            pDevice, std::wstring(pPath),
            Engine::eTexSamplerMode::Clamp,
            Engine::eTexColorSpace::IgnoreSRGB);
        Engine::CTexture* pRaw = tex.get();
        s_ChampionPortraits.emplace(champion, std::move(tex));
        return pRaw;
    }

DrawIcon의 챔피언 비-Rect 분기에서, 테두리 원(vBorder)은 팀색으로 그리고 안쪽 채움 원을 초상화 SRV로 그린다. 기존 vFill 채움 원 호출(line 273-281)을 아래로 교체:

cpp

        void* pPortraitSRV = nullptr;
        if (Icon.eKind == UI::eMinimapIconKind::Champion)
        {
            if (Engine::CTexture* pPortrait = EnsureChampionPortrait(Icon.eChampionId))
                pPortraitSRV = pPortrait->GetNativeSRV();
        }

        if (pPortraitSRV)
        {
            const Vec4 vTint = Icon.bAlive
                ? Vec4{ 1.f, 1.f, 1.f, 1.f }
                : Vec4{ 0.45f, 0.45f, 0.45f, 0.95f };
            pGameInstance->UI_Draw_RawImageCircle(
                pPortraitSRV,
                fCenterX - fRadius, fCenterY - fRadius,
                fRadius * 2.f, fRadius * 2.f,
                kUVFull, vTint, 40);
        }
        else
        {
            pGameInstance->UI_Draw_RawImageCircle(
                nullptr,
                fCenterX - fRadius, fCenterY - fRadius,
                fRadius * 2.f, fRadius * 2.f,
                kUVFull, vFill, 28);
        }

테두리 원은 팀 구분을 위해 vBorder 대신 vFill(팀색)로 두껍게 그리는 것이 LoL UX에 가까우나, 본 세션에서는 기존 vBorder 외곽선을 유지한다(최소 변경). ShutdownRuntime에 캐시 정리 추가:

기존 코드:

cpp

    void CMinimapPanel::ShutdownRuntime()
    {
        s_pMinimapBaseTexture.reset();
    }

아래로 교체:

cpp

    void CMinimapPanel::ShutdownRuntime()
    {
        s_pMinimapBaseTexture.reset();
        s_ChampionPortraits.clear();
    }

`#include <unordered_map>`이 없으면 상단에 추가.

1-8. C:/Users/tnest/Desktop/Winters/Engine/Private/ECS/Systems/TurretAISystem.cpp (포탑 이펙트 높이)

포탑 투사체/이펙트 시작 Y를 올린다.

기존 코드:

cpp

    xf.SetPosition({ turretPos.x, turretPos.y + 2.5f, turretPos.z });

아래로 교체:

cpp

    xf.SetPosition({ turretPos.x, turretPos.y + 4.0f, turretPos.z });

1-9. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp (포탑 머즐 높이)

머즐 섬광은 owner attach라 vAttachOffset.y가 렌더 높이를 지배한다. 서버 투사체 높이와 맞춰 올린다.

기존 코드:

cpp

            pos = { ownerPos.x, ownerPos.y + 2.5f, ownerPos.z };
            attachTo = ownerEntity;

아래로 교체:

cpp

            pos = { ownerPos.x, ownerPos.y + 4.0f, ownerPos.z };
            attachTo = ownerEntity;

기존 코드:

cpp

        fx.vAttachOffset = { 0.f, 2.5f, 0.f };

아래로 교체:

cpp

        fx.vAttachOffset = { 0.f, 4.0f, 0.f };

1-10. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGameInput.cpp (L키 커서 lock 토글)

`if (!bImGuiKbd)` 블록 안 'P' 처리 직후에 L키 토글을 추가한다. DynamicCamera의 ClipCursor 패턴을 따른다.

기존 코드:

cpp

        if (in.IsKeyPressed('P'))
            CGameInstance::Get()->UI_Toggle_InGameShop();

아래에 추가:

cpp

        if (in.IsKeyPressed('L'))
        {
            static bool_t s_bCursorLocked = false;
            s_bCursorLocked = !s_bCursorLocked;
            HWND hWnd = in.GetWindowHandle();
            if (s_bCursorLocked && hWnd)
            {
                RECT rcClient{};
                GetClientRect(hWnd, &rcClient);
                POINT ptTL{ rcClient.left, rcClient.top };
                POINT ptBR{ rcClient.right, rcClient.bottom };
                ClientToScreen(hWnd, &ptTL);
                ClientToScreen(hWnd, &ptBR);
                RECT rcClip{ ptTL.x, ptTL.y, ptBR.x, ptBR.y };
                ClipCursor(&rcClip);
            }
            else
            {
                ClipCursor(nullptr);
            }
        }

`<Windows.h>`가 이미 포함돼 있지 않으면 상단 include 확인(ClipCursor/GetClientRect/ClientToScreen 사용).

2. 검증

미검증:
- 빌드 미검증
- 런타임에서 각 dash가 벽 도착 시 가장 가까운 walkable로 빠지는지 미검증
- 미니맵 top/bottom 위치가 실제 맵과 일치하는지 미검증(시각 검증 필요)
- 원형 초상화가 정상 로드/표시되는지 미검증
- 포탑 이펙트 높이, L키 lock 토글 미검증

검증 명령:
- git diff --check
- MSBuild Server.vcxproj /p:Configuration=Debug /p:Platform=x64
- MSBuild Client.vcxproj /p:Configuration=Debug /p:Platform=x64

확인 필요:
- DashArrival.h는 헤더 전용(inline)이라 vcxproj 등록 불필요. include만 하면 빌드됨.
- 미니맵 좌표 교정값은 45° 등배 가정. 인게임 1프레임 캡처로 top/bottom 대칭과 base 텍스처 정렬을 확인하고 필요 시 미세조정.
- Ezreal E는 현재 Champions에 시뮬 구현이 없어 본 작업 대상에서 제외(미구현).
- 포탑 높이 +4.0f는 잠정값. 포탑 모델 꼭대기 소켓 높이에 맞춰 인게임에서 조정.
