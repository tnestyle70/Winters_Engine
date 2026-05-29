#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/Schemas/Generated/cpp/Command_generated.h"

class CGameRoom;

class CCommandDispatcher
{
public:
    void Dispatch(u32_t sessionId, const Shared::Schema::CommandBatch* batch,
        CGameRoom& room);
};
