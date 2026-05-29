# Winters Engine TODO

> **엔진**: 범용 DX11 C++20 — 하나의 DLL로 LoL/엘든링 동시 구동
> **현재**: LoL 30일 모작 Phase 0 시작 (엔진 기초 완료)
> **다음**: Phase 0 에셋 파이프라인 → Phase 1 JobSystem 강화

---

## 완료 (엔진 기초)

### 삼각형 렌더링 버그 수정
- [x] DebugRender() 이중 Present 제거
- [x] BeginFrame() 뷰포트 매 프레임 재설정
- [x] 스왑체인 FLIP_DISCARD → DISCARD 안정화
- [x] Triangle.hlsl: SV_VertexID 기반 렌더링으로 전환 (VB 불필요)

### 카메라 + 큐브 렌더링
- [x] WintersMath (Vec2/Vec3/Vec4/Mat4)
- [x] CTransform (Dirty Flag + SRT)
- [x] CCamera (FPS, RMB+마우스, WASD)
- [x] CInput (WM_KEYDOWN/UP/MOUSEMOVE)
- [x] DX11ConstantBuffer\<T\>, DX11Buffer IndexBuffer
- [x] CubeGeometry (24v/36i), CubeRenderer (pImpl)
- [x] Default3D.hlsl (MVP + 디퓨즈)
- [x] DX11Pipeline::Create3D()

### ECS (워크트리 작업 완료, main 미머지)
- [x] Entity, ComponentStore, World, ISystem, SystemScheduler, CommandBuffer
- [x] Components: Transform, RigidBody, Health, MeshRenderer 등
- [x] Systems: Physics, Render, AI, Collision, Health
- [ ] **main 브랜치에 머지 필요**

### 백엔드 Phase 0 (인프라)
- [x] Go 모듈 초기화, PostgreSQL/Redis/Kafka 연동
- [x] DB 마이그레이션 7세트 (users, wallets, player_stats, match_history 등)
- [x] JWT 인증, 미들웨어 (auth, logging, recovery)
- [x] Docker Compose: postgres:5433, redis:6379, kafka:9092

### 백엔드 Phase 1 (Auth Service)
- [x] 회원가입/로그인/JWT 토큰 발급/갱신/로그아웃
- [x] bcrypt 비밀번호 해싱, Redis 리프레시 토큰 관리
- [x] cmd/auth/main.go 분리 (package main 충돌 해소)

### 백엔드 Phase 2-4 (Leaderboard + Matchmaking + Profile)
- [x] Leaderboard: Redis Sorted Set + PostgreSQL 이중 쓰기, Kafka consumer
- [x] Matchmaking: MMR 기반 자동 매칭 (1초 간격, 범위 확장 알고리즘), Kafka publish
- [x] Profile: Cache-Aside (Redis 5분 TTL), 전적 기록, Kafka consumer
- [x] 4개 서비스 동시 기동 + API 동작 확인 완료 (2026-04-12)
- [ ] Kafka MatchCompleted 이벤트 E2E 테스트 (Phase 7에서 수행)

---

## LoL 30일 모작 (상세: `.md/.plan/LOL_30DAY_MASTER_PLAN.md`)

### Phase 0 — 에셋 파이프라인 (D0~D2)
- [ ] Engine/Public/Resource/AssetFormat.h (.wmesh/.wanim/.wmat 바이너리 포맷)
- [ ] Tools/blender_export.py (Blender → .wmesh 일괄 변환)
- [ ] Tools/convert_textures.bat (DirectXTex BC7 DDS 변환)
- [ ] CMeshLoader, CAnimLoader, CTextureLoader, CMaterialLoader

### Phase 1 — Fiber JobSystem & 코어 강화 (D3~D5)
- [ ] Fiber Pool + Counter 의존성 그래프 (Naughty Dog GDC 2015)
- [ ] CLinearAllocator (프레임 임시), CPoolAllocator (고정 크기)
- [ ] CEventBus (타입세이프 이벤트, 지연 발행)

### Phase 2 — RenderGraph & Deferred Pipeline (D6~D10)
- [ ] CRenderGraph (패스 DAG, 자동 리소스 관리)
- [ ] G-Buffer Pass (Albedo+Metallic, Normal, Roughness+AO)
- [ ] Clustered Deferred Lighting (Compute Shader)
- [ ] Cascaded Shadow Maps (4단계)
- [ ] PostFX: Bloom, ToneMapping(ACES), FXAA, SSAO, Fog

### Phase 3 — GPU-Driven & Profiling (D11~D13)
- [ ] GPU 프러스텀 컬링 Compute Shader
- [ ] IndirectDraw (ExecuteIndirect)
- [ ] CProfiler (CPU/GPU 타이밍, ImGui 오버레이)
- [ ] CDisplaySettings (해상도, 창모드, RenderScale)

### Phase 4 — 네트워크 & 게임 서버 (D14~D19)
- [ ] CUDPSocket, CKCPTransport (UDP + 신뢰성 레이어)
- [ ] CPacketSerializer (FlatBuffers)
- [ ] CClientNet (연결, 입력 전송, 스냅샷 수신)
- [ ] CNetworkPrediction (클라이언트 예측 + 서버 보정)
- [ ] CIOCPServer, CSession, CSessionMgr
- [ ] CGameRoom (5v5, 챔피언 선택 → 인게임 → 결과)
- [ ] CAOIManager (50m 그리드, 90% 패킷 절감)
- [ ] CLagCompensation (과거 스냅샷 롤백 히트 판정)

### Phase 5 — Go 백엔드: Payment + Shop (D20~D23)
- [ ] Payment Service (8085): 결제→코인 충전, 멱등성, 원자적 트랜잭션
- [ ] Shop Service (8086): 아이템 구매, 인벤토리, 잔액 차감
- [ ] Kafka E2E 통합 테스트 (Phase 7: MatchCompleted → 랭킹+전적 자동 갱신)
- [ ] C++ Client Network SDK (Phase 8: WinHTTP + CAuthClient/CMatchClient 등)

### Phase 6 — 안티치트 (D24~D26)
- [ ] WintersGuard.sys 커널 드라이버 (ObRegisterCallbacks, 프로세스 보호)
- [ ] WintersGuardService.exe 유저모드 (메모리 스캔, 무결성 검사, 하트비트)
- [ ] CAntiCheatServer (speedhack, cooldown, range, damage 서버 검증)

### Phase 7 — 에디터 & 콘텐츠 (D27~D28)
- [ ] ImGui DX11 통합, 에디터 윈도우 (Hierarchy, Inspector, Console)
- [ ] 소환사의 협곡 맵 데이터 (.wmap, .wnav, .wlight)
- [ ] Lua 챔피언 시스템 (아리 Q/W/E/R 데이터 드리븐)

### Phase 8 — 통합 테스트 (D29)
- [ ] E2E: 회원가입 → 로그인 → 상점 → 매치메이킹 → 5v5 → 결과 DB
- [ ] 성능: 1080p 60FPS, 서버 20TPS
- [ ] 메모리 누수 검사

---

## 향후 — 엘든링 모작 (LoL 완료 후)
- [ ] 3인칭 카메라 컨트롤러 (Spring Arm, 락온)
- [ ] 레벨 스트리밍 (오픈월드)
- [ ] 고급 애니메이션 (IK, 레이어드 블렌딩, CCD)
- [ ] 보스 AI (Behavior Tree)
- [ ] 세이브/로드 시스템
- [ ] Co-op 네트워크 (2~4인)
