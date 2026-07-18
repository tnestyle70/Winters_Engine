Session - Champion Resource Differentiation Result

관련 계획: `C:/Users/user/Desktop/Winters/.md/plan/2026-07-18_CHAMPION_RESOURCE_DIFFERENTIATION_PLAN.md`

## 예측과 실측

- 예측: 17개 챔피언을 Mana 10 / Energy 2 / Flow 1 / None 4로 분류하고, 리 신·제드는 최대 기력 200과 생존 중 초당 10 회복을 서버 권위로 사용한다.
  - 실측: `RunChampionResourceContractProbe`가 17개 분류, Energy 200/+10, 사망 중 회복 차단과 최대치 clamp를 통과했다. 제드 Q/W/E 비용도 현재 데이터 계약과 일치했다.
- 예측: Mana 전용 아이템 효과가 Energy/Flow/None에 침범하지 않는다.
  - 실측: 마나무네 계열의 최대 마나·보너스 AD·스택과 정수 약탈자의 마나 회복이 Mana 챔피언에만 적용됨을 SimLab이 확인했다. 기존 Mana 챔피언의 `ManaItems` 회귀도 함께 통과했다.
- 예측: 요네·리븐은 리소스 바가 없어지고, 요네 E 영혼은 모델/FX만 유지한 채 체력바·TAB 중복·AI 리소스 대상으로 취급되지 않는다.
  - 실측: `UIResourceKind::None`이 로컬 HUD와 월드바 레이아웃에서 리소스 바를 제거하도록 배선됐다. 요네 영혼은 `HealthComponent` 없이 전용 presentation tag로 상태바/TAB/AI 리소스 경로에서 제외되며 Client Debug 빌드가 통과했다. 라이브 F5 화면 캡처는 자동 검증 범위에 포함하지 않았다.
- 예측: 리 신·제드는 노란 Energy 바, 야스오는 흰 Flow 바를 사용한다.
  - 실측: HUD atlas의 Energy fill, 로컬 HUD와 ImGui/RHI 월드바의 Mana/Energy/Flow 색 분기, None 높이 축소가 Engine/Client 빌드에서 검증됐다. 실제 모니터의 최종 색감은 라이브 캡처가 남아 있다.
- 예측: 야스오는 Flow 0에서 시작해 이동 거리로 100까지 충전하고, R 성공 시 100이 되며, 리콜·리스폰·순간 위치 변경은 충전에 포함하지 않는다. 완충 상태에서 챔피언/정글 몬스터에게 피격되면 레벨 스케일 보호막을 얻고 Flow를 0으로 소비한다.
  - 실측: `Shield` probe가 0.55/0.50/0.45 world-unit 임계값, 불연속 이동 미적립, 레벨 기반 보호막 235.368, 미니언 비발동, Flow 소비를 통과했다.
- 예측: 데이터 생성과 전체 게임플레이 결정성이 유지된다.
  - 실측: 챔피언 생성 해시 `0x0AB83B83`, LoL definition pack `0xFE468B9E`, ordered contract `0x1F416E7039DD7C48`가 재생성 검사와 일치했다. 전체 `SimLab 600 1234`는 same-seed `D651F4627B66C963`, seed+1 `FA20657F5EA5F6D9`로 통과했다.

## 판정

**PASS — 계획한 코드 반영, 생성기 재현성, Debug 빌드, 전체 SimLab 회귀까지 완료했다.**

- GameSim Debug, Engine Debug, Client Debug, Server Debug, SimLab Debug 빌드가 모두 성공했다.
- `SimLab --resource-only`와 전체 `SimLab 600 1234`가 성공했다.
- `git diff --check`는 오류 없이 종료했으며 기존 작업 트리의 LF→CRLF 경고만 출력했다.
- 에이전트 비평에서 지적된 Mana 아이템 격리, 야스오 불연속 이동 악용, 미니언 비발동, 요네 영혼의 UI/AI 누출, 생성 데이터 계약을 구현과 회귀 probe에 반영했다.
- 이번 범위에서 의도적으로 제외한 공식 메커니즘은 리 신 Flurry 기력 반환·재시전 단계별 비용과 제드 W 동일 대상 적중 기력 반환이다. 이 항목을 현재 구현 완료로 간주하지 않는다.

## 갱신된 트레이드오프

- 리소스 종류를 `StatComponent`와 데이터 계약의 명시 필드로 올려 Mana 전용 효과의 오염을 막은 대신, 새 리소스 챔피언을 추가할 때 서버 통계·스냅샷·HUD 종류를 함께 선언해야 한다.
- 야스오 Flow는 서버 권위 이동 거리 적분으로 결정성을 확보했다. 순간 이동 판별은 현재 불연속 tick 경계에 의존하므로 새로운 이동 수단을 추가할 때 해당 경계를 명시적으로 연결해야 한다.
- HUD 색과 바 존재 여부는 자동 회귀와 빌드로 검증했지만 픽셀 단위 시각 품질은 라이브 F5 캡처가 필요하다. 기능 완료 판정과 별개로 리 신·제드·요네·리븐·야스오의 실제 화면 교차 확인을 최종 비주얼 QA로 남긴다.
