# Git Sync Rules

목적: 노트북/데스크탑이 같은 repo를 번갈아 쓰면서도 history와 worktree를 예측 가능하게 유지한다.

## 작업 시작

```powershell
git status --short --branch
git fetch origin
git switch main
git pull --rebase origin main
```

작업 브랜치를 쓸 때:

```powershell
git switch -c codex/<area>-<short-task>
```

권장 prefix:

- `codex/rhi-laptop-*`
- `codex/rhi-desktop-*`
- `codex/data-*`
- `codex/server-*`

## 작업 중

- work packet의 owned paths만 수정한다.
- read-only paths가 필요해지면 `ACTIVE_WORK_PACKETS.md`를 먼저 갱신한다.
- public header, project file, SDK generated file은 작은 commit으로 묶는다.
- 보고서는 새 파일로 만든다.

## Push 전

```powershell
git fetch origin
git rebase origin/main
Tools/Harness/Run-S17RhiValidation.ps1
git status --short
git diff --check
```

통과 후:

```powershell
git add <owned paths>
git commit -m "<area>: <summary>"
git push origin HEAD
```

## 금지

- dirty worktree에서 무조건 `git pull`하지 않는다.
- 두 장비에서 `main`의 같은 파일을 동시에 수정하지 않는다.
- conflict 해결을 위해 `git reset --hard`, `git checkout -- <file>`을 사용하지 않는다.
- 빌드 산출물, 대용량 resource zip, 로컬 캐시를 범위 확인 없이 stage하지 않는다.

## Handoff

한 장비에서 작업을 넘길 때는 다음을 남긴다.

- pushed branch/commit
- touched files
- validation command/result
- report path
- 다음 장비가 만져도 되는 paths
