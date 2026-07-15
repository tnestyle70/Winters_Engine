Session - 인게임 사운드 무음, 봇 공격성(PlayerLike 게이트·미드 래치·Farm 회귀), 구조물/챔피언 끼임, 정글몹 스탯·체력바를 원인 확정 후 반영하고 빌드·SimLab·push까지 완주한다.

작성일: 2026-07-15 (Agent: Claude)
성격: 세션 문서 — §0 원인(확정) + §1 반영 코드 + §2 검증. 중단 세션 계획서 `.md/plan/2026-07-15_INGAME_SOUND_AI_NAV_JUNGLE_FIX_PLAN.md`를 승계하고, 멀티에이전트 워크플로(조사 4 + 적대 검증 4, 툴콜 423회)로 전 원인을 재검증해 갱신했다. 원 계획서와 다른 결정은 "승계 차이"에 명시한다.

## Current sequence

```text
원인 조사+적대 검증(완료) -> 본 문서 박제 -> 코드/데이터 반영 -> 팩 재생성 -> msbuild 전체 빌드 -> SimLab -> 커밋 분할 -> rebase origin/main -> push
```

## Goal

①챔피언/BGM 전 사운드 무음 수복 + 이렐리아 BA/Q/W/E/R/사망 6슬롯을 `irelia_base_sfx_26_54676396.wav`로 통일(청음 probe) ②봇 공격성 회복: PlayerLike 체력 하드게이트 완화 + GroupMidDefense 영구 래치 활동화 + Farm 유지 비대칭 정렬 + illegal Farm 회귀(RunValidation FAIL) 수복 ③포탑/억제기 주변·아군 챔피언 동반 끼임 수복(클램프 반경 정합 + 풋프린트 탈출 + 슬라이드) ④비-에픽 정글몹(4~10) HP 450(=미니언 225×2)/AD 40(=미니언 동일) + 체력바 폭 정상화. 이후 빌드 검증과 origin push(노트북 pull+Resource.zip 해제만으로 빌드 가능 상태).

## Non-goals

- 에픽 몹(Baron 0/Dragon 1/Blue 2/Red 3) 스탯 변경, 정글 보상 축(EconomyGameplayDefs) 변경.
- 사운드 거리감쇠/3D/청음 재매핑(사운드 세션 잔여 유지), StarCraft BGM 호출 복구(트리·git 이력에 호출 부재 확인).
- 챔피언-챔피언 soft separation 신설(§0-3 해소로 증상 소멸 예상 — 잔여로 이관), 반경 인식 path grid 이중화·구조물 un-carve(원 계획서 §1-12 — 잔여), DashArrival 풋프린트 검증(클램프 0.5 캡으로 진입로 소멸 — 잔여), ChampionAIComponent 신규 필드(ABI/키프레임 레지스트리 무접촉).
- Recall 배선 신설 금지: retreat-도착/retreat-막힘 recall + SimLab probe까지 **이미 배선 완료 확인** — 재계획하지 않는다.

## Why this order

계획서 박제 → 반영이 사용자 직접 적용 워크플로(gotcha 2026-05-14). 사운드/정글은 데이터+국소 코드, AI/이동은 GameSim 결정론에 닿으므로 SimLab 골든 변동(의도)을 한 커밋 경계로 묶는다. **Bot AI는 GameCommand 생산자이며 게임플레이 truth를 직접 변경하지 않는다** — 이 세션의 AI 변경은 전부 intent 결정/명령 생산 단계이고, 이동 변경은 sim 시스템(MoveSystem/WalkabilityAuthority) 소관이다. `hard safety -> active commitment -> new utility` 순서(gotcha 2026-07-12)는 모든 AI 변경에서 보존된다.

## 협업 경계 / 승계 차이

- 원 계획서 대비 결정 변경 3건:
  1. **사운드**: `WintersResolveResourceDirectory` 신설+시작 시 전량 스캔(원 §1-1~1-4) 대신 **첫 재생 시 lazy 로드**. 근거: 전량 스캔 수복 시 WAV 4,451개/1.7GB가 시작 시 FMOD 샘플로 전부 적재된다(기존 "전량 프리로드"는 경로 결함으로 한 번도 동작한 적 없음). lazy는 Engine 공개 API 추가 없이 기존 `WintersResolveContentPath`(`Sound\` 접두 지원 확인, WintersPaths.cpp:82)를 재사용한다.
  2. **끼임**: 반경 인식 path 파이프라인 전면 배선(원 §1-11~1-12, 인터페이스 시그니처+전 호출자+이중 그리드) 대신 **클램프 반경 0.5 캡 + 풋프린트 탈출 + 그레이징 클램프 웨이포인트 스킵** 3점 수술. 계획 그리드(0.5 팽창)와 보행 클램프(0.75)의 비대칭이 확정 원인이므로 보행 측을 계획 측에 정합시키는 것이 최소 수정이다(미니언은 이미 0.5/0.5 정합이라 무증상인 것도 방증).
  3. **미드 방어**: 무위협 6초 타이머 필드 신설(원 §1-10) 대신 **앵커 근접 시 ExecuteLaneCombat 상시 위임**. ChampionAIComponent는 sizeof/offset static_assert(2928B)+크로노 키프레임 레지스트리 대상이라 필드 추가 비용이 크고, 위임만으로 "집결 유지 + 활동(파밍/추격/교전) 재개"를 동시에 얻는다. 래치 자체는 유지(진입 조건만 자기 팀 한정).
- ChampionSoundCatalog의 `m_bLoaded=true` 선고정(원 §1-5)은 **무변경**: 해석/파싱 실패 트레이스가 이미 존재하고 1회 시도 latch는 의도 동작 — JSON은 현재 정상 해석된다.
- EmitBasicAttackCommand 사전 사거리 거절 삭제(원 §1-10 일부)는 **보류**: AttackChase 위임은 옳은 방향이나 이번 증상(끼임·공격성)의 확정 원인이 아니고 전투 리듬 전반에 파급 — 잔여로 이관.
- 워킹트리는 커밋 f9d4d5c 이후 전면 미커밋(500 M+277 ??+6 D, 타 세션 누적) — **기존 더티는 무접촉, 이 세션은 추가 diff만** 만든다. push 시 원격 8커밋(챔피언 조립 리팩터 `9bab5b3`/`022b6df` 포함)과 rebase 충돌 가능 — §2-5 절차 준수.
- SimLab 골든 정본 18110C0D는 AI/이동/정글 수치 변경으로 **의도 변동**한다. 사운드 wav는 git 비추적(Resource.zip 이동) — 사용자 확인으로 zip에 이미 포함.

---

## §0 원인 (전부 적대적 검증 통과, 코드 재확인 완료)

### 0-1. 인게임 사운드 전면 무음

- `CSound_Manager::LoadSoundFolder`(Engine/Private/Sound/Sound_Manager.cpp:130-144)가 exe 폴더 기준 `<exeDir>\Resource\Sound\`를 직접 조합. exe는 `Client\Bin\Debug\WintersGame.exe`(Client.vcxproj:38 OutDir)인데 `Client\Bin\Debug\Resource`는 부재(실물 확인, 숨김/정션 포함) → `FindFirstFileW` 즉시 실패 → `m_mapSounds` 0건 → `PlaySoundOn/PlayEffect/PlayBGM` 전부 `m_mapSounds.end()` 무음 return(:60,:77,:88 — 진단 0). BGM/효과음이 같은 캐시라 동반 무음.
- 텍스처/모델/JSON이 멀쩡한 이유: 그쪽은 `WintersResolveContentPath`의 canonical 상향 탐색(WintersPaths.cpp:93-131, exe에서 8단계 상향으로 `Client\Bin\Resource\` 발견)을 탄다. 사운드 로더만 유일하게 우회.
- 기각된 가설(검증 완료): 키 포맷 불일치(등록 키 = Resource/Sound 상대 forward-slash 경로로 JSON과 바이트 동일), JSON 미해석(Data 루트 상향 탐색 정상+실패 트레이스 실존), 파싱 실패, 게이트 오동작(첫 관측 래치/시퀀스 전진 게이트는 설계대로), 볼륨/뮤트, 스텁.
- 훅 위치(사용자 질문 답): BA/Q/W/E/R = `Scene_InGameNetwork.cpp` `UpdateNetworkChampionLocomotion`의 액션 시퀀스 전진 게이트(:1144-1149)에서 `PlayNetworkChampionActionSound`(:221-243) 1회 호출, 사망 = 사망 포즈 게이트(:1088-1117). Irelia_Skills/FX cue에 PlayEffect를 넣지 않는다(중복/누락 원인). 카탈로그 miss는 이미 bounded 트레이스(:203-217) 보유.

### 0-2. 봇 공격성 저하 — 3중 사슬 + Farm 회귀

1. **PlayerLike 체력 하드게이트**: 난이도≥2 봇 전원(로비 기본 난이도 2 → 로스터 봇 전원)이 PlayerLike(GameRoomSpawn.cpp:679-681). `fDecisionSelfHpRatio >= fDecisionEnemyHpRatio`가 AND 게이트(ChampionAIBrain.cpp:95-100)라 체력 동률 미만이면 점수 무관 교전 불가. 후퇴 문턱 0.55로 RuleBased(0.65)보다 낮음.
2. **Farm 유지 비대칭**: intent 유지 블록(ChampionAIBrain.cpp:81-91)에서 AttackChampion 유지는 조건부인데 FarmMinion은 **무조건 유지**(:89), 기본 반환도 FarmMinion(:111), 공격 시도 실패 시 즉시 Farm 강등(ChampionAISystem.cpp:3802) — 자기강화 파밍 루프. 유지시간 = 0.80s×1.5.
3. **GroupMidDefense 래치**: 아군 외곽 포탑 또는 **팀 무관** 미드 포탑 1기 파괴 시 전 봇 래치(ChampionAISystem.cpp:4395-4405). 래치 후 Recalling 외 전 상태 선점(:4436-4448), 집결 앵커 = 자기 팀 미드 포탑 후방 2.25/간격 1.25 포메이션(:274-374) — **구조물 밀착 밀집**(끼임 노출 증폭). 앵커 기준 9유닛 안에 위협이 없으면 교전/파밍 없이 제자리(:3844-3888). 해제는 사망/부활 리셋(GameRoom.cpp:195 경유)뿐이나 포탑이 죽어있는 한 다음 결정 틱에 재래치 — 행동상 영구. 잭스 미드 정지의 최유력 원인(대안: 프랙티스 Dummy 스폰 — F1 패널로 판별, §2-4).
4. **illegal Farm 회귀(확정)**: Farm 후보 legality는 `enemyMinion != NULL` 요구(ChampionAISystem.cpp:4105-4109)인데 Farm **선택**은 미니언 없이도 가능(기본 폴스루+무조건 유지+꼬리 강등→FollowWave Move). 실행기는 mask를 안 보므로 Accepted. SimLab 스모크 강제 최종틱(240)에서 `bPostComboBAAllowed`가 HP 동률 재유도(ShouldContinueBasicAttackAfterCombo 엄격 `<`, :1112-1121)로 꺼지고 TryEmitAttackChampion이 조기 return(:3027-3030) → 미니언 없는 스모크에서 Farm 행 생성 → `selected=3, legal=0x3` → RunValidation FAIL.
- 부수 확인: 공격성 수치는 전부 하드코딩 C++(JSON 0건). Ashe aggression 0.75(최저)+farm 가중 1.265배. fChampionScoreMargin 0.10, fSkillCastMinInterval 5s(전 챔피언 쿨다운 3s 오버라이드 활성과 부정합). Recall은 retreat-도착/막힘 3s 경유로 완비(변경 금지).

### 0-3. 구조물/챔피언 끼임

- **계획 0.5 vs 보행 0.75 비대칭**: 경로/목표는 0.5 팽창 그리드에서 계획(GameRoomNav.cpp:381, kPathAgentRadius=0.5)되는데 매 틱 클램프는 base 그리드+챔피언 spatialRadius 0.75(GameRoomSpawn.cpp:725, MoveSystem.cpp:590,639). 0.5는 통과하고 0.75는 막히는 "죽은 링"이 포탑(1.5)/억제기(1.8)/넥서스(2.8) carve 주변에 생긴다. 미니언은 0.5/0.5 정합이라 무증상.
- **완전 웨지**: 대시 착지 스냅(DashArrival.h:20, 중심 셀만 검사— 잭스 Q 포함 10개 사이트)/플래시/강제이동 종료가 풋프린트 미검증 위치를 허용 → `SegmentWalkable`이 시작 풋프린트 게이트(NavGrid.cpp:210)로 전 방향 실패 → `TryClampMoveSegmentXZ`의 유일한 탈출은 **중심 셀이 막혔을 때뿐**(WalkabilityAuthority.cpp:394-408) → 전 이동 실패.
- **클램프 = 전체 취소**: 한 세그먼트라도 클램프되면 이동 전체 취소+Idle(MoveSystem.cpp:641-644, 786-792). 취소가 blockedMoveTicks를 0으로 리셋(ClearMoveRuntimeTarget :95)해 retreat 3s recall 탈출도 사장. AI는 동일 Move 재발행(HasEquivalentMoveTarget는 target 소거 후 false) → 발행→클램프→취소→재발행 정지 루프. 사람 클릭도 동일("클릭이 안 먹음").
- **아군끼리 끼임의 정체**: 서버에 챔피언-챔피언 충돌 해소가 **존재하지 않음**(MoveSystem 블로커는 NeutralUnit뿐 :171-174, depenetration은 미니언 전용) — 몸이 서로 막는 게 아니라 **같은 초크에서 각자 구조물 웨지로 동반 빙결**된 것. 웨지 해소로 증상 소멸 예상, soft separation은 잔여.

### 0-4. 정글몹 스탯·체력바

- 스탯은 순수 데이터: SpawnObjectGameplayDefs.json → Build-LoLDefinitionPack.py 코드젠(LoLGameplayDefinitions.generated.cpp, buildHash 0xF78781F7) → 스폰 시 복사(GameRoomSpawn.cpp:317-373). 현재 Krug 1400/55, Gromp 2000/60, Wolf 1600/55, Razorbeak 1200/45, 미니 3종 350~400/25, default 1500/45. 미니언 기준(동일 팩) 225/40.
- 체력바 폭은 HP와 **무관**: 고정 43.088px × fWidthScale(UI_Manager.cpp:4220-4221)이고 Scene_InGame.cpp:727-728이 비-에픽에 `fWidthScale = 3.f` 강제. HP만 내리면 바는 그대로 — 스칼라 제거 필수.
- 클라 로컬 폴백(Jungle_Manager.cpp:562-566)은 default 1500/Baron 8000/Dragon 5000만 분기 — 서버 값과 재정합 필요(로컬 F5 전용).
- 핫리로드(op25) 시 전 StatComponent 일괄 dirty(GameRoomCommands.cpp:2408-2413)가 championId=NONE 중립몹에도 챔피언 스탯 재계산을 트리거할 수 있음 — 가드 필요.
- 미니 3종(8~10)은 현재 350~400/25로 450/40 적용 시 **소폭 상승**한다 — "비-에픽 일괄 미니언×2/미니언 AD" 규칙의 의도된 결과로 수용(사용자 규칙 우선).

---

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Engine/Public/Sound/Sound_Manager.h

`CSound_Manager` 클래스 주석 블록에서 아래로 교체:

기존 코드:

```cpp
//    - Resource/Sound/ 재귀 로드 (파일명 → wstring_t 키)
```

아래로 교체:

```cpp
//    - Resource/Sound/ 상대 키를 첫 재생 시 canonical 루트에서 lazy 로드
```

private 영역에서 아래로 교체:

기존 코드:

```cpp
    // Resource/Sound/ 재귀 로드. 키는 상대 경로 (예: L"BGM/Title.wav")
    void LoadSoundFolder();
    void LoadSoundFolderRecursive(const wstring_t& strFolderPath,
                                   const wstring_t& strRelativePath);
```

아래로 교체:

```cpp
    // 키(Resource/Sound 상대 경로, 예: L"BGM/Title.wav")를 캐시에서 찾거나
    // canonical Resource 루트(WintersResolveContentPath)에서 lazy 로드한다.
    // 실패는 키별 최초 1회만 Debug 로그를 남기고 nullptr를 반환한다.
    FMOD::Sound* FindOrLoadSound(const wstring_t& strSoundKey);
```

### 1-2. C:/Users/user/Desktop/Winters/Engine/Private/Sound/Sound_Manager.cpp

`Initialize`에서 삭제할 코드:

```cpp
    LoadSoundFolder();
```

`// 재생` 섹션 전체(PlaySoundOn/PlayEffect/PlayBGM 세 함수)를 아래로 교체:

기존 코드:

```cpp
void CSound_Manager::PlaySoundOn(const wstring_t& strSoundKey, eSoundChannel eChannel, f32_t fVolume)
{
    if (!m_pSystem) return;
    auto it = m_mapSounds.find(strSoundKey);
    if (it == m_mapSounds.end()) return;

    const u32_t idx = static_cast<u32_t>(eChannel);
    if (idx >= SOUND_CHANNEL_COUNT) return;

    bool bPlaying = false;
    if (m_pChannels[idx]) m_pChannels[idx]->isPlaying(&bPlaying);
    if (bPlaying) m_pChannels[idx]->stop();

    m_pSystem->playSound(it->second, nullptr, false, &m_pChannels[idx]);
    if (m_pChannels[idx]) m_pChannels[idx]->setVolume(fVolume);
}

void CSound_Manager::PlayEffect(const wstring_t& strSoundKey, f32_t fVolume)
{
    if (!m_pSystem) return;
    auto it = m_mapSounds.find(strSoundKey);
    if (it == m_mapSounds.end()) return;

    FMOD::Channel* pChannel = nullptr;
    m_pSystem->playSound(it->second, nullptr, false, &pChannel);
    if (pChannel) pChannel->setVolume(fVolume);
}

void CSound_Manager::PlayBGM(const wstring_t& strSoundKey, f32_t fVolume)
{
    if (!m_pSystem) return;
    auto it = m_mapSounds.find(strSoundKey);
    if (it == m_mapSounds.end()) return;

    const u32_t idx = static_cast<u32_t>(eSoundChannel::BGM);
    m_pSystem->playSound(it->second, nullptr, false, &m_pChannels[idx]);
    if (m_pChannels[idx])
    {
        m_pChannels[idx]->setMode(FMOD_LOOP_NORMAL);
        m_pChannels[idx]->setVolume(fVolume);
    }
}
```

아래로 교체:

```cpp
void CSound_Manager::PlaySoundOn(const wstring_t& strSoundKey, eSoundChannel eChannel, f32_t fVolume)
{
    if (!m_pSystem) return;
    FMOD::Sound* pSound = FindOrLoadSound(strSoundKey);
    if (!pSound) return;

    const u32_t idx = static_cast<u32_t>(eChannel);
    if (idx >= SOUND_CHANNEL_COUNT) return;

    bool bPlaying = false;
    if (m_pChannels[idx]) m_pChannels[idx]->isPlaying(&bPlaying);
    if (bPlaying) m_pChannels[idx]->stop();

    m_pSystem->playSound(pSound, nullptr, false, &m_pChannels[idx]);
    if (m_pChannels[idx]) m_pChannels[idx]->setVolume(fVolume);
}

void CSound_Manager::PlayEffect(const wstring_t& strSoundKey, f32_t fVolume)
{
    if (!m_pSystem) return;
    FMOD::Sound* pSound = FindOrLoadSound(strSoundKey);
    if (!pSound) return;

    FMOD::Channel* pChannel = nullptr;
    m_pSystem->playSound(pSound, nullptr, false, &pChannel);
    if (pChannel) pChannel->setVolume(fVolume);
}

void CSound_Manager::PlayBGM(const wstring_t& strSoundKey, f32_t fVolume)
{
    if (!m_pSystem) return;
    FMOD::Sound* pSound = FindOrLoadSound(strSoundKey);
    if (!pSound) return;

    // 단일 BGM 채널 규칙: 새 BGM 재생 전 기존 채널 정지 (겹침 방지).
    const u32_t idx = static_cast<u32_t>(eSoundChannel::BGM);
    if (m_pChannels[idx]) m_pChannels[idx]->stop();
    m_pSystem->playSound(pSound, nullptr, false, &m_pChannels[idx]);
    if (m_pChannels[idx])
    {
        m_pChannels[idx]->setMode(FMOD_LOOP_NORMAL);
        m_pChannels[idx]->setVolume(fVolume);
    }
}
```

`// Resource/Sound/ 재귀 로드` 섹션 전체(LoadSoundFolder + LoadSoundFolderRecursive)를 아래로 교체:

기존 코드:

```cpp
// ─────────────────────────────────────────────────────────────
//  Resource/Sound/ 재귀 로드
// ─────────────────────────────────────────────────────────────
void CSound_Manager::LoadSoundFolder()
```

(함수 두 개 전체, 파일 끝 `NS_END` 직전까지)

아래로 교체:

```cpp
// ─────────────────────────────────────────────────────────────
//  Resource/Sound/ lazy 로드
//    - 과거 시작 시 전량 스캔은 exe 폴더(<Bin/Debug>) 기준 경로 조합이라
//      canonical 루트(Client/Bin/Resource)를 한 번도 찾지 못했다(전면 무음).
//    - 파일 해석은 Engine 공용 WintersResolveContentPath 재사용
//      ("Sound\" 접두 상대 경로 지원). 실패 진단은 키별 최초 1회 bounded.
// ─────────────────────────────────────────────────────────────
namespace
{
    std::set<wstring_t> s_SoundLoadFailLoggedKeys;
}

FMOD::Sound* CSound_Manager::FindOrLoadSound(const wstring_t& strSoundKey)
{
    auto it = m_mapSounds.find(strSoundKey);
    if (it != m_mapSounds.end())
        return it->second;

    if (!m_pSystem || strSoundKey.empty())
        return nullptr;

    const wstring_t relative = L"Sound/" + strSoundKey;
    wchar_t resolved[MAX_PATH] = {};
    if (!WintersResolveContentPath(relative.c_str(), resolved, MAX_PATH))
    {
        if (s_SoundLoadFailLoggedKeys.insert(strSoundKey).second)
            OutputDebugStringW((L"[Sound] resolve failed: " + relative + L"\n").c_str());
        return nullptr;
    }

    // FMOD 는 UTF-8 경로. wstring → UTF-8 변환.
    char utf8Path[MAX_PATH * 2] = {};
    WideCharToMultiByte(CP_UTF8, 0, resolved, -1,
                        utf8Path, sizeof(utf8Path), nullptr, nullptr);

    FMOD::Sound* pSound = nullptr;
    const FMOD_RESULT r = m_pSystem->createSound(utf8Path, FMOD_DEFAULT, nullptr, &pSound);
    if (r != FMOD_OK || !pSound)
    {
        if (s_SoundLoadFailLoggedKeys.insert(strSoundKey).second)
        {
            OutputDebugStringW((L"[Sound] createSound failed: " + strSoundKey +
                L" fmod=" + std::to_wstring(static_cast<int>(r)) + L"\n").c_str());
        }
        return nullptr;
    }

    m_mapSounds.emplace(strSoundKey, pSound);
    OutputDebugStringW((L"[Sound] loaded: " + strSoundKey + L"\n").c_str());
    return pSound;
}
```

`#include <set>`, `#include <string>` 필요 여부는 기존 include(WintersPCH.h)로 충족되는지 빌드로 확인하고, 부족하면 cpp 상단 `<fmod_errors.h>` 아래에 추가한다.

### 1-3. C:/Users/user/Desktop/Winters/Data/LoL/Sound/ChampionSoundMap.json

Irelia 블록의 sounds를 아래로 교체(청음 probe — 검증 후 개별 WAV 재매핑은 별도 데이터 작업):

기존 코드:

```json
        "basicAttack": "LoL/Champions/Irelia/irelia_base_sfx_1_1030318.wav",
        "skillQ": "LoL/Champions/Irelia/irelia_base_sfx_2_3398832.wav",
        "skillW": "LoL/Champions/Irelia/irelia_base_sfx_3_5540177.wav",
        "skillE": "LoL/Champions/Irelia/irelia_base_sfx_4_7866814.wav",
        "skillR": "LoL/Champions/Irelia/irelia_base_sfx_5_8143845.wav",
        "death": "LoL/Champions/Irelia/irelia_base_sfx_6_11814629.wav"
```

아래로 교체:

```json
        "basicAttack": "LoL/Champions/Irelia/irelia_base_sfx_26_54676396.wav",
        "skillQ": "LoL/Champions/Irelia/irelia_base_sfx_26_54676396.wav",
        "skillW": "LoL/Champions/Irelia/irelia_base_sfx_26_54676396.wav",
        "skillE": "LoL/Champions/Irelia/irelia_base_sfx_26_54676396.wav",
        "skillR": "LoL/Champions/Irelia/irelia_base_sfx_26_54676396.wav",
        "death": "LoL/Champions/Irelia/irelia_base_sfx_26_54676396.wav"
```

### 1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.cpp

`CPlayerLikeChampionBrain`의 `DecideLaneCombatIntent` 본문에서 아래로 교체:

기존 코드:

```cpp
			if (input.fRetreatScore >= 0.55f)
			{
				ai.intentHoldTimer = ai.intentHoldDuration * kCommitScale;
				return eChampionAIIntent::Retreat;
			}
```

아래로 교체:

```cpp
			if (input.fRetreatScore >= 0.65f)
			{
				ai.intentHoldTimer = ai.intentHoldDuration * kCommitScale;
				return eChampionAIIntent::Retreat;
			}
```

기존 코드:

```cpp
					(ai.intent == eChampionAIIntent::Retreat &&
						input.fRetreatScore >= 0.30f) ||
					ai.intent == eChampionAIIntent::FarmMinion)
					return ai.intent;
```

아래로 교체:

```cpp
					(ai.intent == eChampionAIIntent::Retreat &&
						input.fRetreatScore >= 0.30f) ||
					(ai.intent == eChampionAIIntent::FarmMinion &&
						!input.bCanAttackChampion &&
						!input.bCanAttackStructure))
					return ai.intent;
```

기존 코드:

```cpp
			const bool_t bHpAdvantage =
				ai.fDecisionSelfHpRatio >= ai.fDecisionEnemyHpRatio;
			if (input.bCanAttackChampion && bHpAdvantage &&
```

아래로 교체:

```cpp
			// 근소 열세까지는 교전 후보 유지 — 동률·미세 열세에서 무조건 파밍 금지.
			const bool_t bHpAdvantage =
				ai.fDecisionSelfHpRatio + kHpDisadvantageTolerance >=
					ai.fDecisionEnemyHpRatio;
			if (input.bCanAttackChampion && bHpAdvantage &&
```

private 영역에서 기존 코드:

```cpp
		// 사람처럼 결정을 더 오래 유지하는 배율
		static constexpr f32_t kCommitScale = 1.5f;
```

아래에 추가:

```cpp
		// 체력 하드게이트 완화 폭 — 이 이상 열세면 신규 교전을 열지 않는다.
		static constexpr f32_t kHpDisadvantageTolerance = 0.12f;
```

### 1-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

(a) Farm legality — Farm 선택은 미니언 없이도 웨이브 추종 Move를 내므로 legality도 CanMove 단독을 포함해야 한다(illegal Farm 회귀 수복, 진단 계약만 변경):

기존 코드:

```cpp
    if (perception.enemyMinion != NULL_ENTITY &&
        (perception.bCanMove || perception.bCanAttack))
    {
        actionMask.legalCandidateMask |= kAiCandidateFarmBitV1;
    }
```

아래로 교체:

```cpp
    // Farm 은 막타뿐 아니라 웨이브 추종(FollowWave Move)까지 포함한다:
    // 브레인 기본 폴스루가 미니언 없이도 Farm 을 선택하므로, legality 가
    // CanMove 단독을 포함하지 않으면 정상 실행된 Move 가 illegal Farm 행이 된다.
    if (perception.bCanMove ||
        (perception.enemyMinion != NULL_ENTITY && perception.bCanAttack))
    {
        actionMask.legalCandidateMask |= kAiCandidateFarmBitV1;
    }
```

(b) post-combo 조기 return 제거 — 창이 열려 있어도 연속 BA가 불허일 뿐이면 일반 콤보/스킬/BA 경로로 폴스루(교전 의사 유지):

기존 코드:

```cpp
        if (ai.fPostComboBATimer > 0.f)
        {
            if (!ai.bPostComboBAAllowed)
                return false;

            if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
```

아래로 교체:

```cpp
        if (ai.fPostComboBATimer > 0.f && ai.bPostComboBAAllowed)
        {
            if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
```

(c) 미드 포탑 손실 판정을 자기 팀 한정으로 — 함수 주석/시그니처/본문 필터:

기존 코드:

```cpp
    // 미드 레인 타워(팀 무관, 티어 무관)가 하나라도 파괴되면 true.
    // 양 팀 봇이 같은 조건으로 GroupMidDefense에 진입해 미드에서 5:5가 형성된다.
    // 넥서스 타워는 lane=Base라 이 필터에 걸리지 않는다.
    bool_t HasMidLaneTurretLost(CWorld& world)
```

아래로 교체:

```cpp
    // 자기 팀 미드 레인 타워(티어 무관)가 하나라도 파괴되면 true.
    // 방어 집결은 자기 진영 손실에만 반응한다 — 적 포탑을 깬 쪽까지
    // 수비 태세로 전환시키던 팀 무관 판정은 공격성 저하의 원인이었다.
    // 넥서스 타워는 lane=Base라 이 필터에 걸리지 않는다.
    bool_t HasMidLaneTurretLost(CWorld& world, eTeam team)
```

같은 함수 본문에서 기존 코드:

```cpp
                if (bLost ||
                    structure.kind != turretKind ||
                    structure.lane != midLane)
```

아래로 교체:

```cpp
                if (bLost ||
                    structure.team != team ||
                    structure.kind != turretKind ||
                    structure.lane != midLane)
```

호출부(ctx 구성, `if (ai.decisionTimer <= 0.f)` 블록) 기존 코드:

```cpp
            ctx.bMidLaneTurretLost =
                ai.bMidDefenseActive ||
                HasMidLaneTurretLost(world);
```

아래로 교체:

```cpp
            ctx.bMidLaneTurretLost =
                ai.bMidDefenseActive ||
                HasMidLaneTurretLost(world, champion.team);
```

(d) GroupMidDefense 활동화 — 앵커 근접 시 제자리 대기 대신 상시 LaneCombat 위임(파밍/추격/교전 재개; hard safety는 ExecuteLaneCombat 선두에서 그대로 작동). `ExecuteGroupMidDefense` 본문에서 기존 코드:

```cpp
        const f32_t anchorDistanceSq =
            WintersMath::DistanceSqXZ(selfPos, ai.midDefenseAnchor);
        if (anchorDistanceSq <=
            kChampionAIMidDefenseReturnRadius *
                kChampionAIMidDefenseReturnRadius)
        {
            auto isThreatInsideDefenseZone =
                [&](EntityID entity) -> bool_t
                {
                    Vec3 targetPos{};
                    return TryGetPosition(world, entity, targetPos) &&
                        WintersMath::DistanceSqXZ(
                            targetPos,
                            ai.midDefenseAnchor) <=
                            kChampionAIMidDefenseCombatRadius *
                                kChampionAIMidDefenseCombatRadius;
                };

            if (isThreatInsideDefenseZone(ctx.enemyChampion) ||
                isThreatInsideDefenseZone(ctx.enemyMinion))
            {
                ExecuteLaneCombat(
                    world,
                    tc,
                    self,
                    ai,
                    champion,
                    selfPos,
                    ctx,
                    outCommands);
                return;
            }
        }
```

아래로 교체:

```cpp
        const f32_t anchorDistanceSq =
            WintersMath::DistanceSqXZ(selfPos, ai.midDefenseAnchor);
        if (anchorDistanceSq <=
            kChampionAIMidDefenseReturnRadius *
                kChampionAIMidDefenseReturnRadius)
        {
            // 집결 반경 안에서는 제자리 대기 대신 일반 레인 전투로 위임한다.
            // 위협 9유닛 대기 규칙은 봇을 앵커에 영구 정지시키는 원인이었다 —
            // 미드 배정(activeLane/goal)은 래치가 유지하므로 집결은 지속된다.
            ExecuteLaneCombat(
                world,
                tc,
                self,
                ai,
                champion,
                selfPos,
                ctx,
                outCommands);
            return;
        }
```

(e) 포메이션 간격 — 챔피언 반경 0.75 두 개 합(1.5)보다 넓게:

기존 코드:

```cpp
    constexpr f32_t kChampionAIMidDefenseFormationSpacing = 1.25f;
```

아래로 교체:

```cpp
    constexpr f32_t kChampionAIMidDefenseFormationSpacing = 1.75f;
```

(f) 공격성 기본값 — margin 절반, 신규 캐스트 간격을 활성 3s 쿨다운 오버라이드에 정렬(`ApplyChampionAIProfileAndTuning`):

기존 코드:

```cpp
        ai.fChampionScoreMargin = ResolveChampionAITuningParam(
            ai.tuning.championScoreMargin, 0.10f, bOverrideProfile);
```

아래로 교체:

```cpp
        ai.fChampionScoreMargin = ResolveChampionAITuningParam(
            ai.tuning.championScoreMargin, 0.05f, bOverrideProfile);
```

기존 코드:

```cpp
        ai.fSkillCastMinInterval = ResolveChampionAITuningParam(
            ai.tuning.skillCastMinInterval, 5.f, bOverrideProfile);
```

아래로 교체:

```cpp
        ai.fSkillCastMinInterval = ResolveChampionAITuningParam(
            ai.tuning.skillCastMinInterval, 3.f, bOverrideProfile);
```

### 1-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h

`ChampionAIComponent` 기본값을 1-5(f)와 동기화(레이아웃 무변경 — static_assert 영향 없음):

기존 코드:

```cpp
	f32_t fChampionScoreMargin = 0.10f;
```

아래로 교체:

```cpp
	f32_t fChampionScoreMargin = 0.05f;
```

기존 코드:

```cpp
	f32_t fSkillCastMinInterval = 5.f;
```

아래로 교체:

```cpp
	f32_t fSkillCastMinInterval = 3.f;
```

### 1-7. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.cpp

필드 순서: preferredRange, championScan, minionScan, structureScan, leash, aggression, kiteBias, retreatHp, reengageHp, minionPressure, turretRisk, lastHit, siege.

`MakeAsheProfile` — 공격성 바닥(0.75) 상향 + 챔피언 스캔을 미니언 스캔 쪽으로:

기존 코드:

```cpp
        return ChampionAIProfile{
            eChampion::ASHE,
            6.f,
            9.f,
            12.f,
            18.f,
            16.f,
            0.75f,
```

아래로 교체:

```cpp
        return ChampionAIProfile{
            eChampion::ASHE,
            6.f,
            10.f,
            12.f,
            18.f,
            16.f,
            0.90f,
```

`MakeJaxProfile` — 시범 기준(원 계획서 값): aggression 1.35, retreatHp 0.35, reengageHp 0.55, minionPressure 0.70:

기존 코드:

```cpp
            eChampion::JAX,
            1.5f,
            7.f,
            10.f,
            18.f,
            14.f,
            1.25f,
            0.00f,
            0.50f,
            0.65f,
            0.80f,
```

아래로 교체:

```cpp
            eChampion::JAX,
            1.5f,
            7.f,
            10.f,
            18.f,
            14.f,
            1.35f,
            0.00f,
            0.35f,
            0.55f,
            0.70f,
```

### 1-8. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Move/MoveSystem.cpp

(a) 클램프 반경을 계획 그리드 팽창 반경(0.5)에 정합. `ResolveAgentRadius` 위에 상수 추가:

기존 코드:

```cpp
    bool_t IsMoveBlockingKind(eSpatialKind kind)
    {
        return kind == eSpatialKind::NeutralUnit;
    }
```

아래에 추가:

```cpp
    // 매 틱 이동 클램프 반경은 서버 path 그리드 팽창 반경
    // (ServerMinionTuning::kPathAgentRadius = 0.5f)을 넘지 않아야 한다 —
    // 계획은 0.5 로 통과하고 보행은 0.75 로 막히는 구조물 주변 죽은 링이
    // 챔피언 끼임의 원인. 챔피언 반경 0.75 는 전투/타게팅 전용으로 유지.
    constexpr f32_t kNavClearanceRadius = 0.5f;
```

(b) 클램프 호출의 반경 캡:

기존 코드:

```cpp
        const f32_t advance = (std::min)(step, dist);
        const f32_t invDist = 1.f / dist;
        const f32_t radius = ResolveAgentRadius(world, entity);
```

아래로 교체:

```cpp
        const f32_t advance = (std::min)(step, dist);
        const f32_t invDist = 1.f / dist;
        const f32_t radius =
            (std::min)(ResolveAgentRadius(world, entity), kNavClearanceRadius);
```

(c) 그레이징 클램프 = 전체 취소 정책 폐기 — 남은 웨이포인트가 있으면 스킵하며 슬라이드, 마지막 구간만 기존대로 종료. stuck 사유 트레이스는 bounded(최초 1틱):

기존 코드:

```cpp
        if (bSegmentClamped)
        {
            ClearMoveRuntimeTarget(moveTarget);
            if (!IsActionStateLocked(world, entity, stat, pAction, tc))
                SetMovePose(world, entity, ePoseStateId::Idle, tc);
            continue;
        }

        if (!IsActionStateLocked(world, entity, stat, pAction, tc))
            SetMovePose(world, entity, ePoseStateId::Run, tc);
```

아래로 교체:

```cpp
        if (bSegmentClamped)
        {
            // 스치는 클램프가 이동 전체를 취소하면 AI 가 동일 Move 를 재발행하는
            // 정지 루프가 된다. 남은 웨이포인트가 있으면 다음 지점으로 스킵해
            // carve 가장자리를 따라 미끄러지고, 마지막 구간에서만 종료한다.
            const bool_t bHasRemainingPath =
                moveTarget.pathCount > 0 &&
                moveTarget.pathIndex + 1u < moveTarget.pathCount;
            if (!bHasRemainingPath)
            {
                ClearMoveRuntimeTarget(moveTarget);
                if (!IsActionStateLocked(world, entity, stat, pAction, tc))
                    SetMovePose(world, entity, ePoseStateId::Idle, tc);
                continue;
            }

            ++moveTarget.pathIndex;
            RecordBlockedMoveTick(moveTarget);
            if (moveTarget.blockedMoveTicks == 1u)
            {
                char msg[224]{};
                sprintf_s(msg,
                    "[MoveSystem][Stuck] tick=%llu entity=%u reason=segment-clamped pathIndex=%u/%u pos=(%.3f,%.3f,%.3f)\n",
                    static_cast<unsigned long long>(tc.tickIndex),
                    static_cast<u32_t>(entity),
                    static_cast<u32_t>(moveTarget.pathIndex),
                    static_cast<u32_t>(moveTarget.pathCount),
                    resolvedNext.x,
                    resolvedNext.y,
                    resolvedNext.z);
                WintersOutputAIDebugStringA(msg);
            }
        }

        if (!IsActionStateLocked(world, entity, stat, pAction, tc))
            SetMovePose(world, entity, ePoseStateId::Run, tc);
```

### 1-9. C:/Users/user/Desktop/Winters/Server/Private/Game/WalkabilityAuthority.cpp

`TryClampMoveSegmentXZ`의 중심 셀 탈출 블록과 이분 탐색 사이에 풋프린트 오버랩 탈출 추가 — 중심 셀은 walkable인데 반경 풋프린트가 carve에 겹친 시작점은 현재 전 방향 이동 실패(완전 웨지)였다:

기존 코드:

```cpp
        outPosition = pBaseGrid->CellToWorld(nearest.x, nearest.y);
        if (!TrySampleHeight(pSurfaceSampler, outPosition.x, outPosition.z, outPosition.y))
            outPosition.y = from.y;
        return true;
    }

    f32_t low = 0.f;
    f32_t high = 1.f;
```

아래로 교체:

```cpp
        outPosition = pBaseGrid->CellToWorld(nearest.x, nearest.y);
        if (!TrySampleHeight(pSurfaceSampler, outPosition.x, outPosition.z, outPosition.y))
            outPosition.y = from.y;
        return true;
    }

    // 풋프린트 오버랩 탈출: 중심 셀은 walkable 이지만 반경 풋프린트가 carve 에
    // 겹친 시작점은 SegmentWalkable(from, *, radius) 가 전 방향 실패해 영구
    // 웨지가 된다(대시 착지/플래시/강제이동 종료 진입). 반경 0 클램프로
    // 셀 단위 보행을 허용해 겹침에서 걸어 나올 수 있게 한다.
    if (radius > 0.f && !pBaseGrid->SegmentWalkable(from, from, radius))
    {
        Vec3 escaped = desired;
        if (TryClampMoveSegmentXZ(pBaseGrid, pSurfaceSampler, from, desired, 0.f, escaped))
        {
            outPosition = escaped;
            return true;
        }

        outPosition = from;
        return false;
    }

    f32_t low = 0.f;
    f32_t high = 1.f;
```

`SegmentWalkable(from, from, radius)`는 길이 0 세그먼트로 `IsAreaWalkable(from, radius)`와 동치인 풋프린트 검사다(NavGrid.cpp:210 게이트).

### 1-10. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json

subKind 0~3(Baron/Dragon/Blue/Red)은 절대 수정하지 않는다. 아래 8곳의 `maxHp`/`attackDamage`만 변경(각 블록의 나머지 필드 유지):

| 대상 | maxHp | attackDamage |
| --- | ---: | ---: |
| defaultJungleCamp (1500/45) | 450.0 | 40.0 |
| subKind 4 Krug (1400/55) | 450.0 | 40.0 |
| subKind 5 Gromp (2000/60) | 450.0 | 40.0 |
| subKind 6 Wolf (1600/55) | 450.0 | 40.0 |
| subKind 7 Razorbeak (1200/45) | 450.0 | 40.0 |
| subKind 8 RazorbeakMini (400/25) | 450.0 | 40.0 |
| subKind 9 WolfMini (350/25) | 450.0 | 40.0 |
| subKind 10 KrugMini (350/25) | 450.0 | 40.0 |

기준: 미니언 defaultMinion maxHp 225.0 × 2 = 450, attackDamage 40.0 동일. 미니 3종은 규칙 일괄 적용으로 소폭 상승(§0-4 수용 기록). JSON 편집 후 생성기를 실행하고 generated cpp는 손대지 않는다:

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py
python Tools/LoLData/Build-LoLDefinitionPack.py --check
```

diff 게이트: generated/JSON 변경이 정글 4~10·default·buildHash에만 나타나야 하며 0~3·미니언·구조물은 불변. S035가 넣은 포탑 visibilityStates 상한 8도 재생성 후 유지 확인.

### 1-11. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

`SyncWorldHealthBarsToEngineUI`의 정글 루프에서 폭 3배 강제 제거(바 폭은 HP 무관 고정폭×스칼라 — 스칼라가 원인):

기존 코드:

```cpp
    // 정글몹: 에픽 4종(바론/드래곤/블루/레드)은 챔피언형 바(중립=적색 필),
    // 소형 캠프는 미니언형 바를 가로 3배로 넓혀서 표시한다.
```

아래로 교체:

```cpp
    // 정글몹: 에픽 4종(바론/드래곤/블루/레드)은 챔피언형 바(중립=적색 필),
    // 소형 캠프는 미니언과 같은 Unit 기본 폭을 쓴다.
```

같은 루프에서 삭제할 코드:

```cpp
            if (!bEpic)
                Bar.fWidthScale = 3.f;
```

### 1-12. C:/Users/user/Desktop/Winters/Client/Private/Manager/Jungle_Manager.cpp

로컬(비네트워크) 폴백 HP를 서버 팩 값과 재정합(`Spawn_FromEntry`):

기존 코드:

```cpp
    f32_t maxHp = 1500.f;
    if (static_cast<eJungleSub>(entry.subKind) == eJungleSub::Baron)
        maxHp = 8000.f;
    else if (static_cast<eJungleSub>(entry.subKind) == eJungleSub::Dragon)
        maxHp = 5000.f;
```

아래로 교체:

```cpp
    f32_t maxHp = 450.f;
    if (static_cast<eJungleSub>(entry.subKind) == eJungleSub::Baron)
        maxHp = 8000.f;
    else if (static_cast<eJungleSub>(entry.subKind) == eJungleSub::Dragon)
        maxHp = 5000.f;
    else if (static_cast<eJungleSub>(entry.subKind) == eJungleSub::BlueBuff ||
             static_cast<eJungleSub>(entry.subKind) == eJungleSub::RedBuff)
        maxHp = 2300.f;
```

### 1-13. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

`ReloadGameplayDefinitions`의 전량 stat dirty가 championId=NONE 중립몹(정글/미니언)에 챔피언 스탯 재계산 폴백을 적용하지 않도록 가드:

기존 코드:

```cpp
        u32_t refreshedCount = 0u;
        m_world.ForEach<StatComponent>(
            [&](EntityID, StatComponent& stat)
            {
                stat.bDirty = true;
                ++refreshedCount;
            });
```

아래로 교체:

```cpp
        u32_t refreshedCount = 0u;
        m_world.ForEach<StatComponent>(
            [&](EntityID, StatComponent& stat)
            {
                // 중립몹/미니언(championId=NONE)은 챔피언 스탯 재계산 대상이
                // 아니다 — dirty 로 만들면 SpawnObject 팩 값과 어긋난다.
                if (stat.championId == eChampion::NONE)
                    return;
                stat.bDirty = true;
                ++refreshedCount;
            });
```

## 2. 검증

빌드 전 정적 검사:

```powershell
git diff --check
python Tools/LoLData/Build-LoLDefinitionPack.py --check
```

빌드(Engine 공개 헤더 변경 포함 — sln 표준 절차, 개별 vcxproj 단독 빌드 금지, EngineSDK/inc 수동 편집 금지):

```powershell
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m
```

SimLab(결정성 + AI/이동/정글 변경으로 골든 해시 18110C0D에서 **의도 변동** — 새 해시를 이 문서와 커밋 메시지에 기록):

```powershell
Tools/Bin/SimLab.exe 1800 42
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/AIResearch/RunValidation.ps1
```

기대 로그:
- 클라 시작/전투에서 `[Sound] loaded: LoL/Champions/Irelia/irelia_base_sfx_26_54676396.wav` (첫 재생 시 1회), `[Sound] resolve failed`/`createSound failed` 0건.
- RunValidation Live 스모크: `selected=... legal=...` illegal Farm FAIL 소멸 → PASS.
- 구조물 밀착 주행 시 `[MoveSystem][Stuck] reason=segment-clamped` 는 등장 가능하나 정지 루프 없이 슬라이드.

인게임 E2E(사용자 게이트 — 서버 로그만으로 판정 금지):
1. 이렐리아 BA/Q/W/E/R/사망 각 1회 = 동일 WAV 6회 청음, R 다중 cue/재접속/과거 스냅샷 중복 없음.
2. 포탑·억제기 사방 밀착 클릭 주행/어택 체이스/대시 착지 — 끼임 없이 미끄러져 통과. 아군 봇 2기 이상 동일 초크 통과.
3. 포탑 첫 파괴 후: 깬 팀은 수비 전환 없음(자기 팀 손실만 래치), 래치된 봇도 미드에서 파밍/교전 지속(제자리 정지 소멸). 잭스 정지 재현 시 F1 AI 패널에서 state/intent/lastBlockReason 캡처(Dummy 스폰 여부 판별 포함).
4. 체력 12% 이내 열세에서 점수 우위면 교전 개시(PlayerLike), 저체력/포탑 위험은 여전히 Retreat.
5. 정글: Krug/Gromp/Wolf/Raptor+미니 HP 450·AD 40(스냅샷/서버 로그), 에픽 4종 불변, 체력바가 미니언 폭과 동일(fill만 비율).
6. 9키 정의 리로드 후 중립몹 스탯 불변(가드 동작).

롤백 범위: 본 세션 diff는 §1의 13개 파일 + 팩 재생성 산출물뿐(기존 더티 무접촉) — 파일 단위 revert로 원복 가능. SimLab 골든은 커밋 경계에서만 갱신.

push 절차(GIT_SYNC_RULES 준수):

```powershell
git add <논리 단위별> ; git commit ...   # 누적 더티 트랙별 분할 -> S036 별도 커밋
git fetch origin
git rebase origin/main                    # 9bab5b3/022b6df 챔피언 조립 리팩터 충돌 주의, 충돌 해결 시 재빌드
git diff --check
git push origin HEAD
```

잔여(다음 슬라이스): 이렐리아 6슬롯 청음 후 개별 WAV 재매핑, 거리감쇠/3D, 챔피언-챔피언 soft separation(LoL식), 반경 인식 path grid/un-carve(원 계획서 §1-12), DashArrival 풋프린트 검증, EmitBasicAttackCommand 사전 사거리 거절→AttackChase 위임, AI 프로필 데이터드리븐(팩 오버레이), SimLab tick-240 HP 고정 하드닝, 스모크 회귀의 07-15 원인 델타 계측.
