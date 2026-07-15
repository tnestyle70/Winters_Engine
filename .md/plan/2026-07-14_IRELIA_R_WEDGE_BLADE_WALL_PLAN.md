Session - Irelia R 명중 벽을 06.png 기준 전방 꼭짓점과 좌우 직선 변으로 배치하고 E 본체 칼날의 색감을 공유하되 공전 칼날은 생성하지 않는다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Irelia/IreliaFxPresets.h

기존 코드:

```cpp
	// ★ R 명중 시 부채꼴 칼날 깔기 (e_blade.fbx mesh 여러 개)
	//   hitPos: 명중 위치 / vForward: 궁 진행 방향 (정규화)
	//   iCount: 칼날 수 (5 권장) / fSpreadRad: 부채꼴 각도 (π/2 = 90도 권장)
	//   fPlaceDist: 명중 위치에서 칼날까지 거리 (1.5m 권장)
	//   fScale / vRotation: 칼날 자세 (Phase 1 의 BladePitch/Yaw/Roll 적정값 사용)
	// ★ Triangle Mode — bTriangle=true 면 중앙 칼날 forward 추가 push (fTipBoost m),
	//   사이드 칼날 scale 감소 (fSideShrink 0~0.9). 기본 false = 균등 arc.
	void SpawnRBladeFan(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		const Vec3& vHitPos, const Vec3& vForward,
		int32_t iCount, f32_t fSpreadRad, f32_t fPlaceDist,
		f32_t fLifetime, f32_t fScale, const Vec3& vRotation,
		bool bTriangle = false, f32_t fTipBoost = 0.f, f32_t fSideShrink = 0.f);
```

아래로 교체:

```cpp
	// R 명중 시 E 본체 칼날 mesh를 명중점 기준 벽으로 배치한다.
	// bTriangle=true: 좌측 후방 끝점 -> 전방 꼭짓점 -> 우측 후방 끝점의 두 직선 위에 배치한다.
	// bTriangle=false: 기존 균등 원호 배치를 유지한다.
	// iCount는 중앙 꼭짓점 칼날을 정확히 하나 만들 수 있도록 홀수를 사용한다.
	// fSideShrink는 꼭짓점이 아닌 좌우 끝점으로 갈수록 적용할 scale 감소율이다.
	// 이 함수는 SpawnPlaced를 호출하지 않으므로 E 공전 칼날과 Irelia.E.Place cue를 생성하지 않는다.
	void SpawnRBladeFan(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		const Vec3& vHitPos, const Vec3& vForward,
		int32_t iCount, f32_t fSpreadRad, f32_t fPlaceDist,
		f32_t fLifetime, f32_t fScale, const Vec3& vRotation,
		bool bTriangle = false, f32_t fTipBoost = 0.f, f32_t fSideShrink = 0.f);
```

1-2. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Irelia/Irelia_Tuning.h

기존 코드:

```cpp
        i32_t iRWallBladeCount = 24;
        f32_t fRWallBladeSpreadRad = 4.18879f;
        f32_t fRWallBladePlaceDist = 3.0f;
        f32_t fRWallBladeLifetime = 2.5f;
        f32_t fRWallBladeScaleMul = 0.385f;
```

아래로 교체:

```cpp
        i32_t iRWallBladeCount = 25;
        f32_t fRWallBladeSpreadRad = 4.18879f;
        f32_t fRWallBladePlaceDist = 3.0f;
        f32_t fRWallBladeLifetime = 2.5f;
        f32_t fRWallBladeScaleMul = 0.385f;
```

1-3. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp

기존 코드:

```cpp
void IreliaFx::SpawnRBladeFan(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    const Vec3& vHitPos, const Vec3& vForward,
    int32_t iCount, f32_t fSpreadRad, f32_t fPlaceDist,
    f32_t fLifetime, f32_t fScale, const Vec3& vRotation,
    bool bTriangle, f32_t fTipBoost, f32_t fSideShrink)
{
    if (!pRenderer || iCount <= 0)
        return;

    {
        const f32_t fwdYaw = std::atan2f(vForward.x, vForward.z);
        const f32_t shrinkClamp = (fSideShrink < 0.f) ? 0.f
            : (fSideShrink > 0.9f ? 0.9f : fSideShrink);

        const f32_t spreadDuration = 0.28f;
        const f32_t maxStartDelay = 0.14f;
        const f32_t startRadius = 0.35f;

        auto BuildBlade = [&](const Vec3& pos, const Vec3& velocity,
            f32_t lifetime, f32_t startDelay, f32_t fadeIn, f32_t fadeOut,
            f32_t bladeYaw, f32_t bladeScale) -> FxMeshComponent
        {
            FxMeshComponent blade{};
            blade.vWorldPos = pos;
            blade.vVelocity = velocity;
            blade.vRotation = { vRotation.x, vRotation.y + bladeYaw, vRotation.z };
            blade.vScale = { bladeScale, bladeScale, bladeScale };
            blade.modelPath = kPathRBladeFbx;
            blade.texturePath = kPathRBladeTex;
            blade.vColor = { 0.7f, 0.9f, 1.45f, 1.0f };
            blade.blendMode = eBlendPreset::AlphaBlend;
            blade.fLifetime = lifetime;
            blade.fStartDelay = startDelay;
            blade.fFadeIn = fadeIn;
            blade.fFadeOut = fadeOut;
            blade.fAlphaClip = 0.f;
            blade.bDepthWrite = false;
            blade.RefreshMaterialFromLegacyFields();
            return blade;
        };

        for (i32_t i = 0; i < iCount; ++i)
        {
            const f32_t t = (iCount == 1)
                ? 0.5f
                : static_cast<f32_t>(i) / static_cast<f32_t>(iCount - 1);
            const f32_t sideRatio = std::fabs(2.f * t - 1.f);
            const f32_t angleOffset = (-fSpreadRad * 0.5f) + fSpreadRad * t;

            const f32_t bladeYaw = fwdYaw + angleOffset;
            const Vec3 bladeDir{ std::sinf(bladeYaw), 0.f, std::cosf(bladeYaw) };

            f32_t bladeScale = fScale;
            f32_t fwdPush = 0.f;
            if (bTriangle)
            {
                fwdPush = fTipBoost * (1.f - sideRatio);
                bladeScale = fScale * (1.f - shrinkClamp * sideRatio);
            }

            const Vec3 startPos{
                vHitPos.x + bladeDir.x * startRadius + vForward.x * (0.2f + fwdPush * 0.25f),
                vHitPos.y,
                vHitPos.z + bladeDir.z * startRadius + vForward.z * (0.2f + fwdPush * 0.25f)
            };

            const Vec3 finalPos{
                vHitPos.x + bladeDir.x * fPlaceDist + vForward.x * fwdPush,
                vHitPos.y,
                vHitPos.z + bladeDir.z * fPlaceDist + vForward.z * fwdPush
            };

            const f32_t startDelay = sideRatio * maxStartDelay;
            const Vec3 velocity{
                (finalPos.x - startPos.x) / spreadDuration,
                (finalPos.y - startPos.y) / spreadDuration,
                (finalPos.z - startPos.z) / spreadDuration
            };

            FxMeshComponent movingBlade = BuildBlade(
                startPos,
                velocity,
                spreadDuration + 0.08f,
                startDelay,
                0.025f,
                0.08f,
                bladeYaw,
                bladeScale);
            CFxMeshSystem::Spawn(world, pRenderer, movingBlade);

            const f32_t settleDelay = startDelay + spreadDuration * 0.85f;
            const f32_t settleLifetime = (fLifetime > settleDelay)
                ? (fLifetime - settleDelay)
                : 0.1f;

            FxMeshComponent settledBlade = BuildBlade(
                finalPos,
                { 0.f, 0.f, 0.f },
                settleLifetime,
                settleDelay,
                0.05f,
                0.35f,
                bladeYaw,
                bladeScale);
            CFxMeshSystem::Spawn(world, pRenderer, settledBlade);
        }

        return;
    }

}
```

아래로 교체:

```cpp
void IreliaFx::SpawnRBladeFan(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    const Vec3& vHitPos, const Vec3& vForward,
    int32_t iCount, f32_t fSpreadRad, f32_t fPlaceDist,
    f32_t fLifetime, f32_t fScale, const Vec3& vRotation,
    bool bTriangle, f32_t fTipBoost, f32_t fSideShrink)
{
    if (!pRenderer || iCount < 3)
        return;

    const Vec3 forward = WintersMath::NormalizeXZOrZero(vForward);
    if (forward.x == 0.f && forward.z == 0.f)
        return;

    const Irelia::IreliaTuning& tuning = Irelia::GetTuning();
    const f32_t fwdYaw = std::atan2f(forward.x, forward.z);
    const f32_t halfSpread = fSpreadRad * 0.5f;
    const f32_t leftYaw = fwdYaw - halfSpread;
    const f32_t rightYaw = fwdYaw + halfSpread;
    const Vec3 leftDir{ std::sinf(leftYaw), 0.f, std::cosf(leftYaw) };
    const Vec3 rightDir{ std::sinf(rightYaw), 0.f, std::cosf(rightYaw) };
    const Vec3 leftEnd{
        vHitPos.x + leftDir.x * fPlaceDist,
        vHitPos.y,
        vHitPos.z + leftDir.z * fPlaceDist
    };
    const Vec3 rightEnd{
        vHitPos.x + rightDir.x * fPlaceDist,
        vHitPos.y,
        vHitPos.z + rightDir.z * fPlaceDist
    };
    const Vec3 tip{
        vHitPos.x + forward.x * (fPlaceDist + fTipBoost),
        vHitPos.y,
        vHitPos.z + forward.z * (fPlaceDist + fTipBoost)
    };
    const f32_t shrinkClamp = (fSideShrink < 0.f) ? 0.f
        : (fSideShrink > 0.9f ? 0.9f : fSideShrink);

    const f32_t spreadDuration = 0.28f;
    const f32_t maxStartDelay = 0.14f;
    const f32_t startRadius = 0.35f;

    auto LerpPoint = [](const Vec3& a, const Vec3& b, f32_t t) -> Vec3
    {
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        };
    };

    auto BuildBlade = [&](const Vec3& pos, const Vec3& velocity,
        f32_t lifetime, f32_t startDelay, f32_t fadeIn, f32_t fadeOut,
        f32_t bladeYaw, f32_t bladeScale) -> FxMeshComponent
    {
        FxMeshComponent blade{};
        blade.vWorldPos = pos;
        blade.vVelocity = velocity;
        blade.vRotation = { vRotation.x, vRotation.y + bladeYaw, vRotation.z };
        blade.vScale = { bladeScale, bladeScale, bladeScale };
        blade.modelPath = kPathRBladeFbx;
        blade.texturePath = kPathRBladeTex;
        blade.vColor = tuning.eBladeColor;
        blade.blendMode = eBlendPreset::AlphaBlend;
        blade.fLifetime = lifetime;
        blade.fStartDelay = startDelay;
        blade.fFadeIn = fadeIn;
        blade.fFadeOut = fadeOut;
        blade.fAlphaClip = 0.05f;
        blade.bDepthWrite = false;
        blade.RefreshMaterialFromLegacyFields();
        return blade;
    };

    for (i32_t i = 0; i < iCount; ++i)
    {
        const f32_t t = static_cast<f32_t>(i) / static_cast<f32_t>(iCount - 1);
        const f32_t sideRatio = std::fabs(2.f * t - 1.f);
        const f32_t angleOffset = (-halfSpread) + fSpreadRad * t;

        f32_t bladeYaw = fwdYaw + angleOffset;
        Vec3 bladeDir{ std::sinf(bladeYaw), 0.f, std::cosf(bladeYaw) };
        Vec3 finalPos{
            vHitPos.x + bladeDir.x * fPlaceDist,
            vHitPos.y,
            vHitPos.z + bladeDir.z * fPlaceDist
        };

        f32_t bladeScale = fScale;
        f32_t fwdPush = 0.f;
        if (bTriangle)
        {
            finalPos = (t <= 0.5f)
                ? LerpPoint(leftEnd, tip, t * 2.f)
                : LerpPoint(tip, rightEnd, (t - 0.5f) * 2.f);
            bladeDir = WintersMath::NormalizeXZOrZero(Vec3{
                finalPos.x - vHitPos.x,
                0.f,
                finalPos.z - vHitPos.z
            });
            if (bladeDir.x == 0.f && bladeDir.z == 0.f)
                bladeDir = forward;
            bladeYaw = std::atan2f(bladeDir.x, bladeDir.z);
            fwdPush = fTipBoost * (1.f - sideRatio);
            bladeScale = fScale * (1.f - shrinkClamp * sideRatio);
        }

        const Vec3 startPos{
            vHitPos.x + bladeDir.x * startRadius + forward.x * (0.2f + fwdPush * 0.25f),
            vHitPos.y,
            vHitPos.z + bladeDir.z * startRadius + forward.z * (0.2f + fwdPush * 0.25f)
        };

        const f32_t startDelay = sideRatio * maxStartDelay;
        const Vec3 velocity{
            (finalPos.x - startPos.x) / spreadDuration,
            (finalPos.y - startPos.y) / spreadDuration,
            (finalPos.z - startPos.z) / spreadDuration
        };

        FxMeshComponent movingBlade = BuildBlade(
            startPos,
            velocity,
            spreadDuration + 0.08f,
            startDelay,
            0.025f,
            0.08f,
            bladeYaw,
            bladeScale);
        CFxMeshSystem::Spawn(world, pRenderer, movingBlade);

        const f32_t settleDelay = startDelay + spreadDuration * 0.85f;
        const f32_t settleLifetime = (fLifetime > settleDelay)
            ? (fLifetime - settleDelay)
            : 0.1f;

        FxMeshComponent settledBlade = BuildBlade(
            finalPos,
            { 0.f, 0.f, 0.f },
            settleLifetime,
            settleDelay,
            0.05f,
            0.35f,
            bladeYaw,
            bladeScale);
        CFxMeshSystem::Spawn(world, pRenderer, settledBlade);
    }
}
```

2. 검증

미검증:
- 이번 세션은 현재 코드와 세션 문서를 조사해 구현 계획만 작성했다.
- Client/Server/Shared/Engine 소스는 수정하지 않았고 빌드 및 런타임 검증도 수행하지 않았다.

적용 전 충돌 확인:
- `Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp`, `Shared/GameSim/Champions/Irelia/IreliaGameSim.cpp`, `Client/Private/Network/Client/EventApplier.cpp`에는 다른 세션의 미커밋 변경이 있다. 이 계획은 해당 파일과 R visual hook signature/call site를 수정하지 않는다.
- 구현 직전에 이 문서 1-1~1-3에 적은 기존 코드 anchor가 현재 작업 트리와 동일한지 다시 확인한다.
- `Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp`, `Client/Public/GameObject/Champion/Irelia/IreliaFxPresets.h`, `Client/Public/GameObject/Champion/Irelia/Irelia_Tuning.h`에 새 미커밋 변경이 생겼다면 해당 세션 owner와 먼저 조정한다.

자동 검증 명령:
```text
git diff --check
MSBuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

수동 확인:
- R이 적 챔피언을 맞힌 stage 2에서 명중점을 원점으로 볼 때 좌우 끝점이 전방 약 `-1.5m`, 측면 `±2.598m`, 꼭짓점이 전방 `+4.5m`에 놓이는지 확인한다.
- 정면/후면/좌/우 네 방향으로 R을 적중시켜 두 칼날 열이 항상 `leftEnd -> tip -> rightEnd`의 직선 두 변을 만들고, 25개 홀수 배치로 중앙 꼭짓점 칼날이 정확히 하나만 생기는지 캡처한다.
- 0.28초 확산 이동과 0.14초 측면 지연 뒤에도 최종 정착 위치가 원호나 타원으로 휘지 않는지 확인한다.
- R 칼날이 `irelia_base_e_blade.fbx`와 `irelia_base_blades_passive_4_texture.png`를 유지하면서 E 본체의 `eBladeColor`와 alpha clip을 사용하고, 검게 사라지거나 과도하게 잘리지 않는지 확인한다.
- R 칼날마다 `IreliaBladeComponent`, `orbitFxID1`, `orbitFxID2`, `Irelia.E.Place` cue가 생기지 않는지 확인한다.
- E 1타/2타는 기존대로 본체 칼날 1개와 공전 소형 칼날 2개 및 E place cue를 유지하는지 회귀 확인한다.
- R이 적을 맞히지 않고 최대 사거리에 도달한 stage 3 경로와 R 벽의 서버 slow/disarm 판정이 바뀌지 않는지 확인한다.
- normal F5 roster/map/minion/snapshot/UI/FX를 유지한 상태에서 `FxMesh::Drawn`과 frame capture를 비교해 24개에서 25개로 늘어난 칼날과 moving/settled 겹침 구간이 프레임 예산을 깨지 않는지 확인한다.

권위 경계:
- 이번 변경은 Client presentation 전용이다. Shared/GameSim의 현재 R 직사각형 벽 판정과 Server 이벤트 생성은 수정하지 않는다.
- 실제 slow/disarm 통과 판정까지 V자 두 선분에 일치시키려면 별도의 서버 권위 segment/polygon 판정 세션으로 분리한다.

CONFIRM_NEEDED:
- 칼날 직선 배치 뒤에도 `Data/LoL/FX/Champions/Irelia/r_wall.wfx`의 `r_wall_ground_haze`가 전체 실루엣을 타원처럼 보이게 하는지는 실제 캡처로 확인한다. 재현될 때만 WFX 바닥 haze 크기/형상을 별도 후속 변경으로 잡고, 1차 구현에는 섞지 않는다.

천장 예산 30%:
- 구현·검증 시간의 최소 30%를 `06.png`와 동일한 탑다운 구도에서 네 방향 적중 영상을 촬영하고 직선성·꼭짓점·E 본체 색감을 비교하는 실제 결과물에 고정한다.
