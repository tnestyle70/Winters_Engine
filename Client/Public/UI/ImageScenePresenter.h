#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"
#include "Resource/Texture.h"

#include <memory>

struct ImageSourceRect
{
	f32_t fLeft = 0.f;
	f32_t fTop = 0.f;
	f32_t fRight = 0.f;
	f32_t fBottom = 0.f;
};

struct ImageScreenRect
{
	f32_t fX = 0.f;
	f32_t fY = 0.f;
	f32_t fW = 0.f;
	f32_t fH = 0.f;
};

class CImageScenePresenter final
{
public:
	CImageScenePresenter() = default;
	~CImageScenePresenter();

	CImageScenePresenter(const CImageScenePresenter&) = delete;
	CImageScenePresenter& operator=(const CImageScenePresenter&) = delete;

	bool_t Initialize(const wchar_t* pTexturePath, u32_t iSourceWidth, u32_t iSourceHeight);
	void Shutdown();

	void Render();
	bool_t Begin();
	void DrawBackground();
	void DrawBackgroundUV(const Vec4& vUVRect);
	void DrawSourceImage(Engine::CTexture* pTexture, const ImageSourceRect& rect, const Vec4& vColor);
	void DrawSourceImageUV(Engine::CTexture* pTexture, const ImageSourceRect& rect,
		const Vec4& vUVRect, const Vec4& vColor);
	void DrawSourceRect(const ImageSourceRect& rect, const Vec4& vColor);
	void DrawSourceRectOutline(const ImageSourceRect& rect, const Vec4& vColor, f32_t fThickness = 1.f);
	void End();

	bool_t WasSourceRectClicked(const ImageSourceRect& rect) const;
	bool_t ScreenToSource(f32_t fScreenX, f32_t fScreenY, f32_t& outX, f32_t& outY) const;
	bool_t SourceRectToScreen(const ImageSourceRect& rect, ImageScreenRect& outRect) const;

private:
	void UpdateLayout() const;

	std::unique_ptr<Engine::CTexture> m_pTexture{};
	u32_t m_iSourceWidth = 0;
	u32_t m_iSourceHeight = 0;
	mutable f32_t m_fDrawX = 0.f;
	mutable f32_t m_fDrawY = 0.f;
	mutable f32_t m_fDrawW = 0.f;
	mutable f32_t m_fDrawH = 0.f;
	bool_t m_bFrameOpen = false;
	bool_t m_bUseRHIFrame = false;
};
