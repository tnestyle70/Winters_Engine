package profile

import (
	"encoding/json"
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
	r.Post("/me/matches", h.ReportMyMatch)
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

// ReportMyMatch records the caller's finished match from JWT claims:
// inserts match history, applies MMR delta, credits RP to the wallet.
// Client self-report (dev-stage trade-off) — the C++ game server does not
// yet carry account identity, so the client is the only reporter available (S035).
func (h *Handler) ReportMyMatch(w http.ResponseWriter, r *http.Request) {
	claims := middleware.GetClaims(r.Context())
	if claims == nil {
		response.Error(w, http.StatusUnauthorized, "missing claims")
		return
	}

	var req struct {
		Result string `json:"result"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		response.Error(w, http.StatusBadRequest, "invalid body")
		return
	}
	if req.Result != "win" && req.Result != "loss" {
		response.Error(w, http.StatusBadRequest, "result must be win or loss")
		return
	}

	const (
		mmrWinDelta  = 25
		mmrLossDelta = -25
		rpWinReward  = 150
		rpLossReward = 75
	)

	player := MatchCompletedPlayer{
		UserID: claims.UserID,
		Result: req.Result,
	}
	rpReward := int64(rpLossReward)
	if req.Result == "win" {
		player.MMRChange = mmrWinDelta
		rpReward = rpWinReward
	} else {
		player.MMRChange = mmrLossDelta
	}

	matchID := uuid.New()
	if err := h.repo.ReportMatch(r.Context(), claims.UserID, matchID, player, rpReward); err != nil {
		response.Error(w, http.StatusInternalServerError, "failed to report match")
		return
	}

	response.JSON(w, http.StatusOK, map[string]any{
		"match_id":   matchID,
		"mmr_change": player.MMRChange,
		"rp_reward":  rpReward,
	})
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
