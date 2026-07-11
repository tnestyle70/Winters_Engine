# 무기 카드 02 — Starcraft 모작 (WinAPI/GDI, C++20)

> 개인 프로젝트 (git author 단일) · 2025.12.19 ~ 2026.01.20 커밋 (파일 수정 ~01.25) · 저수준 기본기 증명 무기
> 저장소: https://github.com/tnestyle70/Starcraft · 데모 영상: `C:\Users\user\Desktop\스타크래프트\스타 mp4.mp4` (1.23GB, 내용 확인 필요)
> 실행 파일 존재: `Debug/Starcraft.exe` (2026-01-23 빌드)

## 한 줄

엔진·라이브러리 없이 WinAPI/GDI만으로 RTS 코어(경로탐색·군집이동·전장의안개·명령큐·생산/자원·맵에디터)를 구현 — 약 5.4만 줄(서드파티 제외), 3종족 유닛 32종·건물 40종.

## 핵심 시스템 (전부 코드 근거 확인됨)

1. **A\* + 스티어링 결합 이동**: Octile 휴리스틱·8방향·대각 코너컷 방지·타일 비용 가중 A\*(`CNavMgr.cpp:124-268`) 위에 Separation/Alignment/Cohesion 플로킹 + 벽 회피(`CSteeringMgr.h:44-107`) — **"전역 경로탐색과 지역 조향의 역할 분리"**를 설명할 수 있는 구현
2. **명령 큐 아키텍처**: `Order` 구조체(타입+경로+타깃) + `deque<Order>` shift-큐, `Commandable` 인터페이스로 유닛·건물이 동일한 커맨드카드 계약 공유 — 유닛 상태 9종 FSM
3. **전장의 안개 (GDI 최적화)**: UNKNOWN/EXPLORED/VISIBLE 3상태, dirty-flag + 캐시 DC로 0.5초 주기만 재계산, EXPLORED 반투명 (`CFogMgr.h`)
4. **순수 GDI 합성 파이프라인**: 더블버퍼링(Back DC→BitBlt) + `SRCAND`/`SRCPAINT` 마스크 비트연산 + AlphaBlend 3단 합성으로 팀컬러 틴트/클로킹을 셰이더 없이 구현 (`CBmpMgr.cpp:188-274`)
5. **인게임 맵 에디터 + 바이너리 파이프라인**: EDIT 씬에서 타일 편집 → Win32 CreateFile/WriteFile 바이너리 `Tile.dat` → 같은 데이터로 내비 그리드 생성 (`CTileMgr.cpp:524-625` → `CNavMgr::BuildFromTile`)
6. 미니맵(안개 연동·클릭 카메라), 드래그 박스 다중 선택, 자원/생산큐/업그레이드, 충돌(유닛 겹침 분리·벽 침투 방지), FMOD 사운드

## 면접 스토리

- "셰이더 없이 GDI 비트연산으로 알파 합성을 만들면서 렌더링 파이프라인의 본질(합성·버퍼링)을 바닥에서 이해했다"
- "A\*와 스티어링을 분리한 이유" — 이후 Winters NavigationSystem/Pathfinder 설계로 이어진 계보
- 128×128 타일 + 유닛 다수에서 안개·렌더를 dirty-flag로 버틴 최적화

## CONFIRM_NEEDED

- [ ] 개발 기간 공식 표기 (git: 2025-12-19~2026-01-20)
- [ ] 데모 영상 커버 범위
- [ ] 현재 환경 재빌드 확인 (v145 툴셋)
