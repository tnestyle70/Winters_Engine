# 2026-07-15 인게임 점검 빌드 버그픽스 세션

인게임 점검(구조물 파괴 파이프라인 검증 빌드)에서 검출된 버그 일괄 수정 세션.

## 버그 목록

| # | 증상 | 영역 | 상태 |
|---|------|------|------|
| A | 칼리스타 창 FBX 메쉬가 흰색으로 렌더 | Client 렌더/FX | 수정 완료 |
| B | 칼리스타 평타/Q 스택 창이 적 중앙 기준 랜덤 방향으로 여러 개 쌓여야 하는데 1개만 표시 | Client 창 비주얼 | 수정 완료 |
| C | 칼리스타 E(Rend) 데미지 미적용 → 창 개수 비례 공식으로 수정 | Server GameSim | 수정 완료 |
| D | 전 챔피언 쿨다운 3초 / 궁극기 3초 통일 (봇 동일) | Server GameSim 튜닝 | 수정 완료 |

## 원인 분석

### A. 창 흰색 렌더
- 이번 빌드에서 칼리스타 창 비주얼이 클라 프리셋(`KalistaFx::SpawnQSpear/SpawnESpearStuck`, AlphaBlend)에서 **서버 리플리케이트 투사체 + 신규 wfx 큐 경로**로 이관됨 (`Kalista_Registration.cpp`의 훅이 `if (!ctx.bAuthoritativeEvent)` 게이트, 네트워크 플레이에선 큐 경로만 동작).
- 신규 `Data/LoL/FX/Champions/Kalista/*.wfx` 3종이 `"blend_mode": "Additive"` 선언. 그런데 창 텍스처(`kalista_base_q_mis_glow_color.png`)는 **RGB 순백**(측정: 전 픽셀 255,255,255) — 기존 파란 창 색은 전부 틴트(0.5,1.0,1.5)+AlphaBlend 합성에서 나온 것.
- DX11 Additive(SRC_ALPHA/ONE)에서 순백 텍스처 × 틴트를 밝은 맵 위에 가산 → 전 채널 포화 = 흰 실루엣. **텍스처 로드 실패 아님**(경로/로더 전 단계 검증됨).

### B. 창 1개만 표시
- 창은 히트마다 실제로 스폰되고 있었음. 문제는 `rend_spear.wfx`에 회전 `[1.5708, 0.40, 0.20]`·오프셋 `[0.15, 1.20, -0.10]`이 하드코딩 → **모든 창이 동일 위치·각도로 완전히 겹쳐** 1개처럼 보임.
- 레거시 경로(`SpawnESpearStuck`)의 랜덤 분산(yaw 0~2π, 틸트 ±0.3rad, 오프셋 XZ ±0.3/Y 1~2)이 큐 이관 때 유실됨.

### C. E 데미지 0
- **회귀 아님 — 서버 E는 원래 데미지를 넣은 적이 없음** (HEAD도 슬로우만, 현 워킹트리는 스턴만). `DamageRequest`를 만들지도 않음.
- 서버 측 Rend 스택(박힌 창) 추적 자체가 부재: BA/Q 투사체 히트 경로(`GameRoomProjectiles.cpp`)에 칼리스타 훅 없음.
- 데이터 팩(skill.kalista.e)에도 데미지 파라미터 없음. 유일한 Rend 데미지는 클라 프레젠테이션(`KalistaRendSystem::TriggerExplode`)의 로컬 hp 차감(20+30/스택) — 스냅샷이 즉시 덮어씀.

### D. 쿨다운 3초 지점
- 유일 관문 = `CommandExecutor.cpp`의 `ResolveCastSkillCooldown()` (CastSkill 수락 경로에서 1회 호출, slot.cooldownRemaining/Duration 기록).
- **봇도 동일 경로**: ChampionAI → `CServerAICommandProducer` → `m_pendingExecCommands` → `Phase_ExecuteCommands` → 동일 executor (Bot AI는 GameCommand 생산자, 게임플레이 진실 직접 변형 없음 — 우회 없음 확인).
- 해스트/CDR이 이 함수 안에서 이후에 곱해지므로, BA 조기반환 뒤·해스트 블록 앞에서 반환해야 정확히 3.0s.

## 적용 수정

| 파일 | 변경 |
|------|------|
| `Data/LoL/FX/Champions/Kalista/ba_projectile.wfx` | blend_mode Additive → **AlphaBlend** (A) |
| `Data/LoL/FX/Champions/Kalista/q_projectile.wfx` | blend_mode Additive → **AlphaBlend** (A) |
| `Data/LoL/FX/Champions/Kalista/rend_spear.wfx` | blend_mode → AlphaBlend (A) + 하드코딩 스큐 중립화: rotation `[π/2,0,0]`, offset `[0,1,0]` (B) |
| `Client/Public/GameObject/Projectile/ProjectileVisualCatalog.h` | `bAttachedCueRandomJitter` 필드 추가 (구조체 말미, aggregate 안전) (B) |
| `Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp` | 칼리스타 BA/Q 비주얼에 jitter=true (B) |
| `Client/Public/GameObject/FX/FxCuePlayer.h` | `FxCueContext`에 `bApplyAttachJitter`+오프셋/회전 지터 필드 (B) |
| `Client/Private/GameObject/FX/FxCuePlayer.cpp` | `BuildCueMesh`에서 지터 가산 (B) |
| `Client/Private/Network/Client/EventApplier.cpp` | 부착 큐 재생 시 레거시 밴드와 동일한 랜덤 지터 채움 + `<cstdlib>` (B) |
| `Shared/GameSim/Components/KalistaRendComponent.h` | **신규** — `KalistaRendStackComponent` (source/stackCount/window) (C) |
| `Shared/GameSim/Core/Checkpoint/WorldKeyframe.cpp` | 키프레임 레지스트리 등록 (완전성 기계검사 대응) (C) |
| `Shared/GameSim/Champions/Kalista/KalistaGameSim.h` | `ApplyRendStackOnHit` 선언 (C) |
| `Shared/GameSim/Champions/Kalista/KalistaGameSim.cpp` | `ApplyRendStackOnHit` 구현 + OnE에 데미지(기본20+창당30, Physical, 시전자 창만 소모) + Tick 스택 만료 (C) |
| `Server/Private/Game/GameRoomProjectiles.cpp` | 타겟(BA)/비타겟(Q) 히트 지점에 스택 훅 2곳 + include (C) |
| `Shared/GameSim/Definitions/SkillAtomData.h` | `eSkillEffectParamId::DamagePerSpear` 말미 추가 (append-only) (C) |
| `Tools/LoLData/Build-LoLDefinitionPack.py` | `damagePerSpear` 매핑 (C) |
| `Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json` | skill.kalista.e에 baseDamage 20 / damagePerSpear 30 (C) |
| `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp` | `ResolveCastSkillCooldown`에 DEV 오버라이드 `return 3.0f` (BA 조기반환 뒤·해스트 앞) (D) |

- 코드젠 재실행은 **의도적 보류**: 병렬 세션이 데이터 팩 작업 중 + 코드 fallback(20/30)이 JSON 값과 동일해 동작 차이 없음. 다음 regen 때 자동 파리티.
- D 되돌리기 = `return 3.0f;` 블록 삭제 한 곳.

## 알려진 한계 (의도적 보류)

- 창 유지시간: wfx lifetime 6s (레거시 30s + E 시 명시 제거였음). E로 뽑는 연출/제거 연동은 후속.
- 스택 수는 스냅샷 리플리케이트 안 됨(이벤트 기반 시각화) — 중도 접속자는 기존 박힌 창 안 보임.
- 쿨다운 3s 오버라이드 중엔 Ezreal Q 쿨감 패시브로 3s 미만 가능, Riven Q 재시전 설계는 유지됨.
- E 스턴은 기존 동작 유지(이번 세션의 의도적 피벗으로 보임) — 데미지만 추가.

## 검증

- [x] 원인 4건 코드 근거 확보 (병렬 조사 워크플로, 4에이전트)
- [x] 컴파일 에러 0 (수정 소스 6개 전부 통과, GameSim.lib/SimLab.exe 링크 성공. WintersGame/WintersServer.exe 링크만 LNK1104 = 인게임 점검으로 실행 중 잠금 — 게임/서버 종료 후 재빌드하면 해소)
- [x] SimLab PASS — same-seed replay OK, 해시 85A270CA→18110C0D 변동은 쿨다운 3s+E 데미지로 의도된 것. KeyframeAtomic PASS = 신규 KalistaRendStackComponent 레지스트리 등록 검증 통과
- [ ] 인게임 재점검: 창 파란색 / BA·Q 히트마다 랜덤 각도 창 누적 / E = 20+30×창수 데미지+스택 소모 / 전 스킬·R 쿨다운 3.0s
