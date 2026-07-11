# 무기 카드 04 — Winters (자체 C++20/DX11 엔진 + LoL 구조 MOBA)

> 개인 프로젝트 · 집중 개발 2026.04~진행 중 · 메인 무기
> 근거 저장소: `C:\Users\user\Desktop\Winters` (GitHub 공개 여부 CONFIRM_NEEDED)

## 한 줄

C++20 자체 DX11 엔진 위에 LoL 구조의 MOBA를 **서버 권위(authoritative) 멀티플레이**로 구현한 1인 프로젝트 — ECS 시뮬레이션, IOCP 서버, 자체 에셋 포맷/툴 체인 포함.

## 검증된 수치 (전부 코드/문서 근거 있음 — `.md/interview/resume.md` §4)

| 주장 | 근거 |
|---|---|
| 프레임 17.8ms → 9ms (~110fps): 자체 CPU 프로파일러로 스키닝 갱신 16ms(프레임의 90%) 병목 계측 확정 후 제거 | `Engine/Private/Core/Profiler/` |
| FBX/GLB 27종 → `.wmesh` 전수 변환 FAIL 0 | `Tools/WintersAssetConverter/` |
| Irelia FBX 60MB → 1.2MB (~50배), POD static_assert 고정 zero-copy GPU 업로드 | `Engine/Public/AssetFormat/Mesh/` |
| 서버 시뮬레이션 챔피언 15종 (Annie~Zed, 폴더 실측) | `Shared/GameSim/Champions/` |
| 300틱 결정성 해시 검증 — 108파일 리팩터링 전후 해시 동일 (BB6A67502987351F) | `Tools/SimLab/main.cpp` |
| 30Hz 고정 틱 서버, IOCP 워커 4 + 게임 틱 1 스레드 | `Server/Public/Security/LagCompensation.h:12`, `Server/Private/Network/IOCPCore.cpp` |
| 전 코드베이스 감사 65건 / HIGH 18건 확정 (2026-07-09) | `.md/architecture/WINTERS_DEPENDENCY_MAP.md` |

## 핵심 시스템 (면접 하이라이트)

1. **서버 권위 5단 파이프라인**: `Client Input → GameCommand → Server GameSim → FlatBuffers Snapshot/Event → Client Visual`. wire/sim identity 분리(GameCommandWire/GameCommand), 봇 AI도 커맨드 생산자로 통일, 랙 컴펜세이션, 클라 예측 보호(ProtectLocalMoveYaw, 최대 12스냅샷).
2. **ECS**: 세대(generation) 핸들 sparse-set 저장 + Phase 기반 시스템 스케줄러(12시스템), Decision/Apply 2-pass 병렬화.
3. **RHI 추상화**: DX11 런타임 + DX12 실험 경로, `--rhi=` 플래그 전환.
4. **자체 에셋 파이프라인**: `.wmesh/.wskel/.wanim/.wmat` 바이너리 + Assimp 기반 오프라인 컨버터 (임포트→쿡 구조).
5. **부속**: Go 마이크로서비스 백엔드(auth/matchmaking/profile/leaderboard — PostgreSQL/Redis/JWT), 리플레이, ImGui 툴(이펙트 튜너/맵 에디터/F3 프로파일러), 경계 lint(Check-SharedBoundary.ps1).
6. **엔진 이해의 깊이 증명**: UE 5.7.4 풀소스 정독으로 자기 구현과 1:1 매핑 가능 — "스냅샷 델타 복제를 직접 짜봤기 때문에 UE `FRepLayout::CompareProperties`가 커넥션별 섀도 버퍼로 뭘 하는지 안다."

## 면접 스토리 (문제→해결→수치)

- **결정성**: 108파일 리팩터링을 300틱 해시 비교로 무회귀 증명 → "리팩터링을 감으로 하지 않는다"
- **최적화**: 계측(프로파일러) 먼저 → 병목 확정 → 수정 → 17.8ms→9ms. "추측 최적화 안 함"
- **경계 설계**: Shared/GameSim은 Engine/Client include 금지 — 스크립트 lint로 강제

## CONFIRM_NEEDED

- [ ] 총 개발 기간 공식 표기 (집중 구간 2026-04~07은 확인됨, 시작점 표기 방식 결정)
- [ ] GitHub 공개/비공개 여부 및 링크 정책 (현재 tnestyle70 공개 레포 목록에 없음)
- [ ] Kafka 실사용 여부 (스택 문서에만 등장 — 미확인이므로 이력서에서 제외 상태)
- [ ] 데모 영상 (5:5 봇 매치 캡처 필요 — video/ 스크립트 참조)
