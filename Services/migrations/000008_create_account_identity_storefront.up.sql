-- S030: account identity (ID login) + RP storefront (champion products)
-- Idempotent where possible: safe to re-run against an already-migrated database.

ALTER TABLE users
    ALTER COLUMN email DROP NOT NULL,
    ALTER COLUMN password DROP NOT NULL;

CREATE TABLE IF NOT EXISTS user_identities (
    id               UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id          UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    provider         VARCHAR(32) NOT NULL,
    provider_subject VARCHAR(255) NOT NULL,
    provider_email   VARCHAR(255),
    created_at       TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at       TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE(provider, provider_subject)
);

CREATE INDEX IF NOT EXISTS idx_user_identities_user_id
    ON user_identities(user_id);

-- Backfill: existing email accounts become (local_email, lower(email)) identities.
INSERT INTO user_identities (user_id, provider, provider_subject, provider_email)
SELECT id, 'local_email', LOWER(email), email
FROM users
WHERE email IS NOT NULL
ON CONFLICT (provider, provider_subject) DO NOTHING;

ALTER TABLE wallets
    ADD COLUMN IF NOT EXISTS currency_code VARCHAR(8) NOT NULL DEFAULT 'RP';

ALTER TABLE coin_transactions
    DROP CONSTRAINT IF EXISTS coin_transactions_tx_type_check;

ALTER TABLE coin_transactions
    ADD CONSTRAINT coin_transactions_tx_type_check
    CHECK (tx_type IN ('charge', 'purchase', 'refund', 'initial_grant', 'match_reward', 'admin_grant'));

ALTER TABLE shop_items
    ADD COLUMN IF NOT EXISTS product_key VARCHAR(128),
    ADD COLUMN IF NOT EXISTS content_key VARCHAR(128),
    ADD COLUMN IF NOT EXISTS sort_order INT NOT NULL DEFAULT 0,
    ADD COLUMN IF NOT EXISTS is_stackable BOOLEAN NOT NULL DEFAULT true;

CREATE UNIQUE INDEX IF NOT EXISTS uq_shop_items_product_key
    ON shop_items(product_key)
    WHERE product_key IS NOT NULL;

-- Champion storefront seed: full selectable roster, flat 50 RP (S030 user spec).
-- Prices are owned by this seed/DB, never hardcoded in Services or Client code.
INSERT INTO shop_items (name, description, item_type, price, is_active, product_key, content_key, sort_order, is_stackable) VALUES
    ('Ezreal',    'Champion', 'champion', 50, true, 'store.champion.ezreal',    'champion.ezreal',    0,  false),
    ('Fiora',     'Champion', 'champion', 50, true, 'store.champion.fiora',     'champion.fiora',     1,  false),
    ('Jax',       'Champion', 'champion', 50, true, 'store.champion.jax',       'champion.jax',       2,  false),
    ('Master Yi', 'Champion', 'champion', 50, true, 'store.champion.masteryi',  'champion.masteryi',  3,  false),
    ('Annie',     'Champion', 'champion', 50, true, 'store.champion.annie',     'champion.annie',     4,  false),
    ('Ashe',      'Champion', 'champion', 50, true, 'store.champion.ashe',      'champion.ashe',      5,  false),
    ('Yone',      'Champion', 'champion', 50, true, 'store.champion.yone',      'champion.yone',      6,  false),
    ('Irelia',    'Champion', 'champion', 50, true, 'store.champion.irelia',    'champion.irelia',    7,  false),
    ('Yasuo',     'Champion', 'champion', 50, true, 'store.champion.yasuo',     'champion.yasuo',     8,  false),
    ('Kalista',   'Champion', 'champion', 50, true, 'store.champion.kalista',   'champion.kalista',   9,  false),
    ('Sylas',     'Champion', 'champion', 50, true, 'store.champion.sylas',     'champion.sylas',     10, false),
    ('Viego',     'Champion', 'champion', 50, true, 'store.champion.viego',     'champion.viego',     11, false),
    ('Garen',     'Champion', 'champion', 50, true, 'store.champion.garen',     'champion.garen',     12, false),
    ('Zed',       'Champion', 'champion', 50, true, 'store.champion.zed',       'champion.zed',       13, false),
    ('Riven',     'Champion', 'champion', 50, true, 'store.champion.riven',     'champion.riven',     14, false),
    ('Kindred',   'Champion', 'champion', 50, true, 'store.champion.kindred',   'champion.kindred',   15, false),
    ('Lee Sin',   'Champion', 'champion', 50, true, 'store.champion.leesin',    'champion.leesin',    16, false)
ON CONFLICT (product_key) WHERE product_key IS NOT NULL DO NOTHING;
