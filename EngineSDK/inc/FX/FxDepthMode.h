#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"

enum class eFxDepthMode : u8_t
{
	DepthTestWriteOn = 0,
	DepthTestWriteOff = 1,
	OverlayNoDepth = 2,
	SoftParticle = 3,
};

inline bool_t FxDepthModeTestsDepth(eFxDepthMode mode)
{
	return mode == eFxDepthMode::DepthTestWriteOn ||
		mode == eFxDepthMode::DepthTestWriteOff ||
		mode == eFxDepthMode::SoftParticle;
}

inline bool_t FxDepthModeWritesDepth(eFxDepthMode mode)
{
	return mode == eFxDepthMode::DepthTestWriteOn;
}

inline bool_t FxDepthModeUsesSoftParticle(eFxDepthMode mode)
{
	return mode == eFxDepthMode::SoftParticle;
}

inline eFxDepthMode FxDepthModeFromDepthWrite(bool_t bDepthWrite)
{
	return bDepthWrite ? eFxDepthMode::DepthTestWriteOn : eFxDepthMode::DepthTestWriteOff;
}