# 2026-07-18 IRELIA E 유닛 확대 + 표식/Q 초기화 + 귀환 6초 RESULT

관련 계획: 2026-07-18_IRELIA_E_UNIT_HIT_MARK_Q_RESET_RECALL_6S_PLAN.md

```text
1. 예측 vs 실측:
- 팩 재생성 후 --check 클린 → 적중 (해시 0x1889C259 → 0xF66ED47B, 17챔피언/85스킬 불변, 생성 cpp에 recallDurationSec=6.f + irelia e/r MarkDurationSec 등장 확인).
- Winters.sln Debug x64 /m:1 빌드 PASS → 적중 (기존 C4251 경고만, 신규 경고 0).
- SimLab 결정성 exit 0 → 적중. 빗나감: "Irelia 프로브/골든 해시 변동 가능" 예측은 빗나감 — orderedContract=c2ee91f37a8e34b0 불변, IreliaWRelease/DataContract/Keyframe(마크 컴포넌트 등록 포함 blob=98567B replay 일치)/전 프로브 PASS. SimLab 시나리오가 E 세그먼트 경로에 미니언/정글을 세워두지 않아 해시가 흔들리지 않았다 — 즉 신규 경로(E→미니언/정글, 마크 소모)는 SimLab 게이트 밖이다. 이것이 이번 실측의 최우선 데이터: 인게임 게이트 전까지 신규 경로는 기계 검증 공백.
- 인게임 육안 게이트(E→미니언/정글 스턴+대미지, 표식 Q→쿨다운 0, 귀환 6s/이동·피격 중단): 미실행 — 사용자 확인 대기.
2. 판결: 계획 유지 — 빌드/결정성/계약 게이트 전부 그린, 코드 변경은 계획 §2와 1:1.
3. ⑤ 갱신: Q 쿨다운 쓰기 이원화(CommandExecutor 시작 / IreliaGameSim::Tick 리셋)의 대가는 유지. 추가 발견 — E/마크 신규 경로를 커버하는 SimLab 프로브가 없어 회귀 게이트도 없다; 다음 슬라이스에서 E-세그먼트에 미니언/정글을 배치하는 프로브를 추가하면 ⑤의 "언제 틀리나"(조용한 회귀)를 닫을 수 있다.
```

## 후속 세션 — Irelia Q 접촉 판정·처치 초기화·고정 0.25초 제거

```text
1. 예측 vs 실측:
- Q 피해를 cast 승인 시점에서 실제 접촉 tick의 DamageQueue 처리로 옮기고, Champion/MinionOrSummon/JungleMonster를 Q로 처치한 경우에만 Q 쿨다운을 0.1초로 제한한다는 예측 → 적중. Debug/Release `SimLab --irelia-q-only`에서 세 대상군 처치 초기화, Structure 제외, 무적/미적용 시 표식·쿨다운 보존, 피해 이벤트 정확히 1건을 모두 PASS했다.
- 고정 0.25초 Lerp를 `Speed(14) + moveSpeed`의 tick step과 매 tick 이동 대상 재추적으로 바꾸고 접촉 시 Q action lock을 해제한다는 예측 → 적중. 원거리 접촉 전 피해 0건, 근거리 역주행 0건, moving target 종점 갱신, blocked/CannotMove 피해 0건, 비치명 접촉 후 impact+1 tick 기본 공격 Accepted를 PASS했다.
- 30Hz에서 0.1초 재사용은 impact+1/+2 tick Cooldown 거절, impact+3 tick Accepted라는 예측 → 적중.
- 데이터 팩 재생성/검사 PASS: build hash `0x8B32D869`, 17 champions / 85 skills, 생성된 Irelia Q에 `Gap=1.35`, `Speed=14` 확인, 제거한 Q `dashDurationSec` runtime 필드 잔존 0건.
- Debug SimLab focused/stage/full, Debug Server, Debug Client PASS. Debug Server 첫 빌드에서 대상 사망 정리 경로의 제거 필드 참조를 발견해 `GameRoomCommands.cpp`를 계획과 코드에 보강한 뒤 PASS했다.
- Release x64 Server PASS(기존 C4275 경고 3, 오류 0), Client PASS(기존 C4251/C4275 경고 125, 오류 0), SimLab PASS(경고/오류 0) 및 Release focused probe PASS. `git diff --check` PASS.
- F5 네트워크 인게임에서 연속 미니언 Q와 실제 애니메이션 preemption을 녹화·육안 확인하는 게이트는 미실행이다. 서버 판정/명령 수용/Release 빌드는 닫혔지만 최종 손맛은 사용자 플레이 확인이 필요하다.
2. 판결: 계획 유지 — 서브 에이전트 최종 재비평 `P0/P1 없음, 구현 진행 가능` 이후 반영했으며, 빌드가 드러낸 사망 정리 직접 의존 1곳만 동일 계획 §5-3에 보강했다.
3. ⑤ 갱신: Q 접촉은 champion Tick에서 일어나 command phase가 이미 끝난 뒤이므로 다음 명령의 실제 최소 수용 시점은 `impact+1 tick`(30Hz 최대 약 33ms)이다. 이동 대상을 계속 추적하므로 기존 0.25초 상한은 없어졌고, 아주 빠르게 도망가는 대상에서는 대시 시간이 길어질 수 있다. 네트워크 클라이언트의 위치 진실은 서버 snapshot 하나로 제한했으며, F5에서 action animation이 접촉 snapshot/후속 ActionStart에 맞춰 즉시 끊기는지는 수동 시각 게이트로 남긴다.
```
