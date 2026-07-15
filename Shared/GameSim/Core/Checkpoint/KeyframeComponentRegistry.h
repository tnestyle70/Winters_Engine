#pragma once
// Chrono Break P1: 컴포넌트 스토어 키프레임 레지스트리.
// 서버 sim 월드에 존재할 수 있는 모든 컴포넌트 타입은 여기(또는 소유 TU의
// 자기등록)에 등록되어야 한다. WorldKeyframe::Save가 미등록 스토어를 발견하면
// 하드 실패한다(완전성 기계 검사) — 새 타입 추가가 조용한 복원 누락이 되지 않게 한다.
#include "Shared/GameSim/Core/World/World.h"
#include "WintersTypes.h"

#include <cstring>
#include <limits>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace SimCheckpoint
{
	inline u64_t Fnv1a64(const char* pText)
	{
		u64_t hash = 14695981039346656037ull;
		for (const char* p = pText; *p; ++p)
		{
			hash ^= static_cast<u8_t>(*p);
			hash *= 1099511628211ull;
		}
		return hash;
	}

	namespace Detail
	{
		inline void WriteU64(std::vector<u8_t>& out, u64_t v)
		{
			const size_t base = out.size();
			out.resize(base + sizeof(u64_t));
			std::memcpy(out.data() + base, &v, sizeof(u64_t));
		}
		inline bool_t ReadU64(const u8_t*& p, const u8_t* pEnd, u64_t& out)
		{
			if (p > pEnd || static_cast<size_t>(pEnd - p) < sizeof(u64_t))
				return false;
			std::memcpy(&out, p, sizeof(u64_t));
			p += sizeof(u64_t);
			return true;
		}
		template<typename E>
		void WriteVector(std::vector<u8_t>& out, const std::vector<E>& v)
		{
			WriteU64(out, static_cast<u64_t>(v.size()));
			const size_t bytes = v.size() * sizeof(E);
			const size_t base = out.size();
			out.resize(base + bytes);
			if (bytes)
				std::memcpy(out.data() + base, v.data(), bytes);
		}
		template<typename E>
		bool_t ReadVector(const u8_t*& p, const u8_t* pEnd, std::vector<E>& out)
		{
			u64_t count = 0;
			if (!ReadU64(p, pEnd, count))
				return false;
			if (count > static_cast<u64_t>((std::numeric_limits<size_t>::max)()))
				return false;
			const size_t elementCount = static_cast<size_t>(count);
			const size_t remaining = static_cast<size_t>(pEnd - p);
			if (elementCount > remaining / sizeof(E))
				return false;
			const size_t bytes = elementCount * sizeof(E);
			out.resize(elementCount);
			if (bytes)
				std::memcpy(out.data(), p, bytes);
			p += bytes;
			return true;
		}

		inline bool_t ValidateComponentStoreTopology(
			const CWorld& world,
			const std::vector<uint32_t>& sparse,
			const std::vector<EntityID>& dense)
		{
			const auto& slots = world.GetEntityManager().RawSlots();
			if (sparse.size() > slots.size())
				return false;

			constexpr uint32_t kInvalid = UINT32_MAX;
			std::vector<u8_t> seenDense(slots.size(), 0u);
			for (size_t index = 0u; index < dense.size(); ++index)
			{
				const EntityID entity = dense[index];
				if (entity == NULL_ENTITY ||
					entity >= slots.size() ||
					!world.IsAlive(entity) ||
					entity >= sparse.size() ||
					sparse[entity] != index ||
					seenDense[entity] != 0u)
				{
					return false;
				}
				seenDense[entity] = 1u;
			}

			for (size_t entity = 0u; entity < sparse.size(); ++entity)
			{
				const uint32_t denseIndex = sparse[entity];
				if (denseIndex == kInvalid)
					continue;
				if (denseIndex >= dense.size() ||
					dense[denseIndex] != entity)
				{
					return false;
				}
			}
			return true;
		}
	}

	struct KeyframeStoreOps
	{
		std::string stableName;
		u64_t nameHash = 0;
		bool_t (*save)(const CWorld& world, std::vector<u8_t>& out) = nullptr;
		bool_t (*load)(CWorld& world, const u8_t* pPayload, size_t size) = nullptr;
	};

	class KeyframeComponentRegistry
	{
	public:
		static KeyframeComponentRegistry& Get()
		{
			static KeyframeComponentRegistry s_instance;
			return s_instance;
		}

		template<typename T>
		bool_t Register(const char* pStableName)
		{
			static_assert(std::is_trivially_copyable_v<T>,
				"keyframe component must be trivially copyable - use RegisterCustom with a codec");
			return RegisterOps(std::type_index(typeid(T)), pStableName,
				&PodSave<T>, &PodLoad<T>);
		}

		bool_t RegisterCustom(std::type_index ti, const char* pStableName,
			bool_t (*save)(const CWorld&, std::vector<u8_t>&),
			bool_t (*load)(CWorld&, const u8_t*, size_t))
		{
			return RegisterOps(ti, pStableName, save, load);
		}

		const KeyframeStoreOps* Find(std::type_index ti) const
		{
			auto it = m_byType.find(ti);
			return it != m_byType.end() ? &it->second : nullptr;
		}
		const KeyframeStoreOps* FindByHash(u64_t nameHash) const
		{
			auto it = m_byHash.find(nameHash);
			return it != m_byHash.end() ? it->second : nullptr;
		}
		template<typename Fn> void ForEachOps(Fn&& fn) const
		{
			for (const auto& [ti, ops] : m_byType)
				fn(ops);
		}

	private:
		KeyframeComponentRegistry() = default;

		template<typename T>
		static bool_t PodSave(const CWorld& world, std::vector<u8_t>& out)
		{
			const CComponentStore<T>* pStore = world.Checkpoint_TryGetStore<T>();
			if (!pStore)
			{
				Detail::WriteVector(out, std::vector<uint32_t>{});
				Detail::WriteVector(out, std::vector<EntityID>{});
				Detail::WriteVector(out, std::vector<T>{});
				return true;
			}
			Detail::WriteVector(out, pStore->RawSparse());
			Detail::WriteVector(out, pStore->RawDense());
			Detail::WriteVector(out, pStore->RawData());
			return true;
		}

		template<typename T>
		static bool_t PodLoad(CWorld& world, const u8_t* pPayload, size_t size)
		{
			const u8_t* p = pPayload;
			const u8_t* pEnd = pPayload + size;
			std::vector<uint32_t> sparse;
			std::vector<EntityID> dense;
			std::vector<T> data;
			if (!Detail::ReadVector(p, pEnd, sparse) ||
				!Detail::ReadVector(p, pEnd, dense) ||
				!Detail::ReadVector(p, pEnd, data) ||
				p != pEnd ||
				dense.size() != data.size() ||
				!Detail::ValidateComponentStoreTopology(
					world,
					sparse,
					dense))
			{
				return false;
			}
			world.Checkpoint_GetOrCreateStore<T>().RestoreRaw(
				std::move(sparse), std::move(dense), std::move(data));
			return true;
		}

		bool_t RegisterOps(std::type_index ti, const char* pStableName,
			bool_t (*save)(const CWorld&, std::vector<u8_t>&),
			bool_t (*load)(CWorld&, const u8_t*, size_t))
		{
			KeyframeStoreOps ops{};
			ops.stableName = pStableName;
			ops.nameHash = Fnv1a64(pStableName);
			ops.save = save;
			ops.load = load;
			auto [it, bInserted] = m_byType.emplace(ti, std::move(ops));
			if (bInserted)
				m_byHash[it->second.nameHash] = &it->second;
			return true;
		}

		std::unordered_map<std::type_index, KeyframeStoreOps> m_byType;
		std::unordered_map<u64_t, const KeyframeStoreOps*> m_byHash;
	};
}
