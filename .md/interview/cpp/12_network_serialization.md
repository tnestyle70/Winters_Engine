# 네트워크 · 직렬화 · C++

> 대상: Winters 엔진(C++17/DX11, LoL 스타일 서버-권위 클라+서버)을 직접 만든 지원자.
> 이 챕터의 목표: "네트워크 바이트를 C++ 객체로 되돌리는 경계에서 무슨 일이 벌어지고, 왜 대부분의 버그와 취약점이 그 경계에 몰리는가"를 자기 코드로 증명하는 것.

---

## ① 한 줄 본질

**네트워크 경계는 신뢰할 수 없는 바이트(untrusted bytes)가 들어오는 신뢰 경계(trust boundary)이고, 직렬화(serialization)는 메모리 표현을 바이트 스트림으로 바꾸는 일인데 — 패딩·엔디안·포인터 때문에 C++ struct를 그대로 memcpy로 보내면 깨진다. 그래서 (1) 레이아웃을 명시적으로 고정하고, (2) 역참조 전에 반드시 verify하고, (3) 실패를 조용히 삼키지 않는다.**

면접에서 "네트워크로 받은 데이터를 어떻게 다루냐"는 질문의 첫 문장으로 그대로 쓸 수 있다.

---

## ② 기본 개념

### 2.1 TCP vs UDP — 무엇을 고르나

| | TCP | UDP |
|---|---|---|
| 모델 | 신뢰성 있는 **바이트 스트림** | 비신뢰 **데이터그램(메시지)** |
| 순서/재전송 | 커널이 보장 | 없음 (필요하면 직접 구현) |
| 메시지 경계 | **없음** (직접 프레이밍) | 있음 (패킷 = 메시지) |
| head-of-line blocking | 있음 | 없음 |

핵심 오해 교정: **"TCP는 느리고 UDP는 빠르다"가 아니다.** TCP의 진짜 비용은 *head-of-line blocking* — 패킷 하나가 손실되면 그 뒤에 이미 도착한 패킷들도 커널 버퍼에 갇혀 애플리케이션에 전달되지 않는다. 위치 갱신처럼 "최신 상태만 중요하고 과거 손실은 버려도 되는" 데이터에는 이게 치명적이라 프로 FPS/MOBA는 게임플레이 트래픽을 UDP + 자체 신뢰성 계층으로 나른다. 반대로 로그인·로비·상점처럼 "한 바이트도 빠짐없이 순서대로 와야 하는" 트래픽은 TCP가 맞다.

Winters는 현재 게임플레이 replication에도 TCP를 쓴다(개인 프로젝트 규모의 MVP 선택). 대신 스냅샷을 idempotent하게 — 최신 스냅샷 하나가 이전 상태를 완전히 덮어쓰게 — 설계해 손실/지연의 피해를 줄였고, UDP 신뢰성 계층(`Shared/Network/UdpReliabilityChannel.h`, `UdpFragmentHeader.h`)을 병행 설계해 두었다. 면접에서는 "TCP를 골랐지만 head-of-line blocking이 무엇인지 알고, 그 비용을 상쇄하는 설계를 했다"고 말하는 게 정확하다.

참고로 소켓 API 자체는 C 인터페이스라 C++ 레벨의 안전장치가 없다 — 그래서 이 챕터의 나머지 전부(RAII, 소유권, 타입 안전 enum, verify)가 "C API 위에 C++로 경계를 다시 세우는 일"이다.

### 2.2 TCP 프레이밍 — 바이트 스트림에서 메시지 경계를 만든다

TCP는 `send()` 한 번이 `recv()` 한 번과 대응하지 **않는다.** 100바이트를 보내도 40+60으로 쪼개져 오거나, 두 메시지가 붙어서 한 번에 올 수 있다. 그래서 메시지 경계는 애플리케이션이 만들어야 하고, 표준 관용구는 **length-prefix framing**: 헤더에 payload 길이를 박고, 수신측은 바이트를 누적하다가 "헤더 + payload가 전부 도착"했을 때만 한 프레임을 꺼낸다.

세 가지 결과 상태가 필요하다: **NeedMore**(아직 덜 옴 → 대기), **Complete**(한 프레임 완성 → 소비), **Invalid**(magic/version 불일치·길이 초과 → 버퍼 폐기/연결 차단).

### 2.3 소켓 수명과 RAII · 비동기 IO의 버퍼 수명

소켓은 커널 핸들이므로 소유 객체의 소멸자(destructor)에서 `closesocket`을 부르는 RAII가 정석이다. 하지만 **비동기 IO(overlapped/IOCP)에서 진짜 어려운 건 소켓이 아니라 버퍼와 `OVERLAPPED` 구조체의 수명**이다.

`WSARecv`/`WSASend`에 넘긴 버퍼와 `OVERLAPPED`는 **커널이 IO 완료 시점까지 계속 참조**한다. 함수는 `WSA_IO_PENDING`으로 즉시 리턴하지만 IO는 아직 진행 중이다. 이 버퍼를 스택 지역변수로 두면 함수가 리턴하는 순간부터 커널이 소멸된 메모리에 write하는 use-after-free가 된다. 그래서 버퍼/OVERLAPPED는 **"오퍼레이션 완료 시점 이상"** 살아야 한다 — 보통 세션 객체의 멤버 또는 오퍼레이션별 heap 컨텍스트로 둔다. 세션 자체도 pending IO가 남아있는 동안 파괴되면 안 되므로, `shared_ptr` 세션 + pending IO 카운트 같은 수명 연장 장치가 따라온다.

### 2.4 버퍼 관리와 소유권

송신 경로에서 물어야 할 질문은 항상 "이 바이트들은 **누가 소유**하고, **언제까지** 살아야 하나"다:

- **송신 버퍼**: 커널이 다 보낼 때까지 살아야 한다 → 호출자에게 빌리지 말고 **값으로 받아(move) 큐가 소유**하게 한다. 완료 콜백에서 pop.
- **수신 버퍼**: 고정 크기 멤버 버퍼에 받고, 프레이밍 파서가 자기 누적 버퍼로 **소유 복사**한다. 파서 버퍼에는 상한을 둬서 악성 길이 필드가 메모리를 무한히 먹는 DoS를 막는다.
- **직렬화 결과 버퍼**: 빌더가 만든 큰 버퍼는 복사하지 말고 **move-only 소유 타입**(FlatBuffers의 `DetachedBuffer`)으로 반환해 패킷 경로까지 복사 0회로 흐르게 한다.

### 2.5 직렬화에서 C++ 타입의 함정 세 가지

**(1) 패딩(padding).** 컴파일러는 정렬(alignment)을 위해 멤버 사이에 보이지 않는 바이트를 삽입한다. `struct { u16 a; u32 b; }`는 6바이트가 아니라 8바이트일 수 있다. 이 struct를 그대로 wire에 쓰면 상대 컴파일러/플랫폼의 패딩 규칙이 다를 때 필드가 어긋난다. 게다가 패딩 바이트는 미초기화 상태라 정보 누출 통로도 된다.

**(2) 엔디안(endianness).** 정수가 메모리에 놓이는 바이트 순서(little vs big)는 플랫폼마다 다르다. x86/ARM 리틀엔디안끼리는 우연히 맞지만, 정수를 raw로 보내는 순간 "양끝이 같은 엔디안"이라는 암묵적 가정을 하는 셈이다. FlatBuffers 같은 포맷은 wire 표현을 리틀엔디안으로 **명시 정의**해 이 가정을 계약으로 바꾼다.

**(3) 포인터는 보낼 수 없다.** 포인터는 *내 프로세스의 주소 공간*에서만 유효한 숫자다. 받는 쪽에선 쓰레기다. `std::string`·`std::vector`·가상 함수 테이블(vtable pointer)을 가진 클래스도 내부에 포인터를 품으므로 그대로 직렬화 불가 — **오프셋(offset)이나 인덱스, 별도 id로 바꿔야** 한다. 엔티티 참조를 로컬 `EntityID`가 아닌 `NetEntityId`로 보내는 것도 같은 원리다.

### 2.6 수동 패킹 vs zero-copy 스키마 방식

- **수동 패킹**: `#pragma pack(1)`로 패딩을 제거하고 `static_assert(sizeof == N)`으로 크기를 컴파일 타임에 못박은 뒤 `memcpy`로 넣고 뺀다. **작고 고정 크기인 헤더**에 적합. 이때 `reinterpret_cast<Header*>(buf)` 대신 `memcpy(&hdr, buf, sizeof hdr)`를 쓰는 이유는 **정렬 위반 UB와 strict-aliasing UB를 동시에 피하기 위해서**다 — 수신 버퍼의 임의 오프셋이 헤더 정렬을 만족한다는 보장이 없다. 최적화가 켜지면 memcpy는 레지스터 로드로 사라진다.

- **zero-copy 스키마(FlatBuffers 등)**: 스키마(.fbs)에서 코드를 생성하고, 직렬화된 버퍼를 **역직렬화(중간 객체 생성) 없이 그대로** 접근자(`snapshot->serverTick()`)로 읽는다. 접근자는 버퍼 안의 offset을 따라간다. 가변 길이(vector/string/중첩 테이블)와 스키마 진화(필드 추가 시 전방/후방 호환)를 지원한다. 대신 **버퍼가 신뢰 불가하면 접근자가 곧 임의 메모리 역참조**가 되므로 verify가 필수다.

Winters는 둘을 **역할별로 나눠 쓴다**: 바깥 봉투(magic/version/type/length)는 수동 패킹 `PacketHeader`, 안쪽 payload(스냅샷/명령/이벤트)는 FlatBuffers.

### 2.7 verify의 의미 = 신뢰 경계(trust boundary)

**verify는 "파싱 실패 검사"가 아니라 "이 바이트를 역참조해도 프로세스가 안 죽는다는 증명"이다.** FlatBuffers `Verifier`는 버퍼 안의 모든 offset이 버퍼 범위 안을 가리키는지, vector/string 길이가 실제 남은 크기와 맞는지를 한 번 훑는다. 이걸 통과해야만 `GetSnapshot(buf)`가 돌려준 포인터의 접근자가 안전하다. verify 없이 필드를 읽으면 손상/악의적 버퍼가 임의 주소 읽기를 유발한다 — 원격 크래시, 정보 누출.

그리고 verify **실패의 처리**가 실전에서는 verify 자체만큼 중요하다: bare `return`으로 삼키면 "스키마 drift로 전부 버려지는 중"과 "네트워크가 멈춘 것"이 똑같이 조용한 월드 프리즈로 보인다. 복제 경계의 실패는 유한 로그/카운터/세션 플래그를 반드시 남긴다.

---

## ③ 심화 (꼬리질문 대비)

### 3.1 move vs copy — 팬아웃(fan-out)이 결정한다
소비자가 **1명**이면 `std::move`로 버퍼 소유권을 넘긴다(복사 0회). **N명에게 broadcast**하면 move가 소스를 비워버리므로 불가 — 각 수신자가 자기 소유의 복사본을 가져야 한다. "언제 move, 언제 copy?"의 정답은 추상적 규칙이 아니라 *소비자 수*다. (진짜 대규모 팬아웃이라면 `shared_ptr<const vector<u8_t>>`로 불변 버퍼를 공유하는 3안도 있다 — 다만 수신자별 전송 완료 시점 추적이 복잡해지므로, 세션 수가 작을 땐 명시적 복사가 더 단순하고 옳다.)

### 3.2 시퀀스 번호의 wraparound-안전 비교
`u32` 시퀀스 카운터는 2³²에서 0으로 되돈다(wraparound). `a > b`로 비교하면 랩 근처에서 최신 패킷을 과거로 오판한다. 정답 관용구: **`static_cast<int32_t>(a - b) > 0`**. 무부호 뺄셈은 모듈로 2³²로 잘 정의되고(오버플로 UB 아님), 그 차이를 부호 있는 정수로 재해석하면 "최근 절반 윈도우(2³¹)" 안에서는 랩을 건너도 앞뒤 관계가 올바르다. TCP 시퀀스 번호 비교와 같은 원리.

### 3.3 결정론(determinism) — 서버 권위 시뮬의 생명줄
클라 예측과 서버 시뮬이 **비트-동일**해야 스냅샷 화해가 어긋나지 않고 리플레이가 재현된다. 이를 깨는 3대 요인과 대응:
- **부동소수 재결합**: `/fp:fast`는 `(a+b)+c`를 최적화 수준/플랫폼별로 다르게 축약 → 전 프로젝트 `/fp:precise` 통일.
- **컨테이너 순회 순서 흔들림**: ECS 컴포넌트 저장 순서는 스폰/파괴로 바뀜 → 직렬화 전 안정 키(netId)로 재정렬.
- **입력 순서 비결정성**: 여러 세션의 명령이 한 틱에 섞임 → `stable_sort`로 (tick, session, seq) 결정론적 정렬.

### 3.4 스냅샷 vs 이벤트 복제 — 상보적이다
- **스냅샷(state replication)**: "지금 이 엔티티의 위치/HP는 이것"이라는 **절대 상태**. idempotent하고 **손실 내성**이 있다 — 최신 하나만 도착하면 복구된다. 위치·HP·스킬 랭크·쿨다운에 적합.
- **이벤트(event replication)**: "방금 스킬이 발사됐다"는 **한 번뿐인 사실**. 놓치면 복구 불가 → 순서 보장/재전송이 필요하고, 중복 재생을 막는 seq 검사가 필수. 순간적 FX·타격 연출에 적합.

한 틱에 둘 다 흐른다: 스냅샷이 "세계의 현재"를, 이벤트가 "방금 일어난 일회성 사건"을 나른다. 꼬리질문 "델타 압축(delta compression)은?"에는: 스냅샷을 매번 풀 상태로 보내면 대역폭이 크지만 구현이 단순하고 손실 내성이 최대다. 델타는 "기준 스냅샷 대비 변화만" 보내 대역폭을 줄이는 대신, 클라가 어떤 기준(baseline)을 갖고 있는지 ack로 추적해야 해서 복잡도가 뛴다 — 규모가 정당화될 때 도입하는 최적화라고 답한다.

### 3.5 명령 코얼레싱(command coalescing)
빠른 연속 우클릭이 이동 명령을 큐에 5개 쌓으면 서버가 매 틱 다른 목적지로 홱홱 방향을 튼다. 최신 이동 의도만 의미가 있으므로 **같은 세션의 pending Move를 새 것으로 in-place 교체**한다. 단 스킬 같은 비-Move 명령은 절대 병합하지 않는다 — 각각이 권위 있는 사건이다. 코얼레싱의 핵심은 "kind별로 다르게".

### 3.6 예측 화해(client-side prediction reconciliation)
우클릭 순간 클라가 몸통 yaw를 로컬 예측한다. 서버 스냅샷이 그 명령을 아직 반영하기 전에 도착해 예측 yaw를 덮으면 "홱 돌았다 되돌아오는" 아티팩트가 난다. 해법은 **"언제 로컬을 믿고 언제 서버로 되감을지"의 상태머신**: 서버 ack이 예측 명령을 커버했나 / 서버 값이 예측을 따라잡았나 / 서버가 액션락 상태인가 / 반대 방향인가를 조합해 결정하고, 유예가 만료되면 무조건 서버를 채택한다.

### 3.7 wire enum과 id 안정성
- **enum을 wire로 보낼 땐**: (1) underlying type 고정(`: uint8_t`)으로 크기 확정, (2) 값 명시(`IRELIA = 1`)로 재정렬 내성, (3) NONE/END 센티널로 무효값 표현.
- **wire 정수 → enum 캐스트는 위험**: 범위 밖 값을 `static_cast<enum>`하면 이후 switch/배열 인덱싱이 미정의 동작(UB) 영역으로 간다. 캐스트 전 유효 범위 clamp 필수.
- **로컬 엔티티 id를 그대로 노출하지 마라**: 내부 ECS 핸들 대신 별도 `NetEntityId`로 매핑하면 결합도가 낮아지고, 클라가 서버 내부 배열 인덱스를 추측하지 못한다(보안 이점).

---

## ④ Winters에서의 적용

### 4.1 TCP 프레이밍 — length-prefix 누적 파서
`Server/Private/Network/FrameParser.cpp:13`의 `TryPop`이 세 상태 관용구를 그대로 구현한다:
- `FrameParser.cpp:17` 헤더 크기 미만이면 `NeedMore`.
- `FrameParser.cpp:21` `std::memcpy(&hdr, m_Buffer.data(), sizeof(PacketHeader))` — 포인터 캐스팅이 아닌 memcpy로 정렬/aliasing UB 회피.
- `FrameParser.cpp:23-32` magic/version 불일치 또는 `payloadSize > kMaxPayloadBytes`면 `Clear()` 후 `Invalid`.
- `FrameParser.cpp:40-45` payload를 `assign`으로 소유 복사하고 소비한 만큼 `erase`.
- `FrameParser.cpp:9` `Append`에서 누적 버퍼가 `kMaxBufferBytes`를 넘으면 통째로 버림 → **악성 길이 필드로 무한 누적시키는 DoS 방어**.

왜 이 설계? TCP 스트림에는 메시지 경계가 없어 부분 수신을 상태로 관리해야 하고, 상한 없는 누적 버퍼는 신뢰 불가 입력에게 메모리를 내주는 것이기 때문.

### 4.2 wire 헤더 레이아웃 고정 — pack + static_assert + memcpy
`Shared/Network/PacketEnvelope.h:34-44` `#pragma pack(push, 1)`로 `PacketHeader`(magic/version/type/flags/payloadSize/sequence)를 패딩 없이 배치하고, `PacketEnvelope.h:46` `static_assert(sizeof(PacketHeader) == 16, "PacketHeader must be 16 bytes for wire stability.")`로 와이어 크기를 컴파일 타임에 못박는다. `PacketEnvelope.h:67`(WrapEnvelope)과 `:86`(TryExtractFrame)은 `reinterpret_cast`가 아닌 `memcpy`로 헤더를 넣고 뺀다.

왜? 컴파일러 버전/옵션이 바뀌어도 패킷 레이아웃이 흔들리면 안 되고, static_assert는 그 불변식을 **빌드가 실패하는 방식으로** 강제한다 — 주석은 사람이 안 읽지만 컴파일 에러는 못 지나친다.

### 4.3 enum class(태그) vs plain enum(플래그) — 한 파일 안에 공존
`PacketEnvelope.h:13` `enum class ePacketType : uint16_t`는 와이어 크기를 고정하고 암시적 정수 변환을 막는다. 반면 `PacketEnvelope.h:27` `enum ePacketFlags : uint16_t`는 의도적으로 unscoped enum이라 `PacketFlag_Compressed | PacketFlag_Encrypted` 비트 OR를 캐스트 없이 쓴다. "분류값은 enum class, 비트마스크는 plain enum"이라는 실용 선택이 나란히 있다.

`Shared/GameSim/Definitions/LoLMatchContext.h:5` `enum class eChampion : std::uint8_t`도 같은 원칙 — underlying type 1바이트 고정, 명시적 값(`IRELIA = 1` … `LEESIN = 17`), `NONE = 0`/`END = 255` 센티널. 재정렬/추가에도 wire 값이 흔들리지 않는다.

### 4.4 FlatBuffers 조립 + DetachedBuffer move 반환
`Server/Private/Game/SnapshotBuilder.cpp:106` `Build`는 `flatbuffers::DetachedBuffer`(move-only RAII 소유 버퍼)를 반환한다. `SnapshotBuilder.cpp:115` `FlatBufferBuilder fbb(2048)`로 시작해 자식 오프셋(각 `EntitySnapshot`, vector)을 먼저 만들고 부모 테이블을 나중에 만드는 inside-out 규칙으로 조립한 뒤, `SnapshotBuilder.cpp:879-880` `fbb.Finish(snapshot); return fbb.Release();`로 소유권을 넘긴다.

왜 DetachedBuffer? 직렬화 버퍼는 크고 매 틱 만들어지므로 **소유권을 move로 넘겨** 패킷 래핑 경로까지 복사 0회로 흐르게 한다.

같은 `Finish`/`Release` 패턴이 송신 경로 전반에 반복된다 — 클라 명령 배치(`Client/Private/Network/Client/CommandSerializer.cpp:451-453`), 클라 Hello(`GameSessionClient.cpp:326-327`), 서버 로비 상태(`GameRoomLobby.cpp:277-278`), Shared 복제 이벤트(`Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.cpp:28-29`). "빌더가 만든 버퍼는 Release로 소유권째 넘긴다"가 코드베이스 관용구다.

### 4.5 결정론 직렬화 — netId 재정렬 + stable_sort + /fp:precise
- `SnapshotBuilder.cpp:117` `DeterministicEntityIterator<TransformComponent>::CollectSorted`로 엔티티를 모으고, `SnapshotBuilder.cpp:134` 다시 **netId 기준 `std::sort`**로 재정렬한 뒤 스냅샷 행을 쓴다. ECS 저장 순서가 스폰/파괴로 흔들려도 클라가 매 틱 일관된 순서로 해석한다.
- `Server/Private/Game/CommandIngress.cpp:98-106` `std::stable_sort`로 `(acceptedTick, sessionId, sequenceNum)` 3중 키 정렬. 여러 세션의 명령이 한 틱에 섞여도 서버는 항상 같은 순서로 실행 → 시뮬레이션 결정론과 리플레이 재현. stable이라 동일 키에서 도착 순서까지 보존된다.
- `Shared/GameSim/Include/GameSim.vcxproj:54` `<FloatingPointModel>Precise</FloatingPointModel>` — Engine/Client/Server/GameSim 전 프로젝트 통일. `/fp:fast`의 재결합·축약이 클라/서버 float 결과를 어긋나게 해 결정론을 깨는 걸 막는다.

### 4.6 명령 코얼레싱 — kind별 in-place 교체
`CommandIngress.cpp:74` `if (wire.kind == eCommandKind::Move)`일 때 같은 세션의 기존 pending Move를 찾아 `CommandIngress.cpp:81` `oldPending = pending; return;`으로 덮어쓴다. 비-Move 명령은 이 분기를 타지 않고 `CommandIngress.cpp:87` `push_back`으로 전부 쌓여 권위성을 유지한다. gotcha(2026-05-20 Move coalescing)가 이 규약을 박제했다.

### 4.7 시퀀스 wraparound-안전 비교 — 세 곳에서 같은 관용구
- `Shared/Network/SeqMath.h:5` `SeqGreater`가 `static_cast<i32_t>(lhs - rhs) > 0`으로 공통화.
- `Client/Private/Network/Client/SnapshotApplier.cpp:111-118` `IsCommandSeqAtLeast` — `lhs == rhs || static_cast<i32_t>(lhs - rhs) > 0`, 0은 "명령 없음" sentinel로 별도 처리.
- `Client/Private/Network/Client/EventApplier.cpp:170-176` `IsNewerActionSeq` — 같은 관용구로 오래된/중복 액션 이벤트 재생을 차단(`EventApplier.cpp:572`에서 사용).

### 4.8 verify = 신뢰 경계 + 유한 실패 로그
수신 경계 모두 `GetXxx()` 역참조 **전에** `flatbuffers::Verifier`를 통과시킨다:
- 서버 명령 수신: `Server/Private/Network/PacketDispatcher.cpp:73-78` `VerifyCommandBatchBuffer` 실패 시 그냥 return이 아니라 **`pSession->FlagSuspicious()`**로 세션에 표식을 남긴다.
- 클라 스냅샷: `SnapshotApplier.cpp:561-573` `VerifySnapshotBuffer` 실패 시 **static 카운터로 앞 8회만** `OutputDebugStringA`를 emit하고 return.
- 클라 Hello/이벤트: `SnapshotApplier.cpp:485-497`, `EventApplier.cpp:511` 동일 패턴.

왜 "그냥 return"이 아니라 "유한 로그"인가? gotcha(2026-07-09): **bare return만 하면 스키마 drift와 네트워크 정지가 구분 불가능한 "조용한 월드 프리즈"**가 된다. 무한 로그는 그 자체가 새로운 부하이므로 앞 N회만 남긴다.

추가 방어: `Server/Private/Network/Session.cpp:26-42` `TryAcceptSequence`는 이미 처리한 seq 이하를 버리고(재전송/리플레이 차단), `last + 60`을 넘는 점프는 suspicious로 표시한다 — verify(구조 검증)와 별개로 **프로토콜 수준의 신뢰 검사**다.

### 4.9 접속 시점 데이터 빌드 해시 핸드셰이크
`SnapshotApplier.cpp:505-519` `hello->dataBuildHash()`를 클라의 `ChampionGameDataDB::GetBuildHash()`와 비교한다. 0이면 구버전 서버로 보고 검사 생략(전방 호환), 불일치면 앞 8회 유한 로그. "수치가 미묘하게 다른" 형태로만 나타나던 서버/클라 데이터 drift를 **접속 초반에 폭발**시켜 조용한 오작동을 막는 방어적 프로토콜 설계다.

### 4.10 wire → 로컬 경계의 clamp 방어
- **enum clamp**: `SnapshotApplier.cpp:121-126` `ToSnapshotMinionType`이 `subtype >= static_cast<u16_t>(eMinionType::End)`면 `Melee`로 강등한 뒤에야 캐스트. 범위 밖 enumerator로 인한 switch/인덱싱 사고 차단. `ToSnapshotMinionTeam`(`:128`)은 Red만 매칭하고 나머지를 Blue로 접는 방어적 기본값.
- **고정 배열 오버런 clamp**: `SnapshotApplier.cpp:1211` `std::min<u32_t>(pRanks->size(), SkillRankComponent::kSlotCount)`로 개수를 고정 배열 용량에 자른 뒤 `Get(i)`. 신뢰 불가 가변 길이 vector가 로컬 고정 배열을 넘지 못한다.
- **null 가드**: `SnapshotApplier.cpp:1205` `if (const auto* pRanks = es->skillRanks())` — FlatBuffers optional vector는 항상 null 확인 후 사용. 엔티티 순회도 `SnapshotApplier.cpp:630` `for (const auto* es : *entities)` + per-요소 null 체크.

### 4.11 비동기 IO 버퍼 수명 — IOContext는 오퍼레이션보다 오래 산다
`Server/Public/Network/IOCPCore.h:22-31` `IOContext`가 `OVERLAPPED overlapped`, `WSABUF wsaBuf`, `char buffer[8192]`, op enum, `acceptSocket`을 한 구조체로 묶는다. 이 컨텍스트는 `Server/Public/Network/Session.h:72-73` `m_recvContext`/`m_sendContext`로 **세션 멤버**에 산다 — 스택이 아니다. `Server/Private/Network/Session.cpp:56` 매 재게시 전 `ZeroMemory(&m_recvContext.overlapped, ...)`로 초기화해 재사용하고 `Session.cpp:62` `WSARecv`에 넘긴다.

송신 버퍼 수명은 큐가 소유한다: `Session.cpp:91` `Send(std::vector<u8_t> packet)`가 **값으로 받아** `Session.cpp:101` `m_sendQueue.push_back(std::move(packet))`로 이동시키고, `Session.cpp:108` `wsaBuf.buf`는 큐 front의 데이터를 가리킨다. `OnSendComplete`(`:134-141`)가 pop할 때까지 버퍼는 deque 안에 살아있다. 소켓 자체는 `Session.cpp:173-182` `OnDisconnect`가 `closesocket` 후 `INVALID_SOCKET`으로 밀봉하고, 소멸자(`:21-24`)가 이를 호출하는 RAII. `m_pendingIoCount`/`m_bClosing`(`Session.h:67-68`)이 pending IO 중의 파괴/재게시를 막는다.

### 4.12 copy-per-fanout vs move-for-single
- **팬아웃(복사)**: `Server/Private/Game/GameRoomLobby.cpp:286-290` — 로비 상태를 N개 세션에 뿌릴 때 `pSession->Send(std::vector<u8_t>(packet.begin(), packet.end()))`로 세션마다 복사본을 준다. move는 소스를 비우므로 여러 소비자에게 같은 버퍼를 넘길 수 없다.
- **단일 수신자(이동)**: `Server/Private/Game/GameRoomReplication.cpp:170` — 세션별로 새로 빌드한 스냅샷은 소비자가 1명이라 `pSession->Send(std::move(packet))`. 코드가 판단 근거(소비자 수)를 그대로 드러낸다.

### 4.13 로컬 id를 노출하지 않는 간접 매핑
`Shared/GameSim/Replication/EntityIdMap.h:56-57` `m_NetToLocal`/`m_LocalToNet` 두 `unordered_map`으로 양방향 O(1) 조회(공간을 써서 시간 확보). `EntityIdMap.h:28` `IssueNew`는 C++17 if-init 문 `if (NetEntityId existing = ToNet(entity); existing != NULL_NET_ENTITY)`로 변수 스코프를 조건에 가둔다. 로컬 `EntityID`를 와이어에 노출하지 않고 별도 `NetEntityId`로 매핑 — 포인터/핸들을 wire로 보내지 않는 원칙의 실현이자 내부 구조 은닉.

### 4.14 예측 화해 상태머신 — 로컬 이동 yaw 보호
`Client/Public/Network/Client/SnapshotApplier.h:75-84` `LocalMoveYawProtection`(bActive/netId/commandSeq/보호 스냅샷 카운트/yaw)이 상태를 들고, `SnapshotApplier.cpp:660-693`이 스냅샷마다 결정한다: `bLocalChampion`(로컬 챔피언인가) × `bSnapshotCoversProtectedCommand`(ack이 예측 명령을 커버했나, `IsCommandSeqAtLeast`) × `bServerCaughtProtectedYaw`(서버 yaw가 예측을 따라잡았나, `IsYawClose`) × `bServerActionLocked`(사망/공격 액션락) × `bServerOpposesProtectedYaw`(반바퀴 반대, `IsYawHalfTurn`) 조합으로 예측 yaw 유지 vs 서버 yaw 채택을 고른다. 유예 만료/액션락이면 `SnapshotApplier.cpp:810` `m_localMoveYawProtection = {};`로 리셋. "무조건 서버 우선"이 아니라 **명령 ack 시퀀스로 인과를 추적하는** 화해다.

### 4.15 복제하지 않는 것도 설계다 — 서버 권위 상태 vs 클라 표현 상태
`Engine/Public/ECS/Components/RenderComponent.h:10` `RenderComponent`는 `ModelRenderer* pRenderer`를 non-owning view로 들고, 주석(`:7-9`)이 "클라이언트 전용 렌더 브리지라 소유 컨테이너는 씬/스폰 서비스에 있고 **의도적으로 복제하지 않는다**"고 못박는다. 스냅샷에는 서버가 권위를 가진 상태(위치/HP/yaw)만 실리고, 렌더러 포인터 같은 클라 표현 상태는 wire에 절대 오르지 않는다 — "포인터는 직렬화 불가"(2.5)의 컴포넌트 레벨 실현이자, 복제 대상 선별 자체가 서버-권위 경계 설계라는 증거다.

### 4.16 예외 vs 반환값 — 같은 파일에서 경계로 나눈다
`Client/Private/Network/Backend/AuthClient.cpp:81-103` `ParseAuthResponse`: HTTP 성공/실패는 `resp.success` **반환값**으로(네트워크 실패는 정상 제어 흐름), JSON 파싱은 `try { json::parse(...) } catch (const json::exception& e)` **예외**로 다룬다. 파싱은 "구조 자체가 깨질 수 있는" 신뢰 불가 입력이라 예외가 적합하고, `j.value("key", default)`로 키 부재도 예외 없이 방어한다. "예외 vs 에러코드를 어디서 나누나"에 대한 파일 하나짜리 답안.

### 4.17 (안티패턴 인지) 빌드 파이프라인의 코드젠 레이스
`Server/Include/Server.vcxproj:180-182` `FlatcCodegen` 타깃이 `Inputs`/`Outputs` 선언 없이 매 빌드 `run_codegen.bat`(flatc)을 실행한다. GameSim/Client/Server 세 프로젝트에 같은 타깃이 있어 msbuild `/m` 병렬 빌드 시 같은 `*_generated.h`를 동시에 재작성하는 파일 레이스 가능성이 있다. "스키마 코드 생성을 빌드에 안전하게 엮는 법(증분 Inputs/Outputs, 병렬 레이스)"이라는 꼬리질문에, 자기 코드의 약점을 아는 상태로 답할 수 있다.

---

## ⑤ 면접 예상 Q&A

**Q1. TCP인데 왜 프레이밍(길이 헤더)이 필요하죠? recv 한 번이 메시지 하나 아닌가요?**
아니다. TCP는 바이트 스트림이라 `send`/`recv` 경계가 대응하지 않는다 — 한 메시지가 쪼개져 오거나 두 메시지가 붙어 온다. 그래서 length-prefix 헤더로 경계를 직접 만든다. Winters `FrameParser.cpp:13`은 바이트를 누적하다 헤더 미만이면 `NeedMore`, 전체 프레임 도착 시 `Complete`, magic/version/길이 이상이면 `Invalid`를 반환하고, 누적 버퍼에 상한(`kMaxBufferBytes`)을 둬 악성 길이 필드 DoS를 막는다.

**Q2. C++ struct를 그대로 memcpy해서 네트워크로 보내면 뭐가 문제죠?**
세 가지. (1) 패딩 — 컴파일러가 정렬용 바이트를 삽입해 레이아웃이 구현 의존적이고 미초기화 바이트가 새어 나간다. (2) 엔디안 — 정수 바이트 순서가 플랫폼마다 다르다. (3) 포인터 — 내 주소 공간에서만 유효한 숫자다(`std::string`/`std::vector`/vtable 포함 클래스도 마찬가지). Winters는 작고 고정 크기인 헤더만 `#pragma pack(1)` + `static_assert(sizeof == 16)`(`PacketEnvelope.h:34,46`)로 레이아웃을 못박아 수동 패킹하고, 가변 길이 payload는 FlatBuffers 스키마로 다룬다.

**Q3. 네트워크로 받은 FlatBuffers 버퍼를 바로 `GetSnapshot()`으로 읽으면 안 되나요?**
안 된다. zero-copy 접근자는 버퍼 안의 offset을 그대로 따라가므로, 버퍼가 손상/악의적이면 임의 메모리 역참조가 된다. 반드시 `flatbuffers::Verifier`로 모든 offset/길이가 버퍼 범위 안임을 먼저 증명해야 한다. Winters는 서버 수신(`PacketDispatcher.cpp:73`)과 클라 수신(`SnapshotApplier.cpp:561`) 모두 verify를 선행하고, 실패 시 **그냥 return하지 않는다** — 서버는 `FlagSuspicious`, 클라는 앞 8회 유한 로그. bare return이면 스키마 drift와 네트워크 정지를 구분 못 하는 "조용한 월드 프리즈"가 되기 때문이다.

**Q4. 스냅샷 복제와 이벤트 복제의 차이는? 왜 둘 다 필요하죠?**
스냅샷은 "지금 상태는 이것"이라는 절대 상태라 idempotent하고 손실 내성이 있다(최신 하나만 오면 됨) — 위치/HP/쿨다운에 적합. 이벤트는 "방금 스킬이 발사됐다"는 일회성 사실이라 놓치면 복구 불가 — 순서/중복 방지 seq가 필요하고 순간적 FX·타격 연출에 적합. Winters 클라는 `EventApplier.cpp:170` `IsNewerActionSeq`의 wraparound-안전 비교로 오래된/중복 이벤트 재생을 막는다.

**Q5. 빠른 우클릭 연타를 서버가 어떻게 처리하나요?**
명령 코얼레싱. `CommandIngress.cpp:74-84`에서 같은 세션의 pending Move를 새 Move로 in-place 교체해 최신 이동 의도만 남긴다 — 안 그러면 서버가 매 틱 다른 목적지로 방향을 튼다. 단 스킬 같은 비-Move 명령은 절대 병합하지 않는다(각각이 권위 사건). 드레인 후에는 `CommandIngress.cpp:98` `stable_sort`로 `(tick, session, seq)` 정렬해 결정론적 실행 순서를 보장한다.

**Q6. 클라 예측과 서버 권위가 충돌하면 언제 로컬을 믿고 언제 서버로 되감나요?**
우클릭 순간 로컬이 yaw를 예측하는데, 서버가 그 명령을 반영하기 전의 스냅샷이 예측을 덮으면 "홱 돌았다 되돌아오는" 아티팩트가 난다. Winters `SnapshotApplier.cpp:660-693`은 상태머신으로 결정한다: 서버 ack이 예측 명령 seq를 커버했나(`IsCommandSeqAtLeast`), 서버 yaw가 예측을 따라잡았나, 액션락인가, 반바퀴 반대인가. 유예 만료 시 보호를 리셋하고 서버를 채택한다. 핵심은 "무조건 서버 우선"도 "무조건 로컬 우선"도 아니라 **명령 ack로 인과를 추적**하는 것.

**Q7. 32비트 시퀀스 카운터가 오버플로해도 순서 비교를 어떻게 맞추나요?**
`a > b` 대신 `static_cast<int32_t>(a - b) > 0`. 무부호 뺄셈은 모듈로 2³²로 잘 정의되고, 그 차를 부호 있는 정수로 재해석하면 최근 절반 윈도우(2³¹) 안에서는 랩을 건너도 앞뒤가 맞는다 — TCP seq 비교와 같은 원리. Winters는 `SeqMath.h:5` `SeqGreater`로 공통화하고 `IsCommandSeqAtLeast`/`IsNewerActionSeq`가 0을 sentinel로 처리하는 변형을 쓴다.

**Q8. 비동기 IOCP에서 `WSARecv`에 넘긴 버퍼는 언제까지 살아있어야 하나요?**
IO 완료 시점까지. 함수는 `WSA_IO_PENDING`으로 즉시 리턴하지만 커널은 완료까지 버퍼와 OVERLAPPED를 계속 참조하므로, 스택 지역변수면 use-after-free다. Winters는 `IOCPCore.h:22` `IOContext`(OVERLAPPED+WSABUF+8KB 버퍼)를 `Session.h:72-73` 세션 멤버로 두고 재게시마다 `ZeroMemory`로 재사용하며, 송신 버퍼는 `Session.cpp:101` `m_sendQueue`에 move로 넣어 `OnSendComplete`가 pop할 때까지 큐가 소유한다. 소켓은 `OnDisconnect`의 `closesocket` + 소멸자 호출로 RAII 마감한다.

---

## ⑥ 흔한 오답 / 함정

1. **"TCP는 느려서 게임엔 무조건 UDP".** 진짜 문제는 속도가 아니라 head-of-line blocking이다. 로그인/로비는 TCP가 맞고, 게임플레이도 규모에 따라 TCP로 충분할 수 있다(스냅샷을 idempotent하게 설계하면 손실 비용이 낮다). "왜"를 head-of-line으로 설명하지 못하면 감점.

2. **verify를 "파싱 성공 검사"로 이해.** verify는 "이 버퍼를 역참조해도 안전하다"는 메모리 증명이다. 그리고 verify 실패를 bare return으로 삼키면 스키마 drift와 네트워크 stall이 똑같이 "조용한 프리즈"로 보여 디버깅 지옥이 된다 — 유한 trace/counter나 세션 플래그를 반드시 남긴다(gotcha 2026-07-09, `PacketDispatcher.cpp:73-78`).

3. **wire 정수를 검사 없이 `static_cast<enum>`.** 범위 밖 값이 enum에 들어가면 이후 switch/배열 인덱싱이 무너진다. 캐스트 전에 범위 clamp(`SnapshotApplier.cpp:121`), 가변 길이 vector를 고정 배열에 넣을 땐 `std::min` clamp(`:1211`)가 필수.

4. **`reinterpret_cast<Header*>(buf)`로 헤더 읽기.** 버퍼의 임의 오프셋이 헤더 정렬을 만족한다는 보장이 없어 정렬 UB이고, 동시에 strict-aliasing 위반이다. `memcpy(&hdr, buf, sizeof hdr)`가 정답(`FrameParser.cpp:21`, `PacketEnvelope.h:86`) — 최적화 후에는 memcpy 비용이 사라진다.

5. **팬아웃 broadcast에서 `std::move`.** move는 소스를 비우므로 두 번째 수신자부터 빈 버퍼를 받는다. N명에게 뿌릴 땐 각자 소유의 복사본이 필요하다(`GameRoomLobby.cpp:289` copy vs `GameRoomReplication.cpp:170` move). "무조건 move가 빠르다"는 오해.

6. **enum class를 비트플래그로 쓰려다 캐스트 지옥.** enum class는 암시적 정수 변환을 막아 `A | B`가 안 된다. 비트마스크는 underlying type만 고정한 plain enum이 실용적이고, 타입 안전이 필요한 분류값만 enum class로 간다(`PacketEnvelope.h:13` vs `:27`).

7. **overlapped IO 진행 중에 버퍼/세션 파괴.** `WSARecv`가 리턴했다고 IO가 끝난 게 아니다 — `WSA_IO_PENDING`이면 커널이 여전히 버퍼와 OVERLAPPED를 참조 중이다. 스택 버퍼를 넘기거나, pending IO가 남은 세션을 delete하면 커널발 use-after-free다. Winters는 컨텍스트를 세션 멤버(`Session.h:72-73`)로 두고 `m_pendingIoCount`/`m_bClosing`(`Session.h:67-68`)으로 파괴 시점을 지연시킨다.

---

### 요약 체크리스트 — 수신 경계에서 항상 물을 것
1. 프레임 경계는 누가 만드나? (length-prefix, NeedMore/Complete/Invalid)
2. 이 바이트를 역참조해도 되나? (Verifier 통과 전엔 no)
3. 실패하면 흔적이 남나? (유한 로그/카운터/FlagSuspicious — bare return 금지)
4. 정수 → enum, 가변 길이 → 고정 배열 경계에 clamp가 있나?
5. 이 버퍼는 누가 소유하고 언제까지 살아야 하나? (커널 IO 완료 시점, 팬아웃 수)

---

## 다른 챕터와의 연결
- [03_memory_lifetime_raii.md](03_memory_lifetime_raii.md) — RAII, 버퍼 수명, use-after-free (비동기 IO 버퍼 수명의 기초)
- [05_class_design_value_semantics.md](05_class_design_value_semantics.md) — move vs copy, move-only 타입(DetachedBuffer), 값 전달 소유권
- [10_error_handling.md](10_error_handling.md) — 예외 vs 반환값 경계, 유한 실패 로그 정책
- [13_interview_qa_bank.md](13_interview_qa_bank.md) — 통합 질문 은행 (네트워크/직렬화 문항 포함)
