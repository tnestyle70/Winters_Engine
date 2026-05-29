Session - 우클릭 이동 입력에서 서버 truth yaw를 target/path 첫 waypoint 기준으로 즉시 세팅하고, 클라에는 같은 공식의 약한 visual prediction yaw를 더한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp

`CDefaultCommandExecutor::HandleMove` 안에서 이동 target/path가 확정된 뒤, 실제 이동 tick을 기다리지 않고 서버 Transform yaw를 먼저 세팅한다. path가 있으면 첫 waypoint 기준, path가 없으면 최종 target 기준으로 `RotateEntityTowardDirection`을 호출한다.

기존 코드:

```cpp
    if (world.HasComponent<TransformComponent>(cmd.issuerEntity))
    {
        const Vec3 pos = world.GetComponent<TransformComponent>(cmd.issuerEntity).GetLocalPosition();
        target.y = pos.y;

        if (!TryAssignGridMovePath(tc, pos, target, moveTarget))
        {
            static u32_t s_navRejectLogCount = 0;
            if (s_navRejectLogCount < 32u)
            {
                char msg[224]{};
                sprintf_s(msg,
                    "[Command] move reject reason=no-grid-path issuer=%u seq=%u target=(%.2f,%.2f,%.2f)\n",
                    static_cast<u32_t>(cmd.issuerEntity),
                    cmd.sequenceNum,
                    target.x,
                    target.y,
                    target.z);
                OutputCommandDebug(msg);
                ++s_navRejectLogCount;
            }
            moveTarget.bHasTarget = false;
            return;
        }

        if (WintersMath::DistanceSqXZ(pos, target) <= moveTarget.arriveRadius * moveTarget.arriveRadius)
        {
            moveTarget.bHasTarget = false;
            return;
        }
    }

    moveTarget.target = target;
    moveTarget.bHasTarget = true;
```

아래로 교체:

```cpp
    if (world.HasComponent<TransformComponent>(cmd.issuerEntity))
    {
        const Vec3 pos = world.GetComponent<TransformComponent>(cmd.issuerEntity).GetLocalPosition();
        target.y = pos.y;

        if (!TryAssignGridMovePath(tc, pos, target, moveTarget))
        {
            static u32_t s_navRejectLogCount = 0;
            if (s_navRejectLogCount < 32u)
            {
                char msg[224]{};
                sprintf_s(msg,
                    "[Command] move reject reason=no-grid-path issuer=%u seq=%u target=(%.2f,%.2f,%.2f)\n",
                    static_cast<u32_t>(cmd.issuerEntity),
                    cmd.sequenceNum,
                    target.x,
                    target.y,
                    target.z);
                OutputCommandDebug(msg);
                ++s_navRejectLogCount;
            }
            moveTarget.bHasTarget = false;
            return;
        }

        if (WintersMath::DistanceSqXZ(pos, target) <= moveTarget.arriveRadius * moveTarget.arriveRadius)
        {
            moveTarget.bHasTarget = false;
            return;
        }

        const Vec3 facingTarget =
            (moveTarget.pathCount > 0 && moveTarget.pathIndex < moveTarget.pathCount)
            ? moveTarget.pathWaypoints[moveTarget.pathIndex]
            : target;
        RotateEntityTowardDirection(
            world,
            cmd.issuerEntity,
            Vec3{
                facingTarget.x - pos.x,
                0.f,
                facingTarget.z - pos.z
            });
    }

    moveTarget.target = target;
    moveTarget.bHasTarget = true;
```

1-2. C:/Users/user/Desktop/Winters/Client/Private/Scene/InGamePlayerControlBridge.cpp

`namespace` 내부의 `SpawnMovementIndicator` 함수 아래에 클라 약한 yaw 예측 helper를 추가한다. 이 helper는 local cache와 클라 ECS Transform 회전을 같이 갱신한다. 서버 스냅샷이 다음 tick에 덮어쓰므로 gameplay truth는 바꾸지 않는다.

기존 코드:

```cpp
    // Mouse pick indicator arrows converge toward the accepted move target.
    void SpawnMovementIndicator(CScene_InGame& scene, const Vec3& center)
    {
        static constexpr const wchar_t* kTexturePath =
            L"Client/Bin/Resource/Texture/UI/movement_indicator.png";

        static constexpr f32_t kLifetime = 0.32f;
        static constexpr f32_t kStartRadius = 0.95f;
        static constexpr f32_t kEndRadius = 0.18f;
        static constexpr f32_t kInwardSpeed = (kStartRadius - kEndRadius) / kLifetime;
        static constexpr f32_t kYOffset = 0.05f;
        static constexpr f32_t kWidth = 0.55f;
        static constexpr f32_t kHeight = 0.90f;
        static constexpr f32_t kYawOffset = XM_PI;

        const Vec3 radialDirs[4] = {
            { 1.f, 0.f, 0.f },
            { -1.f, 0.f, 0.f },
            { 0.f, 0.f, 1.f },
            { 0.f, 0.f, -1.f },
        };

        for (const Vec3& radial : radialDirs)
        {
            const Vec3 inward{ -radial.x, 0.f, -radial.z };

            FxBillboardComponent fx{};
            fx.renderType = eFxRenderType::GroundDecal;
            fx.texturePath = kTexturePath;
            fx.vWorldPos = {
                center.x + radial.x * kStartRadius,
                center.y + kYOffset,
                center.z + radial.z * kStartRadius
            };
            fx.vVelocity = { inward.x * kInwardSpeed, 0.f, inward.z * kInwardSpeed };
            fx.fWidth = kWidth;
            fx.fHeight = kHeight;
            fx.fYaw = std::atan2f(inward.x, inward.z) + kYawOffset;
            fx.vColor = { 1.f, 1.f, 1.f, 0.95f };
            fx.fLifetime = kLifetime;
            fx.fFadeIn = 0.02f;
            fx.fFadeOut = 0.22f;
            fx.fAlphaClip = 0.02f;
            fx.blendMode = eBlendPreset::AlphaBlend;
            fx.depthMode = eFxDepthMode::DepthTestWriteOff;
            fx.bBillboard = false;

            CFxSystem::Spawn(scene.GetWorld(), fx);
        }
    }
}
```

아래로 교체:

```cpp
    // Mouse pick indicator arrows converge toward the accepted move target.
    void SpawnMovementIndicator(CScene_InGame& scene, const Vec3& center)
    {
        static constexpr const wchar_t* kTexturePath =
            L"Client/Bin/Resource/Texture/UI/movement_indicator.png";

        static constexpr f32_t kLifetime = 0.32f;
        static constexpr f32_t kStartRadius = 0.95f;
        static constexpr f32_t kEndRadius = 0.18f;
        static constexpr f32_t kInwardSpeed = (kStartRadius - kEndRadius) / kLifetime;
        static constexpr f32_t kYOffset = 0.05f;
        static constexpr f32_t kWidth = 0.55f;
        static constexpr f32_t kHeight = 0.90f;
        static constexpr f32_t kYawOffset = XM_PI;

        const Vec3 radialDirs[4] = {
            { 1.f, 0.f, 0.f },
            { -1.f, 0.f, 0.f },
            { 0.f, 0.f, 1.f },
            { 0.f, 0.f, -1.f },
        };

        for (const Vec3& radial : radialDirs)
        {
            const Vec3 inward{ -radial.x, 0.f, -radial.z };

            FxBillboardComponent fx{};
            fx.renderType = eFxRenderType::GroundDecal;
            fx.texturePath = kTexturePath;
            fx.vWorldPos = {
                center.x + radial.x * kStartRadius,
                center.y + kYOffset,
                center.z + radial.z * kStartRadius
            };
            fx.vVelocity = { inward.x * kInwardSpeed, 0.f, inward.z * kInwardSpeed };
            fx.fWidth = kWidth;
            fx.fHeight = kHeight;
            fx.fYaw = std::atan2f(inward.x, inward.z) + kYawOffset;
            fx.vColor = { 1.f, 1.f, 1.f, 0.95f };
            fx.fLifetime = kLifetime;
            fx.fFadeIn = 0.02f;
            fx.fFadeOut = 0.22f;
            fx.fAlphaClip = 0.02f;
            fx.blendMode = eBlendPreset::AlphaBlend;
            fx.depthMode = eFxDepthMode::DepthTestWriteOff;
            fx.bBillboard = false;

            CFxSystem::Spawn(scene.GetWorld(), fx);
        }
    }

    void PredictLocalMoveYaw(CScene_InGame& scene, const Vec3& target)
    {
        CTransform* playerTransform = scene.GetPlayerTransformPtr();
        if (!playerTransform)
            return;

        const Vec3 origin = playerTransform->GetPosition();
        const f32_t dx = target.x - origin.x;
        const f32_t dz = target.z - origin.z;
        if ((dx * dx + dz * dz) <= 0.0001f)
            return;

        const f32_t yaw =
            std::atan2f(dx, dz) +
            GetDefaultChampionVisualYawOffset(scene.GetPlayerChampionId());

        Vec3 rot = playerTransform->GetRotation();
        rot.y = yaw;
        playerTransform->SetRotation(rot);

        CWorld& world = scene.GetWorld();
        const EntityID playerEntity = scene.GetPlayerEntity();
        if (playerEntity != NULL_ENTITY &&
            world.HasComponent<TransformComponent>(playerEntity))
        {
            auto& tf = world.GetComponent<TransformComponent>(playerEntity);
            Vec3 ecsRot = tf.GetRotation();
            ecsRot.y = yaw;
            tf.SetRotation(ecsRot);
        }
    }
}
```

같은 파일의 네트워크 `SendMove` 직후에 weak yaw prediction을 호출한다.

기존 코드:

```cpp
                if (bNetworkActive && scene.m_pCommandSerializer && scene.m_pNetworkView)
                {
                    scene.m_pCommandSerializer->SendMove(*scene.m_pNetworkView, resolvedGround);

                    scene.m_vPlayerDest = resolvedGround;
                    if (scene.m_PlayerEntity != NULL_ENTITY &&
                        scene.m_World.HasComponent<NavAgentComponent>(scene.m_PlayerEntity))
```

아래로 교체:

```cpp
                if (bNetworkActive && scene.m_pCommandSerializer && scene.m_pNetworkView)
                {
                    scene.m_pCommandSerializer->SendMove(*scene.m_pNetworkView, resolvedGround);
                    PredictLocalMoveYaw(scene, resolvedGround);

                    scene.m_vPlayerDest = resolvedGround;
                    if (scene.m_PlayerEntity != NULL_ENTITY &&
                        scene.m_World.HasComponent<NavAgentComponent>(scene.m_PlayerEntity))
```

2. 검증

검증 명령:

```text
git diff --check
msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

확인 필요:
- 서버 `HandleMove`가 이동 수락 tick에 snapshot yaw를 즉시 갱신하는지 확인.
- 클라 네트워크 모드에서 우클릭 순간 local cache와 ECS Transform yaw가 같은 공식으로 먼저 도는지 확인.
- 다음 서버 snapshot이 클라 weak prediction을 정상적으로 덮어써서 gameplay truth가 갈라지지 않는지 확인.
- 기본 공격 windup 중 우클릭 move cancel/queue 흐름이 기존 Session 2 동작을 유지하는지 확인.
