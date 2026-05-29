package payment

import (
	"encoding/json"
	"errors"
	"net/http"

	"github.com/go-chi/chi/v5"
	apperr "winters-backend/pkg/errors"
	"winters-backend/pkg/middleware"
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
	r.Post("/charge", h.Charge)
	r.Get("/balance", h.GetBalance)
	return r
}

func (h *Handler) Charge(w http.ResponseWriter, r *http.Request) {
	claims := middleware.GetClaims(r.Context())
	if claims == nil {
		response.Error(w, http.StatusUnauthorized, "missing claims")
		return
	}

	var req ChargeRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		response.Error(w, http.StatusBadRequest, "invalid request body")
		return
	}

	resp, err := h.svc.Charge(r.Context(), claims.UserID, req)
	if err != nil {
		handleError(w, err)
		return
	}
	response.JSON(w, http.StatusOK, resp)
}

func (h *Handler) GetBalance(w http.ResponseWriter, r *http.Request) {
	claims := middleware.GetClaims(r.Context())
	if claims == nil {
		response.Error(w, http.StatusUnauthorized, "missing claims")
		return
	}

	resp, err := h.svc.GetBalance(r.Context(), claims.UserID)
	if err != nil {
		handleError(w, err)
		return
	}
	response.JSON(w, http.StatusOK, resp)
}

func handleError(w http.ResponseWriter, err error) {
	switch {
	case errors.Is(err, apperr.ErrInvalidInput):
		response.Error(w, http.StatusBadRequest, err.Error())
	case errors.Is(err, apperr.ErrNotFound):
		response.Error(w, http.StatusNotFound, err.Error())
	case errors.Is(err, apperr.ErrUnauthorized):
		response.Error(w, http.StatusUnauthorized, err.Error())
	case errors.Is(err, apperr.ErrInsufficientBalance):
		response.Error(w, http.StatusPaymentRequired, err.Error())
	case errors.Is(err, apperr.ErrIdempotencyConflict):
		response.Error(w, http.StatusConflict, err.Error())
	default:
		response.Error(w, http.StatusInternalServerError, "internal server error")
	}
}
