# 2026-07-18 리플레이 Chrono 타임라인 + 프로필/메인메뉴 복귀 + 3클라 RP 결과

## 1. 예측 vs 실측

- 적중: `win/loss/draw` 완료 결과의 RP를 1000으로 고정했고 `aborted`/빈 결과는 0으로 유지했다. `consumer_test.go` 추가 후 `Services`의 `go test ./...` 전체가 exit 0으로 통과했다. `ReportMatch`는 기존 `(user_id, match_id) ON CONFLICT DO NOTHING` transaction을 유지해 동일 이벤트 재전달 시 RP 중복을 막는다.
- 적중: 모든 snapshot이 아닌 `deltaBaseTick==0` full snapshot만 seek anchor로 인덱싱하고, first full tick부터 시작/Restart하며 목표 tick까지 순방향 적용하도록 수정했다. seek 전 SnapshotApplier→씬 timeline callback→EventApplier 표현 상태와 GameEnd latch를 초기화한다.
- 적중: 기존 `280×116` 저장형 Replay 창을 하단 중앙 `NoSavedSettings` 176px 고정 패널로 교체해 timeline, Restart, Play/Pause, speed, 프로필/메인메뉴 버튼을 한 화면에 배치했다. slider seek는 pause하며, 자연 재생의 `finished false→true` 전이만 2초 자동 메인메뉴 타이머를 시작한다.
- 적중: 프로필 복귀는 위험한 backend reset/reconfigure 대신 `RequestPostMatchRefresh` pending latch를 사용한다. 기존 in-flight가 끝난 뒤 profile/storefront(RP)/replay를 새로 요청하고 profile client의 history를 직렬화한다.
- 적중: `git diff --check` exit 0, Client `Debug|x64` 및 `Release|x64` 빌드 exit 0. 기존 C4251/C4275 DLL interface 경고는 남았지만 신규 오류는 0건이다.
- 적중: `room1_tick1_2119.wrpl`을 Debug/Release `WintersGame.exe --replay-index-smoke=...`로 각각 실행해 exit 0. full snapshot anchor 부재/FlatBuffer/offset/trailing 검증 실패가 없었다.
- 적중: `docker compose up -d --build profile` exit 0 후 `services-profile-1`이 새 이미지로 기동했고 `profile kafka consumer started`와 `profile service starting` 로그를 확인했다. Compose 의존성 때문에 one-off migrate/kafka-init도 재실행됐지만 PostgreSQL/Kafka/Redis와 다른 상시 서비스는 중단되지 않았다.
- 미검증: 실제 서버 1 + 계정 클라 3의 넥서스 파괴, 세 계정 RP 정확히 +1000/전적 1건/cloud replay 1건, 각 화면의 mouse scrub·프로필 복귀·자연 종료 자동 복귀 영상은 수동 촬영 acceptance로 남는다. ESC/F6 종료는 이 acceptance 경로가 아니다.

## 2. 판결

수정 반영 — 독립 비평의 full-snapshot P0, in-flight refresh P1, 끝 tick 수동 scrub P1, first seekable tick P2를 계획에 반영한 구현이 Go 테스트·Debug/Release 빌드·양쪽 WRPL index smoke·profile container 재기동을 통과했다.

## 3. ⑤ 갱신

- 현재 서버 WRPL은 매 tick full snapshot이라 seek가 실질적으로 목표 tick group 1개지만, 포맷이 delta snapshot을 기록하기 시작하면 가장 가까운 full anchor부터 목표까지의 적용 비용이 커진다. 그 시점에는 full keyframe 간격 budget과 seek latency 측정이 필요하다.
- seek는 목표 snapshot의 권위 상태와 이후 이벤트를 복구하지만 목표 이전부터 살아 있던 transient FX의 정확한 age/잔상은 재시뮬레이션하지 않는다. 영상에서 이 차이가 보이면 FX checkpoint/age 직렬화가 별도 범위다.
- Debug/Release와 index-load는 통과했지만 UI mouse interaction과 3개의 독립 계정 projection은 자동화되지 않았다. 세 계정이 같은 signed match assignment를 쓰고 넥서스 파괴로 종료한 단일 촬영에서만 최종 E2E 완료로 판정한다.
