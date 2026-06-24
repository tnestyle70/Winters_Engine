# Session - 2026-06-24 IOCP / RHI Thread Ownership Fix Handoff

목적: IOCP worker crash와 MatchLoading RHI thread assert를 같은 관점, 즉 "누가 소유한 데이터를 어느 thread에서 만지는가"로 정리하고, 다른 장비가 `git pull` 후 같은 맥락으로 이어갈 수 있게 남긴다.

## 0. 협업 동기화

- 작성 시각: `2026-06-24 05:48 +09:00`
- 작업 repo: `C:\Users\user\Desktop\Winters`
- branch: `main`
- sync 결과: `git pull --rebase --autostash` -> `Already up to date.`
- 협업 기준 문서:
  - `.md/collab/ACTIVE_WORK_PACKETS.md`
  - `.md/collab/GIT_SYNC_RULES.md`
  - `.md/collab/HARNESS_RULES.md`
  - `.md/collab/OWNERSHIP_MATRIX.md`
- 이번 work packet:
  - `.md/collab/work-packets/2026-06-24_iocp_rhi_thread_ownership_fix.md`
- 공용 harness report:
  - `.md/build/2026-06-24_IOCP_RHI_THREAD_OWNERSHIP_FIX_HARNESS_REPORT.md`

현재 작업은 아직 commit/push되지 않은 local working tree 기준이다. 다른 장비로 넘기려면 commit/push 후 `ACTIVE_WORK_PACKETS.md`의 상태를 `Handoff`로 바꾼다.

## 1. 반영해야 하는 코드 / 반영된 변경

### 1.1 Server IOCP disconnect ownership

반영 파일:

- `Server/Private/Network/IOCPCore.cpp`
- `Server/Private/Network/Session_Manager.cpp`
- `Server/Public/Network/Session_Manager.h`

반영 내용:

- `CIOCPCore::WorkerLoop`에서 failed completion 처리 시 `Accept`와 `Recv/Send`의 context 소유권을 분리했다.
- `Accept` context는 IOCP가 heap으로 만든 context라 실패 시 `closesocket` 후 `delete ctx`가 맞다.
- `Recv/Send` context는 session 내부 멤버 context라 worker가 `delete ctx`하면 안 된다.
- `Recv/Send` 실패와 zero-byte recv는 `CSession_Manager::OnIoDisconnect`로 보내고, 여기서 `CompletePendingIo()` 후 정상 disconnect/reap 흐름을 탄다.

### 1.2 MatchLoading InGame resource load thread ownership

반영 파일:

- `Client/Public/Scene/Loader.h`
- `Client/Private/Scene/Loader.cpp`
- `Client/Private/Scene/Scene_MatchLoading.cpp`
- `Client/Private/Scene/Scene_Loading.cpp`

반영 내용:

- `CLoader::Create(eSceneID::InGame)`은 더 이상 JobSystem worker에 InGame preload를 submit하지 않는다.
- InGame preload는 `PrepareMainThreadInGameLoad()`가 `LoadStep` queue를 만들고, scene update에서 `TickMainThreadLoad()`가 한 step씩 처리한다.
- `Scene_MatchLoading::OnUpdate`와 `Scene_Loading::OnUpdate`가 loader 완료 전 `TickMainThreadLoad()`를 호출한다.
- map model, FX directory, champion model/texture preload가 main/render owner thread 흐름에서 처리되므로 `CRHIResourceTable` owner-thread assert를 피한다.

## 2. 핵심 원인

### 2.1 IOCP crash의 본질

나눌 수 없는 원인은 "context 소유권을 잘못 판단해서 session 소유 메모리를 worker가 delete했다"이다.

- IOCP completion은 완료 사실을 worker thread에 전달할 뿐, context 메모리 소유권을 자동으로 통일해주지 않는다.
- `Accept` context와 `Recv/Send` context는 생명주기가 다르다.
- failed completion에서 모든 context를 같은 방식으로 `delete`하면, heap object와 embedded member object가 같은 취급을 받는다.
- 그 결과 `operator delete` 호출 stack에서 abort/crash가 발생한다.

### 2.2 MatchLoading RHI assert의 본질

나눌 수 없는 원인은 "RHI resource table은 render owner thread 전용인데, InGame preload가 worker thread에서 GPU resource path를 열었다"이다.

- assert 위치: `CRHIResourceTable.h:129`
- assert 조건: `std::this_thread::get_id() == m_RenderThreadId`
- worker path: `CLoader::Create(InGame)` -> `JobSystem::Submit` -> `RunLoadJob` -> `Ready_For_InGame` -> preload -> `ResourceCache/Model/Texture` -> RHI texture/resource creation
- `CRHIResourceTable`의 invariant는 맞다. 문제는 assert가 아니라 호출 thread가 틀린 것이다.
- 중단 버튼 후 `abort has been called()`가 보이는 것은 Visual C++ assertion abort의 후속 증상이다.

## 3. 원자 단위 핵심 개념

- Thread: 코드는 항상 하나의 실행 흐름 위에서 돈다. owner-thread 전용 객체는 그 thread 밖에서 읽기/쓰기/등록을 하면 invariant가 깨진다.
- IOCP: OS가 완료 이벤트를 queue에 넣어주고 worker가 꺼내는 구조다. 완료 이벤트는 "누가 메모리를 해제해야 하는가"를 대신 결정하지 않는다.
- Context ownership: `new`로 만든 object만 그 소유자가 `delete`한다. session 내부에 박힌 context는 session 생명주기와 함께 간다.
- RHI resource table: GPU resource handle/table은 renderer 쪽 전역 상태에 가깝다. 병렬 preload가 가능하려면 decode/parse 단계와 GPU 등록 단계를 분리해야 한다.
- Loading: "로딩을 비동기로 한다"와 "모든 로딩 단계를 worker에서 한다"는 다르다. 지금 단계에서 본질은 RHI-touch step을 main/render owner thread로 되돌리는 것이다.

## 4. 검증

### 4.1 Sync

```powershell
git pull --rebase --autostash
```

결과:

- `Already up to date.`

### 4.2 Targeted diff check

```powershell
git diff --check -- Client/Private/Scene/Loader.cpp Client/Public/Scene/Loader.h Client/Private/Scene/Scene_MatchLoading.cpp Client/Private/Scene/Scene_Loading.cpp Server/Private/Network/IOCPCore.cpp Server/Private/Network/Session_Manager.cpp Server/Public/Network/Session_Manager.h
```

결과:

- PASS
- LF -> CRLF warning만 출력됨

### 4.3 Server build and smoke

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

결과:

- PASS
- error 0

Runtime smoke:

```powershell
Server\Bin\Debug\WintersServer.exe --smoke-seconds=6
```

결과:

- server process completed without stderr crash
- 20 TCP connect/disconnect smoke에서 IOCP failed/closed path delete crash 재현 안 됨

### 4.4 Client build and MatchLoading smoke

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

결과:

- PASS
- error 0
- 기존 C4828/C4251/C4275 계열 warning은 남아 있음

Runtime smoke:

```powershell
Client\Bin\Debug\WintersGame.exe --banpick-smoke --smoke-start --smoke-champion=EZREAL --fps=60 --no-vsync
```

결과:

- automated start 후 약 18초 생존 확인
- `runtime_dialog_detected=False`
- `CRHIResourceTable must be accessed from the render thread` dialog 재현 안 됨

### 4.5 Shared S17 harness

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Harness\Run-S17RhiValidation.ps1 -ReportPath .md\build\2026-06-24_IOCP_RHI_THREAD_OWNERSHIP_FIX_HARNESS_REPORT.md
```

결과:

- `git diff --check`: PASS
- `Client/Public` + `Shared` concrete graphics audit: PASS
- focused common RHI public header audit: PASS
- CMake/Ninja S17 targets: FAIL

실패 원인:

- `LINK : fatal error LNK1201`
- 대상: `C:\Users\user\Desktop\Winters\Engine\Bin\Debug\WintersEngine.pdb`
- 성격: local PDB write/link failure. 이번 Client loader/server IOCP 수정과 직접 관련된 compile error는 아니다.

다음 장비에서 full harness를 재시도할 때는 Visual Studio/debuggee가 `WintersEngine.pdb`를 잡고 있지 않은지 먼저 확인한다.

## 5. 다른 장비에서 이어갈 때

1. `git status --short --branch`로 dirty 상태를 먼저 본다.
2. 이 문서와 work packet을 읽는다.
3. 같은 수정 파일을 이어 만질 경우 `Client/Public/Scene/Loader.h`, `Server/Public/Network/Session_Manager.h`가 public header임을 감안한다.
4. RHI assert가 다시 뜨면 assert를 끄지 말고 `Retry`로 call stack을 잡는다.
5. 새 RHI worker 호출이 보이면 "decode/parse는 worker, GPU resource registration은 owner thread"로 분리한다.
6. IOCP 쪽 새 crash가 보이면 먼저 context가 heap-owned인지 session-owned인지 확인한다.

## 6. 남은 리스크

- 공용 S17 full harness는 PDB write 실패 때문에 끝까지 통과하지 못했다.
- 정상 `WintersGame.exe` no-args smoke는 full harness가 CMake 단계에서 멈춰 실행되지 않았다.
- 이미 working tree에 존재하던 unrelated dirty files는 건드리거나 되돌리지 않았다.
