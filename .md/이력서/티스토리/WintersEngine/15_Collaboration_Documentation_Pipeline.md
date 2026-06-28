# Winters Engine 해부기 15 - Collaboration과 Documentation Pipeline

협업 문서의 본질은 "문서를 많이 쓴다"가 아니다.

Winters에서 문서와 협업 파이프라인의 본질은 다음이다.

> 여러 작업자가 같은 코드베이스를 건드릴 때 충돌을 줄이고, 변경의 의도와 검증 기준을 공유하는 것

## 문제 정의

혼자 작업하더라도 데스크탑과 노트북에서 동시에 작업하면 사실상 협업 문제와 비슷해진다.

여기에 팀 프로젝트 경험까지 들어오면 다음 문제가 반복된다.

- 같은 파일을 동시에 수정해 conflict가 난다.
- 어떤 세션이 어떤 목표를 진행 중인지 모른다.
- build 중간 산출물이 서로 충돌한다.
- Resource와 code ownership이 섞인다.
- 문서 없이 작업하면 다음 세션에서 맥락이 끊긴다.
- 설계 결정이 대화 속에만 남고 코드베이스에는 남지 않는다.

이 문제는 git만으로 해결되지 않는다.

git은 변경을 기록하지만, 왜 그렇게 바꿨는지와 다음 검증 기준까지 자동으로 설명해주지는 않는다.

## Winters의 접근

Winters는 문서와 규칙을 코드베이스 안에 둔다.

주요 문서:

- `AGENTS.md`
- `.claude/gotchas.md`
- `.md/architecture/WINTERS_CODEBASE_COMPASS.md`
- `.md/plan`
- `.md/TODO`
- `.md/이력서`

문서의 역할:

- `AGENTS.md`: cross-agent 작업 규칙
- `.claude/gotchas.md`: 반복 실수 방지 로그
- `WINTERS_CODEBASE_COMPASS.md`: architecture boundary와 장기 방향
- `.md/plan`: 설계/구현 계획
- `.md/TODO`: audit/status/report
- `.md/이력서`: 포트폴리오/블로그/이력서 원고

## 핵심 규칙

Winters의 협업 규칙은 단순 style guide가 아니다.

구조적 충돌을 줄이는 규칙이다.

대표 규칙:

- Engine은 product client code를 include하지 않는다.
- Shared/GameSim은 Engine/Client/Renderer/UI/DX type에 의존하지 않는다.
- Client는 presentation을 담당하고 gameplay truth를 만들지 않는다.
- Server는 Client visual에 의존하지 않는다.
- Runtime resource는 `Client/Bin/Resource`에서만 해석한다.
- architecture decision은 Compass에 남긴다.
- 반복 실수는 gotchas에 남긴다.

## SR_MinecraftDungeons 경험과 연결

SR_MinecraftDungeons는 팀 작업과 역할 분담 경험으로 설명하기 좋다.

핵심은 "팀 프로젝트를 했다"가 아니라 다음이다.

> Engine, Client, Server 도메인을 나누고, 누가 어떤 파일과 책임을 맡는지 정리해야 merge conflict와 기능 충돌을 줄일 수 있다는 것을 경험했다.

Winters에서는 그 경험을 더 확장해 문서 기반 작업 파이프라인으로 가져왔다.

## 여러 장비 작업과 build 충돌

데스크탑과 노트북에서 동시에 작업하면 build도 충돌할 수 있다.

예를 들어 같은 workspace에서 여러 MSBuild가 동시에 돌면 PCH/PDB 잠금이나 중간 산출물 충돌이 생길 수 있다.

이런 경험은 사소해 보이지만 실제 협업에서도 중요하다.

해결 방향:

- 작업 전 `git status -sb`
- pull/push 시점 명확히 하기
- 역할별 수정 파일 분리
- build session 겹침 주의
- 대용량 Resource와 code repository 분리
- 결과 report 작성

## 면접에서 말할 포인트

협업 경험은 "팀원과 같이 만들었습니다"로 끝나면 약하다.

더 좋은 설명은 이것이다.

> 충돌이 날 수밖에 없는 작업을 파일 ownership, architecture compass, gotcha log, verification report로 나누어 관리했다.

이 설명은 실제 팀에서 중요한 태도를 보여준다.

## 이 글을 이력서 문장으로 압축하면

> 데스크탑/노트북 및 팀 작업 환경에서 파일 ownership, architecture compass, gotcha log, 검증 report를 통해 충돌을 줄이는 문서 기반 협업 파이프라인을 운영했습니다.

## 시리즈 마무리

Winters Engine은 단순히 여러 기능을 붙인 프로젝트가 아니다.

이 시리즈 전체의 핵심은 다음 문장으로 압축된다.

> 기존 엔진 구조의 한계를 문제로 정의하고, Engine/Product Client 분리, RHI, RenderWorldSnapshot, 서버 권위 GameSim, DataDriven Definition Pack, Asset/World/Verification 파이프라인으로 해결하려 한 C++ 자체 엔진 프로젝트

이 관점으로 블로그와 이력서를 연결하면, 단순 구현자가 아니라 문제를 구조로 정의하고 검증 루프로 닫을 줄 아는 개발자로 보일 수 있다.

