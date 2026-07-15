#include "Game/ReplayRecorder.h"
#include "Shared/Replay/ReplayFormat.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <type_traits>

namespace
{
    bool ReadExact(std::ifstream& input, void* pOut, size_t size)
    {
        input.read(
            reinterpret_cast<char*>(pOut),
            static_cast<std::streamsize>(size));
        return input.good();
    }
}

static_assert(sizeof(Winters::Replay::ReplayCommandPayload) == 60u);
static_assert(offsetof(Winters::Replay::ReplayCommandPayload, reserved0) == 42u);
static_assert(offsetof(Winters::Replay::ReplayCommandPayload, clientTick) == 52u);
static_assert(std::is_standard_layout_v<Winters::Replay::ReplayCommandPayload>);
static_assert(std::is_trivially_copyable_v<Winters::Replay::ReplayCommandPayload>);
static_assert(static_cast<u16_t>(
    Winters::Replay::eReplayCommandDomain::LegacyV2PlayerInput) == 0u);
static_assert(static_cast<u16_t>(
    Winters::Replay::eReplayCommandDomain::PlayerInput) == 1u);
static_assert(static_cast<u16_t>(
    Winters::Replay::eReplayCommandDomain::AuthoringMutation) == 2u);
static_assert(static_cast<u16_t>(
    Winters::Replay::eReplayCommandDomain::ControlPlane) == 3u);
static_assert(static_cast<u16_t>(
    Winters::Replay::eReplayCommandDomain::ObservationOnly) == 4u);

int main()
{
    using namespace Winters::Replay;

    ReplayCommandPayload legacyV2{};
    const bool_t bV1ReaderCompatible =
        IsSupportedReplayVersion(1u) &&
        IsReplayRecordTypeSupported(
            1u, static_cast<u8_t>(eReplayRecordType::Snapshot)) &&
        IsReplayRecordTypeSupported(
            1u, static_cast<u8_t>(eReplayRecordType::Event)) &&
        !IsReplayRecordTypeSupported(
            1u, static_cast<u8_t>(eReplayRecordType::Command));
    const bool_t bLegacyV2ZeroIsPlayerInput =
        GetReplayCommandDomain(legacyV2) ==
            eReplayCommandDomain::LegacyV2PlayerInput &&
        IsFaithfulResimCommandDomain(GetReplayCommandDomain(legacyV2)) &&
        IsReplayCommandPayloadSupported(
            2u, &legacyV2, static_cast<u32_t>(sizeof(legacyV2)));

    ReplayCommandPayload unknownDomain = legacyV2;
    unknownDomain.reserved0 = 0xFFFFu;
    const bool_t bCommandPayloadValidation =
        IsReplayRecordTypeSupported(
            2u, static_cast<u8_t>(eReplayRecordType::Command)) &&
        !IsReplayCommandPayloadSupported(
            1u, &legacyV2, static_cast<u32_t>(sizeof(legacyV2))) &&
        !IsReplayCommandPayloadSupported(
            2u, &legacyV2, static_cast<u32_t>(sizeof(legacyV2) - 1u)) &&
        !IsReplayCommandPayloadSupported(
            2u, &unknownDomain, static_cast<u32_t>(sizeof(unknownDomain)));

    const bool_t bFaithfulResimContract =
        IsFaithfulResimCommandDomain(eReplayCommandDomain::PlayerInput) &&
        IsFaithfulResimCommandDomain(eReplayCommandDomain::AuthoringMutation) &&
        !IsFaithfulResimCommandDomain(eReplayCommandDomain::ControlPlane) &&
        !IsFaithfulResimCommandDomain(eReplayCommandDomain::ObservationOnly) &&
        ShouldJournalReplayCommand(eReplayJournalOutcome::SubmittedPlayerInput) &&
        ShouldJournalReplayCommand(eReplayJournalOutcome::AcceptedToolCommand) &&
        !ShouldJournalReplayCommand(eReplayJournalOutcome::RejectedToolCommand) &&
        !ShouldJournalReplayCommand(eReplayJournalOutcome::PausedGameplayVoid);

    const bool_t bToolRevisionContract =
        ShouldAdvanceToolRevision(
            eReplayCommandDomain::AuthoringMutation,
            eReplayJournalOutcome::AcceptedToolCommand) &&
        ShouldAdvanceToolRevision(
            eReplayCommandDomain::ControlPlane,
            eReplayJournalOutcome::AcceptedToolCommand) &&
        !ShouldAdvanceToolRevision(
            eReplayCommandDomain::ObservationOnly,
            eReplayJournalOutcome::AcceptedToolCommand) &&
        !ShouldAdvanceToolRevision(
            eReplayCommandDomain::AuthoringMutation,
            eReplayJournalOutcome::RejectedToolCommand);

    const bool_t bPausedSnapshotGate =
        !CReplayRecorder::ShouldRecordSnapshot(100u, 7u, 100u, 7u) &&
        CReplayRecorder::ShouldRecordSnapshot(101u, 7u, 100u, 7u) &&
        CReplayRecorder::ShouldRecordSnapshot(100u, 8u, 100u, 7u);

    auto recorder = CReplayRecorder::Create(77u, 30u);
    if (!recorder || recorder->RecordCommand(100u, unknownDomain))
    {
        std::printf("[ReplayContract] FAIL: invalid writer domain accepted\n");
        return 1;
    }

    ReplayCommandPayload explicitCommand{};
    explicitCommand.sourceSessionId = 17u;
    explicitCommand.sequenceNum = 23u;
    explicitCommand.kind = 12u;
    explicitCommand.practiceOperation = 7u;
    explicitCommand.clientTick = 91u;
    SetReplayCommandDomain(
        explicitCommand,
        eReplayCommandDomain::AuthoringMutation);
    if (!recorder->RecordCommand(100u, explicitCommand))
    {
        std::printf("[ReplayContract] FAIL: valid writer domain rejected\n");
        return 1;
    }

    const u8_t snapshotByteA = 0xA1u;
    const u8_t snapshotByteB = 0xB2u;
    recorder->RecordSnapshot(100u, &snapshotByteA, 1u);
    recorder->RecordSnapshot(100u, &snapshotByteB, 1u);

    const std::filesystem::path outputPath =
        std::filesystem::temp_directory_path() /
        "WintersReplayCommandContractProbe.wrpl";
    std::string saveError;
    if (!recorder->SaveToFile(outputPath.wstring(), saveError))
    {
        std::printf(
            "[ReplayContract] FAIL: writer save failed: %s\n",
            saveError.c_str());
        return 1;
    }

    ReplayFileHeader fileHeader{};
    ReplayRecordHeader commandHeader{};
    ReplayCommandPayload persistedCommand{};
    std::ifstream input(outputPath, std::ios::binary);
    const bool_t bPersistedCommandContract =
        ReadExact(input, &fileHeader, sizeof(fileHeader)) &&
        ReadExact(input, &commandHeader, sizeof(commandHeader)) &&
        commandHeader.type == static_cast<u8_t>(eReplayRecordType::Command) &&
        commandHeader.payloadSize == sizeof(ReplayCommandPayload) &&
        ReadExact(input, &persistedCommand, sizeof(persistedCommand)) &&
        fileHeader.version == kReplayVersion &&
        fileHeader.recordCount == 3u &&
        fileHeader.snapshotCount == 2u &&
        recorder->GetCommandCount() == 1u &&
        recorder->GetSnapshotCount() == 2u &&
        GetReplayCommandDomain(persistedCommand) ==
            eReplayCommandDomain::AuthoringMutation;
    input.close();
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);

    auto retryRecorder = CReplayRecorder::Create(78u, 30u);
    retryRecorder->RecordSnapshot(7u, &snapshotByteA, 1u);
    const std::filesystem::path blockedDestination =
        std::filesystem::temp_directory_path() /
        "WintersReplayCommandContractProbeBlockedDestination";
    std::filesystem::create_directories(blockedDestination);
    std::string expectedPublishError;
    const bool_t bInitialPublishRejected =
        !retryRecorder->SaveToFile(
            blockedDestination.wstring(),
            expectedPublishError);
    const std::filesystem::path retryOutputPath =
        std::filesystem::temp_directory_path() /
        "WintersReplayCommandContractProbeRetry.wrpl";
    std::string retryError;
    const bool_t bRetryPublished =
        retryRecorder->SaveToFile(retryOutputPath.wstring(), retryError);
    ReplayFileHeader retryHeader{};
    std::ifstream retryInput(retryOutputPath, std::ios::binary);
    const bool_t bPublishRetryContract =
        bInitialPublishRejected &&
        !expectedPublishError.empty() &&
        bRetryPublished &&
        ReadExact(retryInput, &retryHeader, sizeof(retryHeader)) &&
        retryHeader.recordCount == 1u &&
        retryHeader.snapshotCount == 1u &&
        retryHeader.firstTick == 7u &&
        retryHeader.lastTick == 7u;
    retryInput.close();
    std::filesystem::remove(retryOutputPath, removeError);
    std::filesystem::remove(blockedDestination, removeError);

    const bool_t bPass =
        bV1ReaderCompatible &&
        bLegacyV2ZeroIsPlayerInput &&
        bCommandPayloadValidation &&
        bFaithfulResimContract &&
        bToolRevisionContract &&
        bPausedSnapshotGate &&
        bPersistedCommandContract &&
        bPublishRetryContract;

    std::printf(
        "[ReplayContract] %s: v1 read, v2 legacy-zero/domain, 60-byte ABI, "
        "journal/resim outcomes, tool revision, paused snapshot pair, "
        "sealed publish retry\n",
        bPass ? "PASS" : "FAIL");
    return bPass ? 0 : 1;
}
