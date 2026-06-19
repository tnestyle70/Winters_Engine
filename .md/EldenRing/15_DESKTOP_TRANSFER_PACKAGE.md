# EldenRing Desktop Transfer Package

2026-06-18 기준. 이 문서는 노트북에서 만든 EldenRing 작업 환경을 데스크탑으로 옮길 때 사용하는 최소 패키지 규칙이다.

## 목표

전체 원본 추출물(`FullGame`, `UI`, `SourceBundles`)을 통째로 옮기지 않는다.

데스크탑에서는 같은 툴체인과 같은 `Tools/EldenAssetPipeline/*.py`로 다시 추출할 수 있게 만들고, 지금 클라이언트에서 바로 쓰는 cooked/runtime 에셋만 동기화한다.

## 패키지 포함 범위

ZIP에는 다음 항목을 넣는다.

```text
ToolchainArchives/
  blender-4.2.18-windows-x64.zip
  WitchyBND-v3.0.0.0-win-x64.zip
  UXM.Selective.Unpack.2.4.2.0.zip
  io_soulstruct-2.5.0.zip
  texconv.exe

WintersRuntimeTools/
  Tools/Bin/Debug/WintersAssetConverter.exe
  Tools/Bin/Debug/*.dll

Tools/EldenAssetPipeline/
  README.md
  *.py

.md/EldenRing/
  EldenRing pipeline docs

Client/Bin/Resource/EldenRing/
  Runtime/
  Assets/
  Maps/
  Manifests/
```

`Client/Bin/Resource/EldenRing/Characters`는 현재 `EldenLimgraveShowcaseScene`의 직접 참조 경로가 아니므로 이번 최소 패키지에서는 제외한다. 필요하면 별도 `LegacyCharacterProbePack`으로 묶는다.

## 제외 범위

다음은 크기와 파일 수가 커서 ZIP에 넣지 않는다.

```text
Client/Bin/Resource/EldenRing/FullGame
Client/Bin/Resource/EldenRing/UI
Client/Bin/Resource/EldenRing/SourceBundles
Client/Bin/Resource/Texture
```

위 폴더들은 데스크탑에서 원본 게임 설치와 파이프라인으로 다시 만들거나, 필요할 때만 선택 archive로 따로 공유한다.

## 데스크탑 설치 순서

1. 데스크탑에서 Winters repo를 clone 또는 pull 한다.

```bat
git clone https://github.com/tnestyle70/Winter_Engine.git C:\Users\<USER>\Desktop\Winters
```

2. 이 ZIP을 임시 폴더에 푼다.

```text
D:\WintersEldenTransfer
```

3. ZIP 안의 `Client/Bin/Resource/EldenRing`을 repo 루트의 같은 경로로 복사한다.

```bat
robocopy D:\WintersEldenTransfer\Client\Bin\Resource\EldenRing ^
  C:\Users\<USER>\Desktop\Winters\Client\Bin\Resource\EldenRing /E
```

4. `ToolchainArchives`의 zip들을 원하는 위치에 푼다. 기존 문서와 스크립트 기본값은 다음 위치를 가정한다.

```text
C:\Users\<USER>\Downloads\blender-4.2.18-windows-x64
C:\Users\<USER>\Downloads\WitchyBND-v3.0.0.0-win-x64
C:\Users\<USER>\Downloads\UXM.Selective.Unpack.2.4.2.0
C:\Users\<USER>\Downloads\io_soulstruct-2.5.0
C:\Users\<USER>\Downloads\texconv.exe
```

5. `WintersRuntimeTools/Tools/Bin/Debug`의 converter와 DLL은 repo의 `Tools/Bin/Debug`로 복사하거나, 데스크탑에서 `WintersAssetConverter`를 새로 빌드한다.

6. 파이프라인 도움말이 뜨는지 확인한다.

```bat
cd /d C:\Users\<USER>\Desktop\Winters
python Tools\EldenAssetPipeline\elden_pipeline.py --help
```

7. 현재 런타임 에셋 카탈로그를 재검증한다.

```bat
python Tools\EldenAssetPipeline\elden_pipeline.py build-resource-catalog ^
  --resource-root "Client\Bin\Resource\EldenRing" ^
  --out "Client\Bin\Resource\EldenRing\Manifests\eldenring_resource_catalog_desktop.json"
```

8. 추가 캐릭터를 다시 요리할 때는 `FullGame` 산출물이 있는 상태에서 `cook-runtime-character`를 사용한다.

```bat
python Tools\EldenAssetPipeline\elden_pipeline.py cook-runtime-character ^
  --resource-root "Client\Bin\Resource\EldenRing" ^
  --converter "Tools\Bin\Debug\WintersAssetConverter.exe" ^
  --repo-root "C:\Users\<USER>\Desktop\Winters" ^
  --character c2060 ^
  --out "C:\Users\<USER>\Desktop\EldenRingExtract\_full_pipeline\runs\runtime_cook_desktop.json"
```

## 현재 클라이언트 기준 에셋

`EldenLimgraveShowcaseScene`은 다음 경로를 직접 사용한다.

```text
Client/Bin/Resource/EldenRing/Maps/Limgrave
Client/Bin/Resource/EldenRing/Assets/LimgraveStatic
Client/Bin/Resource/EldenRing/Runtime/Character/<cXXXX>
```

`showcase_character_placement.json` 기준 캐릭터는 다음 런타임 `.wmesh`를 사용한다.

```text
Runtime/Character/c3010/c3010.wmesh
Runtime/Character/c2010/c2010.wmesh
Runtime/Character/c2060/c2060.wmesh
Runtime/Character/c3000/c3000.wmesh
Runtime/Character/c2130/c2130.wmesh
Runtime/Character/c3251/c3251.wmesh
```

`Assets/WP_A_7051_EffectMesh`는 첫 low-bone FX/weapon probe와 문서 검증용 자산이므로 같이 포함한다.

## 추출 파이프라인 요약

```text
원본 ELDEN RING Game 폴더
  -> UXM Selective Unpack
  -> WitchyBND unpack / XML 변환
  -> elden_pipeline.py index-game-root
  -> elden_pipeline.py run-full-pipeline
  -> Blender + Soulstruct로 FLVER/HKX 처리
  -> WintersAssetConverter로 .wmesh/.wskel/.wanim/.wmat 생성
  -> elden_pipeline.py cook-runtime-character
  -> Client/Bin/Resource/EldenRing/Runtime
```

주요 명령은 `Tools/EldenAssetPipeline/README.md`, `.md/EldenRing/13_HKX_ANIMATION_PIPELINE.md`, `.md/EldenRing/14_PIPELINE_V2_RUNTIME_CONTRACT.md`를 우선 참고한다.

## 주의 사항

- 원본 EldenRing 에셋과 대량 추출물은 GitHub에 올리지 않는다.
- 런타임 리소스는 `Client/Bin/Resource`에서 해석된다. `Client/Bin/Debug/Resource` 같은 config별 복사본을 기준으로 삼지 않는다.
- WitchyBND는 숨김/분리 콘솔에서 PromptPlus 초기화로 죽을 수 있다. 무인 실행은 `CREATE_NO_WINDOW` 방식과 연속 실패 가드가 있는 파이프라인 경로를 사용한다.
- Blender 애니메이션 FBX export는 armature-only로 내보내면 Assimp가 거부할 수 있다. 반드시 `ARMATURE + MESH`를 포함한다.
- WitchyBND 산출물은 MAX_PATH를 넘기 쉽다. 복사/삭제 스크립트는 `\\?\` 확장 경로와 `rd /s /q "\\?\..."` 폴백을 써야 한다.
- JSON은 UTF-8 BOM이 섞일 수 있으므로 Python에서는 `utf-8-sig`로 읽는다.
- `.wmesh`, `.wskel`, `.wmat`은 같은 stem을 사용하고, 애니메이션은 같은 폴더의 `anims/*.wanim`에 둔다.
- `.wmat`의 diffuse path는 repo-relative 또는 `Client/Bin/Resource` 기준으로 해석 가능한 경로여야 한다.
