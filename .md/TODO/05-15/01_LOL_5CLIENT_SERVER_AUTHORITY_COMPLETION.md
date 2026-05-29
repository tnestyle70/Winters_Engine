# LoL 5-Client Server Authority Completion

Session - LoL 5-client server authority flow에서 이동, 평타, 스킬, death/respawn, FX cue single-source를 완성한다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp

목표:
- client input이 `GameCommand`로 들어온 뒤 server GameSim만 gameplay truth를 변경한다.

반영:
- Move, basic attack, cast skill, level skill command의 validation 결과를 하나의 server acceptance 흐름으로 정리한다.
- cooldown, range, mana, target validity, dead state validation을 command 처리 앞단에서 통일한다.
- client-only champion hook이 HP, position, cooldown, damage를 영구 변경하지 않도록 authoritative command path를 우선한다.

확인 필요:
- 기존 champion별 `OnCastAccepted` / `OnCastFrame` 중 gameplay 결과를 바꾸는 코드가 server/shared 쪽으로 모두 이동했는지 확인.

### 1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/AttackChaseSystem.cpp

목표:
- right-click attack chase와 basic attack이 server에서 target invalidation, range, attack timer를 판단한다.

반영:
- target death, respawn, invisibility/fog future hook을 고려해 stale target을 매 tick 검증한다.
- basic attack cast frame과 damage frame을 server event/cue로 발행한다.
- projectile/basic attack 결과는 `DamageQueueSystem`과 `DamagePipeline`을 통해 처리한다.

### 1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/DeathSystem.cpp

목표:
- champion, minion, turret projectile stale target, respawn을 server에서 단일 처리한다.

반영:
- death state 진입 시 move/attack/skill command가 거절되도록 state flag를 통일한다.
- respawn tick, spawn position, health/mana reset을 server GameSim에 둔다.
- respawn snapshot과 event가 client visual reset을 유도하도록 event schema 사용 여부를 점검한다.

### 1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ReplicatedEventSerializer.cpp

목표:
- damage, skill cast, death, respawn, FX cue가 snapshot과 별도의 event stream으로 중복 없이 전달된다.

반영:
- FX playback은 server event/cue에서 한 번만 발생하게 한다.
- client local prediction에서 재생한 임시 FX와 server cue FX가 중복되지 않도록 actionSeq 또는 cue id를 비교한다.
- event sequence 정렬은 `acceptedTick`, `sessionId`, `sequenceNum` 기준의 안정 정렬 원칙을 따른다.

### 1-5. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

목표:
- 5-client normal flow에서 lobby -> loading -> in-game -> result loop가 유지된다.

반영:
- `Phase_DrainCommands`, `Phase_ExecuteCommands`, `Phase_ServerBotAI`, `Phase_SimulationSystems`, `Phase_ServerDeathAndRespawn`, `Phase_BroadcastEvents`, `Phase_BroadcastSnapshot` 순서를 유지한다.
- bot AI는 component 직접 mutate가 아니라 `m_pendingExecCommands`를 통해 command executor로 흘린다.
- 5-client smoke에서 session leave, reconnect future hook, bot fill default를 normal F5 path와 분리하지 않는다.

### 1-6. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

목표:
- client visual state는 server snapshot을 기준으로 보정된다.

반영:
- movement interpolation, actionSeq animation, HP/mana/cooldown, inventory/stat snapshot 적용을 분리한다.
- local prediction이 server snapshot과 충돌할 때 server value를 최종 진실로 둔다.
- dead/respawn state flag가 visual, UI, input gate에 반영되는지 확인한다.

### 1-7. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

목표:
- server event/cue가 client visual hook과 FX renderer를 한 번만 호출한다.

반영:
- `CVisualHookRegistry`는 presentation만 담당한다.
- legacy `CSkillHookRegistry`가 authoritative server event를 다시 gameplay hook으로 되돌리지 않도록 경로를 분리한다.
- damage number, hit spark, skill cast FX, death/respawn FX의 cue id와 actionSeq를 로그로 확인한다.

### 1-8. C:/Users/user/Desktop/Winters/Client/Private/GamePlay/VisualHookRegistry.cpp

목표:
- champion visual hook 등록은 server cue 기반 presentation entry point로 정리된다.

반영:
- champion별 registration 파일에서 visual hook은 animation/FX/sound만 수행한다.
- gameplay mutation이 필요한 함수는 shared/server champion GameSim hook으로 옮긴다.
- FX cue 중복 재생 방지를 위해 cue tag, caster net id, actionSeq를 key로 하는 de-dup 검사를 추가한다.

## 2. 검증

검증 명령:
- `git diff --check`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`

런타임 검증:
- Server Debug x64 + Client Debug x64 5개 접속.
- 각 client에서 move, right-click attack, Q/W/E/R, death, respawn, reconnect 없이 3분 smoke.
- server log: command accept/reject, damage, death, respawn, event emission 확인.
- client log: snapshot apply, event apply, visual hook, FX render 확인.

합격 기준:
- client-only gameplay truth 변경 없이 server snapshot/event가 최종 상태를 만든다.
- 같은 skill FX가 local hook과 server cue에서 중복 재생되지 않는다.
- death 상태에서 move/attack/skill command가 server에서 거절된다.
