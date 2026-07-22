# 2026-07-18 정수 약탈자·마나무네·무라마나 검증 RESULT

관련 계획: `2026-07-18_ESSENCE_REAVER_MANAMUNE_MURAMANA_PLAN.md`

```text
1. 예측 vs 실측: 예측대로 `--mana-items-only`가 `Essence Reaver BA/Q mana + Manamune 360 -> Muramana`를 PASS했다. 정수 약탈자는 ability accepted 뒤 Spellblade가 장전되고 다음 일반 공격 또는 OnHit 스킬인 이즈리얼 Q가 적중할 때만 1회 마나를 회복하며, 장전 전 Q는 회복하지 않았다. 마나무네는 8초당 4충전, 기본 +3, 챔피언 적중 2배(+6), 첫 4회 +24와 360 상한을 거쳐 같은 inventory slot에서 3042 무라마나로 변환됐다. 이후 flatMana 1000, runtime bonusMana 동기화와 최대 마나 2% bonus AD가 StatSystem 재계산에 반영됐다. Server/SimLab Debug 빌드, LoL definition pack --check, 전용 프로브와 전체 SimLab 600/1234가 모두 exit 0이었다.
2. 판결: 계획 유지 — 현재 ItemEffectSystem과 StatSystem 배선이 이미 요구 계약을 만족해 gameplay 코드 수정 없이 전용 회귀 프로브와 빌드로 닫았다.
3. ⑤ 갱신: 서버 권위 수치와 결정성은 검증됐지만 실제 F5에서 HUD 마나 숫자·아이템 아이콘이 변환 tick에 같은 프레임으로 갱신되는지는 자동 프로브 범위 밖이다. 그 시각 동기화가 어긋날 때만 Client 표시 경로를 별도 수정하며, 마나 계산을 Client로 옮기지는 않는다.
```
