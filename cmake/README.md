# cmake

원자: BuildGraph

`cmake/`, `CMakeLists.txt`, `.vcxproj`, `.filters`, `Winters.sln`은 무엇을 어떻게 빌드하는지 정하는 실행 계약이다.

소유:
- build target
- source inclusion
- tool executable build
- generated output 경로
- cook, package, deploy step과의 연결

소유하지 않음:
- gameplay truth
- product presentation
- engine capability 의미
- authored data 원본

수정 기준:
- 새 `.h/.cpp`가 빌드에 들어간다.
- 새 library, executable, build target이 생긴다.
- 새 cook/package/deploy step이 build graph와 연결된다.
- CI나 release gate가 실제 실행 계약을 가져야 한다.

구조 규칙:
- 아키텍처 리팩터의 시작점으로 `cmake/`나 `.vcxproj`를 먼저 수정하지 않는다.
- source owner가 확정된 뒤 build graph를 따라 맞춘다.
