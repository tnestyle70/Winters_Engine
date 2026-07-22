package replay

import (
	"crypto/subtle"
	"encoding/json"
	"errors"
	"io"
	"log/slog"
	"net/http"
	"strconv"
	"strings"

	"github.com/go-chi/chi/v5"
	"github.com/google/uuid"
	apperr "winters-backend/pkg/errors"
	"winters-backend/pkg/middleware"
	"winters-backend/pkg/response"
)

const maximumControlBodyBytes = 1 << 20

type Handler struct {
	service       *Service
	internalToken string
}

func NewHandler(service *Service, internalToken string) *Handler {
	return &Handler{service: service, internalToken: internalToken}
}

func (h *Handler) InternalRoutes() chi.Router {
	router := chi.NewRouter()
	router.Use(h.requireInternalToken)
	router.Post("/replays/uploads", h.createUpload)
	router.Post("/replays/{replay_id}/parts", h.presignParts)
	router.Post("/replays/{replay_id}/complete", h.completeUpload)
	router.Post("/replays/{replay_id}/abort", h.abortUpload)
	router.Post("/matches/{match_id}/complete", h.completeMatch)
	return router
}

func (h *Handler) UserRoutes() chi.Router {
	router := chi.NewRouter()
	router.Get("/me", h.listMine)
	router.Get("/{replay_id}", h.getReplay)
	router.Post("/{replay_id}/download", h.downloadReplay)
	router.Delete("/{replay_id}/me", h.hideReplay)
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

func decodeJSON(w http.ResponseWriter, r *http.Request, destination any) bool {
	r.Body = http.MaxBytesReader(w, r.Body, maximumControlBodyBytes)
	decoder := json.NewDecoder(r.Body)
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(destination); err != nil {
		response.Error(w, http.StatusBadRequest, "invalid JSON body")
		return false
	}
	if err := decoder.Decode(&struct{}{}); !errors.Is(err, io.EOF) {
		response.Error(w, http.StatusBadRequest, "request body must contain one JSON value")
		return false
	}
	return true
}

func (h *Handler) createUpload(w http.ResponseWriter, r *http.Request) {
	var request CreateUploadRequest
	if !decodeJSON(w, r, &request) {
		return
	}
	result, err := h.service.CreateUpload(r.Context(), request)
	if err != nil {
		writeServiceError(w, err)
		return
	}
	response.JSON(w, http.StatusOK, result)
}

func (h *Handler) presignParts(w http.ResponseWriter, r *http.Request) {
	replayID, ok := parseID(w, chi.URLParam(r, "replay_id"))
	if !ok {
		return
	}
	var request PresignPartsRequest
	if !decodeJSON(w, r, &request) {
		return
	}
	parts, err := h.service.PresignParts(r.Context(), replayID, request.PartNumbers)
	if err != nil {
		writeServiceError(w, err)
		return
	}
	response.JSON(w, http.StatusOK, map[string]any{"parts": parts})
}

func (h *Handler) completeUpload(w http.ResponseWriter, r *http.Request) {
	replayID, ok := parseID(w, chi.URLParam(r, "replay_id"))
	if !ok {
		return
	}
	var request CompleteUploadRequest
	if !decodeJSON(w, r, &request) {
		return
	}
	item, err := h.service.CompleteUpload(r.Context(), replayID, request)
	if err != nil {
		writeServiceError(w, err)
		return
	}
	response.JSON(w, http.StatusOK, item)
}

func (h *Handler) abortUpload(w http.ResponseWriter, r *http.Request) {
	replayID, ok := parseID(w, chi.URLParam(r, "replay_id"))
	if !ok {
		return
	}
	if err := h.service.AbortUpload(r.Context(), replayID); err != nil {
		writeServiceError(w, err)
		return
	}
	w.WriteHeader(http.StatusNoContent)
}

func (h *Handler) completeMatch(w http.ResponseWriter, r *http.Request) {
	matchID, ok := parseID(w, chi.URLParam(r, "match_id"))
	if !ok {
		return
	}
	var request MatchCompletionRequest
	if !decodeJSON(w, r, &request) {
		return
	}
	if err := h.service.CompleteMatch(r.Context(), matchID, request); err != nil {
		writeServiceError(w, err)
		return
	}
	response.JSON(w, http.StatusOK, map[string]string{"status": "completed"})
}

func (h *Handler) listMine(w http.ResponseWriter, r *http.Request) {
	claims := middleware.GetClaims(r.Context())
	if claims == nil {
		response.Error(w, http.StatusUnauthorized, "missing claims")
		return
	}
	limit := 20
	if value := r.URL.Query().Get("limit"); value != "" {
		if parsed, err := strconv.Atoi(value); err == nil {
			limit = parsed
		}
	}
	page, err := h.service.List(
		r.Context(), claims.UserID, limit, r.URL.Query().Get("cursor"))
	if err != nil {
		writeServiceError(w, err)
		return
	}
	response.JSON(w, http.StatusOK, page)
}

func (h *Handler) getReplay(w http.ResponseWriter, r *http.Request) {
	replayID, userID, ok := replayAndUserIDs(w, r)
	if !ok {
		return
	}
	item, err := h.service.Get(r.Context(), replayID, userID)
	if err != nil {
		writeServiceError(w, err)
		return
	}
	response.JSON(w, http.StatusOK, item)
}

func (h *Handler) downloadReplay(w http.ResponseWriter, r *http.Request) {
	replayID, userID, ok := replayAndUserIDs(w, r)
	if !ok {
		return
	}
	grant, err := h.service.Download(r.Context(), replayID, userID)
	if err != nil {
		writeServiceError(w, err)
		return
	}
	response.JSON(w, http.StatusOK, grant)
}

func (h *Handler) hideReplay(w http.ResponseWriter, r *http.Request) {
	replayID, userID, ok := replayAndUserIDs(w, r)
	if !ok {
		return
	}
	if err := h.service.Hide(r.Context(), replayID, userID); err != nil {
		writeServiceError(w, err)
		return
	}
	w.WriteHeader(http.StatusNoContent)
}

func replayAndUserIDs(w http.ResponseWriter, r *http.Request) (uuid.UUID, uuid.UUID, bool) {
	replayID, ok := parseID(w, chi.URLParam(r, "replay_id"))
	if !ok {
		return uuid.Nil, uuid.Nil, false
	}
	claims := middleware.GetClaims(r.Context())
	if claims == nil {
		response.Error(w, http.StatusUnauthorized, "missing claims")
		return uuid.Nil, uuid.Nil, false
	}
	return replayID, claims.UserID, true
}

func parseID(w http.ResponseWriter, raw string) (uuid.UUID, bool) {
	id, err := uuid.Parse(raw)
	if err != nil || id == uuid.Nil {
		response.Error(w, http.StatusBadRequest, "invalid id")
		return uuid.Nil, false
	}
	return id, true
}

func writeServiceError(w http.ResponseWriter, err error) {
	switch {
	case errors.Is(err, apperr.ErrInvalidInput):
		response.Error(w, http.StatusBadRequest, "invalid request")
	case errors.Is(err, apperr.ErrUnauthorized):
		response.Error(w, http.StatusUnauthorized, "unauthorized")
	case errors.Is(err, apperr.ErrForbidden):
		response.Error(w, http.StatusForbidden, "replay access denied")
	case errors.Is(err, apperr.ErrNotFound):
		response.Error(w, http.StatusNotFound, "resource not found")
	case errors.Is(err, apperr.ErrIdempotencyConflict), errors.Is(err, apperr.ErrAlreadyExists):
		response.Error(w, http.StatusConflict, "request conflicts with current state")
	default:
		slog.Error("replay request failed", "error", err)
		response.Error(w, http.StatusInternalServerError, "internal error")
	}
}
