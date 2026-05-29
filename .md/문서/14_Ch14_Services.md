# Ch14. Backend Services (Auth / Shop / Match / Profile / Telemetry / LiveOps)

> Winters 현재: `Services/` Go monorepo + docker-compose. 모듈: auth, shop, matchmaking, profile, payment, leaderboard.
> 레퍼런스: `UnrealEngine/Engine/Plugins/Online/OnlineSubsystem/`, `Runtime/Online/BackgroundHTTP/`.

---

## 1. 기초 원리 — Services는 게임이 아니다

게임 클라/서버와 backend services의 차이:
- **게임**: low-latency (ms 단위), stateful, realtime simulation, C++
- **services**: throughput 최적화 (수만 RPS), stateless API, persistent storage, REST/gRPC, Go/Java/Rust

같은 사람이 짜기 어렵다. 분리된 팀이 운영. **endpoint contract**(스키마)가 협업 인터페이스.

AAA 라이브 서비스의 services 책임:
1. **인증**: 로그인, 토큰, 세션 → 게임 서버가 신뢰
2. **매칭**: 플레이어를 모아 게임 서버 인스턴스 할당
3. **상점/인벤토리**: 구매, 소유권, 사기 방지
4. **소셜**: 친구, 길드, 채팅, 우편
5. **랭킹/리더보드**: 시즌, 통계
6. **라이브 이벤트**: 패치 노트, 이벤트, A/B 테스트
7. **텔레메트리**: 모든 게임 이벤트 수집, 분석
8. **CDN/패치**: 게임 빌드 배포, asset patch
9. **결제**: 안전한 트랜잭션
10. **CMS**: 운영자가 콘텐츠/이벤트 schedule

---

## 2. 핵심 — Online Subsystem 패턴

### 2.1 UE5 Online Subsystem

`UnrealEngine/Engine/Plugins/Online/OnlineSubsystem/Source/Public/OnlineSubsystem.h`.

UE5는 backend를 **interface**로 추상화. 백엔드 구현체(Steam, Epic Online Services, Xbox Live, PSN)가 갈아끼움.

```cpp
class IOnlineSubsystem
{
public:
    virtual IOnlineSessionPtr     GetSessionInterface() const = 0;
    virtual IOnlineFriendsPtr     GetFriendsInterface() const = 0;
    virtual IOnlinePartyPtr       GetPartyInterface() const = 0;
    virtual IOnlineIdentityPtr    GetIdentityInterface() const = 0;
    virtual IOnlinePresencePtr    GetPresenceInterface() const = 0;
    virtual IOnlineUserCloudPtr   GetUserCloudInterface() const = 0;
    virtual IOnlineStorePtr       GetStoreInterface() const = 0;
    virtual IOnlinePurchasePtr    GetPurchaseInterface() const = 0;
    virtual IOnlineAchievementsPtr GetAchievementsInterface() const = 0;
    virtual IOnlineLeaderboardsPtr GetLeaderboardsInterface() const = 0;
};
```

게임 코드:
```cpp
IOnlineSubsystem* OSS = IOnlineSubsystem::Get();   // "STEAM" / "EOS" / "NULL"
OSS->GetIdentityInterface()->Login(0, Credentials);
```

→ 같은 게임 코드를 Steam/Epic/Xbox에 출시. 백엔드 swap.

### 2.2 Go Microservice 패턴 (Winters 현 구조)

```text
Services/
├── cmd/                       각 서비스의 main.go entry
│   ├── auth/
│   ├── matchmaking/
│   └── ...
├── internal/                  비공개 비즈니스 로직
│   ├── auth/
│   │   ├── handler.go         HTTP/gRPC 핸들러
│   │   ├── service.go         비즈니스 로직
│   │   ├── repository.go      DB 접근
│   │   └── model.go           도메인 모델
│   ├── matchmaking/...
│   └── shop/...
├── pkg/                       재사용 가능한 공용 패키지
├── migrations/                DB 스키마 변경
├── go.mod
└── docker-compose.yml
```

이게 **Clean Architecture / Hexagonal Architecture** 패턴. layer 분리:
```text
handler → service → repository → DB
   ↑          ↑           ↑
 HTTP      도메인       Postgres / Redis
 routing  로직 진실     영속화
```

### 2.3 Service-to-Service 통신

- **REST / HTTP+JSON**: 간단, 디버그 쉬움, 느림
- **gRPC + Protobuf**: 강타입, 빠름, schema 진화 어려움
- **Message Queue (Kafka/NATS/Pulsar)**: 비동기, 이벤트 기반
- **GraphQL**: 클라가 필요한 필드만 fetch

LoL/로아 같은 게임은 hybrid:
- 동기 요청: gRPC (auth → match)
- 비동기 이벤트: Kafka (gameplay event → analytics)

### 2.4 Data Storage 패턴

| 데이터 | 저장소 | 이유 |
|--------|--------|------|
| 유저 계정 | PostgreSQL | ACID, 관계 |
| 인벤토리 | PostgreSQL + Redis cache | 자주 읽음, 손실 불가 |
| 세션 / 토큰 | Redis | 빠름, TTL |
| 채팅 메시지 | Redis Streams / Kafka | 시계열, 대량 |
| 리더보드 | Redis Sorted Set | O(log N) ranked |
| 텔레메트리 raw | Kafka → ClickHouse / BigQuery | 거대, 분석 |
| Asset / Patch | S3 / GCS + CDN | 정적, 글로벌 |
| Search (친구 검색) | Elasticsearch | 텍스트 검색 |

### 2.5 Observability

- **Logs**: structured (JSON) → Loki / ELK
- **Metrics**: Prometheus → Grafana
- **Traces**: OpenTelemetry → Jaeger / Tempo
- **Errors**: Sentry / Backtrace

라이브 서비스 운영의 필수. 새벽에 사고 나면 이 3종 없으면 못 자랑함.

---

## 3. 심화

### 3.1 Matchmaking

매우 어려운 분산 문제.

```text
[Naive]
queue.push(player)
while queue.size >= 10:
    teams = queue.pop(10)
    game = launch_server(teams)

[현실]
- ELO/MMR 매칭 (실력 차 minimization)
- region 매칭 (latency)
- role 매칭 (LoL: top/jungle/mid/bot/sup)
- queue time vs match quality tradeoff (대기 길어지면 quality 완화)
- party 보존 (3인 파티는 다른 4인 파티 + 3명 솔로와)
- premades / smurf 제재
- 백엔드 게임 서버 fleet 가용성
```

오픈소스: OpenMatch (Google). 자체 구현은 보통.

### 3.2 게임 서버 Fleet 관리

```text
GameLift / Agones / 자체 fleet manager
  - 인스턴스 풀 (warm)
  - 새 게임 시작 시 instance 할당
  - 게임 끝나면 instance reset 또는 destroy
  - region별 capacity 계획
  - auto-scaling (피크 시간)
```

GTA6 / 로아처럼 게임당 1 instance 모델이면 더 복잡 (인스턴스 dispatch + state preservation).

### 3.3 Anti-fraud / Anti-cheat

- **클라 무결성**: 게임 binary hash → 변조 검출
- **결제 fraud**: 카드 도난, chargeback 패턴 ML
- **봇 검출**: 행동 패턴 (마우스 / 타이밍) ML
- **계정 sharing**: 디바이스 / IP 추적
- **gold farming**: 거래 그래프 분석

### 3.4 Live Ops / CMS

운영자(코드 아닌 사람)가 게임에 변경을 주는 시스템.

```text
Live Ops Dashboard:
  - 이벤트 시작/종료 시간 설정
  - 보상 테이블 변경 (drop rate, currency multiplier)
  - 패치노트 등록
  - 점검 공지
  - 핫픽스 (config push without client patch)
  - A/B 테스트 group 설정
```

LoL 패치, 로아 시즌 시작, GTA6 weekly event 모두 이 시스템 위.

### 3.5 멀티 리전

```text
Region: NA, EU, KR, JP, SEA, BR
Per-region:
  - 게임 서버 cluster
  - Account DB replica (또는 region별 shard)
  - CDN edge
Cross-region:
  - 글로벌 인증 (account)
  - 글로벌 chat / friend
  - 데이터 sovereignty (KR 유저 데이터는 KR에 — 법규)
```

복잡함의 핵심. AWS Global Accelerator / Cloudflare 같은 인프라 + 자체 routing.

### 3.6 Telemetry / Analytics

```text
Client emits event:
   {"event": "skill_used", "skill": "Ezreal_Q", "ts": ..., "player_id": ...}
   → batch → Kafka → ClickHouse / BigQuery
   ↓
   Daily ETL → Aggregate (DAU, retention, monetization)
   ↓
   Dashboard (Looker, Metabase)
   ↓
   Game Design 의사결정
```

LoL 챔프 밸런스 패치의 데이터 근거. 모든 의사결정의 뿌리.

---

## 4. Winters 매핑

### 4.1 현재 상태

`Services/` 골격:
- auth, shop, matchmaking, profile, payment, leaderboard (6 모듈)
- 각 모듈 handler.go/service.go/repository.go/model.go layered
- docker-compose, migrations
- Go monorepo

좋은 출발. 다음 단계:

### 4.2 Ch14 추가 모듈 (제안 — 본 brief 그대로 박제)

```text
Services/internal/
  inventory/          캐릭별 인벤/창고/펫 (로아)
  guild/              길드/연합
  social/             친구/파티/우편/채팅
  liveops/            이벤트/패치노트/공지/CMS
  telemetry/          gameplay event ingest (Kafka producer)
  analytics/          DAU/MAU/retention/funnel (BigQuery 등 외부 DB)
  antifraud/          결제 fraud / 봇 탐지
  crashreport/        Sentry/Backtrace 등가
  notification/       push / 이메일
  cdn/                패치 / asset 다운로드 메타
  region/             멀티 region routing
  presence/           온라인 상태
  party/              파티 룸 (매칭 직전)
  chat/               실시간 chat (Redis Streams/Kafka)

Services/migrations/    Atlas / sqlc / golang-migrate
Services/observability/ Prometheus + Grafana + Loki dashboard
Services/gateway/       API gateway (rate limit, auth verify, routing)
Services/pkg/           공용 (logger, tracer, error types, IDs)
```

### 4.3 Client-side Online 추상

UE5처럼 게임 코드가 백엔드를 모르게:

```cpp
// Client/Public/Network/OnlineSubsystem.h
class WINTERS_CLIENT COnlineSubsystem
{
public:
    IIdentityInterface*    Identity();
    IMatchmakingInterface* Matchmaking();
    IShopInterface*        Shop();
    IInventoryInterface*   Inventory();
    IFriendsInterface*     Friends();
    IPartyInterface*       Party();
    IChatInterface*        Chat();
    ILeaderboardInterface* Leaderboard();
    IAchievementInterface* Achievement();
    IPresenceInterface*    Presence();
    ITelemetryInterface*   Telemetry();
};

// 구현체:
//   COnlineSubsystem_WintersBackend (자체 Go services)
//   COnlineSubsystem_Steam          (Steam SDK)
//   COnlineSubsystem_Epic           (EOS)
```

### 4.4 Schema 공유 (Single Source of Truth)

C++ 클라/서버 ↔ Go services 사이 데이터 type 공유 필요.

옵션:
- **Protobuf** (gRPC + 자동 codegen Go/C++)
- **FlatBuffers** (현재 Winters Shared/Network/PacketDef와 정합)
- **JSON Schema + 자체 codegen**

Winters는 이미 FlatBuffers 쓰니까 같은 .fbs를 backend에서도 사용 권장.

### 4.5 Bot AI / 서버 권위와의 관계

Services는 **게임 외부**. 게임 서버 안에 있는 Bot AI와 무관. 단:
- 매치 시작 시 services가 게임 서버 인스턴스에 "player_X with bot composition"를 전달
- 봇 통계도 telemetry에 들어감 (실제 플레이어 매칭에 도움 / 봇 행동 디버그)

### 4.6 단계별

```text
Ch14-Stage1  현재 6모듈 안정화 + observability 도입
Ch14-Stage2  Schema 공유 (Protobuf 또는 FlatBuffers)
Ch14-Stage3  Inventory / Guild / Social
Ch14-Stage4  LiveOps CMS + Notification
Ch14-Stage5  Telemetry pipeline (Kafka → BigQuery)
Ch14-Stage6  Antifraud / Crashreport
Ch14-Stage7  Multi-region routing
Ch14-Stage8  Client Online Subsystem 추상 (Steam/Epic backend 추가 가능)
Ch14-Stage9  Game server fleet manager (Agones / 자체)
Ch14-Stage10 ML/AB testing 인프라
```

### 4.7 게임별 적용

| 게임 | 필요 Stage |
|------|-----------|
| LoL | Stage 1~5 + 7~9 (region 중요) |
| 로아 | Stage 1~6 + auction house + market |
| 엘든링 | Stage 1, 8 (Steam/PSN/Xbox interop 중심) + 일부 5 |
| GTA6 | 모든 Stage + region + heist instancing + crew |

---

## 5. 검증 명령

```bash
# Stage 1 도입 후
cd Services
docker-compose up
curl -X POST http://localhost:8080/auth/login -d '{"email":"a@b.com","pw":"..."}'
# {"token": "..."}

# Observability
open http://localhost:3000   # Grafana
# auth_login_total{status="success"} = 1234
# auth_login_latency_p99{}     = 87ms

# Telemetry pipeline (Stage 5)
kcat -b kafka:9092 -t gameplay_events -C | head
# {"event":"skill_used","player":"abc","skill":"Ezreal_Q","ts":"..."}
```

---

## 6. 다음 챕터로

Ch14 Stage 5 (Telemetry) → 데이터 기반 의사결정의 출발. Ch15 Data pipeline은 backend cooker와 같이 cook된 데이터를 services가 호스팅하는 부분에서 합류.
