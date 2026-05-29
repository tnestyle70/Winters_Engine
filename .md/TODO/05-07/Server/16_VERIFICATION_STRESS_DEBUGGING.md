# Verification Stress Debugging For Server Fiber

작성일: 2026-05-07  
목표: Server Fiber refactor 의 검증, stress, deadlock 디버깅 절차를 정리한다.

---

## 1. 검증 철학

Fiber 작업의 검증은 세 단계다.

```text
1. 컴파일 검증
2. 실행 생존 검증
3. 결정성 검증
```

성능 측정은 그 다음이다.

```text
정확하지 않은 빠른 서버 = 실패
느리지만 deterministic 한 서버 = 다음 단계 가능
```

---

## 2. Build gates

```powershell
msbuild Engine\Include\Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
msbuild Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
msbuild Server\Include\Server.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

합격:

```text
Engine Debug OK
Server Debug OK
Server Release OK
WintersEngine.dll copied to Server/Bin/*
```

---

## 3. Grep gates

### Submit / WaitForCounter 위치

```powershell
rg -n "Submit\(|WaitForCounter\(" Server/Public Server/Private
```

기대:

```text
Stage 0~5: 0 또는 정확히 계획된 helper 1곳
Stage 6: Snapshot encode helper 안 Submit/WaitForCounter 1곳
```

### Counter 수동 Increment 금지

```powershell
rg -n "Increment\(" Server/Public Server/Private
```

검토:

```text
CJobCounter 를 Submit 과 같이 쓰는 곳에서 수동 Increment 금지
```

### Get_WorkerSlot 금지

```powershell
rg -n "Get_WorkerSlot|Get_WorkerIdx" Server/Public Server/Private
```

기대:

```text
0 hit
```

### IOCP 변경 금지

```powershell
git diff -- Server/Private/Network/IOCPCore.cpp Server/Public/Network/IOCPCore.h
```

기대:

```text
empty diff
```

### job 안 world write 금지

```powershell
rg -n "AddComponent|RemoveComponent|DestroyEntity|CreateEntity|IssueNew|Bind\(|Unbind\(" Server/Private/Game
```

검토:

```text
병렬 job lambda 안에 위 write API 가 있으면 실패
```

---

## 4. 30s smoke

```powershell
Server\Bin\Debug\WintersServer.exe --smoke-seconds=30
```

기대 로그:

```text
[ServerEntry] Initialize OK workers=N mode=...
[Tick] Fiber shell entered
[IOCPCore] listening on port 9000
[Server] Smoke mode: running for 30 seconds.
[Tick] count=900 maxJitter=...
[Tick] Fiber shell exited
[ServerEntry] Shutdown complete
```

합격:

```text
exit code 0
crash 0
deadlock 0
tick count 900 근처
```

---

## 5. Snapshot byte-identical

목표:

```text
직렬 snapshot 과 병렬 snapshot 의 packet bytes 가 동일해야 한다.
```

방법 A. Hash log 추가

개발용으로 snapshot packet 생성 직후 hash를 찍는다.

```cpp
uint64_t HashBytes(const uint8_t* data, size_t size);
std::cout << "[SnapshotHash] tick=" << tc.tickIndex
          << " sid=" << sid
          << " hash=" << hash
          << " size=" << size << "\n";
```

비교:

```powershell
Server\Bin\Debug\WintersServer.exe --smoke-seconds=5 > serial.log
Server\Bin\Debug\WintersServer.exe --smoke-seconds=5 > parallel.log

rg "SnapshotHash" serial.log > serial_hash.log
rg "SnapshotHash" parallel.log > parallel_hash.log
fc serial_hash.log parallel_hash.log
```

방법 B. Test hook

`Phase_BroadcastSnapshot` 안에서 debug build일 때:

```text
serial build result
parallel build result
compare before Send
assert equal
```

초기에는 방법 B가 가장 강하다.

---

## 6. 8 session mock

목표:

```text
session N 개에서 snapshot encode 병렬화가 deadlock 없이 deterministic 한지 확인
```

방법:

```text
Option A. 실제 client 8개 hidden 실행
Option B. Server test hook 으로 fake session ids + fake send sink
Option C. SnapshotBuilder 단위 테스트
```

권장:

```text
초기에는 Option B
네트워크 IO 를 제거하고 encode/determinism 만 검증
```

Fake send sink:

```cpp
struct SentPacketRecord
{
    u32_t sessionId;
    u64_t tick;
    u64_t hash;
    u32_t size;
};
```

합격:

```text
serial matrix[tick][sid] == parallel matrix[tick][sid]
```

---

## 7. Fan-out job stress

목표:

```text
Submit / counter / WaitForCounter 기본 정확성 검증
```

Pseudo:

```cpp
CJobSystem* pJob = CServerEntry::Get_JobSystem();
std::atomic<u32_t> done{0};
CJobCounter counter;

for (u32_t i = 0; i < 10000; ++i)
{
    pJob->Submit([&]()
    {
        done.fetch_add(1, std::memory_order_relaxed);
    }, &counter);
}

pJob->WaitForCounter(&counter);
assert(done.load() == 10000);
```

주의:

```text
counter.Increment(10000) 하지 않는다.
```

합격:

```text
done == jobCount
WaitForCounter return
deadlock 0
```

---

## 8. Nested wait stress

목표:

```text
FiberFull 의 진짜 가치 검증
```

Pseudo:

```cpp
for parent in 0..N:
  Submit ParentJob

ParentJob:
  Submit child A/B/C
  WaitForCounter(childCounter)
  mark parent complete
```

ThreadOnly help-stealing에서도 통과할 수 있지만, FiberFull에서는 다음도 봐야 한다.

```text
worker thread block 0
waiting fiber count 증가/감소 정상
ready fiber resume 정상
waiter map leak 0
```

필수 counters:

```text
Fiber::YieldWait
Fiber::ResumeReady
Fiber::WaitMapInsert
Fiber::WaitMapErase
Fiber::PoolAcquire
Fiber::PoolRelease
```

합격:

```text
all parent complete
all child complete
wait map empty
pool acquired == released before shutdown
created == deleted after shutdown
```

---

## 9. Deadlock debugging

Deadlock 의심 시 순서:

### 1. counter 값 확인

```text
counter.Load()
submitted count
completed count
```

가장 흔한 원인:

```text
counter double increment
job throw 로 decrement 누락
job 이 queue 에 들어가지 않음
```

### 2. Worker alive 확인

로그:

```text
WorkerLoop heartbeat
TryExecuteOneJob hit/miss
GlobalQueue size
ReadyFiberQueue size
```

### 3. Waiter map 확인

```text
counter pointer
target
waiting fiber indices
counter current count
```

### 4. mutex 재진입 확인

```powershell
rg -n "m_stateMutex|m_sendMutex|m_mutex" Server/Public Server/Private
```

job lambda 안에서 같은 mutex를 잡는지 본다.

### 5. Fiber return target 확인

Fiber가 resume되는 worker의 root fiber handle이 현재 return target인지 확인한다.

문제 패턴:

```text
fiber A 안에서 fiber B 를 직접 switch
B 가 root 로 return
A 로 돌아오지 않음
```

해결:

```text
fiber context WaitForCounter 에서 help execute 금지
```

---

## 10. Logging format

Fiber 로그는 너무 많아지기 쉽다.  
항상 counter 형식으로 먼저 본다.

권장:

```text
[Fiber] workers=N mode=FiberFull pool=128
[FiberCounter] submit=10000 complete=10000 wait=1
[FiberWait] insert=123 erase=123 ready=123 resume=123
[FiberPool] created=128 deleted=0 acquired=10000 released=10000 free=128
```

per job 로그는 stress가 작을 때만 켠다.

---

## 11. Performance measurement

정확성 통과 후 측정한다.

측정 항목:

```text
Tick maxJitter
Phase_BroadcastSnapshot ms
Snapshot encode total ms
WaitForCounter wait time
jobs submitted per tick
worker utilization
```

비교:

```text
ThreadOnly serial
ThreadOnly jobified
FiberShell jobified
FiberFull jobified
```

FiberFull이 항상 빠른 것은 아니다.  
작업량이 작으면 overhead가 더 크다.

threshold:

```text
inputs.size() <= 1: serial
small DTO count: serial
large sessions/entities: parallel
```

---

## 12. Final gate checklist

```text
[ ] Engine Debug build
[ ] Server Debug build
[ ] Server Release build
[ ] grep Submit/WaitForCounter expected only
[ ] grep Get_WorkerSlot 0 in Server
[ ] counter double increment 0
[ ] IOCP diff 0
[ ] 30s smoke
[ ] 1 client byte-identical
[ ] 8 session byte-identical
[ ] fan-out stress
[ ] nested wait stress
[ ] pool leak 0
[ ] waiter map leak 0
[ ] tick jitter acceptable
```

