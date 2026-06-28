Session - 네트워크에서 ubyte championId(eChampion enum) 대신 안정 DefinitionKey를 송수신하고, 데이터팩 manifest 버전을 replicate해 클라/서버가 같은 정의 공간을 합의한다.

배경(한 줄): 현재 wire는 `championId:ubyte`(eChampion). 내부엔 안정 식별자 `DefinitionKey`(u32, FNV)와 `ChampionGameplayDef{key,id(ChampionDefId u16),legacyChampion}`가 있지만(`DefinitionIds.h`, `ChampionGameplayDef.h:48-59`) 네트워크로 보내지 않고, `DataPackManifest`(version/hash)도 replicate 안 한다. 규칙·게이트·식별자 규칙은 07(§1-3: DefinitionKey만 네트워크, ChampionDefId/SkillDefId는 pack-local).

검토 반영(2026-06-28) — 확정 정정:
- (★정정) .fbs에는 RPC service가 없고 table/enum 데이터 타입뿐이다. flatc 재생성은 **C++/Go 데이터 타입 스텁 동기화**이지 "Go 서비스가 깨진다"가 아니다. 1-1/검증의 "Go 서비스 재생성/깨짐" 문구를 "flatbuffer 생성 코드(C++/Go) 재생성"으로 교체.
- (성능) `FindChampion(eChampion)`/`FindChampion(DefinitionKey)`는 O(n) 선형 탐색이다. 스냅샷 빌드(10 엔티티 × 30Hz)에서 매번 호출하면 회귀 → **캐싱하거나 `ChampionDefinitionComponent.championDefId`(이미 엔티티에 부착)로 O(1) 조회**한다.
- (I1 확정) `GameplayDefinitionPack`은 Shared에 유지 → 클라가 `FindChampion(DefinitionKey)`를 호출해도 I1 위반 아님. 단 **ServerPrivate gameplay 값(damage/CC)이 Client 바이너리로 누수하지 않도록** pack visibility를 점검(SharedContract: DefinitionKey+manifest, ClientPublic: visual만 네트워크/클라).
- (라인) `SnapshotApplier.cpp`의 OnHello/Onsnapshot read 라인(517/619/717/1325/1519)은 큰 파일이라 착수 직전 `rg`로 재확인.
- (정책) manifest 불일치 시 거부/경고 정책을 명시(local-smoke는 허용할지 포함).

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Hello.fbs, Snapshot.fbs

기존 `championId:ubyte`는 호환을 위해 유지하고, 안정 식별자/manifest를 additive로 추가한다.

```text
Hello.fbs (table Hello):
  + championDefinitionKey:uint;
  + definitionPackDataVersion:uint;
  + definitionPackBuildHash:uint;
Snapshot.fbs (EntitySnapshot, line 42 championId:ubyte 옆):
  + championDefinitionKey:uint;
Snapshot root (1회):
  + definitionPackManifestHash:uint;
```

확인 필요: flatbuffers는 끝에 필드 추가가 forward-compat. `run_codegen`(또는 flatc)으로 C++/Go 양쪽 generated 재생성 필요(07 P7 주의). schema 변경은 Go 서비스 재생성도 동반(중복 방지: 메모리의 schema codegen 이중화).

1-2. C:/Users/tnest/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp + GameRoomLobby.cpp

write 사이트에서 championId(legacy) 옆에 DefinitionKey를 채운다.

```text
- SnapshotBuilder.cpp:241/265/274/755: champion.id(eChampion) -> definitions.FindChampion(eChampion)->key 로 championDefinitionKey 채움. championId(ubyte)도 당분간 유지.
- GameRoomLobby.cpp(~331) CreateHello: championDefinitionKey + manifest(uDataVersion/uBuildHash) 추가.
- Snapshot root: definitions.manifest.uBuildHash 1회 기록.
```

확인 필요: `definitions.manifest`(GameplayDefinitionPack.h:12 내부 `DataPackManifest manifest{}`)에서 uDataVersion/uBuildHash 읽기. FindChampion(eChampion)는 O(n)이나 스냅샷 빌드 빈도 고려(캐시 가능).

1-3. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

read 사이트에서 DefinitionKey 우선, 실패 시 legacy eChampion fallback.

```text
- OnHello(517): manifest 검증(클라 pack version/hash == 서버) 후 EnsureEntity. 불일치면 경고/거부.
- OnSnapshot(619/717/1325/1519): es->championDefinitionKey() -> definitions.FindChampion(DefinitionKey) -> legacyChampion. key 없으면 es->championId() eChampion fallback.
```

확인 필요: 클라가 GameplayDefinitionPack(FindChampion(DefinitionKey), GameplayDefinitionPack.h:20-22)에 접근 가능한지. 서버 전용이면 클라용 pack 접근자 필요(I1). manifest 불일치 처리 정책(거부 vs best-effort) 확정.

1-4. 식별자 불변 규칙 준수

```text
- 네트워크엔 DefinitionKey(u32 안정)만. ChampionDefId(u16 dense, pack-local)/SkillDefId/EntityHandle은 보내지 않는다(07 §1-3).
- eChampion은 컷오버 동안 bridge로만(championId:ubyte 호환 필드). 최종적으로 제거 후보(P8 이후).
```

2. 검증 (07 §6)

미검증:
- schema/codegen 미반영, 송수신 변환 미반영

검증 명령:
- run_codegen(flatc) 후 C++/Go generated 재생성
- python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
- GameSim/Server/Client/SimLab Debug x64 빌드
- 네트워크 smoke(서버-클라 핸드셰이크 + 스냅샷 적용)

통과 기준:
- 클라가 DefinitionKey로 챔피언을 해석하고, manifest 불일치를 감지.
- legacy championId 제거 없이(호환 유지) 동작. G1 빌드 PASS.
- SimLab 해시: 네트워크 레이어는 SimLab 결정론에 포함되지 않으므로 불변(영향 없음). 네트워크 smoke는 수동 게이트.

확인 필요:
- flatbuffers Go codegen 동반 여부(Services). schema 변경 후 Go 미재생성 시 서비스 깨짐(메모리 리스크).
- 클라 pack 접근(I1). manifest 불일치 UX/정책.
