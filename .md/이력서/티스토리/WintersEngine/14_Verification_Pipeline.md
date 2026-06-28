# Winters Engine 해부기 14 - Verification Pipeline

검증 파이프라인의 본질은 "빌드가 된다"가 아니다.

Winters에서 검증의 본질은 다음이다.

> 구조 변경이 runtime/data/client/server 경계를 깨지 않았는지 반복 가능한 명령으로 확인하는 것

## 문제 정의

엔진 리팩터링은 위험하다.

RHI 경계를 바꾸면 Client 렌더링이 깨질 수 있다. DataDriven 전환을 하면 gameplay 수치가 달라질 수 있다. Server authority를 바꾸면 snapshot이 어긋날 수 있다. AI policy를 바꾸면 bot이 command를 잘못 만들 수 있다.

이때 "한 번 실행해봤는데 괜찮았다"는 검증이 아니다.

필요한 것은 반복 가능한 루프다.

```text
변경
-> 데이터 freshness 확인
-> legacy audit
-> goal status
-> visual parity
-> GameSim/Server/Client/SimLab build
-> deterministic regression
-> diff check
-> report
```

## Winters의 검증 스크립트

대표 스크립트:

- `Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1`
- `Tools/LoLData/Collect-LoLLegacyDataAudit.ps1`
- `Tools/LoLData/Get-LoLDataDrivenGoalStatus.ps1`
- `Tools/LoLData/Export-LoLChampionVisualTimingSeed.ps1`
- `Tools/SimLab/SimLab.vcxproj`

`Verify-LoLDataDrivenPipeline.ps1`은 다음을 묶는다.

```text
Definition pack freshness
Legacy ownership audit
Data-driven goal status
Raw product asset path audit
Client visual timing parity
Build GameSim
Build Server
Build Client
Build SimLab
SimLab deterministic regression
Whitespace validation
```

## 왜 DataDriven에 검증이 필요한가

DataDriven 전환은 단순 파일 이동이 아니다.

값의 소유권이 바뀐다.

- C++ hardcode에서 definition pack으로 이동
- gameplay와 visual timing 분리
- ServerPrivate와 ClientPublic 분리
- legacy table reader count 감소

이때 count를 추적하지 않으면 "거의 다 옮겼다"와 "정말 runtime reader가 없어졌다"를 구분할 수 없다.

그래서 goal status가 필요하다.

## Build만으로 부족한 이유

Build는 syntax와 link 문제를 잡는다.

하지만 다음 문제는 build만으로 잡히지 않을 수 있다.

- visual timing mismatch
- gameplay value ownership leak
- deterministic simulation regression
- raw asset path leak
- generated pack stale state
- whitespace/encoding issue

따라서 Winters는 build 앞뒤로 audit과 parity check를 둔다.

## 면접에서 말할 포인트

많은 포트폴리오는 "구현했습니다"에서 끝난다.

Winters는 "이 변경이 어디까지 반영됐고, 어떤 legacy count가 줄었고, 어떤 검증이 통과했는지"를 추적한다.

이건 실무적인 태도다. 대형 리팩터링은 계획보다 검증 루프가 더 중요하기 때문이다.

## 이 글을 이력서 문장으로 압축하면

> DataDriven/RHI/GameSim 리팩터링마다 definition freshness, legacy audit, visual parity, multi-project build, SimLab deterministic regression을 묶은 검증 루프를 운영했습니다.

## 다음 글

다음 글에서는 혼자 또는 여러 장비에서 작업하더라도 협업처럼 충돌을 줄이는 문서/작업 파이프라인을 설명한다.

