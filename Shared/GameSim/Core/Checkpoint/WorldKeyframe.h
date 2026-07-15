#pragma once
// Chrono Break P1: 비트 정확 sim 키프레임 저장/복원.
// 내용 = 엔티티 할당자(slots/freeHead/aliveCount) + 등록된 전 컴포넌트 스토어의
// raw sparse/dense/data + RNG 상태 + EntityIdMap + 틱 인덱스.
// dense 순서를 바이트 그대로 복원하므로, 복원 후 재실행은 무중단 실행과
// 동일한 결정론 궤적을 갖는다 (SimLab RunKeyframeRestoreDeterminismProbe가 게이트).
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Core/Determinism/DeterministicRng.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "WintersTypes.h"

#include <vector>

namespace SimCheckpoint
{
	// 실패 시 std::cerr로 원인(미등록 스토어 typeid 이름 등)을 남기고 false.
	bool_t SaveWorldKeyframe(const CWorld& world, const DeterministicRng& rng,
		const EntityIdMap& entityMap, u64_t tickIndex, std::vector<u8_t>& outBytes);

	// Strong failure guarantee: false leaves world, RNG, entityMap and
	// outTickIndex unchanged. Decode is staged and committed only after full validation.
	bool_t RestoreWorldKeyframe(CWorld& world, DeterministicRng& rng,
		EntityIdMap& entityMap, u64_t& outTickIndex, const std::vector<u8_t>& bytes);
}
