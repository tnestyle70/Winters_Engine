Session - 사일러스 패시브 광역 공격/R 이동 projectile과 제드 패시브/E/R 서버 권위 FX·피해 경로 전수 복구 결과
좌표: 신규 좌표 후보 · 축 C7 · C8
관련: 2026-07-17_SYLAS_ZED_AUTHORITATIVE_PASSIVE_E_R_FX_REMEDIATION_PLAN.md

## 1. 예측 vs 실제

- 일치: 사일러스 passive BA는 `CombatActionFlags::SylasPassive` impact에서 반경 2.75 내 결정적 Health entity 순회로 기본 공격과 별도 `60 + 0.6 AP` 마법 피해를 enqueue한다. WFX는 `sylas_base_ba_swipe1.wmesh/png` 단일 보라색 원형으로 교체했고, non-authoritative stage 2 hit FX를 막아 impact 서버 cue에서 1회 재생하도록 했다.
- 일치: 사일러스 R은 GameSim stage 1 EffectTrigger를 target/direction과 함께 발행한다. 기존 끝점 정적 `r_hook_head`는 제거하고 `Sylas.R.HookProjectile` velocity cue를 추가하여 `sylas_base_r_hookmis_head.png`가 caster에서 target까지 18 unit/s 기준으로 이동한다.
- 일치: 제드 passive는 체력 50% 이하 stage 2 animation, `common_circletimer.wmesh` + `zed_e_hitslash.png`, 잃은 체력 10% 물리 피해가 impact에서 연결돼 있다. E는 caster stage 1과 shadow stage 2 서버 cue, 반경 2.75 중복 제거 피해를 유지한다.
- 일치: 제드 R은 target stage 1 red `zed_crossswipe.wmesh`, server DamagePipeline lethal preview stage 4/5, pop stage 2 marker 제거가 연결돼 있다. 실제/preview R request 모두 `skillId=0`으로 수동 잃은 체력 30% 피해를 보존하여 기존 0 damage atom 덮어쓰기 결함을 제거했다.
- 정적 검사: 요청 asset 10개 존재 확인, WFX/Gameplay JSON 파싱 PASS, definition pack 생성 및 `--check` PASS(`0xB22B83D9`, champions 17, skills 85), 이번 범위 `git diff --check` PASS다. 전체 worktree 검사는 다른 세션 파일 `Engine/Private/RHI/DX11/CDX11Device.cpp:1019`의 기존 trailing whitespace 1건 때문에 PASS가 아니며 이 작업에서 수정하지 않았다.
- 빌드 검사: `Server/Include/Server.vcxproj` Debug x64 PASS, `Client/Include/Client.vcxproj` Debug x64 PASS. 각각 `Server/Bin/Debug/WintersServer.exe`, `Client/Bin/Debug/WintersGame.exe` 링크와 runtime asset copy까지 완료했다. 기존 C4251/C4275 DLL interface 경고는 있었지만 오류는 0건이다.
- 미검증: animation skeleton 실제 재생, Sylas mesh scale 0.040, 색감, R projectile 체감 속도, Zed marker 머리 높이는 사용자 인게임 확인이 남아 있다.

## 2. 판정

- 판정: 수정·빌드 반영 PASS. 기존 `SYLAS_PASSIVE_STACK_BA_ANIMATION_FX_RESULT`는 광역 피해와 R cue를 범위 밖으로 잘못 제외했고, 기존 `ZED_PASSIVE_E_R_AUTHORITATIVE_FX_RESULT`는 E/R stage 1 원격 cue 누락과 R 피해 0 덮어쓰기를 놓쳤다. 이번 remediation이 두 결과를 대체한다.
- 서버 권위 경계는 `GameCommand → GameSim/CombatAction impact → DamageRequest + EffectTrigger → Client VisualHook → WFX`로 유지했다. Client는 피해·lethal 진실을 만들지 않고 서버 cue만 재생한다.
- 빌드 경로: 솔루션 `/t:Server`는 의존 프로젝트에 동일 target을 전달해 MSB4057로 소스 컴파일 전에 실패했으므로, 프로젝트 표준 직접 경로인 `Server.vcxproj`와 `Client.vcxproj`를 사용해 최종 PASS를 확인했다.

## 3. 규칙 갱신

- 새 공용 규칙 추가는 없음. 이번 실패는 기존 “FX는 서버 cue 한 경로”, “수동 피해 request는 data-driven formula 재적용 여부 확인”, “결과서의 범위 제외가 최신 사용자 명세와 충돌하면 새 remediation으로 대체” 규칙으로 설명된다.
- 실제가 사라질 때: 사용자가 새로 생성된 client/server로 Sylas passive/R, Zed passive/E/R을 인게임에서 확인한 뒤 animation·scale·색·속도·marker 높이가 예측과 일치하면 런타임 미검증 상태가 종료된다.
