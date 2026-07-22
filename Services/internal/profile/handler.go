package profile

import (
	"errors"
	"net/http"
	"strconv"

	"github.com/go-chi/chi/v5"
	"github.com/google/uuid"
	apperr "winters-backend/pkg/errors"
	"winters-backend/pkg/middleware"
	"winters-backend/pkg/response"
)

type Handler struct {
	repo *Repository
}

func NewHandler(repo *Repository) *Handler {
	return &Handler{repo: repo}
}

func (h *Handler) Routes() chi.Router {
	r := chi.NewRouter()
	r.Get("/me", h.GetMyProfile)
	r.Get("/me/history", h.GetMyMatchHistory)
	r.Get("/{user_id}", h.GetProfile)
	r.Get("/{user_id}/history", h.GetMatchHistory)
	return r
}

// GetMyProfile serves the caller's own profile from JWT claims only —
// the client never supplies a user id for initial sync.
func (h *Handler) GetMyProfile(w http.ResponseWriter, r *http.Request) {
	claims := middleware.GetClaims(r.Context())
	if claims == nil {
		response.Error(w, http.StatusUnauthorized, "missing claims")
		return
	}

	p, err := h.repo.GetProfile(r.Context(), claims.UserID)
	if err != nil {
		if errors.Is(err, apperr.ErrNotFound) {
			response.Error(w, http.StatusNotFound, "user not found")
			return
		}
		response.Error(w, http.StatusInternalServerError, "failed to get profile")
		return
	}
	response.JSON(w, http.StatusOK, p)
}

func (h *Handler) GetMyMatchHistory(w http.ResponseWriter, r *http.Request) {
	claims := middleware.GetClaims(r.Context())
	if claims == nil {
		response.Error(w, http.StatusUnauthorized, "missing claims")
		return
	}

	records, err := h.repo.GetMatchHistory(r.Context(), claims.UserID, 20, 0)
	if err != nil {
		response.Error(w, http.StatusInternalServerError, "failed to get match history")
		return
	}
	response.JSON(w, http.StatusOK, records)
}

func (h *Handler) GetProfile(w http.ResponseWriter, r *http.Request) {
	userIdStr := chi.URLParam(r, "user_id")
	userId, err := uuid.Parse(userIdStr)
	if err != nil {
		response.Error(w, http.StatusBadRequest, "invalid user_id")
		return
	}

	p, err := h.repo.GetProfile(r.Context(), userId)
	if err != nil {
		if errors.Is(err, apperr.ErrNotFound) {
			response.Error(w, http.StatusNotFound, "user not found")
			return
		}
		response.Error(w, http.StatusInternalServerError, "failed to get profile")
		return
	}
	response.JSON(w, http.StatusOK, p)
}

func (h *Handler) GetMatchHistory(w http.ResponseWriter, r *http.Request) {
	userIdStr := chi.URLParam(r, "user_id")
	userId, err := uuid.Parse(userIdStr)
	if err != nil {
		response.Error(w, http.StatusBadRequest, "invalid user_id")
		return
	}

	limit := 20
	offset := 0
	if v := r.URL.Query().Get("limit"); v != "" {
		if parsed, err := strconv.Atoi(v); err == nil && parsed > 0 && parsed <= 100 {
			limit = parsed
		}
	}
	if v := r.URL.Query().Get("offset"); v != "" {
		if parsed, err := strconv.Atoi(v); err == nil && parsed >= 0 {
			offset = parsed
		}
	}

	records, err := h.repo.GetMatchHistory(r.Context(), userId, limit, offset)
	if err != nil {
		response.Error(w, http.StatusInternalServerError, "failed to get match history")
		return
	}
	response.JSON(w, http.StatusOK, records)
}
