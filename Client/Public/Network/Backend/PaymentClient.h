#pragma once
#include "Defines.h"
#include "Network/Backend/CHttpClient.h"

NS_BEGIN(Client)

struct ChargeResult
{
	bool_t success = false;
	string txId;
	i64_t balance = 0;
	i64_t charged = 0;
	string error;
};

struct BalanceResult
{
	bool_t success = false;
	i64_t balance = 0;
	string error;
};

using ChargeCallback = function<void(const ChargeResult&)>;
using BalanceCallback = function<void(const BalanceResult&)>;

class CPaymentClient
{
private:
	CPaymentClient() = default;
public:
	~CPaymentClient() = default;

	void SetAuthToken(const string& token);
	void Charge(i64_t coinAmount, const string& idempotencyKey, ChargeCallback callback);
	void GetBalance(BalanceCallback callback);
	void ProcessCallbacks();

	static unique_ptr<CPaymentClient> Create(const string& baseURL);
private:
	unique_ptr<CHttpClient> m_pHttp;
};

NS_END
