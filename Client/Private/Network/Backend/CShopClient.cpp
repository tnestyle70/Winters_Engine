#include "Network/Backend/CShopClient.h"
#pragma push_macro("new")
#undef new
#include "Network/Backend/json.hpp"
#pragma pop_macro("new")

using json = nlohmann::json;

unique_ptr<CShopClient> CShopClient::Create(const string& baseURL)
{
    auto pInstance = unique_ptr<CShopClient>(new CShopClient());
    pInstance->m_pHttp = CHttpClient::Create(baseURL);
    return pInstance;
}

void CShopClient::SetAuthToken(const string& token) { m_pHttp->SetAuthToken(token); }

void CShopClient::GetStorefront(StorefrontCallback callback)
{
    m_pHttp->AsyncGet("/shop/storefront", [callback](const HttpResponse& resp) {
        StorefrontData storefront;
        try
        {
            auto j = json::parse(resp.body);
            if (!resp.success || !j.contains("data"))
            {
                storefront.error = j.value("error", "storefront request failed");
                callback(storefront);
                return;
            }

            auto data = j["data"];
            storefront.success = true;
            storefront.currencyCode = data.value("currency_code", "RP");
            storefront.balanceRP = data.value("balance_rp", (i64_t)0);

            for (const auto& obj : data["items"])
            {
                ShopItemData item;
                item.id = obj.value("item_id", "");
                item.name = obj.value("display_name", "");
                item.itemType = "champion";
                item.price = obj.value("price_rp", (i64_t)0);
                item.productKey = obj.value("product_key", "");
                item.contentKey = obj.value("content_key", "");
                item.sortOrder = obj.value("sort_order", 0);
                item.owned = obj.value("owned", false);
                storefront.items.push_back(item);
            }
        }
        catch (const json::exception& e)
        {
            storefront.success = false;
            storefront.error = string("JSON parse error: ") + e.what();
        }
        callback(storefront);
        });
}

void CShopClient::ListItems(ShopListCallback callback)
{
    m_pHttp->AsyncGet("/shop/items", [callback](const HttpResponse& resp) {
        vector<ShopItemData> items;
        try
        {
            auto j = json::parse(resp.body);
            if (!resp.success || !j.contains("data")) { callback(items); return; }

            for (const auto& obj : j["data"])
            {
                ShopItemData item;
                item.id = obj.value("id", "");
                item.name = obj.value("name", "");
                item.description = obj.value("description", "");
                item.itemType = obj.value("item_type", "");
                item.price = obj.value("price", (i64_t)0);
                items.push_back(item);
            }
        }
        catch (...) {}
        callback(items);
        });
}

void CShopClient::Purchase(const string& itemId, PurchaseCallback callback)
{
    string body = "{\"item_id\":\"" + itemId + "\"}";

    m_pHttp->AsyncPost("/shop/purchase", body, [callback](const HttpResponse& resp) {
        PurchaseResult result;
        try
        {
            auto j = json::parse(resp.body);
            if (!resp.success) { result.error = j.value("error", ""); callback(result); return; }

            result.success = true;
            result.status = j["data"].value("status", "");
            result.owned = j["data"].value("owned", false);
            result.remainingCoins = j["data"].value("remaining_coins", (i64_t)0);
        }
        catch (const json::exception& e) { result.error = e.what(); }
        callback(result);
        });
}

void CShopClient::GetInventory(const string& userId, InventoryCallback callback)
{
    m_pHttp->AsyncGet("/shop/inventory/" + userId, [callback](const HttpResponse& resp) {
        vector<InventoryItemData> items;
        try
        {
            auto j = json::parse(resp.body);
            if (!resp.success || !j.contains("data")) { callback(items); return; }

            for (const auto& obj : j["data"])
            {
                InventoryItemData item;
                item.itemId = obj.value("item_id", "");
                item.name = obj.value("name", "");
                item.itemType = obj.value("item_type", "");
                item.quantity = obj.value("quantity", 0);
                items.push_back(item);
            }
        }
        catch (...) {}
        callback(items);
        });
}

void CShopClient::ProcessCallbacks() { m_pHttp->ProcessCallbacks(); }
