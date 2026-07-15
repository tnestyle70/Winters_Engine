# Session - Numeric account storefront and BanPick ownership presentation

Date: 2026-07-14  
Packet: `2026-07-14_account_storefront_banpick_ownership`

## Outcome

숫자 ID `1`로 인증한 계정의 PostgreSQL/JWT storefront를 Client까지 연결해 RP, 챔피언 17종, 가격, 구매 상태를 표시하도록 복구했다. 상점 버튼은 기존 소스 좌표에서 오른쪽으로 48px 이동했고, 밴픽은 동기화가 완료된 온라인 계정의 미보유 챔피언 원본 초상화 위에만 검은 반투명 오버레이를 그린다.

## Root cause evidence

- `Scene_Login`의 배경 화살표는 ID 로그인이 아니라 `RequestOfflineLogin()`을 호출하고 있었다.
- offline seed는 `Skin`/`Boost` 3개뿐인데 `Scene_Shop`은 `item_type=champion`과 `content_key`가 있는 상품만 만들기 때문에 결과 셀이 0개였다.
- Services local-ID validation은 2~32자라 사용자가 지정한 `1`을 400으로 거절했다.
- portrait 리소스와 DB 상품은 누락되지 않았다. gameplay 정의, migration 000008, Client catalog의 `champion.*` 키 17개가 모두 일치했고 square portrait 17개도 존재한다.
- MyInfo의 replay 목록은 로컬 replay 파일 목록이므로 backend storefront 연결 성공의 증거가 아니었다.

## Implemented structure

### Authentication and session

- local ID 허용 범위를 1~32자로 변경하고 7개 table test를 추가했다.
- 로그인 이미지의 화살표는 입력된 ID로 `LoginByID`를 호출한다. ID가 없으면 입력 안내를 표시한다.
- 없는 ID는 기존 계약대로 404 후 `회원 가입` 버튼을 노출하고, 성공한 JWT를 `CClientShellSession`에 저장한다.
- smoke 명령행의 명시적 offline 로그인은 그대로 유지했다.

### Storefront and shop

- `ChampionCatalogEntry`가 `champion.*` content key를 단일 매핑으로 소유한다.
- `Scene_Shop`의 별도 17개 mapping table을 제거하고 catalog의 `FindByContentKey()`를 사용한다.
- Services storefront의 wallet/RP와 inventory ownership 읽기를 read-only repeatable-read transaction 하나로 묶어 같은 DB snapshot을 보장한다.
- `CClientShellDataStore`에 storefront revision을 추가했다. 상품 수가 같아도 새 snapshot/소유권이 오면 cell을 다시 구성한다.
- 구매 완료/중복 구매 결과는 server의 remaining RP와 ownership으로 수렴한다.
- 보유 상품 클릭 문구는 정확히 `이미 구매한 상품입니다.`로 통일했다.

### Main menu and BanPick

- MainMenu 상점 rect/text를 함께 오른쪽으로 48px 이동해 시각 위치와 hit-test를 일치시켰다.
- BanPick 진입 시 storefront 재동기화를 요청하고 backend callback을 계속 pump한다.
- overlay 조건은 `authenticated && !offline && initialSyncReady && listedChampion && !owned`다.
- SyncPending, offline, storefront에 없는 content key는 미보유로 오판하지 않는다.
- 검은 overlay는 원본 portrait를 먼저 그린 뒤 alpha 0.68로 덮는다.
- 현재 overlay는 소유 상태를 보여 주는 표현 계층이다. 기존 5:5 검증/봇 캐릭터 교체 흐름을 보존하기 위해 champion pick 자체는 막지 않았다. 서버 권위 entitlement 차단은 별도 JWT/Shop-to-Lobby 연결 과제다.

## Verification

- `go test ./...` from `Services`: PASS.
- `Client/Include/Client.vcxproj`, Debug x64, `/m:1 /nr:false`: PASS, output `Client/Bin/Debug/WintersGame.exe`.
- `git diff --check` on owned paths: PASS (line-ending notices only).
- content-key coverage: gameplay 17 / Client catalog 17 / DB seed 17, missing 0.
- modified auth service on temporary port:
  - ID `1` registration/authentication: PASS.
  - account `1` storefront: 1000 RP, 17 products.
- isolated purchase E2E account:
  - first purchase `champion.ezreal`: `purchased`, 1000 -> 950 RP.
  - duplicate purchase: `already_owned`, 950 -> 950 RP.
  - storefront ownership persistence: PASS.
- live services after controlled auth-only restart:
  - 8081 auth: PASS for ID `1`.
  - 8084 profile: PASS.
  - 8086 storefront: PASS, account `1` = 1000 RP / 17 products / 0 owned.
- shop service를 repeatable-read 변경 코드로 재시작한 뒤 같은 account `1` storefront를 다시 조회해 1000 RP / 17 products snapshot PASS를 확인했다.

## Review disposition

- 미보유 셀 클릭을 막지 않는다는 review 지적은 의도된 동작으로 유지했다. 이번 범위는 원본 portrait 위 소유권 overlay이며, 사용자가 요청한 전 챔피언 5:5 검증/봇 교체를 끊지 않는다.
- storefront balance/owned가 서로 다른 DB 시점일 수 있다는 review 지적은 수용해 read-only repeatable-read transaction으로 수정했다.
- 동기화 완료 뒤 content key가 storefront에 없을 때 overlay를 생략하는 fail-open은 의도적으로 유지했다. 누락/부분 응답을 미보유로 단정해 모든 테스트 캐릭터를 잘못 잠그는 것보다, status/coverage 검증에서 누락을 잡는 경계다.

The Client build still prints pre-existing C4275 DLL-interface warnings and an unrelated `ChampionSpawnService.cpp` format warning; no new error was introduced.

## Visual handoff

1. `Client/Bin/Debug/WintersGame.exe`를 실행한다.
2. ID 입력란에 `1`을 넣고 로그인 버튼 또는 이미지 화살표를 누른다. 검증 과정에서 계정 `1`은 이미 생성됐다.
3. MainMenu에서 오른쪽으로 옮긴 상점 버튼을 누른다.
4. 1000 RP, 17개 portrait, 각 DB 가격(현재 50 RP)을 확인한다.
5. 하나를 구매해 950 RP/`보유 중`을 확인하고 다시 눌러 `이미 구매한 상품입니다.`를 확인한다.
6. 밴픽에서 구매한 초상화는 원본 상태, 나머지 DB-listed 미보유 초상화는 검은 반투명 overlay인지 확인한다.

## Owned files

- `Services/internal/auth/service.go`
- `Services/internal/auth/service_test.go`
- `Services/internal/shop/repository.go`
- `Client/Private/Scene/Scene_Login.cpp`
- `Client/Private/Scene/Scene_MainMenu.cpp`
- `Client/Private/Scene/Scene_Shop.cpp`
- `Client/Public/Scene/Scene_Shop.h`
- `Client/Private/Scene/Scene_BanPick.cpp`
- `Client/Public/Scene/Scene_BanPick.h`
- `Client/Private/GamePlay/ChampionCatalog.cpp`
- `Client/Public/GamePlay/ChampionCatalog.h`
- `Client/Private/ClientShell/ClientShellDataStore.cpp`
- `Client/Public/ClientShell/ClientShellDataStore.h`
