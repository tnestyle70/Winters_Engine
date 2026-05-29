# Phase 5~8 코드 검토 결과 + 수정 계획서

> **검토일**: 2026-04-13
> **검토 범위**: Services/ 전체 (Phase 0~8)
> **결론**: Phase 0~4 정상, Phase 5~6 심각한 문제 다수 발견

---

## 검토 요약

| Phase | 서비스 | 상태 | 문제 |
|-------|--------|------|------|
| 0 | 인프라 (DB, Redis, Kafka, pkg/*) | ✅ 정상 | - |
| 1 | Auth (8081) | ✅ 정상 | - |
| 2 | Leaderboard (8082) | ✅ 정상 | - |
| 3 | Matchmaking (8083) | ✅ 정상 | - |
| 4 | Profile (8084) | ✅ 정상 | - |
| 5 | Payment (8085) | ❌ **컴파일 불가** | 빈 파일 3개, 오타, 잘못된 타입 |
| 6 | Shop (8086) | ❌ **컴파일 불가** | 빈 파일 3개, 오타, 잘못된 위치 파일 |
| 7 | Kafka Integration | ⚠️ 보류 | 5, 6 수정 후 검증 가능 |
| 8 | C++ Client SDK | ⬜ 미구현 | Phase5~8 계획서에 구조만 존재 |

---

## 발견된 문제 상세

### 🔴 P1: 빈 파일 (내용 없음) — 4개

| # | 파일 | 계획서 대비 상태 |
|---|------|-----------------|
| 1 | `internal/payment/repository.go` | 빈 파일 — ProcessCharge, FindByIdempotencyKey, GetBalance 미구현 |
| 2 | `internal/payment/gateway_mock.go` | 빈 파일 — MockGateway 미구현 |
| 3 | `internal/shop/model.go` | 빈 파일 — ShopItem, InventoryItem, PurchaseRequest/Response 미정의 |
| 4 | `internal/shop/repository.go` | 빈 파일 — ListItems, Purchase, GetInventory 미구현 |

### 🔴 P2: 잘못된 위치 파일 — 1개

| # | 현재 위치 | 올바른 위치 | 설명 |
|---|----------|-----------|------|
| 1 | `internal/shop/main.go` | 삭제 필요 (빈 파일) | cmd/shop/main.go에 있어야 함 |

### 🔴 P3: cmd/ 누락 — 2개

| # | 누락 파일 | 설명 |
|---|----------|------|
| 1 | `cmd/payment/main.go` | Payment 서비스 진입점 없음 |
| 2 | `cmd/shop/main.go` | Shop 서비스 진입점 없음 |

### 🔴 P4: 컴파일 오류 — 5개

| # | 파일 | 줄 | 문제 | 수정 |
|---|------|---|------|------|
| 1 | `internal/payment/gateway.go` L7 | `concext.Context` | 오타 | → `context.Context` |
| 2 | `internal/payment/service.go` L19 | `*kafka.writer` | 소문자 | → `*kafka.Writer` |
| 3 | `internal/payment/service.go` L23 | `make(map[string])` | 불완전 | → `make(map[string]PaymentGateway)` |
| 4 | `internal/payment/service.go` L26-56 | Shop 로직이 Payment에 섞임 | `ListItems`, `Purchase`, `GetInventory`는 Shop 서비스 메서드 | → Payment 전용 로직으로 교체 |
| 5 | `internal/shop/handler.go` L63 | `userIDstr` | 대소문자 불일치 | → `userIDStr` |

### 🟡 P5: 타입 오류 — 2개

| # | 파일 | 줄 | 문제 | 수정 |
|---|------|---|------|------|
| 1 | `internal/payment/model.go` L34 | `CoinAmount string` | string이면 안 됨 | → `CoinAmount int64` |
| 2 | `internal/payment/model.go` L37 | `"completed_at, omitempty"` | 쉼표 뒤 공백 | → `"completed_at,omitempty"` |

### 🟡 P6: 구조 오류 — 2개

| # | 파일 | 문제 |
|---|------|------|
| 1 | `internal/payment/model.go` L41 | `paymentCompletedEvent` 소문자 시작 → 패키지 외부 미노출 | → `PaymentCompletedEvent` |
| 2 | `internal/payment/model.go` L31 | `GatewaytxID` | → `GatewayTxID` |

---

## 수정 계획

### 수정 1: `internal/payment/gateway.go` — 오타 수정

**L7 수정 전**:
```go
VerifyReceipt(ctx concext.Context, receiptData string, expectedAmount int64) (gatewayTxID string, err error)
```
**L7 수정 후**:
```go
VerifyReceipt(ctx context.Context, receiptData string, expectedAmount int64) (gatewayTxID string, err error)
```

---

### 수정 2: `internal/payment/gateway_mock.go` — 전체 작성

```go
package payment

import (
	"context"
	"github.com/google/uuid"
)

type MockGateway struct{}

func (g *MockGateway) Name() string { return "mock" }

func (g *MockGateway) VerifyReceipt(ctx context.Context, receiptData string, expectedAmount int64) (string, error) {
	return "mock_" + uuid.New().String(), nil
}

func (g *MockGateway) Refund(ctx context.Context, gatewayTxID string, amount int64) error {
	return nil
}
```

---

### 수정 3: `internal/payment/model.go` — 타입/네이밍 수정

**L31**: `GatewaytxID` → `GatewayTxID`
**L34**: `CoinAmount string` → `CoinAmount int64`
**L37**: `"completed_at, omitempty"` → `"completed_at,omitempty"`
**L41**: `paymentCompletedEvent` → `PaymentCompletedEvent`

---

### 수정 4: `internal/payment/repository.go` — 전체 작성

계획서(Phase5_to_Phase8.md)의 repository.go 코드 전문 그대로 적용.
- `ProcessCharge()` — 원자적 트랜잭션 (payment_transactions INSERT + wallets UPDATE + coin_transactions INSERT)
- `FindByIdempotencyKey()` — 멱등성 키 조회
- `GetBalance()` — 잔액 조회

---

### 수정 5: `internal/payment/service.go` — 전체 교체

현재 Shop 로직이 섞여 있음. 계획서의 Payment 전용 service.go로 교체.
- `RegisterGateway()`, `Charge()`, `GetBalance()` 메서드
- Kafka `TopicPaymentEvents`에 `PaymentCompleted` 이벤트 발행

---

### 수정 6: `internal/payment/handler.go` — 신규 생성

계획서의 handler.go 코드 전문 적용.
- `POST /charge`, `GET /balance` 라우트

---

### 수정 7: `cmd/payment/main.go` — 신규 생성

계획서의 cmd/payment/main.go 코드 전문 적용.

---

### 수정 8: `internal/shop/main.go` — 삭제

빈 파일, 잘못된 위치. 삭제.

---

### 수정 9: `internal/shop/model.go` — 전체 작성

계획서의 shop/model.go 코드 전문 적용.
- `ShopItem`, `InventoryItem`, `PurchaseRequest`, `PurchaseResponse`, `ItemPurchasedEvent`

---

### 수정 10: `internal/shop/repository.go` — 전체 작성

계획서의 shop/repository.go 코드 전문 적용.
- `ListItems()`, `Purchase()` (FOR UPDATE 행 잠금 + 원자적 트랜잭션), `GetInventory()`

---

### 수정 11: `internal/shop/handler.go` L63 — 오타 수정

**수정 전**: `userID, err := uuid.Parse(userIDstr)`
**수정 후**: `userID, err := uuid.Parse(userIDStr)`

---

### 수정 12: `cmd/shop/main.go` — 신규 생성

계획서의 cmd/shop/main.go 코드 전문 적용.

---

## 수정 파일 목록

| 동작 | 파일 | 내용 |
|------|------|------|
| 수정 | `internal/payment/gateway.go` | 오타 수정 (concext→context) |
| 작성 | `internal/payment/gateway_mock.go` | MockGateway 구현 |
| 수정 | `internal/payment/model.go` | 타입/네이밍 4건 수정 |
| 작성 | `internal/payment/repository.go` | ProcessCharge, FindByIdempotencyKey, GetBalance |
| 교체 | `internal/payment/service.go` | Payment 전용 로직으로 교체 |
| 생성 | `internal/payment/handler.go` | POST /charge, GET /balance |
| 생성 | `cmd/payment/main.go` | Payment 서비스 진입점 |
| 삭제 | `internal/shop/main.go` | 빈 파일, 잘못된 위치 |
| 작성 | `internal/shop/model.go` | ShopItem, InventoryItem 등 |
| 작성 | `internal/shop/repository.go` | ListItems, Purchase, GetInventory |
| 수정 | `internal/shop/handler.go` | L63 오타 수정 |
| 생성 | `cmd/shop/main.go` | Shop 서비스 진입점 |

**총 12건** (생성 4, 작성 4, 수정 3, 삭제 1)
