# 1. 예측 vs 실제

- 예측: 미니언 source가 챔피언 또는 포탑/억제기를 처치하면 kill-feed가 발행되고 빈 portrait 대신 `offscreenping_atlas.png` 미니언 glyph가 표시된다. 실제: `DamageQueueSystem`의 source gate가 champion/minion을 구분하고 지원 target 3종에 event를 허용하며, client source net entity 판정과 UI banner bool 체인이 `970,264..1014,315` UV draw까지 연결됐다.
- 예측: 일반 crit와 Fiora E 1·2타, Zed 약자멸시에 `statspanel_atlas.png` 치명타 표식이 표시되되 비치명 피해가 crit truth로 변조되지 않는다. 실제: `bWasCrit`는 그대로 유지하고 별도 `DamageFlag_ShowCriticalIndicator`를 `DamageEvent.flags`로 직렬화했다. Fiora E 1·2타와 Zed passive bonus만 flag를 설정하며 UI는 실제 crit 또는 flag일 때 `232,488..256,512` child를 숫자 왼쪽에 그린다.
- 예측: schema와 API 전달 chain이 일치한다. 실제: `Shared/Schemas/run_codegen.bat` PASS, atlas/schema/serializer/client/UI assertion PASS, 지정 파일 `git diff --check` PASS였다.
- 계획과 다른 점: 없음. 빌드와 인게임 눈 검증은 사용자가 실행 중인 클라/서버를 보존하라는 요청에 따라 수행하지 않았다.

# 2. 판정

- 수정 반영.
- 서버 권위 damage 결과와 presentation indicator를 분리했고, 새 PNG나 별도 texture copy 없이 `Client/Bin/Resource`의 기존 두 atlas만 재사용했다.
- 정적 검증 범위에서는 schema field, serializer, client event apply, GameInstance/UI API, SRV 생명주기, UV 렌더 경로가 모두 연결됐다. 컴파일·링크 판정은 후속 Client/Server Debug x64 빌드가 필요하다.

# 3. 규칙 갱신

- 치명타처럼 gameplay truth와 강조 표시가 겹치는 UI는 `bWasCrit`를 재해석하지 말고 별도 서버 전달 presentation flag로 구분한다.
- atlas child는 parent 해상도와 alpha 실측 범위를 기록하고, 작은 glyph 확대가 필요하면 원본 PNG를 복제하지 말고 여백을 줄인 UV crop으로 해결한다.
- 비챔피언 kill-feed source는 가짜 champion id로 치환하지 않고 authoritative source net entity를 presentation 단계에서 분류한다.
