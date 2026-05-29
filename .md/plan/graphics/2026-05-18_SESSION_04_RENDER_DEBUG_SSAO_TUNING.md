Session - Render Debug 패널에서 SSAO를 켜고 수치를 조율할 수 있게 한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Client/Private/UI/RenderDebug.cpp

기존 코드:

```cpp
            bool bs = pScene->IsDbgShowStructures();
            if (ImGui::Checkbox("Structures / Jungle", &bs)) pScene->SetDbgShowStructures(bs);

            bool bc = pScene->IsDbgShowColliders();
            if (ImGui::Checkbox("ECS Colliders", &bc)) pScene->SetDbgShowColliders(bc);
```

아래로 교체:

```cpp
            bool bs = pScene->IsDbgShowStructures();
            if (ImGui::Checkbox("Structures / Jungle", &bs)) pScene->SetDbgShowStructures(bs);

            ImGui::SeparatorText("SSAO");
            bool bSSAO = pScene->GetSSAOEnabled();
            if (ImGui::Checkbox("SSAO", &bSSAO)) pScene->SetSSAOEnabled(bSSAO);

            f32_t fSSAORadius = pScene->GetSSAORadius();
            if (ImGui::SliderFloat("SSAO radius", &fSSAORadius, 0.05f, 4.0f))
                pScene->SetSSAORadius(fSSAORadius);

            f32_t fSSAOIntensity = pScene->GetSSAOIntensity();
            if (ImGui::SliderFloat("SSAO intensity", &fSSAOIntensity, 0.1f, 4.0f))
                pScene->SetSSAOIntensity(fSSAOIntensity);

            bool bc = pScene->IsDbgShowColliders();
            if (ImGui::Checkbox("ECS Colliders", &bc)) pScene->SetDbgShowColliders(bc);
```

2. 검증

미검증:
- 빌드 미검증
- Render Debug 패널에서 SSAO 슬라이더 조작 시 즉시 화면 반영되는지 미검증

검증 명령:
- git diff --check
- msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64

수동 확인:
- F1 Render Debug 패널에서 SSAO checkbox가 정상 동작하는지 확인.
- radius/intensity를 최소/최대에 가깝게 움직여 챔피언 발밑/갑옷 틈 음영 변화가 보이는지 확인.
