#ifndef Engine_Macro_h__
#define Engine_Macro_h__

// ── Enum 캐스팅 ──────────────────────────────────────────────────
#define ETOI(_enum)		static_cast<int32_t>(_enum)
#define ETOUI(_enum)	static_cast<uint32_t>(_enum)

// ── 메시지 박스 ──────────────────────────────────────────────────
#ifndef MSG_BOX
#define MSG_BOX(_message) MessageBox(nullptr, TEXT(_message), L"System Message", MB_OK)
#endif

// ── 네임스페이스 ─────────────────────────────────────────────────
#define NS_BEGIN(_namespace) namespace _namespace {
#define NS_END }
#define USING(_namespace) using namespace _namespace;

// ── DLL Export/Import (기존 WINTERS_API 와 공존) ─────────────────
#ifdef WINTERS_ENGINE_EXPORTS
#define ENGINE_DLL __declspec(dllexport)
#else
#define ENGINE_DLL __declspec(dllimport)
#endif

// ── NULL 체크 ────────────────────────────────────────────────────
#define NULL_CHECK(_ptr)	\
	{ if ((_ptr) == nullptr) { return; } }

#define NULL_CHECK_RETURN(_ptr, _return)	\
	{ if ((_ptr) == nullptr) { return _return; } }

#define NULL_CHECK_MSG(_ptr, _message)	\
	{ if ((_ptr) == nullptr) { MessageBox(NULL, _message, L"System Message", MB_OK); } }

#define NULL_CHECK_RETURN_MSG(_ptr, _return, _message)	\
	{ if ((_ptr) == nullptr) { MessageBox(NULL, _message, L"System Message", MB_OK); return _return; } }

// ── HRESULT 체크 ─────────────────────────────────────────────────
#define FAILED_CHECK(_hr)	if (((HRESULT)(_hr)) < 0)	\
	{ MessageBoxW(NULL, L"Failed", L"System Error", MB_OK); return E_FAIL; }

#define FAILED_CHECK_RETURN(_hr, _return)	if (((HRESULT)(_hr)) < 0)	\
	{ MessageBoxW(NULL, L"Failed", L"System Error", MB_OK); return _return; }

#define FAILED_CHECK_MSG(_hr, _message)	if (((HRESULT)(_hr)) < 0)	\
	{ MessageBoxW(NULL, _message, L"System Message", MB_OK); return E_FAIL; }

#define FAILED_CHECK_RETURN_MSG(_hr, _return, _message)	if (((HRESULT)(_hr)) < 0)	\
	{ MessageBoxW(NULL, _message, L"System Message", MB_OK); return _return; }

// ── 싱글톤 ──────────────────────────────────────────────────────
#define NO_COPY(CLASSNAME)	\
	private:	\
		CLASSNAME(const CLASSNAME&) = delete;	\
		CLASSNAME& operator=(const CLASSNAME&) = delete;

#define DECLARE_SINGLETON(CLASSNAME)	\
	NO_COPY(CLASSNAME)	\
	public:	\
		static CLASSNAME* Get()	\
		{	\
			static CLASSNAME instance;	\
			return &instance;	\
		}	\
	private:

#define Safe_Reset(_smart_ptr)	\
	do { if ((_smart_ptr)) { (_smart_ptr).reset(); } } while (0)

#endif // Engine_Macro_h__
