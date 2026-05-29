# S06. RenderQueue, Counters, Culling

목표: RHI 전환과 동시에 draw call 최적화가 가능한 renderer 구조로 바꾼다.

## 문제

현재 `Scene_InGame::OnRender()`는 렌더러를 직접 순서대로 호출한다. 이 구조에서는 다음이 어렵다.

- draw call counter
- material/model sorting
- static instancing
- frustum culling
- render pass 분리
- GPU-driven 확장

## 새 구조

```text
Engine/Public/Renderer/RenderItem.h
Engine/Public/Renderer/RenderQueue.h
Engine/Public/Renderer/Renderer.h
Engine/Private/Renderer/Renderer.cpp
```

## RenderItem 최소 필드

- `EntityID entity`
- `Mat4 world`
- `RHIMeshHandle` 또는 임시 `CModel*`
- `RHIMaterialHandle` 또는 임시 `CTexture*`
- `RenderLayer`
- `RenderFlags`
- `Bounds`
- `sortKey`

## 1차 목표

1. draw call counter 추가
2. triangle counter 추가
3. `RenderQueue::Submit(RenderItem)` 추가
4. Scene 직접 render 호출을 queue submit으로 줄이기
5. CPU frustum culling 추가
6. static object render skip과 anim skip 연결

## Profiler counter

필수 counter:

- `Render::DrawCalls`
- `Render::Triangles`
- `Render::ItemsSubmitted`
- `Render::ItemsCulled`
- `Render::StaticInstances`
- `Render::StateChanges`

## 합격 기준

```powershell
rg -n "Render::DrawCalls|Render::Triangles|RenderQueue|RenderItem" Engine Client
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64
```

런타임에서 F3 profiler에 draw/triangle counter가 보이면 합격.

## 후속

Static instancing은 RHI CommandList 위에서 `DrawIndexedInstanced`로 먼저 구현한다. GPU-driven indirect draw는 그 다음이다.
