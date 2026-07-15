Session - S020 반영 결과: AI 패널 스크롤/컴팩트화, 미니맵 Z 전단 제거, 봇 스킬 5초 게이트 — 빌드·SimLab 전 PASS, 인게임 게이트 대기.

날짜: 2026-07-13. 계획: `S020_AI_PANEL_MINIMAP_SKILL_INTERVAL_SESSION_20260713.md`.

## 1. 결론

세 건 모두 코드 반영 완료, 자동 검증 전부 통과. 남은 것은 사용자 인게임 확인(§3)뿐.

1. **미니맵**: `MinimapPanel.h` Z 반대각 156.69→94.385 (값 1개) — world→UV 기저가 전단에서 등배 직교로 복원. 아이콘/클릭 점프/카메라 바운즈/FoW가 같은 struct를 쓰므로 자동 정합.
2. **봇 스킬 5초**: `EmitSkillCommand` 단일 초크포인트에 fresh-cast 간격 게이트 + 신규 콤보 시작 게이트. 진행 중 커밋(활성 콤보/다이브/stage-2)은 예외 — `[SimLab][ChampionAI] PASS (commitment)` 로 커밋 불변 무회귀 증명.
3. **AI 패널**: 봇 콤보 셀렉터 상시 노출 + 전체 테이블/튜닝 15슬라이더/트레이스/미니언을 CollapsingHeader 뒤로(기본 접힘, Selected AI만 열림). 창 720px 기본 + 크기 제약. 휠 스크롤을 먹던 ScrollY 테이블이 접힘 상태에선 노출 0개.

## 2. 자동 검증 결과 (2026-07-13)

- msbuild Winters.sln Debug x64: **에러 0** (경고 236 = 기존 베이스라인), 2:37.
- `Tools\Bin\Debug\SimLab.exe`: **전 프로브 PASS** — ChampionAI(CC/commitment/cadence/dive), ChampionAIFow(Codex 신규 — 시야/last-seen/influence cadence 포함), KalistaR, Viego, YoneE, EzrealProjectile, ActionLock, NavGrid, KeyframeAtomic, EntityIdMap, CommandOutcome.
- `[SimLab][Keyframe] PASS: save@300 -> restore -> replay 301..600 matches (blob=89999 bytes)` — `fSkillCastCooldownTimer` 필드 추가 후에도 왕복·재실행 완전 일치 (blob은 S015의 81027에서 Codex 추가분+본 필드로 성장). 콘솔의 `[Keyframe] restore failed - ...` 라인들은 KeyframeAtomic 프로브의 **의도된 부정 테스트** 출력.
- same-seed replay OK / seed+1 감도 OK. 내 변경 4파일 `git diff --check` clean.

## 3. 인게임 게이트 (사용자)

1. **미니맵**: 탑/바텀 라인 챔피언이 코너 대각 방향으로 정상 배치(중앙 압축 해소). 미니맵 클릭 카메라 점프 좌표 일치. FoW 오버레이가 아이콘과 정합. 외곽 구조물이 미니맵 밖으로 잘리면 보고 (그 경우 X/Z 스팬을 **같은 배율로** 함께 키우는 재보정 — 한 축만 만지지 않는다).
2. **F9 패널**: 기본 높이에서 휠 스크롤 동작, Bot 콤보 선택, 헤더 4종(All Bots/Runtime Tuning/Decision Trace/Server Minions) 접힘·펼침.
3. **스킬 간격**: 라인전 관전 — R 포함 스킬 버스트 사이 ≥5초, 콤보(Q→BA→E→…) 내부 연계는 즉시, Jax 다이브 무결. 체감이 과하게 수동적이면 Next slice의 노브 승격.

## 4. 남은 경계

- 5초 상수는 `kChampionAISkillCastMinInterval` (ChampionAISystem.cpp 상단) — 튜닝 노브 아님. 체감 조정 요구 시 15번째 `eChampionAITuningId` 승격(16 §4.3 6단계).
- Yone E2 soul-return 수제 push는 의도적 비게이트 (stage-2 성격).
- 미커밋 대기열: 본 세션 diff + Codex S017/S018 + 크로노 브레이크(S015) 전부 미커밋 — S015/S018/S020 인게임 게이트 통과 후 일괄 checkpoint commit 권장.
