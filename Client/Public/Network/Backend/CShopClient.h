#pragma once
#include "Defines.h"
#include "Network/Backend/CHttpClient.h"

NS_BEGIN(Client)

struct ShopItemData
{
    string  id;
    string  name;
    string  description;
    string  itemType;
    i64_t   price = 0;
    string  productKey;
    string  contentKey;
    i32_t   sortOrder = 0;
    bool_t  owned = false;
};

struct PurchaseResult
{
    bool_t  success = false;
    string  status;          // "purchased" | "already_owned"
    bool_t  owned = false;
    i64_t   remainingCoins = 0;
    string  error;
};

// 한 계정의 잔액+상품+소유권 원자 스냅샷 (GET /shop/storefront).
struct StorefrontData
{
    bool_t  success = false;
    string  currencyCode;
    i64_t   balanceRP = 0;
    vector<ShopItemData> items;
    string  error;
};

struct InventoryItemData
{
    string  itemId;
    string  name;
    string  itemType;
    i32_t   quantity = 0;
};

using ShopListCallback = function<void(const vector<ShopItemData>&)>;
using PurchaseCallback = function<void(const PurchaseResult&)>;
using InventoryCallback = function<void(const vector<InventoryItemData>&)>;
using StorefrontCallback = function<void(const StorefrontData&)>;

class CShopClient
{
public:
    ~CShopClient() = default;

    static unique_ptr<CShopClient> Create(const string& baseURL);

    void    SetAuthToken(const string& token);
    void    GetStorefront(StorefrontCallback callback);
    void    ListItems(ShopListCallback callback);
    void    Purchase(const string& itemId, PurchaseCallback callback);
    void    GetInventory(const string& userId, InventoryCallback callback);
    void    ProcessCallbacks();

private:
    CShopClient() = default;

    unique_ptr<CHttpClient> m_pHttp;
};

NS_END
