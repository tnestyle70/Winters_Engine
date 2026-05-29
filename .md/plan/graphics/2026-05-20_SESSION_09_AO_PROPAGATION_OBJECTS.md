Session - DX11 AO propagation to structures jungle and minions

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Client/Public/Manager/Structure_Manager.h

기존 코드:

```cpp
    void    Render(const Mat4& matViewProj, const Vec3& vCameraWorld = Vec3{});
```

아래로 교체:

```cpp
    void    Render(const Mat4& matViewProj, const Vec3& vCameraWorld = Vec3{},
        void* pAmbientOcclusionSRV = nullptr);
```

1-2. C:/Users/user/Desktop/Winters/Client/Private/Manager/Structure_Manager.cpp

기존 코드:

```cpp
void CStructure_Manager::Render(const Mat4& matViewProj, const Vec3& vCameraWorld)
{
    if (!m_pWorld) return;
    const u8_t localTeam = UI::QueryLocalTeam(*m_pWorld);
    m_pWorld->ForEach<StructureComponent, RenderComponent, TransformComponent>(
        [&](EntityID id, StructureComponent&, RenderComponent& rc, TransformComponent& xform)
        {
            if (!rc.bVisible || !rc.pRenderer) return;
            if (!UI::IsRenderableForLocal(*m_pWorld, id, localTeam)) return;
            rc.pRenderer->Update(0.f);
            rc.pRenderer->UpdateCamera(matViewProj, vCameraWorld);
            rc.pRenderer->UpdateTransform(xform.GetWorldMatrix());
            rc.pRenderer->Render();
        });
}
```

아래로 교체:

```cpp
void CStructure_Manager::Render(const Mat4& matViewProj, const Vec3& vCameraWorld,
    void* pAmbientOcclusionSRV)
{
    if (!m_pWorld) return;
    const u8_t localTeam = UI::QueryLocalTeam(*m_pWorld);
    m_pWorld->ForEach<StructureComponent, RenderComponent, TransformComponent>(
        [&](EntityID id, StructureComponent&, RenderComponent& rc, TransformComponent& xform)
        {
            if (!rc.bVisible || !rc.pRenderer) return;
            if (!UI::IsRenderableForLocal(*m_pWorld, id, localTeam)) return;
            rc.pRenderer->Update(0.f);
            rc.pRenderer->SetAmbientOcclusionSRV(pAmbientOcclusionSRV);
            rc.pRenderer->UpdateCamera(matViewProj, vCameraWorld);
            rc.pRenderer->UpdateTransform(xform.GetWorldMatrix());
            rc.pRenderer->Render();
        });
}
```

1-3. C:/Users/user/Desktop/Winters/Client/Public/Manager/Jungle_Manager.h

기존 코드:

```cpp
    void    Render(const Mat4& matViewProj, const Vec3& vCameraWorld = Vec3{});
```

아래로 교체:

```cpp
    void    Render(const Mat4& matViewProj, const Vec3& vCameraWorld = Vec3{},
        void* pAmbientOcclusionSRV = nullptr);
```

1-4. C:/Users/user/Desktop/Winters/Client/Private/Manager/Jungle_Manager.cpp

기존 코드:

```cpp
void CJungle_Manager::Render(const Mat4& matViewProj, const Vec3& vCameraWorld)
{
    if (!m_pWorld) return;
    m_pWorld->ForEach<JungleComponent, RenderComponent, TransformComponent>(
        [&](EntityID, JungleComponent&, RenderComponent& rc, TransformComponent& xform)
        {
            if (!rc.bVisible || !rc.pRenderer) return;
            rc.pRenderer->Update(0.f);
            rc.pRenderer->UpdateCamera(matViewProj, vCameraWorld);
            rc.pRenderer->UpdateTransform(xform.GetWorldMatrix());
            rc.pRenderer->Render();
        });
}
```

아래로 교체:

```cpp
void CJungle_Manager::Render(const Mat4& matViewProj, const Vec3& vCameraWorld,
    void* pAmbientOcclusionSRV)
{
    if (!m_pWorld) return;
    m_pWorld->ForEach<JungleComponent, RenderComponent, TransformComponent>(
        [&](EntityID, JungleComponent&, RenderComponent& rc, TransformComponent& xform)
        {
            if (!rc.bVisible || !rc.pRenderer) return;
            rc.pRenderer->Update(0.f);
            rc.pRenderer->SetAmbientOcclusionSRV(pAmbientOcclusionSRV);
            rc.pRenderer->UpdateCamera(matViewProj, vCameraWorld);
            rc.pRenderer->UpdateTransform(xform.GetWorldMatrix());
            rc.pRenderer->Render();
        });
}
```

1-5. C:/Users/user/Desktop/Winters/Client/Public/Manager/Minion_Manager.h

기존 코드:

```cpp
    void    Render(const Mat4& matVP, const Vec3& vCameraWorld = Vec3{});
```

아래로 교체:

```cpp
    void    Render(const Mat4& matVP, const Vec3& vCameraWorld = Vec3{},
        void* pAmbientOcclusionSRV = nullptr);
```

1-6. C:/Users/user/Desktop/Winters/Client/Private/Manager/Minion_Manager.cpp

기존 코드:

```cpp
void CMinion_Manager::Render(const Mat4& matVP, const Vec3& vCameraWorld)
{
    WINTERS_PROFILE_SCOPE("Minion::Render");
    if (!m_pWorld) return;
    const u8_t localTeam = UI::QueryLocalTeam(*m_pWorld);
    m_pWorld->ForEach<MinionStateComponent, RenderComponent, TransformComponent>(
        [&](EntityID id, MinionStateComponent&, RenderComponent& rc, TransformComponent& xform)
        {
            if (!rc.bVisible || !rc.pRenderer) return;
            if (!UI::IsRenderableForLocal(*m_pWorld, id, localTeam)) return;
            // Update(dt) ??Tick ??RenderComponent ForEach ?먯꽌 ?섑뻾 (?ш린???뚮뜑留?
            FlushTransformForRender(xform);
            rc.pRenderer->UpdateCamera(matVP, vCameraWorld);
            rc.pRenderer->UpdateTransform(xform.GetWorldMatrix());
            rc.pRenderer->Render();
        });
}
```

아래로 교체:

```cpp
void CMinion_Manager::Render(const Mat4& matVP, const Vec3& vCameraWorld,
    void* pAmbientOcclusionSRV)
{
    WINTERS_PROFILE_SCOPE("Minion::Render");
    if (!m_pWorld) return;
    const u8_t localTeam = UI::QueryLocalTeam(*m_pWorld);
    m_pWorld->ForEach<MinionStateComponent, RenderComponent, TransformComponent>(
        [&](EntityID id, MinionStateComponent&, RenderComponent& rc, TransformComponent& xform)
        {
            if (!rc.bVisible || !rc.pRenderer) return;
            if (!UI::IsRenderableForLocal(*m_pWorld, id, localTeam)) return;
            // Update(dt) ??Tick ??RenderComponent ForEach ?먯꽌 ?섑뻾 (?ш린???뚮뜑留?
            FlushTransformForRender(xform);
            rc.pRenderer->SetAmbientOcclusionSRV(pAmbientOcclusionSRV);
            rc.pRenderer->UpdateCamera(matVP, vCameraWorld);
            rc.pRenderer->UpdateTransform(xform.GetWorldMatrix());
            rc.pRenderer->Render();
        });
}
```

1-7. C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameRenderBridge.cpp

기존 코드:

```cpp
    CStructure_Manager::Get()->Render(vp, cameraWorld);
    CJungle_Manager::Get()->Render(vp, cameraWorld);
    CMinion_Manager::Get()->Render(vp, cameraWorld);
```

아래로 교체:

```cpp
    CStructure_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV);
    CJungle_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV);
    CMinion_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV);
```

의도:
- 현재 AO는 맵과 챔피언에만 연결되어 있다.
- 구조물, 정글, 미니언도 같은 diffuse-only shader를 쓰므로 AO SRV를 같은 방식으로 전달한다.
- 정상 F5 flow에서 roster/map/minion 시스템을 숨기지 않고 실제 장면 전체의 접촉감을 확인한다.

2. 검증

미검증:
- 구조물/정글/미니언 접촉부가 SSAO on/off에 반응하는지 확인.
- AO가 없는 backend 또는 pass failure에서 default white texture/null fallback이 과도한 어두움으로 이어지지 않는지 확인.
- 미니언 대량 렌더에서 SetAmbientOcclusionSRV 호출이 성능상 문제 없는지 프로파일러로 확인.

검증 명령:
- `git diff --check`
- `msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`

수동 확인:
- F1 Render Debug `Stress` preset으로 포탑, 정글 몬스터, 미니언 발밑을 비교.
