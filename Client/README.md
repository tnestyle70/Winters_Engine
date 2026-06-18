# Client

원자: ProductPresentation

Client는 플레이어가 보는 제품 경험을 소유한다.

소유:
- player input
- camera, feel, interpolation
- animation, FX, audio playback
- HUD, UI, debug presentation
- snapshot/event/cue를 view state로 바꾸는 코드

소유하지 않음:
- 최종 HP, 피해, 쿨타임, 승패
- Engine runtime primitive
- Server authority
- Backend state
- Build, cook, package 계약

구조 규칙:
- 기존 `Client/` 폴더명과 구조를 유지한다.
- 새 폴더는 ProductPresentation 하위 의미가 기존 구조에 담기지 않을 때만 추가한다.
- 새 `.h/.cpp`를 추가하기 전에는 `.vcxproj`, `.filters`, `CMakeLists.txt`를 수정하지 않는다.
