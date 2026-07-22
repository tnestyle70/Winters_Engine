Session - 이즈리얼 E가 로컬 예측부터 서버 확정까지 마우스 커서 방향으로 이동하도록 payload 회귀 복구
좌표: 신규 좌표 후보 · 축: C1 기준계, C7 권위와 정합성
관련: 2026-07-17_DATA_DRIVEN_100_PERCENT_RESULT.md

## 1. 결정 기록

① 문제·제약: E 최대 이동거리 4.75 안에서 커서 방향 이동을 유지해야 하나, `GroundTarget` 전환 뒤 `direction=(0,0,0)`을 읽은 로컬 예측이 기본 `+Z`로 이동한다(인게임 실측: 뒤로 이동).
② 순진한 해법의 실패: E를 다시 `Direction`으로 되돌리면 서버의 2.0 단거리 커서 지점 보존 계약이 사라지고 항상 4.75를 이동한다.
③ 메커니즘: `GroundTarget` 명령 생성 시 절대 좌표 `groundPos`와 캐스터→커서 정규 방향 `direction`을 함께 채운다.
④ 대조: 서버 GameSim은 `groundPos-origin`으로 거리·방향을 다시 검증하고, 클라이언트는 같은 명령의 `direction`으로 즉시 예측·방향 애니메이션·FX를 재생한다.
⑤ 대가: 모든 `GroundTarget` 명령의 보조 `direction`이 0이 아니게 된다. 0벡터를 비공식 sentinel로 쓰는 소비자가 발견되면 이즈리얼 전용 resolver로 범위를 축소해야 한다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameLocalSkills.cpp

`CScene_InGame::BuildCastCommand`의 `eTargetMode::GroundTarget` 분기에서 아래 블록을 교체한다.

기존 코드:

```cpp
    case eTargetMode::GroundTarget:
    {
        if (!m_pCamera) return false;
        Vec3 ground = ResolveMouseMapSurfacePos();
        outCmd.groundPos = ground;
        return true;
    }
```

아래로 교체:

```cpp
    case eTargetMode::GroundTarget:
    {
        if (!m_pCamera) return false;
        const Vec3 ground = ResolveMouseMapSurfacePos();
        const Vec3 origin = m_pPlayerTransform
            ? m_pPlayerTransform->GetPosition()
            : Vec3{};
        outCmd.groundPos = ground;
        outCmd.direction = WintersMath::DirectionXZ(
            origin,
            ground,
            Vec3{},
            0.0001f);
        return true;
    }
```

## 3. 검증

예측:
- 이즈리얼 E 명령은 `groundPos`를 유지하면서 `direction=NormalizeXZ(groundPos-origin)`을 보내므로 로컬 예측이 더 이상 0벡터의 `+Z` fallback을 사용하지 않는다.
- 서버의 4.75 최대거리, 짧은 커서 거리 보존, 착지 지점 보정은 `groundPos` 기반이므로 기존 GameSim 결과가 변하지 않는다.
- 다른 `GroundTarget` 스킬은 기존 절대 목표 좌표에 보조 방향만 추가된다. 이를 직접 검증하는 클라이언트 payload 자동화 게이트는 현재 없다.
- Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다.

검증 명령:
- `git diff --check -- Client/Private/Scene/Scene_InGameLocalSkills.cpp .md/plan/2026-07-18_EZREAL_E_CURSOR_DIRECTION_PAYLOAD_REGRESSION_PLAN.md`
- `rg -n -C 12 "case eTargetMode::GroundTarget" Client/Private/Scene/Scene_InGameLocalSkills.cpp`

미검증:
- 사용자가 실행 중인 Release 서버·클라이언트를 보호하기 위해 이번 세션에서는 Client/Server 빌드와 인게임 검증을 수행하지 않는다.

확인 필요:
- 다음 빌드 후 이즈리얼 기준 커서를 전방·후방·좌·우 및 4.75 이내/밖에 두고 E의 첫 프레임 예측과 서버 스냅샷 확정 위치가 같은지 눈 검증한다.
