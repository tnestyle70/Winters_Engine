# 2026-07-18 구조물 파괴 잔해 RESULT

관련 계획: 2026-07-18_STRUCTURE_DESTRUCTION_REMNANT_PLAN.md

```text
1. 예측 vs 실측:
- 스키마 확장(visibleWhenAlive, 기본값 !visibleWhenDestroyed) 후 생성 24개 상태 대입(포탑 16 + 억제기 4 + 넥서스 4) — 적중. 팩 0xDCC9E458, --check 클린. 넥서스 항목은 기본값 규칙으로 의미 불변(생성 코드 대입값으로 확인).
- 빌드 PASS. SimLab exit 0: 신규 RunStructureDestructionRemnantProbe PASS(파괴 후 엔티티 잔존·TargetableTag 제거·CanBeTargetedBy false·BA DeadTarget 거절·소생 시 태그 복구), 계약 해시 1f416e7039dd7c48 불변(스킬 데이터 비접촉 증명), same-seed 644D94AFB7BD5622 = Codex 조작감 반영 기준선과 동일(600틱 시나리오에 구조물 사망 없음 → 태그 계약이 골든 비간섭).
- 인게임 육안 게이트: 미실행 — 포탑 잔해([3] Stage3Stump 단독) 외형·억제기 [0]/[1] 방향·Z-fight 부재·잔해 호버/우클릭 무반응은 사용자 확인 대기. 서브메시가 익명(meshes[0]-N)이라 오선택 가능 — 스왑은 JSON+재생성만으로 교정.
2. 판결: 계획 유지 — 지시 순서(SnapshotApplier→공통 Targetable→억제기 시각 상태→구조물 SimLab) 전부 반영.
3. ⑤ 갱신: 시각 상태 표현이 3종→4종(양측 숨김 추가)으로 확장되며 ObjectVisualDefs의 상태 의미가 "파괴 시 표시 여부"에서 "상태별 표시 매트릭스"로 넓어졌다 — 이후 다단계 파괴 연출(단계별 스텀프)을 원하면 이 축을 enum 상태로 승격하는 시점이 온다. 억제기 부활 규칙 도입 시 DeathSystem 소생 분기가 태그를 복구하므로 리스폰 타이머만 추가하면 된다.
```
