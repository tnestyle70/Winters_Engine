#include "UI/ImageScenePresenter.h"

#include "Core/CInput.h"
#include "GameInstance.h"
#include "Resource/Texture.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

#include <algorithm>
#include <string>

namespace
{
	ImU32 ToImColor(const Vec4& vColor)
	{
		const auto clampByte = [](f32_t v) -> int
		{
			if (v < 0.f)
				v = 0.f;
			if (v > 1.f)
				v = 1.f;
			return static_cast<int>(v * 255.f);
		};

		return IM_COL32(
			clampByte(vColor.x),
			clampByte(vColor.y),
			clampByte(vColor.z),
			clampByte(vColor.w));
	}
}

CImageScenePresenter::~CImageScenePresenter()
{
	Shutdown();
}

bool_t CImageScenePresenter::Initialize(
	const wchar_t* pTexturePath,
	u32_t iSourceWidth,
	u32_t iSourceHeight)
{
	Shutdown();

	if (!pTexturePath || iSourceWidth == 0 || iSourceHeight == 0)
		return false;

	IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();
	if (!pDevice)
		return false;

	m_pTexture = Engine::CTexture::Create(
		pDevice,
		std::wstring(pTexturePath),
		Engine::eTexSamplerMode::Clamp,
		Engine::eTexColorSpace::IgnoreSRGB);

	m_iSourceWidth = iSourceWidth;
	m_iSourceHeight = iSourceHeight;
	return m_pTexture != nullptr;
}

void CImageScenePresenter::Shutdown()
{
	if (m_bFrameOpen)
		End();

	m_pTexture.reset();
	m_iSourceWidth = 0;
	m_iSourceHeight = 0;
	m_fDrawX = 0.f;
	m_fDrawY = 0.f;
	m_fDrawW = 0.f;
	m_fDrawH = 0.f;
}

void CImageScenePresenter::Render()
{
	if (!Begin())
		return;

	DrawBackground();
	End();
}

bool_t CImageScenePresenter::Begin()
{
	if (!m_pTexture || m_bFrameOpen)
		return false;

	const ImGuiIO& io = ImGui::GetIO();
	if (io.DisplaySize.x <= 0.f || io.DisplaySize.y <= 0.f)
		return false;

	UpdateLayout();
	const u32_t iScreenWidth = (std::max)(1u, static_cast<u32_t>(io.DisplaySize.x + 0.5f));
	const u32_t iScreenHeight = (std::max)(1u, static_cast<u32_t>(io.DisplaySize.y + 0.5f));
	m_bUseRHIFrame = CGameInstance::Get()->UI_Begin_RawImagePass(
		iScreenWidth,
		iScreenHeight,
		false);
	if (!m_bUseRHIFrame)
		return false;

	m_bFrameOpen = true;
	return true;
}

void CImageScenePresenter::DrawBackground()
{
	DrawBackgroundUV(Vec4(0.f, 0.f, 1.f, 1.f));
}

void CImageScenePresenter::DrawBackgroundUV(const Vec4& vUVRect)
{
	if (!m_bFrameOpen || !m_pTexture)
		return;

	if (m_bUseRHIFrame)
	{
		CGameInstance::Get()->UI_Draw_RawImage(
			m_pTexture->GetNativeSRV(),
			m_fDrawX,
			m_fDrawY,
			m_fDrawW,
			m_fDrawH,
			vUVRect,
		Vec4(1.f, 1.f, 1.f, 1.f));
		return;
	}
}

void CImageScenePresenter::DrawSourceImage(
	Engine::CTexture* pTexture,
	const ImageSourceRect& rect,
	const Vec4& vColor)
{
	DrawSourceImageUV(pTexture, rect, Vec4(0.f, 0.f, 1.f, 1.f), vColor);
}

void CImageScenePresenter::DrawSourceImageUV(Engine::CTexture* pTexture, 
	const ImageSourceRect& rect, const Vec4& vUVRect, const Vec4& vColor)
{
	if (!m_bFrameOpen || !pTexture)
		return;

	ImageScreenRect screen{};
	if (!SourceRectToScreen(rect, screen))
		return;

	if (m_bUseRHIFrame)
	{
		CGameInstance::Get()->UI_Draw_RawImage(
			pTexture->GetNativeSRV(),
			screen.fX,
			screen.fY,
			screen.fW,
			screen.fH,
			vUVRect,
			vColor);
		return;
	}
}

void CImageScenePresenter::DrawSourceRect(const ImageSourceRect& rect, const Vec4& vColor)
{
	if (!m_bFrameOpen)
		return;

	ImageScreenRect screen{};
	if (!SourceRectToScreen(rect, screen))
		return;

	if (m_bUseRHIFrame)
	{
		CGameInstance::Get()->UI_Draw_RawImage(
			nullptr,
			screen.fX,
			screen.fY,
			screen.fW,
			screen.fH,
			Vec4(0.f, 0.f, 1.f, 1.f),
			vColor);
		return;
	}

	ImGui::GetBackgroundDrawList()->AddRectFilled(
		ImVec2(screen.fX, screen.fY),
		ImVec2(screen.fX + screen.fW, screen.fY + screen.fH),
		ToImColor(vColor));
}

void CImageScenePresenter::DrawSourceRectOutline(
	const ImageSourceRect& rect,
	const Vec4& vColor,
	f32_t fThickness)
{
	if (!m_bFrameOpen)
		return;

	ImageScreenRect screen{};
	if (!SourceRectToScreen(rect, screen))
		return;

	if (m_bUseRHIFrame)
	{
		const f32_t fT = (std::max)(1.f, fThickness);
		CGameInstance::Get()->UI_Draw_RawImage(
			nullptr,
			screen.fX,
			screen.fY,
			screen.fW,
			fT,
			Vec4(0.f, 0.f, 1.f, 1.f),
			vColor);
		CGameInstance::Get()->UI_Draw_RawImage(
			nullptr,
			screen.fX,
			screen.fY + screen.fH - fT,
			screen.fW,
			fT,
			Vec4(0.f, 0.f, 1.f, 1.f),
			vColor);
		CGameInstance::Get()->UI_Draw_RawImage(
			nullptr,
			screen.fX,
			screen.fY,
			fT,
			screen.fH,
			Vec4(0.f, 0.f, 1.f, 1.f),
			vColor);
		CGameInstance::Get()->UI_Draw_RawImage(
			nullptr,
			screen.fX + screen.fW - fT,
			screen.fY,
			fT,
			screen.fH,
			Vec4(0.f, 0.f, 1.f, 1.f),
			vColor);
		return;
	}

	ImGui::GetBackgroundDrawList()->AddRect(
		ImVec2(screen.fX, screen.fY),
		ImVec2(screen.fX + screen.fW, screen.fY + screen.fH),
		ToImColor(vColor),
		0.f,
		0,
		fThickness);
}

void CImageScenePresenter::End()
{
	if (!m_bFrameOpen)
		return;

	if (m_bUseRHIFrame)
		CGameInstance::Get()->UI_End_RawImagePass();

	m_bUseRHIFrame = false;
	m_bFrameOpen = false;
}

bool_t CImageScenePresenter::WasSourceRectClicked(const ImageSourceRect& rect) const
{
	if (!CInput::Get().IsLButtonPressed())
		return false;

	f32_t fSourceX = 0.f;
	f32_t fSourceY = 0.f;
	if (!ScreenToSource(
		static_cast<f32_t>(CInput::Get().GetMouseX()),
		static_cast<f32_t>(CInput::Get().GetMouseY()),
		fSourceX,
		fSourceY))
	{
		return false;
	}

	return fSourceX >= rect.fLeft &&
		fSourceX <= rect.fRight &&
		fSourceY >= rect.fTop &&
		fSourceY <= rect.fBottom;
}

bool_t CImageScenePresenter::ScreenToSource(
	f32_t fScreenX,
	f32_t fScreenY,
	f32_t& outX,
	f32_t& outY) const
{
	if (m_iSourceWidth == 0 || m_iSourceHeight == 0)
		return false;

	UpdateLayout();

	if (m_fDrawW <= 0.f || m_fDrawH <= 0.f ||
		fScreenX < m_fDrawX || fScreenX > m_fDrawX + m_fDrawW ||
		fScreenY < m_fDrawY || fScreenY > m_fDrawY + m_fDrawH)
	{
		return false;
	}

	outX = (fScreenX - m_fDrawX) * static_cast<f32_t>(m_iSourceWidth) / m_fDrawW;
	outY = (fScreenY - m_fDrawY) * static_cast<f32_t>(m_iSourceHeight) / m_fDrawH;
	return true;
}

bool_t CImageScenePresenter::SourceRectToScreen(
	const ImageSourceRect& rect,
	ImageScreenRect& outRect) const
{
	if (m_iSourceWidth == 0 || m_iSourceHeight == 0)
		return false;

	UpdateLayout();

	const f32_t fScaleX = m_fDrawW / static_cast<f32_t>(m_iSourceWidth);
	const f32_t fScaleY = m_fDrawH / static_cast<f32_t>(m_iSourceHeight);
	outRect.fX = m_fDrawX + rect.fLeft * fScaleX;
	outRect.fY = m_fDrawY + rect.fTop * fScaleY;
	outRect.fW = (rect.fRight - rect.fLeft) * fScaleX;
	outRect.fH = (rect.fBottom - rect.fTop) * fScaleY;
	return outRect.fW > 0.f && outRect.fH > 0.f;
}

void CImageScenePresenter::UpdateLayout() const
{
	if (m_iSourceWidth == 0 || m_iSourceHeight == 0)
		return;

	const ImGuiIO& io = ImGui::GetIO();
	const f32_t fScreenW = io.DisplaySize.x;
	const f32_t fScreenH = io.DisplaySize.y;
	if (fScreenW <= 0.f || fScreenH <= 0.f)
		return;

	const f32_t fScale = (std::min)(
		fScreenW / static_cast<f32_t>(m_iSourceWidth),
		fScreenH / static_cast<f32_t>(m_iSourceHeight));

	m_fDrawW = static_cast<f32_t>(m_iSourceWidth) * fScale;
	m_fDrawH = static_cast<f32_t>(m_iSourceHeight) * fScale;
	m_fDrawX = (fScreenW - m_fDrawW) * 0.5f;
	m_fDrawY = (fScreenH - m_fDrawH) * 0.5f;
}
