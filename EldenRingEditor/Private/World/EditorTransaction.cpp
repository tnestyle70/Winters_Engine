#include "World/EditorTransaction.h"

#include <algorithm>
#include <utility>

namespace
{
	size_t ClampInsertIndex(size_t index, size_t size)
	{
		return index < size ? index : size;
	}

	size_t FindPlacementIndex(const CWorldCellDocument& document, u32_t placementId)
	{
		const std::vector<WorldPlacement>& placements = document.Placements();
		for (size_t i = 0; i < placements.size(); ++i)
		{
			if (placements[i].id == placementId)
				return i;
		}
		return placements.size();
	}

	void RemovePlacementById(CWorldCellDocument& document, u32_t placementId)
	{
		std::vector<WorldPlacement>& placements = document.Placements();
		placements.erase(
			std::remove_if(
				placements.begin(),
				placements.end(),
				[placementId](const WorldPlacement& placement)
				{
					return placement.id == placementId;
				}),
			placements.end());
	}

	void InsertPlacementAt(CWorldCellDocument& document, const WorldPlacement& placement, size_t index)
	{
		RemovePlacementById(document, placement.id);

		std::vector<WorldPlacement>& placements = document.Placements();
		const size_t insertIndex = ClampInsertIndex(index, placements.size());
		placements.insert(placements.begin() + static_cast<std::ptrdiff_t>(insertIndex), placement);
	}

	void SelectPlacementIfAlive(const CWorldCellDocument* pDocument, u32_t* pSelectedPlacementId, u32_t placementId)
	{
		if (!pSelectedPlacementId)
			return;

		if (placementId == 0 || !pDocument || pDocument->FindPlacement(placementId))
		{
			*pSelectedPlacementId = placementId;
			return;
		}

		*pSelectedPlacementId = 0;
	}
}

void CEditorTransaction::Push(std::unique_ptr<IEditorCommand> pCommand)
{
	if (!pCommand)
		return;

	const char* const pName = pCommand->Name();
	pCommand->Do();
	m_historyLabels.emplace_back(pName ? pName : "Command");
	m_undoStack.push_back(std::move(pCommand));
	m_redoStack.clear();
}

void CEditorTransaction::Undo()
{
	if (m_undoStack.empty())
		return;

	std::unique_ptr<IEditorCommand> pCommand = std::move(m_undoStack.back());
	m_undoStack.pop_back();

	const char* const pName = pCommand->Name();
	pCommand->Undo();
	m_historyLabels.emplace_back(std::string("Undo: ") + (pName ? pName : "Command"));
	m_redoStack.push_back(std::move(pCommand));
}

void CEditorTransaction::Redo()
{
	if (m_redoStack.empty())
		return;

	std::unique_ptr<IEditorCommand> pCommand = std::move(m_redoStack.back());
	m_redoStack.pop_back();

	const char* const pName = pCommand->Name();
	pCommand->Do();
	m_historyLabels.emplace_back(std::string("Redo: ") + (pName ? pName : "Command"));
	m_undoStack.push_back(std::move(pCommand));
}

void CEditorTransaction::Clear()
{
	m_undoStack.clear();
	m_redoStack.clear();
	m_historyLabels.clear();
}

CAddPlacementCommand::CAddPlacementCommand(
	CWorldCellDocument* pDocument,
	WorldPlacement placement,
	u32_t* pSelectedPlacementId,
	u32_t selectionAfterUndo,
	size_t insertIndex)
	: m_pDocument(pDocument)
	, m_placement(std::move(placement))
	, m_pSelectedPlacementId(pSelectedPlacementId)
	, m_selectionAfterUndo(selectionAfterUndo)
	, m_insertIndex(insertIndex)
{
}

void CAddPlacementCommand::Do()
{
	if (!m_pDocument)
		return;

	InsertPlacementAt(*m_pDocument, m_placement, m_insertIndex);
	SelectPlacementIfAlive(m_pDocument, m_pSelectedPlacementId, m_placement.id);
}

void CAddPlacementCommand::Undo()
{
	if (!m_pDocument)
		return;

	RemovePlacementById(*m_pDocument, m_placement.id);
	SelectPlacementIfAlive(m_pDocument, m_pSelectedPlacementId, m_selectionAfterUndo);
}

CDeletePlacementCommand::CDeletePlacementCommand(
	CWorldCellDocument* pDocument,
	WorldPlacement placement,
	u32_t* pSelectedPlacementId,
	u32_t selectionAfterDo,
	size_t originalIndex)
	: m_pDocument(pDocument)
	, m_placement(std::move(placement))
	, m_pSelectedPlacementId(pSelectedPlacementId)
	, m_selectionAfterDo(selectionAfterDo)
	, m_originalIndex(originalIndex)
{
}

void CDeletePlacementCommand::Do()
{
	if (!m_pDocument)
		return;

	RemovePlacementById(*m_pDocument, m_placement.id);
	SelectPlacementIfAlive(m_pDocument, m_pSelectedPlacementId, m_selectionAfterDo);
}

void CDeletePlacementCommand::Undo()
{
	if (!m_pDocument)
		return;

	InsertPlacementAt(*m_pDocument, m_placement, m_originalIndex);
	SelectPlacementIfAlive(m_pDocument, m_pSelectedPlacementId, m_placement.id);
}

CTransformPlacementCommand::CTransformPlacementCommand(
	CWorldCellDocument* pDocument,
	u32_t placementId,
	const Vec3& oldPosition,
	const Vec3& oldRotationDeg,
	const Vec3& oldScale,
	const Vec3& newPosition,
	const Vec3& newRotationDeg,
	const Vec3& newScale,
	u32_t* pSelectedPlacementId)
	: m_pDocument(pDocument)
	, m_placementId(placementId)
	, m_oldPosition(oldPosition)
	, m_oldRotationDeg(oldRotationDeg)
	, m_oldScale(oldScale)
	, m_newPosition(newPosition)
	, m_newRotationDeg(newRotationDeg)
	, m_newScale(newScale)
	, m_pSelectedPlacementId(pSelectedPlacementId)
{
}

void CTransformPlacementCommand::Do()
{
	Apply(m_newPosition, m_newRotationDeg, m_newScale);
}

void CTransformPlacementCommand::Undo()
{
	Apply(m_oldPosition, m_oldRotationDeg, m_oldScale);
}

void CTransformPlacementCommand::Apply(const Vec3& position, const Vec3& rotationDeg, const Vec3& scale)
{
	if (!m_pDocument)
		return;

	WorldPlacement* pPlacement = m_pDocument->FindPlacement(m_placementId);
	if (!pPlacement)
	{
		SelectPlacementIfAlive(m_pDocument, m_pSelectedPlacementId, 0);
		return;
	}

	pPlacement->position = position;
	pPlacement->rotationDeg = rotationDeg;
	pPlacement->scale = scale;
	SelectPlacementIfAlive(m_pDocument, m_pSelectedPlacementId, m_placementId);
}
