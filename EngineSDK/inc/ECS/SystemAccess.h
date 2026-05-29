#pragma once

#include <typeindex>
#include <vector>

#include "WintersTypes.h"

enum class eSystemAccessMode : u8_t
{
	Read,
	Write,
};

struct SystemComponentAccess
{
	std::type_index type{ typeid(void) };
	eSystemAccessMode mode = eSystemAccessMode::Read;
	const char* pszDebugName = nullptr;
};

struct SystemAccessDesc
{
	bool_t bUnknown = true;
	bool_t bWritesWorldStructure = false;
	std::vector<SystemComponentAccess> vecComponents;
};

class CSystemAccessBuilder
{
public:
	template<typename T>
	CSystemAccessBuilder& Read()
	{
		Add<T>(eSystemAccessMode::Read);
		return *this;
	}

	template<typename T>
	CSystemAccessBuilder& Write()
	{
		Add<T>(eSystemAccessMode::Write);
		return *this;
	}

	CSystemAccessBuilder& CreatesOrDestroysEntities()
	{
		m_desc.bUnknown = false;
		m_desc.bWritesWorldStructure = true;
		return *this;
	}

	CSystemAccessBuilder& UnknownWritesAll()
	{
		m_desc.bUnknown = true;
		m_desc.bWritesWorldStructure = true;
		m_desc.vecComponents.clear();
		return *this;
	}

	const SystemAccessDesc& GetDesc() const
	{
		return m_desc;
	}

private:
	template<typename T>
	void Add(eSystemAccessMode mode)
	{
		m_desc.bUnknown = false;
		m_desc.vecComponents.push_back(
			SystemComponentAccess{ std::type_index(typeid(T)), mode, typeid(T).name() });
	}

	SystemAccessDesc m_desc{};
};

inline bool SystemAccessConflicts(const SystemAccessDesc& lhs, const SystemAccessDesc& rhs)
{
	if (lhs.bUnknown || rhs.bUnknown)
		return true;
	if (lhs.bWritesWorldStructure || rhs.bWritesWorldStructure)
		return true;

	for (const SystemComponentAccess& a : lhs.vecComponents)
	{
		for (const SystemComponentAccess& b : rhs.vecComponents)
		{
			if (a.type != b.type)
				continue;
			if (a.mode == eSystemAccessMode::Write || b.mode == eSystemAccessMode::Write)
				return true;
		}
	}

	return false;
}
