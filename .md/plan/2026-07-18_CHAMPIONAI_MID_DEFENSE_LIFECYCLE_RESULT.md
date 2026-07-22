# 2026-07-18 ChampionAI 미드 방어 래치 수명 RESULT

짝 계획서: `2026-07-18_CHAMPIONAI_MID_DEFENSE_LIFECYCLE_PLAN.md`

## 1. 예측 vs 실측

```text
적중:
- SimLab MidDefense 프로브: 기존 assert 유지 + 신규 3 assert(해제/유지/조우 교전) PASS.
  Debug/Release 동일 해시 1735AF9283C9F02E(변동은 의도, 구성 간 결정성도 유지).
- Run-BotAiValidation Debug 전체 PASS(경계 감사+4빌드 /m:1+SimLab). Release 파이프라인 PASS.
- 키프레임 프로브 PASS — sizeof 2936 실측 일치, 새 필드 직렬화 정상(save@300→restore→replay 일치).
- Release 소크 1800틱×3: 개별 런 전부 PASS, p99 2.78~3.55ms(예산 33.3ms 대비 여유 ~10배),
  deadline_misses=0 — 위협 전역 쿼리(5Hz×봇 수) 비용은 관측 불가 수준.

빗나감 (최우선 기록):
1) Debug 1800틱 소크가 성능 게이트로 FAIL(p99 34.7ms>33.3ms). 원인=Debug 구성 아티팩트 —
   1800틱 Debug 기준선 자체가 부재였고(기존 증거는 전부 300틱), 기능 지표는 전부 정상.
   "성능 판정은 최적화 빌드로만" 가차가 소크 게이트에도 적용됨을 재확인.
2) 크로스런 world_hash 동일성 게이트 FAIL — 단, 원인은 본 변경이 아님을 바이너리 diff로 실증:
   3런 차이 224바이트 전량이 ChampionAssistCreditComponent 스토어 내부(ChampionAI 스토어는
   바이트 동일). iLastDamageTick에 QPC 규모 값 유입 + Credit 패딩 11B 미초기화 의심.
   replay_hash는 런/구성 무관 동일(D7225D79ADC5178D). 이 게이트는 사망>0 매치에서만 발화라
   기존 단일런 증거로는 한 번도 걸린 적 없음 → 별도 세션 칩 발행(assist-credit 수정).
```

## 2. 판결

수정 반영 유지 — 신규 계약 3종 그린 + Release 소크 개별 PASS, 실패 2건은 각각 구성 아티팩트와 기존 결함으로 귀속 실증.

## 3. ⑤ 갱신

```text
- 위협 전역 쿼리 비용 우려는 실측으로 소멸(Release p99 여유 10배).
- 새 대가 확인: 크로스런 결정성 게이트가 어시스트 크레딧 기존 결함에 가로막혀, 본 변경류(봇 행동
  변경)의 "3런 해시 동일" 자동 게이트를 당분간 쓸 수 없다 — assist-credit 수정 전까지 개별 런
  PASS+SimLab 프로브가 유일한 자동 게이트다.
- 위협 반경 20/홀드 6s 체감(churn 여부)은 여전히 인게임 게이트 대기 — 자동 게이트 없음.
```

## 인게임 검증 레시피 (사용자 게이트)

```text
1. powershell -ExecutionPolicy Bypass -File Services\StartBackend.ps1  (WINTERS_DEV_AUTH_ENABLED 포함)
2. Server\Bin\Debug\WintersServer.exe 실행 → 클라이언트로 네트워크 매치 시작
3. F9 (AI Debug 패널): All Bots 테이블 Macro 열 관찰
   - 포탑 파괴 + 미드에 적 웨이브/챔피언 존재 → 상대 봇 Macro=DefendMid로 집결
   - 위협 해소 후 ~6초 → Macro=HomeLane 복귀, 원래 레인에서 파밍 재개   ← 핵심 관찰점
   - 로테이션 중 사거리 내 아군 챔피언에게 반격/교전하는지 (기존: 무시하고 통과)
4. F10 (레거시 디버그) Champion AI text 오버레이: 봇 머리 위 "GroupMidDef/DefendMid" 라벨
   (기존 "Unknown") — 시각 캡처용
5. ESC로 나가면 Replay/AITrace/AITrace_<UTC>.jsonl 에 봇별 결정 트레이스 자동 덤프 —
   집결/해제 전이 틱과 유틸리티 점수 사후 분석 가능
튜닝 필요 시: ChampionAISystem.cpp 상수 kChampionAIMidDefenseThreatRadius(20)/ThreatHoldSec(6).
```
