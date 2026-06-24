# Work Packet: IOCP / RHI Thread Ownership Fix

## Metadata

- ID: `2026-06-24_iocp_rhi_thread_ownership_fix`
- Status: `Active`
- Owner: Desktop
- Branch: `main`
- Base: `origin/main`
- Started: `2026-06-24`
- Report: `.md/plan/2026-06-24_IOCP_RHI_THREAD_OWNERSHIP_FIX_HANDOFF_PLAN.md`
- Harness report: `.md/build/2026-06-24_IOCP_RHI_THREAD_OWNERSHIP_FIX_HARNESS_REPORT.md`

Status is `Active` because the work is documented locally but not yet committed/pushed. Change to `Handoff` only after push.

## Owned Paths

- `Server/Private/Network/IOCPCore.cpp`
- `Server/Private/Network/Session_Manager.cpp`
- `Server/Public/Network/Session_Manager.h`
- `Client/Private/Scene/Loader.cpp`
- `Client/Public/Scene/Loader.h`
- `Client/Private/Scene/Scene_MatchLoading.cpp`
- `Client/Private/Scene/Scene_Loading.cpp`
- `.md/plan/2026-06-24_IOCP_RHI_THREAD_OWNERSHIP_FIX_HANDOFF_PLAN.md`
- `.md/build/2026-06-24_IOCP_RHI_THREAD_OWNERSHIP_FIX_HARNESS_REPORT.md`
- `.md/collab/work-packets/2026-06-24_iocp_rhi_thread_ownership_fix.md`

## Read-Only Paths

- `Engine/**`
- `Shared/**`
- `Client/Private/Scene/Scene_InGame*`
- `Client/Include/Client.vcxproj`
- `Server/Include/Server.vcxproj`
- `EngineSDK/inc/**`

## Validation

- `git pull --rebase --autostash`
  - `Already up to date.`
- Targeted `git diff --check`
  - PASS, LF/CRLF warnings only
- `Server\Include\Server.vcxproj` Debug x64 MSBuild
  - PASS, error 0
- Server runtime smoke
  - PASS, IOCP disconnect/delete crash not reproduced
- `Client\Include\Client.vcxproj` Debug x64 MSBuild
  - PASS, error 0
- Client MatchLoading smoke
  - PASS, Visual C++ RHI assertion dialog not detected
- `Tools\Harness\Run-S17RhiValidation.ps1`
  - partial PASS: diff/audit steps passed
  - FAIL at CMake/Ninja link with `LNK1201` writing `Engine\Bin\Debug\WintersEngine.pdb`

## Handoff Notes

- IOCP fix separates heap-owned `Accept` context from session-owned `Recv/Send` context.
- MatchLoading fix keeps InGame resource preload RHI-touching steps on the main/render owner thread.
- Do not silence `CRHIResourceTable` assert. If it returns, capture call stack and move the offending RHI call to the owner-thread load path.
- Do not delete unrelated dirty files or generated artifacts when picking up this packet.
