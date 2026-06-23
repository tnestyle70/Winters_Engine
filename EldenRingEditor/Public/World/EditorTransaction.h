#pragma once

#include "World/WorldCellDocument.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class IEditorCommand
{
public:
	virtual ~IEditorCommand() = default;

	virtual void Do() = 0;
	virtual void Undo() = 0;
	virtual const char* Name() const = 0;
};

class CEditorTransaction
{
public:
	void Push(std::unique_ptr<IEditorCommand> pCommand);
	bool CanUndo() const { return !m_undoStack.empty(); }
	bool CanRedo() const { return !m_redoStack.empty(); }
	void Undo();
	void Redo();
	void Clear();

	size_t UndoCount() const { return m_undoStack.size(); }
	size_t RedoCount() const { return m_redoStack.size(); }
	const std::vector<std::string>& History() const { return m_historyLabels; }

private:
	std::vector<std::unique_ptr<IEditorCommand>> m_undoStack;
	std::vector<std::unique_ptr<IEditorCommand>> m_redoStack;
	std::vector<std::string> m_historyLabels;
};

class CAddPlacementCommand final : public IEditorCommand
{
public:
	CAddPlacementCommand(
		CWorldCellDocument* pDocument,
		WorldPlacement placement,
		u32_t* pSelectedPlacementId,
		u32_t selectionAfterUndo,
		size_t insertIndex);

	void Do() override;
	void Undo() override;
	const char* Name() const override { return "Add Placement"; }

private:
	CWorldCellDocument* m_pDocument = nullptr;
	WorldPlacement m_placement;
	u32_t* m_pSelectedPlacementId = nullptr;
	u32_t m_selectionAfterUndo = 0;
	size_t m_insertIndex = 0;
};

class CDeletePlacementCommand final : public IEditorCommand
{
public:
	CDeletePlacementCommand(
		CWorldCellDocument* pDocument,
		WorldPlacement placement,
		u32_t* pSelectedPlacementId,
		u32_t selectionAfterDo,
		size_t originalIndex);

	void Do() override;
	void Undo() override;
	const char* Name() const override { return "Delete Placement"; }

private:
	CWorldCellDocument* m_pDocument = nullptr;
	WorldPlacement m_placement;
	u32_t* m_pSelectedPlacementId = nullptr;
	u32_t m_selectionAfterDo = 0;
	size_t m_originalIndex = 0;
};

class CTransformPlacementCommand final : public IEditorCommand
{
public:
	CTransformPlacementCommand(
		CWorldCellDocument* pDocument,
		u32_t placementId,
		const Vec3& oldPosition,
		const Vec3& oldRotationDeg,
		const Vec3& oldScale,
		const Vec3& newPosition,
		const Vec3& newRotationDeg,
		const Vec3& newScale,
		u32_t* pSelectedPlacementId);

	void Do() override;
	void Undo() override;
	const char* Name() const override { return "Transform Placement"; }

private:
	void Apply(const Vec3& position, const Vec3& rotationDeg, const Vec3& scale);

	CWorldCellDocument* m_pDocument = nullptr;
	u32_t m_placementId = 0;
	Vec3 m_oldPosition{ 0.f, 0.f, 0.f };
	Vec3 m_oldRotationDeg{ 0.f, 0.f, 0.f };
	Vec3 m_oldScale{ 1.f, 1.f, 1.f };
	Vec3 m_newPosition{ 0.f, 0.f, 0.f };
	Vec3 m_newRotationDeg{ 0.f, 0.f, 0.f };
	Vec3 m_newScale{ 1.f, 1.f, 1.f };
	u32_t* m_pSelectedPlacementId = nullptr;
};
