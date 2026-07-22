Session - 기본 공격 cadence · 애니메이션 타이밍 전수 감사/수정 결과

관련 계획: `C:/Users/user/Desktop/Winters/.md/plan/2026-07-18_BASIC_ATTACK_CADENCE_ANIMATION_TIMING_AUDIT_PLAN.md`

## 예측과 실측

- 예측: 공격속도 수치 자체와 `1 / attackSpeed` cooldown 산식은 맞지만, 30 Hz float countdown의 미세한 양수 잔여 때문에 일부 속도에서 재공격이 한 tick 늦어진다.
  - 실측: 수정 전 AS 2.0의 0.5초 cooldown은 15번째 감소 뒤에도 양수 잔여가 남아 16틱 cadence, 즉 실효 1.875회/초였다. `SkillCooldownSystem`이 `1e-6` 이하 잔여를 같은 tick에 0으로 정규화하도록 수정한 뒤 AS 1.5는 20틱=1.5회/초, AS 2.0은 15틱=2회/초로 정확히 일치했다.
- 예측: attack speed, animation playback scale, impact windup, action lock은 같은 값으로 통일할 대상이 아니라 서로 다른 단위의 계약이다.
  - 실측: 서버 cooldown은 `1 / finalAttackSpeed`, 서버와 클라이언트의 애니메이션 배율은 모두 `finalAttackSpeed / baseAttackSpeed`, impact는 `castFrame / clip TPS / authored playback`, action lock은 행동 수명으로 사용되고 있었다. 산식과 서버/클라이언트 배율 전달은 이미 정합했으며, 문제는 cooldown 잔여와 잘못된 marker 시간 환산이었다.
- 예측: 기존 BA marker 데이터가 30 Hz 프레임으로 해석됐지만 실제 `.wanim`은 별도 TPS를 가진다.
  - 실측: 17개 선택 BA clip 모두 TPS 24였고 marker가 존재하는 16챔피언의 `basicAttackWindupSec`가 `castFrame / 30 / playback`으로 저장되어 impact가 33~50ms 빨랐다. 이를 `castFrame / 24 / playback` 값으로 전수 교정했다. marker가 0인 Yasuo만 0.175초 명시적 fallback을 유지했다.
- 예측: Irelia BA action lock 0.46초는 실제 recovery marker보다 짧아 클라이언트 애니메이션 끝을 약간 자른다.
  - 실측: 선택 clip의 recovery는 `14 / 24 / 1.25 = 0.466667초`였으므로 lock을 0.466667초로 정렬했다. 새 asset 계약 검사는 windup과 recovery가 action lock을 넘지 않는지 17챔피언 모두 확인한다.
- 예측: Irelia가 레벨 6에서 Trinity Force와 Blade of the Ruined King만 장착하면 공격속도 1.5에 미치지 못한다.
  - 실측: 두 아이템의 bonus attack speed는 0.30 + 0.25이며, 성장치를 포함한 최종 수치는 1.081580이었다. 1.5를 관측했다면 룬·패시브·연습도구 override 같은 추가 보정이 포함된 조건이다.
- 예측: 기존 AttackSpeedLab은 float 대입과 최초 공격만 검사해 지속 재공격 오류를 놓쳤다.
  - 실측: 실제 GameRoom 순서 `Cooldown -> CombatAction -> AttackChase -> pending execute`를 각 속도별 300틱 반복하도록 확장했다. 결과는 AS 0.8/1.0/1.5/2.0/2.5/3.003에서 각각 38/30/20/15/12/10틱 cadence였고, 10초 동안 최초 공격 포함 8/11/16/21/26/31회의 action start·sequence·cooldown reset이 모두 일치했다.
- 예측: 계획서 비평에서 구현 전 검증 공백이 발견될 수 있다.
  - 실측: 독립 서브 에이전트 `/root/plan_critic`가 BuyItem 반환 상태 오해, 실제 AttackChase 순서 누락, loader validity/hash/duration clamp 누락, Irelia recovery lock, AS 3.003/4x cap, codegen 순서와 병행 dirty 보호를 지적했다. 주 에이전트가 전부 수용하고 계획서를 수정한 뒤 반영했다.

## 판정

**PASS — 착각이 아니었다. 실제 AS 2.0 cadence 지연과 16챔피언 BA impact 조기 발생을 수정했고, 계획·비평·반영·코드젠·Debug/Release 빌드·전체 회귀를 완료했다.**

- `Test-BasicAttackTimingContract.py`: 17챔피언 PASS, asset marker 정합 16, explicit fallback 1.
- 챔피언 코드젠: 생성 후 `--check` PASS, hash `0x6BACE85B`.
- LoL definition pack: 생성 후 `--check` PASS, hash `0x76B43B5B`.
- SimLab Debug 빌드: 오류 0, 경고 0.
- `SimLab.exe --gamefeel-only`: 17 windup과 6개 AS 지속 cadence 및 Irelia item 공식 PASS.
- `SimLab.exe 600 1234`: 전체 회귀와 same-seed replay PASS.
- Engine/Server/Client Debug 및 Release 직접 프로젝트 빌드: 모두 오류 0. 기존 C4251/C4275 DLL 인터페이스 경고는 남아 있다.
- `git diff --check`: 오류 없음. 출력은 기존 LF/CRLF 변환 경고뿐이다.
- 최초 solution 다중 타깃 명령은 MSBuild target 구문이 각 vcxproj에 전달되어 MSB4057로 실패했다. 제품 오류가 아니라 명령 선택 오류였으며, 각 프로젝트를 `/m:1`로 직접 빌드해 Debug/Release 양쪽을 검증했다.
- 병행 Codex가 수정 중인 AI/Yone/schema 및 그 generated 산출물은 되돌리지 않았다. 현재 canonical source 전체를 생성기에 통과시켜 일치 상태만 검증했다.

## 갱신된 트레이드오프

- 30 Hz 서버에서는 모든 실수 공격속도를 완전히 표현할 수 없다. AS 0.8은 38틱으로 약 0.78947회/초, AS 3.003은 10틱으로 3.0회/초다. 사용자가 지적한 1.5와 2.0은 각각 20틱과 15틱으로 정확하다.
- `1e-6` ready epsilon은 BA뿐 아니라 일반 skill slot cooldown의 무의미한 초미세 잔여도 같은 tick에 0으로 만든다. Summoner cooldown과 stage window는 변경하지 않았다.
- 4배 animation scale cap은 서버와 클라이언트가 동일하게 유지한다. 최종 AS cap 3.003의 cadence와는 다른 안전 정책이며, 이번 수정에서 제거하지 않았다.
- Yasuo의 BA marker는 cast/recovery 모두 0이라 0.175초는 asset-derived 값이 아니다. marker가 저작되면 fallback 목록에서 제거하고 실제 TPS로 다시 산출해야 한다.
- Irelia passive와 Lethal Tempo의 실제 attack speed 스택 효과는 현재 별도 gameplay 기능 공백이다. 이번 작업은 표시된 최종 AS가 주어진 뒤 cadence·애니메이션·impact가 그 수치를 따르는 계약까지만 닫았다.
- 자동 검증은 공격음과 손맛을 판정하지 못한다. 동일 대상에서 AS 1.5/2.0을 각각 10초 플레이해 시각·타격음·피해 횟수를 비교하는 F5 캡처는 최종 수동 ceiling QA로 남는다.

## 계획서 비평 정책 갱신

- `AGENTS.md`, `CLAUDE.md`, `.md/계획서작성규칙.md`에 dated 구현 계획서는 소스 수정 전에 최소 1명의 독립 서브 에이전트 read-only 비평을 받도록 규칙을 추가했다.
- 주 에이전트는 P0/P1/P2 지적별 수용·기각·보류와 근거를 계획서에 남기고 수정한 뒤 구현해야 한다.
- 서브 에이전트를 사용할 수 없으면 `CONFIRM_NEEDED`로 기록하고 구현이나 검토 완료를 주장할 수 없다.
