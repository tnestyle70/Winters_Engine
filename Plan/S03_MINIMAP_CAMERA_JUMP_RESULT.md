Session Result - S03 minimap camera jump.

1. 반영 결과

미니맵 좌클릭의 의미를 챔피언 이동에서 카메라 이동으로 변경했다.

반영된 원자 흐름:

```text
mouse screen pos
-> minimap uv
-> world XZ
-> camera focus XZ
-> minimap camera frame follows camera focus
```

코드 반영:

```text
Client/Public/DynamicCamera.h
Client/Private/DynamicCamera.cpp
Client/Public/Scene/Scene_InGame.h
Client/Private/Scene/Scene_InGame.cpp
Client/Public/UI/MinimapPanel.h
Client/Private/UI/MinimapPanel.cpp
```

핵심 동작:

```text
미니맵 좌클릭:
- 클릭 좌표를 미니맵 uv로 변환한다.
- uv를 world XZ로 역투영한다.
- 카메라 follow를 끄고 현재 시야 오프셋을 유지한 채 카메라 focus를 해당 XZ로 이동한다.
- 렌더 프레임은 camera GetAt 기준으로 그려지므로 클릭 지점으로 이동한다.

우클릭:
- 기존 챔피언 이동 명령 경로를 유지한다.

A + 좌클릭:
- 미니맵 위에서는 attack-move로 해석하지 않는다.
```

2. 검증

실행한 검증:

```powershell
git diff --check
```

결과:

```text
통과. CRLF 경고만 출력됨.
```

빌드 검증:

```powershell
Engine/Include/Engine.vcxproj Debug x64
Client/Include/Client.vcxproj Debug x64
```

주의:

```text
Visual Studio가 열려 있어 일반 Debug PDB 링크가 잠길 수 있다.
검증 빌드는 PDB/증분 링크를 끄는 옵션으로 통과시킨다.
```

산출물:

```text
Client/Bin/Debug/WintersGame.exe
Engine/Bin/Debug/WintersEngine.dll
```

수동 확인 필요:

```text
1. WintersGame.exe 실행
2. 미니맵 좌클릭 시 카메라와 흰 프레임이 클릭 지점으로 이동하는지 확인
3. 우클릭 이동이 기존처럼 유지되는지 확인
```
