#pragma once
#include <cstdint>

// ─────────────────────────────────────────────────────────────────
//  PacketDef.h  |  Client ↔ Server 공용 패킷 정의
//
//  Client와 Server 양쪽에서 include 한다.
//  엔진(DLL)에는 포함되지 않음 - 순수 데이터 정의 파일.
//
//  DX9 엔진의 PacketDef.h를 계승하되 확장:
//    - 프로토콜 버전 관리 추가
//    - Sequence/Ack 번호 (신뢰성 레이어 기반)
//    - 향후 Protobuf 또는 FlatBuffers로 전환 예정
//
//  #pragma pack(push, 1):
//    구조체 패딩 제거 → 네트워크 전송 시 크기가 예측 가능해짐
//    플랫폼 간 크기 불일치를 방지하기 위해 고정 크기 타입 사용
// ─────────────────────────────────────────────────────────────────

#pragma pack(push, 1)

// ── 프로토콜 상수 ──────────────────────────────────────────────
constexpr uint16_t WINTERS_PROTOCOL_VERSION = 1;
constexpr uint32_t WINTERS_MAX_PLAYERS      = 100;
constexpr uint32_t WINTERS_MAX_PACKET_SIZE  = 4096;
constexpr uint32_t WINTERS_SERVER_PORT      = 9000;
constexpr uint32_t WINTERS_SERVER_TPS       = 20;      // Server Tick Per Second

// ── 패킷 타입 ──────────────────────────────────────────────────
enum class PacketType : uint16_t
{
    // ── Client → Server ────────────────────────────────────
    C2S_CONNECT     = 0x0001,   // 접속 요청
    C2S_DISCONNECT  = 0x0002,   // 접속 해제
    C2S_INPUT       = 0x0003,   // 플레이어 입력 (매 틱)
    C2S_PING        = 0x0004,   // 핑 측정

    // ── Server → Client ────────────────────────────────────
    S2C_CONNECT_ACK = 0x1001,   // 접속 승인 (PlayerID 부여)
    S2C_SPAWN       = 0x1002,   // 다른 플레이어 스폰
    S2C_DESPAWN     = 0x1003,   // 다른 플레이어 퇴장
    S2C_SNAPSHOT    = 0x1004,   // 게임 상태 스냅샷 (매 틱)
    S2C_PONG        = 0x1005,   // 핑 응답
};

// ── 공통 패킷 헤더 ─────────────────────────────────────────────
// 모든 패킷 앞에 붙는 공통 헤더
struct PacketHeader
{
    uint16_t    protocolVersion = WINTERS_PROTOCOL_VERSION;
    PacketType  type            = PacketType::C2S_PING;
    uint16_t    size            = 0;       // 헤더 포함 전체 패킷 크기 (바이트)
    uint32_t    sequence        = 0;       // 송신 측 패킷 번호 (신뢰성)
    uint32_t    ack             = 0;       // 수신 확인 번호
};

// ── C2S 패킷 ───────────────────────────────────────────────────

struct C2S_ConnectPacket
{
    PacketHeader    header;
    char            nickname[32]    = {};
    uint32_t        clientVersion   = 0;
};

// 플레이어 입력 (Client → Server, 매 틱 전송)
// 서버 권위 시뮬레이션: 서버가 이 입력으로 게임 상태를 결정
struct C2S_InputPacket
{
    PacketHeader    header;
    uint32_t        clientTick  = 0;       // 클라이언트 예측 틱 (보정에 사용)
    float           moveX       = 0.f;     // 이동 방향 X  (-1 ~ 1)
    float           moveY       = 0.f;     // 이동 방향 Y  (-1 ~ 1)
    uint8_t         buttons     = 0;       // 비트 플래그: 점프/공격/스킬
    float           lookYaw     = 0.f;     // 카메라 수평 회전
    float           lookPitch   = 0.f;     // 카메라 수직 회전
};

// ── S2C 패킷 ───────────────────────────────────────────────────

struct S2C_ConnectAckPacket
{
    PacketHeader    header;
    uint32_t        playerID    = 0;       // 서버가 부여한 플레이어 ID
    uint32_t        serverTick  = 0;       // 현재 서버 틱 (시간 동기화)
};

// 단일 플레이어 상태 (Snapshot 내부에서 사용)
struct PlayerState
{
    uint32_t    playerID    = 0;
    float       posX        = 0.f;
    float       posY        = 0.f;
    float       posZ        = 0.f;
    float       rotY        = 0.f;
    uint8_t     animState   = 0;       // 애니메이션 상태 인덱스
    uint16_t    hp          = 0;
    uint8_t     flags       = 0;       // 비트 플래그: 사망/무적/스턴 등
};

// 매 서버 틱마다 전체 클라이언트에 브로드캐스트
// 향후: Interest Management → 각 클라이언트에 관련 있는 플레이어만 전송
struct S2C_SnapshotPacket
{
    PacketHeader    header;
    uint32_t        serverTick  = 0;
    uint8_t         playerCount = 0;
    PlayerState     players[WINTERS_MAX_PLAYERS] = {};
};

#pragma pack(pop)
