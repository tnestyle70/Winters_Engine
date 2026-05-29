# Winters Engine — Security & Anti-Cheat Knowledge Base

> 대상: DX11 C++20 멀티플레이어 PvP 게임 엔진 개발자
> 목표: 기초 보안 개념 → 리버스 엔지니어링 이해 → 유저모드 안티치트 → 커널 레벨 안티치트 (Vanguard 스타일)
> 작성일: 2026-04-12

---

## 파트 구성

| # | 파일 | 내용 |
|---|------|------|
| 1 | [PART1_SECURITY_FUNDAMENTALS.md](PART1_SECURITY_FUNDAMENTALS.md) | Windows 보안 모델, 메모리 보호, 프로세스 격리, PE 포맷, 권한 |
| 2 | [PART2_ATTACK_TECHNIQUES.md](PART2_ATTACK_TECHNIQUES.md) | DLL Injection, Code Injection, API Hooking, Memory Hacking, 치트 엔진 원리 |
| 3 | [PART3_USERMODE_ANTICHEAT.md](PART3_USERMODE_ANTICHEAT.md) | 유저모드 탐지/방어, 무결성 검증, 디버거 탐지, 난독화 |
| 4 | [PART4_KERNEL_ANTICHEAT.md](PART4_KERNEL_ANTICHEAT.md) | 커널 드라이버 안티치트, 콜백, 미니필터, 하이퍼바이저, Vanguard 아키텍처 분석 |
| 5 | [PART5_WINTERS_IMPLEMENTATION.md](PART5_WINTERS_IMPLEMENTATION.md) | Winters Engine 실전 적용 — Phase별 구현 계획 (기초→중급→고급) |

---

## 로드맵

```
Level 0 (기초): 서버 권위 검증
  → 이미 설계됨 (ARCHITECTURE_FINAL.md)
  → 클라이언트를 신뢰하지 않는 구조

Level 1 (유저모드 기초): 메모리 무결성 + 디버거 탐지
  → 코드 섹션 해시 검증
  → IsDebuggerPresent / NtQueryInformationProcess
  → 간단한 DLL 인젝션 탐지

Level 2 (유저모드 심화): Hooking 탐지 + 모듈 검증
  → IAT/EAT 변조 탐지
  → 서명되지 않은 모듈 탐지
  → 스레드 하이재킹 감지

Level 3 (커널 기초): 커널 드라이버 + 콜백
  → ObRegisterCallbacks (프로세스 핸들 보호)
  → PsSetCreateProcessNotifyRoutine
  → 치트 프로세스 차단

Level 4 (커널 심화): 미니필터 + DKOM 탐지
  → 파일 시스템 미니필터 (치트 파일 감시)
  → SSDT 훅 탐지
  → 커널 메모리 무결성

Level 5 (하이퍼바이저): VT-x 기반 감시 (참고)
  → EPT 위반 감지
  → 커널 패치 불가능하게 만들기
```

## 핵심 원칙

1. **서버가 진실**: 클라이언트의 모든 입력은 서버에서 검증
2. **심층 방어**: 한 레이어가 뚫려도 다음 레이어가 방어
3. **탐지 > 차단**: 즉시 차단보다 데이터 수집 후 밴 웨이브
4. **성능 예산**: 안티치트가 프레임 시간을 1ms 이상 잡아먹으면 안 됨
5. **오탐 최소화**: 정상 유저를 치터로 판별하는 것이 최악의 시나리오
