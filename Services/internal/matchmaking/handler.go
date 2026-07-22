package matchmaking

import (
	"crypto/subtle"
	"encoding/json"
	"errors"
	"net/http"
	"strings"

	"github.com/go-chi/chi/v5"
	"github.com/google/uuid"
	apperr "winters-backend/pkg/errors"
	"winters-backend/pkg/middleware"
	"winters-backend/pkg/response"
)

type Handler struct {
	svc           *Service
	internalToken string
}

func NewHandler(svc *Service, internalToken string) *Handler {
	return &Handler{svc: svc, internalToken: internalToken}
}

func (h *Handler) Routes() chi.Router {
	router := chi.NewRouter()
	router.Post("/join", h.Join)
	router.Delete("/leave", h.Leave)
	router.Get("/status", h.Status)
	return router
}

func (h *Handler) InternalRoutes() chi.Router {
	router := chi.NewRouter()
	router.Use(h.requireInternalToken)
	router.Get("/game-sessions/{game_session_id}/active", h.ActiveGameSession)
	router.Post("/game-sessions/{game_session_id}/start", h.StartGameSession)
	router.Post("/game-sessions/{game_session_id}/abort", h.AbortGameSession)
	router.Post("/game-sessions/{game_session_id}/ready", h.ReadyGameSession)
	return router
}

func (h *Handler) requireInternalToken(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		const prefix = "Bearer "
		header := r.Header.Get("Authorization")
		if !strings.HasPrefix(header, prefix) ||
			subtle.ConstantTimeCompare(
				[]byte(strings.TrimPrefix(header, prefix)),
				[]byte(h.internalToken)) != 1 {
			response.Error(w, http.StatusUnauthorized, "invalid internal credential")
			return
		}
		next.ServeHTTP(w, r)
	})
}

func (h *Handler) Join(w http.ResponseWriter, r *http.Request) {
	claims := middleware.GetClaims(r.Context())
	if claims == nil {
		response.Error(w, http.StatusUnauthorized, "missing claims")
		return
	}
	status, err := h.svc.Join(r.Context(), claims.UserID)
	if err != nil {
		response.Error(w, http.StatusConflict, err.Error())
		return
	}
	response.JSON(w, http.StatusOK, status)
}

func (h *Handler) Leave(w http.ResponseWriter, r *http.Request) {
	claims := middleware.GetClaims(r.Context())
	if claims == nil {
		response.Error(w, http.StatusUnauthorized, "missing claims")
		return
	}
	if err := h.svc.Leave(r.Context(), claims.UserID); err != nil {
		response.Error(w, http.StatusInternalServerError, err.Error())
		return
	}
	response.JSON(w, http.StatusOK, map[string]string{"status": "left"})
}

func (h *Handler) Status(w http.ResponseWriter, r *http.Request) {
	claims := middleware.GetClaims(r.Context())
	if claims == nil {
		response.Error(w, http.StatusUnauthorized, "missing claims")
		return
	}
	status, err := h.svc.Status(r.Context(), claims.UserID)
	if err != nil {
		response.Error(w, http.StatusInternalServerError, err.Error())
		return
	}
	response.JSON(w, http.StatusOK, status)
}

func (h *Handler) ActiveGameSession(w http.ResponseWriter, r *http.Request) {
	gameSessionID := chi.URLParam(r, "game_session_id")
	if gameSessionID == "" || len(gameSessionID) > 64 {
		response.Error(w, http.StatusBadRequest, "invalid game session id")
		return
	}
	active, occupied, err := h.svc.ActiveGameSession(r.Context(), gameSessionID)
	if err != nil {
		response.Error(w, http.StatusInternalServerError, err.Error())
		return
	}
	result := map[string]any{
		"occupied":   occupied,
		"generation": active.Generation,
	}
	if occupied {
		result["match_id"] = active.MatchID.String()
		result["status"] = active.Status
	}
	response.JSON(w, http.StatusOK, result)
}

func (h *Handler) StartGameSession(w http.ResponseWriter, r *http.Request) {
	gameSessionID := chi.URLParam(r, "game_session_id")
	if gameSessionID == "" || len(gameSessionID) > 64 {
		response.Error(w, http.StatusBadRequest, "invalid game session id")
		return
	}
	var request struct {
		MatchID    string             `json:"match_id"`
		Generation int64              `json:"generation"`
		RosterHash string             `json:"roster_hash"`
		Players    []LobbyStartPlayer `json:"players"`
	}
	decoder := json.NewDecoder(http.MaxBytesReader(w, r.Body, 64*1024))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(&request); err != nil || request.RosterHash == "" {
		response.Error(w, http.StatusBadRequest, "invalid JSON body")
		return
	}
	matchID, err := uuid.Parse(request.MatchID)
	if err != nil {
		response.Error(w, http.StatusBadRequest, "invalid match id")
		return
	}
	if err := h.svc.StartGameSession(
		r.Context(), gameSessionID, matchID, request.Generation, request.Players); err != nil {
		switch {
		case errors.Is(err, errCapacityMismatch),
			errors.Is(err, errCapacityOccupied),
			errors.Is(err, errRosterMismatch):
			response.Error(w, http.StatusConflict, err.Error())
		case errors.Is(err, apperr.ErrInvalidInput):
			response.Error(w, http.StatusBadRequest, err.Error())
		default:
			response.Error(w, http.StatusInternalServerError, err.Error())
		}
		return
	}
	response.JSON(w, http.StatusOK, map[string]any{
		"status": "running", "match_id": matchID.String(),
		"generation": request.Generation, "roster_hash": request.RosterHash,
	})
}

func (h *Handler) AbortGameSession(w http.ResponseWriter, r *http.Request) {
	gameSessionID := chi.URLParam(r, "game_session_id")
	if gameSessionID == "" || len(gameSessionID) > 64 {
		response.Error(w, http.StatusBadRequest, "invalid game session id")
		return
	}
	var request struct {
		MatchID    string `json:"match_id"`
		Generation int64  `json:"generation"`
	}
	decoder := json.NewDecoder(http.MaxBytesReader(w, r.Body, 4096))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(&request); err != nil {
		response.Error(w, http.StatusBadRequest, "invalid JSON body")
		return
	}
	matchID, err := uuid.Parse(request.MatchID)
	if err != nil || request.Generation <= 0 {
		response.Error(w, http.StatusBadRequest, "invalid game session identity")
		return
	}
	if err := h.svc.AbortGameSession(
		r.Context(), gameSessionID, matchID, request.Generation); err != nil {
		if errors.Is(err, errCapacityMismatch) || errors.Is(err, errCapacityOccupied) {
			response.Error(w, http.StatusConflict, err.Error())
			return
		}
		response.Error(w, http.StatusInternalServerError, err.Error())
		return
	}
	response.JSON(w, http.StatusOK, map[string]string{"status": "aborted"})
}

func (h *Handler) ReadyGameSession(w http.ResponseWriter, r *http.Request) {
	gameSessionID := chi.URLParam(r, "game_session_id")
	if gameSessionID == "" || len(gameSessionID) > 64 {
		response.Error(w, http.StatusBadRequest, "invalid game session id")
		return
	}
	var request struct {
		MatchID string `json:"match_id"`
	}
	decoder := json.NewDecoder(http.MaxBytesReader(w, r.Body, 4096))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(&request); err != nil {
		response.Error(w, http.StatusBadRequest, "invalid JSON body")
		return
	}
	matchID, err := uuid.Parse(request.MatchID)
	if err != nil {
		response.Error(w, http.StatusBadRequest, "invalid match id")
		return
	}
	if err := h.svc.ReleaseGameSession(r.Context(), gameSessionID, matchID); err != nil {
		if errors.Is(err, errCapacityMismatch) {
			response.Error(w, http.StatusConflict, err.Error())
			return
		}
		response.Error(w, http.StatusInternalServerError, err.Error())
		return
	}
	response.JSON(w, http.StatusOK, map[string]string{"status": "ready"})
}
