Session - Niagara style WFX editor bridge for Elden Ring FX

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Client/Private/UI/WfxEffectToolPanel.cpp

기존 코드:

```cpp
        char szTextureRoot[512] = "Client/Bin/Resource/Texture/Character";
```

아래에 추가:

```cpp
        int iTextureRootPreset = 0;
        bool bShowEldenRingTexturePresets = true;
```

기존 코드:

```cpp
    void RenderTexturesTab()
    {
        ImGui::InputText("Texture Root", s_State.szTextureRoot, sizeof(s_State.szTextureRoot));
```

아래에 추가:

```cpp
        if (s_State.bShowEldenRingTexturePresets)
        {
            if (ImGui::Button("ER UI"))
                strcpy_s(s_State.szTextureRoot, "Client/Bin/Resource/EldenRing/UI");
            ImGui::SameLine();
            if (ImGui::Button("ER Textures"))
                strcpy_s(s_State.szTextureRoot, "Client/Bin/Resource/EldenRing");
        }
```

기존 코드:

```cpp
        ImGui::BulletText("Tune in .wfx first; Niagara-style graph nodes can be layered on this document model later.");
```

아래로 교체:

```cpp
        ImGui::BulletText("Tune in .wfx first; graph nodes are layered on the same document model.");
```

의도:
- 기존 WFX 문서 모델을 버리지 않는다.
- Niagara식 node graph는 WFX emitter/curve/texture binding 위에 편집 UI로 얹는다.
- Elden Ring texture root scan은 우선 Client WFX tool에서 검증한다. EldenRingClient DX12 ImGui가 안정화되면 같은 패널을 editor module로 옮긴다.

1-2. C:/Users/tnest/Desktop/Winters/Client/Public/UI/WfxGraphDocument.h

새 파일:

```cpp
CONFIRM_NEEDED - 기존 FX/FxAsset.h, Client/Private/GameObject/FX/WfxDocument.cpp, WfxEffectToolPanel.cpp의 저장 포맷을 확인한 뒤 전체 파일 본문을 작성한다.

책임:
- WFX asset을 깨지 않고 graph editor metadata를 별도 optional block으로 보존한다.
- Node 종류: TextureSample, CurveFloat, SpawnRate, ColorOverLife, SizeOverLife, Erosion, OutputEmitter.
- Runtime은 기존 FxAsset/FxCuePlayer가 읽는 emitter desc로 bake한다.
```

1-3. C:/Users/tnest/Desktop/Winters/Client/Private/UI/WfxGraphDocument.cpp

새 파일:

```cpp
CONFIRM_NEEDED - WfxGraphDocument.h 확정 후 전체 구현 본문을 작성한다.
구현 기준:
- Load old WFX without graph block -> graph metadata empty.
- Save with graph block -> old emitter data도 항상 같이 저장.
- BakeGraphToEmitters()가 실패하면 기존 emitter data를 유지한다.
```

1-4. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/FX/FxCuePlayer.cpp

삭제할 코드/범위:

```cpp
이번 세션에서는 서버 cue 재생 경로를 바꾸지 않는다.
WFX graph는 editor-side bake 결과가 기존 FxAsset emitter desc로 내려온 뒤에만 FxCuePlayer에 들어온다.
```

1-5. C:/Users/tnest/Desktop/Winters/EldenRingClient/Private/EldenAssetProbeScene.cpp

기존 코드:

```cpp
void CEldenRingAssetProbeScene::OnImGui()
{
```

아래에 추가:

```cpp
    CONFIRM_NEEDED - DX12 ImGui bootstrap 통과 후 WFX panel을 EldenRingClient에 노출할지,
    Client editor의 기존 WfxEffectToolPanel을 먼저 강화할지 결정한다.
    권장 순서: Client WFX tool에서 Elden Ring texture scan과 WFX bake 검증 후 EldenRingClient editor panel로 이식.
```

2. 검증

2-1. 검증 명령

```powershell
msbuild Winters.sln /t:Client /p:Configuration=Debug /p:Platform=x64
```

2-2. 런타임 확인

```text
1. 기존 LoL WFX catalog scan/load/save가 깨지지 않는다.
2. Texture tab에서 Client/Bin/Resource/EldenRing 아래 png texture를 scan할 수 있다.
3. ER UI 또는 ER Textures preset으로 main menu UI texture와 *_a, *_n, *_m, *_r, *_em 산출물을 찾을 수 있다.
4. 새 graph metadata가 없는 기존 .wfx 파일도 그대로 로드된다.
```

2-3. 다음 세션 게이트

```text
FX는 서버 cue -> client visual path 원칙을 유지한다.
Editor preview는 별도 preview path를 가져도 되지만, 실제 gameplay FX는 server cue를 한 번만 재생하는 기존 흐름을 깨지 않는다.
```
