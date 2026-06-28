# Winters Engine 해부기 12 - Asset Format과 Resource Pipeline

Asset Pipeline의 본질은 "모델 파일을 로드한다"가 아니다.

Winters에서 Asset Pipeline의 본질은 다음이다.

> 외부 리소스를 runtime이 안정적으로 읽을 수 있는 엔진 고유 포맷과 검증 가능한 배포 단위로 변환하는 것

## 문제 정의

처음에는 FBX, PNG, 추출된 texture를 바로 읽으면 빠르다.

하지만 프로젝트가 커지면 리소스는 코드보다 더 큰 문제가 된다.

- 원본 에셋 경로가 장비마다 다르다.
- Map/Texture 폴더가 10GB, 50GB 단위로 커진다.
- git에 올릴 것과 Drive로 공유할 것이 불분명하다.
- material/texture binding이 깨진다.
- skeleton/animation을 runtime에서 매번 복잡하게 해석해야 한다.
- 공개 가능한 코드와 공개하면 안 되는 상용 원본 에셋이 섞인다.

이 문제는 단순히 "파일을 잘 정리하자"로 해결되지 않는다.

엔진이 읽을 runtime format과 source asset, generated asset, packaged resource를 구분해야 한다.

## Winters의 접근

Winters는 runtime binary format을 지향한다.

대표 포맷:

- `.wmesh`
- `.wmat`
- `.wtex`
- `.wskel`
- `.wanim`
- `.wfx`
- `.wseq`
- `.wmap`

관련 코드:

- `EngineSDK/inc/AssetFormat/Mesh/WMeshLoader.h`
- `EngineSDK/inc/AssetFormat/Material`
- `EngineSDK/inc/AssetFormat/Anim`
- `Tools/WintersAssetConverter`
- `Tools/EldenAssetPipeline`

## WMesh 예시

`WMeshLoaded`는 mesh 파일을 runtime에서 읽은 결과를 나타낸다.

```cpp
struct WMeshLoaded
{
    MeshMetaHeader header;
    std::vector<SubMeshDesc> subMeshes;
    std::vector<BoneEntry> bones;
    std::vector<SubMeshBounds> bounds;

    const uint8_t* pVertexBlob = nullptr;
    size_t uVertexBlobBytes = 0;
    const uint8_t* pIndexBlob = nullptr;
    size_t uIndexBlobBytes = 0;

    std::vector<uint8_t> m_vRawFile;
};
```

여기서 중요한 점은 vertex/index blob을 runtime이 바로 사용할 수 있는 형태로 유지하려는 방향이다. 원본 DCC 포맷을 매번 runtime에서 해석하는 것이 아니라, 사전에 변환된 binary format을 우선한다.

## Resource 경로 정책

Winters의 runtime resource는 `Client/Bin/Resource` 기준으로 해석한다.

이 기준은 중요하다.

config별 output 폴더에 Resource를 복사하기 시작하면 다음 문제가 생긴다.

- Debug/Release별 리소스 불일치
- clone 후 재현 어려움
- 어느 Resource가 진짜 runtime source인지 불분명
- 대용량 asset sync 혼란

따라서 code는 git에 올리고, 대용량 Resource는 별도 Drive/package로 공유하되 runtime path contract는 하나로 유지하는 방향이 필요하다.

## Elden Asset Pipeline과의 연결

Elden 방향에서는 상용 원본 에셋을 그대로 공개할 수 없다.

중요한 것은 원본을 올리는 것이 아니라, 각 장비에서 추출/변환 가능한 pipeline과 runtime이 읽을 수 있는 Winters format을 정리하는 것이다.

즉 공개 포트폴리오에서는 asset 자체보다 다음을 보여주는 것이 안전하고 강하다.

- converter code
- format spec
- validation report
- runtime loader
- local-only extraction guide
- placeholder asset으로 재현 가능한 demo

## 면접에서 말할 포인트

Asset Pipeline은 엔진 개발자의 중요한 역량이다.

게임은 코드만으로 돌아가지 않는다. asset import, conversion, validation, runtime loading, packaging, gitignore, Drive sync까지 제품 파이프라인의 일부다.

## 이 글을 이력서 문장으로 압축하면

> 외부 리소스를 runtime에서 직접 해석하지 않고 `.wmesh/.wmat/.wtex/.wskel/.wanim` 등 Winters binary format으로 변환·검증하는 에셋 파이프라인을 설계했습니다.

## 다음 글

다음 글에서는 LoL arena 구조를 넘어 Elden형 Action RPG로 확장하기 위한 World Partition과 Streaming 방향을 설명한다.

