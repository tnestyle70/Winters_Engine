Session - 연출 값(visualYawOffset/animPlaySpeed/castFrame/recoveryFrame)을 ClientPublic visual DB로 분리해 reader를 거기로 돌리고, 권위 timing(action-lock/stage-window/windup ticks)은 ServerPrivate에 단일화한다. 이게 P8(ChampionGameDataDB 삭제)의 선행 blocker다.

배경(한 줄): 4개 연출 필드는 `ChampionVisualTimingSeed.json`으로 추출됐지만(parity 0) 아직 **읽는 클라 DB가 없고**, legacy reader가 gameplay 테이블/SkillDef에서 읽는다. 권위 timing(lockDurationSec=`ResolveSkillTiming` 177-196, action-lock ticks 212-216, stageWindowSec 228-236, windup 78-88/206-210 — 비연속)은 ServerPrivate 결정론 체인으로 유지한다. 규칙·게이트는 07.

검토 반영(2026-06-28) — 확정 정정:
- (★분류 정정) `Scene_InGameNetwork.cpp:262`는 `ChampionGameDataDB::ResolveSkillTiming(...).lockDurationSec` = **권위 action-lock 값**이지 연출이 아니다. 1-2의 연출 reader 목록에서 **제외**한다(이동 금지, ServerPrivate 유지).
- (확정) 새 visual pack을 새로 만들 필요 없음: `Client/Private/Data/LoLVisualDefinitionPack.h:34-36`에 이미 `castFrame/recoveryFrame/animationPlaybackSpeed` 필드가 있다. 1-1은 신규 pack이 아니라 이 기존 visual pack 위에 **reader 함수만 노출**하는 것으로 한다.
- (확정) yaw는 이미 `ClientData::ResolveChampionModelYawOffset`로 분리되어 `SnapshotApplier.cpp:718`이 호출 중 → yaw는 이번 slice 대상 아님. 남은 연출 reader는 castFrame/recoveryFrame/playbackSpeed뿐.
- (재확인) `ChampionVisualTimingSeed.json`이 generated visual pack 빌드 입력으로 실제 사용되는지 freshness로 확인.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Client/Private/Data/LoLVisualTimingPack.h + .cpp (새 파일) 또는 기존 LoLVisualDefinitionPack 확장

`Data/LoL/ClientPublic/Visual/Champion/ChampionVisualTimingSeed.json`을 읽는 ClientPublic 전용 read-only DB. champion별 modelYawOffset, slot/stage별 playbackSpeed/castFrame/recoveryFrame을 제공한다. generator가 seed -> generated pack으로 emit하는 기존 visual pack 경로(`Client/Private/Data/Generated/LoLVisualDefinitions.generated.cpp`)를 재사용한다.

```cpp
namespace ClientData
{
    f32_t ResolveChampionModelYawOffset(eChampion champion);     // 이미 SnapshotApplier가 호출(718)
    f32_t ResolveSkillVisualPlaybackSpeed(eChampion, u8_t slot, u8_t stage);
    f32_t ResolveSkillVisualCastFrame(eChampion, u8_t slot, u8_t stage);
    f32_t ResolveSkillVisualRecoveryFrame(eChampion, u8_t slot, u8_t stage);
}
```

확인 필요: `ClientData::ResolveChampionModelYawOffset`(SnapshotApplier.cpp:718에서 이미 호출됨)의 현재 구현이 무엇을 읽는지 확인. 이미 ClientPublic visual pack을 읽으면 yaw는 이번 slice에서 손댈 필요 없고, castFrame/recoveryFrame/playbackSpeed reader만 추가하면 된다.

1-2. 연출 reader 전환 (각 reader를 1-1 ClientData 경로로)

연구로 확인된 reader(파일:라인 작업 직전 재확인):

```text
- Client/Private/Scene/Scene_InGame.cpp:896,949-954  castFrame/recoveryFrame 프레임 이벤트 -> ClientData::ResolveSkillVisualCastFrame/RecoveryFrame
- Client/Private/Scene/Scene_InGameNetwork.cpp:262    skill timing(연출분) -> ClientData
- Client/Private/UI/SkillTimingPanel.cpp:30-31        튜너 UI -> ClientData(또는 dev override)
- Client/Private/GameObject/SkillTable.cpp            visualPlaySpeed 등 -> 최종 ClientData (SkillTable은 P8 삭제 대상)
- Client/Private/Data/Generated/LoLVisualDefinitions.generated.cpp  이미 visual pack (유지)
```

확인 필요: castFrame/recoveryFrame이 "연출 프레임 이벤트"로만 쓰이는지, 아니면 server 판정 hit 타이밍에 영향을 주는지 각 사이트에서 확인. 판정에 쓰이면 그것은 gameplay이므로 ServerPrivate에 남기고 visual로 옮기지 않는다.

1-3. 권위 timing은 그대로 ServerPrivate 유지 (이동 금지 명시)

```text
유지(ServerPrivate, ChampionGameDataDB.cpp):
- ResolveSkillTiming.lockDurationSec (177-196)
- ResolveSkillActionLockTicks (212-216)
- ResolveSkillStageWindowSec (228-236)
- ResolveBasicAttackWindupTicks (206-210), BuildBasicAttackTiming (78-88)
소비: MoveSystem.cpp:311, CommandExecutor.cpp:718/836/920/2408, ChampionAISystem.cpp:572
```

`ResolveSkillTiming`이 현재 `lockDurationSec`(권위)와 `animPlaySpeed`(연출)를 한 struct로 반환한다면, animPlaySpeed 필드를 권위 reader에서 떼어 1-1로 옮기고, 권위 경로는 lockDurationSec/ticks만 읽게 한다.

확인 필요: `ChampionSkillTimingDefaults`에 animPlaySpeed가 섞여 있는지(09 1-6에서 지적). 섞여 있으면 권위 struct에서 animPlaySpeed 제거 -> 연출 reader는 ClientData로, 권위 reader는 영향 없음.

1-4. ChampionGameDataDB visual 의존 제거 준비

위 전환이 끝나면 `ResolveVisualYawOffset` 등 visual resolver의 runtime reader가 0이 된다. 이는 P8(ChampionGameDataDB 삭제)의 선행 조건이다. 이 slice에서는 삭제하지 않고 reader 0 도달만 만든다.

2. 검증 (07 §6)

미검증:
- 클라 visual DB reader 미반영, 연출 reader 전환 미반영

검증 명령:
- python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check (visual seed freshness 포함)
- GameSim/Server/Client/SimLab Debug x64 빌드
- powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1

통과 기준:
- G2 Parity: client visual timing parity mismatchCount == 0 (seed == legacy 값).
- G4 SimLab same-seed 해시 불변: 권위 timing을 안 건드렸으므로 변하면 안 된다(변하면 연출/권위 경계를 잘못 그은 것).
- G3: ChampionGameDataDB visual resolver runtime reader가 감소 -> 0 목표.

확인 필요:
- I1: ClientPublic visual 값이 Server 권위 경로에 들어가지 않는지.
- castFrame/recoveryFrame이 hit 판정에 쓰이는 사이트가 있으면 그것은 gameplay -> 이동 금지(경계 오분류 방지).
