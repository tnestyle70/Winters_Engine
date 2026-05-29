# Ch17. Winters Editor, Asset Importer, Asset Loader

이 문서는 Winters Engine에 에디터, 에셋 임포터, 에셋 로더를 제대로 세우기 위한 개념서다. 목표는 단순히 "파일을 읽는다"가 아니라, 개발자가 에디터에서 소스 파일을 가져오고, 엔진 포맷으로 변환하고, 런타임이 안정적으로 로드하고, 변경 사항이 다시 에디터와 게임에 반영되는 전체 순환을 닫는 것이다.

현재 Winters에는 이미 중요한 기반이 있다.

- `Tools/WintersAssetConverter`: Assimp로 `.fbx/.gltf/.obj` 계열 소스를 읽어 `.wmesh/.wmat/.wskel/.wanim`으로 굽는 도구.
- `Engine/Public/AssetFormat/*`: Winters cooked 포맷의 reader/writer.
- `Engine/Public/Resource/*`: `CModel`, `CTexture`, `CResourceCache` 중심의 동기 런타임 로더.
- `Client/Public/Scene/Scene_Editor.h`와 `Client/Private/Scene/Scene_Editor.cpp`: 맵 오브젝트, 미니언 웨이포인트, NavGrid 편집용 ImGui 에디터 씬.
- `Engine/Public/Editor/ImGuiLayer.h`: ImGui 컨텍스트, docking, 폰트, DX11 backend 초기화.

따라서 이번 구축의 올바른 방향은 "없는 것을 한 번에 거대하게 새로 만든다"가 아니라, 이미 살아 있는 포맷과 로더를 기준으로 그 앞뒤를 채우는 것이다.

## 1. 가장 기초 원리

게임 엔진의 에셋 시스템은 보통 네 종류의 파일과 상태를 구분한다.

첫째, source asset이다. 아티스트나 디자이너가 만든 원본 파일이다. 예를 들면 `.fbx`, `.gltf`, `.png`, `.psd`, `.wav`, `.json`, `.hlsl` 같은 파일이다. 이 파일들은 사람이 편집하기 좋지만 런타임이 바로 쓰기에는 느리거나, 너무 자유롭거나, 플랫폼별로 불안정하다.

둘째, imported metadata다. 원본 파일을 엔진이 어떻게 해석해야 하는지 저장하는 sidecar 데이터다. 예를 들면 "이 FBX는 X축 미러를 켠다", "스케일은 0.01이다", "이 텍스처는 sRGB로 읽는다", "이 메시의 skeleton은 같은 폴더의 `body.wskel`이다" 같은 설정이다. Unreal의 `.uasset`이나 Unity의 `.meta`가 이 역할을 한다. Winters에서는 초기에 `.wasset` sidecar를 두는 것이 가장 단순하다.

셋째, cooked asset이다. 런타임이 빠르게 읽을 수 있도록 엔진 전용 포맷으로 변환한 결과물이다. Winters에서는 이미 `.wmesh`, `.wmat`, `.wskel`, `.wanim`, `.wfx`가 이 역할을 한다. cooked 파일은 사람이 직접 고치는 대상이 아니라 importer가 재생성하는 산출물이다.

넷째, runtime resource다. cooked asset을 메모리에 읽고, 필요하면 GPU 리소스로 올린 객체다. `CTexture`, `CModel`, `CMesh`, `CSkeleton`, `CAnimation`, `ID3D11ShaderResourceView`, vertex buffer, index buffer 등이 여기에 속한다.

핵심은 이 네 층을 섞지 않는 것이다.

```text
Source Asset
  예: Annie.fbx, diffuse.png
  사람이 편집한다.

Import Metadata
  예: Annie.wasset
  엔진이 원본을 어떻게 cook할지 기억한다.

Cooked Asset
  예: Annie.wmesh, Annie.wmat, Annie.wskel, anims/idle.wanim
  런타임 로딩 최적화 포맷이다.

Runtime Resource
  예: CModel, CTexture, CAnimation
  게임 프레임에서 바로 쓰는 객체다.
```

이 구분이 없으면 처음에는 편해 보이지만 금방 무너진다. 런타임 코드가 `.fbx`를 직접 읽기 시작하고, 에디터가 cooked 파일만 알고, importer 설정이 배치 파일에만 숨어 있으면 재임포트, 캐시 무효화, 버전 변경, 팀 협업이 전부 고통스러워진다.

## 2. Importer와 Loader의 차이

임포터와 로더는 이름이 비슷하지만 책임이 다르다.

Importer는 개발 시간 도구다. 느려도 된다. Assimp를 써도 되고, 원본 파일을 여러 번 훑어도 되고, 경고를 많이 뱉어도 된다. 중요한 것은 원본의 애매함을 엔진 규칙으로 확정하는 것이다.

Loader는 런타임 도구다. 빨라야 한다. 예측 가능해야 한다. 가능하면 할당을 줄이고, 검증 실패를 빨리 반환하고, GPU 리소스 생성 시점을 통제해야 한다. 런타임 loader는 `.fbx` 같은 source asset에 의존하지 말고 `.wmesh/.wmat/.wskel/.wanim` 같은 cooked asset을 읽어야 한다.

Winters의 현재 상태로 보면:

- `CWMeshWriter::WriteFromAssimp`: importer 쪽 책임.
- `CWMeshLoader::Load`: cooked loader 쪽 책임.
- `CModel::LoadModel`: cooked asset을 런타임 resource로 만드는 책임.
- `CResourceCache::LoadModel`: 같은 모델을 중복 생성하지 않도록 캐싱하는 책임.

앞으로 추가할 시스템은 이 경계를 더 분명하게 만들어야 한다.

```text
Importer path:
source.fbx + import settings
  -> Assimp scene
  -> CWMeshWriter / CWMaterialWriter / CWSkelWriter / CWAnimWriter
  -> .wmesh/.wmat/.wskel/.wanim
  -> .wasset manifest 갱신

Loader path:
virtual asset path
  -> AssetRegistry 조회
  -> cooked path 해석
  -> CWMeshLoader/CWMaterialLoader/CWSkelLoader/CWAnimLoader
  -> CModel/CTexture/CAnimation 생성
  -> CResourceCache 또는 AssetHandle에 저장
```

## 3. 왜 AssetRegistry가 필요한가

지금도 `CResourceCache`는 경로 기반 캐시 역할을 한다. 그런데 완성형 에셋 시스템에는 경로 캐시만으로 부족한 영역이 있다.

첫째, 에셋 identity가 필요하다. 파일 경로는 쉽게 바뀐다. `Irelia/body.wmesh`가 `Champions/Irelia/Skins/Base/body.wmesh`로 이동해도 논리적으로는 같은 에셋일 수 있다. 그래서 에셋에는 stable guid 또는 stable key가 필요하다.

둘째, dependency graph가 필요하다. 모델 하나는 메시 하나만 읽지 않는다. `.wmesh`는 `.wmat`를 요구하고, `.wmat`는 텍스처를 요구하며, skinned mesh는 `.wskel`과 여러 `.wanim`을 요구한다. 하나를 재임포트하면 무엇을 다시 로드해야 하는지 알아야 한다.

셋째, 에디터 UI가 전체 에셋 목록을 알아야 한다. Content Browser는 디렉터리 파일 목록만 보여주는 것이 아니라 type, import 상태, cooked 상태, 마지막 오류, 참조 대상, preview 가능 여부를 보여줘야 한다.

넷째, 로딩 정책이 필요하다. 어떤 에셋은 즉시 로드하고, 어떤 에셋은 챔피언 선택 단계에서 prefetch하고, 어떤 에셋은 실제 사용 직전에 lazy load해야 한다. 이 판단은 단순 파일 함수보다 registry가 갖는 편이 낫다.

Winters에서는 첫 버전의 AssetRegistry가 거창할 필요는 없다. 다음 정보만 안정적으로 들고 있어도 큰 변화가 생긴다.

```text
AssetRecord
  guid
  virtualPath
  sourcePath
  cookedPrimaryPath
  type
  importerVersion
  sourceTimestamp
  sourceHash
  dependencies
  lastImportStatus
```

## 4. Virtual Path가 필요한 이유

런타임 코드가 `Client/Bin/Resource/Texture/MAP/output/sr_base_flip.wmesh` 같은 물리 경로를 직접 많이 들고 있으면, 폴더 구조가 바뀔 때 코드가 흔들린다. 에셋 시스템은 물리 경로 대신 논리 경로를 중심에 둬야 한다.

예:

```text
Physical path:
Client/Bin/Resource/Model/Irelia/Irelia.wmesh

Virtual path:
/Game/Champions/Irelia/Base/Irelia
```

Virtual path는 사람이 읽기 좋고, 에디터가 검색하기 좋고, 파일 이동과 cooked output 정책을 숨기기 좋다. 내부적으로는 registry가 virtual path를 cooked path로 해석한다.

Winters의 첫 단계에서는 모든 기존 코드를 곧장 virtual path로 갈아엎을 필요는 없다. 기존 `CResourceCache::LoadModel(path)`는 유지하고, 새 함수로 `LoadModelAsset(virtualPath)` 또는 `LoadModelByAssetPath(assetPath)`를 추가하는 식이 안전하다.

## 5. Cook의 본질

Cook은 원본을 런타임 친화 형태로 확정하는 과정이다.

메시 cook은 정점 레이아웃을 확정한다. Winters의 `.wmesh`는 static stride와 skinned stride가 이미 정해져 있다. 런타임은 "이 FBX가 tangent를 갖고 있나" 같은 고민을 하지 않고, `.wmesh`의 `vertex_stride`와 `vertex_format_flags`만 확인하면 된다.

머티리얼 cook은 외부 DCC의 재질 표현을 게임 렌더러가 이해하는 값으로 줄인다. Winters의 `.wmat`는 지금 diffuse texture path 중심이지만, 이후 PBR 파라미터, alpha mode, shader domain, texture slots, material hash를 확장할 수 있다.

스켈레톤 cook은 bone order를 확정한다. 애니메이션과 메시가 같은 bone index를 쓰도록 `.wskel`의 bone order와 `.wmesh`의 skinning index를 맞춰야 한다.

애니메이션 cook은 keyframe channel과 skeleton hash를 확정한다. Winters의 `.wanim` trailer에는 `skel_hash`가 있어서 런타임 mismatch를 막을 수 있다.

중요한 원칙은 deterministic cook이다. 같은 source와 같은 import settings를 넣으면 같은 cooked output이 나와야 한다. 그래야 cache, CI, 팀 협업, 문제 재현이 가능하다.

## 6. Import Settings

Import settings는 source 파일마다 다르다. 메시의 경우 최소한 다음이 필요하다.

- source path
- output directory
- scale
- mirror X
- generate bounds
- skeleton path
- material output policy
- animation output directory
- excluded node prefix
- importer version

텍스처의 경우 다음이 필요하다.

- color space: auto, sRGB, linear
- compression preset: none, UI, normal, albedo, mask
- mipmap policy
- sampler default: wrap, clamp
- alpha mode
- output extension

사운드의 경우 다음이 필요하다.

- streaming 여부
- loop 여부
- volume normalization 여부
- output bank/group

Winters 첫 버전에서 모든 항목을 한 번에 만들 필요는 없다. 그러나 파일 구조는 나중에 늘릴 수 있게 둬야 한다. 그래서 `.wasset` manifest에 `key=value`나 단순 JSON 계열을 두고, unknown key를 무시하는 파서를 쓰는 것이 좋다.

## 7. Dependency Graph

에셋은 혼자 존재하지 않는다.

예를 들어 챔피언 모델 하나의 dependency graph는 이렇게 생긴다.

```text
/Game/Champions/Irelia/Base/Irelia
  -> Irelia.wmesh
  -> Irelia.wmat
       -> diffuse.dds
       -> normal.dds
  -> Irelia.wskel
  -> anims/idle.wanim
  -> anims/run.wanim
  -> anims/attack1.wanim
```

이 graph가 있으면 다음 질문에 답할 수 있다.

- diffuse texture가 바뀌면 어떤 material과 model preview를 갱신해야 하는가.
- skeleton이 바뀌면 어떤 animation을 다시 검증해야 하는가.
- 챔피언 선택 화면에서 어떤 파일을 미리 읽어야 하는가.
- 런타임 unload 시 어떤 자원이 아직 다른 에셋에서 참조 중인가.

첫 단계에서는 dependency graph를 완전한 그래프 DB로 만들 필요는 없다. `.wasset`에 직접 dependency path 목록을 넣고, AssetRegistry가 그것을 scan하면 충분하다.

## 8. CPU Load와 GPU Finalize 분리

비동기 로딩을 하려면 CPU 로드와 GPU finalize를 분리해야 한다.

CPU load는 파일 IO, 압축 해제, binary parse, validation, CPU-side 구조 생성이다. worker thread에서 할 수 있다.

GPU finalize는 vertex buffer, index buffer, texture SRV, sampler, pipeline resource를 만드는 단계다. DX11 immediate context나 renderer state와 얽히기 쉬워 main/render thread에서 처리하는 편이 안전하다.

현재 `CModel::Create`는 파일 읽기와 GPU mesh 생성이 한 함수 안에 묶여 있다. 그래서 첫 async 버전은 무리하게 완전 분리하지 말고, 단계적으로 간다.

1. 먼저 registry와 cache API를 세워 기존 동기 로드를 감싼다.
2. 다음으로 cooked file parse를 CPU payload로 분리한다.
3. 마지막으로 main thread finalize queue를 도입한다.

이 순서가 중요한 이유는, 한 번에 background thread에서 `CModel::Create`를 돌리면 DX11 리소스 생성 경계가 흐려지고 재현 어려운 크래시가 생길 수 있기 때문이다.

## 9. Cache와 Handle

캐시는 "같은 파일을 두 번 만들지 않는다"는 약속이다. 하지만 완성형 캐시는 단순 map보다 조금 더 많은 정책을 가진다.

- key normalization: slash 통일, 대소문자 통일, color space 등 load option 포함.
- strong reference: 지금 당장 쓰는 asset은 유지.
- weak reference: 다시 쓰일 수 있지만 버려도 되는 asset.
- invalidation: 파일 변경, reimport, 수동 reload 때 기존 객체를 버리거나 교체.
- stats: cached count, memory bytes, last access frame.

Handle은 사용자가 캐시 내부 포인터를 직접 오래 붙잡지 않게 하는 장치다. 첫 단계에서는 `shared_ptr<CModel>`이 이미 모델에 쓰이고 있으므로 그대로 활용할 수 있다. 이후에는 `AssetHandle<T>`로 상태를 표현할 수 있다.

```text
Unloaded -> Loading -> CpuReady -> GpuReady -> Failed
```

핸들은 이 상태를 드러내야 한다. 에디터 preview는 `GpuReady`일 때만 렌더하고, `Loading`이면 spinner, `Failed`이면 오류 메시지를 보여준다.

## 10. Hot Reload

Hot reload는 "파일이 바뀌면 자동으로 반영한다"가 전부가 아니다. 안전하게 하려면 단계가 필요하다.

1. 파일 변경 감지.
2. 변경 파일이 source인지 cooked인지 manifest인지 분류.
3. source라면 importer job 생성.
4. cooked output 갱신 완료 후 registry record 업데이트.
5. runtime cache invalidation.
6. 이미 화면에 표시 중인 preview와 scene object 갱신.
7. 실패 시 이전 정상 cooked asset 유지.

마지막 항목이 특히 중요하다. importer가 실패했다고 현재 게임 화면에서 멀쩡히 쓰던 모델까지 날리면 에디터 사용감이 나빠진다. 따라서 reimport는 성공한 output을 atomic하게 교체해야 한다. 임시 파일에 쓰고, 검증한 뒤 rename하는 방식이 안전하다.

## 11. Editor의 세 층

에디터는 UI만이 아니다. 세 층으로 나눠야 한다.

첫째, tool state다. 선택된 에셋, 선택된 엔티티, 현재 import job, 필터 문자열, preview camera 같은 상태다.

둘째, command/transaction이다. 에디터 조작은 undo/redo 가능해야 한다. 초기에는 asset import와 scene placement 정도만 command로 기록해도 된다.

셋째, view/panel이다. Content Browser, Inspector, Import Queue, Preview Viewport, Output Log 같은 화면이다.

현재 `CScene_Editor`는 맵 배치 에디터이므로, 첫 적용은 이 씬에 Content Browser와 Import Queue 패널을 추가하는 것이 현실적이다. 장기적으로는 `Engine/Public/Editor` 아래에 generic panel framework를 두고, 게임별 에디터가 패널을 등록하게 만들면 좋다.

## 12. Content Browser의 최소 기능

처음부터 Unreal급 Content Browser를 만들 필요는 없다. 그러나 다음 기능은 MVP에 포함되어야 한다.

- root directory scan.
- extension별 type 표시.
- search/filter.
- selected asset inspector.
- source path와 cooked path 표시.
- import/reimport button.
- last import result 표시.
- model/texture preview hook.
- double click load smoke.

여기서 preview는 욕심내지 않는다. 텍스처는 ImGui image로 보여주고, 모델은 우선 `ModelRenderer` 또는 기존 scene camera에 임시 preview entity를 띄우는 방식이면 충분하다.

## 13. Import Queue

임포트는 즉시 실행처럼 보여도 내부적으로는 job이다.

```text
AssetImportJob
  source path
  destination path
  asset type
  import settings
  status
  started time
  finished time
  output files
  warnings
  errors
```

job queue가 있으면 batch import, cancel, retry, progress 표시가 가능하다. 첫 버전은 main thread에서 blocking 실행해도 된다. 중요한 것은 UI와 도구가 job이라는 모델을 공유하는 것이다. 나중에 worker thread나 외부 process 실행으로 바꿔도 UI 구조가 유지된다.

## 14. Validation

에셋 파이프라인의 품질은 validation에서 갈린다.

메시 validation:

- vertex count와 index count 상한 확인.
- submesh count 상한 확인.
- material index 범위 확인.
- skinned mesh의 bone count가 shader limit 이내인지 확인.
- bounds 존재 여부 확인.
- skeleton과 mesh bone hash 일치 확인.

텍스처 validation:

- 파일 존재.
- 해상도 상한.
- sRGB/linear 설정.
- normal map이 sRGB로 들어오지 않았는지 확인.
- UI texture가 mipmap/compression 정책을 어기지 않았는지 확인.

애니메이션 validation:

- expected skeleton hash 일치.
- duration과 ticks per second 유효성.
- channel bone index 범위.
- loop 여부.
- event time이 duration 안에 있는지 확인.

Importer validation:

- source file timestamp/hash 기록.
- importer version 기록.
- output 생성 여부.
- generated outputs를 다시 loader로 열어보는 roundtrip 확인.

Winters에는 이미 loader validation이 어느 정도 들어 있다. 예를 들어 `CWMeshLoader`는 magic, version, flags, vertex stride, index stride, bone count 등을 검증한다. 다음 단계는 importer가 출력 직후 loader로 다시 읽는 roundtrip 검증을 표준화하는 것이다.

## 15. Build와 CI

에셋 파이프라인은 로컬 에디터만의 기능이 아니다. 빌드 시스템과 연결되어야 한다.

초기 로컬 명령:

```text
Tools/Bin/Debug/WintersAssetConverter.exe import <source> -o <out>
Tools/Bin/Debug/WintersAssetConverter.exe scan Data/SourceAssets -manifest Data/AssetManifest.wdb
Tools/Bin/Debug/WintersAssetConverter.exe validate Client/Bin/Resource
```

장기 CI:

```text
1. source asset scan
2. changed source만 reimport
3. cooked output validation
4. runtime smoke load
5. Client/Server build
6. packaged resource copy
```

Visual Studio 프로젝트는 수동 XML 등록이 필요할 수 있지만, 현재 CMake 쪽은 `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)`를 쓰기 때문에 새 Engine source는 CMake에서 자동 발견된다. 계획서에서는 `.vcxproj` 직접 수정 요구가 없으면 검증 섹션에 "새 파일 프로젝트 포함 확인"으로 남기는 것이 규칙에 맞다.

## 16. Winters에 맞춘 최종 구조

최종 구조는 다음처럼 잡는다.

```text
Engine/Public/Asset
  AssetTypes.h
  AssetPath.h
  AssetManifest.h
  AssetDatabase.h
  AssetImportTypes.h
  IAssetImporter.h
  AssetLoader.h
  AssetWatcher.h
  AssetValidator.h

Engine/Private/Asset
  AssetPath.cpp
  AssetManifest.cpp
  AssetDatabase.cpp
  AssetLoader.cpp
  AssetWatcher.cpp
  AssetValidator.cpp

Engine/Private/Tools/AssetConverter
  main.cpp
  AssetConverterCommands.h
  AssetConverterCommands.cpp

Client/Public/Editor
  ContentBrowserPanel.h
  AssetImportPanel.h

Client/Private/Editor
  ContentBrowserPanel.cpp
  AssetImportPanel.cpp
```

단, 첫 세션부터 이 전체 구조를 모두 만들면 과하다. 실제 적용은 다음 순서가 안전하다.

```text
Session 01: Asset identity, path, manifest, database
Session 02: AssetConverter import/reimport/validate command
Session 03: Runtime loader/cache bridge
Session 04: Scene_Editor content browser/import panel
Session 05: Hot reload, validation, build integration
```

## 17. 성공 기준

이 작업이 성공했다는 말은 다음이 되는 상태다.

- `.fbx` 하나를 에디터에서 선택해 import하면 `.wmesh/.wmat`가 생성된다.
- skeleton 옵션이 있는 source는 `.wskel`과 `anims/*.wanim`까지 생성된다.
- 생성 직후 loader roundtrip validation이 돈다.
- Content Browser가 source, cooked, manifest 상태를 보여준다.
- Runtime은 asset path로 모델과 텍스처를 가져오고, 중복 로드를 하지 않는다.
- Reimport 성공 시 캐시가 무효화되고 preview가 갱신된다.
- Reimport 실패 시 이전 정상 asset이 유지된다.
- `git diff --check`와 Debug x64 build가 통과한다.

이 전체 흐름이 닫히면 Winters의 에셋 파이프라인은 "파일 변환 스크립트 묶음"이 아니라 엔진의 정식 subsystem이 된다.
