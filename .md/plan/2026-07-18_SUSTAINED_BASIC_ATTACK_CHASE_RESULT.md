# 2026-07-18 지속 평타(어택 체이스 영속화) RESULT

관련 계획: 2026-07-18_SUSTAINED_BASIC_ATTACK_CHASE_PLAN.md

```text
1. 예측 vs 실측:
- 빌드 PASS → 적중 (1차 실패는 코드가 아니라 병행 세션 빌드와의 PDB 잠금 충돌 C1041 — 재시도로 해소, devenv+mspdbsrv 2개 실증).
- SimLab: MidDefense 해시 1735AF9283C9F02E 불변 + ActionGeneration PASS → 적중 (봇 bSustain=false 게이트가 병행 미드래치 세션 비충돌을 실증).
- 빗나감(외부 요인): 중간 런에서 KalistaR FAIL 1건 발생 — 원인은 본 변경이 아니라 병행 세션의 상태효과 대개편(Invulnerable 플래그·만료 epsilon·Cleanse 신설)이 실행 중이던 반쪽 상태. 판정 근거 ①KalistaR 프로브 본문에 BasicAttack 명령 0건(본 변경 경로 실행 불가) ②StatusEffectSystem.cpp/main.cpp 수정 시각(17:30~17:31)이 내 빌드와 교차 ③병행 편집 완료 후 재빌드에서 KalistaR PASS 복귀.
- 전 프로브 PASS, same-seed 결정성 유지(hash 33F18F0230B7575F — 병행 CC 개편 포함 트리 기준).
- 인게임 육안 게이트(1클릭 지속공격/추격, 이동·스킬로 해제, 사망 시 idle, 스윙 중 클릭 보존): 미실행 — 사용자 확인 대기.
2. 판결: 계획 유지 — 4파일 반영이 계획 §2와 1:1, 외부 FAIL은 귀책 분리 완료.
3. ⑤ 갱신: "지속 루프 커버 프로브 없음"은 그대로 공백(인게임이 유일 게이트). 추가 실측 교훈 — 병행 세션과 같은 트리에서 SimLab 판정할 때는 FAIL 귀책을 파일 수정 시각+프로브 명령 종류로 분리한 뒤 재빌드로 재판정해야 한다(이번 KalistaR가 그 사례).
```
