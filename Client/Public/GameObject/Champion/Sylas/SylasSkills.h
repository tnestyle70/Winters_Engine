#pragma once

struct VisualHookContext;

namespace Sylas
{
	namespace Visual
	{
		void OnBACastFrame(VisualHookContext& ctx);
		void OnQCastFrame(VisualHookContext& ctx);
		void OnWCastFrame(VisualHookContext& ctx);
		void OnECastFrame(VisualHookContext& ctx);
		void OnRCastFrame(VisualHookContext& ctx);
	}
}
void SylasKeepAlive();
