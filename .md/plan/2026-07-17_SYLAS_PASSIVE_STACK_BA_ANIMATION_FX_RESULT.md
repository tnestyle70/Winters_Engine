Session - 사일러스 5초 패시브 스택과 기본/Passive BA 애니메이션·전기 PNG WFX 경로 반영 결과
좌표: 신규 좌표 후보 · 축: C7 · C8
관련: 2026-07-17_SYLAS_PASSIVE_STACK_BA_ANIMATION_FX_PLAN.md

## 1. 예측 vs 실측: 적중/빗나감과 이유

- 적중: `maxStacks=3`, `stackWindowSec=5`를 ServerPrivate canonical JSON으로 옮겼고 생성 pack hash는 예측대로 `0x8E9EF70F`에서 `0x8FF5D34F`로 한 번 바뀐 뒤 `--check`에서 고정됐다.
- 적중: 미학습 Q 거절 시 스택 0, 학습 Q 승인 시 스택 1, 4회 적립 시 최대 3, 4.99초 생존/5.01초 만료, BA당 1회 소모, 잔여 시간 비갱신, Passive BA stage 2/cue 1회, 소진 뒤 기본 BA stage 1을 집중 SimLab이 모두 확인했다.
- 적중: 전체 SimLab은 기존 same-seed hash `D9B579C1425033BB`와 seed+1 hash `71291010C7E00163`을 유지하며 PASS했다. 기존 GameSim 회귀는 관측되지 않았다.
- 빗나감: `msbuild`가 PATH에 없어서 첫 호출만 실패했다. Visual Studio 2022 Community의 절대 경로로 재실행해 SimLab 빌드 오류 0을 확인했다. 첫 전체 종속성 빌드의 기존 C4251/C4275 경고는 145개, 최종 증분 빌드는 경고/오류 0이었다.
- 미검증: Client/Server 전체 빌드와 실제 F5 렌더 결과는 이번 세션에서 실행하지 않았다. SimLab 빌드는 프로젝트 종속성상 Engine/GameSim을 함께 빌드했다.

## 2. 판정: 계획 유지 | 수정 반영 | 롤백 및 근거

- 수정 반영. `CommandExecutor`가 승인한 사일러스 스킬만 `SylasSimComponent`에 스택을 적립하며, BA 시작 시 서버가 stage 2/`SylasPassive` flag로 한 스택을 소비한다. impact의 EffectTrigger가 클라이언트 visual hook까지 한 번만 전달된다.
- 기본 BA는 stage 1, Passive BA는 stage 2로 `SkillDef`와 ClientPublic visual data에 명시했다. `EventApplier`의 사일러스 문자열 하드코딩은 일반 BA stage 2 animation key 조회로 교체했다.
- `Sylas.PassiveBA.Hit` WFX의 core billboard를 36,493 byte `sylas_base_recall_electricity.png` 2x2/4 frame/12fps atlas로 교체했다. 기존 torus/swing mesh emitter는 유지한다.
- 158,024 byte `skinned_mesh_sylas_attack_passive.wanim`이 이미 존재하고 모델의 cooked animation 자동 로드 경로에 포함되므로 재베이킹하지 않았다. 파일이 삭제되거나 로더에서 이름을 찾지 못할 때만 raw ANM 재베이킹으로 전환한다.
- 롤백 조건은 집중 SimLab의 stack/window/stage/cue 실패, F5에서 Passive BA 대신 기본 BA가 재생됨, cue 중복, atlas 검은 사각형 또는 중심 이탈이다. 해당 시 canonical 수치는 유지하고 visual stage/WFX 파라미터만 분리 롤백한다.

## 3. 재갱신: 실측 뒤 사라진 대가·언제 틀리나

- 사라진 대가: 4초 C++ 상수와 BA 소모 시 4초 재충전이 제거돼 기획 값과 런타임이 갈라지지 않는다. 사일러스 전용 EventApplier 문자열 분기도 제거됐다.
- 남은 대가: additive atlas의 `width/height=2.8`, 중심 높이 `1.05`, lifetime `0.36`, 12fps가 실제 캐릭터 실루엣에서 적절한지는 사용자 육안 검증이 필요하다. 값이 어긋나도 서버 스택·stage 계약은 건드리지 않고 WFX만 조정한다.
- 언제 틀리나: Passive BA에 별도 광역/마법 피해, 스택 HUD, 스택별 개별 만료 시간이 요구되면 현재 명세보다 넓다. 그 경우 DamageRequest와 UI snapshot 계약을 별도 계획으로 추가해야 한다.
- 범위 외 dirty 작업은 수정하지 않았다. 전체 `git diff --check`의 유일한 실패는 선행 작업 `Engine/Private/RHI/DX11/CDX11Device.cpp:1019`의 trailing whitespace이며, 이번 대상 파일만 검사한 `git diff --check`는 통과했다.
