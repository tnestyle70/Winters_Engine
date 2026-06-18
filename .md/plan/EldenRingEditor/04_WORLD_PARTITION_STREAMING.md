Session - Limgrave World Partition streaming seed

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/EldenRingClient/Public/EldenRingWorldPartition.h

새 파일:

```cpp
CONFIRM_NEEDED - 전체 파일 본문은 camera/viewer 타입과 map assembly transform 계약 확인 후 작성한다.

책임:
- mapId 기반 grid key를 가진 cell을 관리한다.
- active radius, preload radius, unload delay를 가진다.
- cell 상태를 Unloaded, Loading, Loaded, Visible로 구분한다.
- 현재는 async IO를 바로 넣지 않고 main-thread deterministic load로 시작한다.
- transform이 없는 cell은 "catalog preview only"로 두고, 배치 가능한 instance만 renderer에 넘긴다.
```

1-2. C:/Users/tnest/Desktop/Winters/EldenRingClient/Private/EldenRingWorldPartition.cpp

새 파일:

```cpp
CONFIRM_NEEDED - CEldenRingAssetProbeScene의 camera source와 CEldenRingMapAssembly의 transform 지원 여부 확인 후 전체 구현 본문을 작성한다.
구현 기준:
- m60_42_36_00을 초기 loaded cell로 고정한다.
- viewer world position이 없으면 editor debug slider의 fake camera position으로 cell visibility를 검증한다.
- Load/Unload는 CRHIFxMeshResourceCache PreloadMesh/Find 상태와 연결한다.
- OutputDebugStringA에 loaded/visible/unloaded count를 bounded log로 남긴다.
```

1-3. C:/Users/tnest/Desktop/Winters/EldenRingClient/Public/EldenRingProbeScene.h

기존 코드:

```cpp
	std::unique_ptr<CEldenRingMapAssembly> m_pMapAssembly;
```

아래에 추가:

```cpp
	std::unique_ptr<CEldenRingWorldPartition> m_pWorldPartition;
	Vec3 m_vEditorCameraWorld{ 0.f, 0.f, 0.f };
	int m_iWorldPartitionActiveRadius = 1;
	int m_iWorldPartitionPreloadRadius = 2;
```

1-4. C:/Users/tnest/Desktop/Winters/EldenRingClient/Private/EldenAssetProbeScene.cpp

기존 코드:

```cpp
void CEldenRingAssetProbeScene::OnUpdate(f32_t deltaTime)
{
    if (m_pCubeRenderer)
        m_pCubeRenderer->Update(deltaTime);
}
```

아래로 교체:

```cpp
void CEldenRingAssetProbeScene::OnUpdate(f32_t deltaTime)
{
    if (m_pCubeRenderer)
        m_pCubeRenderer->Update(deltaTime);

    if (m_pWorldPartition)
    {
        m_pWorldPartition->SetActiveRadius(m_iWorldPartitionActiveRadius);
        m_pWorldPartition->SetPreloadRadius(m_iWorldPartitionPreloadRadius);
        m_pWorldPartition->Update(m_vEditorCameraWorld, deltaTime);
    }
}
```

기존 코드:

```cpp
    if (m_pStaticMeshRenderer && m_bSmokeMeshReady)
    {
```

아래에 추가:

```cpp
        CONFIRM_NEEDED - WorldPartition이 반환하는 visible instances 타입이 확정되면,
        smoke mesh 1개 대신 visible instance 목록을 순회해서 DrawMesh를 호출한다.
        단, transform 없는 map reference는 이 렌더 루프에 넣지 않는다.
```

1-5. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_Editor.cpp

삭제할 코드/범위:

```cpp
이번 세션에서는 기존 LoL Scene_Editor를 수정하지 않는다.
EldenRingClient에서 partition smoke가 검증된 뒤, 필요한 panel과 loader만 Client editor로 역이식한다.
```

2. 검증

2-1. 검증 명령

```powershell
msbuild Winters.sln /t:EldenRingClient /p:Configuration=Debug-DX12 /p:Platform=x64
```

2-2. 런타임 확인

```text
1. Editor panel에서 active/preload radius 값을 바꿀 수 있다.
2. fake camera position을 이동하면 visible/loaded/unloaded count가 변한다.
3. transform 없는 m60_42_36_00 reference는 streaming count에는 들어가도 renderer draw call에는 들어가지 않는다.
4. 정상 F5 LoL/Winters flow의 roster, map, minion, snapshot, champion system을 숨기지 않는다.
```

2-3. 다음 세션 게이트

```text
World Partition은 "많은 리소스를 한 번에 로드하지 않는 에디터 운영 모델"이 목적이다.
실제 Limgrave 전체 배치는 MSB transform parser가 들어온 뒤, cell instance transform이 검증된 다음에만 활성화한다.
```
