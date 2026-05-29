package errors

import "errors"

var (
	ErrNotFound             = errors.New("resource not found")
	ErrAlreadyExists        = errors.New("resource already exists")
	ErrInvalidInput         = errors.New("invalid input")
	ErrUnauthorized         = errors.New("unauthorized")
	ErrForbidden            = errors.New("forbidden")
	ErrInsufficientBalance  = errors.New("insufficient balance")
	ErrIdempotencyConflict = errors.New("idempotency conflict")
)
