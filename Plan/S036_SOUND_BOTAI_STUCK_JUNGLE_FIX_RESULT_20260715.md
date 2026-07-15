Session - S036 결과 보고: 사운드 무음·봇 공격성(3중 사슬+Farm 회귀)·구조물 끼임·정글몹 스탯/체력바를 원인 확정→반영→빌드→SimLab/스모크 검증→origin push까지 완주했다.

작성일: 2026-07-15 (Agent: Claude)
계획서: `Plan/S036_SOUND_BOTAI_STUCK_JUNGLE_FIX_SESSION_20260715.md` (승계 원본: `.md/plan/2026-07-15_INGAME_SOUND_AI_NAV_JUNGLE_FIX_PLAN.md`)
조사: 멀티에이전트 워크플로 8에이전트(조사4+적대검증4, 툴콜 423) + 인라인 rg/Read 재검증. 전 원인 적대적 검증 통과.

## 결과 요약

| 증상 | 확정 원인 | 반영 | 검증 |
| --- | --- | --- | --- |
| 전 사운드 무음 | Sound_Manager가 exe 폴더(`Bin/Debug`) 기준 `Resource/Sound` 조합 → 캐시 0건, Play* 무음 return | lazy 로드로 전환: `WintersResolveContentPath("Sound/"+키)` canonical 해석, 키별 1회 bounded 실패 로그, BGM stop-before-play. 이렐리아 6슬롯 `irelia_base_sfx_26_54676396.wav` 통일(청음 probe) | 빌드 그린. 청음은 인게임 E2E(하단) |
| 봇 소극적/잭스 미드 정지 | ①PlayerLike HP 하드게이트(동률 미만 교전 금지) ②Farm 무조건 유지+기본 폴스루 ③GroupMidDefense 래치: 팀 무관 미드포탑 손실 트리거+앵커 9유닛 밖 위협 무시(제자리) — 포탑 밀집 배치가 끼임 노출도 증폭 | ①허용오차 0.12+retreat 0.65 ②Farm 조건부 유지+post-combo 조기 return 폴스루화 ③자기팀 한정 트리거+집결 반경 내 LaneCombat 상시 위임+간격 1.75 ④margin 0.05·캐스트 3s·Ashe 0.90/스캔10·Jax 1.35/0.35/0.55/0.70 | SimLab MidDefense 프로브 신계약 PASS, 골든 불변 |
| illegal Farm 회귀(RunValidation FAIL) | Farm legality는 미니언 요구·선택은 비요구(웨이브 추종 Move) + 스모크 틱240 강제 창이 HP 동률 재유도에 뒤집힘 | legality에 CanMove 단독 포함(진단 계약만) + 스모크 틱240 적 HP ≤50% 고정(밸런스 비의존) | 라이브 스모크 seed42 `records=8 ticks=240 final legal` PASS, RunValidation 전체 PASS |
| 포탑/억제기·아군끼리 끼임 | 계획 그리드 0.5 vs 보행 클램프 0.75 비대칭 죽은 링 + 풋프린트 오버랩 시 탈출 부재(완전 웨지: 대시착지/플래시 진입) + 클램프=이동 전체 취소 재발행 루프. 아군끼리 = 동반 웨지(챔피언 상호 충돌은 원래 없음) | 클램프 반경 0.5 캡 + `TryClampMoveSegmentXZ` 풋프린트 탈출(반경0 폴백) + 그레이징 클램프 웨이포인트 스킵/슬라이드 + `[MoveSystem][Stuck] reason=segment-clamped` bounded 트레이스 | 빌드+SimLab 그린. 밀착 주행은 인게임 E2E |
| 정글몹 HP/체력바 과대 | 팩 데이터(Krug1400/55~Gromp2000/60, 미니350/25)+바 폭은 HP 무관 `fWidthScale=3.f` 고정 | subKind 4~10+default = HP 450(미니언 225×2)/AD 40(동일), 에픽 0~3 불변, 팩 재생성 0x10774DA5, 폭 3배 제거, 클라 로컬 폴백 정합, 리로드 시 중립몹 stat dirty 가드 | generated diff 게이트 통과(정글 한정), 빌드 그린 |

미니 3종(8~10)은 350~400/25 → 450/40으로 **소폭 상승** — "비-에픽 일괄 미니언×2/AD 동일" 규칙의 의도된 결과.

## 커밋 스택 (origin/main = e749123, ahead 0 / behind 0)

```text
e749123 gitignore: exclude CodexVerify/CodexPostFx build outputs and heavy AI evidence dumps
72da7e1 shared: route ChampionGameplayAssembly through Phase 7F ECS adapters
cdf4109 infra/services: Go backend (auth/shop/profile), EldenRing project files, ...
a1d1d00 sim/server: bot aggression + farm-mask + mid-defense + movement wedge fixes, jungle 450/40 (S036)
415e3be client: jungle HP bar width fix + local fallback (S036) + accumulated client work
f314bde engine: sound manager lazy canonical-root loading (S036) + accumulated engine work
dcd13b2 docs: multi-session plan/TODO/collab snapshot + S036 session plan
```

S036 외 누적분(f9d4d5c 이후 타 세션 500M+277??)이 영역 커밋에 함께 실려 있음 — 각 커밋 메시지에 명기.

## 검증 증거

- 빌드: `msbuild Winters.sln Debug x64` 전 7타깃 그린, 에러 0 (리베이스 후 재빌드 포함).
- SimLab 1800틱 seed42: **PASS**, 골든 해시 `18110C0D7C01FA27` **불변** — 메인 시나리오는 봇 AI/클램프 경로 미사용이라 재생성 불필요. MidDefense 프로브는 신계약(집결 반경 내 LaneCombat 위임, 간격 1.75 허용 반경 4.5)으로 갱신 후 PASS.
- RunValidation: 전체 PASS. 라이브 스모크 seed42: `[SimLab][AILiveSmoke] PASS: records=8 ticks=240 pure-team-filtered final legal` — 확정 회귀 해소.
- `Tools/Harness/Run-S17RhiValidation.ps1`: 런타임 프로브 5종 전부 alive (보고서 `.md/build/2026-07-15_143053_...md`).
- `Check-SharedBoundary.ps1`: PASS (원격 리팩터의 직접 ECS include 3건을 Phase 7F 어댑터로 수리 후).
- `git diff --check`: 클린.

## 리베이스/이력 정리 (push 전 처리)

1. **rebase origin/main(behind 8)**: 충돌 1파일(`GameRoomSpawn.cpp`) — 원격 챔피언 조립 리팩터(ServerChampionEntityFactory/ChampionGameplayAssembly) 구조 채택 + `kChampionAIMidLane` 보존. 원격 리팩터에서 **소실됐던 델타 3건 이식**: ①칼리스타 서약(Oathsworn) 인벤토리 시딩(원격에 참조 0건이었음 — 미이식 시 칼리스타 서약 기능 소실) ②`GetActiveLoL*` 게터(op25 핫리로드가 챔피언 스폰에 반영되도록) ③스폰 팩 miss bounded 로그.
2. **Shared 경계 수리**: 리팩터 파일의 직접 `ECS/*` include 3건 → `Core/Ecs`/`Core/World` 어댑터 (GameSim PreBuild가 빌드 실패로 강제).
3. **이력 재작성(미푸시 구간만)**: `.md/build/evidence/` **11.9GB**(862파일)와 `*/Bin/CodexVerify/` **424MB**가 커밋에 휩쓸려 push 408 실패 → filter-branch로 제거(디스크 파일은 보존), gitignore에 `**/Bin/CodexVerify/`·`**/Bin/CodexPostFx/`·`/.md/build/evidence/` 추가. **AI 학습 증거 12GB는 git 부적합 매체 — Resource.zip처럼 로컬/외부 저장 유지**(RL 세션의 "evidence 고정"은 별도 매체 필요).
4. 파생물 정책 준수: `profiler.json`(스태시 복원)·`Profiles/`·`Replay/`는 커밋 제외.
5. GitHub 저장소가 `Winters_Engine.git`으로 이름 변경됨 — 로컬 origin URL 갱신 완료.

## 노트북 절차

```powershell
git pull --rebase origin main   # 새 URL: https://github.com/tnestyle70/Winters_Engine.git
# Resource.zip 해제 -> Client/Bin/Resource (사운드 wav 포함 확인됨)
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m
```

## 인게임 E2E 체크리스트 (사용자 게이트 — 서버 로그만으로 판정 금지)

1. 이렐리아 BA/Q/W/E/R/사망 = 동일 WAV 6회 청음. VS 출력에 `[Sound] loaded: LoL/Champions/Irelia/irelia_base_sfx_26_54676396.wav` 1회, `resolve failed`/`createSound failed` 0건.
2. 포탑·억제기 사방 밀착 클릭 주행/어택 체이스/대시 착지 — 끼임 없이 슬라이드. 아군 봇 동일 초크 동시 통과.
3. 포탑 첫 파괴 후: 깬 팀 수비 전환 없음, 래치된 봇도 미드에서 파밍/교전 지속. 잭스 정지 재현 시 F1 AI 패널 state/intent/lastBlockReason 캡처(프랙티스 Dummy 스폰 여부 판별 포함).
4. 체력 12% 이내 열세 교전 개시 / 저체력·포탑 위험 Retreat 유지.
5. 정글 4~10 HP450/AD40, 에픽 불변, 체력바 = 미니언 폭.
6. 9키 정의 리로드 후 중립몹 스탯 불변.

## 잔여 (다음 슬라이스)

- 이렐리아 청음 후 6슬롯 개별 WAV 재매핑(데이터 작업), 거리감쇠/3D 사운드.
- 챔피언-챔피언 soft separation(LoL식 — 웨지 해소로 증상 소멸 예상, 관찰 후 결정), 반경 인식 path grid/구조물 un-carve(원 계획서 §1-12), DashArrival 풋프린트 검증, EmitBasicAttackCommand 사거리 사전거절→AttackChase 위임.
- AI 프로필 데이터드리븐(팩 오버레이), 스모크 회귀를 만든 07-15 밸런스 델타 계측(now 하네스 고정으로 비차단).
- AI 학습 증거 12GB의 외부 저장 매체 결정(git 제외 확정).
- gotcha 후보: "영역 통째 git add 전 `git diff --cached --stat`으로 Bin/evidence 스윕 확인" — 사용자 승인 시 gotchas.md 등재.

## 롤백 범위

S036 코드 변경은 계획서 §1의 13파일+팩 재생성+SimLab 하네스 2블록+어댑터 include 3건 — 파일 단위 revert 가능. 커밋 a1d1d00(sim/server)이 AI/이동/정글 본체, f314bde(engine)가 사운드.
