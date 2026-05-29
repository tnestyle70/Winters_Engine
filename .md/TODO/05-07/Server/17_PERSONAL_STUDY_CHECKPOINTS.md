# Personal Fiber Study Checkpoints

작성일: 2026-05-07  
목표: 직접 Fiber를 이해하면서 Server refactor를 진행하기 위한 학습 체크포인트.

---

## 1. Day 1: Fiber 기초를 말로 설명하기

읽을 문서:

```text
11_FIBER_CONCEPTS_SERVER_DEEP_DIVE.md
.md/guide/FIBER_LEARNING_GUIDE.md
```

직접 답하기:

```text
1. Thread 와 Fiber 의 차이는?
2. Fiber 는 왜 OS thread 를 block 하지 않는가?
3. ConvertThreadToFiber 는 왜 thread 마다 한 번인가?
4. FiberProc 는 왜 return 하면 안 되는가?
5. WaitForCounter 에서 Fiber yield 가 왜 필요한가?
```

코드에서 찾기:

```powershell
rg -n "ConvertThreadToFiber|CreateFiber|SwitchToFiber|DeleteFiber|WaitForCounter" Engine/Private Engine/Public
```

완료:

```text
[ ] Win32 Fiber API 5개 설명 가능
[ ] FiberShell 과 FiberFull 차이 설명 가능
```

---

## 2. Day 2: Server thread map 그리기

읽을 문서:

```text
12_CURRENT_SERVER_CONCURRENCY_AUDIT.md
```

직접 그리기:

```text
main thread
tick thread
accept thread
IOCP worker
JobSystem worker
```

질문:

```text
1. IOCP worker 를 Fiber 화하지 않는 이유는?
2. CGameRoom::Tick 은 어떤 mutex 를 잡는가?
3. Network worker callback 이 m_stateMutex 에서 block 될 수 있는 지점은?
4. Tick 이 job 을 기다릴 때 어떤 mutex 문제가 생길 수 있는가?
```

완료:

```text
[ ] Server thread diagram 직접 작성
[ ] m_stateMutex 사용 지점 5개 설명 가능
```

---

## 3. Day 3: CJobSystem 계약 이해

읽을 문서:

```text
13_ENGINE_FIBER_PREREQUISITES_AND_FIXES.md
.md/plan/engine/FIBER_JOBSYSTEM_STABILIZATION_PLAN.md
```

직접 답하기:

```text
1. CJobSystem 에 WINTERS_ENGINE 이 필요한 이유는?
2. Submit(job, &counter) 는 counter 를 언제 증가시키는가?
3. external submit 이 worker deque 에 push 되면 왜 위험한가?
4. Fiber context WaitForCounter 에서 help execute 하면 왜 위험한가?
5. Get_WorkerSlot 이 yield 후 달라질 수 있는 이유는?
```

완료:

```text
[ ] counter double increment 사고를 설명 가능
[ ] Chase-Lev owner push 규칙 설명 가능
```

---

## 4. Day 4: ServerEntry + Tick Fiber Shell 직접 적용

작업:

```text
Stage 1: CJobSystem export
Stage 2: CServerEntry
Stage 3: TickThread Fiber shell
```

코드 리뷰 질문:

```text
1. Initialize 실패 시 cleanup 되는가?
2. IOCP start 실패 시 CServerEntry::Shutdown 되는가?
3. room->Stop 후 Shutdown 순서가 맞는가?
4. ConvertThreadToFiber 실패 시 fallback 되는가?
5. ConvertFiberToThread 가 항상 호출되는가?
```

완료:

```text
[ ] 30s smoke
[ ] Fiber shell entered/exited log
[ ] Submit/WaitForCounter Server hit 0
```

---

## 5. Day 5: Snapshot helper split

작업:

```text
Phase_BroadcastSnapshot 를 helper 로 분리
아직 병렬화하지 않음
```

질문:

```text
1. helper split 후 packet bytes 가 같아야 하는 이유는?
2. lastAckedSeq = 0 은 기존 어느 literal 과 같은가?
3. Send 는 왜 아직 직렬인가?
4. CWorld 를 아직 job 안에서 직접 읽지 않는 이유는?
```

완료:

```text
[ ] Submit/WaitForCounter hit 0
[ ] 1 client snapshot hash 동일
[ ] snap broadcast count 동일
```

---

## 6. Day 6: Snapshot encode jobify

작업:

```text
DTO collect serial
FlatBuffer encode parallel
Send serial
```

질문:

```text
1. outputs[i] 만 job i 가 쓰면 왜 race 가 없는가?
2. inputs vector 는 왜 WaitForCounter 전까지 살아있어야 하는가?
3. job lambda 안 try/catch 가 왜 필요한가?
4. counter.Increment 를 왜 직접 하지 않는가?
```

완료:

```text
[ ] 1 client byte-identical
[ ] 8 session byte-identical
[ ] no deadlock
```

---

## 7. Day 7: Decision / Apply 설계

대상:

```text
Phase_ServerMinionAI
```

직접 작성:

```text
MinionAI read set
MinionAI write set
MinionDecision struct
Apply order
deterministic hash 항목
```

질문:

```text
1. Decision job 안에서 금지되는 API 는?
2. DamageRequest enqueue 는 왜 Apply 에서 하는가?
3. Transform.SetPosition 은 왜 Apply 에서 하는가?
4. EntityID sorted order 가 왜 중요한가?
```

완료:

```text
[ ] Decision struct 작성
[ ] old loop vs Decision/Apply state hash 계획 작성
```

---

## 8. Day 8: FiberFull nested wait 이해

읽을 문서:

```text
16_VERIFICATION_STRESS_DEBUGGING.md
.md/plan/engine/FIBER_JOB_SYSTEM_v2.md
```

직접 설명:

```text
Parent fiber 가 child counter 를 기다릴 때 무슨 일이 일어나는가?
waiter map 에 무엇이 들어가는가?
counter 완료 시 누가 waiting fiber 를 ready 로 옮기는가?
resume 은 같은 worker 에서 보장되는가?
```

완료:

```text
[ ] nested wait stress pseudo code 작성
[ ] waiter map insert/erase 흐름 설명 가능
```

---

## 9. 면접식 최종 질문

다음에 막힘없이 답하면 진짜로 잡힌 것이다.

```text
Q1. Server에서 IOCP thread와 JobSystem worker thread를 분리해야 하는 이유는?
Q2. FiberFull 에서 WaitForCounter 는 왜 help-stealing 하지 않고 yield 해야 하는가?
Q3. counter double increment 는 어떤 로그/증상으로 보이는가?
Q4. m_stateMutex 를 잡은 채 WaitForCounter 하는 것이 항상 나쁜가, 어떤 조건에서 위험한가?
Q5. CWorld read-only 병렬화가 현재 바로 확정되지 않는 이유는?
Q6. Snapshot 병렬화에서 Send 를 직렬로 유지하는 이유는?
Q7. MinionAI 를 직접 병렬화하지 않고 Decision/Apply 로 나누는 이유는?
Q8. Get_WorkerSlot 값을 yield 전후에 캐시하면 왜 위험한가?
Q9. FiberPool leak 검증에서 shutdown 전/후에 각각 무엇을 봐야 하는가?
Q10. "성능 향상"보다 "byte-identical"이 먼저인 이유는?
```

---

## 10. 네가 직접 쓸 짧은 요약 템플릿

각 Stage 끝나면 아래를 채운다.

```text
Stage:
변경 파일:
내가 이해한 개념:
가장 위험한 지점:
검증 명령:
검증 결과:
다음 Stage 전제:
```

이 기록이 쌓이면, Fiber는 더 이상 낯선 기술이 아니라 네 엔진의 일부가 된다.

