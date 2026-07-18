Session - Zed Q 수리검 축/재질 및 W·R 독립 4초 그림자 교환 복구 결과
좌표: Client Projectile/Fx · Shared/GameSim Zed · LoL Definition Pack
관련: `.md/plan/2026-07-17_ZED_Q_SHURIKEN_W_R_SHADOW_SWAP_REMEDIATION_PLAN.md`

## 1. 예측 대비 측정

| 항목 | 예측 | 측정 결과 |
|---|---|---|
| Q 메쉬 축 | Roll(Z) 90° 후 world-Y 회전 | WFX `rotation.z=1.5707963`, 제드 메쉬 방향 Yaw 차단, spin `12.566` 확인 |
| Q 텍스처 | 메쉬 UV용 아틀라스 사용 | `zed_shuriken_tx.png` 존재(266,812 bytes) 및 WFX 경로 확인 |
| W stage | Ground→Self, 4초 | 생성 C++/JSON에서 `Ground/Self`, `StageDependent`, `4.0` 확인 |
| R stage | Unit→Self, 4초 | 생성 C++/JSON에서 `Unit/Self`, `StageDependent`, `4.0` 확인 |
| W/R 상태 | 독립 수명/좌표 | `ZedSimComponent`의 `wShadow`, `rShadow` 분리 및 Q/E 두 slot 순회 확인 |
| 생성 정합 | source와 generated 일치 | ChampionGameData `0x780B63F7`, LoL Definition Pack `0x523FD8DB`, 양쪽 `--check` PASS |
| 정적 무결성 | 파서/공백 오류 없음 | Python AST, JSON/WFX parse, 잔존 단일-shadow 검색, targeted `git diff --check` PASS |

## 2. 판정

정적 구현은 완료다. Q 눕힘/UV, W·R 독립 4초 재입력, 서버 위치 교환, R 후방 착지, 그림자 체력바 제거, Q/E 복제 원점까지 코드와 생성 데이터가 같은 계약을 가리킨다. C++ Debug 빌드와 인게임 눈 검증은 실행 중인 사용자 서버/클라이언트를 보호하기 위해 보류했으므로 최종 런타임 판정은 `PENDING`이다.

## 3. 갱신 비용

이번 변경은 제드 Q/W/R과 stage별 target 생성 경로로 제한했다. 후속 비용은 Debug x64 1회 빌드와 인게임 체크 7개(W1 무이동, 체력바 없음, W2 교환, R 후방 착지, R2 교환, W/R 동시 Q·E, Q 수평 회전)이며, 실패 시 `[ZedShadow][R1]` 및 `[ZedShadow][Swap]` Debug 출력으로 서버 좌표부터 분리 진단한다.
