# S00. Smoke Project Repro

목표: 이전 데스크탑에 있었던 `Smoke` 테스트 프로젝트가 왜 현재 clone에 없는지 재현하고, 앞으로 같은 혼선을 막는다.

## 재현 절차

1. 임시 파일을 만든다.

```text
Tools/Smoke/Smoke.vcxproj
```

2. `git status --short`를 확인한다.

예상:

```text
?? Tools/Smoke/
```

3. `git ls-files | rg -i smoke`를 확인한다.

예상:

```text
Client/Private/GamePlay/SchemaSmoke.cpp
Client/Private/GamePlay/SharedGameSimSmoke.cpp
```

4. `git check-ignore -v Tools\Smoke\Smoke.vcxproj`를 확인한다.

예상:

```text
not ignored
```

## 결론

- `Smoke.vcxproj`는 ignored가 아니다.
- 그러므로 이전 데스크탑에서 보였다면 `git status`에 untracked로 떠야 했다.
- commit/push하지 않은 `Smoke.vcxproj`는 clone에 따라오지 않는다.
- `Smoke.exe`는 `*.exe` ignore 대상이라 clone에 따라오지 않는 것이 정상이다.

## 정리 규칙

- 재현용 `Tools/Smoke/Smoke.vcxproj`는 만들자마자 삭제한다.
- 앞으로 Smoke 검증은 별도 project가 아니라 기존 project 내부 compile-only 파일로 둔다.
- DX12 검증도 별도 `DX12.exe`가 아니라 `WintersGame.exe`의 backend switch로 수행한다.

## 이전 데스크탑 확인 명령

```powershell
git status --short --ignored | Select-String -Pattern "Smoke|smoke|DX12|dx12|D3D12|d3d12|\.exe|\.vcxproj"
git ls-files | rg -i "smoke|dx12|d3d12|directx12"
Get-Content Winters.sln | Select-String -Pattern "Smoke|DX12|D3D12"
```

판정:

- `?? Tools/Smoke/Smoke.vcxproj`: 로컬 미커밋 테스트 프로젝트다.
- `!! Smoke.exe`: ignored 빌드 산출물이다.
- `Winters.sln`에만 `Smoke`가 보이고 git status가 modified면 solution만 로컬 수정된 것이다.
- 아무것도 없는데 Visual Studio에 보이면 `.vs/.suo/.user` 캐시 또는 다른 solution을 연 것이다.
