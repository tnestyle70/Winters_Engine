# 무기 카드 03 — Minecraft Dungeons 모작 (5인 팀, 자체 DX9 엔진)

> 팀 프로젝트 (5인, git 실측) · 2026.03.10 ~ 04.06 (약 4주, 커밋 실측) · 협업 증명 무기
> 저장소: https://github.com/tnestyle70/SR_Minecraft_Dungeons (본인 계정 소유)
> 데모 영상: `C:\Users\user\Desktop\SR_MinecraftDungeons\SR_MinecraftDungeons (1).mp4` (409MB, 내용 확인 필요)

## 한 줄

자체 DX9 엔진(DLL) + 클라이언트 + IOCP 전용 서버 + UI 에디터 4프로젝트 구조(약 12만 줄)의 5인 팀 모작 — 전체 커밋의 64%(220/342) 기여, 레포 소유·PR 135건 머지 관리.

## 협업 증명 (git 실측 — 이력서에서 가장 강한 형태의 근거)

- 342커밋 / PR 기반 브랜치 협업 (Merge PR 135건, #136까지)
- 5인 기여 분포: 본인(winter77) 220 / JS 50 / Taejun 32 / KCY 30 / 서준원 10
- 팀원별 파일 프리픽스 관례(`CJS*`/`CCY*`/`CTG*`)로 담당 영역 분리 — 프리픽스 없는 코어 파일 다수가 본인 커밋

## 본인 담당 (git author + 최초 생성자 근거)

1. **네트워크/서버 전체**: IOCP 비동기 서버(`Server/CServer.cpp`), 커스텀 바이너리 패킷 프로토콜(`Shared/PacketDef.h` — Login/Spawn/Input/StateSnapshot/DragonSync 등 20여 구조체), **20TPS 고정 틱 + 틱마다 상태 스냅샷 브로드캐스트**(`Server/CGameLoop.h` TICK_RATE=20, `BroadcastSnapshot`), 클라 `CNetworkMgr/CNetworkPlayer/CNetworkStage`. 커밋: "feature/IOCP Refactoring", "Death Sync", "Network Damage Bug Fix"
2. **엔더드래곤 보스전**: 패턴/연출/전용 동기화 패킷 (`CEnderDragon.cpp` 최초 작성, "EnderDragon Patterns/Dissolve/DragonCam")
3. **ImGui 맵 에디터**: 멀티 스테이지 저장/로드, TriggerBox(씬 전환·몬스터 스폰) (`CEditor.cpp` 최초 작성, 21회 수정)
4. **엔진 컴포넌트**: 파티클 시스템, 사운드(FMOD), 포스트프로세스 셰이더(`PostProcess.fx` — Noise/GlassBreak/DragonBreath 등 6 technique), 쿼드트리 프러스텀 컬링("feature/QuadTree"), 인벤토리/HUD

## 서사 연결 (핵심)

**여기서 20TPS 스냅샷 동기화를 팀으로 만들었고 → Winters에서 30Hz 서버 권위 풀 파이프라인(커맨드 검증·예측·랙컴펜·결정성 해시)으로 혼자 확장했다.** 네트워크 성장 곡선이 git으로 증명되는 구조.

## CONFIRM_NEEDED

- [ ] 공식 역할 명칭 확정 (예: "서버/네트워크 + 보스전 담당" — git 근거는 강하나 팀 내 공식 분담 표기는 본인 확인)
- [ ] 커밋 안 한 팀원 존재 여부 (5인 표기 확정)
- [ ] 데모 영상 내용/발표용 여부
- [ ] GitHub에서 fork로 표시되는 문제 — 원본/포크 관계 정리 (핀 고정 + README에 역할 명시로 보완)
