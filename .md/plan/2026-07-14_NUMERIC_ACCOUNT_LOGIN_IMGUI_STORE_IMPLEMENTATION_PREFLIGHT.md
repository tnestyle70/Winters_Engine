Session - 숫자 개발 계정 1~5를 소셜 identity와 같은 방식으로 내부 UUID 계정에 연결하고, PostgreSQL에서 RP·계정 정보·챔피언 소유권을 복원한 뒤 MainMenu ImGui 상점과 밴픽 표현까지 충돌 없이 연결한다.
Session - 본 문서는 2026-07-13 계정/RP/상점 아키텍처 계획의 1차 수직 슬라이스 구현 전 잠금 문서다. 초기 RP는 10,000, 판매 상품은 명시적 15종, 미보유 챔피언도 플레이 가능으로 고정한다.
Session - 현재 S024 5v5 Bot Stability와 다른 Client 작업이 진행 중이므로 Services부터 분리 구현하고, CHttpClient와 Scene_BanPick 소유권이 반환된 뒤 Client를 직렬 통합한다. 동시 MSBuild는 금지한다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/.md/architecture/WINTERS_DATA_ARCHITECTURE.md

`## 1. 데이터 소유권 매트릭스 (목표 = 현재의 방향)`의 기존 소유권 설명 아래에 다음 계정 식별·복원 경계를 추가한다. 숫자 `1`~`5`를 `users.id`로 쓰지 않는 것이 핵심이다.

아래에 추가:

```markdown
### Account Identity와 복원 경계

Winters의 내부 AccountID는 `users.id UUID` 하나다. 로그인 화면에서 입력하는 숫자 개발 계정, 일반 계정, Google/Kakao 같은 소셜 계정은 모두 외부 identity이며 다음 튜플로 UUID에 매핑한다.

`(provider, provider_subject) -> users.id UUID`

예:

- `(dev_slot, "1") -> UUID-A`
- `(dev_slot, "2") -> UUID-B`
- `(google, "provider-issued-sub") -> UUID-C`

`provider_subject`는 공급자가 보장하는 불변 식별자다. email, 화면 표시명, Client가 보낸 UUID를 계정 병합 기준으로 쓰지 않는다. 서로 다른 provider identity를 하나의 계정에 연결하는 작업은 로그인된 사용자의 명시적 link 절차에서만 허용한다.

복원 흐름은 `Login identity -> Auth lookup -> JWT(UUID claims) -> /profile/me + /shop/storefront -> Client snapshot replacement`다. RP, 계정 XP/레벨, 구매 이력은 PostgreSQL이 권위자이며 Client 로컬 JSON이나 오프라인 기본값으로 온라인 계정을 덮지 않는다.

Meta 상점 구매는 `Client request + idempotency key -> JWT UUID -> wallet row lock -> ownership check -> RP 차감 + entitlement + ledger commit -> storefront response` 한 transaction으로 처리한다.

밴픽 소유권은 표현 정보이며 챔피언 선택 가능 여부와 분리한다. `Owned`, `UnownedListed`, `NotListed`, `SyncPending` 중 오직 `UnownedListed`만 검은 overlay를 표시하고 입력/선택은 차단하지 않는다.

인게임 아이템 상점은 이 Meta 상점과 별개다. 아이템 가격·구매 조건·인게임 gold·슬롯 변경은 Server/Shared GameSim이 권위자이고, Services의 RP wallet이나 champion entitlement를 사용하지 않는다.
```

### 1-2. C:/Users/user/Desktop/Winters/.md/collab/ACTIVE_WORK_PACKETS.md

현재 파일 자체와 관련 Client/Server 파일이 다른 세션에서 변경 중이므로 지금 수정하지 않는다. 각 구현 시작 직전에 최신 파일을 다시 읽고 한 owner가 짧은 직렬 구간에서 packet을 등록한다.

`CONFIRM_NEEDED`:

- S024가 완료 또는 Handoff인지 확인한다.
- 이미 S025 이상 번호가 생겼으면 아래 임시 번호를 최신 번호로 재배정한다.
- 한 파일을 두 packet이 동시에 Owned로 선언하지 않는다.

등록할 범위:

```text
S025 Account/Storefront Backend
Owned: Services/migrations/000008_*, Services/internal/auth/**, Services/internal/profile/**,
       Services/internal/shop/**, Services/pkg/auth/jwt.go, Services/pkg/config/config.go,
       Services/cmd/auth/main.go, Services/.env.example, Services/Makefile,
       Data/Account/AccountEconomyPolicy.json, Data/Account/Store/ChampionProducts.json
Read-only: Client/**, Server/**, Shared/**
Gate: go test ./..., 숫자 계정/구매/재로그인 E2E

S026 Client Numeric Login/MainMenu Store
Owned: AuthClient.*, ProfileClient.*, CShopClient.*, ClientShell/**,
       Scene_Login.*, Scene_MainMenu.*
Read-only: CHttpClient.*, Scene_BanPick.*, LobbyRosterHelpers.*, Client.vcxproj*, Engine/**,
           Server/**, Shared/**
Gate: S025 API 고정, CHttpClient Handoff, S024 종료 후 단독 Client 빌드

S027 BanPick Entitlement Overlay
Owned: Scene_BanPick.h, Scene_BanPick.cpp
Read-only: LobbyRosterHelpers.*, ClientShell/**, Server/**, Shared/**, schemas/generated/**
Gate: 기존 Scene_BanPick 변경 owner의 Handoff와 최신 diff 재검토

S028 Match Reward / Real Social Provider
Deferred: S024 및 Server Network 작업 이후 별도 계획/packet
```

### 1-3. C:/Users/user/Desktop/Winters/Services/migrations/000008_create_account_identity_storefront.up.sql

새 파일: `CONFIRM_NEEDED`

구현 직전 migration 번호가 여전히 비어 있는지 다시 확인한다. 2026-07-14 감사 시점에는 `000007`이 마지막이었다. 같은 번호를 다른 세션이 사용했으면 번호만 올리고 SQL 의미는 유지한다.

이 migration은 2026-07-13 계획의 광범위한 경기 보상 테이블까지 한 번에 넣지 않는다. 이번 수직 슬라이스에 필요한 다음 항목만 포함한다.

```sql
ALTER TABLE users
    ALTER COLUMN email DROP NOT NULL,
    ALTER COLUMN password DROP NOT NULL;

CREATE TABLE user_identities (
    id               UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id          UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    provider         VARCHAR(32) NOT NULL,
    provider_subject VARCHAR(255) NOT NULL,
    provider_email   VARCHAR(255),
    created_at       TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at       TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE(provider, provider_subject)
);

CREATE INDEX idx_user_identities_user_id
    ON user_identities(user_id);

INSERT INTO user_identities (user_id, provider, provider_subject, provider_email)
SELECT id, 'local_email', LOWER(email), email
FROM users
WHERE email IS NOT NULL
ON CONFLICT (provider, provider_subject) DO NOTHING;

CREATE TABLE account_progression (
    user_id    UUID PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    level      INT NOT NULL DEFAULT 1 CHECK (level >= 1),
    total_xp   BIGINT NOT NULL DEFAULT 0 CHECK (total_xp >= 0),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

INSERT INTO account_progression (user_id)
SELECT id FROM users
ON CONFLICT (user_id) DO NOTHING;

ALTER TABLE wallets
    ADD COLUMN currency_code VARCHAR(8) NOT NULL DEFAULT 'RP';

ALTER TABLE coin_transactions
    DROP CONSTRAINT coin_transactions_tx_type_check;

ALTER TABLE coin_transactions
    ADD CONSTRAINT coin_transactions_tx_type_check
    CHECK (tx_type IN ('charge', 'purchase', 'refund', 'initial_grant', 'match_reward', 'admin_grant'));

ALTER TABLE coin_transactions
    ADD COLUMN source_kind VARCHAR(32),
    ADD COLUMN source_id VARCHAR(128),
    ADD COLUMN idempotency_key VARCHAR(128);

CREATE UNIQUE INDEX uq_coin_transactions_user_idempotency
    ON coin_transactions(user_id, idempotency_key)
    WHERE idempotency_key IS NOT NULL;

ALTER TABLE shop_items
    ADD COLUMN product_key VARCHAR(128),
    ADD COLUMN content_key VARCHAR(128),
    ADD COLUMN sort_order INT NOT NULL DEFAULT 0,
    ADD COLUMN is_stackable BOOLEAN NOT NULL DEFAULT true;

CREATE UNIQUE INDEX uq_shop_items_product_key
    ON shop_items(product_key)
    WHERE product_key IS NOT NULL;

CREATE TABLE purchase_receipts (
    id              UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id         UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    item_id         UUID NOT NULL REFERENCES shop_items(id),
    request_key     VARCHAR(128) NOT NULL,
    status          VARCHAR(24) NOT NULL CHECK (status IN ('purchased', 'already_owned')),
    balance_after   BIGINT NOT NULL CHECK (balance_after >= 0),
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE(user_id, request_key)
);
```

기존 계정의 잔액을 무조건 10,000으로 덮는 migration은 넣지 않는다. 신규 계정의 최초 RP grant는 계정 생성 transaction에서 한 번만 수행하며 ledger의 `idempotency_key='initial-grant-v1'`로 중복을 막는다. 기존 개발 계정 backfill은 별도 명시적 운영 SQL로 수행한다.

### 1-4. C:/Users/user/Desktop/Winters/Services/migrations/000008_create_account_identity_storefront.down.sql

새 파일:

```sql
DO $$
BEGIN
    RAISE EXCEPTION
        'account identity/storefront migration is forward-only; restore a verified backup instead';
END
$$;
```

계정 identity, wallet ledger, entitlement는 파괴적 rollback 대상으로 보지 않는다.

### 1-5. C:/Users/user/Desktop/Winters/Data/Account/AccountEconomyPolicy.json

2026-07-13 계획의 전체 정책 파일 중 이번 수직 슬라이스는 다음 값만 먼저 소비한다. 새 파일의 최종 body는 같은 계획의 레벨 커브와 경기 보상 정의를 합친 뒤 한 owner가 작성한다.

새 파일: `CONFIRM_NEEDED`

확정 값:

```json
{
  "schemaVersion": 1,
  "currencyCode": "RP",
  "startingBalance": 10000
}
```

확인 필요:

- 경기 종료 보상/레벨 커브를 같은 파일에 지금 포함할지 S028에서 확장할지 결정한다.
- 어떤 경우에도 Services 코드와 Client 코드에 `10000`을 중복 하드코딩하지 않는다.

### 1-6. C:/Users/user/Desktop/Winters/Data/Account/Store/ChampionProducts.json

새 파일: `CONFIRM_NEEDED`

현재 코드의 selectable catalog는 15종이 아니라 17종이다. 요청한 15종 UI를 맞추기 위해 1차 판매 목록은 아래 15종으로 고정하고, Kindred와 Lee Sin은 `NotListed`로 두는 것을 기본안으로 한다. 두 챔피언도 밴픽 선택과 플레이는 가능하고 검은 overlay를 표시하지 않는다.

```text
champion.ezreal
champion.fiora
champion.jax
champion.masteryi
champion.annie
champion.ashe
champion.yone
champion.irelia
champion.yasuo
champion.kalista
champion.sylas
champion.viego
champion.garen
champion.zed
champion.riven
```

최종 JSON의 각 상품은 다음 필드를 가진다.

```json
{
  "productKey": "store.champion.ezreal",
  "contentKey": "champion.ezreal",
  "displayName": "Ezreal",
  "priceRp": 0,
  "sortOrder": 0,
  "enabled": true,
  "stackable": false
}
```

`priceRp: 0`은 형식 예시일 뿐 실제 seed 값이 아니다. 구현 시점의 Riot 가격 기준일과 출처를 고정한 뒤 15행의 완전한 body를 작성한다. 가격은 Services seed/DB가 소유하며 Client에 하드코딩하지 않는다. 초상화 경로도 이 파일에 넣지 않고 `contentKey -> eChampion -> GetRosterChampionPortraitPath()`로 기존 ClientPublic visual pack과 결합한다.

### 1-7. C:/Users/user/Desktop/Winters/Services/pkg/auth/jwt.go

`type TokenPair struct`를 아래 구조로 교체하여 Client가 email이 아니라 canonical UUID를 세션에 저장하도록 한다.

기존 코드:

```go
type TokenPair struct {
	AccessToken  string `json:"access_token"`
	RefreshToken string `json:"refresh_token"`
	ExpiresAt    int64  `json:"expires_at"`
}
```

아래로 교체:

```go
type TokenPair struct {
	UserID       uuid.UUID `json:"user_id"`
	DisplayName  string    `json:"display_name"`
	AccessToken  string    `json:"access_token"`
	RefreshToken string    `json:"refresh_token"`
	ExpiresAt    int64     `json:"expires_at"`
}
```

`GenerateTokenPair`의 반환값에도 전달받은 `userId`, `username`을 채운다. JWT claims의 Subject/UserID는 현재 UUID 계약을 유지한다.

### 1-8. C:/Users/user/Desktop/Winters/Services/internal/auth/model.go

`type LoginRequest struct` 아래에 개발용 숫자 계정 요청을 추가한다.

아래에 추가:

```go
type DevSlotRequest struct {
	Slot int `json:"slot"`
}
```

허용 범위는 service에서 `1 <= Slot && Slot <= 5`로 다시 검증한다. 숫자 계정은 실제 보안 로그인으로 간주하지 않고 local feature validation용 identity adapter로 명시한다.

social/dev 계정의 nullable email/password를 조회할 때 현재 `string` scan이 실패하지 않도록 repository의 `FindByID`는 `COALESCE(email, '')`, `COALESCE(password, '')`를 사용한다. 일반 password login은 빈 password hash를 무조건 Unauthorized로 처리한다.

### 1-9. C:/Users/user/Desktop/Winters/Services/internal/auth/repository.go

`CreateUserWithWalletAndStats`를 범용 계정 transaction으로 정리하고, 다음 두 경로를 추가한다.

아래에 추가할 공개 메서드:

```go
func (r *Repository) CreateDevSlotAccount(ctx context.Context, slot int, startingBalance int64) (*User, error)
func (r *Repository) FindByIdentity(ctx context.Context, provider, providerSubject string) (*User, error)
```

`CreateDevSlotAccount` transaction은 다음 순서를 한 commit으로 수행한다.

```text
1. provider='dev_slot', provider_subject='<1..5>'에 대해 transaction advisory lock 획득
2. identity가 이미 있으면 ErrAlreadyExists
3. users UUID 생성(username='Dev Slot N', email/password NULL)
4. user_identities 삽입
5. wallets(balance=AccountEconomyPolicy.startingBalance, currency_code='RP') 삽입
6. player_stats 기본 행 삽입
7. account_progression(level=1,total_xp=0) 삽입
8. coin_transactions initial_grant 행 삽입
9. commit
```

unique conflict를 일반 500으로 숨기지 말고 `ErrAlreadyExists`로 변환한다. Login 경로는 identity를 조회할 뿐 계정이나 RP를 새로 만들지 않는다.

기존 `CreateUserWithWalletAndStats`도 local 계정 생성 시 `(local_email, lower(email))` identity, 10,000 RP wallet, `initial_grant` ledger를 같은 transaction에서 만들도록 교체한다. normal register와 dev register가 서로 다른 초기 잔액 경로를 가져서는 안 된다.

### 1-10. C:/Users/user/Desktop/Winters/Services/internal/auth/service.go

`Login` 아래에 회원가입과 로그인을 분리한 다음 메서드를 추가한다. 하나의 `없으면 생성` 버튼으로 합치면 숫자 오입력으로 새 계정이 만들어지므로 사용하지 않는다.

아래에 추가:

```go
func (s *Service) RegisterDevSlot(ctx context.Context, req DevSlotRequest) (*jwtauth.TokenPair, error)
func (s *Service) LoginDevSlot(ctx context.Context, req DevSlotRequest) (*jwtauth.TokenPair, error)
```

두 메서드는 `WINTERS_DEV_AUTH_ENABLED=true`일 때만 동작한다. Register는 이미 존재하면 409, Login은 존재하지 않으면 404를 반환한다. 성공 시 둘 다 내부 UUID와 표시명을 포함한 TokenPair를 발급한다.

### 1-11. C:/Users/user/Desktop/Winters/Services/internal/auth/handler.go

`Routes()`의 `/login` 아래에 다음 route를 추가한다.

기존 코드:

```go
	r.Post("/login", h.Login)
```

아래에 추가:

```go
	r.Post("/dev/register", h.RegisterDevSlot)
	r.Post("/dev/login", h.LoginDevSlot)
```

handler는 JSON body를 `DevSlotRequest`로 decode한 뒤 service를 호출한다. 응답 계약은 기존 response wrapper 안의 다음 data다.

```json
{
  "user_id": "internal-uuid",
  "display_name": "Dev Slot 1",
  "access_token": "...",
  "refresh_token": "...",
  "expires_at": 0
}
```

실제 Google/Kakao 로그인은 후속 S028에서 Authorization Code + PKCE로 provider token을 Services가 교환·검증하고, 검증된 `provider_subject`를 같은 `user_identities`에 넣는다. Client가 provider access token이나 임의 subject를 신뢰 데이터로 제출하는 구조는 금지한다.

### 1-12. C:/Users/user/Desktop/Winters/Services/internal/profile/handler.go

현재 `/profile/{user_id}`가 URL의 UUID를 받는 경로는 다른 계정 조회 용도와 분리한다. 자신의 초기 동기화에는 JWT claims만 사용하는 `/profile/me`를 추가한다.

아래에 추가할 route와 규칙:

```go
r.Get("/me", h.GetMyProfile)
```

`GetMyProfile`은 `middleware.GetClaims(r.Context()).UserID`만 repository에 전달하고 Client가 보낸 UUID를 사용하지 않는다. 응답에는 `user_id`, `username`, `level`, `total_xp`, `wins`, `losses`, `mmr`를 포함한다.

### 1-13. C:/Users/user/Desktop/Winters/Services/internal/shop/model.go

`ShopItem`, `PurchaseRequest`, `PurchaseResponse`를 storefront snapshot에 필요한 안정적인 키와 멱등 키를 포함하도록 확장한다.

아래에 추가할 필드/타입:

```go
type StorefrontItem struct {
	ItemID      uuid.UUID `json:"item_id"`
	ProductKey  string    `json:"product_key"`
	ContentKey  string    `json:"content_key"`
	DisplayName string    `json:"display_name"`
	PriceRP     int64     `json:"price_rp"`
	Owned       bool      `json:"owned"`
	SortOrder   int       `json:"sort_order"`
}

type StorefrontResponse struct {
	CurrencyCode string           `json:"currency_code"`
	BalanceRP    int64            `json:"balance_rp"`
	Items        []StorefrontItem `json:"items"`
}

type PurchaseRequest struct {
	ItemID     uuid.UUID `json:"item_id"`
	RequestKey string    `json:"request_key"`
}
```

### 1-14. C:/Users/user/Desktop/Winters/Services/internal/shop/handler.go

`Routes()`를 JWT UUID 기반 self-service API로 확장한다.

아래에 추가/교체:

```go
	r.Get("/storefront", h.GetStorefront)
	r.Post("/purchase", h.Purchase)
```

`GetStorefront`와 `Purchase`는 모두 claims의 `UserID`만 사용한다. `/inventory/{user_id}`는 이번 Client 초기 동기화에서 더 이상 호출하지 않는다. 관리/디버그 용도로 남길 경우에도 본인 또는 관리자 권한 검증 없이는 다른 사용자의 inventory를 반환하지 않도록 막는다.

### 1-15. C:/Users/user/Desktop/Winters/Services/internal/shop/repository.go

`ListItems` + `GetInventory`를 Client에서 조합하는 경로 대신 다음 snapshot 조회를 추가한다.

아래에 추가할 공개 메서드:

```go
func (r *Repository) GetStorefront(ctx context.Context, userID uuid.UUID) (*StorefrontResponse, error)
func (r *Repository) PurchaseChampion(ctx context.Context, userID, itemID uuid.UUID, requestKey string) (*PurchaseResponse, error)
```

`GetStorefront`는 wallet balance와 활성 champion 상품 15종을 inventory에 LEFT JOIN하여 한 계정의 일관된 view를 반환한다.

`PurchaseChampion`은 transaction 안에서 다음을 보장한다.

```text
- requestKey receipt가 있으면 기존 결과 반환
- item_type='champion', enabled, is_stackable=false 확인
- 이미 보유하면 RP를 차감하지 않고 already_owned 반환
- wallet row FOR UPDATE
- 잔액 검증 및 차감
- inventory insert
- coin_transactions purchase ledger insert
- purchase_receipts insert
- commit 후 remaining_rp와 owned=true 반환
```

현재 구현의 `ON CONFLICT ... quantity + 1`은 챔피언에 사용할 수 없다. 중복 구매 시 수량 증가와 중복 과금을 모두 금지한다.

### 1-16. C:/Users/user/Desktop/Winters/Services/Makefile

현재 migration 명령은 Docker PostgreSQL의 host port `5433`을 전달하지 않고 모든 up SQL을 매번 재실행한다. 구현 전 최소한 포트를 수정하고, 이미 적용한 migration을 다시 실행하지 않는 migration ledger/runner를 선택한다.

기존 코드:

```make
		PGPASSWORD=winters_dev_2026 psql -h localhost -U winters -d winters -f $$f; \
```

아래로 교체:

```make
		PGPASSWORD=winters_dev_2026 psql -h localhost -p 5433 -U winters -d winters -f $$f; \
```

단, 이 포트 수정만으로 반복 실행 안전성이 생기지는 않는다. 1차 로컬 검증에서는 새 migration 한 개만 명시적으로 적용하고, 정식 운영 전에는 `schema_migrations`를 가진 runner로 교체한다.

### 1-17. C:/Users/user/Desktop/Winters/Client/Public/Network/Backend/AuthClient.h

`AuthResult`에 canonical 계정 정보를 추가한다.

기존 코드:

```cpp
	bool_t  success      = false;
	string  accessToken;
```

아래로 교체:

```cpp
	bool_t  success      = false;
	string  userID;
	string  displayName;
	string  accessToken;
```

`Login` 선언 아래에 다음 개발용 API를 추가한다.

아래에 추가:

```cpp
	void RegisterDevSlot(i32_t iSlot, AuthCallback callback);
	void LoginDevSlot(i32_t iSlot, AuthCallback callback);
```

### 1-18. C:/Users/user/Desktop/Winters/Client/Private/Network/Backend/AuthClient.cpp

`ParseAuthResponse`에서 `user_id`, `display_name`을 파싱하고, 두 개발용 메서드가 각각 `/auth/dev/register`, `/auth/dev/login`에 `{ "slot": N }`을 POST하도록 추가한다.

`CHttpClient.cpp/.h`는 현재 다른 세션의 async lifetime 변경이 존재하므로 이 packet에서 수정하지 않는다. 기존 `AsyncPost`, `ProcessCallbacks`, `SetAuthToken` 공개 계약만 소비한다.

### 1-19. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_Login.h

email/password 기존 필드는 일반 계정 탭용으로 유지하고, 숫자 개발 계정 상태를 추가한다.

`m_szPassword` 아래에 추가:

```cpp
	i32_t m_iDevSlot = 1;
	bool_t m_bRegisterInFlight = false;
```

private 메서드에 다음을 추가한다.

```cpp
	void RequestDevSlotRegister();
	void RequestDevSlotLogin();
	void HandleAuthenticatedResult(const Client::AuthResult& result);
```

### 1-20. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_Login.cpp

현재 `OnUpdate`의 이미지 화살표 클릭이 무조건 `RequestOfflineLogin()`을 호출하는 경로를 제거한다. `--banpick-smoke`의 명시적 자동 오프라인 경로는 회귀 스모크를 위해 유지한다.

기존 코드:

```cpp
	if (!m_bLoginInFlight && m_ImageUI.WasSourceRectClicked(kLoginArrowRect))
		RequestOfflineLogin();
```

아래로 교체:

```cpp
	if (!m_bLoginInFlight && m_ImageUI.WasSourceRectClicked(kLoginArrowRect))
		m_strStatus = "Choose Register or Login in the account panel";
```

`OnImGui()`는 기능 검증용 창으로 다음 세 탭을 표시한다.

```text
개발 계정
  - InputInt: 1..5
  - 회원가입 버튼: 존재하면 409 표시
  - 로그인 버튼: 없으면 404 표시
  - Debug 빌드 전용 Offline Smoke 버튼

일반 계정
  - 기존 email/password 입력
  - 기존 Login, 추후 Register 연결

소셜 로그인
  - Google/Kakao는 S028 예정 안내
```

온라인 성공 처리의 기존 `SetAuthenticatedAccount(email, email, token)`을 아래 의미로 교체한다.

```cpp
	CClientShellSession::Instance().SetAuthenticatedAccount(
		result.userID,
		result.displayName,
		result.accessToken);
```

1차 수직 슬라이스는 EXE를 다시 켤 때 숫자를 다시 입력하는 방식으로 영속성을 검증한다. refresh token 자동 저장은 하지 않는다. 후속 자동 로그인은 Windows Credential Manager/DPAPI에 refresh token만 저장하고 access token은 메모리에 둔다.

### 1-21. C:/Users/user/Desktop/Winters/Client/Public/ClientShell/ClientShellTypes.h

`ShellStoreItem` 위에 상점/밴픽이 공유할 표현 상태를 추가한다.

아래에 추가:

```cpp
enum class eShellCommerceState : u32_t
{
	SyncPending,
	NotListed,
	UnownedListed,
	Owned,
};
```

`ShellStoreItem`에 안정적인 commerce key와 상태를 추가한다.

아래에 추가:

```cpp
	std::string strProductKey{};
	std::string strContentKey{};
	eShellCommerceState eCommerceState = eShellCommerceState::SyncPending;
```

DB의 item UUID는 구매 request용이고, `contentKey`는 챔피언/초상화 연결용이다. `eChampion` 정수값이나 표시명을 DB identity로 쓰지 않는다.

### 1-22. C:/Users/user/Desktop/Winters/Client/Public/ClientShell/ClientShellDataStore.h

초기 동기화 상태와 content key 조회 API를 추가한다.

아래에 추가:

```cpp
	bool_t IsInitialSyncReady() const { return m_bInitialSyncReady; }
	eShellCommerceState GetChampionCommerceState(const std::string& strContentKey) const;
	const ShellStoreItem* FindStoreItemByContentKey(const std::string& strContentKey) const;
```

private에 다음을 추가한다.

```cpp
	bool_t m_bInitialSyncReady = false;
```

### 1-23. C:/Users/user/Desktop/Winters/Client/Private/ClientShell/ClientShellDataStore.cpp

온라인 세션에 가짜 level 30/RP 1350/skin 목록이 섞이지 않도록 초기화 경계를 분리한다.

- `Reset()`은 profile/store/readiness를 모두 비운다.
- `SeedOfflineDefaults()`는 `session.IsOfflineAccount()`일 때만 허용한다.
- storefront snapshot 적용 시 RP와 상품 목록을 통째로 교체하고 마지막에 `m_bInitialSyncReady=true`로 바꾼다.
- 15종 상품에 없는 현재 playable 챔피언은 조회 결과 `NotListed`다.
- storefront 요청 실패/미완료는 `SyncPending`이며 미보유로 간주하지 않는다.
- 구매 성공 시 balance와 해당 `contentKey`의 상태를 `Owned`로 갱신한다.

### 1-24. C:/Users/user/Desktop/Winters/Client/Public/Network/Backend/CShopClient.h

기존 items/inventory 3요청 조합 대신 한 번에 일관된 snapshot을 받는 storefront DTO와 메서드를 추가한다.

아래에 추가:

```cpp
struct StorefrontData
{
	string currencyCode;
	i64_t balanceRP = 0;
	vector<ShopItemData> items;
	string error;
};

using StorefrontCallback = function<void(const StorefrontData&)>;

void GetStorefront(StorefrontCallback callback);
```

`ShopItemData`에는 `productKey`, `contentKey`, `owned`, `sortOrder`를 추가하고 `Purchase` 요청에는 새 UUID `request_key`를 포함한다.

### 1-25. C:/Users/user/Desktop/Winters/Client/Private/Network/Backend/CShopClient.cpp

`GetStorefront`는 JWT가 설정된 HTTP client로 `/shop/storefront`를 GET하고 RP/15개 상품/owned 상태를 한 응답에서 파싱한다. Purchase success는 `remaining_rp`, `status`, `owned`를 파싱한다.

이 파일도 `CHttpClient` 내부 구현은 수정하지 않고 공개 API만 소비한다.

### 1-26. C:/Users/user/Desktop/Winters/Client/Public/ClientShell/ClientShellBackendService.h

UI가 초기 sync와 구매 중 상태를 안전하게 표시하도록 다음 query를 추가한다.

아래에 추가:

```cpp
	bool_t IsInitialSyncReady() const;
	bool_t IsPurchaseInFlight() const { return m_bPurchaseRequestInFlight; }
```

기존 profile/items/inventory 개별 in-flight 상태는 storefront 전환 후 정리하되, 이 변경으로 사용되지 않게 된 필드만 삭제한다.

### 1-27. C:/Users/user/Desktop/Winters/Client/Private/ClientShell/ClientShellBackendService.cpp

`RequestInitialSync()`를 JWT claims 기반 `/profile/me`와 `/shop/storefront` 호출로 바꾼다. storefront 하나가 RP·상품·보유를 원자적 snapshot으로 제공하므로 items와 inventory의 callback 순서 race를 제거한다.

`ConfigureFromSession()`은 같은 user/token으로 다시 호출될 때 진행 중 callback을 파괴하지 않도록 idempotent하게 만든다. generation guard는 유지한다.

### 1-28. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_MainMenu.h

새 `Scene_Shop`을 만들지 않고 MainMenu 내부에 임시 full-screen ImGui 상점 mode를 추가한다. 이 선택은 현재 dirty인 `Client.vcxproj/.filters`와 scene 등록부를 건드리지 않기 위한 1차 구현 경계다.

private에 다음 상태와 메서드를 추가한다.

```cpp
	void RenderChampionStoreImGui();
	void RequestStorePurchase(const ShellStoreItem& item);
	bool_t m_bChampionStoreOpen = false;
```

위 signature를 위해 `ClientShell/ClientShellTypes.h`를 직접 include한다.

정확한 portrait texture 소유 타입과 native SRV wrapper는 S026 시작 시 최신 `CTexture`/ImGui 1.92 호출부를 다시 확인한다. 새 asset이나 Engine 수정은 하지 않는다.

### 1-29. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_MainMenu.cpp

`OnEnter()`의 무조건적인 offline seed를 교체한다.

기존 코드:

```cpp
	CClientShellDataStore::Instance().SeedOfflineDefaults(CClientShellSession::Instance());
```

아래로 교체:

```cpp
	CClientShellDataStore::Instance().Reset();
	if (CClientShellSession::Instance().IsOfflineAccount())
		CClientShellDataStore::Instance().SeedOfflineDefaults(CClientShellSession::Instance());
```

`OnImGui()`는 다음 동작을 구현한다.

```text
MainMenu mode
  - 상점 버튼

Champion Store mode
  - 상단: 뒤로 가기, 현재 RP, sync/purchase status
  - 탭: 챔피언
  - BeginChild 세로 스크롤
  - 15개 정사각형 portrait, 5열 x 3행
  - 미보유: 이름, RP 가격, 구매 버튼
  - 보유: portrait 위 반투명 검정 overlay + "이미 구매한 상품입니다"
  - sync pending: 구매 버튼 비활성화, loading 표시
```

상점 mode에서도 Scene이 바뀌지 않으므로 기존 `OnUpdate()`의 `ProcessCallbacks()`가 계속 실행된다. 구매 중 뒤로 가기를 눌러도 callback owner가 파괴되지 않으며 완료 후 DataStore가 갱신된다.

ImGui는 이번 기능 검증용 임시 UI다. 기능이 안정된 뒤 별도 Shop scene 또는 Engine runtime UI로 옮길 때 MainMenu mode를 삭제한다. 두 UI 경로를 장기간 병행하지 않는다.

`OnUpdate()`의 `kGameStartRect` click은 `!m_bChampionStoreOpen`일 때만 처리하여 상점 위로 가려진 MainMenu hit area가 게임 시작을 발생시키지 않게 한다.

### 1-30. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_BanPick.h

현재 다른 세션 변경이 존재하므로 S027 Handoff 전에는 수정하지 않는다. commerce 상태를 셀에 복사해 캐시하지 않고 렌더 시 DataStore를 조회한다. 구매/재동기화 직후 화면이 바로 바뀌어야 하기 때문이다.

`CONFIRM_NEEDED`:

- S027 시작 시 현재 header diff를 다시 읽고 정말 새 member가 필요한지 확인한다.
- 가능하면 header는 변경하지 않고 cpp의 presentation query만으로 끝낸다.

### 1-31. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_BanPick.cpp

현재 dirty인 `RenderChampionGridAndRosterOverlay()`의 champion cell loop에서 portrait를 그린 직후, selection highlight 전에 commerce overlay만 추가한다.

적용 규칙:

```text
Owned          -> overlay 없음
UnownedListed  -> 검정 alpha 약 0.55 overlay
NotListed      -> overlay 없음
SyncPending    -> 소유권 overlay 없음; 필요하면 작은 동기화 표시만 사용
```

`ResolveClickedChampion`, local/server select input, lobby command, Shared schema는 수정하지 않는다. 즉 미보유 챔피언도 실제 선택/플레이 가능하다는 요구를 그대로 지킨다.

### 1-32. C:/Users/user/Desktop/Winters/Client/Include/Client.vcxproj

1차 MainMenu ImGui 상점 mode에서는 변경하지 않는다. 2026-07-13 계획의 `Scene_Shop.cpp` 신규 등록 지시는 이번 문서가 대체한다.

### 1-33. C:/Users/user/Desktop/Winters/Client/Include/Client.vcxproj.filters

1차 MainMenu ImGui 상점 mode에서는 변경하지 않는다. 현재 Always-Lock이자 다른 세션의 대규모 변경 파일이므로 불필요한 merge를 만들지 않는다.

### 1-34. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

이번 숫자 로그인/RP/챔피언 상점 수직 슬라이스에서는 변경하지 않는다. S024 안정성 작업과 직접 충돌한다.

경기 종료 RP/XP는 후속 S028에서 다음 별도 권위 흐름으로 연결한다.

```text
Authenticated Match Ticket
-> Server GameEnded
-> trusted internal match result
-> Services idempotent reward receipt
-> wallet/progression transaction
-> Client result refetch
```

Client가 승패나 보상량을 직접 제출하게 만들지 않는다.

### 1-35. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ItemDef.h

이번 슬라이스에서는 변경하지 않는다. 인게임 아이템 상점의 권위 데이터는 ServerPrivate/Shared GameSim 방향을 유지하며, 계정 RP 상점 API에서 내려주는 champion 상품과 섞지 않는다.

후속 인게임 상점 구현은 다음 흐름을 사용한다.

```text
Client BuyItem intent
-> GameCommand
-> Server/GameSim gold/shop-range/slot/recipe validation
-> authoritative inventory mutation
-> Snapshot/Event
-> Client icon/description presentation
```

### 1-36. C:/Users/user/Desktop/Winters/Services/pkg/config/config.go

Auth 설정에 개발 숫자 계정 endpoint 활성화 여부를 추가한다. service가 `os.Getenv`를 직접 읽지 않고 config를 constructor로 주입받게 한다.

아래에 추가할 설정:

```go
DevAuthEnabled bool
```

환경 변수 `WINTERS_DEV_AUTH_ENABLED`의 기본값은 `false`다. 명시적으로 `true`인 로컬 개발 환경에서만 `/auth/dev/*`가 활성화된다.

### 1-37. C:/Users/user/Desktop/Winters/Services/.env.example

기존 DB/서비스 환경 변수 아래에 다음을 추가한다.

아래에 추가:

```dotenv
WINTERS_DEV_AUTH_ENABLED=false
```

실제 로컬 `.env`만 `true`로 바꾸고 source control의 기본값은 계속 `false`로 유지한다.

### 1-38. C:/Users/user/Desktop/Winters/Services/cmd/auth/main.go

`auth.NewService(repo, jwtMgr)` 호출에 `cfg.DevAuthEnabled`와 account economy policy를 주입한다. policy load/validate가 실패하면 Auth service를 시작하지 않는다.

기존 코드:

```go
	svc := auth.NewService(repo, jwtMgr)
```

아래 의미로 교체:

```go
	svc := auth.NewService(repo, jwtMgr, accountPolicy, cfg.DevAuthEnabled)
```

정확한 policy loader package와 constructor signature는 2026-07-13 계획의 `Services/pkg/accountpolicy/policy.go` 구현과 함께 고정한다.

### 1-39. C:/Users/user/Desktop/Winters/Client/Public/Network/Backend/ProfileClient.h

`ProfileData`에 `/profile/me`가 반환하는 계정 진행도를 추가한다.

아래에 추가:

```cpp
	i32_t level = 1;
	i64_t totalXP = 0;
```

Client가 임의 user ID를 전달하지 않는 다음 메서드를 추가한다.

아래에 추가:

```cpp
	void GetMyProfile(ProfileCallback callback);
```

### 1-40. C:/Users/user/Desktop/Winters/Client/Private/Network/Backend/ProfileClient.cpp

`GetMyProfile`은 `/profile/me`를 GET하고 `user_id`, `username`, `level`, `total_xp`, `wins`, `losses`, `mmr`를 파싱한다. 초기 로그인 복원 경로에서는 기존 `GetProfile(userId, ...)`를 호출하지 않는다.

### 1-41. C:/Users/user/Desktop/Winters/Services/docker-compose.yml

1차 수직 슬라이스에서는 변경하지 않는다. PostgreSQL/Redis/Kafka는 Docker, Go 서비스는 host debugger에서 실행하는 현재 구성을 사용한다. 이 경계가 안정된 뒤 Auth/Profile/Shop을 compose service로 추가하여 one-command 개발 환경으로 승격한다.

지금 즉시 Go service까지 compose에 넣으면 코드 hot reload/debug와 서비스 dependency 문제를 동시에 확장하므로 S025 완료 조건에는 포함하지 않는다.

## 2. 검증

사전 상태 확인:

- 2026-07-14 감사 시점에 Docker Desktop daemon은 실행 중이 아니었고 `5433`, `6379`, `9092`, `8081`, `8084`, `8086` listener도 없었다.
- `Services/docker-compose.yml`은 PostgreSQL, Redis, Kafka만 실행한다. Go Auth/Profile/Shop은 별도 프로세스로 실행해야 한다.
- PostgreSQL host port는 `5433`; container 내부는 `5432`다.
- `docker compose down`은 named `pgdata`를 보존하지만 `docker compose down -v`는 계정·RP·구매 이력을 삭제하므로 금지한다.
- 현재 `go test ./...`는 통과하지만 테스트 파일이 없어 `[no test files]` 상태다. 기능 완료 기준으로 보지 않는다.

권장 로컬 기동 순서:

```powershell
Set-Location C:\Users\user\Desktop\Winters\Services
docker compose up -d
docker compose ps

$env:DB_PORT = '5433'
$env:WINTERS_DEV_AUTH_ENABLED = 'true'
go run ./cmd/auth
go run ./cmd/profile
go run ./cmd/shop
```

각 `go run`은 별도 terminal에서 실행한다. 로그인/상점 API만 검증할 때 C++ Game Server는 필요하지 않다. 밴픽과 실제 게임 진입을 검증할 때만 Winters Server를 추가로 실행한다.

Backend 자동 검증:

```text
1. slot 1 Register -> 201, UUID-A, RP 10,000
2. slot 1 Register 재시도 -> 409, users/wallet/initial_grant 증가 없음
3. slot 1 Login -> 200, 동일 UUID-A
4. slot 2 Register/Login -> UUID-B, slot 1과 wallet/inventory 완전 분리
5. slot 0, 6, 음수, 문자열 -> 400
6. 존재하지 않는 slot Login -> 404
7. slot 1 동시 최초 Register -> identity/users/wallet/progression 각 1행
8. JWT-A로 /profile/me와 /shop/storefront -> A 데이터만 반환
9. 챔피언 구매 -> RP 차감 + entitlement + ledger + receipt 한 transaction
10. 같은 request_key 재전송 -> 추가 차감 없음
11. 다른 request_key로 보유 챔피언 재구매 -> already_owned, 추가 차감 없음
12. 잔액 부족 -> entitlement/ledger/잔액 모두 변화 없음
```

DB 복원 확인 query:

```sql
SELECT
    ui.provider,
    ui.provider_subject,
    u.id AS user_id,
    w.balance,
    w.currency_code
FROM user_identities ui
JOIN users u ON u.id = ui.user_id
JOIN wallets w ON w.user_id = u.id
WHERE ui.provider = 'dev_slot'
ORDER BY ui.provider_subject;
```

필수 Client E2E:

```text
1. Client 실행 -> 개발 계정 1 회원가입/로그인
2. MainMenu 초기 sync 완료 -> RP 10,000과 15개 상품 표시
3. Ezreal 구매 -> RP 즉시 차감, 해당 카드에 "이미 구매한 상품입니다"
4. 밴픽 진입 -> Ezreal은 overlay 없음, 판매 중 미보유 챔피언은 검정 overlay
5. overlay가 있는 챔피언도 클릭/선택/게임 진입 가능
6. Client EXE 종료
7. Client 재실행 -> 개발 계정 1 로그인
8. 동일 UUID, 차감된 RP, Ezreal 보유 상태 복원
9. 개발 계정 2 로그인 -> RP 10,000, Ezreal 미보유로 분리
10. Auth/Profile/Shop 중 하나가 꺼진 상태 -> SyncPending이지 전 챔피언 미보유 처리 아님
```

사회 로그인 후속 검증:

- provider가 발급한 immutable subject가 같은 경우에만 같은 UUID를 복원한다.
- 같은 email의 다른 provider를 자동 병합하지 않는다.
- 로그인된 계정의 link flow 없이 `(provider, subject)`를 다른 UUID로 옮기지 않는다.
- provider code/token exchange와 검증은 Services에서 수행한다.

충돌/빌드 검증:

- S025는 Services만 수정하므로 S024와 병행할 수 있다.
- S026은 CHttpClient Handoff 이후 시작한다. CHttpClient 자체는 수정하지 않는다.
- S027은 Scene_BanPick Handoff 이후 시작하고 기존 server-required/local-smoke 변경을 보존한다.
- `Client.vcxproj`, `.filters`, `LobbyRosterHelpers`, schemas/generated, Server/GameRoom, ReplayRecorder는 이번 수직 슬라이스에서 수정하지 않는다.
- S024가 빌드 중이면 다른 세션은 MSBuild를 실행하지 않는다. 최종 빌드는 owner 한 명이 `/m:1`로 실행한다.

검증 명령:

```powershell
Set-Location C:\Users\user\Desktop\Winters\Services
docker compose config
go test ./...

Set-Location C:\Users\user\Desktop\Winters
git diff --check
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m:1
```

완료 기준:

- 숫자 계정 1~5가 각각 불변 내부 UUID에 매핑된다.
- EXE 종료 후 같은 숫자로 로그인하면 RP와 구매 이력이 PostgreSQL에서 복원된다.
- 온라인 계정에 offline RP 1350/level 30 seed가 섞이지 않는다.
- 상점은 15종 portrait, 스크롤, 뒤로 가기, RP, 구매/보유 표시를 제공한다.
- 밴픽 검은 overlay는 `UnownedListed`에만 나타나고 선택 가능 여부를 바꾸지 않는다.
- 다른 계정의 profile/inventory를 URL UUID 조작으로 읽을 수 없다.
- 중복 구매·중복 request·동시 최초 회원가입에도 RP/entitlement가 정확히 한 번만 변경된다.
- S024와 기존 dirty Client 변경을 보존한 상태로 Debug x64 빌드가 통과한다.
