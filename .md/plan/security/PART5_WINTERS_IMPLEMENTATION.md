# Part 5: Winters Engine 안티치트 — 실전 구현 계획서

> Phase별 점진적 구현 — 기초(서버 권위) → 유저모드 → 커널 → 심화

---

## 전체 아키텍처

```
┌─────────────────────────────────────────────────────────┐
│                    서버 인프라                            │
│  ┌─────────┐  ┌──────────────┐  ┌─────────────────┐    │
│  │ 게임서버 │  │ 안티치트 서버 │  │ 밴 시스템       │    │
│  │ (C++ IOCP)│  │ (Go/분석)    │  │ (DB + 밴 웨이브)│    │
│  └─────┬───┘  └──────┬───────┘  └────────┬────────┘    │
│        │             │                    │              │
│        └─────────────┼────────────────────┘              │
│                      │ Kafka (detection-events 토픽)     │
└──────────────────────┼──────────────────────────────────┘
                       │
            ┌──────────┼──────────┐
            │          │          │
┌───────────▼──┐ ┌────▼─────┐ ┌─▼──────────────┐
│ WintersAC.sys│ │ WintersAC │ │ WintersLOL.exe │
│ (커널 드라이버)│ │ Service   │ │ (게임 클라이언트)│
│              │ │ (.exe)    │ │                │
│ - 핸들 보호  │ │ - IOCTL   │ │ - 인게임 탐지  │
│ - DLL 감시   │ │ - 유저모드 │ │ - 서버 보고    │
│ - 프로세스   │ │   스캔    │ │ - 메모리 암호화│
│   감시       │ │ - 서버 통신│ │ - Heartbeat   │
└──────────────┘ └──────────┘ └────────────────┘
```

---

## Phase 0: 서버 권위 검증 (기초, 즉시 적용)

> **이미 설계됨** — ARCHITECTURE_FINAL.md
> 클라이언트를 신뢰하지 않는 구조. 코드 구현만 하면 됨.

### 구현 사항

```
AntiCheat/
├── Server/                         ← 게임 서버 내부
│   ├── CServerValidator.h/cpp      ← 입력 검증 엔진
│   ├── CSpeedChecker.h/cpp         ← 이동 속도 검증
│   ├── CRangeChecker.h/cpp         ← 스킬 사거리 검증
│   └── CCooldownVerifier.h/cpp     ← 쿨다운 서버 타이머
```

### CServerValidator.h/cpp

```cpp
#pragma once
#include "WintersPCH.h"

// 서버 사이드 치트 탐지 — 게임 서버에 내장
class CServerValidator
{
public:
    ~CServerValidator() = default;

    static std::unique_ptr<CServerValidator> Create()
    {
        return std::unique_ptr<CServerValidator>(new CServerValidator());
    }

    // 이동 검증: 클라이언트가 보낸 위치가 물리적으로 가능한지
    enum class MoveResult { Valid, SpeedHack, Teleport, Invalid };

    MoveResult ValidateMovement(
        uint32_t entityID,
        const Vec3& newPos,
        float serverDeltaTime,
        float maxSpeed)
    {
        auto it = m_LastPositions.find(entityID);
        if (it == m_LastPositions.end())
        {
            m_LastPositions[entityID] = newPos;
            return MoveResult::Valid;
        }

        float distance = Vec3::Distance(it->second, newPos);
        float maxDistance = maxSpeed * serverDeltaTime * 1.1f; // 10% 허용 오차

        if (distance > maxDistance * 5.0f)
        {
            // 텔레포트 수준의 이동 → 즉시 거부
            return MoveResult::Teleport;
        }
        if (distance > maxDistance)
        {
            // 속도 핵 의심 → 카운터 증가
            m_SpeedViolations[entityID]++;
            if (m_SpeedViolations[entityID] > 10)
            {
                return MoveResult::SpeedHack;
            }
            // 초기에는 서버 보정으로 처리
            return MoveResult::Valid;
        }

        m_LastPositions[entityID] = newPos;
        m_SpeedViolations[entityID] = 0; // 정상이면 카운터 리셋
        return MoveResult::Valid;
    }

    // 쿨다운 검증
    bool ValidateAbilityUse(uint32_t entityID, uint32_t abilitySlot)
    {
        auto key = std::make_pair(entityID, abilitySlot);
        auto it = m_CooldownTimers.find(key);

        if (it != m_CooldownTimers.end())
        {
            double now = GetServerTime();
            if (now < it->second)
            {
                // 쿨다운 미완료 → 치트!
                return false;
            }
        }
        return true;
    }

    void SetCooldown(uint32_t entityID, uint32_t slot, float duration)
    {
        m_CooldownTimers[{entityID, slot}] = GetServerTime() + duration;
    }

    // 사거리 검증
    bool ValidateRange(const Vec3& casterPos, const Vec3& targetPos, float maxRange)
    {
        float dist = Vec3::Distance(casterPos, targetPos);
        return dist <= maxRange * 1.05f; // 5% 허용 오차 (네트워크 지연 보정)
    }

    // FOW (Fog of War) — 시야 밖 정보 미전송
    bool IsVisible(uint32_t observerID, uint32_t targetID)
    {
        // 팀이 같으면 항상 보임
        // 시야 범위 내에 있는지 계산
        // AOI 그리드 기반 시야 판정
        // false이면 클라이언트에 해당 엔티티 데이터 전송 안 함!
        return false; // 구현 필요
    }

private:
    CServerValidator() = default;

    std::unordered_map<uint32_t, Vec3>     m_LastPositions;
    std::unordered_map<uint32_t, uint32_t> m_SpeedViolations;
    std::map<std::pair<uint32_t, uint32_t>, double> m_CooldownTimers;

    double GetServerTime() const
    {
        // 서버 고정 틱 시간 반환
        return 0.0; // 구현 필요
    }
};
```

### Verification

```
[ ] 클라이언트가 속도 2배로 이동 시도 → 서버에서 거부, 보정 위치 전송
[ ] 쿨다운 중 스킬 재사용 시도 → 서버에서 거부
[ ] 사거리 밖 적 공격 시도 → 서버에서 거부
[ ] FOW 밖 적 정보 → 클라이언트에 전송 안 됨
```

---

## Phase 1: 유저모드 기초 (Level 1)

> 게임 프로세스 내부에서 기본 탐지.
> 별도 드라이버 불필요, EXE에 링크.

### 구현 사항

```
AntiCheat/
├── Client/                         ← 게임 클라이언트 내부
│   ├── CAntiCheatClient.h/cpp      ← 안티치트 메인 모듈
│   ├── CDebugDetector.h/cpp        ← 디버거 탐지
│   ├── CIntegrityChecker.h/cpp     ← 코드 무결성
│   ├── CModuleScanner.h/cpp        ← DLL 스캔
│   └── CHeartbeat.h/cpp            ← 서버 heartbeat
```

### CAntiCheatClient — 메인 모듈

```cpp
#pragma once
#include "WintersPCH.h"

class CAntiCheatClient
{
public:
    ~CAntiCheatClient() = default;

    static std::unique_ptr<CAntiCheatClient> Create(const std::string& serverURL)
    {
        auto ac = std::unique_ptr<CAntiCheatClient>(new CAntiCheatClient());
        ac->m_ServerURL = serverURL;
        ac->m_pDebugDetector = CDebugDetector::Create();
        ac->m_pIntegrity = CIntegrityChecker::Create(GetModuleHandleW(nullptr));
        ac->m_pModuleScanner = CModuleScanner::Create();
        ac->m_pHeartbeat = CHeartbeat::Create(serverURL);
        return ac;
    }

    // 매 프레임이 아닌 비동기 스레드에서 실행
    void StartMonitoring()
    {
        m_bRunning = true;
        m_MonitorThread = std::thread([this]() { MonitorLoop(); });
    }

    void StopMonitoring()
    {
        m_bRunning = false;
        if (m_MonitorThread.joinable())
            m_MonitorThread.join();
    }

private:
    CAntiCheatClient() = default;

    void MonitorLoop()
    {
        while (m_bRunning)
        {
            // 랜덤 간격으로 검사 (패턴 예측 방지)
            std::this_thread::sleep_for(
                std::chrono::milliseconds(2000 + (rand() % 3000)));

            // 1. 디버거 탐지 (여러 방법 병행)
            if (m_pDebugDetector->Detect())
            {
                ReportDetection(DetectionType::Debugger);
            }

            // 2. 코드 무결성 (.text 섹션 해시)
            if (!m_pIntegrity->Verify())
            {
                ReportDetection(DetectionType::CodeTamper);
            }

            // 3. 모듈 스캔 (미서명 DLL, 블랙리스트)
            auto suspicious = m_pModuleScanner->Scan();
            for (const auto& mod : suspicious)
            {
                ReportDetection(DetectionType::SuspiciousModule, mod.name);
            }

            // 4. Heartbeat (서버에 "나 정상이야" 주기적 전송)
            m_pHeartbeat->Send();
        }
    }

    void ReportDetection(DetectionType type,
                         const std::wstring& detail = L"")
    {
        // 서버에 암호화된 탐지 리포트 전송
        // → 즉시 킥하지 않음, 서버가 판단
    }

    std::atomic<bool> m_bRunning = false;
    std::thread       m_MonitorThread;
    std::string       m_ServerURL;

    std::unique_ptr<CDebugDetector>    m_pDebugDetector;
    std::unique_ptr<CIntegrityChecker> m_pIntegrity;
    std::unique_ptr<CModuleScanner>    m_pModuleScanner;
    std::unique_ptr<CHeartbeat>        m_pHeartbeat;

    enum class DetectionType
    {
        Debugger, CodeTamper, SuspiciousModule,
        IATHook, OverlayDetected, MemoryTamper
    };
};
```

### Verification

```
[ ] Cheat Engine 연결 시 → 디버거 탐지 리포트 전송
[ ] .text 섹션 1바이트 패치 시 → 코드 변조 탐지
[ ] 미서명 DLL 인젝션 시 → 모듈 탐지
[ ] Heartbeat 미전송 시 → 서버에서 타임아웃 감지
```

---

## Phase 2: 유저모드 심화 (Level 2)

### 추가 구현

```
AntiCheat/
├── Client/
│   ├── CHookDetector.h/cpp          ← IAT/Inline 훅 탐지
│   ├── COverlayDetector.h/cpp       ← 오버레이 윈도우 탐지
│   ├── CMemoryEncryptor.h/cpp       ← 게임 데이터 암호화
│   └── CTimingValidator.h/cpp       ← 타이머 무결성
```

### 핵심: 메모리 암호화 적용

```cpp
// 게임 코드에서 직접 float 대신 CEncryptedFloat 사용

struct HealthComponent
{
    CEncryptedFloat current;    // float 대신 암호화된 값
    CEncryptedFloat maximum;
    CDistributedFloat regenRate; // 분산 저장
    bool alive = true;
};

// Cheat Engine "float 100.0" 스캔 → 매치 안 됨!
// 포인터 스캔도 어려워짐 (키가 매번 바뀌므로 값이 불안정)
```

---

## Phase 3: 커널 기초 (Level 3)

### 프로젝트 구조

```
AntiCheat/
├── Driver/                          ← 커널 드라이버
│   ├── WintersAC.inf                ← 드라이버 설치 정보
│   ├── main.cpp                     ← DriverEntry
│   ├── ProcessProtection.cpp        ← ObRegisterCallbacks
│   ├── ImageMonitor.cpp             ← PsSetLoadImageNotifyRoutine
│   ├── Communication.cpp            ← IOCTL 핸들러
│   └── WintersAC.vcxproj            ← WDK 프로젝트
├── Service/                         ← 유저모드 서비스
│   ├── WintersACService.cpp         ← 드라이버 로드/통신
│   └── ServerReporter.cpp           ← 서버 리포트
├── Client/                          ← 게임 내 모듈 (Phase 1~2)
└── Server/                          ← 서버 검증 (Phase 0)
```

### WDK 빌드 설정 (vcxproj)

```xml
<!-- 핵심 설정 -->
<PropertyGroup>
  <TargetVersion>Windows10</TargetVersion>
  <DriverType>WDM</DriverType>
  <KMDF_VERSION_MAJOR>1</KMDF_VERSION_MAJOR>
</PropertyGroup>

<PropertyGroup>
  <!-- 테스트 서명 (개발 중) -->
  <Sign_File>true</Sign_File>
  <TestSign>true</TestSign>
  <!-- 릴리스 시 EV 인증서로 교체 -->
</PropertyGroup>
```

### Verification

```
[ ] 치트가 OpenProcess(PROCESS_VM_READ)로 게임 핸들 요청
    → ObCallback이 VM_READ 제거 → ReadProcessMemory 실패
[ ] 미서명 DLL이 게임 프로세스에 로드 시도
    → ImageLoadNotify에서 탐지 → 서버 리포트
[ ] 알려진 치트 프로세스 실행 시도
    → ProcessNotify에서 차단 (선택적)
[ ] 드라이버 로드/언로드 정상 동작
    → BSOD 없이 안정적으로 운영
```

---

## Phase 4: 커널 심화 (Level 4)

### 추가 구현

```
AntiCheat/
├── Driver/
│   ├── KernelScanner.cpp            ← 커널 무결성 검사
│   ├── VadWalker.cpp                ← VAD 트리 스캔 (Manual Map 탐지)
│   ├── Minifilter.cpp               ← 파일 시스템 감시
│   └── HwidCollector.cpp            ← 하드웨어 ID 수집
```

---

## 서버 사이드 분석 시스템

### Go 안티치트 분석 서비스

```
Services/
├── internal/anticheat/
│   ├── model.go                     ← 탐지 리포트 모델
│   ├── consumer.go                  ← Kafka detection-events 컨슈머
│   ├── analyzer.go                  ← 패턴 분석 + 밴 결정
│   ├── repository.go                ← PostgreSQL 저장
│   └── handler.go                   ← 관리 API
├── cmd/anticheat/
│   └── main.go
```

### Kafka 토픽

```
detection-events:
  Key: user_id
  Value: {
    "type": "debugger|code_tamper|suspicious_module|speed_hack|...",
    "severity": 1-10,
    "timestamp": "2026-04-12T15:30:00Z",
    "client_data": { ... },
    "match_id": "uuid",
    "hwid_hash": "sha256"
  }

밴 결정 로직:
  - severity >= 8: 즉시 킥 (서버에서)
  - severity 5~7: 플래그, 관찰 모드
  - severity < 5: 로그만 기록
  - 동일 유저 탐지 N회 이상: 자동 밴 큐
  - 밴 웨이브: 매주 일괄 처리 (치트 개발자 혼란)
```

---

## 구현 순서 타임라인

```
Week 1: Phase 0 (서버 권위)
  - CServerValidator 구현
  - 게임 서버에 통합
  - 이동/스킬/쿨다운 검증

Week 2: Phase 1 (유저모드 기초)
  - CDebugDetector, CIntegrityChecker
  - CModuleScanner, CHeartbeat
  - 서버 리포트 연동

Week 3: Phase 2 (유저모드 심화)
  - CHookDetector (IAT/Inline)
  - CMemoryEncryptor (값 암호화)
  - COverlayDetector

Week 4-5: Phase 3 (커널 기초)
  - WDK 환경 설정
  - DriverEntry + IOCTL
  - ObRegisterCallbacks
  - PsSetLoadImageNotifyRoutine
  - 테스트 서명으로 로컬 테스트

Week 6: Phase 4 (커널 심화)
  - VAD 워커
  - 미니필터
  - HWID 수집
  - 서버 분석 파이프라인 (Go + Kafka)

이후: Phase 5 (하이퍼바이저 — 연구)
  - VT-x 학습
  - EPT 기반 메모리 보호 PoC
  - 프로덕션 적용은 별도 판단
```

---

## 보안 고려사항

```
1. 안티치트 드라이버 자체의 보안:
   - 드라이버 코드에 취약점이 있으면 역으로 악용됨
   - IOCTL 입력 검증 철저 (버퍼 오버플로우, 정수 오버플로우)
   - ProbeForRead/ProbeForWrite로 유저모드 버퍼 검증
   - SEH(__try/__except)로 크래시 방지

2. 오탐(False Positive) 방지:
   - 디버거 탐지: Visual Studio, x64dbg (개발 중 예외 처리)
   - 모듈 탐지: OBS, Discord, NVIDIA Overlay는 정상
   - 화이트리스트 관리 시스템 필요

3. 프라이버시:
   - HWID 수집 시 단방향 해시만 서버에 전송
   - 파일 시스템 스캔 시 게임 관련 경로만
   - GDPR/개인정보 보호법 준수

4. 안정성:
   - 커널 드라이버 BSOD = 유저 PC 재부팅
   - 철저한 테스트 (Driver Verifier, WHQL)
   - 점진적 롤아웃 (10% → 50% → 100%)
```
