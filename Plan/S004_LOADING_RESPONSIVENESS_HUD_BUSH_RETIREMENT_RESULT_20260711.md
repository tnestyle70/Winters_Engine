Session - 로딩 응답성·HUD 리소스 회귀·crossed-card 부쉬 폐기 결과와 인게임 검증 경계를 고정한다.

## 1. 결론

- 로딩 멀티스레드가 단순히 실수로 원복된 것은 아니다. `1813b00` 시점에는 worker가 모델 전체를 만들었지만, `18ca031`에서 모델 생성이 RHI handle/resource table까지 소유하게 된 뒤 `b4d2237`에서 render-owner thread 위반을 막기 위해 InGame 모델 preload가 메인 스레드 단계식 로드로 의도적으로 이동했다.
- 현재의 `LOL (응답 없음)`은 그 안전 조치 이후 한 단계가 너무 커진 문제다. Map11 WMesh/WMat 파싱, material별 legacy/RHI texture 생성, combined mesh 생성이 한 번의 `OnUpdate` 안에서 실행되어 Win32 message pump로 돌아오지 못했다.
- 이번 S004는 과거의 위험한 worker GPU 생성을 되살리지 않았다. CPU-only MapSurfaceSampler는 JobSystem worker에서 만들고, GPU/RHI 생성은 메인/render owner에 유지했다. 긴 model load 중에는 cooperative message pump를 실행하고 `WM_QUIT`을 latch하여 닫기 요청을 잃지 않게 했다.
- HUD 파일은 삭제되지 않았다. `f9d4d5c`의 Actor UI 일반화 과정에서 코드가 존재하지 않는 `ActorHUD_Default.png`, `actor_hud_layout.json` 같은 이름으로 바뀌었지만 실제 리소스 alias가 생성되지 않은 것이 원인이다. 실제 Irelia HUD/layout 경로를 다시 연결했다.
- S003 crossed-card 부쉬는 기술적으로 로드·렌더됐지만 시각적으로 실패했다. PNG foliage plane을 여러 방향으로 겹친 구조라 체적, 실루엣, 잎 깊이와 재질 반응이 부족했고 스크린샷처럼 큰 평면 카드로 보였다. normal F5 Stage를 `B0`로 복귀시키고 생성 도구와 산출물을 제거했다.
- 곤충/새/오리/firefly 앰비언트는 부쉬 실패와 무관하므로 유지했다.

## 2. 코드 근거와 원인

### 2-1. 로딩

- `Client/Private/Scene/Loader.cpp`
  - InGame은 기존부터 `PrepareMainThreadInGameLoad()` 후 프레임당 `LoadStep` 하나를 처리했다.
  - 첫 map model step 하나가 42 MB급 WMesh와 수백 material texture를 전부 포함하므로 `TickMainThreadLoad()`가 호출된 한 프레임 안에서 장시간 반환하지 못했다.
- `Engine/Private/Resource/Model.cpp`
  - 기존 `LoadCookedTextures()`는 material entry마다 `CTexture::Create`와 `RHI_CreateTextureFromFile`을 모두 호출했다.
  - Map11 정적 감사 기준 textured binding 약 390개 중 normalized unique path는 약 160개였다. 같은 path가 여러 material에서 반복될 때 decode/GPU resource 생성과 RHI destroy가 중복됐다.
- `Engine/Private/Platform/CWin32Window.cpp`
  - 기존 `PumpMessages()`는 `WM_QUIT`을 제거한 뒤 별도 상태를 기억하지 않았다. model load 내부에서 메시지를 펌프할 경우 outer loop가 quit을 다시 볼 수 없는 위험이 있었다.
- `Client/Private/Scene/Scene_InGameMapNav.cpp`
  - 기존 `InitializeMapSurfaceSampler()`는 scene `OnEnter`에서 같은 WMesh를 다시 읽고 512x512 surface grid를 동기 생성했다.

### 2-2. HUD

- `Engine/Private/Manager/UI/UI_Manager.cpp`
  - 실패 경로: `ActorHUD_Default.png`, `actor_hud_layout.json`, `ActorHitFlash.png`, `ActorStatusFlash.png`, `SkillRankPip.png`.
  - 실제 존재 경로: `HUD_Irelia_2.png`, `hud_irelia_layout.json`, `lol_ingame_hit.png`, `lol_ingame_stun.png`, `defaultcoloroverlifetime.png`.
- `Client/Bin/Resource/UI/hud_irelia_layout.json`
  - `stats.panel`, center/inventory, `portrait.face.shape = circle`, `portrait.frame`을 포함한다.
  - 존재하지 않는 layout을 읽지 못하면 built-in fallback으로 내려가 이 항목들이 빠지므로 스크린샷처럼 HUD frame, circular portrait 일부와 passive 배치가 사라졌다.
- actor HUD registration, per-frame state sync, overlay render 경로는 살아 있었다. 따라서 renderer 재작성보다 실제 asset/layout path 복구가 최소 수정이다.

### 2-3. 부쉬

- S003의 `map11_bush_cluster.wmesh`는 독립적인 완성형 bush mesh가 아니라 windgrass foliage plane을 여러 방향으로 복제한 crossed-card였다.
- Stage v5의 64개 BushEntry가 이 mesh를 normal F5에 스폰했고, 각 entry는 concealment volume도 함께 생성했다.
- 렌더만 끄거나 `visible=false`로 바꾸면 보이지 않는 근사 concealment volume 64개가 VisionSystem에 남는다. 따라서 정상 폐기는 Stage 자체를 `B0`로 복귀시키는 것이다.
- CSV/WBRUSH는 위치 연구 자료로만 남겼다. cooker에서 `--migrate-stage`와 crossed-card mesh 경로를 제거하여 normal Stage를 다시 오염시키지 못하게 했다.

## 3. 실제 반영

### 3-1. 안전한 로딩 응답성

- `CLoader`
  - map mesh/surface 선택과 기본 transform을 `CScene_InGame`의 공용 함수로 통일했다.
  - CPU-only `CMapSurfaceSampler`를 JobSystem worker에서 생성한다.
  - `CJobCounter::Decrement(acq_rel)`와 `IsComplete(acquire)` 뒤에만 완성 sampler를 scene factory 결과에 move한다.
  - loader 소멸 시 cancel flag를 세우고 worker 완료를 기다린다.
  - vertex/index raster loop가 취소를 주기적으로 확인한다.
- `CScene_InGame`
  - factory 생성 후 `OnEnter` 전에 prepared sampler를 주입한다.
  - map load가 성공했고 prepared sampler가 ready일 때만 동기 surface rebuild를 생략한다.
  - smoke/map load 실패에서는 기존 `bMapLoaded == false` 의미를 유지한다.
- `CModel` / `CResourceCache`
  - normalized path + sampler + color-space 기준으로 model-local texture를 dedupe한다.
  - material index mapping과 unique legacy/RHI owner 배열을 분리했다.
  - 같은 RHI handle을 여러 material이 참조해도 owner 배열에서 한 번만 파괴한다.
  - `Model::MaterialBindings`, `Model::UniqueMaterialTextures` profiler counter를 추가했다.
  - WMesh/WMat/unique texture/mesh build 경계에서 optional yield callback으로 message pump를 호출한다.
- `CWin32Window`
  - `WM_QUIT` latch를 추가하여 nested pump가 quit을 소비해도 이후 모든 pump가 false를 반환한다.
  - idempotent `SetSystemCursorVisible()`에서 Win32 cursor count를 정규화한다.
- Loading scenes
  - 진입 시 OS cursor를 표시하고 engine custom cursor를 끈다.
  - 이탈 시 원래 게임 cursor 모드로 복원한다.

### 3-2. HUD 복원

- actor HUD base: `HUD_Irelia_2.png`.
- layout: `hud_irelia_layout.json`.
- hit/stun/rank pip: 실제 존재 파일로 복구.
- 같은 일반화 회귀에 포함된 minion/turret HP bar와 깨진 shop reference path도 실제 파일로 복구.
- 검사한 관련 리소스 12개는 모두 존재했다.

### 3-3. 부쉬 폐기

- `Data/Stage1.dat`
  - `WSTG v4 / S30 / J12 / W27 / B0 / 5,408 bytes`.
  - SHA-256: `6494A5637D2D24BD962C11ACBD1E495E7DDC5319E2216126C0AF86A369731A28`.
- 삭제:
  - `Tools/build_map11_bush_cluster.py`.
  - generated OBJ/MTL/WMesh/WMat 4개.
- 유지:
  - generic Bush manager/editor/concealment 구조.
  - canonical coordinate 연구용 WBRUSH cooker.
  - ambient bird/duck/firefly/insect 경로.
- S003 session/result 문서 상단에 `RETIRED`를 기록하여 빌드 성공을 시각 성공으로 오해하지 않게 했다.

## 4. 자동 검증 결과

- `git diff --check`: PASS. 저장소의 기존 LF/CRLF 안내만 출력됐다.
- Stage hash/size/count: PASS.
- crossed-card tool/generated assets 부재 검사: PASS.
- HUD 관련 리소스 존재 검사: 12/12 PASS.
- research-only WBRUSH cooker: PASS, 64 entries / 1,040 bytes. 실행 전후 Stage hash 불변.
- Engine Debug x64 build: PASS.
  - `Engine/Include/Engine.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64`.
  - `UpdateLib.bat` post-build로 EngineSDK header/lib/bin 동기화 완료.
- Client Debug x64 build/link: PASS.
  - `Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64`.
  - 산출물: `Client/Bin/Debug/WintersGame.exe`.
- 첫 시도였던 `Winters.sln /t:Client`는 솔루션의 project target을 dependency vcxproj에 전파하는 구조 때문에 `MSB4057`로 거절됐다. 코드 오류가 아니며 위의 Engine→Client 직접 프로젝트 순서로 정상 검증했다.
- 기존 경고는 남아 있다.
  - `UI_Manager.cpp`에 과거부터 존재하던 invalid UTF-8 comment/문자열 경고 `C4828`.
  - DLL interface 관련 `C4251/C4275`.
  - 기존 `ChampionSpawnService.cpp` format warning `C4477`.
  - 이번 세션에서는 대규모 인코딩 정규화나 무관한 warning 정리를 하지 않았다.

## 5. 인게임 검증 절차

### 5-1. 로딩

1. Debug x64 `Client/Bin/Debug/WintersGame.exe`를 새 프로세스로 실행한다.
2. BanPick에서 Irelia를 고르고 match loading card 화면으로 진입한다.
3. map load가 진행되는 동안 마우스를 계속 원형으로 움직인다.
4. 기대 결과:
   - OS cursor가 끊기지 않고 이동한다.
   - title이 `LOL (응답 없음)`으로 바뀌지 않는다.
   - 창의 이동/닫기 메시지가 처리된다.
   - 인게임 전환 후 OS cursor는 숨고 game cursor가 한 개만 보인다.
5. 별도 1회는 로딩 도중 창 닫기를 눌러 프로세스가 파괴된 HWND로 다음 프레임을 진행하지 않고 종료하는지 확인한다.

### 5-2. HUD

1. 1920x1080 Irelia 인게임에서 하단 중앙을 확인한다.
2. 기대 결과:
   - 큰 HUD base/frame이 보인다.
   - circular portrait face와 portrait frame이 보인다.
   - passive icon이 portrait/Q icon에 겹치지 않는다.
   - Q/W/E/R, rank pip, HP/MP, stats panel, inventory/gold가 layout 위치에 맞는다.
3. 해상도를 1280x720로 바꿔 center anchor가 유지되는지 확인한다.
4. Viego 또는 Yasuo로 다시 들어가 actor content의 portrait/passive/skill icon은 바뀌고 공통 HUD frame/layout은 유지되는지 확인한다.

### 5-3. 부쉬와 앰비언트

1. normal F5 인게임에서 기지와 lane/jungle을 이동한다.
2. 기대 결과:
   - 스크린샷의 큰 crossed PNG card 부쉬가 한 장도 보이지 않는다.
   - debug log의 Stage bush count는 0이다.
   - bird/duck/firefly/insect ambient object는 유지된다.
3. Editor에서 Stage1을 load해 `B:0`인지 확인한다. 기존 generic Bush 편집 UI는 미래의 실제 3D bush asset용으로 남아 있지만 S003 crossed-card asset은 선택할 수 없다.

## 6. 남은 경계

- 이번 구현은 창과 입력의 응답성을 회복하는 안전한 중간 단계다. map model의 GPU buffer 생성과 legacy/RHI image upload는 여전히 render owner thread에서 실행되므로 로딩 배경 애니메이션이 모든 순간 60 FPS로 유지된다는 보장은 없다.
- 완전한 비동기 로딩의 다음 단계는 `worker CPU prepare -> bounded main-thread GPU finalize queue` 분리다. 현재 RHI owner rule을 깨고 `CModel::Create` 전체를 worker로 되돌리면 안 된다.
- `CFxAssetRegistry::LoadDirectory`는 parse뿐 아니라 global registry를 mutate하므로 이번에는 worker로 옮기지 않았다. 후속 시 worker parse result와 main-thread publish를 분리해야 한다.
- 부쉬를 다시 도입하려면 billboard/crossed-card를 재사용하지 말고, alpha-tested foliage material·normal/roughness·LOD를 갖춘 실제 bush 3D asset을 먼저 확보한 뒤 별도 세션으로 진행한다.
