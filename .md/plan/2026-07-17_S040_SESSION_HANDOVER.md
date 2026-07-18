Session - S037~S040 세션 인수인계서: 다음 세션은 이력서·영상·BP 로드맵을 이어받는다.

## 2026-07-17 후속 마감 정정 — Data Driven 100%

- Data Driven 65%→100% 컷오버는 완료됐다. 상세 증거는 `2026-07-17_DATA_DRIVEN_100_PERCENT_RESULT.md`를 기준으로 한다.
- P3~P9 12/12, model/UI/AI 17/17, schema/reload 11/11, draft round-trip 0 failure다.
- Debug/Release GameSim·Server·Client·SimLab과 공식 `-RequireComplete` 검증이 모두 PASS했다.
- 이전 미결 큐의 `ZedPassiveR`는 현재 critical-indicator flag 계약으로 회귀식을 갱신해 PASS다.
- `Keyframe`의 truncated/load-error 문구는 의도한 failure-atomicity 주입 로그이며 `KeyframeAtomic`과 replay는 PASS다. 실제 RED로 취급하지 않는다.
- definition pack build hash는 `0xFC99B7EE`, same-seed hash는 `E16C6B35D8D99A1A`다.
- 다음 세션 우선순위는 그대로 이력서 → visual 육안 검증/영상+Notion → BP 로드맵이며, Data Driven은 새 회귀가 생기지 않는 한 재개하지 않는다.

```text
작성: 2026-07-17 세션 마감. 정본: 카드덱 .md/interview/cards/(05=82장·06=184노드),
log.csv 동일 폴더(대필 계약: 봇이 기록), 인수 로드맵 .md/plan/2026-07-17_ENGINE_ACQUISITION_DEBUG_ROADMAP.md,
계획서 규칙 v2 .md/계획서작성규칙.md (①~⑤ 결정기록+예측 검증 — 이 세션에서 전면 개정).
```

## 1. 이 세션에서 끝난 것

```text
- 답안집: Codex 심화판 병합(검증 64건) → 05=82장/06=184노드. Downloads=인쇄용 사본.
- 인수(⓪) 루프: Day 0 사다리 통과 → W01 부분 → 비에고 빙의 체인 → 씬 수명주기 →
  데이터 흐름(스폰/레벨업/아이템→StatSystem 수렴) → FX 렌더 체인 8정거장 지도.
- 흔적 기관 은퇴 3건 (전부 빌드 검증 통과): Blueprint 파이프라인+StaticScene,
  Engine CStatusEffectSystem. 계획서/RESULT 문서 존재.
- vcxfilters 7프로젝트 전수 재동기화 (Replay 4파일 등록, 잔여 0).
- Codex 중단 편집 수리: UI_Manager.cpp UI_StatsPanelAtlasUV 함수 이사 → 전 솔루션 그린.
- 피오라 바이탈 시각 슬라이스 적용+빌드 그린: passive_vital.wfx(crest3+backing 2층),
  r_mark.wfx(crest/warning/glow2 3층 피자), kVitalGap 1.35→1.6.
  계획서: 2026-07-17 세션 대화 내 v2 계획서 (§2-4/2-5는 미적용 — 아래 미결).
```

## 2. 다음 세션 과제 (사용자 지정 순서)

```text
A. 이력서 작성 — Notion 작업장: https://app.notion.com/p/39cb8c3c75e280b597c0fd2f7aaacc43
   Notion 데스크톱 7.26.0 설치 완료(winget, 해시 검증). 로그인은 사용자 직접(자격증명은 봇 금지 영역).
   봇이 노션에 직접 쓰려면 Notion MCP 커넥터 연결 필요 — 다음 세션에서 원하면 설정.
   ⚠선행 확정 필수: 던전스 서버가 이력서엔 IOCP, 실코드는 select(덱 MD10/MD01).
   원장 .md/이력서/weapons/03 정정 여부 사용자 확정 → 이력서 반영.
   잔여 CONFIRM: NYPC 최종 순위·NEXT NATION 명칭·스타 최장버그·던전스 인원/갈등·Tracy 실측.
B. 영상 + Notion gif — 촬영 후보: 비에고 원혼 빙의(BP로 체인 검증된 상태),
   사일러스 R 강탈, 피오라 R 4방향 신규 비주얼(이번에 갈아끼움 — 육안 미검증!),
   씬 전환 수명주기. EffectTuner 단발 재생이 촬영 셋업 최단 경로.
C. Winters Engine BP 로드맵 계속 — 경로 1 마감(W01 관측4·실험A/B·m_Parent) →
   경로 2(프레임: W03/W13/W07/W08/W11). 인수 로드맵 문서가 전체 지도.
D. 도메인 완벽 이해 = 42좌표 방어 + closed-book 시험 3(레포 밖 rewrite-exam/).
```

## 3. 미결 큐 (우선순위순)

```text
1) [게이트 RED] SimLab exit 255 — Codex 잔여 결함 2건:
   - [ZedPassiveR] FAIL: BA requests/cue normal=1 passive=0 cue=1 (Zed WIP 미완)
   - [Keyframe] truncated store payload (WorldKeyframe.cpp:485) — 유력: sim 컴포넌트 성장
     (FioraSimComponent +25줄/Zed +3/CombatAction +1) vs 키프레임 직렬화 비대칭.
     W15 좌표의 실전 사냥감 — BP를 :483-485에 걸고 nameHash/payloadSize 대조.
   이 2건 해소 전까지 골든 해시 재판정 불가.
2) 피오라 잔여: (a) 신규 wfx 육안 검증 + BP 관통(SpawnVital/ApplyVitalOffset/ConsumeVital)
   (b) W -90° 회전 미적용 (원 버그 리포트 2번 항목 — 소비점 Fiora_Skills.cpp:229,
   옵션: w_cast.wfx rotation vs 코드 Y축 회전) (c) §2-4/2-5 서버 상수 5종
   (acquireRange 8/lifetime 8/respawn 1.5/sideDot 0.55/maxHpRatio 0.03 — FioraGameSim.cpp:32-36)
   JSON 이관 — 사일러스 "JSON 3-stack/5s" 로드 경로 재조사 필요(SylasGameSim에 pDefinitions
   직접 사용 없음 → CommandExecutor/Assembly 경유 추정).
3) 판결 큐: OnShutdown Change_Scene(End,nullptr) 무효 호출(조사 — 종료 로그 확인),
   오프라인 시뮬 스택 7종(제품 결정 — 스모크/랩 의존 확인 선행).
4) log.csv 미납(사용자 구두 회수 대기): W01 divergence·m_Parent 유도·원혼 EntityID 질문·
   애쉬 빌보드 판결·데이터 흐름 예측 3건.
5) 인게임 육안 3종(은퇴 슬라이스 런타임 검증): 씬 왕복·스턴/버프·사일러스 스폰.
6) 커밋: 오늘 작업 전체 미커밋 (은퇴 3건 + 수리 + 필터 + 피오라 시각 + 카드덱 + 문서).
   Codex WIP(Fiora/Zed 게임심 +1,800줄)와 분리 커밋 권장.
```

## 4. 세션 간 계약 (유지)

```text
- m_Parent 판정 스포일 금지 (사용자 연습문제 — 메모리 project-acquisition-loop-track 참조).
- log.csv는 봇 대필, 판정·측정은 사용자 몫. 채점지 문서는 독립 판독 후에만.
- "X 전부 한 다음에" 제안 = 회피 패턴 점검 신호. 천장 질문.
- Bot AI는 GameCommand 생산자 — 게임플레이 진실 직접 변경 금지.
- 훅 오탐(레포 밖 파일 차단) 시 Bash 우회 + 보고서 명시 관례.
```
