#include "Shared/Network/PacketEnvelope.h"
#include "Shared/Schemas/Generated/cpp/Command_generated.h"
#include "Shared/Schemas/Generated/cpp/Event_generated.h"
#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"

#include <type_traits>
#include <vector>

namespace
{
    static_assert(sizeof(PacketHeader) == 16);
    static_assert(sizeof(Shared::Schema::Vec3) == 12);
    static_assert(static_cast<int>(Shared::Schema::CommandKind::CastSkill) == 2);
    static_assert(static_cast<int>(Shared::Schema::CommandKind::BasicAttack) == 3);
    static_assert(static_cast<int>(Shared::Schema::EventKind::Damage) == 1);

    void SchemaSmoke_CompileOnly()
    {
        flatbuffers::FlatBufferBuilder builder(1024);

        Shared::Schema::Vec3 groundPos(0.f, 1.f, 0.f);
        Shared::Schema::Vec3 direction(1.f, 0.f, 0.f);

        auto cmd = Shared::Schema::CreateCommandPacket(
            builder,
            Shared::Schema::CommandKind::CastSkill,
            42,
            100,
            1,
            7,
            &groundPos,
            &direction,
            0,
            0);

        std::vector<flatbuffers::Offset<Shared::Schema::CommandPacket>> commands;
        commands.push_back(cmd);

        auto batch = Shared::Schema::CreateCommandBatchDirect(builder, &commands, 1234);
        builder.Finish(batch);

        flatbuffers::Verifier verifier(builder.GetBufferPointer(), builder.GetSize());
        const bool verified = Shared::Schema::VerifyCommandBatchBuffer(verifier);
        (void)verified;

        auto envelope = WrapEnvelope(
            ePacketType::CommandBatch,
            1,
            builder.GetBufferPointer(),
            static_cast<u32_t>(builder.GetSize()));

        ParsedFrame frame{};
        u32_t consumed = 0;
        const bool extracted = TryExtractFrame(
            envelope.data(),
            static_cast<u32_t>(envelope.size()),
            frame,
            consumed);

        (void)extracted;
        (void)frame;
        (void)consumed;
    }
}
