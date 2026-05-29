package middleware

import (
	"context"
	"net/http"
	"strings"

	jwtauth "winters-backend/pkg/auth"
	"winters-backend/pkg/response"
)

type contextKey string

const UserClaimsKey contextKey = "user_claims"

func JWTAuth(jwtMgr *jwtauth.JWTManager) func(http.Handler) http.Handler {
	return func(next http.Handler) http.Handler {
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			authHeader := r.Header.Get("Authorization")
			if authHeader == "" {
				response.Error(w, http.StatusUnauthorized, "missing authorization header")
				return
			}
			parts := strings.SplitN(authHeader, " ", 2)
			if len(parts) != 2 || parts[0] != "Bearer" {
				response.Error(w, http.StatusUnauthorized, "invalid authorization format")
				return
			}
			claims, err := jwtMgr.ValidateAccessToken(parts[1])
			if err != nil {
				response.Error(w, http.StatusUnauthorized, "invalid token")
				return
			}
			ctx := context.WithValue(r.Context(), UserClaimsKey, claims)
			next.ServeHTTP(w, r.WithContext(ctx))
		})
	}
}

func GetClaims(ctx context.Context) *jwtauth.Claims {
	claims, _ := ctx.Value(UserClaimsKey).(*jwtauth.Claims)
	return claims
}
