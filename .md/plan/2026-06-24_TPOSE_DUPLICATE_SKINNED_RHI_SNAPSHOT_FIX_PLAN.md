Session - RHI scene snapshot must not render skinned champions as duplicate bind-pose meshes.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Engine/Private/Renderer/ModelRenderer.cpp

`ModelRenderer::AppendRenderSnapshotMeshes(RenderWorldSnapshot& snapshot, const VisibilityMask& mask, u32_t maxItems) const` 안에서 아래 기존 코드를:

기존 코드:

```cpp
    if (!m_pImpl ||
        !m_pImpl->bReady ||
        !m_pImpl->pSharedModel ||
        !m_pImpl->bHasWorldMatrix)
    {
        return 0;
    }

    return m_pImpl->pSharedModel->AppendRenderSnapshotMeshes(
        snapshot,
        m_pImpl->matWorld,
        mask,
        maxItems);
```

아래로 교체:

```cpp
    if (!m_pImpl ||
        !m_pImpl->bReady ||
        !m_pImpl->pSharedModel ||
        !m_pImpl->bHasWorldMatrix)
    {
        return 0;
    }

    // RenderWorldSnapshot/RHISceneRenderer currently has no skinned vertex path or bone palette.
    // Skinned models must stay on the legacy animated renderer until RHI skinning is explicit.
    if (m_pImpl->pSharedModel->HasSkeleton())
        return 0;

    return m_pImpl->pSharedModel->AppendRenderSnapshotMeshes(
        snapshot,
        m_pImpl->matWorld,
        mask,
        maxItems);
```

### 1-2. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameRender.cpp

`AppendChampionSnapshotMeshes` 안에서 아래 기존 코드를:

기존 코드:

```cpp
                if (rc.bSceneManaged || !rc.bVisible || !rc.pRenderer)
                    return;
                if (!UI::IsRenderableForLocal(world, e, localTeam, bRevealAllForPlayback))
                    return;
```

아래로 교체:

```cpp
                if (rc.bSceneManaged || !rc.bVisible || !rc.pRenderer)
                    return;
                if (rc.bAnimated || rc.pRenderer->HasSkeleton())
                    return;
                if (!UI::IsRenderableForLocal(world, e, localTeam, bRevealAllForPlayback))
                    return;
```

## 2. 검증

검증 명령:

- `git diff --check`
- `& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Engine/Include/Engine.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal`
- `& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal`

수동 확인:

- 일반 F5/네트워크 클라이언트에서 비에고와 사일러스가 기존 애니메이션 포즈로만 한 번 렌더되는지 확인.
- 같은 위치에 팔 벌린 bind-pose/T-pose 모델이 겹쳐 보이지 않는지 확인.
- `--rhi-scene-only` 실행 시 skinned champion은 숨겨질 수 있음을 확인하고, 이는 RHI skinned pipeline이 생기기 전까지 의도된 임시 제한으로 기록한다.
- 맵, 구조물, 정글, 미니언 static scene object snapshot 렌더가 유지되는지 확인.

후속 확인:

- RHI에 skinned vertex layout, bone palette buffer, skinned shader, per-instance bone binding이 추가되면 위 skip을 제거하고 `RenderWorldSnapshot`에 skinned mesh item을 별도 타입으로 추가한다.
