package auth

import (
	"encoding/json"
	"errors"
	"net/http"

	"github.com/go-chi/chi/v5"
	apperr "winters-backend/pkg/errors"
	"winters-backend/pkg/response"
)

type Handler struct {
	svc *Service
}

func NewHandler(svc *Service) *Handler {
	return &Handler{svc: svc}
}

func (h *Handler) Routes() chi.Router {
	r := chi.NewRouter()
	r.Post("/register", h.Register)
	r.Post("/login", h.Login)
	r.Post("/refresh", h.Refresh)
	r.Post("/logout", h.Logout)
	return r
}

func (h *Handler) Register(w http.ResponseWriter, r *http.Request) {
	var req RegisterRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		response.Error(w, http.StatusBadRequest, "invalid request body")
		return
	}
	tokens, err := h.svc.Register(r.Context(), req)
	if err != nil {
		handleError(w, err)
		return
	}
	response.JSON(w, http.StatusCreated, tokens)
}

func (h *Handler) Login(w http.ResponseWriter, r *http.Request) {
	var req LoginRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		response.Error(w, http.StatusBadRequest, "invalid request body")
		return
	}
	tokens, err := h.svc.Login(r.Context(), req)
	if err != nil {
		handleError(w, err)
		return
	}
	response.JSON(w, http.StatusOK, tokens)
}

func (h *Handler) Refresh(w http.ResponseWriter, r *http.Request) {
	var req RefreshRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		response.Error(w, http.StatusBadRequest, "invalid request body")
		return
	}
	tokens, err := h.svc.Refresh(r.Context(), req)
	if err != nil {
		handleError(w, err)
		return
	}
	response.JSON(w, http.StatusOK, tokens)
}

func (h *Handler) Logout(w http.ResponseWriter, r *http.Request) {
	var req LogoutRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		response.Error(w, http.StatusBadRequest, "invalid request body")
		return
	}
	_ = h.svc.Logout(r.Context(), req)
	response.JSON(w, http.StatusOK, map[string]string{"message": "logged out"})
}

func handleError(w http.ResponseWriter, err error) {
	switch {
	case errors.Is(err, apperr.ErrInvalidInput):
		response.Error(w, http.StatusBadRequest, err.Error())
	case errors.Is(err, apperr.ErrUnauthorized):
		response.Error(w, http.StatusUnauthorized, err.Error())
	case errors.Is(err, apperr.ErrNotFound):
		response.Error(w, http.StatusNotFound, err.Error())
	case errors.Is(err, apperr.ErrAlreadyExists):
		response.Error(w, http.StatusConflict, err.Error())
	case errors.Is(err, apperr.ErrInsufficientBalance):
		response.Error(w, http.StatusPaymentRequired, err.Error())
	default:
		response.Error(w, http.StatusInternalServerError, "internal server error")
	}
}
