Session - NYPC 대회 툴체인(.py)을 Winters 봇 랩(Tools/BotLab)으로 이식하는 우선순위·경계·검증을 고정한다.

작성일: 2026-07-12 (검증 반영판)
성격: 축 설계 문서 (마스터 = 18). `14_NYPC_COMPETITION_ML_LAB_BRIDGE.md`의 경계 헌장을 승계하며, §10의 통합 보류를 본 문서로 해제한다.
경로 주석: NYPC Lab은 이 머신 기준 `C:\Users\user\Desktop\NYPC` (AGENTS.md/14/16의 `C:\Users\tnest\...` 표기는 구 머신 경로 — 16 §0.3이 경고한 stale path 부류).

> **레인 충돌 — 사용자 중재 필요 (2026-07-12)**: 같은 날 병렬 세션(Codex 레인)이 같은 주제를 선점해 `Tools/AIResearch/` + SHA provenance bridge manifest 방식의 계획(`.md/plan/2026-07-12_S017_NYPC_NEXT_NATION_TO_WINTERS_AI_CHRONOBREAK_PLAN.md`, `.md/architecture/WINTERS_NYPC_HUMANLIKE_AI_RESEARCH_ARCHITECTURE.md`)을 작성했고 14 문서에 상태 절을 추가했다. 두 계획은 방향이 같다(전량 복사 폐기, 규칙 진실은 GameSim, Python은 오프라인). 차이 = 디렉토리명(`Tools/AIResearch` vs `Tools/BotLab`)과 1차 산출물(manifest 봉인 우선 vs 리그 러너 우선). **통합 권고: 디렉토리는 하나만** — Codex의 manifest 봉인을 P0로 앞세우고, 본 문서의 P1~P5(리그/부검/튜너/게이트/오라클)를 그 뒤에 접붙이는 병합이 자연스럽다. 병합 전까지 두 문서 모두 코드 착수 금지.

보류 해제 근거 (14 §10 진입 조건 대조):
- NYPC Lab에서 replay/log/league 안정화 완료 (mushroom + battlefield 두 대회 완주, 리그/게이트/부검 툴 실사용).
- 탐색 기반 에이전트가 baseline을 통계적으로 이김 (battlefield 2100 레이팅 lane, 리그 리포트 실재).
- feature/action 설계 문서화 (battlefield Perception/J 스펙 문서군).
- 사용자가 2026-07-12 Winters 통합을 명시 요청 ("NYPC에서 사용했던 툴들 전부 다 연동하고 .py 전부 다 가져와서").
- 유지되는 것: 학습/분석은 Python 오프라인, 런타임은 baked artifact/순수 데이터만 (16 §3.6) — 이건 보류가 아니라 영구 경계.

범위 주의: 사용자 요청 표현은 ".py 전부 다"였으나 본 문서는 아래 근거로 **선별 이식**한다 (규칙 엔진 이중 진실 금지, 게임 종속 툴 제외). 이 축소는 사용자 승인 대상 — 제외 목록은 "이식하지 않는 것" 절에 명시.

불변식: Bot AI는 GameCommand 생산자다. Python 툴은 Winters 프로세스 밖에서 SimLab/JSONL을 소비·구동만 하며 GameSim에 링크되지 않는다.

## Goal

NYPC의 측정 생태계(리그/부검/튜너/오라클/게이트)를 Winters 봇용으로 재구축한다. 원리: **봇보다 측정 도구가 먼저 강해야 한다** (17 §3-7 검증 생태계 천장 교훈).

## Non-goals

- NYPC 원본 레포 수정 (이식은 복사-개작; `C:\Users\user\Desktop\NYPC`는 불변 참조).
- .py의 게임 규칙 엔진(`engine/rules.py`) 이식 — Winters의 규칙 진실은 GameSim이고 headless 러너는 SimLab이다. Python이 규칙을 재구현하면 이중 진실 (금지).
- PyTorch 학습 체인 — M5에서 별도 계획 (이 문서는 측정/구동 툴만).

## 디렉토리와 실행 계약

- 착지: `Tools/BotLab/` (신설, CONFIRM_NEEDED: 이름 — Tools 하위 기존 명명 규약 확인). vcxproj 무관(파이썬), 레포에는 스크립트+README만.
- 구동 대상: `Tools/Bin/Debug/SimLab.exe` (경로 함정 — exe는 `Tools/SimLab/` 아래가 아니라 `Tools/Bin/Debug/`에 출력된다).
- 현행 SimLab CLI: `argv[1]=tickCount, argv[2]=seed`. BotLab이 요구하는 확장(아래 리그 계약)은 SimLab측 슬라이스로 계획.
- 데이터 계약: 21 D3의 trace JSONL — 직렬화는 Shared 헬퍼 단일 소유, SimLab은 sink만 (21과 공유).

## 리그 계약 (P1 선행 스펙 — SimLab 확장 슬라이스)

현행 SimLab은 고정 틱 실행+해시 러너라 "매치" 개념이 없다. P1 계획서에서 다음을 확정해야 W/L CSV와 승률이 정의된다:

1. **헤드리스 승패/종료 지표**: 넥서스 파괴 시스템 존재 여부 확인(CONFIRM_NEEDED) — 없으면 "틱 N 시점 골드/킬/구조물 점수 차"를 매치 결과로 정의하고 결과 라인으로 출력.
2. **사이드별 봇 구성 CLI**: Blue/Red 각각 brain/난이도/노브 프리셋을 지정하는 인자 (예: `--blue brain=PlayerLike,aggr=1.2 --red brain=RuleBased`). 구성은 명령 왕복이 아니라 SimLab 월드 셋업 단계에서 주입 (headless라 클라 없음).
3. **Frozen baseline 정책**: 단일 진화 바이너리에서는 설정만으로 공용 의사결정 코드를 동결할 수 없다 → **승급 시점의 SimLab.exe를 태그와 함께 아카이브** (NYPC Fishtest식 과거 챔피언 풀 — 17 §3-7 "나보다 강한 축 유지"의 물리적 구현). 아카이브 exe와 현재 exe는 각자 자기 프로세스에서 실행되고 결과 라인으로만 비교.

## 이식 우선순위 (원본 → 역할 → Winters 착지)

| 순위 | 원본 (NYPC) | Winters 착지 | 대응 M |
|---|---|---|---|
| P1 | `battlefield/tools/league.py`, `claude_broad.py`/`claude_round_robin.py` | `botlab_league.py` — SimLab 배치 실행(사이드별 구성 × seed 세트), 결과 CSV, **진행 즉시 flush** | M1 |
| P2 | `battlefield/tools/claude_turn_inspect.py` (턴 부검) | `botlab_tick_autopsy.py` — JSONL에서 틱 지정 → perception/점수/선택/거부사유 덤프 | M1 |
| P3 | `battlefield/tools/claude_evcore2_tune.py`((1+λ) ES), `claude_ga_tune.py`, `battlefield/tools/tune_proxy.py` | `botlab_tune.py` — ChampionAITuning 14노브+profile 가중치 오프라인 튜닝 (fitness = 리그 마진, holdout seed 분리) | M2+ |
| P4 | `battlefield/tools/claude_ai_fidelity.py`, clone gate 계열 | `botlab_behavior_gate.py` — 골든 시나리오(22 U3)에서 행동 diff = 봇 행동 회귀 게이트 | M3 |
| P5 | `battlefield/tools/claude_j_oracle.py`/`claude_j_autopsy.py` | `botlab_value_autopsy.py` — ChampionAIValuation 기준 사후 최선 대비 regret 부검 (블런더 마이닝) | M4 |
| P6 | mushroom `claude_nnue_*`/BC 체인, `lab/ml_hello_world` | M5 학습 계획서에서 (반응 예측 BC) | M5 |

이식하지 않는 것 (사용자 승인 대상): 전장 전용 상대 클론(`sample_ai*_proxy.py` — 게임 종속), 공식 리플레이 포맷 변환기(`claude_replay_txt.py` — NYPC 심판 종속), `engine/rules.py`(이중 진실 금지), tmp_* 일회성 분석.

## NYPC Gotchas 승계 (BotLab 헌법 — `NYPC/Gotchas.md`)

1. **§1 측정 함정**: offline≠outcome; Blue/Red 교차 측정(FIRST/SECOND 대응); 튜닝 seed와 holdout seed 분리; 작은 n은 동전 던지기.
2. **§7 하네스 인프라**: 진행 즉시 flush("30초에 진행 못 보면 깨진 측정"); `multiprocessing.Pool` 금지 → OS 프로세스 병렬 + 재시도 래퍼; SimLab은 틱 예산 결정론이라 병렬 안전.
3. **§17 resumable**: 배치는 단위별 체크포인트 — 크래시에 in-flight만 손실.
4. **인코딩**: Windows cp949 → `PYTHONIOENCODING=utf-8` + ASCII 출력 기본.
5. **§8 승급 게이트**: 노브/코드 변경의 채택은 "리그 broad + 회귀 시나리오 + 결정론 parity 전부 통과"를 조건으로 — 감으로 승급 금지. 승급 = baseline exe 아카이브 갱신.

## 검증

- P1: 같은 seed 세트 두 번 = 같은 CSV (바이트 비교). 리그 계약 3항목이 계획서에서 확정돼 있을 것.
- P2: 부검 1건 30초 룰 (15 Phase 3 기준) — 임의 틱의 "왜 그 행동"이 30초 안에 설명되는가.
- P3: holdout에서 개선 확인 없는 튜닝 결과는 채택 금지 (승급 게이트).
- P5: 알려진 블런더(고의 악화 노브) 주입 시 regret 상승을 검출하는 자기 검증.

## Files touched (예정)

`Tools/BotLab/{botlab_league.py, botlab_tick_autopsy.py, botlab_tune.py, botlab_behavior_gate.py, botlab_value_autopsy.py, README.md}` (신설), `Tools/SimLab/main.cpp` (리그 계약: 종료 지표/사이드 구성 인자/JSONL sink/키프레임 로더 — 21 D3·22 U3와 공동 슬라이스).

## Rollback scope

BotLab은 전부 신규 파일 — 폴더 삭제로 원복. SimLab CLI 확장은 인자 파싱 블록 단위 원복.

## Next slice

P1 계획서 (M1): 리그 계약 3항목(종료 지표/사이드 구성/frozen baseline 아카이브) 확정 → SimLab 확장 → `botlab_league.py` 이식.
