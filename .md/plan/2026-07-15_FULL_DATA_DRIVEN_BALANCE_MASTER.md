# 2026-07-15 완전 Data-Driven 밸런스 마스터 로드맵 — 빌드 1회 → JSON 수정 → 반영 → 플레이 → 크로노 되감기

- 근거 감사: [2026-07-15_DATA_DRIVEN_BALANCE_AUDIT.md](2026-07-15_DATA_DRIVEN_BALANCE_AUDIT.md) (전 항목 파일:라인 앵커, 핵심 앵커 12건 재검증 CONFIRMED)
- 상위 트랙 정합: `.md/plan/collab-pipeline/00_INDEX_DATA_DRIVEN_ENTITY_PIPELINE.md` 의 P3~P9 컷오버 트랙을 **대체하지 않고 재배열**한다. 기존 트랙은 핫리로드를 P9(선택)로 뒀지만, 사용자 목표(기획자 크로노 루프)가 핫리로드를 키스톤으로 승격시킨다. P3d(fallback 삭제)/P4/P6/P8 의 종료 조건과 07 문서 게이트는 그대로 유효.
- S033 이연 항목 승계: SkillScalingTable 활성(더블카운트 감사 선행), 아이템 팩 편입.
- 첫 실행 계획서: [2026-07-15_M0_RUNTIME_DEFPACK_RELOAD_PLAN.md](2026-07-15_M0_RUNTIME_DEFPACK_RELOAD_PLAN.md) — 바로 착수 가능.

## 아키텍처 결정 (감사 근거)

**D1. 핫어플라이 본선 = 런타임 정의 팩 오버레이 리로드 (per-entity 오버라이드 확장 아님).**
- 근거: `tc.pDefinitions` 는 매 틱 포인터 주입(GameRoomTick.cpp:124-133)이라 호출부 무수정 스왑 가능. practice 오버라이드는 per-entity/32캡/쿨다운·마나 데드존이라 "밸런스 패치" 시맨틱이 안 나온다. 오버라이드 채널은 **개별 실험용으로 존치**, 정의 레벨 변경은 팩 리로드.
- 방식: 코드젠 팩 **복사 후 진실 JSON 값 덮어쓰기(overlay)** — JSON 에 없는 필드는 소성값 유지. 파서 실패/미지 키 = 전체 거부(활성 팩 불변). 구조(스테이지 수/타겟 모드/키 집합)는 여전히 코드젠 소관 → **구조 변경만 리빌드, 수치 변경은 무리빌드**.
- 결정론: 기본 경로는 소성 팩 그대로(SimLab 골든 85A270CA375932B7 불변). 리로드는 practice op(신규 25)로만 발동 — WRPL AuthoringMutation 으로 자동 저널링(ClassifyReplayCommandDomain default 분기)되어 기존 크로노/리플레이 계약에 얹힌다.

**D2. 코드젠은 유지한다 (제거 아님).**
- Release/골든/CI 의 진실은 계속 소성 팩. 런타임 로더는 Debug 디자이너 루프 전용 오버레이. `--check` 신선도 게이트 유지 — 세션 종료 시 JSON 커밋 + 코드젠 재생성으로 소성값과 재수렴(리로드가 읽는 파일 = 코드젠이 읽는 파일과 동일하므로 드리프트 없음).

**D3. JSON 이 없는 도메인(경제/아이템/공용전투/미니언튜닝)은 "신규 JSON → 코드젠 소성 → 리로드 편입" 순서로 같은 원자 프로토콜을 반복한다** (추출→패리티→reader 전환→게이트→fallback 삭제, 07 문서 규칙 그대로).

**D4. SkillScalingTable 은 계속 이연** (S033 결정 유지). flat 주입 사이트(17챔피언 훅 + CommandExecutor 폴백)의 ratio 전환 감사가 선행되기 전에 표를 채우지 않는다. 팩 리로드가 데미지 튜닝을 이미 커버하므로 긴급도 낮음.

**D5. Bot AI 는 GameCommand 생산자다.** 어떤 리로드 경로도 클라이언트가 게임플레이 진실을 직접 변조하지 않는다 — 리로드는 서버 practice op 검증 경로로만 발동하고, 클라 JSON 파싱은 명령 생성까지만.

**D6. 부재 기능은 마이그레이션 대상이 아니라 설계 결정 대상.** 크리틱 확인: HP/마나 리젠, Ignite(로드아웃 id 14 죽은 데이터), 정글 보상/버프(바론·드래곤 = HUD 카운터뿐), 분수 레이저/스폰 무적/부쉬 은신, 죽음 타이머 스케일링은 **구현 자체가 없다**. 데이터화 계획에 섞지 않고 별도 기능 백로그로 관리(M2 에 정글 보상만 편입 — 경제 정합상 필수).

## 슬라이스 로드맵

### M0. 런타임 정의 팩 리로드 (키스톤 — 계획서 완성, 바로 착수)
- 범위: champions.json + SkillEffectGameplayDefs.json + SummonerSpellGameplayDefs.json → Debug 서버 런타임 오버레이 + `ReloadGameplayDefinitions` op(25) + ChampionTuner 리로드 버튼 + StatComponent 전체 bDirty. SpawnObject 팩 제외(스폰 시점 복사 의미론 — M4).
- 클라 패리티(크리틱 검증): HP/스탯/쿨다운/XP/골드/사거리 링은 전부 스냅샷 리플리케이션 → **클라 수정 없이 1틱 내 전파**. 알려진 잔여 거짓말 표면 2곳은 문서화된 캐리오버: Kalista 패시브 대시 예측(클라 소성값 — 러버밴딩 가능), 액션 락 애니 페이싱(Scene_InGameNetwork.cpp:332) — M6 에서 회수.
- 게이트: 5빌드 exit 0, SimLab 골든 불변(85A270CA375932B7), F5 눈검증 = Q 데미지 JSON 수정→리로드 버튼→즉시 반영→되감기 후 신값 유지.

### M1. 스킬 JSON 커버리지 완성 (기존 P3a/P3b/P3c 마무리)
- 감사 §4.3 누락 스킬 전량: `skill.yasuo.w`(윈드월 — 신규 param id 필요: HalfLength/HalfLengthPerRank/HalfThickness/FormationSec 류), `skill.sylas.r`(강탈 45s), `skill.kalista.q`(생 리터럴 70dmg), `skill.ashe.q/e/basic_attack`, `skill.fiora.e`, `skill.jax.w/r`, `skill.masteryi.q/w/e`, Kindred E 3타(`MaxStacks` 재사용), Annie/Sylas 패시브(PassivePolicy 원자 — 09 계획 P3a).
- 컴포넌트 기본값 밸런스(§4.4)를 def-resolve 로 전환. Kalista R 드리프트 폴백 정리(P3d 방식: 폴백 0.f 통일 + pack-miss 카운터).
- 게이트: `Get-LoLDataDrivenGoalStatus.ps1` p3SkillEffectHardcode 감소, SimLab 골든 의도 갱신 1회.

### M2. 경제/보상/XP 데이터화 (JSON 신설)
- 신규 `Data/LoL/ServerPrivate/Gameplay/EconomyGameplayDefs.json`: XP 곡선 17개, 킬 300/어시 150/퍼블 100, victimNextLevelXPFactor 0.5, 미니언/포탑 보상, 공유 반경 20(2파일 중복 단일화), 패시브 골드(3300틱/30틱/2g), 어시 윈도우 10s, 리콜 2s/와드 90s.
- Build-LoLDefinitionPack.py 확장 소성 + RewardRegistry `LoadFromPack()` 전환(ctor 하드코딩 삭제) + M0 리로드에 편입(레지스트리 재적재).
- 동시 청산: 죽은 필드(teamXP/goldGrowth/maxKillerGold) 구현-또는-삭제 결정, **정글 보상 분기 신설**(eRewardSourceKind::Jungle 등록 — 현재 캠프/에픽 = 0골드 0XP), `ChampionAIValuation.h` 그림자 상수(300/21/14/250)를 팩 참조로 전환(봇 가치판단 자동 동기).
- 게이트: SimLab 경제 프로브 의도 갱신, 봇 구매 루프 회귀 없음.

### M3. 아이템 카탈로그 데이터화 (S033 이연 집행)
- 신규 `ItemGameplayDefs.json`(34종 가격+스탯 + 와드 트린켓 통합) → 소성 + CItemRegistry 팩 전환 + 리로드 편입.
- **클라 필수 배선**(크리틱 MUST): 상점 UI 는 씬 초기화 때 클라 컴파일 ItemDef.h 를 소성(LoLUIContentRegistry.cpp) — 리로드 revision 통지 + 기존 `ReapplyLoLShopItems` 재호출로 매치 중 가격/스탯 표시 갱신.
- 게이트: 상점 E2E + BuyItem 가격 오버라이드(op19 Price)와 공존 확인.

### M4. 미니언/웨이브/구조물/정글 잔여 (기존 P6)
- ServerMinionTuning.h 밸런스 분(웨이브 캐던스/윈드업/분리력) + 시간 성장 2.5%/min + 웨이브 구성(3+3+공성 3웨이브) → SpawnObjectGameplayDefs.json 확장. 원거리 미니언 투사체(14/0.45/0.85), 시체 타이머 1.2/1.5 단일화, 정글 AI 잔여(복귀 반경 0.5/시야 10/Tibbers 펫 거리) 포함.
- SpawnObject 팩 리로드 편입: 리로드 후 신규 스폰(다음 웨이브)부터 적용 + 구조물은 기존 op24(ClearStructureStatOverrides) 재기반화로 즉시 재적용.
- 이중 진실 제거: MinionCombatDef.h 클라 소비(오프라인 스폰 경로 한정임을 크리틱 확인 — 격리 or 공유 정의 전환).
- **포탑 투사체 DamagePipeline 경유 전환**(현재 raw HP 차감 — 방어력/실드/킬 보상 우회; 밸런스 변동이므로 골든 의도 갱신).

### M5. 오버라이드 데드존/공용 전투 마감
- 쿨다운/마나/캐스트타임 resolve 를 world-aware 오버로드로 전환(수락-후-무시 버그 수정, GameplayDefinitionQuery.cpp:270-319).
- 공용 전투 상수 처리 결정: 치명타 1.75(현재 아무도 안 씀 = 전역 고정)/CDR 캡 0.4(2곳 중복)/AS 클램프/랭크 게이트 6·11·16 → 데이터화 vs 코어 룰 잔류(성장 곡선 0.7025+0.0175n 은 잔류 후보 — 확인 필요).
- 폴백 상수 전면 삭제(P3d 종료): BA 55(**2곳** — CommandExecutor.cpp:2954 + CombatActionSystem.cpp:73)/스킬 45+25*rank/투사체 55+25 → 하드 실패 + HUD 카운터.
- 랭크 공식 정규화(rank vs rank-1), 클라 로컬 데미지 경로는 네트워크 플레이에서 도달 불가 확인됨(크리틱) — 오프라인 전용 명시 게이트만 추가.

### M6. 크로노 루프 마감 + 신뢰 장치
- Hello 핸드셰이크에 팩 해시(0x57A21F98 계열) + 리로드 revision 포함, 매치 중 revision 통지 이벤트, 불일치 가시화(현재 레거시 해시만·Debug 로그만).
- 클라 소성값 잔여 회수: Kalista 패시브 대시 예측 파라미터, 액션 락 애니 페이싱, 클라 *_Registration.cpp 스킬 리터럴(P4 timing contract 와 합류).
- 리로드 UX 증분: 파일 워처(collab-pipeline 05 계획 CDataHotReload 재사용) 또는 단축키 — 수동 버튼 이후.
- WRPL faithful re-sim 러너(밸런스 바꿔 리플레이 재시뮬)는 별도 트랙 유지 — AI 크로노 브레이크(S015~S02x) 계보와 합류 지점.
- Release "designer build" 구성 여부 결정(현재 전부 _DEBUG).

## 기능 백로그로 분리 (데이터화 아님 — 설계 결정 필요)

HP/마나 리젠 스탯, Ignite 캐스트 커맨드, 정글 버프(레드/블루)·바론/드래곤 팀 버프, 분수 회복/레이저, 스폰 무적, 부쉬 은신(서버측), 레벨/시간 스케일 죽음 타이머, 실드 정책(현재 단일 슬롯 덮어쓰기·kMaxBuffs 16 침묵 드랍).

## 실행 방식

- 각 슬라이스는 `/plan-rules` 형식 개별 계획서(M0 완료) 또는 `08_DATA_DRIVEN_FULL_CUTOVER_CODEX_PROMPT.md` 로 Codex 위임.
- 전 슬라이스 공통 게이트: `python Tools/LoLData/Build-LoLDefinitionPack.py --check` → `Verify-LoLDataDrivenPipeline.ps1` → MSBuild Engine/GameSim/Server/Client/SimLab Debug x64 → `Tools/SimLab/x64/Debug/SimLab.exe 1800 42` 골든(변경 의도 시에만 해시 갱신 + 사유 기록) → F5 인게임 눈검증(사용자 게이트).
- 롤백 단위 = 슬라이스(각 슬라이스 독립 커밋; M0 은 신규 파일 중심이라 revert 안전).
- 순서 의존: M0 → (M1, M2, M3 병렬 가능) → M4 → M5 → M6. M1~M4 는 각각 완료 즉시 M0 리로드의 커버리지가 자동 확장된다.
