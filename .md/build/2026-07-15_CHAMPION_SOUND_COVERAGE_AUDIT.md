# 2026-07-15 챔피언 사운드 커버리지 전수 감사 보고

요청: "챔피언 폴더 기준 BA/Q/W/E/R/사망 사운드가 15개 캐릭터 전부 반영 안 된 건지 검토 + BGM SoundManager 방식 적용 검토"

## 결론

**미반영 챔피언 없음. 17종 × 6이벤트 = 102칸 전부 배선 완비.** (로스터는 15가 아니라 17종 — eChampion IRELIA..LEESIN, `Shared/GameSim/Definitions/LoLMatchContext.h:5-26`)

| 계층 | 검증 결과 |
|---|---|
| enum ↔ 이름 테이블 | 17/17 (`ChampionSoundCatalog.cpp:25-44`) |
| `Data/LoL/Sound/ChampionSoundMap.json` | 17챔피언 × 6슬롯 = 102칸 전부 채움, 레포에 단 1부(스테일 사본 없음) |
| 디스크 wav 파일 | JSON이 가리키는 102파일 전부 실존 (`Client/Bin/Resource/Sound/LoL/Champions/<이름>/`) |
| 코드 트리거 | 챔피언 화이트리스트/하드코딩 게이트 없음 — 17종 균일 (`Scene_InGameNetwork.cpp:1099-1136`) |

## BGM SoundManager 방식 포팅 — 불필요 (오히려 회귀)

- 챔피언 스킬/사망/평타는 **이미 CSound_Manager 경유**: `PlayNetworkChampionSound` → `CGameInstance::PlayEffect` → `CSound_Manager::PlayEffect`(`Sound_Manager.cpp:73-82`) — BGM과 **같은 매니저, 같은 사운드 레지스트리**(`m_mapSounds`, 부팅 시 Resource/Sound 재귀 로드).
- `PlayBGM` 은 고정 단일 채널 + `FMOD_LOOP_NORMAL` + 이전 소리 교체 방식 — SFX 에 쓰면 연속 평타/스킬이 서로 끊고 무한 루프됨. 현재의 자동 채널 `PlayEffect` 가 SFX 에 올바른 선택.
- 참고: `PlayBGM`/`PlaySoundOn` 은 현재 코드베이스에서 **호출부 0곳** — "검증된 BGM 경로"라는 전제 자체가 성립하지 않고, 실제 검증된 경로가 챔피언 `PlayEffect` 쪽이다.

## 그럼에도 인게임에서 무음일 수 있는 런타임 원인 (배선 외적)

1. **첫 관측 게이트**: 엔티티가 스냅샷에 처음 등장한 직후의 첫 액션/사망은 의도적으로 무음 (`bFirstAnimStateObservation`, `Scene_InGameNetwork.cpp:1099,1132`). 챔피언 무관 균일 — 게임 시작 직후/시야 진입 직후 첫 스킬은 원래 안 울림.
2. **Data 루트 앵커**: `WintersPaths.cpp:142-169` 가 `Winters.sln` 옆 `Data\` 만 인정 — sln 없는 배포 폴더에서 실행하면 카탈로그 로드 실패로 **전 챔피언 일괄 무음**(일부만 무음이면 이 원인 아님). 개발 트리는 정상.
3. **FMOD 디코드 실패**: 특정 wav 가 `createSound` 실패하면 그 키만 레지스트리 미등록 → 해당 슬롯만 무음. 부팅 로그 `[Sound] loaded:` (`Sound_Manager.cpp:180`) 로 확인 가능.
4. Recall/Viego 영혼 흡수는 6이벤트 밖 — 설계상 무음.

## 이번에 반영한 것 (유일한 실질 약점 = 미스 무음 삼킴)

- `Scene_InGameNetwork.cpp` `PlayNetworkChampionSound`: 카탈로그 미스 시 bounded 16회 `[Sound] champion sound catalog miss champion=N slot=N` 진단 로그 추가 — 이제 특정 챔피언/슬롯이 조용히 실패하면 디버그 출력으로 즉시 관측된다 (기존: 완전 무음 삼킴).

## 반영 안 한 것 (후속 후보)

- `WintersPaths.cpp` Data 폴백에 exe-상대 후보 추가 (배포 패키징 견고성 — 개발 루프와 무관하여 보류)
- 잔여 사운드 작업은 기존 메모대로 청음 재매핑 + 거리 감쇠

## 검증

- 빌드 미검증 (사용자 지시로 빌드 이연). 진단 로그는 Client Debug 에서 `WINTERS_ENABLE_NON_AI_DEBUG_STRING=1` 게이트로 출력됨.
- 재현 절차: 무음 케이스 발생 시 DebugView/VS 출력창에서 `[Sound]` 필터 → catalog miss 로그가 찍히면 매핑 문제, 안 찍히면 위 런타임 원인 1~3 순서로 진단.
