# Winters Engine — 1만명 동시접속 부하 테스트 계획서

> **대상**: C++ IOCP 게임 서버 + Go 백엔드 마이크로서비스
> **목표**: 10,000 CCU (Concurrent Users) 환경에서 서버 안정성·응답 지연·패킷 무결성 검증
> **도구**: Wireshark, tshark, 자체 봇 클라이언트, Grafana/Prometheus, k6
> **수행자**: 개발자 본인 (수동 실행)
> **작성일**: 2026-04-09

---

## 목차

1. [테스트 전제 조건 (Ready Criteria)](#1-테스트-전제-조건)
2. [테스트 환경 구성](#2-테스트-환경-구성)
3. [Wireshark / tshark 설정 가이드](#3-wireshark--tshark-설정-가이드)
4. [봇 클라이언트 (부하 생성기)](#4-봇-클라이언트-부하-생성기)
5. [테스트 시나리오 (5단계)](#5-테스트-시나리오-5단계)
6. [패킷 분석 체크리스트](#6-패킷-분석-체크리스트)
7. [백엔드 (Go) 부하 테스트](#7-백엔드-go-부하-테스트)
8. [모니터링 & 메트릭 수집](#8-모니터링--메트릭-수집)
9. [성능 기준 (Pass/Fail)](#9-성능-기준-passfail)
10. [결과 보고서 템플릿](#10-결과-보고서-템플릿)
11. [트러블슈팅 가이드](#11-트러블슈팅-가이드)

---

## 1. 테스트 전제 조건

서버/백엔드가 아래 기능을 구현 완료한 시점에서 부하 테스트를 시작한다.

### 1-1. 게임 서버 (C++ IOCP) Ready Criteria

| # | 기능 | 검증 방법 |
|---|------|-----------|
| 1 | **UDP 소켓 바인드** (ws2_32) | `netstat -an` 으로 포트 리스닝 확인 |
| 2 | **IOCP 기반 비동기 I/O** | 멀티 클라이언트 접속 테스트 (최소 10개) |
| 3 | **CSessionMgr** 세션 관리 | 접속/해제 시 세션 정상 생성/파괴 확인 |
| 4 | **CGameLoop** 서버 틱 루프 | 20~60 TPS 안정적 동작 확인 |
| 5 | **PacketDef.h** 패킷 프로토콜 | FlatBuffers 직렬화/역직렬화 정상 동작 |
| 6 | **BroadcastSnapshot** | 2명 이상 클라이언트에 게임 상태 전송 확인 |
| 7 | **AOI 그리드** | 50m×50m 그리드 분할, 셀 기반 브로드캐스트 |

### 1-2. 백엔드 (Go 마이크로서비스) Ready Criteria

| # | 서비스 | 검증 방법 |
|---|--------|-----------|
| 1 | **Auth** (JWT 발급/검증) | `curl POST /auth/login` → 토큰 반환 |
| 2 | **Matchmaking** (MMR 기반 매칭) | 매칭 요청 → 게임 서버 할당 응답 |
| 3 | **Profile** (전적/통계) | `GET /profile/{userId}` → JSON 응답 |
| 4 | **PostgreSQL** 연결 | `docker exec winters-postgres pg_isready` |
| 5 | **Redis** 연결 | `docker exec winters-redis redis-cli ping` |
| 6 | **Kafka** 이벤트 | 토픽 publish/consume 확인 |

### 1-3. 인프라 Ready Criteria

| # | 항목 | 상세 |
|---|------|------|
| 1 | **docker-compose up** | postgres, redis, kafka 전부 healthy |
| 2 | **게임 서버 바이너리** | `WintersServer.exe` 빌드 성공 |
| 3 | **네트워크 방화벽** | 게임 서버 UDP 포트 + 백엔드 HTTP 포트 개방 |
| 4 | **Wireshark 설치** | v4.x + USBPcap/Npcap 드라이버 설치 |

---

## 2. 테스트 환경 구성

### 2-1. 물리 장비 구성

```
┌─────────────────────────────────────────────────────────┐
│                    테스트 네트워크                         │
│                                                         │
│  [봇 머신 1]──┐                                         │
│  (2,500 봇)   │     ┌──────────────┐                    │
│  [봇 머신 2]──┼────→│  스위치/허브   │                    │
│  (2,500 봇)   │     │  (미러 포트)  │                    │
│  [봇 머신 3]──┤     └──────┬───────┘                    │
│  (2,500 봇)   │            │                            │
│  [봇 머신 4]──┘            │                            │
│  (2,500 봇)          ┌─────▼─────┐   ┌──────────────┐  │
│                      │ 게임 서버  │   │ Wireshark PC │  │
│                      │ (C++ IOCP)│   │ (미러 캡처)   │  │
│                      └─────┬─────┘   └──────────────┘  │
│                            │                            │
│                      ┌─────▼─────┐                      │
│                      │ 백엔드     │                      │
│                      │ (Go+Docker)│                      │
│                      └───────────┘                      │
└─────────────────────────────────────────────────────────┘
```

**장비별 최소 사양:**

| 역할 | CPU | RAM | 네트워크 | 비고 |
|------|-----|-----|---------|------|
| **게임 서버** | 8코어+ | 16GB+ | 1Gbps | IOCP 스레드 = 코어수 × 2 |
| **봇 머신** (×4) | 4코어+ | 8GB+ | 1Gbps | 머신당 2,500 봇 |
| **Wireshark PC** | 4코어+ | 16GB+ | 1Gbps | 패킷 캡처 전용 |
| **백엔드 서버** | 4코어+ | 16GB+ | 1Gbps | Docker (PG+Redis+Kafka+Go) |

> **단일 PC 테스트**: 장비가 부족하면 한 PC에서 게임 서버 + 봇 클라이언트를 동시 실행 가능.
> 단, 1만 봇을 한 PC에서 돌리려면 **최소 32GB RAM, 16코어** 필요.
> 이 경우 Wireshark는 `localhost` (Npcap loopback adapter) 캡처.

### 2-2. 네트워크 구성

```
게임 서버:
  - UDP 바인드: 0.0.0.0:7777 (게임 패킷)
  - TCP 바인드: 0.0.0.0:7778 (초기 핸드셰이크/인증)

백엔드:
  - Auth:         http://localhost:8081
  - Matchmaking:  http://localhost:8082
  - Profile:      http://localhost:8083
  - PostgreSQL:   localhost:5432
  - Redis:        localhost:6379
  - Kafka:        localhost:9092
```

### 2-3. 스위치 미러 포트 설정 (물리 분리 환경)

Wireshark PC를 스위치의 **미러 포트(SPAN 포트)**에 연결하여 게임 서버 트래픽을 복제 캡처한다.

```
관리형 스위치 CLI 예시 (Cisco):
  monitor session 1 source interface Gi0/1    ← 게임 서버 포트
  monitor session 1 destination interface Gi0/8  ← Wireshark PC 포트

비관리형 스위치: 허브(Hub) 사용 또는 TAP 장비 사용
```

---

## 3. Wireshark / tshark 설정 가이드

### 3-1. Wireshark 캡처 필터 (Capture Filter)

캡처 시 불필요한 트래픽을 제외하여 디스크/메모리 절약.

```
# 게임 서버 UDP 패킷만 캡처 (포트 7777)
udp port 7777

# 게임 서버 UDP + TCP 핸드셰이크 (포트 7777 + 7778)
port 7777 or port 7778

# 특정 서버 IP만 캡처
host 192.168.1.100 and (port 7777 or port 7778)

# 백엔드 HTTP도 함께 캡처
host 192.168.1.100 and (port 7777 or port 7778 or port 8081 or port 8082)
```

### 3-2. Wireshark 디스플레이 필터 (Display Filter)

캡처 후 분석 시 사용.

```
# 게임 패킷만 보기
udp.port == 7777

# 특정 클라이언트 IP의 패킷
ip.src == 192.168.1.50 and udp.port == 7777

# 패킷 크기 이상 탐지 (MTU 초과)
udp.port == 7777 and frame.len > 1500

# TCP 재전송 탐지 (핸드셰이크 문제)
tcp.port == 7778 and tcp.analysis.retransmission

# 패킷 간격 이상 (100ms 이상 지연)
udp.port == 7777 and frame.time_delta > 0.1

# 특정 패킷 타입 (FlatBuffers 헤더 기준)
# PacketDef.h에서 정의한 패킷 ID가 페이로드 첫 2바이트라고 가정
udp.port == 7777 and data[0:2] == 00:01   # 예: 이동 패킷 (ID=0x0001)
udp.port == 7777 and data[0:2] == 00:02   # 예: 스킬 패킷 (ID=0x0002)
udp.port == 7777 and data[0:2] == 00:ff   # 예: 스냅샷 패킷 (ID=0x00FF)
```

### 3-3. Wireshark 커스텀 Dissector (Lua 플러그인)

Winters 패킷 프로토콜을 Wireshark에서 자동 파싱하려면 Lua dissector를 작성한다.

**파일**: `%APPDATA%\Wireshark\plugins\winters_dissector.lua`

```lua
-- Winters Game Protocol Dissector
-- PacketDef.h의 패킷 구조에 맞춰 수정 필요

local winters_proto = Proto("winters", "Winters Game Protocol")

-- 필드 정의
local f_packet_id   = ProtoField.uint16("winters.packet_id", "Packet ID", base.HEX)
local f_sequence    = ProtoField.uint32("winters.sequence", "Sequence Number", base.DEC)
local f_timestamp   = ProtoField.uint32("winters.timestamp", "Server Tick", base.DEC)
local f_payload_len = ProtoField.uint16("winters.payload_len", "Payload Length", base.DEC)
local f_payload     = ProtoField.bytes("winters.payload", "Payload (FlatBuffers)")

winters_proto.fields = { f_packet_id, f_sequence, f_timestamp, f_payload_len, f_payload }

-- 패킷 ID → 이름 매핑 (PacketDef.h 기반으로 업데이트)
local packet_names = {
    [0x0001] = "C2S_Move",
    [0x0002] = "C2S_Skill",
    [0x0003] = "C2S_Input",
    [0x0010] = "S2C_Snapshot",
    [0x0011] = "S2C_SpawnEntity",
    [0x0012] = "S2C_DestroyEntity",
    [0x0020] = "S2C_SkillResult",
    [0x00F0] = "C2S_Heartbeat",
    [0x00F1] = "S2C_Heartbeat_Ack",
    [0x00FF] = "S2C_Disconnect",
}

function winters_proto.dissector(buffer, pinfo, tree)
    if buffer:len() < 10 then return end  -- 최소 헤더 크기

    pinfo.cols.protocol = "WINTERS"

    local subtree = tree:add(winters_proto, buffer(), "Winters Game Protocol")

    local packet_id = buffer(0, 2):uint()
    local name = packet_names[packet_id] or string.format("Unknown(0x%04X)", packet_id)

    subtree:add(f_packet_id, buffer(0, 2)):append_text(" [" .. name .. "]")
    subtree:add(f_sequence,  buffer(2, 4))
    subtree:add(f_timestamp, buffer(6, 4))

    local payload_len = buffer(10, 2):uint()
    subtree:add(f_payload_len, buffer(10, 2))

    if buffer:len() > 12 then
        subtree:add(f_payload, buffer(12, payload_len))
    end

    pinfo.cols.info = name .. " seq=" .. buffer(2, 4):uint()
end

-- UDP 포트 7777에 dissector 바인딩
local udp_table = DissectorTable.get("udp.port")
udp_table:add(7777, winters_proto)
```

### 3-4. tshark CLI 캡처 (대용량 장시간 캡처용)

Wireshark GUI는 1만 CCU 트래픽을 실시간 표시하면 메모리 폭발. **tshark CLI**로 파일 캡처 후 오프라인 분석.

```bash
# 기본 캡처 → pcap 파일로 저장 (100MB마다 파일 분할)
tshark -i "이더넷" -f "udp port 7777" \
  -b filesize:102400 \
  -b files:50 \
  -w C:\LoadTest\capture_10k.pcapng

# 로컬호스트 캡처 (단일 PC 테스트 시)
tshark -i "Npcap Loopback Adapter" -f "udp port 7777" \
  -b filesize:102400 \
  -w C:\LoadTest\capture_local.pcapng

# 실시간 통계 출력 (초당 패킷 수)
tshark -i "이더넷" -f "udp port 7777" \
  -q -z io,stat,1,"COUNT(frame)frame","SUM(frame.len)frame"

# 특정 시간대 패킷만 추출 (후처리)
tshark -r capture_10k.pcapng \
  -Y "frame.time >= \"2026-04-09 14:00:00\" and frame.time <= \"2026-04-09 14:05:00\"" \
  -w filtered_5min.pcapng
```

### 3-5. Wireshark I/O 그래프 설정

캡처 완료 후 `Statistics → I/O Graphs`에서 아래 그래프를 생성:

| 그래프 | Y축 | 필터 | 목적 |
|--------|-----|------|------|
| 초당 패킷 수 (PPS) | Packets/s | `udp.port==7777` | 전체 처리량 확인 |
| 초당 바이트 (BPS) | Bytes/s | `udp.port==7777` | 대역폭 사용량 |
| C→S 이동 패킷 | Packets/s | `udp.port==7777 and data[0:2]==00:01` | 입력 처리량 |
| S→C 스냅샷 | Packets/s | `udp.port==7777 and data[0:2]==00:10` | 브로드캐스트 빈도 |
| 패킷 크기 분포 | AVG(frame.len) | `udp.port==7777` | 평균 패킷 크기 추이 |

---

## 4. 봇 클라이언트 (부하 생성기)

### 4-1. 봇 클라이언트 아키텍처

실제 게임 클라이언트 대신 **경량 봇**으로 1만 CCU를 시뮬레이션한다.

```
┌─────────────────────────────────────────────────────┐
│                 BotManager.exe                      │
│                                                     │
│  ┌─────────┐  ┌─────────┐       ┌─────────┐       │
│  │ Bot #1  │  │ Bot #2  │  ...  │Bot #2500│       │
│  │ (UDP    │  │ (UDP    │       │ (UDP    │       │
│  │  소켓)  │  │  소켓)  │       │  소켓)  │       │
│  └────┬────┘  └────┬────┘       └────┬────┘       │
│       │            │                 │             │
│       └────────────┼─────────────────┘             │
│                    │                               │
│            ┌───────▼───────┐                       │
│            │  IO 스레드 풀  │                       │
│            │  (IOCP 기반)  │                       │
│            └───────────────┘                       │
└─────────────────────────────────────────────────────┘
```

### 4-2. 봇 클라이언트 구현 요구사항

```cpp
// BotClient 핵심 구조 (개념 코드)
struct BotConfig
{
    uint32_t    botCount        = 2500;       // 머신당 봇 수
    const char* serverIP        = "192.168.1.100";
    uint16_t    serverPort      = 7777;
    uint32_t    sendIntervalMs  = 50;         // 20Hz 전송 (50ms 간격)
    bool        enableMovement  = true;       // 이동 시뮬레이션
    bool        enableSkills    = false;      // 스킬 사용 시뮬레이션
    bool        enableChat      = false;      // 채팅 시뮬레이션
};

// 각 봇의 행동 시뮬레이션
class CBot
{
    enum class State { Connecting, InLobby, InGame, Disconnecting };

    State       m_state;
    SOCKET      m_socket;       // UDP 소켓
    uint32_t    m_sessionId;
    Vec3        m_position;     // 가짜 위치
    uint32_t    m_sequenceNum;

    void Tick()
    {
        switch (m_state)
        {
        case State::InGame:
            // 랜덤 이동 입력 생성 (WASD 시뮬레이션)
            SendMovePacket(RandomDirection());
            // 30초마다 스킬 사용
            if (ShouldUseSkill())
                SendSkillPacket(RandomSkillSlot());
            break;
        // ... 다른 상태 처리
        }
    }
};
```

### 4-3. 봇 행동 프로파일

| 프로파일 | 봇 수 | 행동 | 패킷 전송률 |
|---------|-------|------|------------|
| **Idle** | 2,000 | 접속만 유지, 하트비트만 전송 | 1 pkt/s |
| **Walker** | 5,000 | 랜덤 방향 이동만 | 20 pkt/s |
| **Fighter** | 2,000 | 이동 + 스킬 사용 | 25 pkt/s |
| **Chatter** | 1,000 | 이동 + 채팅 스팸 | 22 pkt/s |
| **합계** | **10,000** | | |

**예상 초당 전체 패킷량:**
```
  Idle:    2,000 × 1   =   2,000 pkt/s (C→S)
  Walker:  5,000 × 20  = 100,000 pkt/s (C→S)
  Fighter: 2,000 × 25  =  50,000 pkt/s (C→S)
  Chatter: 1,000 × 22  =  22,000 pkt/s (C→S)
  ─────────────────────────────────────────
  총 C→S:               ≈ 174,000 pkt/s

  S→C 스냅샷 (20Hz, AOI 적용):
  10,000 × 20 × (주변 15명 기준) / 10,000 = 20 pkt/s/client
  총 S→C: 10,000 × 20 = 200,000 pkt/s

  양방향 합계: ≈ 374,000 pkt/s
```

---

## 5. 테스트 시나리오 (5단계)

점진적으로 부하를 증가시켜 병목 지점을 파악한다.

### Stage 1: 스모크 테스트 (100 CCU)

**목표**: 기본 기능 정상 동작 확인

| 항목 | 상세 |
|------|------|
| **봇 수** | 100명 (단일 머신) |
| **시간** | 10분 |
| **행동** | 접속 → 인증 → 매칭 → 게임 진입 → 이동 → 퇴장 |
| **Wireshark** | GUI 실시간 캡처 가능 |
| **확인 사항** | 접속/해제 정상, 패킷 송수신 정상, 세션 누수 없음 |

```bash
# Wireshark 캡처 시작
tshark -i "이더넷" -f "udp port 7777" -w C:\LoadTest\stage1_100ccu.pcapng

# 봇 실행
BotManager.exe --count=100 --server=192.168.1.100:7777 --duration=600
```

**Wireshark 확인 포인트:**
1. `Statistics → Conversations → UDP` : 100개의 고유 소스 포트 확인
2. `Statistics → I/O Graphs` : PPS가 안정적인지 확인
3. 패킷 손실 여부: 시퀀스 번호 연속성 체크

### Stage 2: 기능 검증 (1,000 CCU)

**목표**: 핵심 게임플레이 시나리오 검증

| 항목 | 상세 |
|------|------|
| **봇 수** | 1,000명 (단일 머신) |
| **시간** | 30분 |
| **행동** | Walker 700 + Fighter 200 + Idle 100 |
| **Wireshark** | tshark CLI 캡처 |
| **확인 사항** | AOI 필터링 정상 동작, 스냅샷 크기 합리적 |

```bash
# tshark 캡처 + 실시간 통계
tshark -i "이더넷" -f "udp port 7777" \
  -b filesize:102400 -w C:\LoadTest\stage2_1000ccu.pcapng &

tshark -i "이더넷" -f "udp port 7777" \
  -q -z io,stat,5,"COUNT(frame)frame"
```

**Wireshark 분석 (캡처 후):**
1. AOI 검증: 특정 봇 IP 필터링 → 수신하는 S2C_Snapshot에 포함된 엔티티 수가 ~15개 이하인지
2. `Statistics → Packet Lengths` : 패킷 크기 분포 확인
3. `Statistics → Flow Graph` : 연결 흐름 정상 확인

### Stage 3: 부하 테스트 (5,000 CCU)

**목표**: 서버 처리량 한계 접근

| 항목 | 상세 |
|------|------|
| **봇 수** | 5,000명 (2대 머신 × 2,500) |
| **시간** | 1시간 |
| **행동** | 전체 프로파일 비율 적용 (50% 규모) |
| **Wireshark** | tshark 파일 분할 캡처 |
| **확인 사항** | 서버 CPU/메모리 추이, 패킷 지연 증가 여부 |

```bash
# 게임 서버 머신에서 tshark (서버 측 캡처)
tshark -i "이더넷" -f "udp port 7777" \
  -b filesize:102400 -b files:100 \
  -w C:\LoadTest\stage3_server_5000ccu.pcapng

# 봇 머신 1에서 tshark (클라이언트 측 캡처)
tshark -i "이더넷" -f "udp port 7777" \
  -b filesize:102400 -b files:20 \
  -w C:\LoadTest\stage3_client1_5000ccu.pcapng
```

**핵심 분석:**
1. **서버 틱 지연**: S2C_Snapshot 간격이 50ms (20Hz) 근처 유지되는지
   ```
   tshark -r stage3_server_5000ccu.pcapng \
     -Y "data[0:2]==00:10" \
     -T fields -e frame.time_delta_displayed | \
     awk '{sum+=$1; count++} END {print "avg:", sum/count, "s"}'
   ```
2. **패킷 손실률**: 시퀀스 번호 갭 분석
3. **대역폭**: `Statistics → I/O Graphs` 에서 BPS 확인

### Stage 4: 풀 부하 테스트 (10,000 CCU)

**목표**: 최종 목표 달성 여부 확인

| 항목 | 상세 |
|------|------|
| **봇 수** | 10,000명 (4대 머신 × 2,500) |
| **시간** | 2시간 |
| **행동** | 전체 프로파일 비율 적용 |
| **Wireshark** | 서버 측 tshark + 봇 머신 1대에서 샘플링 캡처 |
| **확인 사항** | 10K CCU 안정 유지, 메모리 누수 없음, 패킷 지연 ≤100ms |

```bash
# 서버 측 캡처 (장시간이므로 파일 수 제한)
tshark -i "이더넷" -f "udp port 7777" \
  -b filesize:204800 -b files:200 \
  -w C:\LoadTest\stage4_server_10000ccu.pcapng

# 5분 간격 스냅샷 통계
while true; do
  echo "=== $(date) ==="
  tshark -i "이더넷" -f "udp port 7777" -a duration:10 \
    -q -z io,stat,1,"COUNT(frame)frame"
  sleep 290
done
```

**Wireshark 심층 분석:**

1. **RTT 측정** (Heartbeat 기반):
   ```
   # C2S_Heartbeat(0x00F0) 전송 → S2C_Heartbeat_Ack(0x00F1) 수신 간격
   tshark -r stage4_server_10000ccu.pcapng \
     -Y "data[0:2]==00:f1" \
     -T fields -e frame.time_relative -e ip.dst | head -100
   ```

2. **AOI 효율성 검증**:
   ```
   # 특정 클라이언트가 수신한 스냅샷 패킷 크기 분포
   tshark -r stage4_server_10000ccu.pcapng \
     -Y "ip.dst==192.168.1.50 and data[0:2]==00:10" \
     -T fields -e frame.len | sort -n | uniq -c
   ```

3. **피크 트래픽 구간 식별**:
   ```
   # 1초 단위 PPS 추출 → CSV → 그래프화
   tshark -r stage4_server_10000ccu.pcapng \
     -q -z io,stat,1,"COUNT(frame)frame" > pps_stats.csv
   ```

### Stage 5: 스트레스 테스트 (10,000+ / 장애 주입)

**목표**: 서버 한계 초과 시 graceful degradation 확인

| 시나리오 | 방법 | 확인 사항 |
|---------|------|-----------|
| **과부하** | 12,000~15,000 봇 접속 | 서버가 크래시하지 않고 신규 접속 거부 |
| **네트워크 지연** | `tc` 또는 `clumsy`로 100ms 지연 추가 | 클라이언트 예측/보정 정상 동작 |
| **패킷 손실** | `clumsy`로 5~10% 패킷 드롭 | 재전송/복구 메커니즘 동작 |
| **서버 킬** | 게임 서버 프로세스 강제 종료 | 클라이언트 타임아웃 처리, 자동 재접속 |
| **봇 대량 동시 접속** | 1,000명/초 속도로 접속 시도 | 접속 큐 / rate limiting 동작 |

```bash
# clumsy를 사용한 네트워크 장애 주입 (Windows)
# 다운로드: https://jagt.github.io/clumsy/
# 100ms 지연 + 5% 패킷 드롭
clumsy.exe --filter "udp and udp.DstPort == 7777" --lag on --lag-time 100 --drop on --drop-chance 5

# Wireshark에서 장애 주입 전후 비교
tshark -r stage5_stress.pcapng \
  -Y "udp.port==7777" \
  -q -z io,stat,1,"COUNT(frame)frame","AVG(frame.time_delta)frame.time_delta"
```

---

## 6. 패킷 분석 체크리스트

각 테스트 단계에서 Wireshark 캡처 파일을 분석할 때 확인할 항목.

### 6-1. 패킷 무결성

| # | 확인 항목 | Wireshark 방법 | Pass 기준 |
|---|----------|---------------|-----------|
| 1 | **UDP 체크섬 오류** | `udp.checksum.status == "Bad"` | 0건 |
| 2 | **잘린 패킷** | `_ws.short` | 0건 |
| 3 | **MTU 초과** | `frame.len > 1500` | 0건 (또는 프래그먼트 정상) |
| 4 | **알 수 없는 패킷 ID** | Dissector에서 "Unknown" 표시 | 0건 |
| 5 | **시퀀스 번호 갭** | 커스텀 스크립트로 연속성 검증 | 0.1% 미만 |

### 6-2. 성능 지표

| # | 지표 | 측정 방법 | 목표 |
|---|------|----------|------|
| 1 | **초당 패킷 수 (PPS)** | `io,stat` | Stage4 기준 ≥ 350K pps 처리 |
| 2 | **평균 패킷 크기** | `Statistics → Packet Lengths` | 이동: ~50B, 스냅샷: ~200-500B |
| 3 | **서버 응답 지연** | Heartbeat RTT | p50 ≤ 20ms, p99 ≤ 100ms |
| 4 | **스냅샷 간격** | S2C_Snapshot frame.time_delta | 50ms ± 10ms (20Hz) |
| 5 | **대역폭** | `io,stat` (bytes) | 서버 ≤ 500Mbps (1Gbps의 50%) |

### 6-3. 프로토콜 이상 탐지

```
# 비정상 패턴 탐지 필터 모음

# 1. 동일 클라이언트가 비정상적으로 많은 패킷 전송 (초당 100개 이상)
# → Wireshark: Statistics → Endpoints → UDP → Sort by Packets
# → 상위 클라이언트의 PPS가 예상(20~25)보다 크면 이상

# 2. 서버가 응답하지 않는 클라이언트 (Heartbeat ACK 없음)
udp.port == 7777 and data[0:2] == 00:f0 and !data[0:2] == 00:f1

# 3. 패킷 크기 이상 (FlatBuffers 직렬화 오류 가능)
udp.port == 7777 and data[0:2] == 00:01 and frame.len > 200  # 이동 패킷 200B 초과

# 4. 중복 시퀀스 번호 (재전송 감지)
# → tshark로 시퀀스 번호 추출 후 정렬/중복 검사
tshark -r capture.pcapng -Y "udp.port==7777" \
  -T fields -e ip.src -e data[2:4] | sort | uniq -d
```

---

## 7. 백엔드 (Go) 부하 테스트

### 7-1. k6 부하 테스트 스크립트

게임 서버와 별개로 Go 백엔드 HTTP API를 k6로 부하 테스트한다.

**설치:**
```bash
# Windows
choco install k6
# 또는 https://k6.io/docs/get-started/installation/
```

**k6 스크립트**: `tests/load/k6_backend_10k.js`

```javascript
import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate, Trend } from 'k6/metrics';

// 커스텀 메트릭
const loginLatency = new Trend('login_latency');
const matchLatency = new Trend('match_latency');
const errorRate = new Rate('errors');

export const options = {
  stages: [
    { duration: '2m',  target: 1000 },   // 2분간 1,000 VU로 램프업
    { duration: '5m',  target: 5000 },   // 5분간 5,000 VU로 증가
    { duration: '10m', target: 10000 },  // 10분간 10,000 VU 유지
    { duration: '5m',  target: 10000 },  // 5분간 10,000 VU 유지 (안정화)
    { duration: '3m',  target: 0 },      // 3분간 서서히 감소
  ],
  thresholds: {
    'login_latency':  ['p(95)<500', 'p(99)<1000'],   // 로그인 p95 < 500ms
    'match_latency':  ['p(95)<1000', 'p(99)<2000'],   // 매칭 p95 < 1s
    'errors':         ['rate<0.01'],                   // 에러율 < 1%
    'http_req_duration': ['p(95)<800'],                // 전체 p95 < 800ms
  },
};

const BASE_URL = 'http://localhost:8081';

export default function () {
  // 1. 로그인 (Auth 서비스)
  const loginRes = http.post(`${BASE_URL}/auth/login`, JSON.stringify({
    username: `bot_${__VU}_${__ITER}`,
    password: 'test_password',
  }), { headers: { 'Content-Type': 'application/json' } });

  check(loginRes, {
    'login status 200': (r) => r.status === 200,
    'has token': (r) => r.json('token') !== undefined,
  }) || errorRate.add(1);

  loginLatency.add(loginRes.timings.duration);
  const token = loginRes.json('token');

  sleep(1);

  // 2. 매칭 요청 (Matchmaking 서비스)
  const matchRes = http.post('http://localhost:8082/match/queue', JSON.stringify({
    mode: 'moba_5v5',
    mmr: Math.floor(Math.random() * 2000) + 500,
  }), {
    headers: {
      'Content-Type': 'application/json',
      'Authorization': `Bearer ${token}`,
    },
  });

  check(matchRes, {
    'match status 200': (r) => r.status === 200,
  }) || errorRate.add(1);

  matchLatency.add(matchRes.timings.duration);

  sleep(2);

  // 3. 프로필 조회
  const profileRes = http.get(`http://localhost:8083/profile/me`, {
    headers: { 'Authorization': `Bearer ${token}` },
  });

  check(profileRes, {
    'profile status 200': (r) => r.status === 200,
  }) || errorRate.add(1);

  sleep(1);
}
```

**실행:**
```bash
# k6 실행 + 결과 JSON 출력
k6 run --out json=C:\LoadTest\k6_result.json tests/load/k6_backend_10k.js

# Wireshark와 동시에 백엔드 트래픽 캡처
tshark -i "이더넷" -f "tcp port 8081 or tcp port 8082 or tcp port 8083" \
  -w C:\LoadTest\backend_traffic.pcapng
```

### 7-2. 백엔드 Wireshark 분석 포인트

```
# HTTP 응답 시간 분석
tshark -r backend_traffic.pcapng \
  -Y "http.response" \
  -T fields -e http.request.uri -e http.response.code -e http.time

# TCP 재전송 (서버 과부하 징후)
tcp.analysis.retransmission and (tcp.port == 8081 or tcp.port == 8082)

# TCP RST (연결 거부 = 서버 과부하)
tcp.flags.reset == 1 and (tcp.port == 8081 or tcp.port == 8082)

# Keep-Alive 연결 수 확인
Statistics → Conversations → TCP → 포트 8081 기준 정렬
```

### 7-3. Kafka 메시지 지연 확인

```bash
# Kafka consumer lag 확인
docker exec winters-kafka kafka-consumer-groups.sh \
  --bootstrap-server localhost:9092 \
  --describe --group winters-backend

# 토픽별 메시지 수 확인
docker exec winters-kafka kafka-topics.sh \
  --bootstrap-server localhost:9092 \
  --describe --topic match-events
```

---

## 8. 모니터링 & 메트릭 수집

### 8-1. 서버 시스템 메트릭

테스트 중 서버 리소스를 모니터링한다.

```bash
# Windows: Performance Monitor 또는 PowerShell
# 5초 간격 CPU/메모리/네트워크 로깅

# PowerShell 스크립트 (서버에서 실행)
while ($true) {
    $cpu = (Get-Counter '\Processor(_Total)\% Processor Time').CounterSamples.CookedValue
    $mem = (Get-Process WintersServer).WorkingSet64 / 1MB
    $net = (Get-Counter '\Network Interface(*)\Bytes Total/sec').CounterSamples.CookedValue | Measure-Object -Sum
    $ts = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    "$ts, CPU: $([math]::Round($cpu,1))%, MEM: $([math]::Round($mem,1))MB, NET: $([math]::Round($net.Sum/1MB,2))MB/s" |
      Tee-Object -FilePath C:\LoadTest\server_metrics.csv -Append
    Start-Sleep 5
}
```

### 8-2. 게임 서버 내부 메트릭 (구현 필요)

서버 코드에 아래 카운터를 내장해야 한다:

| 메트릭 | 설명 | 수집 주기 |
|--------|------|-----------|
| `server.tick_ms` | 틱당 처리 시간 (ms) | 매 틱 |
| `server.active_sessions` | 현재 접속 세션 수 | 매 틱 |
| `server.packets_recv_per_sec` | 초당 수신 패킷 | 1초 |
| `server.packets_send_per_sec` | 초당 송신 패킷 | 1초 |
| `server.memory_used_mb` | 서버 메모리 사용량 | 5초 |
| `server.aoi_entities_avg` | AOI 셀당 평균 엔티티 수 | 매 틱 |
| `server.packet_queue_depth` | Lock-Free Queue 깊이 | 매 틱 |

**출력 형식** (CSV 또는 stdout):
```
[2026-04-09 14:30:00.050] TICK=12345 dt=48ms sessions=10012 recv=175230/s send=201000/s mem=1024MB aoi_avg=12.3 queue=342
```

### 8-3. Grafana 대시보드 (선택사항)

Prometheus + Grafana를 docker-compose에 추가하여 실시간 대시보드를 구성할 수 있다.

```yaml
# docker-compose.yml에 추가
  prometheus:
    image: prom/prometheus:latest
    ports:
      - "9090:9090"
    volumes:
      - ./monitoring/prometheus.yml:/etc/prometheus/prometheus.yml

  grafana:
    image: grafana/grafana:latest
    ports:
      - "3000:3000"
    environment:
      GF_SECURITY_ADMIN_PASSWORD: admin
```

---

## 9. 성능 기준 (Pass/Fail)

### 9-1. 게임 서버 (C++ IOCP)

| 지표 | Pass | Warning | Fail |
|------|------|---------|------|
| **동시 접속** | 10,000 유지 | 9,000~9,999 | < 9,000 |
| **서버 틱 지연** | ≤ 50ms (20Hz) | 50~80ms | > 80ms |
| **Heartbeat RTT p50** | ≤ 20ms | 20~50ms | > 50ms |
| **Heartbeat RTT p99** | ≤ 100ms | 100~200ms | > 200ms |
| **패킷 손실률** | ≤ 0.1% | 0.1~1% | > 1% |
| **서버 CPU** | ≤ 70% | 70~85% | > 85% |
| **서버 메모리** | ≤ 8GB | 8~12GB | > 12GB 또는 지속 증가 (누수) |
| **대역폭** | ≤ 500Mbps | 500~800Mbps | > 800Mbps |
| **크래시** | 0회 | - | 1회 이상 |

### 9-2. 백엔드 (Go)

| 지표 | Pass | Warning | Fail |
|------|------|---------|------|
| **로그인 응답 p95** | ≤ 500ms | 500ms~1s | > 1s |
| **매칭 응답 p95** | ≤ 1s | 1~2s | > 2s |
| **HTTP 에러율** | ≤ 1% | 1~5% | > 5% |
| **PostgreSQL 커넥션** | ≤ 100 | 100~200 | pool exhaustion |
| **Redis 지연 p99** | ≤ 10ms | 10~50ms | > 50ms |
| **Kafka consumer lag** | ≤ 1,000 | 1,000~10,000 | > 10,000 |

---

## 10. 결과 보고서 템플릿

각 테스트 단계 완료 후 아래 형식으로 결과를 기록한다.

```markdown
# 부하 테스트 결과 — Stage X (YYYY-MM-DD)

## 환경
- 게임 서버: [IP, CPU, RAM, OS]
- 봇 머신: [대수, 각 스펙]
- 네트워크: [대역폭, 스위치 모델]

## 테스트 파라미터
- CCU 목표: X
- 실제 접속: X
- 테스트 시간: Xh Xm
- 봇 프로파일: [Idle X, Walker X, Fighter X, Chatter X]

## 결과 요약

| 지표 | 결과 | 판정 |
|------|------|------|
| 동시 접속 유지 | | ✅/⚠️/❌ |
| 서버 틱 지연 (avg/p99) | | ✅/⚠️/❌ |
| Heartbeat RTT (p50/p99) | | ✅/⚠️/❌ |
| 패킷 손실률 | | ✅/⚠️/❌ |
| 서버 CPU (avg/max) | | ✅/⚠️/❌ |
| 서버 메모리 (avg/max) | | ✅/⚠️/❌ |
| 대역폭 (avg/max) | | ✅/⚠️/❌ |
| 크래시 횟수 | | ✅/❌ |

## Wireshark 분석
- 캡처 파일: [경로]
- 총 패킷 수:
- 평균 PPS (C→S / S→C):
- 패킷 크기 분포:
- 이상 패킷:

## 발견된 이슈
1. [이슈 설명 + Wireshark 스크린샷/필터]

## 다음 단계
- [ ] 이슈 수정 후 재테스트
- [ ] 다음 Stage 진행
```

---

## 11. 트러블슈팅 가이드

### 11-1. Wireshark 캡처 문제

| 증상 | 원인 | 해결 |
|------|------|------|
| 패킷이 안 잡힘 | 잘못된 인터페이스 선택 | `tshark -D`로 인터페이스 목록 확인 |
| localhost 패킷 안 보임 | Npcap 루프백 미설치 | Npcap 재설치 (Install Npcap in WinPcap API-compatible mode 체크) |
| 캡처 중 Wireshark 크래시 | 대용량 트래픽 GUI 표시 | tshark CLI 사용, GUI는 오프라인 분석만 |
| pcap 파일이 너무 큼 | 필터 미적용 | 캡처 필터 적용 (`udp port 7777`) |
| Dissector 미작동 | Lua 플러그인 경로 오류 | `Help → About → Folders → Personal Plugins` 경로 확인 |

### 11-2. 서버 성능 문제

| 증상 | Wireshark 확인 | 원인 가능성 | 해결 |
|------|---------------|------------|------|
| 틱 지연 증가 | S2C_Snapshot 간격 > 50ms | 게임 로직 병목 | 프로파일러로 핫스팟 확인 |
| 패킷 드롭 | 시퀀스 번호 갭 | IOCP 소켓 버퍼 부족 | `setsockopt SO_RCVBUF` 증가 |
| 대역폭 폭발 | BPS 급증 | AOI 미작동 | AOI 그리드 크기 조정 |
| 메모리 증가 | 세션 수 정상인데 메모리↑ | 패킷 버퍼 누수 | 메모리 프로파일러 (VLD) |
| 접속 실패 | TCP SYN 재전송 | Accept 큐 부족 | listen backlog 증가 |

### 11-3. 봇 클라이언트 문제

| 증상 | 원인 | 해결 |
|------|------|------|
| 2,500 봇 이상 생성 불가 | 소켓 FD 제한 | 레지스트리 MaxUserPort 수정 (65534) |
| 봇 연결 타임아웃 | 포트 고갈 | `netsh int ipv4 set dynamicport udp start=1025 num=64000` |
| 봇 머신 CPU 100% | 봇 루프가 busy-wait | Sleep(1) 추가 또는 타이머 기반 전송 |

### 11-4. Windows 소켓 튜닝

```bash
# 최대 동적 포트 범위 확장 (봇 머신에서 실행)
netsh int ipv4 set dynamicport udp start=1025 num=64000
netsh int ipv4 set dynamicport tcp start=1025 num=64000

# TIME_WAIT 타임아웃 단축 (레지스트리)
# HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters
# TcpTimedWaitDelay = 30 (기본 120초 → 30초)

# 소켓 버퍼 크기 확인
netsh winsock show catalog
```

---

## 부록: 파일 구조

```
C:\LoadTest\                          ← 테스트 결과 저장 루트
├── stage1_100ccu.pcapng
├── stage2_1000ccu.pcapng
├── stage3_server_5000ccu.pcapng
├── stage3_client1_5000ccu.pcapng
├── stage4_server_10000ccu.pcapng
├── stage5_stress.pcapng
├── backend_traffic.pcapng
├── k6_result.json
├── server_metrics.csv
├── pps_stats.csv
└── reports\
    ├── stage1_report.md
    ├── stage2_report.md
    ├── stage3_report.md
    ├── stage4_report.md
    └── stage5_report.md
```

---

> **참고**: 이 계획서는 서버/백엔드가 테스트 가능 수준에 도달한 후 실행한다.
> 각 Stage를 순차적으로 진행하며, 이전 Stage에서 Pass 판정을 받은 후 다음 Stage로 진행한다.
> Stage 4(10K CCU)에서 모든 지표가 Pass이면 부하 테스트 완료로 판정한다.
