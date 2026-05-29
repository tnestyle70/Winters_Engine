Session - 롤과 엘든링 시연 가치가 보이도록 이펙트 툴과 샘플을 데모 품질로 다듬는다.

1. 반영해야 하는 코드

성공 기준:
- 툴 자체가 포트폴리오 장면이 된다. 에셋 검색, 값 편집, preview, save, runtime reload가 한 흐름으로 보인다.
- 롤형 샘플과 엘든링형 샘플이 각각 엔진 표현력을 보여준다.
- 성능, 로그, 실패 상태가 정리되어 시연 중 불안정해 보이지 않는다.
- “엔진의 한계가 게임의 한계”라는 메시지가 툴 결과물로 증명된다.

작업 파일:
- `C:/Users/user/Desktop/Winters/Client/Private/UI/WfxEffectToolPanel.cpp`
- `C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxCuePlayer.cpp`
- `C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxSystem.cpp`
- `C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxMeshSystem.cpp`
- `C:/Users/user/Desktop/Winters/Data/LoL/FX`
- `C:/Users/user/Desktop/Winters/Client/Bin/Resource/Texture/FX`

샘플 목표:
- 롤형 샘플
  - `Annie.Q.Fireball`: projectile head, trail, hit pop
  - `Irelia.E.Connect`: beam, blade flash, ground mark
  - `Yasuo.W.WindWall`: wall body, edge shimmer, impact block cue
- 엘든링형 샘플
  - boss sword trail: mesh/ribbon slash with delayed ember
  - ground rupture: decal, shockwave ring, debris-like sprite burst
  - magic sigil: layered atlas, UV scroll, erode threshold animation

구현 방향:
- 성능 budget을 UI에 표시한다.
  - active billboard count
  - active mesh FX count
  - texture load failures
  - last preview spawn count
- preview spawn 직후 `OutputDebugStringA/W`에 cue, emitter count, spawned count를 남긴다.
- unsupported emitter type은 숨기지 않고 UI warning으로 보여준다.
- 샘플 `.wfx`는 champion/genre별 폴더를 분리한다.
  - `Data/LoL/FX/Champions/...`
  - `Data/Portfolio/FX/Elden/...`
- 시연용 버튼은 기능 설명문이 아니라 실제 workflow를 빠르게 실행하는 command로 둔다.
  - Open Sample
  - Preview
  - Save Copy
  - Reload Runtime
- 시연용 상태 초기화 버튼을 둔다. 이전 preview entity와 transient registry 상태를 정리해야 한다.

비범위:
- 완전한 Niagara-style node graph는 이번 로드맵 이후 단계다.
- 물리 기반 파편 시뮬레이션, GPU particle simulation은 별도 엔진 세션으로 분리한다.
- 새 gameplay skill 제작은 이펙트 툴 완성 후 별도 gameplay 세션에서 한다.

2. 검증

검증 명령:
- `git diff --check`
- `"C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m /nodeReuse:false`

수동 확인:
- 시연 루트 1: asset browser에서 Annie Q 선택, color/lifetime 수정, preview, save copy, reload runtime까지 1분 안에 보여준다.
- 시연 루트 2: Irelia E connect를 열고 beam width, color, erode threshold를 바꿔 전후 차이를 보여준다.
- 시연 루트 3: Elden boss sword trail 샘플을 열고 slash, ember, ground decal layer를 emitter별로 켜고 끈다.
- 툴 창을 켠 상태에서도 일반 인게임 조작과 렌더가 망가지지 않는다.
- 누락 리소스가 있는 샘플은 의도한 warning을 띄우고, 정상 샘플은 warning 없이 preview된다.
- active FX count가 계속 증가하지 않고 preview clear/restart 후 안정된다.

시연 체크:
- 첫 화면에서 asset list, preview, inspector가 동시에 보인다.
- 수정한 값이 즉시 preview에 반영된다.
- 저장된 `.wfx`가 다시 로드되어 같은 값을 가진다.
- runtime cue 경로에서 같은 `.wfx`를 재생한다.
