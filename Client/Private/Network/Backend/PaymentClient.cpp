#include "Network/Backend/PaymentClient.h"
#pragma push_macro("new")
#undef new
#include "Network/Backend/json.hpp"
#pragma pop_macro("new")

using json = nlohmann::json;

void CPaymentClient::SetAuthToken(const string& token)
{
	m_pHttp->SetAuthToken(token);
}

void CPaymentClient::Charge(i64_t coinAmount, const string & idempotencyKey, 
	ChargeCallback callback)
{
    string body = "{\"gateway\":\"mock\",\"coin_amount\":"
        + std::to_string(coinAmount)
        + ",\"idempotency_key\":\"" + idempotencyKey
        + "\",\"receipt_data\":\"client\"}";

    m_pHttp->AsyncPost("/payment/charge", body, [callback](const HttpResponse& resp) {
        ChargeResult result;
        try
        {
            auto j = json::parse(resp.body);
            if (!resp.success) { result.error = j.value("error", ""); callback(result); return; }

            auto data = j["data"];
            result.success = true;
            result.txId = data.value("tx_id", "");
            result.balance = data.value("balance", (i64_t)0);
            result.charged = data.value("charged", (i64_t)0);
        }
        catch (const json::exception& e) { result.error = e.what(); }
        callback(result);
        });
}

void CPaymentClient::GetBalance(BalanceCallback callback)
{
    m_pHttp->AsyncGet("/payment/balance", [callback](const HttpResponse& resp) {
        BalanceResult result;
        try
        {
            auto j = json::parse(resp.body);
            if (!resp.success) { result.error = j.value("error", ""); callback(result); return; }

            result.success = true;
            result.balance = j["data"].value("balance", (i64_t)0);
        }
        catch (const json::exception& e) { result.error = e.what(); }
        callback(result);
        });
}

void CPaymentClient::ProcessCallbacks()
{
    m_pHttp->ProcessCallbacks();
}

unique_ptr<CPaymentClient> CPaymentClient::Create(const string & baseURL)
{
	auto pInstance = unique_ptr<CPaymentClient>(new CPaymentClient());
	pInstance->m_pHttp = CHttpClient::Create(baseURL);
	return pInstance;
}
