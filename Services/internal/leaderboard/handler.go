package leaderboard

import (
	"net/http"
	"strconv"

	"github.com/go-chi/chi/v5"
	"github.com/google/uuid"
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
	r.Get("/top", h.GetTop)
	r.Get("/rank/{user_id}", h.GetRank)
	return r
}

func (h *Handler) GetTop(w http.ResponseWriter, r *http.Request) {
	limit := 100
	if v := r.URL.Query().Get("limit"); v != "" {
		if parsed, err := strconv.Atoi(v); err == nil && parsed > 0 && parsed <= 500 {
			limit = parsed
		}
	}

	entries, err := h.repo.GetTop(r.Context(), limit)
	if err != nil {
		response.Error(w, http.StatusInternalServerError, "failed to get leaderboard")
		return
	}
	response.JSON(w, http.StatusOK, entries)
}

func (h *Handler) GetRank(w http.ResponseWriter, r *http.Request) {
	userIdStr := chi.URLParam(r, "user_id")
	userId, err := uuid.Parse(userIdStr)
	if err != nil {
		response.Error(w, http.StatusBadRequest, "invalid user_id")
		return
	}

	rank, err := h.repo.GetRank(r.Context(), userId)
	if err != nil {
		response.Error(w, http.StatusNotFound, "user not found in leaderboard")
		return
	}
	response.JSON(w, http.StatusOK, rank)
}
