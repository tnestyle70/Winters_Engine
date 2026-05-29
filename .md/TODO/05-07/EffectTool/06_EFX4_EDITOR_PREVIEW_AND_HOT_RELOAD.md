# EFX-4 Editor Preview와 Hot Reload

작성일: 2026-05-07
상태: 구현 계획
의존:
- `05_EFX3_DX12_MASTER_MATERIAL_RENDERER.md`
- `.md/plan/EffectTool/24_EDITOR_7_PANELS_BAKE.md`
- `.md/plan/EffectTool/26_HOT_RELOAD_AND_COOKED_BINARY_BAKE.md`

목적:
- 기존 Irelia 전용 `EffectTuner`를 Material Instance 중심의 EffectTool panel로 확장한다.
- 노드 그래프 에디터보다 먼저 `.wfx`/`.wmi`를 열고, core knob를 조정하고, preview spawn을 확인하는 워크플로우를 만든다.
- Hot reload는 manual reload부터 시작하고, async compile은 2차로 넣는다.

---

## 1. 현재 기준선

현재 파일:

```txt
Client/Public/UI/EffectTuner.h
Client/Private/UI/EffectTuner.cpp
Client/Private/Scene/InGameDebugBridge.cpp
```

현재 기능:

```txt
1. Irelia preset combo
2. Color/Width/Height/Lifetime/Atlas/UV/Blend slider
3. Spawn Test button
4. Save Preset button은 clipboard code를 생성
```

현재 한계:

```txt
1. Irelia 전용이다.
2. `.wfx`/`.wmi`를 열거나 저장하지 않는다.
3. Material Instance parameter layout을 모른다.
4. Preview viewport가 없다.
5. Hot reload는 없다.
```

---

## 2. 결정

```txt
1. EFX-4 1차는 Client ImGui panel에서 시작한다.
2. 별도 WintersEditor.exe는 DX12 RHI와 ImGui DX12 backend가 안정된 뒤 분리한다.
3. 노드 GraphPanel은 후순위다.
4. Material Instance core knob panel이 1차 디자이너 워크플로우다.
5. `.wfx` system과 `.wmi` material instance를 직접 저장한다.
6. Hot reload 1차는 manual reload button이다.
7. Async recompile은 CShaderCompiler와 EFX-3 PSO rebuild가 안정된 뒤 넣는다.
```

---

## 3. 신규 파일

Client panel:

```txt
Client/Public/UI/EffectToolPanel.h
Client/Private/UI/EffectToolPanel.cpp
```

Engine editor utility:

```txt
Engine/Public/FX/v2/Editor/FxEditSession.h
Engine/Public/FX/v2/Editor/FxPreviewController.h
Engine/Private/FX/v2/Editor/FxEditSession.cpp
Engine/Private/FX/v2/Editor/FxPreviewController.cpp
```

추후 별도 editor:

```txt
Tools/WintersEditor/WintersEditor.vcxproj
Tools/WintersEditor/Public/FX/FxEditorTab.h
Tools/WintersEditor/Private/FX/FxEditorTab.cpp
```

EFX-4 1차에서 `Tools/WintersEditor`는 만들지 않아도 된다.

---

## 4. Panel 구성

1차 panel:

```txt
Asset Browser
  .wfx list
  .wmi list
  reload button

System Panel
  system name
  emitters list
  selected emitter renderer type

Material Panel
  master material name
  core knob sliders
  advanced toggle
  texture path inputs

Curve Panel
  color over life preview
  alpha over life preview
  1차는 control point table, graph UI는 후순위

Preview Panel
  Spawn selected system at player
  Reset preview instances
  Loop toggle

Compile Log
  JSON parse errors
  shader compile errors
  asset reload messages
```

2차 panel:

```txt
Graph Panel
ScratchPad
GPU compute debug
DataInterface inspector
```

---

## 5. Core knob

항상 상단에 고정 노출:

```txt
TintColor
EmissionColor
EmissionIntensity
Contrast
DissolveThreshold
DistortionStrength
UvPanA
FresnelPower
```

Advanced:

```txt
EdgeWidth
EdgeColor
CenterMaskPower
SoftParticleDistance
Atlas settings
RandomVariation
PolarRotation
SixWayLightingScale
VolumetricDensity
```

디자인 이유:

```txt
LoL FX를 빠르게 깎는 데 필요한 값은 대부분 core knob에 있다.
Graph node를 열기 전에 `.wmi` 값만으로 fire/ice/poison/void 변주를 만들어야 한다.
```

---

## 6. Hot reload 1차

Manual reload sequence:

```txt
1. 사용자가 Material Panel에서 slider 변경
2. CFxEditSession이 dirty flag 설정
3. Save 버튼 클릭
4. .wmi canonical save
5. Reload Current 버튼 클릭
6. CFxMaterialInstanceJsonLoader load
7. PreviewController Reset
8. 다음 frame preview에 반영
```

완료 기준:

```txt
[ ] slider 변경 후 .wmi 저장
[ ] reload 후 preview 색/알파/UV 변화 확인
[ ] 기존 InGame smoke와 충돌 없음
```

---

## 7. Hot reload 2차

Async reload sequence:

```txt
1. file watcher 또는 Save 이벤트 발생
2. compile queue에 job push
3. worker thread가 .wfx/.wmi load + shader compile 필요 여부 판정
4. Game thread에 result enqueue
5. registry replace
6. active preview reset
7. compile log 갱신
```

주의:

```txt
1. DX12 PSO rebuild는 render thread와 race가 나면 안 된다.
2. 첫 구현은 active preview instance만 reset한다.
3. 실제 gameplay active FX hot reload는 EFX-8에서 확장한다.
```

완료 기준:

```txt
[ ] Save 후 200ms 이내 reload target
[ ] compile fail 시 기존 material 유지
[ ] error log 표시
```

---

## 8. Preview 위치

1차:

```txt
InGame player position에 preview spawn
```

이유:

```txt
현재 Client debug panel과 world/camera/fx renderer가 이미 있다.
별도 render target viewport를 만들기 전에도 바로 시각 검증 가능하다.
```

2차:

```txt
Offscreen preview render target
ImGui::Image
camera orbit
grid floor
```

DX12 backend:

```txt
ImGui_ImplDX12_Init
SRV descriptor heap
Preview RTV/SRV texture
```

---

## 9. Save path

기본 저장 위치:

```txt
Client/Bin/Resource/FX/LoL/{Champion}/{AssetName}.wfx
Client/Bin/Resource/FX/LoL/{Champion}/MI_{AssetName}.wmi
Client/Bin/Resource/FX/Common/Materials/{SharedName}.wmi
```

Elden 저장 위치:

```txt
Client/Bin/Resource/FX/Elden/{Category}/{AssetName}.wfx
Client/Bin/Resource/FX/Elden/{Category}/MI_{AssetName}.wmi
```

path 규칙:

```txt
1. SolutionDir 기준 상대 경로.
2. slash는 `/`.
3. C++ string literal 절대 경로 금지.
4. Editor panel text input에서도 `\` 입력 시 `/`로 normalize.
```

---

## 10. 구현 단계

### EFX4-1. EffectToolPanel shell

완료 기준:

```txt
[ ] InGameDebugBridge에서 EffectToolPanel render
[ ] 기존 EffectTuner와 동시 표시 가능
[ ] .wfx/.wmi path input
```

### EFX4-2. Material Panel

완료 기준:

```txt
[ ] .wmi load
[ ] core knob slider 표시
[ ] save canonical
[ ] reload preview
```

### EFX4-3. System Panel

완료 기준:

```txt
[ ] .wfx load
[ ] emitter list 표시
[ ] selected emitter renderer type 표시
[ ] material instance 연결 확인
```

### EFX4-4. Preview spawn

완료 기준:

```txt
[ ] player position spawn
[ ] loop preview
[ ] reset all preview instances
[ ] preview instance tag로 gameplay FX와 구분
```

### EFX4-5. Compile log

완료 기준:

```txt
[ ] JSON parse error 출력
[ ] missing texture path 출력
[ ] shader compile error 출력
[ ] last reload time 표시
```

---

## 11. 검증

Manual test:

```txt
1. EffectToolPanel 열기
2. Irelia_Q_Trail.wfx load
3. MI_Irelia_Q_Trail.wmi load
4. EmissionIntensity 3 -> 12 변경
5. Save
6. Reload Current
7. Spawn Preview
8. bloom/intensity 변화 확인
```

Grep:

```powershell
rg "IreliaFx::" Client/Private/UI/EffectToolPanel.cpp
rg "Scene_InGame" Engine/Public/FX/v2 Engine/Private/FX/v2
rg "\\\\Users\\\\|C:\\\\" Client/Bin/Resource/FX Engine/Public/FX/v2 Engine/Private/FX/v2
```

기대:

```txt
EffectToolPanel은 champion-specific function을 직접 호출하지 않는다.
Engine FX v2는 Scene_InGame을 모른다.
FX asset에는 절대 경로가 없다.
```

완료 기준:

```txt
[ ] Material Instance edit-save-reload
[ ] Preview spawn
[ ] 기존 EffectTuner 기능 보존
[ ] Client Debug build
[ ] direct local InGame smoke
```

