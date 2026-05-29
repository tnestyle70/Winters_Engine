#ifndef Engine_Typedef_h__
#define Engine_Typedef_h__

namespace Engine
{
	// ── Boolean ──
	typedef		bool						bool_t;

	// ── Character ──
	typedef		char						char_t;
	typedef		wchar_t						tchar_t;
	typedef std::wstring wstring_t;

	// ── Floating Point ──
	typedef		float						f32_t;
	typedef		double						f64_t;

	// ── Integer ──
	typedef		int8_t						i8_t;
	typedef		int16_t						i16_t;
	typedef		int32_t						i32_t;
	typedef		int64_t						i64_t;
	typedef		uint8_t						u8_t;
	typedef		uint16_t					u16_t;
	typedef		uint32_t					u32_t;
	typedef		uint64_t					u64_t;

	// ── DirectXMath 저장용 (연산용 operator 없음, 별도 계산용 변수 필요) ──
	typedef		DirectX::XMFLOAT2			float2_t;
	typedef		DirectX::XMFLOAT3			float3_t;
	typedef		DirectX::XMFLOAT4			float4_t;
	typedef		DirectX::XMFLOAT4X4			float4x4_t;
}

#endif // Engine_Typedef_h__
