#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"

#include <string>
#include <vector>

struct WorldPlacement
{
	u32_t id = 0;
	std::string kind;
	std::string name;
	std::string wmesh;
	Vec3 position{ 0.f, 0.f, 0.f };
	Vec3 rotationDeg{ 0.f, 0.f, 0.f };
	Vec3 scale{ 1.f, 1.f, 1.f };
	bool animated = false;
	bool transformResolved = true;
};

struct WorldReference
{
	std::string kind;
	std::string model;
	std::string reason;
};

class CWorldCellDocument
{
public:
	void Clear();

	bool Load(const std::string& strJsonRelative);
	bool Save(const std::string& strJsonRelative) const;

	const std::string& Schema() const { return m_schema; }
	const std::string& CellId() const { return m_cellId; }
	const std::string& DataLayer() const { return m_dataLayer; }
	u32_t Area() const { return m_area; }
	u32_t BlockX() const { return m_blockX; }
	u32_t BlockY() const { return m_blockY; }
	u32_t Variant() const { return m_variant; }
	f32_t CellSizeMeters() const { return m_cellSizeMeters; }
	const Vec3& Origin() const { return m_origin; }

	std::vector<WorldPlacement>& Placements() { return m_placements; }
	const std::vector<WorldPlacement>& Placements() const { return m_placements; }
	std::vector<WorldReference>& References() { return m_references; }
	const std::vector<WorldReference>& References() const { return m_references; }

	WorldPlacement* FindPlacement(u32_t id);
	const WorldPlacement* FindPlacement(u32_t id) const;
	u32_t AllocPlacementId();

private:
	std::string m_schema = "winters.world.cell.v1";
	std::string m_cellId = "untitled";
	u32_t m_area = 0;
	u32_t m_blockX = 0;
	u32_t m_blockY = 0;
	u32_t m_variant = 0;
	f32_t m_cellSizeMeters = 64.f;
	Vec3 m_origin{ 0.f, 0.f, 0.f };
	std::string m_dataLayer = "Base";
	std::vector<WorldPlacement> m_placements;
	std::vector<WorldReference> m_references;
	u32_t m_nextId = 1;
};
