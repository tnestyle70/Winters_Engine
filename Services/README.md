# Services

원자: BackendState, LiveOpsContract

Services는 match 밖의 backend state와 live service 계약을 소유한다.

소유:
- account
- matchmaking
- profile
- store
- entitlement
- service migrations
- telemetry/live ops 계약이 backend와 분리되어야 할 때의 service boundary

소유하지 않음:
- in-match gameplay truth
- Client visual state
- Engine runtime primitive
- cooked asset source

구조 규칙:
- 기존 `Services/` 폴더명과 Go 구조를 유지한다.
- `Telemetry/`, `LiveOps/` 같은 새 폴더는 backend state와 별도 계약이 필요할 때만 추가한다.
