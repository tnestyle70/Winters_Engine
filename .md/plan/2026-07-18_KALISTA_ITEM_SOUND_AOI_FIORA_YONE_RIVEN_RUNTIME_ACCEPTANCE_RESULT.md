Session - Kalista 3599 · Sound AOI · Fiora/Yone/Riven FX 회귀 복구 및 빌드 검증 결과

관련 요청: 칼리스타 3599 인벤토리 아이콘, 원거리·비가시 사운드 차단, 피오라 W/패시브/R 표시 위치, 요네 W -90도, 리븐 0.7 배율과 E 흰 사각형 제거

# 1. 결론

요청한 코드와 데이터 변경을 반영했고 Debug/Release Data-Driven 전체 파이프라인을 모두 통과했다.

- LoL Definition Pack: `0x940E8A30`
- Data-Driven 목표: `12/12 complete`
- gameplay 계약: `17 champions / 85 skills / 98 stages`
- ordered gameplay contract: `c2ee91f37a8e34b0`
- visual timing: `17 champions / 101 stages / mismatch 0`
- UI item atlas: 3599 sprite 포함, 생성기 freshness 통과
- Debug 전체 파이프라인: `PASS`
- Release 전체 파이프라인: `PASS`
- Release 산출물: Engine DLL, Server EXE, Client EXE, SimLab EXE 생성 완료

서버 권위 gameplay 판정은 건드리지 않았다. 피오라 vital 피격 판정은 기존 서버의 target→attacker 방향과 cardinal vital 방향 간 내적 계산을 그대로 사용하며, 이번 수정은 잘못 오프셋되던 Client 표시를 서버 방향 계약에 맞춘 것이다.

# 2. 실제 반영 내용

## 2-1. Kalista Black Spear 3599

원인은 3599 PNG가 존재해도 item shop atlas 생성 대상에서 특수 아이템으로 제외되고, UI가 일반 상점 gameplay definition을 찾지 못하면 인벤토리에 숫자 ID를 그리던 데 있었다.

반영:

1. `Tools/UIAtlas/build_itemshop_atlas.py`
   - 3599를 `inventory-only special`로 분류했다.
   - atlas에서 제외하지 않고 `item:3599_kalistapassiveitem.png` sprite를 생성한다.
   - inventory-only 아이콘이 빠지면 생성기가 실패하도록 invariant를 추가했다.
2. `Client/Private/GamePlay/LoLUIContentRegistry.cpp`
   - `kKalistaOathswornItemId`를 직접 사용해 Black Spear 표시 정보를 등록한다.
   - 가격 0, `Champion-bound item`, 구매 불가로 등록한다.
3. `Engine/Private/Manager/UI/UI_Manager.cpp`, `LuaUIHost.cpp`
   - inventory/HUD 조회에서는 3599 표시 정보를 사용할 수 있다.
   - direct shop grid, 선택, 구매 요청, Lua shop 목록에서는 구매 불가 항목을 제외한다.

결과적으로 3599는 인벤토리에서 숫자가 아니라 전용 아이콘으로 보이지만, 일반 상점 상품이나 ServerPrivate 구매 아이템으로 승격되지 않는다.

## 2-2. Champion sound AOI

원인은 replicated champion action/death sound와 voice가 발생 위치를 검사하지 않고 전역 재생 함수로 들어가던 데 있었다. 현재 프로젝트에서 실제 인게임 champion SFX/voice 재생 호출은 이 네트워크 경로 하나로 수렴한다.

반영:

1. `Data/LoL/Sound/ChampionSoundMap.json`
   - `maxAudibleDistance: 24.0`을 추가했다.
2. `ChampionSoundCatalog`
   - 최대 청취 거리를 JSON에서 읽고, 비정상 값은 24.0으로 복구한다.
3. `Scene_InGameNetwork.cpp`
   - 자기 자신의 액션은 항상 재생한다.
   - 원격 액션/사망 SFX와 voice는 source의 `VisibilityComponent.teamVisibilityMask`에 로컬 팀 bit가 있어야 한다.
   - source와 local listener의 XZ 거리가 최대 청취 거리 이하여야 한다.
   - 둘 중 하나라도 만족하지 않으면 재생하지 않는다.

UI/BGM과 로컬 이동 voice는 이 AOI에 묶지 않았다. gameplay 결과나 서버 snapshot도 변경하지 않았다.

## 2-3. Fiora W

`w_cast.wfx`의 6.8 길이 ground indicator가 Fiora 중앙을 기준으로 앞뒤 절반씩 놓이던 것이 문제였다.

- local Z 중심을 3.4만큼 전방으로 이동했다.
- W 이펙트의 뒤쪽은 Fiora 근처에서 시작하고, 긴 쪽 끝은 authoritative cue 방향, 즉 Fiora가 조준한 방향을 향한다.
- W 판정 범위와 서버 방향은 변경하지 않았다.

## 2-4. Fiora passive/R vital

새 PNG는 원 전체와 한 방향 표식을 함께 가진 텍스처다. 기존 코드는 예전의 분리된 표식 PNG처럼 전체 이미지를 대상 중심에서 1.6만큼 밀어냈기 때문에 원 중심과 챔피언 중심이 어긋났다.

반영:

- passive와 R vital 본체에서 `ApplyVitalOffset`을 제거했다.
- vital 이미지를 target champion XZ 중앙에 고정했다.
- cue 방향으로 회전할 수 있도록 centered `GroundDecal`, `billboard=false`로 구성했다.
- passive는 서버가 보낸 한 cardinal 방향으로 원 내부 표식을 회전한다.
- R은 네 방향 cue를 각각 별도 visual entity로 유지해, 한 방향을 터뜨리면 그 방향만 제거할 수 있다.
- 네 방향이 한 장에 박힌 `r_warning` 중복 emitter는 제거했다.
- `ApplyVitalOffset`은 원형 표시가 아니라 실제로 바깥쪽에 터져야 하는 consumed vital pop에만 남겼다.

서버 `TryConsumeVital`의 실제 피격 조건은 그대로다. target→attacker 정규화 벡터와 저장된 vital 방향을 dot-product로 비교하므로, 올바른 방향에서 맞았을 때만 passive/R vital이 소모된다.

## 2-5. Yone W

`Yone_FxPresets.cpp`에서 W visual forward만 XZ 기준 `-PI/2` 회전했다.

- W mesh/FX 표현: -90도 보정
- 서버 hit area, command direction, 피해 판정: 변경 없음

## 2-6. Riven FX와 E shield

반영:

- 모든 Riven cue에 `{0.7, 0.7, 0.7}` scale multiplier를 적용했다.
- billboard와 mesh가 같은 비율을 사용한다.
- E의 불투명한 `riven_base_e_sheildmult.png` billboard emitter를 제거했다.
- shield mesh texture를 alpha가 있는 `riven_base_e_sheildmeshtext.png`로 교체했다.

따라서 리븐 FX는 현재 구성 대비 70% 크기로 줄고, E에서 3x3 흰 사각형을 만들던 직접 원인이 제거됐다.

# 3. 검증 결과

## 3-1. 집중 데이터·리소스 검증

실행 및 결과:

```text
python Tools/LoLData/Test-ChampionGameDataSchema.py
[ChampionSchemaContract] PASS

python Tools/LoLData/Build-LoLDefinitionPack.py --check
Checked LoL definition pack 0x940E8A30
Champions: 17, skills: 85, summoner spells: 1

python Tools/UIAtlas/build_itemshop_atlas.py --check
PASS — 3599 atlas sprite present

Focused JSON/WFX semantic probes
PASS — sound distance 24, centered Fiora decals, Riven E invalid billboard absent

git diff --check
PASS — whitespace error 0
```

## 3-2. Debug 전체 파이프라인

```powershell
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug -RequireComplete
```

결과:

```text
[LoLDataDriven] PASS
```

포함 범위: schema mutation, definition freshness, legacy ownership audit, draft round-trip, Data-Driven 12/12, Shared boundary, GameSim/Server/Client/SimLab Debug build, 결정론 회귀, 최종 diff gate.

## 3-3. Release 전체 파이프라인

```powershell
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration Release -RequireComplete
```

결과:

```text
Engine/Bin/Release/WintersEngine.dll
Server/Bin/Release/WintersServer.exe
Client/Bin/Release/WintersGame.exe
Tools/Bin/Release/SimLab.exe
[SimLab][DataContract] PASS: 17 champions, 85 skills, 98 stages, orderedContract=c2ee91f37a8e34b0
[LoLDataDriven] PASS
```

빌드에는 기존 DLL interface 경고 `C4251/C4275`가 남아 있으나 새 compile/link error는 없다.

# 4. 인게임 최종 acceptance

자동 검증은 데이터 구조, 컴파일, 링크, 판정 회귀를 닫지만 카메라 투영과 텍스처 체감은 실제 화면으로 확인해야 한다. 다음 항목만 F5에서 최종 판정한다.

1. Kalista 시작 인벤토리에서 `3599` 숫자 대신 Black Spear 아이콘이 보인다.
2. Black Spear는 상점 상품 목록에 없고 구매할 수도 없다.
3. 원격 챔피언은 비가시 상태 또는 24.0 초과 거리에서 액션/사망 SFX와 voice가 들리지 않는다.
4. 가시 상태이면서 24.0 이내인 원격 챔피언은 들리고, 자기 champion sound는 항상 들린다.
5. Fiora W의 긴 끝이 바라보는 방향을 향하고, 이펙트가 몸 뒤로 절반 뻗지 않는다.
6. passive 원의 중앙과 적 champion 중앙이 일치한다. 표시된 방향에서 공격하면 터지고 반대쪽에서는 터지지 않는다.
7. R 네 vital도 적 중앙에 놓이며, 한 방향 소모 시 해당 방향 표시만 사라진다.
8. Yone W FX가 기존 대비 -90도 회전하되 피해 부채꼴은 바뀌지 않는다.
9. Riven FX가 70% 크기로 보이고 E shield의 흰 사각형이 사라지며 shield mesh는 계속 따라간다.

# 5. 권위 경계

- JSON/WFX: 최대 청취 거리, 텍스처, emitter 형태, visual offset/scale처럼 기획·표현 데이터를 소유한다.
- Client C++: visibility/distance 조건 적용, cue 방향 변환, UI 표시/구매 가능 여부를 실행한다.
- Shared/Server GameSim: Fiora vital 방향 선택과 피격 판정, skill hit 결과를 계속 소유한다.
- 3599는 champion-bound inventory presentation이며 일반 구매 gameplay item data가 아니다.

이 경계 덕분에 이번 수정은 표시와 청취 정책을 고치면서 서버 gameplay 진실을 다시 Client 쪽으로 되돌리지 않는다.
