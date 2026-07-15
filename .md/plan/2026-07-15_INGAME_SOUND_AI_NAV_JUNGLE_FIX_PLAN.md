Session - 인게임 사운드, 챔피언 AI/이동, 구조물 주변 끼임, 정글몹 스탯·체력바 전수 조사 및 수정 계획 (2026-07-15)

이 문서는 분석·수정 계획서다. 이번 세션에서는 제품 C++/JSON을 수정하거나 빌드하지 않는다. 사용자가 현재 클라이언트와 서버를 실행 중이므로, 아래의 빌드·런타임 검증은 실행 권한이 주어지고 실행 중인 프로세스가 종료된 뒤에만 수행한다. 현재 워크트리의 기존 변경과 미추적 ChampionSoundCatalog.{h,cpp}, Data/LoL/Sound/ChampionSoundMap.json은 이전 세션의 사용자 작업으로 간주하며, 이 문서는 그것을 되돌리거나 완료된 것으로 주장하지 않는다.

조사 결론은 다음과 같다.

| 증상 | 확인된 원인 | 해결 원리 |
| --- | --- | --- |
| 인게임 SFX/BGM가 모두 무음 | Engine/Private/Sound/Sound_Manager.cpp의 CSound_Manager::LoadSoundFolder가 실행 파일 폴더 기준으로 Debug/Resource/Sound를 만든다. 실제 리소스는 Client/Bin/Resource/Sound에 있고 Debug/Resource/Sound는 없다. 재귀 탐색 실패와 키 누락이 모두 조용히 return되어 FMOD까지 도달하지 않는다. | Engine의 이미 존재하는 canonical Resource 탐색을 디렉터리에도 재사용하고, 로드·키 누락·FMOD 실패를 제한된 Debug 로그로 관찰 가능하게 만든다. |
| Irelia BA/Q/W/E/R/사망 사운드 위치가 불명확 | 실제 훅은 Client/Private/Scene/Scene_InGameNetwork.cpp의 UpdateNetworkChampionLocomotion이다. 서버가 복제한 ActionState sequence가 새로 도착했을 때 PlayNetworkChampionActionSound를 한 번 호출한다. | Irelia_Skills.cpp나 VFX cue에 별도 PlayEffect를 넣지 않는다. 중앙 복제 훅을 유지하고 ChampionSoundMap.json의 여섯 슬롯만 같은 검증 WAV 키로 바꾼다. |
| 봇이 소극적이고 Jax가 미드에 머묾 | 기본 봇 난이도 2 이상은 PlayerLike brain이다. 이 brain은 자기 HP가 적 HP보다 작으면 전투를 완전히 막고, Farm intent를 1.2초 고정 보유한다. Jax profile의 aggression=1.25가 있어도 이 hard gate를 넘을 수 없다. 또한 GroupMidDefense는 양 팀 어느 한쪽의 미드 포탑 손실에도 들어갈 수 있고 정상적인 원래 라인 복귀 종료가 없다. | profile 가중치와 brain의 hard gate를 분리하여 조정하고, 미드 방어 진입을 자기 팀 손실로 한정하며 위협 소멸 후의 유한 종료/원래 lane 복귀를 추가한다. |
| 아군 챔피언·포탑·억제기 근처에서 끼임 | MoveSystem의 동적 blocker는 NeutralUnit만 취급한다. 챔피언 간 soft separation/depenetration이 없다. 경로 탐색용 그리드는 반경 0.5로 팽창하지만 챔피언 이동 반경은 0.75이고 TryBuildMovePath 인터페이스에는 반경이 없다. AI는 구조물 중심으로 MoveToTarget도 보낸다. | 챔피언 반경을 경로 탐색부터 clamp·smoothing까지 전달하고, 캐릭터에는 hard block이 아닌 결정론적 soft separation을 적용한다. 공격 사거리 밖 대상은 CommandExecutor의 AttackChase에 맡겨 구조물 중심 이동을 없앤다. |
| 막힌 퇴각에서 Recall이 안 됨 | MoveSystem이 clamp 실패 때 ClearMoveRuntimeTarget을 호출해 blockedMoveTicks와 target을 먼저 지운다. ChampionAISystem의 3초 blocked-retreat recall fallback 조건은 따라서 성립하지 못한다. | 실패를 clear가 아니라 repath/blocked 상태로 보존하고, 퇴각일 때만 기존 recall fallback이 정확히 발화하게 한다. |
| 일반 정글몹 체력·체력바가 과도함 | SpawnObjectGameplayDefs.json의 일반 camp는 HP 1200~2000, AD 45~60이다. Client/Private/Scene/Scene_InGame.cpp는 Epic이 아닌 camp의 fWidthScale을 3.f로 강제한다. 체력 수치는 채움 비율이고 바 폭의 원인이 아니다. | Dragon/Blue/Red/Baron을 제외한 subKind 4~10을 HP 450(기본 minion HP 225의 2배), AD 40(기본 minion AD와 동일)으로 바꾸고, non-epic의 폭 3배 강제를 제거한다. |

사운드의 실제 데이터·호출 경로는 다음과 같다. 서버에서 공격/스킬이 허용되면 GameSim ActionState가 snapshot/event로 복제되고, Client의 UpdateNetworkChampionLocomotion이 sequence 증가를 확인한 뒤 PlayNetworkChampionActionSound 또는 사망 슬롯을 호출한다. 그 함수가 ChampionSoundCatalog 키를 CGameInstance::PlayEffect로 전달하고, Sound_Manager가 FMOD::System::playSound를 호출한다. 따라서 BA/Q/W/E/R/Death는 중앙 복제 게이트 한 곳에서 재생해야 하며, Irelia skill 구현이나 여러 개의 효과 cue에 사운드를 중복 삽입하면 Q의 이동, R의 복수 효과 cue, 재접속/과거 snapshot에서 누락 또는 중복될 수 있다.

현재 데이터의 Irelia 여섯 슬롯은 _1~_6 WAV를 가리키고, 검증용 파일은 Client/Bin/Resource/Sound/LoL/Champions/Irelia/irelia_base_sfx_26_54676396.wav에 존재한다. 이는 44.1 kHz/16-bit PCM WAV다. BGM도 같은 Sound_Manager 캐시를 쓰므로, 현재 트리에서는 BGM 호출자가 없어도 동일한 root 오류 때문에 재생될 수 없다. 소스 전체에서 현재 활성 PlayBGM 호출이나 과거 주석의 Star BGM 호출은 찾지 못했다. 따라서 과거 BGM 성공은 당시 실행 파일/리소스 배치 또는 임시 호출의 결과로만 설명할 수 있으며, 이 계획에서는 근거 없이 그 호출을 복구하지 않는다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Engine/Include/WintersPaths.h — Sound용 canonical 디렉터리 해석 API

기존 WintersResolveContentPath 선언(12행)을 아래 선언으로 교체한다. 기존 파일 해석 API는 유지하고, Sound Manager가 파일 전용 resolver를 우회해 Debug 폴더를 직접 조합하지 않도록 한다.

~~~cpp
// On success, fills outFullPath with a path suitable for D3DCompileFromFile.
WINTERS_ENGINE bool WintersResolveContentPath(
    const wchar_t* relativePath,
    wchar_t* outFullPath,
    uint32_t outCapacityChars);

// Resource 상대 디렉터리를 Client/Bin/Resource 아래의 실제 디렉터리로 해석한다.
// 예: L"Resource\\Sound\\" -> <Client>/Bin/Resource/Sound\\
WINTERS_ENGINE bool WintersResolveResourceDirectory(
    const wchar_t* relativeDirectory,
    wchar_t* outFullPath,
    uint32_t outCapacityChars);
~~~

Engine 공개 헤더가 바뀌므로 구현 후 EngineSDK/inc를 직접 편집하지 않는다. Engine 빌드 성공 뒤 기존 UpdateLib.bat 절차로 생성 mirror를 갱신하는지 검증한다.

### 1-2. C:/Users/user/Desktop/Winters/Engine/Private/Core/WintersPaths.cpp — 기존 Resource root 탐색 재사용

기존 TryResolveCanonicalResourcePath(171행) 바로 아래에 파일 존재 대신 DirectoryExists를 사용하는 private TryResolveCanonicalResourceDirectory를 추가한다. TryGetResourceSubPath, TryFindCanonicalResourceRoot의 현재 상향 탐색 규칙을 그대로 쓴다. 아래 public 함수는 WintersResolveContentPath(198행) 함수 끝 뒤에 추가한다.

~~~cpp
WINTERS_ENGINE bool WintersResolveResourceDirectory(
    const wchar_t* relativeDirectory,
    wchar_t* outFullPath,
    uint32_t outCapacityChars)
{
    if (!relativeDirectory || !outFullPath || outCapacityChars < MAX_PATH)
        return false;

    std::wstring resourceSubPath;
    if (!TryGetResourceSubPath(NormalizeSlashes(relativeDirectory), resourceSubPath))
        return false;

    wchar_t exePath[MAX_PATH] = {};
    const DWORD exeLength = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (exeLength > 0 && exeLength < MAX_PATH &&
        TryResolveCanonicalResourceDirectory(exePath, resourceSubPath, outFullPath, outCapacityChars))
    {
        return true;
    }

    wchar_t cwd[MAX_PATH] = {};
    const DWORD cwdLength = GetCurrentDirectoryW(MAX_PATH, cwd);
    return cwdLength > 0 && cwdLength < MAX_PATH &&
        TryResolveCanonicalResourceDirectory(cwd, resourceSubPath, outFullPath, outCapacityChars);
}
~~~

위 코드의 NormalizeSlashes 이름은 구현 시 현재 WintersPaths.cpp의 실제 helper 이름에 맞춰 사용한다. 새 helper는 Resource/Sound 자체가 존재하는지 확인하고, 성공 경로 끝에 slash를 하나만 정규화한다. 상대 경로·현재 작업 폴더를 새 독자 규칙으로 만들지 않는다.

### 1-3. C:/Users/user/Desktop/Winters/Engine/Public/Sound/Sound_Manager.h — 로드 결과 계수 전달

기존 LoadSoundFolderRecursive 선언(53~54행)을 아래로 교체한다. 신규 파일을 만들지 않고 현재 manager에만 관측 계수를 둔다.

~~~cpp
void LoadSoundFolder();
void LoadSoundFolderRecursive(
    const wstring_t& strFolderPath,
    const wstring_t& strRelativePath,
    u32_t& uScannedFileCount,
    u32_t& uLoadedFileCount,
    u32_t& uFailedFileCount);
~~~

### 1-4. C:/Users/user/Desktop/Winters/Engine/Private/Sound/Sound_Manager.cpp — 실제 원인 수정 및 FMOD 관측성

기존 LoadSoundFolder(130~143행)의 GetModuleFileNameW 기반 직접 조합 전체를 아래 동작으로 교체한다.

~~~cpp
void CSound_Manager::LoadSoundFolder()
{
    wchar_t soundRoot[MAX_PATH] = {};
    if (!WintersResolveResourceDirectory(
            L"Resource\\Sound\\", soundRoot, _countof(soundRoot)))
    {
        OutputDebugStringW(
            L"[Sound] Resource/Sound root not found; cache remains empty.\\n");
        return;
    }

    u32_t uScannedFileCount = 0;
    u32_t uLoadedFileCount = 0;
    u32_t uFailedFileCount = 0;
    LoadSoundFolderRecursive(
        soundRoot, L"", uScannedFileCount, uLoadedFileCount, uFailedFileCount);

    // root와 세 count를 한 줄로 OutputDebugStringW에 남긴다.
}
~~~

기존 LoadSoundFolderRecursive(146행 이하)는 아래 원칙으로 교체한다.

- FindFirstFileW 실패 시 root, 상대 경로, GetLastError를 한 번 기록한다. 더 이상 무음으로 빈 cache를 만들지 않는다.
- 디렉터리 재귀는 유지하되 key separator는 현재 JSON과 같은 slash로 정규화한다.
- 확장자가 .wav인 파일만 FMOD::System::createSound에 전달한다. 현재 Resource/Sound은 WAV 4,347개이며, 무관한 파일을 전부 FMOD에 넘기지 않는다.
- createSound 실패에는 FMOD_ErrorString(result), relative key를 기록하고 failed를 증가한다.
- 이미 같은 key가 있으면 새 FMOD::Sound를 release하고 duplicate를 기록한다. map overwrite/leak을 만들지 않는다.
- 성공 때만 loaded를 증가한다. 로그는 첫 32개 실패와 최종 요약으로 제한한다.

PlaySoundOn(56행), PlayEffect(73행), PlayBGM(84행)의 m_mapSounds.end() 즉시 return은 제거하지 말고, 각 key별 최초 1회만 누락 key를 Debug 로그에 남긴 후 return하게 바꾼다. PlayBGM은 새 BGM을 재생하기 전에 BGM channel을 stop하여 API 주석의 단일 BGM channel 규칙을 실제로 지킨다. playSound/setLoopCount/setVolume의 FMOD 결과도 실패 때만 로그로 남긴다.

이 변경은 FMOD 초기화 자체를 바꾸지 않는다. CGameInstance 초기화가 Sound Manager Initialize를 이미 호출한다는 점이 확인되었고, 문제는 그 뒤의 빈 sound map이다.

### 1-5. C:/Users/user/Desktop/Winters/Client/Public/GamePlay/ChampionSoundCatalog.h 및 Client/Private/GamePlay/ChampionSoundCatalog.cpp — JSON 실패를 성공으로 고정하지 않기

이 두 파일은 현재 워크트리의 이전 세션 산출물이다. 기존 API와 map 형식은 유지한다. Header의 m_bLoaded(49행) 바로 아래에 아래 상태를 추가한다.

~~~cpp
bool_t m_bLoaded = false;
bool_t m_bLoadAttempted = false;
~~~

LoadFromJson(95행)의 첫 부분에 있는 아래 코드를 삭제한다.

~~~cpp
// 실패해도 로드 시도는 1회로 고정 (Find 의 lazy-load 게이트)
m_bLoaded = true;
~~~

같은 위치를 아래의 실제 helper 호출 순서로 교체한다.

~~~cpp
m_bLoadAttempted = true;
m_bLoaded = false;
for (auto& slots : m_Keys)
{
    for (std::wstring& key : slots)
        key.clear();
}

wchar_t szResolvedPath[MAX_PATH] = {};
if (!pRelativePath ||
    !WintersResolveContentPath(pRelativePath, szResolvedPath, MAX_PATH))
{
    OutputDebugStringW(
        L"[ChampionSound] map resolve failed: Data/LoL/Sound/ChampionSoundMap.json\\n");
    return false;
}

if (!LoadFromResolvedPath(szResolvedPath))
{
    OutputDebugStringW(
        (L"[ChampionSound] map load failed: " + std::wstring(szResolvedPath) + L"\\n").c_str());
    return false;
}

m_bLoaded = true;
return true;
~~~

Find(121행)의 lazy 조건은 !m_bLoadAttempted로 바꾼다. 실패가 발생해도 cache가 정상 loaded인 것처럼 보이지 않고, F9/Debug에서 JSON 누락과 key 누락을 분리할 수 있다. 런타임 중 무한 재시도는 하지 않는다.

### 1-6. C:/Users/user/Desktop/Winters/Data/LoL/Sound/ChampionSoundMap.json — Irelia 검증 WAV 여섯 슬롯 통일

Irelia 객체의 기존 sounds 블록(61~68행)을 정확히 아래로 교체한다. 파일을 복사하거나 Debug/Resource에 중복 배치하지 않는다.

~~~json
"sounds": {
  "basicAttack": "LoL/Champions/Irelia/irelia_base_sfx_26_54676396.wav",
  "skillQ": "LoL/Champions/Irelia/irelia_base_sfx_26_54676396.wav",
  "skillW": "LoL/Champions/Irelia/irelia_base_sfx_26_54676396.wav",
  "skillE": "LoL/Champions/Irelia/irelia_base_sfx_26_54676396.wav",
  "skillR": "LoL/Champions/Irelia/irelia_base_sfx_26_54676396.wav",
  "death": "LoL/Champions/Irelia/irelia_base_sfx_26_54676396.wav"
}
~~~

검증이 끝난 뒤에는 각 슬롯을 의도한 개별 WAV로 되돌릴 별도 data 작업으로 취급한다. 이번 변경은 Sound Manager path, catalog, BA/Q/W/E/R/death 재생 게이트를 한 번에 검증하기 위한 임시 probe이며 밸런스/연출 최종 데이터가 아니다.

### 1-7. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameNetwork.cpp — 사운드 호출 위치는 유지하고 중복 삽입 금지

수정 대상은 새 PlayEffect 호출이 아니라 기존 중앙 훅의 계측이다.

- PlayNetworkChampionSound(198~219행): catalog key, champion, slot, sequence를 최초 실패 시만 Debug 로그로 남긴다.
- PlayNetworkChampionActionSound(221~243행): BasicAttack/Q/W/E/R의 mapping을 유지한다.
- UpdateNetworkChampionLocomotion의 death gate(1088~1117행)와 action sequence gate(1144~1148행): 기존의 첫 snapshot 제외 및 이전 sequence 회귀 중복 방지를 유지한다.

IreliaGameSim.cpp, Irelia_Skills.cpp, projectile/effect cue callback에는 PlayEffect를 추가하지 않는다. 서버는 action을 승인·복제하고 클라이언트는 한 번만 presentation을 수행한다는 Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual 경계를 지킨다.

### 1-8. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.cpp 및 Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.cpp — 가중치가 무력화되는 PlayerLike hard gate 완화

ChampionAIBrain.cpp의 PlayerLike 분기에서 다음의 현재 정책을 교체한다.

~~~cpp
// 현재 문제: retreat score 0.55 이상이면 후퇴,
// self HP가 enemy HP보다 작으면 fight를 완전히 배제,
// Farm intent hold 동안 적 챔피언 진입도 지연.
~~~

교체 정책은 아래와 같다.

~~~cpp
// namespace scope, CPlayerLikeChampionBrain 앞에 둔다.
constexpr f32_t kPlayerLikeRetreatScoreThreshold = 0.65f;
constexpr f32_t kPlayerLikeHpDisadvantageTolerance = 0.12f;

if (input.fRetreatScore >= kPlayerLikeRetreatScoreThreshold)
{
    ai.intentHoldTimer = ai.intentHoldDuration * kCommitScale;
    return eChampionAIIntent::Retreat;
}

if (ai.intentHoldTimer > 0.f)
{
    if ((ai.intent == eChampionAIIntent::AttackChampion &&
            input.bCanAttackChampion) ||
        (ai.intent == eChampionAIIntent::SiegeStructure &&
            input.bCanAttackStructure) ||
        (ai.intent == eChampionAIIntent::Retreat &&
            input.fRetreatScore >= 0.30f) ||
        (ai.intent == eChampionAIIntent::FarmMinion &&
            !input.bCanAttackChampion &&
            !input.bCanAttackStructure))
    {
        return ai.intent;
    }
}

ai.intentHoldTimer = ai.intentHoldDuration * kCommitScale;
const bool_t bCombatHpAcceptable =
    ai.fDecisionSelfHpRatio + kPlayerLikeHpDisadvantageTolerance >=
    ai.fDecisionEnemyHpRatio;
if (input.bCanAttackChampion && bCombatHpAcceptable &&
    input.fChampionScore >= input.fFarmScore + ai.fChampionScoreMargin &&
    input.fChampionScore >= input.fStructureScore)
{
    return eChampionAIIntent::AttackChampion;
}
~~~

위 코드는 현재 ChampionAIBrainInput의 실제 fields와 ChampionAIComponent의 fDecisionSelfHpRatio/fDecisionEnemyHpRatio를 사용한다. 핵심은 hard HP equality를 12% 허용 오차로 바꾸고, 기존 turret safety/CanAttackChampion은 제거하지 않는 것이다.

ChampionAIPolicy.cpp의 MakeJaxProfile 반환 initializer에서는 아래 네 값만 시범 기준으로 바꾼다. 다른 챔피언 profile에 일괄 전파하지 않는다.

~~~cpp
aggression = 1.35f;            // 기존 1.25f
retreatHpRatio = 0.35f;        // 기존 0.50f
reengageHpRatio = 0.55f;       // 기존 0.65f
minionPressureWeight = 0.70f;  // 기존 0.80f
// turretRiskWeight = 0.90f, siegeWeight = 1.05f, lastHitWeight = 1.00f 유지
~~~

이는 공격성 trace와 사망률을 보며 조정할 출발값이다. “더 공격적”을 위해 turret 위험 제한을 풀지 않는다.

### 1-9. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h, Shared/GameSim/Systems/ChampionAISystem.cpp, Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp 및 Server/Private/Game/GameRoomCommands.cpp — 런타임 가중치 조정 경로 완성

현재 eChampionAITuningId와 ChampionAITuning에는 판단 interval·거리·threshold 위주만 있고 aggression/farm/turret/siege profile weight를 직접 조정할 slot이 없다. 다음 네 tuning id와 0~3 범위의 multiplier를 기존 enum/구조체 끝에 추가한다.

~~~cpp
FightUtilityMultiplier,
FarmUtilityMultiplier,
TurretRiskMultiplier,
SiegeUtilityMultiplier,
~~~

기본값은 모두 1.f다. ChampionAISystem의 ApplyChampionAIProfileAndTuning에서 아래처럼 profile 값에 곱한다.

~~~cpp
vin.fightUtilityWeight =
    profile.aggression * ai.tuning.fFightUtilityMultiplier;
vin.farmUtilityWeight =
    profile.minionPressureWeight * profile.lastHitWeight *
    ai.tuning.fFarmUtilityMultiplier;
vin.turretRiskWeight =
    profile.turretRiskWeight * ai.tuning.fTurretRiskMultiplier;
vin.siegeUtilityWeight =
    profile.siegeWeight * ai.tuning.fSiegeUtilityMultiplier;
~~~

CommandExecutor.cpp의 ResolveChampionAITuningParamById와 GameRoomCommands.cpp의 같은 parameter resolver에 네 enum을 모두 매핑한다. component 크기/offset을 검사하는 static_assert와 checkpoint 직렬화가 있다면 동일 변경을 함께 갱신한다. Client F9 AI debug panel은 기존 tuning command가 enum을 label로 표시하는지 먼저 확인하고, 누락이면 네 label/slider만 추가한다. 이 항목은 서버 authoritative debug command를 이용하며 client가 AI 진실을 바꾸지 않는다.

### 1-10. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAISystem.cpp 및 Shared/GameSim/Components/ChampionAIComponent.h — Farm, 구조물 공격, Jax 미드 방어의 정확한 상태 전이

EmitBasicAttackCommand의 현재 raw attackRange 사전 거절 블록을 삭제한다.

~~~cpp
// 삭제할 코드: target과의 center distance가 raw attackRange보다 크면
// false를 반환하여 AI가 구조물/미니언 중심으로 MoveToTarget을 보내는 분기.
~~~

대신 살아 있고 적대적이며 targetable인지만 검증한 뒤 BasicAttack GameCommand를 보낸다. CommandExecutor::HandleBasicAttack은 이미 issuer/target radius를 포함한 effective range를 계산하고, 범위 밖이면 StartAttackChase를 시작한다. 이 경로의 arriveRadius는 공격 가능한 standoff 거리이므로, Farm과 TryExecuteStructureAttack이 포탑/억제기 중심까지 걸어가려는 현재 원인을 없앤다.

FindEnemyMinion과 TryExecuteMinionFarm은 현재 low HP + 거리 점수로 target을 고르지만, 실제 impact 시점의 last-hit 예측은 하지 않는다. 구현 시 다음 순서로 추가한다.

1. 현재 combat/attack timing API를 확인하여 ChampionAIContext에 자신의 기본 공격 windup·projectile impact time과 대상의 현재 Health를 넣는다.
2. ChampionAISystem.cpp의 minion 후보 점수 직전에 EstimateMinionHealthAtImpact를 추가한다. 예상 HP가 자신의 한 번의 기본 공격 유효 피해 범위일 때 큰 보너스를 주고, 도착 전에 죽을 HP이면 후보에서 제외한다.
3. Damage 계산은 client 추정이나 random을 쓰지 않고 Shared GameSim의 armor/AD/attack timing 값을 사용한다. 기존에 incoming minion damage 예측 API가 없다면 이 부분은 CONFIRM_NEEDED로 남기고, 단순 maxHp 비율 heuristic을 새 authoritative 규칙으로 만들지 않는다.

GroupMidDefense 진입 코드(HasAlliedOuterTurretLost/HasMidLaneTurretLost 호출부)도 아래 원칙으로 교체한다.

~~~cpp
// 기존: 어느 팀의 mid turret 손실도 mid defense trigger가 될 수 있음.
// 변경: self team의 mid turret/outer turret 손실만 true로 판단.
const bool_t bOwnMidDefenseNeed =
    HasAlliedOuterTurretLost(world, selfTeam) ||
    HasMidLaneTurretLost(world, selfTeam);
~~~

ChampionAIComponent에 mid defense의 무위협 누적 시간을 추가하고, ExecuteGroupMidDefense에서 아래 종료 조건을 둔다.

~~~cpp
if (!bEnemyChampionInMidDefenseZone &&
    !bEnemyStructureThreatInMidDefenseZone &&
    !bActiveCombatAction)
{
    ai.fMidDefenseNoThreatSec += dt;
}
else
{
    ai.fMidDefenseNoThreatSec = 0.f;
}

if (ai.fMidDefenseNoThreatSec >= 6.f)
{
    ai.bMidDefenseActive = false;
    ai.activeLane = ai.assignedLane;
    ai.eState = eChampionAIState::LaneCombat;
    ai.eIntent = eChampionAIIntent::FarmMinion;
    ai.fMidDefenseNoThreatSec = 0.f;
}
~~~

실제 member naming은 기존 component 명명 규칙에 맞춘다. 새 member는 checkpoint ABI/static_assert에 반영한다. 이 종료가 있어야 아군 Jax가 한 번의 미드 방어 이후 무기한 미드 anchor에 남지 않는다.

ResolveMidDefenseAnchor의 현재 kChampionAIMidDefenseFormationSpacing=1.25f는 챔피언 반경 0.75f 두 개의 합(1.5f)보다 작다. 아래 조건을 사용해 최소 1.75f로 만든다.

~~~cpp
const f32_t fFormationSpacing =
    std::max(1.75f, fSelfRadius * 2.f + 0.25f);
~~~

slot index는 EntityID 기반으로 유지해 결정성을 보존한다. 새 anchor가 walkable하지 않으면 구조물 중심 방향으로 clamp하지 말고, 새 반경 인식 path query의 가장 가까운 reachable candidate를 선택한다.

### 1-11. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/MoveTargetComponent.h 및 Shared/GameSim/Systems/MoveSystem.cpp — clamp 실패를 recall 가능한 repath 상태로 보존하고 챔피언 soft separation 추가

MoveTargetComponent의 blockedMoveTicks/bestMoveDistance 인접 영역에 아래 flag를 추가하고, component layout 검사와 checkpoint 저장/복원을 갱신한다.

~~~cpp
bool_t bNeedsRepath = false;
~~~

MoveSystem.cpp에서 TryClampMoveSegment 실패 또는 부분 clamp 처리의 현재 ClearMoveRuntimeTarget 호출을 아래 상태 전이로 바꾼다.

~~~cpp
moveTarget.bNeedsRepath = true;
RecordBlockedMoveTick(moveTarget, currentPosition, targetPosition, dt);
// target, blockedMoveTicks, bestMoveDistance는 지우지 않는다.
~~~

다음 tick에는 같은 target으로 path를 한 번 재생성한다. 재생성도 막히면 target을 계속 지우지 않고 blocked time을 누적한다. Retreat state가 3초에 도달할 때만 ChampionAISystem의 기존 EmitRecall fallback이 작동한다. Farm/Follow/Defend는 제한된 재시도 뒤 intent를 재평가하여 새 target을 고르게 하고, 일반 이동이 영구 busy loop가 되지 않게 한다.

MoveSystem의 IsMoveBlockingKind/CollectMoveBlockers는 NeutralUnit만 hard blocker로 취급하는 현재 규칙을 다음처럼 분리한다.

~~~cpp
// Structure/Object/Core/NeutralUnit: navigation/clamp의 hard obstacle.
// Character: 이동을 완전히 금지하지 않는 soft-separation 대상.
~~~

이동 적용 뒤 겹친 Character pair에 대해 EntityID가 작은 쪽부터 한 번씩만 계산하고, 각자 반경 합 + 0.05f까지의 부족분을 반씩 벌린다. 보정 segment도 TryClampMoveSegment와 반경 인식 walkability를 통과해야 한다. 상대가 더 낮은 EntityID인 경우 반대 보정을 하지 않아 순서·replay가 달라지지 않게 한다. dead/recalling/non-spatial entity는 제외한다. 챔피언을 hard blocker로 단순 추가하면 좁은 lane에서 서로 영구 정지하므로 사용하지 않는다.

Debug 빌드에는 entity, intent/state, agent radius, from/to, path index/count, clamp reason, blocked seconds를 1초당 entity별 한 줄 이하로 OutputDebugStringA/W 또는 기존 Server AI trace에 기록한다.

### 1-12. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h, Server/Public/Game/GameRoom.h, Server/Private/Game/GameRoomNav.cpp 및 Shared/GameSim/Systems/WalkabilityAuthority.* — 챔피언 반경을 경로 전체에 전달

IWalkableQuery::TryBuildMovePath의 기존 선언은 mover radius가 없다. 마지막 parameter로 아래를 추가하고 모든 override/caller를 같은 커밋에서 갱신한다.

~~~cpp
f32_t fAgentRadiusWorld
~~~

MoveSystem, CommandExecutor의 StartAttackChase, ChampionAISystem, Jungle/Minion AI의 caller는 ResolveAgentRadius 결과를 전달한다. 미니언은 기존 0.5f, 챔피언은 SpawnObject/Champion 정의의 0.75f를 전달한다.

GameRoomNav.cpp의 BuildServerPathNavGrid와 CGameRoom::TryBuildMovePath는 단일 0.5f path grid를 모든 agent에게 재사용하지 않게 바꾼다. terrain base와 구조물 carve 결과에서 minion(0.5f)과 champion(0.75f) clearance grid를 분리 생성하고, fAgentRadiusWorld에 따라 선택한다. radius가 그 범위를 벗어나는 future agent는 raw nav grid에서 같은 반경의 dynamic clearance 검사로 fallback한다. WalkabilityAuthority의 direct segment, smoothing, closest-reachable point도 동일 fAgentRadiusWorld로 검사하여 “path는 성공했지만 MoveSystem clamp는 실패”가 일어나지 않게 한다.

Server의 구조물 obstacle은 Spawn 때만 carve하고 영구히 남기지 않는다. CarveServerStructuresOnNavGrid는 terrain base를 훼손하지 않은 clone에 살아 있는 Structure/Health entity만 carve하도록 하고, 포탑·억제기 사망/비활성 이벤트에서 base -> carve -> 두 clearance grid -> lane flow field -> RefreshChampionAIGoals 순으로 재구성한다. 정확한 death event owner는 구현 직전에 Structure health/death emission을 확인하여 연결한다(CONFIRM_NEEDED). 매 tick 전체 grid를 재구성하는 방식은 사용하지 않는다.

### 1-13. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomChampionAI.cpp — 구조물 뒤 safe anchor의 여유 확보

기존 kChampionAISafeAnchorBehindTurret=3.f 사용 위치에서 fixed distance만 쓰지 말고, 구조물 radius + champion radius + 0.50f 이상의 여유를 계산한다.

~~~cpp
const f32_t fRequiredBehindDistance =
    fStructureRadius + fChampionRadius + 0.50f;
const f32_t fBehindDistance =
    std::max(kChampionAISafeAnchorBehindTurret, fRequiredBehindDistance);
~~~

후보 anchor는 1-12의 radius-aware query로 reachable한 위치에 보정한다. 포탑 반경 1.5f, 억제기 1.8f, nexus turret 1.8f, nexus 2.8f라는 현재 Spawn/Structure radius와 다른 magic number를 새로 만들지 않는다.

### 1-14. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp — gameplay definition hot reload의 neutral stat drift 차단

ReloadGameplayDefinitions(2396~2433행)는 현재 모든 StatComponent를 dirty 처리한다. Jungle monster에는 championId=NONE이므로 champion stat 재계산 fallback이 적용되면 SpawnObject 데이터의 Health/Jungle/Stat 값이 서로 달라질 수 있다. 아래 필터를 dirty mark 앞에 둔다.

~~~cpp
if (pJungleMonsterTag != nullptr ||
    pStat->championId == eChampion::NONE)
{
    return;
}
pStat->bDirty = true;
~~~

실제 ForEach lambda에서 return이 lambda continue인지 확인하고, 아니라면 continue 가능한 구조로 바꾼다. 이번 정의 변경은 새 room/restart에서 neutral에게 적용한다. live reload에서 중립몹 수치를 즉시 바꾸는 별도 기능은 Health/Jungle/Stat을 원자적으로 모두 갱신하는 설계가 확보되기 전에는 추가하지 않는다.

### 1-15. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json 및 Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp — 일반 정글몹 base stats

기본 minion은 maxHp 225, attackDamage 40이다. defaultJungleCamp 및 subKind 4~10의 maxHp/attackDamage만 아래 표처럼 바꾼다. subKind 0~3(Baron/Dragon/Blue/Red)은 절대 수정하지 않는다.

| 대상 | subKind | maxHp | attackDamage |
| --- | ---: | ---: | ---: |
| Krug | 4 | 450 | 40 |
| Gromp | 5 | 450 | 40 |
| Wolf | 6 | 450 | 40 |
| Raptor | 7 | 450 | 40 |
| Krug mini | 8 | 450 | 40 |
| Wolf mini | 9 | 450 | 40 |
| Raptor mini | 10 | 450 | 40 |
| defaultJungleCamp fallback | 해당 없음 | 450 | 40 |

JSON 편집 뒤 아래 generator를 실행해 generated cpp를 갱신한다. generated cpp를 손으로 고치지 않는다.

~~~powershell
python Tools/LoLData/Build-LoLDefinitionPack.py
~~~

이 값은 “기본 minion의 현 시점 base HP의 두 배”다. minion time-scaling까지 정확히 두 배로 따라가야 한다는 요구는 현재 jungle에 없는 별도 scaling 정책이므로, 이번 범위에는 임의로 넣지 않는다. 필요하면 다음 계획에서 성장 curve를 공통 data definition으로 분리한다.

### 1-16. C:/Users/user/Desktop/Winters/Client/Private/Manager/Jungle_Manager.cpp 및 Client/Private/Scene/Scene_InGame.cpp — snapshot fallback과 체력바 폭 정합

Jungle_Manager.cpp의 regular jungle local fallback(526~604행)은 현재 1500 HP 계열 값을 중복 보유한다. online snapshot이 도착하면 덮어쓰더라도 stage bind 이전/실패 시 오래된 값이 보일 수 있다. 이 fallback은 SpawnObject definition pack의 동일 subKind stat을 읽도록 바꾸고, client 상수로 450을 또 복제하지 않는다. Epic 0~3 fallback은 기존 값을 유지한다.

Scene_InGame.cpp의 SyncWorldHealthBarsToEngineUI에서 아래 두 줄을 삭제한다.

~~~cpp
if (!bEpic)
    Bar.fWidthScale = 3.f;
~~~

non-epic jungle은 Unit health bar의 기본 scale 1.f를 사용한다. Engine/Private/Manager/UI/UI_Manager.cpp가 Unit 기본 폭 약 43.088px에 scale을 곱하므로, 이 변경으로 현재 129.264px(3배) 폭만 정상화된다. Jungle.hp/maxHp 비율 계산, Epic의 Character bar 분류, Dragon/Blue/Red/Baron 표시는 건드리지 않는다.

### 1-17. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp — 회귀 probe를 기존 headless harness에 추가

새 test project/file을 만들지 않고 existing SimLab main의 scenario registration 아래에 다음 probe를 추가한다.

1. subKind 4~10 spawn 직후 Health.current/max, Jungle.hp/maxHp가 모두 450이고 Stat.baseAttackDamage/attackDamage가 모두 40인지 assert한다.
2. subKind 0~3의 현재 값이 표의 변경 전 값과 동일한지 assert한다.
3. ReloadGameplayDefinitions 이후 neutral의 Health/Jungle/Stat 불일치가 없고 champion만 stat dirty 처리되는지 assert한다.
4. 5v5 30분 seed replay에서 champion pair separation 후 radius sum + 0.05f 미만 overlap이 지속되지 않는지, structure edge에서 3초 이상 same clamp reason이 지속되면 trace가 남는지 assert한다.
5. retreat path를 의도적으로 막고 3초 뒤 Recall command가 한 번만 발행되는지, normal Farm path는 recall이 아닌 repath/intent refresh가 되는지 assert한다.
6. Jax와 최소 한 lane bot에 PlayerLike profile을 부여해 fight/farm/retreat score, selected intent, mid-defense enter/exit, lane restore가 trace에 남는지 assert한다.

SimLab에는 client UI/FMOD가 없으므로 sound와 bar pixel 폭은 manual client 검증으로 남긴다. SimLab 통과만으로 F5 정상 동작을 주장하지 않는다.

## 2. 검증

이번 세션에서는 아래 명령을 실행하지 않는다. 사용자 클라이언트/서버가 종료되고 빌드 권한을 받은 뒤에 순서대로 실행한다.

### 2-1. 정적·data 검증

~~~powershell
git diff --check
python Tools/LoLData/Build-LoLDefinitionPack.py --check
~~~

- JSON의 Irelia 여섯 key가 정확히 LoL/Champions/Irelia/irelia_base_sfx_26_54676396.wav인지 확인한다.
- 실제 파일이 Client/Bin/Resource/Sound/LoL/Champions/Irelia/irelia_base_sfx_26_54676396.wav에 한 개 존재하는지 확인한다.
- Client/Bin/Debug/Resource 또는 Client/Bin/Release/Resource 복사본을 만들지 않는다. runtime resource root는 Client/Bin/Resource 하나다.
- generated definition diff에는 일반 jungle 4~10/default만 나타나고 0~3은 나타나지 않아야 한다.

### 2-2. 빌드·SDK mirror 검증

Engine 공개 header 변경이 포함되므로 Engine target을 먼저 빌드하고 기존 프로젝트의 UpdateLib.bat를 실행해 EngineSDK mirror를 갱신한다. 그 다음 Debug x64의 Engine, Server, Client, SimLab target을 현재 솔루션/구성의 표준 command로 빌드한다. 경고를 무시하지 않으며, public API signature를 바꾼 뒤 EngineSDK/inc를 직접 편집하지 않는다.

### 2-3. Sound runtime 검증

1. Client 시작 로그에서 Sound root가 Client/Bin/Resource/Sound로 해석되고 scanned/loaded/failed count가 출력되는지 본다. loaded는 0보다 커야 하고 target WAV key가 cache에 있어야 한다.
2. 일회성 debug probe로 CGameInstance::PlayBGM(L"BGM/TerranBGM1.wav", volume)을 호출해 같은 cache root에서 BGM이 들리는지 확인한다. 새 BGM 호출이 이전 BGM channel을 교체하는지도 확인한다. 확인 뒤 probe는 제거하며 normal F5 흐름에 Star BGM을 하드코딩하지 않는다.
3. 5v5 normal F5에서 Irelia의 BA, Q, W, E, R, Death를 각 한 번씩 수행한다. 여섯 경우 모두 같은 target WAV가 한 번만 들려야 한다.
4. R의 복수 effect cue, snapshot 첫 관측, 이전 sequence 회귀, 재접속/late join에서 중복 재생이 없어야 한다. Server action 승인 로그만으로 sound 통과를 판정하지 않고 client cache-hit와 FMOD 성공 로그를 함께 본다.
5. JSON parse 실패, sound key 누락, FMOD create/play 실패는 각 key별 제한 로그로 즉시 구분되어야 한다.

### 2-4. AI·Farm·Recall·구조물 이동 검증

1. 기본 난이도 2 PlayerLike Jax와 다른 lane bot을 포함한 normal 5v5를 실행한다. F9 AI debug/Server trace에서 fightScore, farmScore, retreatScore, intent, lane, target NetId, move blocked seconds를 캡처한다.
2. Jax가 적 챔피언과 HP 차이 12% 이내이고 turret 위험이 없는 경우 Farm hold만으로 공격 기회를 1.2초 놓치지 않는지 확인한다. 반대로 low HP/turret 위험에서는 Retreat가 유지되는지 확인한다.
3. outer/mid turret 손실 후 GroupMidDefense가 자기 팀 손실에만 진입하고, mid threat가 6초 없어지면 assigned lane으로 돌아가 FarmMinion 상태가 되는지 확인한다. NetId-to-champion mapping도 캡처하여 “Jax”임을 추측하지 않는다.
4. 일반 미니언, 포탑, 억제기 공격에서 AI가 target center MoveToTarget 대신 HandleBasicAttack -> StartAttackChase의 effective range standoff로 서는지 확인한다.
5. 아군 챔피언 2~5명이 같은 lane/미드 방어 slot으로 모일 때 persistent overlap, reciprocal push, deadlock이 없어야 한다. spacing은 최소 1.75f이고 separation은 deterministic replay에서 동일해야 한다.
6. 포탑·억제기 사방을 walking/attack chase/retreat로 통과한다. champion radius 0.75f 경로가 clamp failure 없이 생성되는지, destroyed structure 후 nav grid가 재구성되어 옛 obstacle이 남지 않는지 확인한다.
7. 포탑 모서리에서 retreat를 의도적으로 막아 3초 후 recall이 정확히 한 번 발행되는지 확인한다. 일반 farm 이동에는 recall이 발행되지 않고 bounded repath 후 intent가 갱신되는지 확인한다.

### 2-5. 정글몹·체력바 검증

1. 새 server room에서 Krug/Gromp/Wolf/Raptor 및 mini subKind 4~10을 각각 spawn한다. server debug/snapshot에서 maxHp=450, hp=450, baseAD=40, AD=40을 확인한다.
2. Baron/Dragon/Blue/Red가 기존 HP/AD 그대로인지 함께 확인한다.
3. gameplay definition reload를 수행한 경우 neutral Health/Jungle/Stat 세 값이 서로 불일치하지 않는지 확인한다. 즉시 stat 변경이 필요하면 room restart가 요구된다는 것을 UI/운영 절차에 명시한다.
4. client health bar는 일반 정글몹이 minion과 같은 Unit 기본 폭으로 보이고, fill은 450 HP 비율로만 변해야 한다. Epic 네 종류의 Character bar 및 폭은 이전과 같아야 한다.

완료 판정은 “빌드 성공”이 아니라 위의 sound 6 action, Jax lane restore, recall fallback, structure-edge 이동, 7종 일반 jungle stats, health bar 폭의 실측 증거가 모두 남는 것이다. 정상 F5 roster, map, minion, snapshot, champion, UI, FX를 숨겨 수치를 만들지 않는다.
