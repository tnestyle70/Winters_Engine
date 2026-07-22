#pragma once

#include "WintersTypes.h"

#include <memory>
#include <string>
#include <vector>

struct ReplayUploadParticipant
{
    std::string userID;
    std::string result;
    u32_t perspectiveNetId = 0u;
    i32_t kills = 0;
    i32_t deaths = 0;
    i32_t assists = 0;
};

struct ReplayUploadArtifact
{
    wstring_t path;
    std::string matchID;
    u64_t sizeBytes = 0;
    u32_t formatVersion = 0;
    u32_t tickRate = 0;
    u32_t recordCount = 0;
    u32_t snapshotCount = 0;
    u32_t eventCount = 0;
    u32_t commandCount = 0;
    u64_t firstTick = 0;
    u64_t lastTick = 0;
    std::vector<ReplayUploadParticipant> participants;
};

class CReplayUploadQueue final
{
public:
    static CReplayUploadQueue& Instance();

    bool_t StartFromEnvironment();
    bool_t Enqueue(ReplayUploadArtifact artifact);
    bool_t StageGameSessionReady(
        const std::string& gameSessionID,
        const std::string& matchID);
    bool_t PublishGameSessionReady(
        const std::string& gameSessionID,
        const std::string& matchID);
    void Shutdown(bool_t bDrain);
    bool_t IsEnabled() const;
    u64_t GetGameSessionGenerationFloor() const;

private:
    CReplayUploadQueue();
    ~CReplayUploadQueue();

    CReplayUploadQueue(const CReplayUploadQueue&) = delete;
    CReplayUploadQueue& operator=(const CReplayUploadQueue&) = delete;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
