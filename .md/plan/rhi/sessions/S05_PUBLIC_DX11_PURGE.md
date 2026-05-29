# S05. Public DX11 Purge

목표: Engine Public/SDK 표면에서 DX11 native 타입을 제거한다.

## 선행 조건

- S01 완료
- S02 완료
- S03 완료
- S04에서 주요 renderer가 `IRHICommandList`를 받도록 전환 완료

## 이동 대상

Public에서 Private로 이동:

```text
Engine/Public/RHI/CDX11Device.h -> Engine/Private/RHI/DX11/DX11Device.h
Engine/Public/RHI/DX11/*.h      -> Engine/Private/RHI/DX11/*.h
```

## vcxproj 갱신

대상:

- `Engine/Include/Engine.vcxproj`
- `Engine/Include/Engine.vcxproj.filters`

`ClInclude Include="..\Public\RHI\DX11\..."`를 전부 `..\Private\RHI\DX11\...`로 바꾼다.

## SDK 검증

```powershell
rg -n "d3d11.h|ID3D11|RHI/DX11|CDX11Device" EngineSDK/inc Engine/Include Engine/Public
```

0 hit가 목표다.

## 예외

ImGui DX11 backend는 private implementation escape로 둔다. Public API에는 드러나면 안 된다.

## 합격 기준

```powershell
rg -n "d3d11.h|ID3D11|RHI/DX11|CDX11Device" Engine/Include Engine/Public Client/Public
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64
```

Public/Client Public에서는 0 hit, 빌드 통과.
