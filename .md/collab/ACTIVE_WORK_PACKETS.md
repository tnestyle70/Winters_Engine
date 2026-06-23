# Active Work Packets

목적: 노트북/데스크탑을 서로 다른 작업자로 보고, 동시에 만질 파일과 merge 순서를 명확히 한다.

## 상태 값

- `Reserved`: 작업자가 범위를 예약했지만 아직 코드 수정 전.
- `Active`: 코드/문서 수정 진행 중.
- `Handoff`: push 완료, 다른 장비가 pull/rebase 후 이어받을 수 있음.
- `Merged`: 통합 브랜치에 반영 완료.
- `Blocked`: 범위 충돌, 빌드 실패, 리소스 누락 등으로 중지.

## 운영 규칙

- 새 작업 시작 전 이 문서에 work packet을 추가하거나 기존 packet 상태를 갱신한다.
- 한 work packet은 owner device, branch, owned paths, read-only paths, validation harness, report path를 반드시 기록한다.
- `EngineSDK/inc/**`, `.vcxproj`, `.filters`, public header는 충돌 위험 파일로 보고 work packet 하나만 소유한다.
- 같은 파일을 두 장비에서 동시에 수정해야 하면 먼저 `Handoff` 상태로 넘기고 상대 장비가 `git pull --rebase`를 완료한 뒤 작업한다.
- 완료 보고서는 `.md/build/YYYY-MM-DD_*.md`에 새 파일로 남긴다. 기존 보고서를 덮어쓰지 않는다.

## 현재 Packet

| ID | 상태 | Owner | Branch | 범위 | Report |
| --- | --- | --- | --- | --- | --- |
| `2026-06-24_s17_collab_harness_bootstrap` | `Handoff` | Desktop | `main` | `.md/collab/**`, `Tools/Harness/Run-S17RhiValidation.ps1` | `.md/build/2026-06-24_COLLAB_HARNESS_BOOTSTRAP_REPORT.md` |

## Packet Template

```text
ID:
상태:
Owner:
Branch:
Base:
Owned paths:
Read-only paths:
Validation:
Report:
Handoff notes:
```
