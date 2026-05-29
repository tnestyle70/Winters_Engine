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
};

struct PurchaseResult
{
    bool_t  success = false;
    i64_t   remainingCoins = 0;
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

class CShopClient
{
public:
    ~CShopClient() = default;

    static unique_ptr<CShopClient> Create(const string& baseURL);

    void    SetAuthToken(const string& token);
    void    ListItems(ShopListCallback callback);
    void    Purchase(const string& itemId, PurchaseCallback callback);
    void    GetInventory(const string& userId, InventoryCallback callback);
    void    ProcessCallbacks();

private:
    CShopClient() = default;

    unique_ptr<CHttpClient> m_pHttp;
};

NS_END
