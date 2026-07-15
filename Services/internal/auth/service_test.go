package auth

import (
	"errors"
	"strings"
	"testing"

	apperr "winters-backend/pkg/errors"
)

func TestServiceNormalizeLoginID(t *testing.T) {
	tests := []struct {
		name        string
		raw         string
		enabled     bool
		want        string
		wantErr     error
		wantErrText string
	}{
		{
			name:    "single character numeric id",
			raw:     "1",
			enabled: true,
			want:    "1",
		},
		{
			name:    "surrounding whitespace is trimmed",
			raw:     " \t1\r\n",
			enabled: true,
			want:    "1",
		},
		{
			name:    "mixed case is preserved",
			raw:     "PlayerOne",
			enabled: true,
			want:    "PlayerOne",
		},
		{
			name:        "empty id",
			raw:         "",
			enabled:     true,
			wantErr:     apperr.ErrInvalidInput,
			wantErrText: "id must be 1-32 characters",
		},
		{
			name:        "internal whitespace",
			raw:         "player one",
			enabled:     true,
			wantErr:     apperr.ErrInvalidInput,
			wantErrText: "id must not contain whitespace",
		},
		{
			name:        "thirty three characters",
			raw:         strings.Repeat("a", 33),
			enabled:     true,
			wantErr:     apperr.ErrInvalidInput,
			wantErrText: "id must be 1-32 characters",
		},
		{
			name:        "id auth disabled",
			raw:         "1",
			enabled:     false,
			wantErr:     apperr.ErrUnauthorized,
			wantErrText: "id auth is disabled",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			service := &Service{idAuthEnabled: tt.enabled}

			got, err := service.normalizeLoginID(tt.raw)
			if tt.wantErr == nil {
				if err != nil {
					t.Fatalf("normalizeLoginID() unexpected error: %v", err)
				}
				if got != tt.want {
					t.Fatalf("normalizeLoginID() = %q, want %q", got, tt.want)
				}
				return
			}

			if !errors.Is(err, tt.wantErr) {
				t.Fatalf("normalizeLoginID() error = %v, want error wrapping %v", err, tt.wantErr)
			}
			if !strings.Contains(err.Error(), tt.wantErrText) {
				t.Fatalf("normalizeLoginID() error = %q, want text %q", err.Error(), tt.wantErrText)
			}
		})
	}
}
