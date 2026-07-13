Session - Winters의 소셜 계정, RP, 계정 레벨, 챔피언 상품/소유권, 경기 종료 보상과 인게임 아이템 상점을 서버 권위 경계에 맞춰 연결한다.
Session - 초기 RP는 10,000, 밴픽 미보유 챔피언은 검은 오버레이만 표시하고 선택은 허용한다. 구현 예산은 기반/정합성 70%, 플레이 가능한 상점·결과 UI 30%로 고정하고 1차 외부 시연 마감은 2026-07-31로 제안한다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/.md/architecture/WINTERS_DATA_ARCHITECTURE.md

`## 1. 데이터 소유권 매트릭스 (목표 = 현재의 방향)` 아래에 다음 경계를 추가한다.

아래에 추가:

```markdown
### Account Meta Economy와 Match Economy

| 수명 | 권위 소유자 | 포함 데이터 | 포함하면 안 되는 데이터 |
|---|---|---|---|
| 계정/메타 | Services + PostgreSQL | AccountID, social identity, RP, account XP/level, champion entitlement, purchase/reward ledger | 인게임 gold, 아이템 슬롯, 챔피언 전투 레벨 |
| 한 경기 | Server + Shared/GameSim | match gold, item inventory, item gameplay stats, shop access, champion combat XP/level | RP, 계정 레벨, social token |
| 표현 | Client + ClientPublic pack | 챔피언 초상화, 상품명, 아이콘, 설명, overlay, 결과 UI | 잔액 차감, 보상 확정, 구매 가능 판정 |

계정 상점 흐름은 `Client request -> JWT claims -> Shop transaction -> PostgreSQL ledger/entitlement -> response -> Client view state`이다.

경기 종료 보상 흐름은 `Authenticated match ticket -> Server GameEnded -> trusted internal result -> idempotent Services transaction -> RP/XP ledger -> Client refetch/result view`이다. Client가 경기 결과나 보상량을 직접 제출해서는 안 된다.

인게임 아이템 상점은 `Client BuyItem intent -> GameCommand -> Server/GameSim validation -> gold/inventory mutation -> Snapshot + purchase result Event -> Client UI`를 따른다. 서버는 가격/스탯/구매 가능 여부를 ServerPrivate item pack에서 읽고, Client는 ClientPublic 아이콘/이름/설명과 서버가 전달한 표시용 가격/카탈로그 revision만 결합한다.
```

### 1-2. C:/Users/user/Desktop/Winters/Services/migrations/000008_create_account_meta_economy.up.sql

새 파일:

```sql
ALTER TABLE users
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

CREATE INDEX idx_user_identities_user
    ON user_identities(user_id);

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
    CHECK (tx_type IN (
        'charge',
        'purchase',
        'refund',
        'initial_grant',
        'match_reward',
        'admin_grant'
    ));

ALTER TABLE coin_transactions
    ADD COLUMN source_kind VARCHAR(32),
    ADD COLUMN source_id VARCHAR(128),
    ADD COLUMN idempotency_key VARCHAR(128);

CREATE UNIQUE INDEX uq_coin_tx_user_idempotency
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

CREATE TABLE match_reward_receipts (
    id            UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    match_id      UUID NOT NULL,
    user_id       UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    result        VARCHAR(16) NOT NULL CHECK (result IN ('win', 'loss', 'draw', 'abandoned')),
    rp_awarded    BIGINT NOT NULL CHECK (rp_awarded >= 0),
    xp_awarded    BIGINT NOT NULL CHECK (xp_awarded >= 0),
    balance_after BIGINT NOT NULL CHECK (balance_after >= 0),
    level_after   INT NOT NULL CHECK (level_after >= 1),
    created_at    TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE(match_id, user_id)
);

CREATE UNIQUE INDEX uq_match_history_user_match
    ON match_history(user_id, match_id);

WITH granted AS (
    UPDATE wallets
       SET balance = 10000,
           updated_at = NOW()
     WHERE balance = 0
     RETURNING user_id, balance
)
INSERT INTO coin_transactions (
    user_id,
    amount,
    tx_type,
    reference,
    balance_after,
    source_kind,
    source_id,
    idempotency_key
)
SELECT
    user_id,
    10000,
    'initial_grant',
    'account-economy-v1',
    balance,
    'account_creation',
    user_id::text,
    'initial-grant-v1'
FROM granted;
```

`uq_match_history_user_match` 생성 전 다음 중복 검사를 수행하고 결과가 있으면 migration을 중단한다. 자동 삭제로 사용자 전적을 정리하지 않는다.

```sql
SELECT user_id, match_id, COUNT(*)
FROM match_history
GROUP BY user_id, match_id
HAVING COUNT(*) > 1;
```

### 1-3. C:/Users/user/Desktop/Winters/Services/migrations/000008_create_account_meta_economy.down.sql

금융 ledger와 social-only 계정을 자동 파괴하는 rollback을 금지한다. 백업 복원이나 명시적 데이터 이관 없이 내려가지 않도록 forward-only migration으로 둔다.

새 파일:

```sql
DO $$
BEGIN
    RAISE EXCEPTION
        '000008_create_account_meta_economy is forward-only; restore a verified database backup instead';
END
$$;
```

### 1-4. C:/Users/user/Desktop/Winters/Data/Account/AccountEconomyPolicy.json

계정 생성 RP, 경기 보상, 계정 레벨 커브의 단일 authoring source로 사용한다. Services는 시작 시 한 번 validate/load하고 요청마다 JSON을 다시 읽지 않는다.

새 파일:

```json
{
  "schemaVersion": 1,
  "currencyCode": "RP",
  "startingBalance": 10000,
  "matchRewards": {
    "completedRp": 50,
    "winBonusRp": 25,
    "lossBonusRp": 0,
    "completedXp": 100,
    "winBonusXp": 50,
    "lossBonusXp": 0
  },
  "levelThresholds": [
    { "level": 1, "totalXp": 0 },
    { "level": 2, "totalXp": 100 },
    { "level": 3, "totalXp": 250 },
    { "level": 4, "totalXp": 450 },
    { "level": 5, "totalXp": 700 },
    { "level": 6, "totalXp": 1000 },
    { "level": 7, "totalXp": 1350 },
    { "level": 8, "totalXp": 1750 },
    { "level": 9, "totalXp": 2200 },
    { "level": 10, "totalXp": 2700 },
    { "level": 11, "totalXp": 3250 },
    { "level": 12, "totalXp": 3850 },
    { "level": 13, "totalXp": 4500 },
    { "level": 14, "totalXp": 5200 },
    { "level": 15, "totalXp": 5950 },
    { "level": 16, "totalXp": 6750 },
    { "level": 17, "totalXp": 7600 },
    { "level": 18, "totalXp": 8500 },
    { "level": 19, "totalXp": 9450 },
    { "level": 20, "totalXp": 10450 },
    { "level": 21, "totalXp": 11500 },
    { "level": 22, "totalXp": 12600 },
    { "level": 23, "totalXp": 13750 },
    { "level": 24, "totalXp": 14950 },
    { "level": 25, "totalXp": 16200 },
    { "level": 26, "totalXp": 17500 },
    { "level": 27, "totalXp": 18850 },
    { "level": 28, "totalXp": 20250 },
    { "level": 29, "totalXp": 21700 },
    { "level": 30, "totalXp": 23200 }
  ]
}
```

### 1-5. C:/Users/user/Desktop/Winters/Data/Account/Store/ChampionProducts.json

새 파일: `CONFIRM_NEEDED`

확인 필요:

- 현재 playable 17종(`ezreal`, `fiora`, `jax`, `kindred`, `leesin`, `masteryi`, `annie`, `ashe`, `yone`, `irelia`, `yasuo`, `kalista`, `sylas`, `viego`, `garen`, `zed`, `riven`)을 모두 포함한다.
- 각 행은 `productKey`, `contentKey`, `displayName`, `priceRp`, `sortOrder`, `enabled`, `stackable=false`를 가진다.
- `productKey`는 `store.champion.ezreal`, `contentKey`는 `champion.ezreal` 형식의 안정적인 문자열로 고정한다. DB UUID나 `eChampion` 정수값을 콘텐츠 식별자로 쓰지 않는다.
- 정확한 LoL RP 가격은 구현 세션에서 가격 기준일과 출처를 먼저 고정한 뒤 전체 파일 body를 작성한다. 검증되지 않은 기억값을 계획서에 하드코딩하지 않는다.
- portrait 경로는 이 파일에 넣지 않는다. Client가 `contentKey -> eChampion -> GetRosterChampionPortraitPath()`로 ClientPublic visual pack과 결합한다.

### 1-6. C:/Users/user/Desktop/Winters/Services/internal/auth/repository.go

`CreateUserWithWalletAndStats` 안의 다음 기존 코드를 교체한다.

기존 코드:

```go
	_, err = tx.Exec(ctx, `INSERT INTO wallets (user_id, balance) VALUES ($1, 0)`, user.ID)
	if err != nil {
		return fmt.Errorf("insert wallet: %w", err)
	}

	_, err = tx.Exec(ctx, `INSERT INTO player_stats (user_id, mmr) VALUES ($1, 1000)`, user.ID)
	if err != nil {
		return fmt.Errorf("insert player_stats: %w", err)
	}
```

아래로 교체:

```go
	var startingBalance int64
	err = tx.QueryRow(ctx,
		`INSERT INTO wallets (user_id, balance, currency_code)
		 VALUES ($1, $2, 'RP') RETURNING balance`,
		user.ID, r.startingBalance,
	).Scan(&startingBalance)
	if err != nil {
		return fmt.Errorf("insert wallet: %w", err)
	}

	_, err = tx.Exec(ctx,
		`INSERT INTO coin_transactions
		 (user_id, amount, tx_type, reference, balance_after, source_kind, source_id, idempotency_key)
		 VALUES ($1, $2, 'initial_grant', 'account-creation', $2,
		         'account_creation', $1::text, 'initial-grant-v1')`,
		user.ID, startingBalance)
	if err != nil {
		return fmt.Errorf("insert initial RP ledger: %w", err)
	}

	_, err = tx.Exec(ctx, `INSERT INTO account_progression (user_id) VALUES ($1)`, user.ID)
	if err != nil {
		return fmt.Errorf("insert account progression: %w", err)
	}

	_, err = tx.Exec(ctx, `INSERT INTO player_stats (user_id, mmr) VALUES ($1, 1000)`, user.ID)
	if err != nil {
		return fmt.Errorf("insert player_stats: %w", err)
	}
```

`Repository`와 `NewRepository`에는 validated `startingBalance`를 주입한다. DB default나 Client seed가 정책의 주인이 되지 않게 한다.

### 1-7. C:/Users/user/Desktop/Winters/Services/pkg/auth/jwt.go

`type TokenPair struct`를 다음으로 교체하고 `GenerateTokenPair` 반환 시 `UserID`, `Username`을 함께 채운다. 이 변경으로 Client가 이메일을 UUID 자리에 넣는 현재 오류를 제거한다.

아래로 교체:

```go
type TokenPair struct {
	AccessToken  string    `json:"access_token"`
	RefreshToken string    `json:"refresh_token"`
	ExpiresAt    int64     `json:"expires_at"`
	UserID       uuid.UUID `json:"user_id"`
	Username     string    `json:"username"`
}
```

`GenerateTokenPair`의 반환 literal에 아래를 추가한다.

아래에 추가:

```go
		UserID:       userId,
		Username:     username,
```

### 1-8. C:/Users/user/Desktop/Winters/Services/internal/auth/model.go

`User.Password`는 social-only 계정을 표현할 수 있도록 nullable password hash로 바꾸고, provider-neutral identity 계약을 추가한다.

기존 코드:

```go
	Password  string    `json:"-"`
```

아래로 교체:

```go
	PasswordHash *string `json:"-"`
```

`type LogoutRequest struct` 아래에 추가:

```go
type SocialIdentity struct {
	Provider        string
	ProviderSubject string
	ProviderEmail   string
	DisplayName     string
}

type SocialLoginStartResponse struct {
	AuthorizationURL string `json:"authorization_url"`
	State             string `json:"state"`
}
```

기존 email/password 가입/로그인 흐름은 유지하되 null hash 계정에는 password 로그인을 허용하지 않는다.

### 1-9. C:/Users/user/Desktop/Winters/Services/internal/auth/handler.go

`Routes()`에 self-resource와 provider-neutral social route를 추가한다.

기존 코드:

```go
	r.Post("/logout", h.Logout)
```

아래에 추가:

```go
	r.Get("/me", h.Me)
	r.Get("/social/{provider}/start", h.StartSocialLogin)
	r.Get("/social/{provider}/callback", h.CompleteSocialLogin)
```

`Me`는 middleware claims의 `UserID`만 사용한다. Social callback은 provider token을 Client에 전달하지 않고, backend가 identity를 upsert한 뒤 Winters access/refresh token 또는 1회용 login code만 반환한다.

실제 Google/Kakao/Naver provider adapter 새 파일: `CONFIRM_NEEDED`

확인 필요:

- 첫 수직 슬라이스는 `provider=dev`로 동일 DB linking/upsert/token 발급을 검증한다.
- 실제 provider는 하나를 선택한 뒤 Authorization Code + PKCE, redirect URI, secret 보관 위치, 약관을 확정한다.
- 동일 `(provider, provider_subject)`는 항상 동일 `users.id`로 귀속하고, 이메일 문자열만으로 계정을 자동 병합하지 않는다.

### 1-10. C:/Users/user/Desktop/Winters/Services/internal/profile/model.go

`type Profile struct`의 `Username` 아래에 계정 진행도 read model을 추가한다. RP는 Shop storefront가 단일 read owner가 되며 Profile cache에 복제하지 않는다.

아래에 추가:

```go
	AccountLevel int   `json:"account_level"`
	AccountXP    int64 `json:"account_xp"`
	NextLevelXP  int64 `json:"next_level_xp"`
```

`MatchCompletedPlayer`에는 서버가 확정한 전투 결과만 받고 RP/XP 보상량은 받지 않는다. 보상량은 Services의 `AccountEconomyPolicy`가 계산한다.

### 1-11. C:/Users/user/Desktop/Winters/Services/internal/profile/repository.go

`GetProfile` SQL에 `account_progression`을 join하고 `level`, `total_xp`를 scan한다. `NextLevelXP`는 validated level threshold pack에서 계산한다. RP는 이 5분 Redis profile cache에 넣지 않고 `/shop/storefront`에서 읽어 stale balance race를 막는다.

기존 `InsertMatchHistory`와 `UpdatePlayerStats`의 분리 호출을 제거하고 다음 단일 transaction API로 교체한다.

아래로 교체:

```go
func (r *Repository) ApplyMatchCompletedPlayer(
	ctx context.Context,
	matchID uuid.UUID,
	player MatchCompletedPlayer,
	policy AccountEconomyPolicy,
) (*MatchRewardReceipt, error)
```

이 함수의 transaction 순서는 고정한다.

1. `(match_id, user_id)`의 `match_reward_receipts`를 먼저 조회한다. 있으면 기존 receipt를 반환하고 mutation을 하지 않는다.
2. `wallets`, `account_progression`, `player_stats`를 `FOR UPDATE`로 잠근다.
3. `match_history`를 한 번 insert한다.
4. 전적/MMR을 한 writer에서 한 번만 갱신한다.
5. Services 정책으로 `rpAwarded`, `xpAwarded`, `levelAfter`를 계산한다.
6. wallet/progression을 갱신한다.
7. `coin_transactions`에 `match_reward`, `idempotency_key='match:' + matchID`를 기록한다.
8. `match_reward_receipts`를 기록하고 commit한다.
9. commit 이후 profile cache를 무효화한다.

별도 leaderboard consumer는 MMR을 다시 더하지 않고, profile transaction이 확정한 절대 MMR을 Redis projection에 반영하도록 교체한다.

### 1-12. C:/Users/user/Desktop/Winters/Services/internal/profile/consumer.go

`handleMessage`의 다음 두 분리 호출을 삭제한다.

삭제할 코드:

```go
		if err := c.repo.InsertMatchHistory(ctx, p.UserID, event.MatchID, p); err != nil {
			slog.Error("insert match record", "user_id", p.UserID, "error", err)
			continue
		}

		if err := c.repo.UpdatePlayerStats(ctx, p.UserID, p); err != nil {
			slog.Error("update player stats", "user_id", p.UserID, "error", err)
			continue
		}
```

같은 위치에 아래 호출을 추가한다.

아래에 추가:

```go
		receipt, err := c.repo.ApplyMatchCompletedPlayer(ctx, event.MatchID, p, c.policy)
		if err != nil {
			slog.Error("apply match completion", "user_id", p.UserID, "error", err)
			return err
		}
		slog.Info("account reward settled",
			"match_id", event.MatchID,
			"user_id", p.UserID,
			"rp", receipt.RPAwarded,
			"xp", receipt.XPAwarded,
			"level", receipt.LevelAfter)
```

Kafka 재전달은 정상 상황으로 취급하고 receipt 때문에 중복 전적·중복 RP·중복 XP가 생기지 않게 한다.

### 1-13. C:/Users/user/Desktop/Winters/Services/internal/profile/handler.go

`Routes()`에 claims 기반 self-resource를 추가한다.

기존 코드:

```go
	r.Get("/{user_id}", h.GetProfile)
	r.Get("/{user_id}/history", h.GetMatchHistory)
```

아래로 교체:

```go
	r.Get("/me", h.GetMyProfile)
	r.Get("/me/history", h.GetMyMatchHistory)
```

관리/디버그용 타 계정 조회가 필요하면 별도 admin route와 role 검증을 둔다. 일반 JWT로 임의 UUID를 전달하는 현재 IDOR 경로는 제거한다.

### 1-14. C:/Users/user/Desktop/Winters/Services/internal/shop/model.go

기존 DTO를 stable product/content identity와 RP 용어로 교체한다.

아래로 교체:

```go
type ShopItem struct {
	ID          uuid.UUID `json:"id"`
	ProductKey  string    `json:"product_key"`
	ContentKey  string    `json:"content_key"`
	Name        string    `json:"name"`
	Description string    `json:"description"`
	ItemType    string    `json:"item_type"`
	PriceRP     int64     `json:"price_rp"`
	SortOrder   int       `json:"sort_order"`
	IsActive    bool      `json:"is_active"`
	IsStackable bool      `json:"is_stackable"`
	Owned       bool      `json:"owned"`
}

type StorefrontResponse struct {
	CatalogRevision string     `json:"catalog_revision"`
	RPBalance       int64      `json:"rp_balance"`
	Items           []ShopItem `json:"items"`
}

type PurchaseRequest struct {
	ItemID         uuid.UUID `json:"item_id"`
	IdempotencyKey string    `json:"idempotency_key"`
}

type PurchaseResponse struct {
	Status       string `json:"status"`
	ProductKey   string `json:"product_key"`
	ContentKey   string `json:"content_key"`
	RemainingRP  int64  `json:"remaining_rp"`
	AlreadyOwned bool   `json:"already_owned"`
}
```

`InventoryItem`에도 `ProductKey`, `ContentKey`를 추가한다.

### 1-15. C:/Users/user/Desktop/Winters/Services/internal/shop/repository.go

`ListItems`는 `product_key`, `content_key`, `sort_order`, `is_stackable`을 반환한다. claims 기반 `GetStorefront(userID)`는 wallet balance와 상품별 `EXISTS inventory`를 같은 read model로 반환한다.

기존 `Purchase`의 `ON CONFLICT ... quantity + 1` 경로를 제거하고 다음 transaction 규칙으로 교체한다.

1. `(user_id, request_key)` receipt가 있으면 같은 응답을 반환한다.
2. 상품과 wallet을 `FOR UPDATE`로 읽는다.
3. `is_stackable=false`이고 inventory가 있으면 RP를 차감하지 않고 `already_owned` receipt를 남긴다.
4. 잔액 부족이면 transaction mutation 없이 `ErrInsufficientBalance`를 반환한다.
5. wallet 차감, unique entitlement insert, coin ledger, purchase receipt를 한 transaction에서 commit한다.
6. 응답은 DB가 반환한 `remaining_rp`만 사용한다.

Champion product는 반드시 `item_type='champion'`, `is_stackable=false`, non-empty `content_key`여야 한다. Client가 이미 보유 표시를 했다는 이유로 서버의 중복 구매 방어를 생략하지 않는다.

### 1-16. C:/Users/user/Desktop/Winters/Services/internal/shop/handler.go

`Routes()`를 claims 기반 storefront로 교체한다.

기존 코드:

```go
	r.Get("/items", h.ListItems)
	r.Post("/purchase", h.Purchase)
	r.Get("/inventory/{user_id}", h.GetInventory)
```

아래로 교체:

```go
	r.Get("/storefront", h.GetStorefront)
	r.Post("/purchase", h.Purchase)
	r.Get("/inventory/me", h.GetMyInventory)
```

`GetStorefront`와 `GetMyInventory`는 URL user ID를 받지 않고 middleware claims의 `UserID`만 사용한다. `Purchase`는 비어 있거나 지나치게 긴 idempotency key를 400으로 거절한다.

### 1-17. C:/Users/user/Desktop/Winters/Client/Public/Network/Backend/AuthClient.h

`AuthResult`에 실제 계정 identity를 추가한다.

아래에 추가:

```cpp
	string  userId;
	string  username;
```

refresh token은 access token과 분리한다. access token은 memory-only, refresh token은 Windows Credential Manager 또는 DPAPI로 보호하는 `ClientCredentialStore`를 통해 저장한다.

새 `ClientCredentialStore.h/.cpp`: `CONFIRM_NEEDED`

확인 필요:

- current executable identity와 logout semantics를 확인한 뒤 Windows Credential Manager와 DPAPI 중 하나를 고정한다.
- 평문 JSON, registry plain string, command line에 refresh token을 쓰지 않는다.
- logout은 `/auth/logout` 성공 여부와 무관하게 local credential을 삭제하고, 서버에는 best-effort revoke를 보낸다.

### 1-18. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_Login.cpp

`HandleOnlineLoginResult`의 다음 코드를 교체한다.

기존 코드:

```cpp
	CClientShellSession::Instance().SetAuthenticatedAccount(
		email,
		email,
		result.accessToken);
```

아래로 교체:

```cpp
	CClientShellSession::Instance().SetAuthenticatedAccount(
		result.userId,
		result.username,
		result.accessToken,
		result.refreshToken);
```

`OnUpdate`에서 `kLoginArrowRect`가 무조건 `RequestOfflineLogin()`을 호출하는 현재 동작은 다음 두 명시적 버튼으로 분리한다.

- `Online Login`: email/password 또는 social OAuth/OIDC flow.
- `Offline Smoke`: `--offline` 또는 명시적 개발 버튼에서만 실행.

소셜 버튼은 system browser를 열고 one-time login code를 회수한다. provider access token을 C++ Client session에 보관하지 않는다.

### 1-19. C:/Users/user/Desktop/Winters/Client/Public/Network/Backend/ProfileClient.h

`ProfileData`에 아래 필드를 추가한다.

아래에 추가:

```cpp
	i32_t accountLevel = 1;
	i64_t accountXp = 0;
	i64_t nextLevelXp = 0;
```

`GetProfile(const string& userId, ...)`는 `GetMyProfile(...)`로 교체하고 `/profile/me`만 호출한다.

### 1-20. C:/Users/user/Desktop/Winters/Client/Public/Network/Backend/CShopClient.h

`ShopItemData`에 다음 필드를 추가한다.

아래에 추가:

```cpp
	string productKey;
	string contentKey;
	i32_t sortOrder = 0;
	bool_t owned = false;
	bool_t stackable = false;
```

`PurchaseResult.remainingCoins`는 `remainingRP`로 교체하고 `status`, `productKey`, `contentKey`, `alreadyOwned`를 추가한다. `ListItems + GetInventory` 초기 조합 대신 `GetStorefront` 한 호출로 catalog revision, RP, 상품, owned를 적용한다.

`Purchase`는 Client가 생성한 UUID request key를 함께 보낸다. 네트워크 timeout 재시도는 같은 key를 재사용하고, 새 click만 새 key를 만든다.

### 1-21. C:/Users/user/Desktop/Winters/Client/Public/ClientShell/ClientShellTypes.h

`ShellStoreItem`을 다음 계약으로 교체한다.

아래로 교체:

```cpp
struct ShellStoreItem
{
	std::string strItemID{};
	std::string strProductKey{};
	std::string strContentKey{};
	std::string strName{};
	std::string strDescription{};
	std::string strItemType{};
	i32_t iPriceRP = 0;
	i32_t iSortOrder = 0;
	bool_t bOwned = false;
	bool_t bPurchaseInFlight = false;
};
```

`ShellProfileSummary`에는 `i64_t iAccountXP`, `i64_t iNextLevelXP`를 추가하고 RP는 server response에서만 갱신한다.

### 1-22. C:/Users/user/Desktop/Winters/Client/Private/ClientShell/ClientShellDataStore.cpp

`SeedOfflineDefaults`는 online session에서 절대 실행되지 않도록 첫 조건을 교체한다.

기존 코드:

```cpp
	if (m_bSeeded)
		return;
```

아래로 교체:

```cpp
	if (m_bSeeded || !session.IsOfflineAccount())
		return;
```

offline smoke는 명시적으로 휘발성/all-unlocked임을 UI에 표시한다. 온라인 영속 기능을 로컬 JSON에 중복 저장해 두 번째 권위 소스를 만들지 않는다.

`ApplyProfileData`는 `accountLevel`, `accountXp`, `nextLevelXp`를 적용한다. `ApplyStorefront`는 RP와 `productKey/contentKey/sortOrder/owned`를 적용하고 안정 정렬한다. 구매 성공 및 `already_owned` 모두 owned로 수렴시키고 서버의 `remainingRP`를 적용한다.

`IsChampionOwned(const std::string& contentKey)` read-only query를 추가한다. `CChampionCatalog::bSelectable`은 변경하지 않는다.

### 1-23. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_MainMenu.cpp

namespace 상수에서 `kGameStartRect` 아래에 상점 버튼 source rect를 추가한다. 현재 `MainMenu1.png` 상단 동전 더미 아이콘의 source coordinate를 기준으로 캡처 검증 후 미세 조정한다.

아래에 추가:

```cpp
	constexpr ImageSourceRect kStoreRect{ 1003.f, 0.f, 1070.f, 82.f };
```

`OnUpdate`의 game start click 아래에 추가:

```cpp
	if (m_ImageUI.WasSourceRectClicked(kStoreRect))
		ChangeToShop();
```

`ChangeToShop()`은 `eSceneID::Shop`과 `CScene_Shop::Create()`로 전환한다. 상단 baked RP 숫자는 실제 `ShellProfileSummary.iRP`를 그리는 dynamic layer로 덮는다.

### 1-24. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_Shop.h

새 파일: `CONFIRM_NEEDED`

확인 필요:

- 영구 runtime text를 `CImageScenePresenter`의 direct ImDrawList로 추가하지 않는다.
- 현재 Engine runtime text adapter를 먼저 확정하고, 없다면 `CUITextRenderer`/font manager를 통한 generic text primitive를 최소 구현한다.
- Scene은 `ChampionStoreViewState`만 소비하며 HTTP DTO, PostgreSQL, `eChampion` 가격 규칙을 직접 알지 않는다.

### 1-25. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_Shop.cpp

새 파일: `CONFIRM_NEEDED`

확인 필요:

- `OnEnter`: initial sync 요청, current playable champion catalog와 storefront를 `contentKey`로 join, `GetRosterChampionPortraitPath()`로 texture load.
- `OnUpdate`: `CClientShellBackendService::ProcessCallbacks()`를 매 frame 호출, back 버튼 처리, 단일 in-flight purchase 처리.
- `OnRender`: 6열 사각 portrait grid, 상품명/가격/RP, selected detail, purchase button을 runtime UI path로 렌더.
- owned 상품: portrait 위 `Vec4(0.f, 0.f, 0.f, 0.58f)` overlay와 `이미 구매한 상품입니다` 표시, 재구매 요청 금지.
- 잔액 부족: button disable 표현을 하되 서버 검증은 유지.
- 응답 후 DB에서 온 `remainingRP/owned`로 즉시 다시 그린다.
- offline smoke에서 구매를 허용할 경우 화면에 `비영속 개발 모드`를 명시한다.

### 1-26. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_BanPick.h

`ChampionCell`에 presentation-only ownership bit를 추가한다.

아래에 추가:

```cpp
		bool_t bOwned = false;
```

이 필드는 선택 가능 여부가 아니다.

### 1-27. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_BanPick.cpp

`BuildChampionCells()`에서 `cell.champion = champion;` 아래에 `champion.<canonical-name>` content key로 DataStore의 owned 상태를 읽어 `cell.bOwned`를 채운다.

`RenderChampionGridAndRosterOverlay()`의 portrait draw 뒤, outline 앞에 아래를 추가한다.

아래에 추가:

```cpp
		if (!cell.bOwned && !CClientShellSession::Instance().IsOfflineAccount())
			m_ImageUI.DrawSourceRect(cell.rect, Vec4(0.f, 0.f, 0.f, 0.62f));
```

`ResolveClickedChampion`, `HandleServerChampionSelectInput`, `HandleLocalChampionSelectInput`, Server `LobbyAuthority::TryPickChampion`에는 entitlement gate를 추가하지 않는다. 미보유도 실제 선택/플레이 가능한 이번 데모 정책을 보존한다.

### 1-28. C:/Users/user/Desktop/Winters/Client/Include/Client.vcxproj

`Scene_MainMenu.cpp`와 `Scene_MatchLoading.cpp` 사이에 아래를 추가한다.

아래에 추가:

```xml
    <ClCompile Include="..\Private\Scene\Scene_Shop.cpp" />
```

`Scene_MainMenu.h`와 `Scene_MatchLoading.h` 사이에 아래를 추가한다.

아래에 추가:

```xml
    <ClInclude Include="..\Public\Scene\Scene_Shop.h" />
```

### 1-29. C:/Users/user/Desktop/Winters/Client/Include/Client.vcxproj.filters

`Scene_MainMenu`과 같은 `01. Scene` 계열에 `Scene_Shop.h/.cpp`를 등록한다. 실제 filter 명은 구현 전에 `Shop` 전용 filter를 만들지, `MainMenu` 아래에 둘지 확정한다.

XML body: `CONFIRM_NEEDED`

### 1-30. C:/Users/user/Desktop/Winters/Shared/Schemas/Hello.fbs

현재 Server의 `DispatchHello`가 no-op이고 C++ session에 AccountID가 없으므로, client-to-server handshake에 signed match ticket을 추가한다. 동일 `Hello` table을 양방향으로 계속 사용할지 `ClientHello.fbs`로 분리할지는 packet generator와 호환성 확인 후 확정한다.

schema body: `CONFIRM_NEEDED`

확인해야 할 최종 필드는 다음과 같다.

- `matchTicket`: Matchmaking이 발급한 signed, short-lived ticket.
- ticket claims: `match_id`, `user_id`, `room_id`, `expires_at`, `nonce`.
- `clientDataBuildHash` 및 item catalog revision.
- raw social token이나 refresh token은 게임 서버로 보내지 않는다.

### 1-31. C:/Users/user/Desktop/Winters/Server/Public/Network/Session.h

`CSession`에 인증 전/후 상태, AccountID, MatchID를 보관한다. `SetControlledEntity`와 gameplay/lobby command는 ticket 검증이 끝난 session만 허용한다.

새 field/type body: `CONFIRM_NEEDED`

확인 필요:

- 프로젝트 공용 UUID 타입이 없으므로 임의 raw string field를 바로 추가하지 말고 Services UUID를 표현하는 network-safe `AccountId` 타입 위치를 먼저 확정한다.
- match ticket 검증 secret/공개키는 binary hardcode가 아니라 server secret/config로 주입한다.

### 1-32. C:/Users/user/Desktop/Winters/Server/Private/Network/PacketDispatcher.cpp

현재 no-op인 `DispatchHello`를 다음 gate로 교체한다.

1. FlatBuffer verify.
2. ticket signature, expiry, nonce, room/match binding 검증.
3. session에 AccountID/MatchID bind.
4. 실패 시 command를 받지 않고 disconnect 또는 typed reject.
5. 성공 후에만 lobby seat/pick/start command를 허용.

클라이언트가 보낸 user UUID 문자열만 믿지 않는다.

### 1-33. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

경기 종료 판정이 현재 비어 있으므로 `GameEnded`가 확정되는 단일 지점에서 한 번만 immutable `MatchCompletedPayload`를 만든다. payload에는 match/account binding, 승패, K/D/A, MMR delta의 서버 truth만 넣는다.

새 `MatchResultPublisher.h/.cpp`: `CONFIRM_NEEDED`

확인 필요:

- 현재 GameMode의 nexus/game-over 판정과 Result scene 전환이 아직 없으므로 먼저 종료 조건 owner를 확정한다.
- C++ Server에서 Kafka client를 새로 넣지 않는다. internal HTTPS endpoint에 idempotency key=`match_id`로 제출하고 Services가 DB outbox/Kafka를 소유한다.
- 네트워크 실패 시 bounded retry와 로컬 durable outbox를 사용한다. Client가 대신 제출하지 않는다.

### 1-34. C:/Users/user/Desktop/Winters/Services/internal/matchmaking/handler.go

internal service-authenticated `POST /internal/matches/{match_id}/complete`를 추가한다. body의 match ID와 ticket/room binding을 검증하고 `MatchCompleted` outbox를 idempotent하게 기록한다.

외부 JWT 사용자 route와 internal server credential route를 같은 middleware로 취급하지 않는다. exact handler body는 GameRoom 종료 payload와 service-auth 방식이 확정된 뒤 작성한다: `CONFIRM_NEEDED`.

### 1-35. C:/Users/user/Desktop/Winters/Client/Public/Defines.h

`eSceneID::Result`는 이미 있으므로 새 enum을 만들지 않는다. 경기 결과 scene 구현은 이 ID를 재사용한다.

새 `Scene_Result.h/.cpp`: `CONFIRM_NEEDED`

확인 필요:

- reward receipt의 `rpAwarded`, `xpAwarded`, `levelBefore/After`, `balanceAfter`를 표시한다.
- 응답 지연 시 `보상 정산 중` 상태로 두고 profile/storefront refetch로 수렴한다.
- Client가 RP/XP를 미리 더해 영속 truth를 만들지 않는다.

### 1-36. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/ItemGameplayDefs.json

`Shared/GameSim/Definitions/ItemDef.h`의 15개 하드코딩 item을 ServerPrivate authoring data로 옮긴다. item ID, price, stat modifier, enabled/purchasable을 포함한다.

새 파일 body는 `Build-LoLDefinitionPack.py`의 최종 item schema가 정해진 뒤 작성한다: `CONFIRM_NEEDED`.

### 1-37. C:/Users/user/Desktop/Winters/Tools/LoLData/Build-LoLDefinitionPack.py

item gameplay section을 validate/cook하고 다음 오류를 build failure로 만든다.

- duplicate/zero item ID.
- 음수 가격/스탯.
- 알 수 없는 stat key.
- ClientPublic item catalog에 없는 purchasable item.
- server/client catalog revision 불일치.

생성 pack에는 runtime asset path나 localized name을 넣지 않는다.

### 1-38. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ItemDef.h

`CItemRegistry::Find` 안의 static 15-item array를 삭제한다. Shared에는 `ItemDef`, `ItemStatModifier`, read-only query interface만 남기고 실제 definition pack은 Server가 room 시작 시 주입한다.

Client가 이 header를 include해 가격/스탯 truth를 컴파일하는 경로를 제거한다.

### 1-39. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

`HandleBuyItem`의 `CItemRegistry::Instance().Find(cmd.itemId)`를 injected `ItemGameplayPack` query로 교체한다.

현재 gold/빈 slot 검사에 다음 서버 검증을 추가한다.

- authenticated issuer가 소유한 entity인지.
- item catalog revision이 유효한지.
- item이 enabled/purchasable인지.
- 현재 entity가 shop access volume 안에 있는지.
- 충분한 gold와 빈 inventory slot이 있는지.

성공 또는 실패마다 `ItemPurchaseResultEvent`를 한 번 발생시키고, Snapshot의 gold/inventory가 최종 truth가 되게 한다. reject reason은 `NotInShop`, `UnknownItem`, `Disabled`, `InsufficientGold`, `InventoryFull`, `CatalogMismatch`로 제한한다.

### 1-40. C:/Users/user/Desktop/Winters/Shared/Schemas/Event.fbs

기존 one-shot event union에 item purchase result를 추가한다.

schema body: `CONFIRM_NEEDED`

필수 payload는 `requestSequence`, `itemId`, `accepted`, `rejectReason`, `goldAfter`, `inventoryRevision`, `catalogRevision`이다. Client가 이 Event만으로 inventory를 확정하지 않고 Snapshot과 수렴하게 한다.

### 1-41. C:/Users/user/Desktop/Winters/Client/Private/GamePlay/LoLUIContentRegistry.cpp

`RegisterLoLShopItems`에서 `CItemRegistry::Instance().Find`로 Client가 authoritative price/stat을 읽는 코드를 제거한다.

ClientPublic `ItemShopCatalog.json`은 icon/name/description/section/order만 소유한다. match 접속 시 서버가 전달한 compact `ItemShopViewCatalog(itemId, visiblePrice, purchasable, revision)`를 join해 Engine `UIShopItemAssetDesc`를 등록한다.

아이템 이미지 전체를 HTTP/DB에서 매번 내려받지 않는다. 서버가 내려주는 것은 게임플레이 catalog의 공개 projection과 revision이고, 구매 판정은 매번 Server/GameSim이 다시 수행한다.

### 1-42. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameLifecycle.cpp

현재 `UI_SendBuyItemCommand -> SendBuyItem` callback은 유지한다. catalog sync 이전에는 buy button을 비활성화하고, `ItemPurchaseResultEvent`와 다음 Snapshot을 적용한 후 status text를 갱신한다.

이 경로에 RP, account entitlement, Services ShopClient를 연결하지 않는다.

### 1-43. C:/Users/user/Desktop/Winters/Services/internal/payment/gateway_mock.go

현재 mock gateway는 영수증과 금액을 검증하지 않으므로 개발 전용임을 명시하고 production profile에서 route를 비활성화한다. 이번 챔피언 상점 수직 슬라이스는 `initial_grant`와 `match_reward`만 사용하며 실제 현금 RP 충전으로 간주하지 않는다.

### 1-44. C:/Users/user/Desktop/Winters/Services/pkg/accountpolicy/policy.go

새 파일: `CONFIRM_NEEDED`

확인 필요:

- `AccountEconomyPolicy.json`을 process 시작 시 한 번 읽고 schema version, currency, starting balance, reward의 음수 여부, level/threshold 단조 증가를 검증한다.
- `ResolveLevel(totalXP)`와 `ResolveMatchReward(result)`는 pure function으로 제공한다.
- Auth와 Profile service가 같은 immutable policy instance를 주입받는다.
- Docker/로컬 실행 모두에서 안정적인 절대 경로를 구성하도록 `ACCOUNT_POLICY_PATH`를 사용한다. Client runtime resource resolver와 섞지 않는다.
- loader와 level/reward pure function 단위 테스트 body를 함께 확정한 후 파일 전체를 작성한다.

### 1-45. C:/Users/user/Desktop/Winters/Services/cmd/auth/main.go

`config.Load()` 뒤에 account policy를 validate/load하고 `auth.NewRepository(db, rdb, policy.StartingBalance)`로 주입한다. policy load 실패는 RP 0 fallback으로 계속 실행하지 않고 process 시작을 실패시킨다.

Auth route는 다음처럼 분리한다.

- public: register, login, refresh, social start/callback.
- authenticated: logout/revoke가 bearer auth를 추가로 요구하는 계약을 선택한 경우 JWT middleware 적용.
- provider callback은 state/PKCE 검증을 통과해야 한다.

현재 `r.Mount("/auth", handler.Routes())` 하나로 모두 노출하는 경로는 `PublicRoutes()`와 `AuthenticatedRoutes()`가 확정된 후 교체한다.

### 1-46. C:/Users/user/Desktop/Winters/Services/internal/auth/service.go

`Register`에서 hash 문자열 주소를 `User.PasswordHash`에 넣고, `Login`은 `PasswordHash == nil`인 social-only 계정에 대해 동일한 unauthorized 응답을 반환한다.

기존 코드:

```go
	user := &User{Username: req.Username, Email: req.Email, Password: string(hashed)}
```

아래로 교체:

```go
	passwordHash := string(hashed)
	user := &User{
		Username:     req.Username,
		Email:        req.Email,
		PasswordHash: &passwordHash,
	}
```

기존 password 비교 앞에 아래를 추가한다.

아래에 추가:

```go
	if user.PasswordHash == nil {
		return nil, fmt.Errorf("%w: invalid credentials", apperr.ErrUnauthorized)
	}
```

password 비교는 `[]byte(*user.PasswordHash)`를 사용한다. Social callback은 provider identity를 transaction으로 find-or-create/link하고 동일한 `issueTokens`를 사용한다.

### 1-47. C:/Users/user/Desktop/Winters/Services/internal/auth/repository.go

`INSERT INTO users`에는 `user.PasswordHash`를 전달하고 `FindByEmail`, `FindByID`의 password scan target도 `&u.PasswordHash`로 교체한다. social identity upsert와 user 생성, wallet/progression/initial ledger 생성은 한 transaction에서 끝나야 한다.

`NewRepository` signature를 다음으로 교체한다.

아래로 교체:

```go
func NewRepository(db *pgxpool.Pool, rdb *redis.Client, startingBalance int64) *Repository
```

`Repository`에는 `startingBalance int64`를 추가한다. 음수 값은 생성자에서 거절하거나 process startup validation으로 도달 불가하게 만든다.

### 1-48. C:/Users/user/Desktop/Winters/Services/cmd/profile/main.go

account policy load 후 다음 생성 경로를 교체한다.

기존 코드:

```go
	consumer := profile.NewConsumer(repo, reader)
```

아래로 교체:

```go
	consumer := profile.NewConsumer(repo, reader, policy)
```

`profile.NewRepository`에도 level threshold resolver를 주입해 `GetProfile.NextLevelXP`와 reward settlement가 같은 curve를 사용하게 한다.

### 1-49. C:/Users/user/Desktop/Winters/Services/internal/leaderboard/consumer.go

다음 기존 delta 재계산을 삭제한다.

삭제할 코드:

```go
		currentMMR, err := c.repo.GetCurrentMMR(ctx, p.UserID)
		if err != nil {
			slog.Error("get current mmr", "user_id", p.UserID, "error", err)
			continue
		}
		newMMR := currentMMR + p.MMRChange
		if newMMR < 0 {
			newMMR = 0
		}
		if err := c.repo.UpdateScore(ctx, p.UserID, newMMR); err != nil {
```

Profile settlement 완료 event가 가진 절대 `MMRAfter`를 projection에 적용하거나, DB의 확정 MMR을 읽어 `UpdateScore`한다. 동일 raw `MatchCompleted`에서 두 consumer가 각각 MMR delta를 쓰는 구조를 유지하지 않는다.

### 1-50. C:/Users/user/Desktop/Winters/Services/cmd/shop/main.go

현재 `handler.Routes()`를 사용하지 않고 route를 수동 등록하므로 아래 기존 block을 직접 교체해야 한다.

기존 코드:

```go
	r.Mount("/shop", func() chi.Router {
		sr := chi.NewRouter()
		sr.Get("/items", handler.ListItems)
		sr.Group(func(r chi.Router) {
			r.Use(middleware.JWTAuth(jwtMgr))
			r.Post("/purchase", handler.Purchase)
			r.Get("/inventory/{user_id}", handler.GetInventory)
		})
		return sr
	}())
```

아래로 교체:

```go
	r.Group(func(r chi.Router) {
		r.Use(middleware.JWTAuth(jwtMgr))
		r.Mount("/shop", handler.Routes())
	})
```

Champion catalog 공개 조회가 나중에 필요하더라도 account-specific `owned`와 RP가 들어가는 storefront는 반드시 JWT route로 둔다.

### 1-51. C:/Users/user/Desktop/Winters/Client/Private/Network/Backend/AuthClient.cpp

`ParseAuthResponse`에서 다음을 추가한다.

아래에 추가:

```cpp
		result.userId = data.value("user_id", "");
		result.username = data.value("username", "");
```

success인데 `user_id`가 비어 있으면 인증 성공으로 취급하지 않는다. `Refresh`도 새 token pair의 identity를 갱신하고 secure credential store에 refresh token을 교체 저장한다.

현재 `Logout()`의 local string clear만 수행하는 구현은 서버 `/auth/logout` best-effort 호출과 secure credential 삭제로 교체한다.

### 1-52. C:/Users/user/Desktop/Winters/Client/Private/Network/Backend/ProfileClient.cpp

`GetProfile` URL을 `/profile/me`로 교체하고 파서에 다음을 추가한다.

아래에 추가:

```cpp
			profile.accountLevel = data.value("account_level", 1);
			profile.accountXp = data.value("account_xp", static_cast<i64_t>(0));
			profile.nextLevelXp = data.value("next_level_xp", static_cast<i64_t>(0));
```

history URL도 `/profile/me/history`로 교체한다.

### 1-53. C:/Users/user/Desktop/Winters/Client/Private/Network/Backend/CShopClient.cpp

`ListItems`와 `GetInventory(userId)`를 `/shop/storefront` parser 하나로 교체한다. response의 `catalog_revision`, `rp_balance`, `items[].product_key/content_key/price_rp/sort_order/owned`를 모두 전달한다.

`Purchase` body는 JSON 문자열 직접 연결 대신 JSON object serializer를 사용하고 다음 필드를 보낸다.

```json
{
  "item_id": "<shop UUID>",
  "idempotency_key": "<request UUID>"
}
```

응답은 `remaining_rp`, `already_owned`, `product_key`, `content_key`, `status`를 parse한다.

### 1-54. C:/Users/user/Desktop/Winters/Client/Private/ClientShell/ClientShellBackendService.cpp

`RequestInitialSync`의 profile call에서 `m_strUserID` argument를 제거하고, store list/inventory 두 요청을 storefront 한 요청으로 합친다. 이에 따라 `m_bStoreRequestInFlight`, `m_bInventoryRequestInFlight`는 `m_bStorefrontRequestInFlight` 하나로 교체한다.

`RequestPurchase`는 click 시 만든 request UUID를 callback 완료까지 보존한다. timeout 재시도 API는 같은 request UUID를 재사용한다.

Scene 전환 중 pending callback generation guard는 현재 구조를 유지한다. `Scene_Shop::OnUpdate`가 이 service의 `ProcessCallbacks()`를 호출한다.

### 1-55. C:/Users/user/Desktop/Winters/Client/Public/ClientShell/ClientShellSession.h

`SetAuthenticatedAccount`에 `refreshToken`을 추가하고 getter를 추가한다.

아래로 교체:

```cpp
	void SetAuthenticatedAccount(
		const std::string& userId,
		const std::string& displayName,
		const std::string& accessToken,
		const std::string& refreshToken);
```

아래에 추가:

```cpp
	const std::string& GetRefreshToken() const { return m_strRefreshToken; }
```

private field에 `std::string m_strRefreshToken{}`을 추가한다. 실제 장기 저장은 session string 자체가 아니라 secure credential store가 소유한다.

### 1-56. C:/Users/user/Desktop/Winters/Client/Private/ClientShell/ClientShellSession.cpp

`SetAuthenticatedAccount`에서 user UUID가 비어 있으면 authenticated state로 전환하지 않는다. access token은 memory에, refresh token은 secure store와 session에 반영한다. `SetOfflineAccount`와 `Logout`은 refresh token도 clear한다.

### 1-57. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_MainMenu.h

`ChangeToLogin()` 아래에 다음 declaration을 추가한다.

아래에 추가:

```cpp
	void ChangeToShop();
```

### 1-58. C:/Users/user/Desktop/Winters/Client/Public/GamePlay/ChampionCatalog.h

`ChampionCatalogEntry`에 stable account-store join key를 추가한다.

아래에 추가:

```cpp
	const char* contentKey = nullptr;
```

`ChampionCatalog.cpp`의 current 17-entry order table을 `{ eChampion, "champion.<name>" }` seed로 교체하고 visual pack과 one-to-one 검증한다. Store/BanPick이 각자 enum-to-string switch를 만들지 않는다. 이 key는 entitlement presentation join용이며 `bSelectable/bPlayable`을 바꾸지 않는다.

### 1-59. C:/Users/user/Desktop/Winters/Services/docker-compose.yml

Auth/Profile service가 동일한 read-only `AccountEconomyPolicy.json`을 읽도록 mount와 `ACCOUNT_POLICY_PATH`를 추가한다. 실제 compose의 서비스별 working directory와 Windows/Linux path를 확인한 뒤 YAML 전체 변경 block을 작성한다: `CONFIRM_NEEDED`.

## 2. 검증

현재 문서 작업 검증:

- `git diff --check -- .md/plan/2026-07-13_ACCOUNT_RP_CHAMPION_STOREFRONT_AND_ITEM_SHOP_ARCHITECTURE.md`
- 이번 세션은 계획 문서만 추가하므로 Client/Server runtime build는 실행하지 않는다.
- 기존 dirty worktree의 AI/projectile/engine 변경 파일은 수정·stage·revert하지 않는다.

구현 Slice A — 계정/DB/상점 정합성:

- `cd Services && go test ./...`
- fresh DB에 migration up 성공.
- 중복 match history가 있는 DB에서는 migration이 자동 삭제하지 않고 명시적으로 실패.
- 신규 email 계정과 DevSocial 계정 모두 starting RP 10,000, level 1, XP 0.
- 같은 social `(provider, subject)` 재로그인 시 같은 `users.id`.
- `/profile/me`, `/shop/storefront`, `/shop/inventory/me`가 claims 계정만 반환.
- 다른 user UUID를 URL/body에 넣어 타 계정 inventory/profile을 조회할 수 없음.
- 같은 champion을 서로 다른 request key로 두 번 눌러도 두 번째는 `already_owned`, RP 재차감 없음.
- 같은 request key를 timeout/retry해도 같은 receipt와 잔액 반환.
- 잔액 부족 시 wallet/inventory/ledger 모두 mutation 0.
- Services/Client exe 재시작 후 같은 계정으로 로그인하면 RP와 owned champion 복원.

구현 Slice B — Client 상점/밴픽:

- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`
- MainMenu 상점 icon click -> `eSceneID::Shop` 전환.
- current playable 17개 사각 초상화가 중복 asset mapping 없이 표시.
- 구매 성공 즉시 RP 감소와 owned overlay/text 갱신.
- 이미 보유 상품은 `이미 구매한 상품입니다`와 검은 overlay, 추가 요청 0.
- 밴픽은 미보유에만 검은 overlay, 클릭/선택/게임 시작은 online/local 모두 성공.
- Shop scene에서도 backend callback pump가 동작해 응답이 멈추지 않음.
- offline smoke는 비영속 표시가 있고 online 데이터와 합쳐지지 않음.
- 모든 runtime resource는 `Client/Bin/Resource`에서만 resolve.

구현 Slice C — 경기 종료 RP/XP:

- signed/expired/wrong-room match ticket 각각 accept/reject 자동 테스트.
- Client가 임의 UUID를 보낸 경우 reward 귀속 불가.
- 한 `MatchCompleted`를 Kafka로 3회 재전달해 match history 1행, reward receipt 1행, RP/XP 1회만 증가.
- win/loss/draw/abandoned 정책 케이스별 reward 테스트.
- threshold 직전/정확히 도달/여러 레벨 통과/max level 테스트.
- profile와 leaderboard가 MMR을 한 번만 반영.
- game server 결과 publish가 일시 실패해도 durable retry 후 한 번 정산.
- Result scene은 receipt를 표시하고 재접속/profile refetch와 같은 잔액으로 수렴.

구현 Slice D — 인게임 아이템 상점:

- `Build-LoLDefinitionPack.py` item schema valid/duplicate/negative/unknown stat 테스트.
- Client binary가 `CItemRegistry` hardcoded price/stat을 읽지 않는지 `rg` 검증.
- shop 밖 구매, 잔액 부족, full inventory, unknown/disabled item의 reject reason 확인.
- 정상 구매는 gold 1회 차감, slot 1회 삽입, stat 1회 dirty, result Event 1회.
- packet reorder/drop 상황에서도 Snapshot gold/inventory가 최종 truth로 수렴.
- server/client item catalog revision mismatch는 구매를 막고 가시적인 debug/status를 남김.
- ClientPublic icon/name/description 누락은 visual fallback만 발생하고 서버 구매 truth를 바꾸지 않음.

최종 handoff gate:

- `git diff --check`
- `cd Services && go test ./...`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`
- Docker PostgreSQL/Redis/Kafka E2E capture와 Client 재시작 persistence capture 첨부.
- 2026-07-31 수직 슬라이스 시연 범위는 `DevSocial 로그인 -> 상점 -> 챔피언 구매 -> exe 재시작 -> owned 복원 -> 밴픽 overlay/선택 -> 1경기 종료 -> RP/XP 증가`로 고정한다.
