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
