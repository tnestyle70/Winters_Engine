#include "UI/ModelAnimPanel.h"

#include "Scene/Scene_InGame.h"
#include "ECS/World.h"
#include "ECS/Components/RenderComponent.h"
#include "ECS/Components/MeshGroupVisibilityComponent.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/ViegoSoulComponent.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/Champion/Yone/Yone_MeshGroups.h"
#include "Renderer/ModelRenderer.h"
#include "Resource/Animator.h"
#include "Resource/Animation.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

#include <cstdio>

namespace
{
	struct ModelAnimPanelState
	{
		EntityID selectedEntity = NULL_ENTITY;
		int selectedClip = -1;
		bool bPreviewActive = false;
		bool bLoop = true;
		bool bReverse = false;
		f32_t fPlaySpeed = 1.f;
		bool bPaused = false;
		f32_t fScrubTicks = 0.f;
	};
	ModelAnimPanelState g_State{};

	ModelRenderer* ResolveSelectedRenderer(CScene_InGame* pScene)
	{
		CWorld& world = pScene->GetWorld();
		if (g_State.selectedEntity == NULL_ENTITY ||
			!world.IsAlive(g_State.selectedEntity) ||
			!world.HasComponent<RenderComponent>(g_State.selectedEntity))
		{
			return nullptr;
		}
		return world.GetComponent<RenderComponent>(g_State.selectedEntity).pRenderer;
	}

	MeshGroupVisibilityComponent& EnsureVisibility(CWorld& world, EntityID entity)
	{
		return world.HasComponent<MeshGroupVisibilityComponent>(entity)
			? world.GetComponent<MeshGroupVisibilityComponent>(entity)
			: world.AddComponent<MeshGroupVisibilityComponent>(
				entity, MeshGroupVisibilityComponent{});
	}
}

namespace UI
{
	void CModelAnimPanel::Render(CScene_InGame* pScene)
	{
		if (!pScene)
			return;

		ImGui::SetNextWindowPos(ImVec2(60.f, 80.f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(560.f, 740.f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Model & Animation Lab"))
		{
			ImGui::End();
			return;
		}

		CWorld& world = pScene->GetWorld();

		ImGui::TextUnformatted("Target Champion");
		if (ImGui::BeginTable("##modelAnimTargets", 4,
			ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY,
			ImVec2(0.f, 140.f)))
		{
			ImGui::TableSetupColumn("Champion");
			ImGui::TableSetupColumn("Tag");
			ImGui::TableSetupColumn("NetId");
			ImGui::TableSetupColumn("Entity");
			ImGui::TableHeadersRow();

			world.ForEach<ChampionComponent, RenderComponent>(
				[&](EntityID entity, ChampionComponent& champ, RenderComponent& rc)
			{
				if (rc.bSceneManaged || !rc.pRenderer)
					return;
				if (world.HasComponent<ViegoSoulComponent>(entity))
					return;

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::PushID(static_cast<int>(entity));
				if (ImGui::Selectable(
					GetChampionDisplayName(champ.id),
					g_State.selectedEntity == entity,
					ImGuiSelectableFlags_SpanAllColumns))
				{
					g_State.selectedEntity = entity;
					g_State.selectedClip = -1;
					g_State.bPreviewActive = false;
					g_State.bPaused = false;
				}
				ImGui::PopID();
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(
					entity == pScene->GetPlayerEntity() ? "(local)" : "");
				ImGui::TableSetColumnIndex(2);
				const NetEntityId netId = pScene->GetEntityIdMap()
					? pScene->GetEntityIdMap()->ToNet(entity)
					: NULL_NET_ENTITY;
				ImGui::Text("%u", netId);
				ImGui::TableSetColumnIndex(3);
				ImGui::Text("%u", entity);
			});
			ImGui::EndTable();
		}
		ImGui::TextDisabled(
			"F10 Practice의 Spawn Champion으로 배치한 챔피언도 여기 나타난다.");
		ImGui::Separator();

		ModelRenderer* pRenderer = ResolveSelectedRenderer(pScene);
		if (!pRenderer)
		{
			g_State.selectedEntity = NULL_ENTITY;
			ImGui::TextUnformatted("Select a champion above.");
			ImGui::End();
			return;
		}

		if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const uint32 clipCount = pRenderer->GetAnimationCount();
			ImGui::Text("%u clips", clipCount);
			if (ImGui::BeginListBox("##modelAnimClips", ImVec2(-1.f, 150.f)))
			{
				for (uint32 i = 0; i < clipCount; ++i)
				{
					const char* pName = pRenderer->GetAnimationNameByIndex(i);
					if (!pName)
						continue;
					char label[160]{};
					std::snprintf(label, sizeof(label), "[%u] %s", i, pName);
					if (ImGui::Selectable(
						label, g_State.selectedClip == static_cast<int>(i)))
					{
						g_State.selectedClip = static_cast<int>(i);
						g_State.bPreviewActive = true;
						g_State.bPaused = false;
						pRenderer->PlayAnimationByIndexAdvanced(
							i, g_State.bLoop, g_State.bReverse, g_State.fPlaySpeed);
					}
				}
				ImGui::EndListBox();
			}

			ImGui::Checkbox("Preview Active", &g_State.bPreviewActive);
			ImGui::SameLine();
			ImGui::Checkbox("Loop", &g_State.bLoop);
			ImGui::SameLine();
			ImGui::Checkbox("Reverse", &g_State.bReverse);
			ImGui::SameLine();
			ImGui::Checkbox("Pause##modelAnim", &g_State.bPaused);

			ImGui::SetNextItemWidth(220.f);
			ImGui::SliderFloat("Speed", &g_State.fPlaySpeed, 0.05f, 4.f, "%.2fx");

			Engine::CAnimator* pAnimator = pRenderer->GetAnimator();
			const Engine::CAnimation* pCurrent =
				pAnimator ? pAnimator->GetCurrentAnimation() : nullptr;

			// 매 프레임 재주장: 로코모션/액션 리플리케이션이 상태 변화 시 클립을
			// 갈아치우므로 이름 가드로 비교해 다를 때만 다시 재생한다
			// (Scene_InGameNetwork의 PlayLoopNetworkActionIfNeeded 패턴).
			// OnImGui가 프레임 마지막에 실행되어 패널의 쓰기가 최종 승리한다.
			if (g_State.bPreviewActive && g_State.selectedClip >= 0 && pAnimator)
			{
				const char* pDesired = pRenderer->GetAnimationNameByIndex(
					static_cast<uint32>(g_State.selectedClip));
				if (pDesired && (!pCurrent || pCurrent->GetName() != pDesired))
				{
					pRenderer->PlayAnimationByIndexAdvanced(
						static_cast<uint32>(g_State.selectedClip),
						g_State.bLoop, g_State.bReverse, g_State.fPlaySpeed);
					pCurrent = pAnimator->GetCurrentAnimation();
				}

				const f32_t fApplied =
					g_State.bReverse ? -g_State.fPlaySpeed : g_State.fPlaySpeed;
				pAnimator->SetPlaySpeedRawForTool(g_State.bPaused ? 0.f : fApplied);
			}

			if (pCurrent && pAnimator)
			{
				const f64_t dDuration = pCurrent->GetDuration();
				const f64_t dTps = pCurrent->GetTicksPerSecond();
				const f64_t dSeconds =
					dTps > 0.0 ? pAnimator->GetCurrentTime() / dTps : 0.0;
				ImGui::Text("Now: %s  |  tick %.1f / %.1f  (%.2fs)",
					pCurrent->GetName().c_str(),
					pAnimator->GetCurrentFrame(),
					static_cast<f32_t>(dDuration),
					static_cast<f32_t>(dSeconds));

				if (g_State.bPaused)
				{
					g_State.fScrubTicks =
						static_cast<f32_t>(pAnimator->GetCurrentTime());
					ImGui::SetNextItemWidth(-1.f);
					if (ImGui::SliderFloat("##modelAnimScrub", &g_State.fScrubTicks,
						0.f, static_cast<f32_t>(dDuration), "tick %.1f"))
					{
						pAnimator->SetCurrentTimeTicksForTool(
							static_cast<f64_t>(g_State.fScrubTicks));
					}
				}
			}
			ImGui::TextDisabled(
				"Pause 후 슬라이더로 틱 단위 스크럽. Preview Active 해제 시 게임이 다시 제어.");
		}

		if (ImGui::CollapsingHeader(
			"Submesh Decomposition", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const uint32 meshCount = pRenderer->GetMeshCount();
			const uint32 infoCount = pRenderer->GetSubmeshInfoCount();
			ImGui::Text("%u meshes (%u named)", meshCount, infoCount);

			bool bRestoredDefault = false;
			if (ImGui::Button("Restore Default"))
			{
				// AttachVisual과 동일 규칙: Yone은 기본 마스크, 그 외는 컴포넌트 제거.
				const ChampionComponent& champ =
					world.GetComponent<ChampionComponent>(g_State.selectedEntity);
				if (static_cast<eChampion>(champ.id) == eChampion::YONE)
				{
					auto& visibility = EnsureVisibility(world, g_State.selectedEntity);
					visibility.mask = Yone::MeshGroups::MaskBaseDefault();
					visibility.bEnabled = true;
				}
				else if (world.HasComponent<MeshGroupVisibilityComponent>(
					g_State.selectedEntity))
				{
					world.RemoveComponent<MeshGroupVisibilityComponent>(
						g_State.selectedEntity);
				}
				bRestoredDefault = true;
			}

			if (!bRestoredDefault)
			{
				auto& visibility = EnsureVisibility(world, g_State.selectedEntity);
				visibility.bEnabled = true;

				ImGui::SameLine();
				if (ImGui::Button("All"))
					visibility.mask = MakeAllVisibleMask();
				ImGui::SameLine();
				if (ImGui::Button("None"))
					visibility.mask = VisibilityMask{};

				for (uint32 i = 0; i < meshCount; ++i)
				{
					const char* pName =
						i < infoCount ? pRenderer->GetSubmeshNameByIndex(i) : nullptr;
					char label[96]{};
					if (pName && pName[0] != '\0')
					{
						std::snprintf(label, sizeof(label), "[%u] %s (mat %u)",
							i, pName, pRenderer->GetSubmeshMaterialIndexByIndex(i));
					}
					else
					{
						std::snprintf(label, sizeof(label), "[%u] mesh", i);
					}

					ImGui::PushID(static_cast<int>(i) + 2000);
					bool bVisible = IsSubmeshVisible(visibility.mask, i);
					if (ImGui::Checkbox(label, &bVisible))
						SetSubmeshVisible(visibility.mask, i, bVisible);
					ImGui::SameLine();
					if (ImGui::SmallButton("Solo"))
					{
						visibility.mask = VisibilityMask{};
						SetSubmeshVisible(visibility.mask, i, true);
					}
					ImGui::PopID();
				}
			}
			ImGui::TextDisabled(
				"발견한 파트 인덱스는 Yone_MeshGroups.h 패턴의 네임드 마스크로 박제 권장.");
		}

		ImGui::End();
	}
}
