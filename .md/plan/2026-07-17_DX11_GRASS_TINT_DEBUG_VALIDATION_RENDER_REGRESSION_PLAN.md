# DX11 Grass Tint Debug Validation Render Regression Plan

Session - Grass Tint 도입 뒤 맵 정적 드로우가 28ms로 증가한 DX11 Debug 렌더 회귀를 제거한다
좌표: 신규 좌표 후보 · 축: C8 검증이 병목 · C6 가지치기
관련: S15_ENGINE_RENDERING_FILTER_AUDIT.md
천장 예산: 전체 작업의 30%를 동일 장면 Timeline 재캡처와 예측 대 실측 RESULT 기록에 배정한다.

## 1. 결정 기록

① 문제·제약: 60FPS 예산 16.67ms에서 300프레임 중앙값은 Frame 51.02ms, Render 38.38ms, `Model::RenderCombinedStatic` 28.02ms다. 맵 1080서브메시·약 395드로우는 이전 캡처와 같아 장면량 증가가 아니다.
② 순진한 해법의 실패: DX11 Debug Layer 또는 Grass Tint를 끄면 7월 17일 회귀는 숨길 수 있지만 normal F5 시각 경로와 검증 안전성을 잃으며, 맵 절두체 컬링 개편은 이전에도 같았던 드로우 구조를 원인으로 오인한다.
③ 메커니즘: 표준 서브메시를 그리기 전에 Grass Tint의 유효한 `t6/s6`을 바인딩하고, 읽기 전용 리소스를 null로 되돌리지 않아 같은 셰이더를 쓰는 후속 드로우에서도 Debug Layer의 미바인딩 검증을 피한다.
④ 대조: 표준/Grass Tint 픽셀 셰이더 변형 분리는 장기적으로 더 명시적이지만 셰이더·파이프라인 수명과 캐시까지 넓어진다. 이번에는 기존 `CTexture::Bind`와 `g_bUseGrassTint` 계약 안에서 한 경로만 고친다.
⑤ 대가: DX11 즉시 컨텍스트의 `t6/s6`에 읽기 전용 Grass Tint가 남는다. 향후 해당 텍스처를 RTV/UAV로 재사용하거나 다른 패스가 slot 6의 null 상태에 의존하면 이 선택은 틀리며, 그 패스가 자신의 리소스를 명시적으로 바인딩하도록 바꿔야 한다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Engine/Private/Renderer/ModelRenderer.cpp

`ModelRenderer::RenderWithVisibility`의 `RenderMaterialPasses` 람다에서 아래로 교체한다.

기존 코드:

```cpp
        if (bHasStandardSubmeshes)
        {
            UpdateObjectConstants(false);
            m_pImpl->cbPerObject.Bind(pContext, 1);
            m_pImpl->pSharedModel->RenderWithMask(pDevice, standardMask);
        }

        m_pImpl->pGrassTintTexture->Bind(pDevice, 6);
        UpdateObjectConstants(true);
        m_pImpl->cbPerObject.Bind(pContext, 1);
        m_pImpl->pSharedModel->RenderWithMask(pDevice, grassTintMask);

        ID3D11ShaderResourceView* pNullSRV = nullptr;
        ID3D11SamplerState* pNullSampler = nullptr;
        pContext->PSSetShaderResources(6, 1, &pNullSRV);
        pContext->PSSetSamplers(6, 1, &pNullSampler);
```

아래로 교체:

```cpp
        // Mesh3D/Skinned3D declare t6/s6 for every draw. Keep a valid read-only
        // binding even while g_bUseGrassTint is false so the DX11 Debug Layer
        // does not validate hundreds of standard map draws against null slots.
        m_pImpl->pGrassTintTexture->Bind(pDevice, 6);

        if (bHasStandardSubmeshes)
        {
            UpdateObjectConstants(false);
            m_pImpl->cbPerObject.Bind(pContext, 1);
            m_pImpl->pSharedModel->RenderWithMask(pDevice, standardMask);
        }

        UpdateObjectConstants(true);
        m_pImpl->cbPerObject.Bind(pContext, 1);
        m_pImpl->pSharedModel->RenderWithMask(pDevice, grassTintMask);
```

## 3. 검증

예측:
- `Mesh3D.hlsl`/`Skinned3D.hlsl`의 `t6/s6`과 `g_bUseGrassTint` 계약은 유지되고, 표준 맵과 Grass Tint의 시각 결과는 변하지 않는다.
- Client Debug x64 빌드가 통과하며, 동일 장면 재캡처에서 `Mesh::DrawCalls` 약 395~450과 GPU 시간은 비슷한 채 `Model::RenderCombinedStatic` 중앙값이 28.02ms에서 이전 기준 1~3ms에 가까워진다.
- 게이트: `git diff --check`, Engine 포함 Client Debug x64 `/m:1` 빌드, 다음 normal F5 Timeline 300프레임 비교. 런타임 재캡처 전에는 성능 회복을 확정하지 않는다.

검증 명령:
- `git diff --check -- Engine/Private/Renderer/ModelRenderer.cpp .md/plan/2026-07-17_DX11_GRASS_TINT_DEBUG_VALIDATION_RENDER_REGRESSION_PLAN.md`
- `& 'C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe' Engine/Include/Engine.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 '/p:OutDir=C:/Users/user/AppData/Local/Temp/WintersCodex/EngineDebug/' '/p:IntDir=C:/Users/user/AppData/Local/Temp/WintersCodex/EngineObj/' /p:PostBuildEventUseInBuild=false /m:1`
- `& 'C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe' Shared/GameSim/Include/GameSim.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 '/p:OutDir=C:/Users/user/AppData/Local/Temp/WintersCodex/ClientDebug/' '/p:IntDir=C:/Users/user/AppData/Local/Temp/WintersCodex/GameSimObj/' /p:PreBuildEventUseInBuild=false /p:PostBuildEventUseInBuild=false /m:1`
- `& 'C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe' Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 '/p:SolutionDir=C:/Users/user/Desktop/Winters/' '/p:OutDir=C:/Users/user/AppData/Local/Temp/WintersCodex/ClientDebug/' '/p:IntDir=C:/Users/user/AppData/Local/Temp/WintersCodex/ClientObj/' /p:BuildProjectReferences=false /p:PreBuildEventUseInBuild=false /p:PostBuildEventUseInBuild=false /m:1`
- normal F5에서 기존 캡처와 같은 카메라·해상도·로스터로 Save Timeline 300프레임 캡처 후 `Frame`, `Render`, `Map::DrawFrustumCulled`, `Model::RenderCombinedStatic`, `GPU::FrameUs`, `Mesh::DrawCalls` 비교

미검증:
- 계획 작성 시점에는 수정 후 normal F5 Timeline과 Visual Studio DX11 InfoQueue 경고 소거 여부를 아직 측정하지 않았다.

확인 필요:
- 수정 후에도 `Model::RenderCombinedStatic`이 3ms를 넘으면 Visual Studio Output에서 slot 6 미바인딩 경고와 GPU capture/debugger hook을 확인하고, 표준/Grass Tint 셰이더 변형 분리 여부를 재판정한다.
