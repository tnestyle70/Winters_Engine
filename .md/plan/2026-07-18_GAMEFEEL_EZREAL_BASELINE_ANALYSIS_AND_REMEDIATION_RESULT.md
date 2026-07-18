# 2026-07-18 이즈리얼 기준 전 챔피언 조작감 분석·개선 RESULT

관련 계획: `2026-07-18_GAMEFEEL_EZREAL_BASELINE_ANALYSIS_AND_REMEDIATION_PLAN.md`

```text
1. 예측 vs 실측: 이즈리얼이 부드러운 핵심 원인은 단순 애니메이션 배속이 아니라 ① Q/W/E의 lockDurationSec와 commandLockSec가 0.25초로 일치하고 ② 원거리 기본 공격이 보행 세그먼트 게이트를 우회하며 ③ 200ms BA impact가 실제 Attack1 클립 약 227ms와 가깝다는 결합이었다. 전 챔피언 15개 lock 값 수렴에 더해, 17개 basicAttackWindupSec를 데이터화하고 근접/원거리 기본 공격의 시작·적중 게이트를 같은 range 정책으로 통일했으며, 네트워크 액션의 recoveryFrame과 실제 FBX ticksPerSecond를 소비하도록 반영했다. 생성 해시 ChampionGameData=0x8CBBD326, LoL definition pack=0x0A70BCD7로 의도 변동했고 생성기 --check·스키마·Engine/Server/Client/SimLab Debug 빌드가 통과했다. `--gamefeel-only`와 전체 `600 1234`는 17/17 windup·range gate, AttackSpeedLab, orderedContract=1F416E7039DD7C48, same-seed hash=644D94AFB7BD5622를 PASS했다. 인게임 BA+QWER 교차 체감과 실제 모델 프레임 캡처는 자동 검증 밖이라 미실행이다.
2. 판결: 수정 반영 — Claude의 lock 수렴은 유지하되, 그 변경만으로는 남아 있던 이즈리얼 전용 예외·35% 고정 windup·미사용 recoveryFrame·FBX TPS 오해를 공통 데이터/정책 경로로 보완했다.
3. ⑤ 갱신: 데이터·권위·빌드 회귀는 닫혔지만 체감 수치는 1차 기준선이다. 특정 챔피언의 공격 아이덴티티가 손상되거나 실제 FBX의 frame 의미가 visual timing seed의 tick 의미와 다르면 틀리며, 이 경우 lock을 다시 늘리는 대신 Model&Anim Lab 캡처로 해당 champion의 basicAttackWindupSec와 recoveryFrame만 재튜닝한다.
```
