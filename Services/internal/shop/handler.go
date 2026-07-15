package shop

import (
	"encoding/json"
	"errors"
	"net/http"

	"github.com/go-chi/chi/v5"
	"github.com/google/uuid"
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
	r.Get("/items", h.ListItems)
	r.Get("/storefront", h.GetStorefront)
	r.Post("/purchase", h.Purchase)
	r.Get("/inventory/{user_id}", h.GetInventory)
	return r
}

// GetStorefront returns the caller's atomic storefront snapshot.
// Identity comes from JWT claims only — never from the request.
func (h *Handler) GetStorefront(w http.ResponseWriter, r *http.Request) {
	claims := middleware.GetClaims(r.Context())
	if claims == nil {
		response.Error(w, http.StatusUnauthorized, "missing claims")
		return
	}
	resp, err := h.svc.GetStorefront(r.Context(), claims.UserID)
	if err != nil {
		handleError(w, err)
		return
	}
	response.JSON(w, http.StatusOK, resp)
}

func (h *Handler) ListItems(w http.ResponseWriter, r *http.Request) {
	items, err := h.svc.ListItems(r.Context())
	if err != nil {
		response.Error(w, http.StatusInternalServerError, "failed to list items")
		return
	}
	response.JSON(w, http.StatusOK, items)
}

func (h *Handler) Purchase(w http.ResponseWriter, r *http.Request) {
	claims := middleware.GetClaims(r.Context())
	if claims == nil {
		response.Error(w, http.StatusUnauthorized, "missing claims")
		return
	}

	var req PurchaseRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		response.Error(w, http.StatusBadRequest, "invalid request body")
		return 
	}

	resp, err := h.svc.Purchase(r.Context(), claims.UserID, req.ItemID)
	if err != nil {
		handleError(w, err)
		return
	}
	response.JSON(w, http.StatusOK, resp)
}

func (h *Handler) GetInventory(w http.ResponseWriter, r *http.Request) {
	userIdStr := chi.URLParam(r, "user_id")
	userId, err := uuid.Parse(userIdStr)
	if err != nil {
		response.Error(w, http.StatusBadRequest, "invalid user_id")
		return 
	}
	items, err := h.svc.GetInventory(r.Context(), userId)
	if err != nil {
		response.Error(w, http.StatusInternalServerError, "failed to get inventory")
		return
	}
	response.JSON(w, http.StatusOK, items)
}

func handleError(w http.ResponseWriter, err error) {
	switch {
	case errors.Is(err, apperr.ErrNotFound):
		response.Error(w, http.StatusNotFound, err.Error())
	case errors.Is(err, apperr.ErrInvalidInput):
		response.Error(w, http.StatusBadRequest, err.Error())
	case errors.Is(err, apperr.ErrInsufficientBalance):
		response.Error(w, http.StatusPaymentRequired, err.Error())
	default:
		response.Error(w, http.StatusInternalServerError, "internal server error")
	}
}