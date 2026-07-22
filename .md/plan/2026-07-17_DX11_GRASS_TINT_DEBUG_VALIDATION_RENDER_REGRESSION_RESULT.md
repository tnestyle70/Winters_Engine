# DX11 Grass Tint Debug Validation Render Regression Result

1. 예측 vs 실측: `git diff --check`와 `ModelRenderer.cpp` 컴파일은 적중했고, 격리된 Engine Debug DLL·GameSim Debug LIB·Client Debug EXE가 모두 생성됐다. 최초 `Winters.sln /t:Engine`은 target 해석으로 실패했고 정상 Debug 출력은 실행 중인 `WintersGame.exe`/다른 MSBuild가 PDB를 점유해 `LNK1201`이 발생했으므로, 프로젝트 직접 지정과 별도 `OutDir/IntDir`로 검증 명령을 보정했다. 수정 후 Timeline 300프레임은 현재 디버그 세션을 중단하지 않아 미실측이다.
2. 판결: 수정 반영 — 유효한 `t6/s6`을 표준 패스 전에 바인딩하고 null 해제를 제거한 소스와 충돌 회피 빌드 명령을 유지한다. 성능 회복 판결은 다음 normal F5 재캡처에서 내린다.
3. ⑤ 갱신: 읽기 전용 slot 6 상태 유지로 컴파일·링크 회귀는 없었다. 다른 패스가 slot 6 null에 의존하거나 Grass Tint 텍스처를 출력 리소스로 전환할 때는 틀리며, 새 Timeline에서 `Model::RenderCombinedStatic`이 3ms를 넘을 때 셰이더 변형 분리를 재검토한다.
