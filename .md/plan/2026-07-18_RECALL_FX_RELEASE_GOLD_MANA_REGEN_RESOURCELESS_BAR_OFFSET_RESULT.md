Session - 귀환 FX·Release 4000 골드·마나 초당 3 재생·무자원 월드바 정렬 결과

관련 계획: `C:/Users/user/Desktop/Winters/.md/plan/2026-07-18_RECALL_FX_RELEASE_GOLD_MANA_REGEN_RESOURCELESS_BAR_OFFSET_PLAN.md`

## 예측과 실측

- 예측: 귀환 전용 원본이 없으면 저장소에 존재하는 `fiora_base_e_buff_mult.png`를 공용 파란 지면 링으로 사용하고, 권위 Recall action의 시작·취소·완료·사망·엔티티 제거·타임라인 재기준에 맞춰 한 번만 생성·제거한다.
  - 실측: 귀환 전용 WFX는 없었고 fallback PNG는 `Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_e_buff_mult.png`에 존재했다. `Recall.Channel` 단일 GroundDecal WFX를 추가하고 snapshot의 action sequence/`lockEndTick`에 연결했다. WFX JSON·texture 경로 계약과 Client Debug/Release 빌드가 통과했다.
- 예측: Debug의 기존 시작 골드 10000은 유지하고 Server Release에서만 4000으로 조립한다.
  - 실측: `Server.vcxproj`가 Debug에는 `_DEBUG`, Release에는 `NDEBUG`를 선언한다. `BuildChampionEntity`의 loadout 복사 직후 Release 전용 4000 override를 적용했으며 Server Debug/Release 빌드가 모두 통과했다.
- 예측: Mana 10명은 생존 중 초당 3을 서버 권위로 회복하고, 기존 Energy 초당 10과 Flow/None 0 계약은 유지한다.
  - 실측: 원본 JSON의 Mana 10개 항목을 모두 3.0으로 갱신하고 `TickResourceRegeneration` 허용 종류를 Mana/Energy로 좁혔다. 챔피언 데이터 해시 `0x0B775958`, definition pack 해시 `0x17C567E4`로 재생성·`--check`가 일치했다. SimLab resource-only와 전체 회귀가 PASS했고 최대치 clamp·사망 차단 경로는 기존 서버 tick을 그대로 공유한다.
- 예측: 무자원 챔피언은 빈 마나바를 다시 그리지 않고, 축소된 체력바의 위쪽을 Mana/Energy/Flow 2단 바의 위쪽과 맞춘다.
  - 실측: 공용 `BuildHealthBarScreenRects`에서 무자원 바의 `BarMin.y`를 원래 전체 높이의 위쪽에 고정했다. ImGui/RHI 양쪽이 같은 rect를 사용하며 Engine Debug와 Client Debug/Release 빌드가 통과했다.
- 예측: 다른 Codex가 수정 중인 AI/SimLab 파일과 충돌하지 않는다.
  - 실측: `GameRoomChampionAI.cpp`, `ChampionAISystem.*`, `Tools/SimLab/main.cpp` 및 그 계획·결과·빌드 보고서는 편집하지 않았다. 현재 dirty 상태를 포함한 SimLab Debug 빌드와 전체 회귀까지 통과했다.

### 2026-07-19 귀환 FX 가시성 복구 실측

- 예측: 기존 귀환 WFX가 안 보인 원인은 asset 손상보다 action 수명 판정 오류일 가능성이 높다.
  - 실측: `RecallComponent::bActive`는 6초간 유지되지만 Recall은 replicated gameplay action lock 대상이 아니어서 기존 `lockEndTick == startTick`이었다. 클라이언트의 `serverTick < lockEndTick` 조건은 첫 snapshot부터 거짓이었고 cue 생성이 실행되지 않았다. 원본 PNG는 256x256 청색 이중 링으로 정상이며 동일 경로·GroundDecal 형식도 기존 loader 계약과 일치했다.
- 예측: 서버의 실제 Recall 상태를 snapshot에 직접 복제하면 시작·취소·완료·사망·replay seek가 같은 수명 기준을 공유한다.
  - 실측: snapshot `stateFlags`의 비어 있던 bit 31을 `kSnapshotStateRecallFlag`로 예약하고 `RecallComponent::bActive`일 때만 서버가 설정하도록 반영했다. 클라이언트는 action sequence/lock 추정을 제거하고 매 프레임 flag와 FX handle을 동기화한다. 독립 검토에서 기존 state bit 0~6 및 AI debug bit 7~30과 충돌하지 않음을 확인했고 P0 없이 `CONDITIONAL PASS`를 받았다.
- 예측: 귀환 링을 A키 공격 사거리 원과 같은 함수·발밑 offset으로 계산하면 챔피언별 크기와 중심이 일치한다.
  - 실측: `StartNetworkRecallFx()`가 A키 렌더 경로와 같은 `GameplayQuery::ResolveAttackRangePreviewRadius()`를 호출하고 `radius * 2`를 WFX width/height override로 사용한다. `recall.wfx`의 authoring 기본값은 Ashe 기준 직경 15.9, 발밑 offset은 A키와 같은 `+0.02f`로 갱신했다.
- 예측: Debug trace와 양 구성 빌드로 cue/texture 계약과 Release 무로그 경계를 확인할 수 있다.
  - 실측: Debug 실행 파일에는 최대 32회의 `[RecallFx] spawn/stop` 및 texture preload 결과 문자열이 링크됐고 Release 실행 파일에서는 제거됐다. WFX JSON·texture·크기·offset 계약, 전체 `git diff --check`, GameSim/Server/Client Debug와 Release 빌드가 모두 통과했다. 첫 Engine Release 링크는 기존 호환 불일치 PDB의 `LNK1201`로 실패했으나 PDB를 분리한 재링크부터 Engine/Server/Client Release가 모두 성공했다.
- 예측: 자동 smoke가 실제 경기까지 진입하면 화면 가시성도 캡처할 수 있다.
  - 실측: Server Debug/Release 3초 smoke는 각각 exit 0, Release Server+Ashe DX11 client는 15초 생존했다. SimLab resource-only/전체 회귀와 `retreat->Recall` probe도 PASS했다. 다만 저장된 로그인 세션에서 `--banpick-smoke`가 경기 시작을 자동 진행하지 못해 화면 자동화는 커스텀 로비에서 멈췄다. 따라서 실제 인게임 `B`의 파란 링 픽셀은 PASS로 주장하지 않고 수동 F5 최종 게이트로 남긴다.

## 판정

**PASS — 계획 반영, 데이터 재생성, Debug/Release 빌드와 SimLab 회귀까지 완료했다.**

- 챔피언 데이터 schema test, 두 생성기의 생성 및 `--check`, Recall WFX JSON/texture 계약이 모두 성공했다.
- GameSim Debug, Engine Debug, Client Debug, Server Debug, Server Release, Client Release, SimLab Debug를 `/m:1`로 빌드해 모두 성공했다.
- `SimLab.exe --resource-only`와 전체 `SimLab.exe`가 모두 PASS했다.
- 기존 C4251/C4275 DLL 인터페이스 경고는 남아 있으나 신규 컴파일 오류는 없다.
- 자동 검증은 픽셀 색감과 실제 화면 높이를 판정하지 않으므로 F5에서 귀환 시작/취소/완료 및 Mana/None 월드바를 보는 시각 QA는 별도 최종 확인 항목이다.

### 2026-07-19 추가 판정

**코드·계약·회귀·Debug/Release 빌드 PASS, 실제 인게임 귀환 링 픽셀은 수동 확인 필요.**

- 전혀 보이지 않던 1차 원인은 WFX asset이 아니라 즉시 만료되는 action-lock 기반 client gate였고, 서버 `RecallComponent` snapshot flag로 교체했다.
- 크기는 고정 추정값이 아니라 각 챔피언의 A키 preview 계산값을 그대로 사용하며 중심은 발밑 `y + 0.02f`다.
- Server Debug/Release boot smoke, Release DX11 client 생존, SimLab Recall 권위 회귀까지 성공했다.
- 자동 화면 진입 실패를 링 실패로 해석하지 않으며, 반대로 화면 PASS로도 승격하지 않는다. Debug F5에서 `B` 후 `[RecallFx] spawn ... spawned=1 texture=1`과 청색 링을 함께 보는 것이 마지막 수동 관문이다.

## 갱신된 트레이드오프

- 귀환 링은 전용 원본이 들어올 때까지 Fiora E multiplier 텍스처를 빌려 쓴다. 교체 지점은 `recall.wfx`의 texture 한 곳으로 제한했다.
- Release 골드 override는 명시적으로 4000을 강제하므로 Release 런타임 overlay로 시작 골드를 바꾸는 요구가 생기면 `SpawnLoadoutPolicyDef`에 구성별 정책을 데이터화해야 한다.
- Mana와 Energy는 같은 서버 재생 tick을 공유하지만 수치는 authored data로 분리했다. 새 자원 종류가 자연 재생을 요구하면 허용 종류와 회귀 probe를 함께 확장해야 한다.
- 무자원 바는 0.68배 높이를 유지해 리소스 없음은 계속 구분되며, 이번 변경은 위쪽 기준선만 맞춘다.
- 귀환 FX 수명은 이제 action lock과 분리되어 snapshot bit 31을 wire 의미로 소비한다. 이후 state bit를 추가할 때 bit 31 예약을 유지해야 한다.
- WFX 기본 15.9는 authoring preview용이고 런타임 진실은 챔피언별 A키 반경 override다. 기본 공격 사거리 또는 충돌 반경 공식이 바뀌면 귀환 링도 같은 helper를 통해 자동으로 따라간다.
- 자동 시각 QA는 저장된 세션에서 custom lobby를 넘지 못했다. 재현 시 Debug 1서버+1클라이언트에서 `B`, 이동/피격 취소, 6초 완료를 각각 확인하고 spawn/stop trace와 화면을 한 세트로 보관한다.
