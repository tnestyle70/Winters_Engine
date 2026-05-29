# ThirdPartyLib 편입 표준 절차

**목적**: 새 라이브러리 (예: Jolt Physics, Lua 5.4, zstd) 를 `Engine/ThirdPartyLib/` 에 편입하는 **반복 가능한 절차**. vcpkg 의존 없이 레포를 self-contained 로 유지.

**전제**:
- `Engine.vcxproj` 는 `<VcpkgEnabled>false</VcpkgEnabled>` + `<VcpkgEnableManifest>false</VcpkgEnableManifest>` 유지
- `$(SolutionDir)UpdateLib.bat` 이 Engine PostBuild 와 Client PreBuild 양쪽에서 호출되어 SDK 배포 + 런타임 DLL 복사 담당

---

## 1. 폴더 구조 템플릿

```
Engine/ThirdPartyLib/<LibName>/
  Inc/                공개 헤더
                      (서브디렉토리 보존 — #include <libname/foo.h> 형태 지원)
  Lib/
    Debug/            *.lib   (Debug 빌드용 — 이름이 Release 와 다를 수 있음 *_d.lib 등)
    Release/          *.lib   (Release 빌드용)
  Bin/
    Debug/            *.dll   (Debug + transitive 전부)
    Release/          *.dll   (Release 빌드용)
```

**예외 — Debug/Release 구분 없는 라이브러리 (예: FMOD)**: `Lib/` 밑바로 `fmod_vc.lib`, `Bin/` 밑 바로 `fmod.dll`. 서브폴더 생략.

---

## 2. 실제 예시 (현재 편입된 3 개)

### Assimp (Debug/Release 분리 + transitive DLL 5 개)
```
ThirdPartyLib/Assimp/
  Inc/assimp/*.h                              (Importer.hpp, scene.h, postprocess.h 등)
  Lib/Debug/assimp-vc143-mtd.lib
  Lib/Release/assimp-vc143-mt.lib
  Bin/Debug/ (6 파일: assimp-vc143-mtd + poly2tri + minizip + zlibd1 + kubazip + pugixml)
  Bin/Release/ (6 파일: assimp-vc143-mt + poly2tri + minizip + zlib1 + kubazip + pugixml)
```
- Debug/Release 에서 transitive DLL 의 **이름 다름** (`zlibd1.dll` vs `zlib1.dll`) — 각각 복사

### DirectXTK (Debug/Release 분리, lib 이름 동일)
```
ThirdPartyLib/DirectXTK/
  Inc/directxtk/*.h                           (CommonStates.h, Effects.h, SimpleMath.h 등)
  Lib/Debug/DirectXTK.lib
  Lib/Release/DirectXTK.lib
  Bin/Debug/DirectXTK.dll
  Bin/Release/DirectXTK.dll
```
- `#include <directxtk/WICTextureLoader.h>` 형태 유지하려면 `Inc/directxtk/` 서브디렉토리 필수

### FMOD (Debug/Release 공용, 단일 DLL)
```
ThirdPartyLib/FMOD/
  Inc/*.h                                     (fmod.hpp, fmod_errors.h 등 8 개, 서브디렉토리 없이 flat)
  Lib/fmod_vc.lib
  Bin/fmod.dll
```
- UpdateLib.bat 이 단일 DLL 을 양쪽 OutDir 로 복사

---

## 3. Engine.vcxproj 수정 (Debug + Release 양쪽 동일 패턴)

### 3-1. AdditionalIncludeDirectories
```xml
<AdditionalIncludeDirectories>
  ...기존...;
  $(ProjectDir)..\ThirdPartyLib\<LibName>\Inc;
  %(AdditionalIncludeDirectories)
</AdditionalIncludeDirectories>
```

### 3-2. Link > AdditionalLibraryDirectories (Debug + Release 각자)
```xml
<!-- Debug -->
<AdditionalLibraryDirectories>
  $(ProjectDir)..\ThirdPartyLib\<LibName>\Lib\Debug;
  %(AdditionalLibraryDirectories)
</AdditionalLibraryDirectories>

<!-- Release -->
<AdditionalLibraryDirectories>
  $(ProjectDir)..\ThirdPartyLib\<LibName>\Lib\Release;
  %(AdditionalLibraryDirectories)
</AdditionalLibraryDirectories>
```

- Debug/Release 공용 lib (FMOD) 는 `Lib\` 하나만

### 3-3. Link > AdditionalDependencies
```xml
<!-- Debug -->
<AdditionalDependencies>
  ...기존...;<libname>_d.lib;%(AdditionalDependencies)
</AdditionalDependencies>

<!-- Release -->
<AdditionalDependencies>
  ...기존...;<libname>.lib;%(AdditionalDependencies)
</AdditionalDependencies>
```

---

## 4. UpdateLib.bat 수정 — 런타임 DLL 을 Client/Bin 으로

`endlocal` 직전에 블록 추가:

```batch
REM -- <LibName> runtime DLLs --
if exist "%ROOT%\Engine\ThirdPartyLib\<LibName>\Bin\Debug\" (
    xcopy /Y /D "%ROOT%\Engine\ThirdPartyLib\<LibName>\Bin\Debug\*.dll" "%ROOT%\Client\Bin\Debug\"
)
if exist "%ROOT%\Engine\ThirdPartyLib\<LibName>\Bin\Release\" (
    xcopy /Y /D "%ROOT%\Engine\ThirdPartyLib\<LibName>\Bin\Release\*.dll" "%ROOT%\Client\Bin\Release\"
)
```

Debug/Release 공용 DLL (FMOD):
```batch
if exist "%ROOT%\Engine\ThirdPartyLib\<LibName>\Bin\<libname>.dll" (
    xcopy /Y /D "%ROOT%\Engine\ThirdPartyLib\<LibName>\Bin\<libname>.dll" "%ROOT%\Client\Bin\Debug\"
    xcopy /Y /D "%ROOT%\Engine\ThirdPartyLib\<LibName>\Bin\<libname>.dll" "%ROOT%\Client\Bin\Release\"
)
```

**주의**: `.bat` 은 ASCII-only. 한글 주석 금지 (CLAUDE.md Gotchas 참고). 주석은 영어.

---

## 5. 검증 순서

1. Engine 리빌드
2. `Client/Bin/Debug/` 에 새 DLL 존재 확인:
   ```bash
   ls Client/Bin/Debug/ | grep <libname>
   ```
3. `dumpbin -DEPENDENTS WintersEngine.dll` 로 의존성 추가 확인:
   ```bash
   cd Client/Bin/Debug && dumpbin -DEPENDENTS WintersEngine.dll | grep -i <libname>
   ```
4. F5 실행 — `0xC0000135` (DLL not found) 없이 로딩
5. Release 구성도 동일하게 검증

---

## 6. 흔한 문제 & 대응

| 문제 | 원인 | 해결 |
|---|---|---|
| `unresolved external symbol <libname>::Foo` | AdditionalDependencies 에 .lib 이름 누락 | vcxproj Debug/Release 각자 확인 |
| `Cannot open include file: <libname/foo.h>` | AdditionalIncludeDirectories 에 Inc 경로 없음 | Debug/Release 양쪽 추가 |
| `0xC0000135 STATUS_DLL_NOT_FOUND` | Client/Bin 에 DLL 없음 | UpdateLib.bat 의 xcopy 블록 확인. Engine 이 재빌드되어야 PostBuild 가 돌음 |
| Transitive DLL 누락 (예: `zlibd1.dll` 없다) | 직접 의존 라이브러리만 복사, transitive 놓침 | `dumpbin -DEPENDENTS <libname>.dll` 로 추적하여 ThirdPartyLib/Bin 에 전부 편입 |
| Debug 빌드에서 Release lib 링크 시도 | AdditionalLibraryDirectories 가 한쪽만 설정 | Debug/Release 각자 별개로 ItemDefinitionGroup 에 추가 |
| MSBuild 가 vcpkg 통합 자동 시도 | `<VcpkgEnabled>` 명시 안 함 | vcxproj 의 Globals PropertyGroup 에 `<VcpkgEnabled>false</VcpkgEnabled>` + `<VcpkgEnableManifest>false</VcpkgEnableManifest>` 박아둘 것 |

---

## 7. 주의사항

- **vcpkg 혼용 금지**: 같은 라이브러리를 vcpkg 와 ThirdPartyLib 양쪽에 두면 링크 경로 충돌 가능. ThirdPartyLib 로 편입했으면 vcpkg 에서 `vcpkg remove <libname>` 또는 vcxproj 에서 `<VcpkgEnabled>false</VcpkgEnabled>` 로 차단
- **lib 경로가 $(ProjectDir)..\ 상대경로**: `$(ProjectDir)` 는 `Engine/Include/` → `..\` 는 `Engine/`. 레포 이동 시에도 견고
- **Release transitive 이름 차이 주의**: Debug 의 `zlibd1.dll` → Release 의 `zlib1.dll` 같은 naming 차이를 놓치면 Release 런타임에서 `0xC0000135`
- **header-only 라이브러리** (예: nlohmann/json): Lib/ Bin/ 생략, `Inc/<libname>/*.hpp` 만. AdditionalDependencies 도 생략
- **Debug 빌드는 `d` 접미사 관례**: vcpkg 는 `*_vc143-mtd.lib` (debug) / `*_vc143-mt.lib` (release) 처럼 접미사 다름. ThirdPartyLib 편입 시 원본 이름 유지
- **Static lib (.lib만, dll 없음)**: `Bin/` 폴더 생략. lib 링크만으로 끝

---

## 8. 체크리스트 (새 편입 시 매번 확인)

- [ ] `Engine/ThirdPartyLib/<LibName>/{Inc, Lib, Bin}` 생성
- [ ] 헤더 복사 (서브디렉토리 보존)
- [ ] lib 파일 Debug/Release 분리 복사
- [ ] DLL 파일 Debug/Release 분리 복사 (transitive 전부 포함)
- [ ] `Engine.vcxproj` Debug + Release 양쪽에 Include/Library 경로 + AdditionalDependencies 추가
- [ ] `UpdateLib.bat` 에 DLL xcopy 블록 추가 (ASCII 주석만)
- [ ] Engine 리빌드 → Client/Bin/Debug 에 DLL 배포 확인
- [ ] `dumpbin -DEPENDENTS` 로 의존성 확인
- [ ] F5 실행 정상
- [ ] Release 구성 동일 검증
- [ ] CLAUDE.md "기술 스택" 섹션에 한 줄 추가

---

## 9. 관련 문서

- 코딩 컨벤션 / 서드파티 정책: `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md`
- 아키텍처 큰 그림: `.md/architecture/WINTERS_GAMEPLAY_ARCHITECTURE.md`
- 레포 가이드: `CLAUDE.md` (루트)
