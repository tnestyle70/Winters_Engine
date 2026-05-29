---
name: library-reference
description: >
  WintersEngine 클래스 API 사용법 조회.
  "어떻게 써?", "함수 뭐 있어?", "사용법" 등 API 질문에 트리거.
---

# Skill: Library Reference — WintersEngine API

## 실행 순서
1. `references/engine-full-api.md` 읽기
2. 해당 클래스 섹션 찾기 → 사용 예시와 함께 답변

## 현재 구현된 클래스

### 공개 API (Engine/Include/)
WintersMath, CTransform, CCamera, CInput, CubeRenderer, TriangleRenderer, IWintersApp, WintersRun

### 내부 API (Engine/Public/)
CDX11Device, DX11Shader, DX11Buffer, DX11Pipeline, DX11ConstantBuffer\<T\>, CubeGeometry, CEngineApp

### ECS (워크트리 작업 완료, main 미머지)
EntityManager, ComponentStore\<T\>, World, ISystem, SystemScheduler, CommandBuffer

## LoL 30일 모작 — Phase별 추가 예정 클래스

### Phase 0: Resource
CMeshLoader, CAnimLoader, CTextureLoader, CMaterialLoader, AssetFormat.h

### Phase 1: Core 강화
CJobSystem(Fiber), CJobCounter, CLinearAllocator, CPoolAllocator, CEventBus

### Phase 2: Renderer 확장
CRenderGraph, CGBuffer, CShadowMap, CPostFXPipeline, CClusteredLightMgr

### Phase 3: GPU-Driven & Profiling
DX11IndirectDraw, DX11GPUCuller, CProfiler, CDisplaySettings

### Phase 4: Network
CUDPSocket, CKCPTransport, CPacketSerializer, CClientNet, CNetworkPrediction

### Phase 4: Server
CIOCPServer, CSession, CSessionMgr, CGameRoom, CGameLogic, CServerWorld, CAOIManager, CLagCompensation

### Phase 6: AntiCheat
CAntiCheatServer, WintersGuard.sys(커널), WintersGuardService.exe(유저모드)

### Phase 7: Editor
CImGuiRenderer, CEditorWindow, CSceneHierarchy, CInspector, CMapEditor, CChampionEditor
