Session - 2026-07-17 Actor HUD XP mask reveal + frame composite 결과

## 1. 예측 vs 실측

- 예측: frame source `202×168`을 destination `128×128`로 바꿀 때 XP source `48×120`은 X/Y 배율을 각각 상속해 `30.42×91.43`이 된다.
- 실측: 정적 계산값은 `48×(128/202)=30.42`, `120×(128/168)=91.43`이며 runtime layout의 track/fill rect도 `[252.25, 48.50, 30.42, 91.43]`으로 일치했다.
- 예측: 합성 순서는 `portrait.face → portrait.xp.arc.track → portrait.xp.arc.fill → portrait.frame`이어야 border와 level orb가 XP 위에 마지막으로 그려진다.
- 실측: JSON 파싱 후 대상 네 요소의 배열 순서가 정확히 위 순서와 일치했다. fill은 `bind=xpRatio`, `clip=maskBottomToTop`이다.
- 예측: procedural 경로가 남으면 authored texture와 fallback 사이에서 다시 회귀한다.
- 실측: `Engine`과 runtime UI 리소스에서 `DrawRingArc`, `ringArc`, arc 전용 필드 참조는 0건이다. `DrawImageVerticalReveal`은 선언·정의·호출이 각각 1건이다.
- 미실측: 사용자가 직접 빌드·디버깅하기로 한 경계에 따라 DX11 shader compile과 인게임 XP 0/50/100% 캡처는 실행하지 않았다.

## 2. 판정

코드 반영은 완료됐다. XP fill은 더 이상 width/height 또는 UV를 줄이지 않는다. 동일한 authored quad에서 `localV < 1-saturate(Progress)` 픽셀만 discard하고, texture alpha가 원호 실루엣을 결정한다. track은 항상 전체 표시되며 frame은 마지막에 합성된다. 일반 `DrawImage`와 `DrawImageCircle`은 vertex의 `revealRatio=-1` 기본값으로 reveal 분기가 비활성이라 기존 HUD 렌더에는 마스크가 적용되지 않는다.

runtime layout과 atlas manifest는 모두 JSON 파싱에 성공했고 scoped `git diff --check`도 통과했다. 최종 완료 판정은 사용자 빌드에서 HUD Layout Tuner의 `preview XP ratio`를 켜고 0%, 50%, 100%를 확인한 뒤 내린다.

## 3. 비용 갱신

- UI vertex stride는 `32→40 bytes`로 25% 증가한다. 기존 최대 65,536 vertices 기준 동적 vertex buffer 상한은 약 `2.0 MiB→2.5 MiB`다.
- XP fill 자체는 기존 일반 quad와 같은 6 vertices다. 추가 비용은 XP 픽셀의 간단한 threshold branch와 discard이며 procedural 48구간 원호의 288 vertices는 제거됐다.
- `Client/Bin/Resource/UI`는 `.gitignore` 대상이라 runtime JSON 두 파일은 일반 git diff에 나타나지 않는다. tracked Engine fallback을 같은 값으로 유지해 리소스 로드 실패 시에도 동작이 갈라지지 않게 했다.
- Engine public header가 바뀌었으므로 사용자 빌드 단계에서 루트 `UpdateLib.bat` 동기화가 필요하다.
